#include "Protocol.h"

#include <QHostInfo>
#include <memory>

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
    snprintf(command, sizeof(command), "%s", cmd);          // Need to copy because of logic in TcpProtcol::recieveSocket
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
UdpProtocol::UdpProtocol(int updateInterval, unsigned _pingMissesBeforeDisconnect) :
    host(QHostAddress::Null), port(0), isConnected(false),
    pingMissesBeforeDisconnect(_pingMissesBeforeDisconnect), waitingForAnswer(0)
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
        waitingForAnswer = 0;
        char status[socket->pendingDatagramSize() + 1];
        qint64 size = socket->readDatagram(status, socket->pendingDatagramSize());
        status[(size >= 0) ? size : 0] = '\0'; // NUL-terminate
        qDebug() << "Got status message (size: " << size << ")" << status;

        if (size == -1)
        {
            emit error(tr("Problem reading status message from server. Disconnecting."));
            serverDisconnect();
        }
        else if (0 == strncmp(status, "ERROR", 5))
        {
            // Parse ERROR message
            QString msg = tr("Got error message from server:");
            msg.append(status+4); // Since strncmp passed we know there's at least 5 chars in this string
            qDebug() << "ERROR: " << msg;
            emit error(msg);
        }
        else
        {
            parseStatusMessage(status);
        }
    }
}

void UdpProtocol::sendMsg(const char *msg)
{
    qDebug() << "(" << host << port << ")" << "UDP writeDatagram:" << msg;
    socket->writeDatagram(msg, strlen(msg), host, port);
}

void UdpProtocol::pingServer()
{
    if (waitingForAnswer > pingMissesBeforeDisconnect)
    {
        // If we've been waiting for an answer longer than the update interval consider us as
        // having lost connection with the server.
        waitingForAnswer = 0;
        serverDisconnect();
        emit error(tr("Lost \"connection\" with server."));
        return;
    }

    ++waitingForAnswer;
    this->sendMsg("status");
}
