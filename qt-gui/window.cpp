#include "window.h"

#include <stdio.h>
#include <functional>
#include <memory>

#include <QtGlobal>
#include <QApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

// Set to non-zero when debugging GUI without actually connecting to server
#define DEBUG_NO_CONNECT 0

static const quint16 DEFAULT_PORT = 1128;
static const unsigned TIMEOUT = 10000;

Window::Window()
{
    using namespace std::placeholders;

    masterSlider = new VolumeSlider("Master", this);
    masterSlider->setValue(99);
    frontSlider  = new LRVolumeSlider("Front", this);
    censubSlider = new LRVolumeSlider("Center/Sub", this, "CEN", "SUB");
    rearSlider   = new LRVolumeSlider("Rear", this);

    connectionBox = new ConnectionBox();


    QVBoxLayout *vLayout = new QVBoxLayout(this);

    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(masterSlider);
    sliderLayout->addWidget(frontSlider);
    sliderLayout->addWidget(censubSlider);
    sliderLayout->addWidget(rearSlider);

    vLayout->addWidget(connectionBox, Qt::AlignRight);
    vLayout->addLayout(sliderLayout);

    this->setLayout(vLayout);

    auto setVol = [this](const char *bothChan, // TODO: Make this into a traditional private slot? + use QSignalMapper
                         const char *lChan,
                         const char *rChan,
                         int lValue, int rValue) {
        // Scale by master slider
        int maValue = this->masterSlider->value();
        lValue = (lValue * maValue) / VolumeSlider::maxVal;
        rValue = (rValue * maValue) / VolumeSlider::maxVal;

        // Optimize when both channels same value
        if (lValue == rValue) {
            this->sendCmd("set", bothChan, lValue);
        } else {
            this->sendCmd("set", lChan, lValue);
            this->sendCmd("set", rChan, rValue);
        }
    };
    connect(frontSlider,  &LRVolumeSlider::valueChanged, std::bind(setVol, "F",      "FL",  "FR",  _1, _2));
    connect(censubSlider, &LRVolumeSlider::valueChanged, std::bind(setVol, "CENSUB", "CEN", "SUB", _1, _2));
    connect(rearSlider,   &LRVolumeSlider::valueChanged, std::bind(setVol, "R",      "RL",  "RR",  _1, _2));

    auto setMute = [this](const char *bothChan,
                          const char *lChan,
                          const char *rChan,
                          bool lState, bool rState) {
        // Optimize for both channels, same value
        if (lState == rState) {
            this->sendCmd("mutechan", bothChan, (int)lState);
        } else {
            this->sendCmd("mutechan", lChan, (int)lState);
            this->sendCmd("mutechan", rChan, (int)rState);
        }
    };
    connect(frontSlider,  &LRVolumeSlider::muteStateChanged, std::bind(setMute, "F",      "FL",  "FR",  _1, _2));
    connect(censubSlider, &LRVolumeSlider::muteStateChanged, std::bind(setMute, "CENSUB", "CEN", "SUB", _1, _2));
    connect(rearSlider,   &LRVolumeSlider::muteStateChanged, std::bind(setMute, "R",      "RL",  "RR",  _1, _2));

    connect(masterSlider, &VolumeSlider::valueChanged, frontSlider,  &LRVolumeSlider::emitValueChanged);
    connect(masterSlider, &VolumeSlider::valueChanged, censubSlider, &LRVolumeSlider::emitValueChanged);
    connect(masterSlider, &VolumeSlider::valueChanged, rearSlider,   &LRVolumeSlider::emitValueChanged);
    connect(masterSlider, &VolumeSlider::muteStateChanged, [this](bool state) { this->sendCmd("mute", (int)state); });

    // Sliders disabled by default
    this->sliderDisable();

    // Set up socket (but don't connect just yet)
    this->socketSetup();

    connectionBox->setValues("", DEFAULT_PORT);
    connect(connectionBox, &ConnectionBox::connect,    this, &Window::serverConnect);
    connect(connectionBox, &ConnectionBox::disconnect, this, &Window::serverDisconnect);
}

Window::Window(const QString &host) :
    Window(host, DEFAULT_PORT)
{
}

Window::Window(const QString &host, quint16 port) :
    Window()
{
    connectionBox->setValues(host, port);
    connectionBox->click();
}

Window::~Window()
{
    // Not really neccesary since sockets are closed on destruction (which Qt automates).
    // Although we might want to be nice and send "byebye" to the server?
    this->serverDisconnect();
}

void Window::error(const QString& message)
{
    QMessageBox mbox(
        QMessageBox::Critical,
        tr("Error"),
        message,
        QMessageBox::Ok,
        this);

    mbox.exec();
}

void Window::error(const QString& message, const QString &details)
{
    QMessageBox mbox(
        QMessageBox::Critical,
        tr("Error"),
        message,
        QMessageBox::Ok,
        this);

    mbox.setDetailedText(details);
    mbox.exec();
}


void Window::fatalError(const QString& details)
{
    QMessageBox mbox(
        QMessageBox::Critical,
        tr("Fatal Error"),
        tr("%1 has encountered an error and cannot continue to work.\n"
           "Please press OK button to quit.").arg(qApp->applicationName()),
        QMessageBox::Ok,
        this);

    mbox.setDetailedText(details);
    mbox.exec();

    // This might be called during the constructor (before qApp->exec) so defer quit call
    // TODO: figure out how to return non-zero code (QCoreApplication::exit(int) is static...)
    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    // QMetaObject::invokeMethod(qApp, "exit", Qt::QueuedConnection, Q_ARG(int, 1));
}

