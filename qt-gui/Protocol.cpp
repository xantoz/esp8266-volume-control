#include "Protocol.h"

#include <QHostInfo>
#include <memory>

#include <iostream>
#include <sstream>
#include <algorithm>

static const unsigned TIMEOUT = 10000;

void Protocol::socketSetup(QAbstractSocket *socket)
{
    connect(socket, static_cast<void (QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
            [this,socket](QAbstractSocket::SocketError) {
                QString errorString = socket->errorString();
                qDebug() << "SOCKET ERROR" << errorString;
                socket->abort();
                emit error(errorString);
            });
    connect(socket, &QAbstractSocket::readyRead, this, &Protocol::receiveStatusMessage);
}

void Protocol::sendCmd(const char *cmd)
{
    snprintf(command, sizeof(command), "%s", cmd);          // Need to copy because of logic in TcpProtcol::recieveStatusMessage
    this->sendMsg(command);
}

void Protocol::sendCmd(const char *cmd, int level)
{
    snprintf(command, sizeof(command), "%s %d", cmd, level);
    this->sendMsg(command);
}

void Protocol::sendCmd(const char *cmd, const char *chan, int level)
{
    snprintf(command, sizeof(command), "%s %s %d", cmd, chan, level);
    this->sendMsg(command);
}

void Protocol::parseStatusMessage(const char *status)
{
    // Note: This assumes L/R positions of CEN/SUB as L = SUB and R = CEN ...
    // TODO?: instead implement named properties server-side (python dict-like syntax?)
    ServerStatus values;                                    // TODO: handle both OK and no OK in beginning
    if (14 != sscanf(status, "OK 0: ( %d , %d , %d , %d ) ; 1: ( %d , %d , %d , %d ) ; 2: ( %d , %d , %d , %d ) ; Master: %d Mute: %d ",
                     &values.fl_level, &values.fr_level, &values.fl_mute, &values.fr_mute,
                     &values.sub_level, &values.cen_level, &values.sub_mute, &values.cen_mute,
                     &values.rl_level, &values.rr_level, &values.rl_mute, &values.rr_mute,
                     &values.master, &values.global_mute))
    {
        qDebug() << "ERROR: Couldn't parse server message";
        emit error(tr("Couldn't parse server message: ") + QString(status).simplified());
        return;
    }

    emit statusUpdate(values);
}

//// TcpProtocol ////

TcpProtocol::TcpProtocol()
{
    socket = new QTcpSocket(this);
    socketSetup(socket);

    connect(socket, &QTcpSocket::connected, this, &TcpProtocol::connected);
    connect(socket, &QTcpSocket::disconnected, this, &TcpProtocol::disconnected);
}

void TcpProtocol::serverConnect(const QString &host, quint16 port)
{
    // Fire-once connection (get server status on socket connect)
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(socket, &QTcpSocket::connected, [this, conn]() {
            this->sendCmd("status"); // Send status cmd
            this->disconnect(*conn);
        });
    socket->connectToHost(host, port);
}

void TcpProtocol::serverDisconnect()
{
    if (socket->state() == QTcpSocket::UnconnectedState)
        return;

    this->sendCmd("byebye");
    // TODO: read back 'CYA' here?

    socket->close();
}

void TcpProtocol::sendMsg(const char *msg)
{
    char data[256];
    snprintf(data, sizeof(data), "%s\n", msg);
    socket->write(data);
    if (!socket->waitForBytesWritten(TIMEOUT)) // TODO: Change to flush? Or otherwise make asynchronous
    {
        this->serverDisconnect();
        emit error(tr("Timed out sending command to server."));
        return;
    }

    socket->waitForReadyRead(TIMEOUT);
}

void TcpProtocol::receiveStatusMessage()
{
    // TODO: make this loop until we exhaust having stuff to read (see UdpProtocol)

    char status[512];
    qint64 lineLength = socket->readLine(status, sizeof(status));
    if (lineLength == -1)
    {
        emit error(tr("Problem reading status message from server. Disconnecting."));
        serverDisconnect();
        return;
    }

    qDebug() << "Got status string:" << QString(status).simplified();

    static const char errorString[] = "ERROR";
    if (0 == strncmp(status, errorString, sizeof(errorString)-1))
    {
        // Parse ERROR message
        QString msg = tr("Got error message from server:");
        msg.append(status+sizeof(errorString)-2); // Since strncmp passed we know there's at least 5 chars in this string
        qDebug() << "ERROR: " << msg;
        emit error(msg);
        return;
    }

    static const char statusString[] = "status";
    if (0 == strncmp(this->command, statusString, sizeof(statusString)-1))
    {
        // Parse and apply status message to sliders if we requested this status message
        // specifically using the status command

        qDebug() << "Parsing status string";
        parseStatusMessage(status);
    }
}


//// UdpProtocol ////
UdpProtocol::UdpProtocol(int updateInterval,
                         unsigned _pingMissesBeforeDisconnect,
                         int _retransmitDelay) :
    host(QHostAddress::Null), port(0), isConnected(false),
    pingMissesBeforeDisconnect(_pingMissesBeforeDisconnect),
    retransmitDelay(_retransmitDelay),
    waitingForAnswer(0)
{
    socket = new QUdpSocket(this);
    socketSetup(socket);

    statusUpdateTimer = new QTimer(this);
    statusUpdateTimer->setInterval(updateInterval);
    connect(statusUpdateTimer, &QTimer::timeout, this, &UdpProtocol::pingServer);
}

/**
 * Simulates a connection by pinging the server with a "status" command, waiting for a reply
 * (in blocking mode). Then starts a timer that periodically pings the UDP server.
 */
void UdpProtocol::serverConnect(const QString &host, quint16 port)
{
    if (this->isConnected)
    {
        emit error(tr("Trying to connect, but already connected"));
        return;
    }

    // TODO: non-blocking lookup
    QHostInfo hinfo = QHostInfo::fromName(host);
    bool found = false;
    for (auto &addr: hinfo.addresses())
    {
        bool ok = false;
        quint32 ipv4addr = addr.toIPv4Address(&ok);
        if (ok)
        {
            this->host.setAddress(ipv4addr);
            found = true;
            break;
        }
    }
    if (!found)
    {
        emit error(tr("Could not find an (IPv4) address for host: ") + host);
        return;
    }
    this->port = port;

    this->sendMsg("status"); // ping the server with a status cmd
    if (this->socket->waitForReadyRead(1000)) // wait for a response (timeout = 1s) (TODO: make
                                              // this asynchronous, because GUI needs to update
                                              // during connection process)
    {
        qDebug() << "UDP \"connection\" success";
        this->isConnected = true;
        emit connected();
        this->waitingForAnswer = 0;
        this->statusUpdateTimer->start();
    }
    else
    {
        qDebug() << "UDP \"connection\" failure";
        emit error(tr("Could not ping server"));
        this->host.clear();
        this->port = 0;
        this->statusUpdateTimer->stop();
        this->waitingForAnswer = 0;
    }
}

void UdpProtocol::serverDisconnect()
{
    if (!isConnected)
    {
        emit error(tr("Trying to disconnect, but already disconnected."));
        return;
    }

    this->host.clear();
    this->port = 0;
    this->statusUpdateTimer->stop();

    this->waitingForAnswer = 0;
    this->isConnected = false;

    emit disconnected();
}

void UdpProtocol::receiveStatusMessage()
{
    while (socket->hasPendingDatagrams())
    {
        char response[socket->pendingDatagramSize() + 1];
        qint64 size = socket->readDatagram(response, socket->pendingDatagramSize());
        if (size < 0)
        {
            emit error(tr("Problem reading response from server. Disconnecting."));
            serverDisconnect();
            return;
        }
        response[size] = '\0'; // NUL-terminate
        qDebug() << "Got response (size: " << size << ")" << response;

        if (0 == strncmp(response, "ERROR", 5))
        { // Parse ERROR message
            QString msg = tr("Got error message from server: ");
            msg.append(response+5); // Since strncmp passed we know there's at least 5 chars
            qDebug() << tr("ERROR: ") << msg;
            emit error(msg);
        }
        else if (0 == strncmp(response, "ACK", 3))
        { // ACK
            quint64 ack;
            std::istringstream stream(response+3);
            stream >> ack;
            this->largestReceivedAck = std::max(ack, this->largestReceivedAck);
        }
        else if (0 == strncmp(response, "OK", 2))
        { // Status message starts with OK (response to status command)
            this->waitingForAnswer = 0;
            // TODO: check the sequence number, and ignore responses from too old status
            // messages (to avoid problems if they arrive here out of order)
            parseStatusMessage(response);
        }
        else
        {
            QString msg = tr("Unknown response from server: ");
            msg.append(response);
            qDebug() << tr("ERROR: ") << msg;
            emit error(msg);
        }
    }
}

void UdpProtocol::sendMsg(const char *msg)
{
    static const char *STATUS = "status";
    bool isStatus = (0 == strncmp(msg, STATUS, strlen(STATUS)));

    QByteArray msgAry(msg, strlen(msg));
    quint64 seqNr;

    if (isStatus)
    {
        // We use a separate group of sequence numbers for status commands, and we do not
        // attempt to do any retransmissions in case of missed responses.
        seqNr = ++this->largestSentStatusSeqNr;
    }
    else
    {
        // Other commands use the regular sequence numbers. We will try to retransmit the latest
        // sent command on timeout.
        seqNr = ++this->largestSentAck;
    }

    // prepend sequence number to command
    msgAry.prepend((QString::number(seqNr) += ' ').toLatin1());

    if (!isStatus)
    {
        // It should be fine to capture msgAry by value, as QByteArray:s are implicitly shared
        std::function<void(void)> callback;
        callback = [this, seqNr, msgAry, &callback, retries = 0u]() mutable {
            if (seqNr <= this->largestReceivedAck) {
                qDebug() << "SUCCESS: Command found ACK nicely: " << QString::fromLatin1(msgAry.data());
                return;
            }
            if (seqNr < this->largestSentAck) {
                qDebug() << "Not retrying because newer command sent: " << QString::fromLatin1(msgAry.data());
                return;
            }
            if (retries > this->pingMissesBeforeDisconnect) {
                qDebug() << "WARNING: Reached maximum retries for command: " << QString::fromLatin1(msgAry.data());
                return;
            }

            // Retry
            ++retries;
            qDebug() << "RETRY (" << host << port << ")" << "UDP writeDatagram: " << QString::fromLatin1(msgAry.data());
            socket->writeDatagram(msgAry, host, port);
            QTimer::singleShot(this->retransmitDelay, Qt::PreciseTimer, this, callback);
        };

        /* TODO?: Use a timer attached to the class which we stop and change the callback to
           instead. This way we avoid calling old callbacks which have been converted to
           do-nothings. OTOH this way is easier converted to retransmitting always + in
           order semantics if we decide we want to replicate more of TCP in UDP (yey) in
           the future. */
        QTimer::singleShot(this->retransmitDelay, Qt::PreciseTimer, this, callback);
    }

    qDebug() << "(" << host << port << ")" << "UDP writeDatagram: " << QString::fromLatin1(msgAry.data());
    socket->writeDatagram(msgAry, host, port);
}

void UdpProtocol::pingServer()
{
    if (this->waitingForAnswer > this->pingMissesBeforeDisconnect)
    {
        // If we've been waiting for an answer longer than the update interval consider us as
        // having lost connection with the server.
        this->waitingForAnswer = 0;
        this->serverDisconnect();
        emit error(tr("Lost \"connection\" with server."));
        return;
    }

    ++this->waitingForAnswer;
    this->sendMsg("status");
}