void Window::socketSetup()
{
    if (DEBUG_NO_CONNECT)
    {
        this->socket = NULL;
    }
    else
    {
        this->socket = new QTcpSocket(this);

        connect(socket, &QTcpSocket::disconnected, this, &Window::sliderDisable);
        connect(socket, &QTcpSocket::disconnected, connectionBox, &ConnectionBox::setDisconnected);
        connect(socket, &QTcpSocket::disconnected, []() { qDebug() << "Disconnected"; });
        connect(socket, &QTcpSocket::connected,    this, &Window::sliderEnable);
        connect(socket, &QTcpSocket::connected,    connectionBox, &ConnectionBox::setConnected);
        connect(socket, &QTcpSocket::connected,    []() { qDebug() << "Connected"; });
        connect(socket, static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>
                (&QAbstractSocket::error), [this](QAbstractSocket::SocketError) {
                    auto errorString = this->socket->errorString();
                    qDebug() << "SOCKET ERROR " << errorString;
                    this->socket->abort();
                    this->error(errorString);
                    this->connectionBox->setDisconnected(); // Need to reset connectionBox on failure during connection and such
                });
        connect(socket, &QTcpSocket::readyRead, this, &Window::readStatusMessage);
    }
}

void Window::sliderDisable()
{
    masterSlider->setEnabled(false);
    frontSlider->setEnabled(false);
    censubSlider->setEnabled(false);
    rearSlider->setEnabled(false);
}

void Window::sliderEnable()
{
    masterSlider->setEnabled(true);
    frontSlider->setEnabled(true);
    censubSlider->setEnabled(true);
    rearSlider->setEnabled(true);
}

void Window::readStatusMessage()
{
    char status[512];
    qint64 lineLength = socket->readLine(status, sizeof(status));
    if (lineLength == -1)
    {
        error(tr("Problem reading status message from server. Disconnecting."));
        serverDisconnect();
        return;
    }

    qDebug() << "Got status string:" << QString(status).simplified();

    if (0 == strncmp(status, "ERROR", 5))
    {
        // Parse ERROR message
        QString msg = tr("Got error message from server:");
        msg.append(status+4); // Since strncmp passed we know there's at least 5 chars in this string
        qDebug() << "ERROR: " << msg;
        error(msg);
        return;
    }

    if (0 == strncmp(this->command, "status", 6))
    {
        // Parse and apply status message to sliders if we requested this status message
        // specifically using the status command

        qDebug() << "Parsing status string";
        parseStatusMessage(status);
    }
}

void Window::parseStatusMessage(const char *status)
{
    // Note: This assumes L/R positions of CEN/SUB as L = SUB and R = CEN ...
    // TODO?: instead implement named properties server-side (python dict-like syntax?)
    int fl_level, fr_level, fl_mute, fr_mute,
        sub_level, cen_level, sub_mute, cen_mute,
        rl_level, rr_level, rl_mute, rr_mute;
    int global_mute;
    if (13 != sscanf(status, "OK 0: ( %d , %d , %d , %d ) ; 1: ( %d , %d , %d , %d ) ; 2: ( %d , %d , %d , %d ) ; Mute: %d ",
                     &fl_level, &fr_level, &fl_mute, &fr_mute,
                     &sub_level, &cen_level, &sub_mute, &cen_mute,
                     &rl_level, &rr_level, &rl_mute, &rr_mute,
                     &global_mute))
    {
        qDebug() << "ERROR: Couldn't parse server message";
        error(tr("Couldn't parse server message"), QString(status).simplified());
        return;
    }

    // We're just adjusting our sliders to server reality, don't send any signals
    QSignalBlocker frontBlock(frontSlider), censubBlock(censubSlider), rearBlock(rearSlider);

    frontSlider->setValues(fl_level, fr_level);
    frontSlider->setMuteBoxes(fl_mute, fr_mute);
    censubSlider->setValues(cen_level, sub_level); // NOTE: Argument order!
    censubSlider->setMuteBoxes(cen_mute, sub_mute);
    rearSlider->setValues(rl_level, rr_level);
    rearSlider->setMuteBoxes(rl_mute, rr_mute);

    // TODO?: Make master slider a server-side property (because we'll be able to read it back)
    masterSlider->setValue(VolumeSlider::maxVal);
    masterSlider->setMuteBox(global_mute);
}

void Window::serverConnect(const QString &host, quint16 port)
{
    if (NULL == socket)
        return;

    // Fire-once connection (get server status on socket connect)
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(socket, &QTcpSocket::connected, [this, conn]() {
            this->sendCmd("status"); // Send status cmd
            this->disconnect(*conn);
        });
    socket->connectToHost(host, port);
}

void Window::serverDisconnect()
{
    if (NULL == socket)
        return;

    if (socket->state() == QTcpSocket::UnconnectedState)
        return;

    this->sendCmd("byebye");
    // TODO: read back 'CYA' here?

    socket->close();
}

void Window::sendCmd(const char *cmd)
{
    snprintf(command, sizeof(command), "%s\n", cmd);
    this->sendMsg(command);
}

void Window::sendCmd(const char *cmd, int level)
{
    snprintf(command, sizeof(command), "%s %d\n", cmd, level);
    this->sendMsg(command);
}

void Window::sendCmd(const char *cmd, const char *chan, int level)
{
    snprintf(command, sizeof(command), "%s %s %d\n", cmd, chan, level);
    this->sendMsg(command);
}

void Window::sendMsg(const char *data)
{
    if (NULL == socket)
    {
        qDebug() << "Tried to send but socket not initialized";
        return;
    }

    socket->write(data);
    if (!socket->waitForBytesWritten(TIMEOUT))
    {
        this->serverDisconnect();
        error(tr("Timed out sending command to server."));
        return;
    }

    socket->waitForReadyRead(TIMEOUT);
}
