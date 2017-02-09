#include "window.h"

#include <stdio.h>
#include <functional>

#include <QApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QHBoxLayout>

// Set to non-zero when debugging GUI without actually connecting to server
#define DEBUG_NO_CONNECT 0

static const unsigned TIMEOUT = 10000;

Window::Window(const QString &host, quint16 port)
{

    frontSlider  = new LRVolumeSlider("Front", this);
    censubSlider = new LRVolumeSlider("Center/Sub", this, "CEN", "SUB");
    rearSlider   = new LRVolumeSlider("Rear", this);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(frontSlider);
    layout->addWidget(censubSlider);
    layout->addWidget(rearSlider);

    this->setLayout(layout);

    if (DEBUG_NO_CONNECT)
    {
        socket = NULL;
    }
    else
    {
        socket = new QTcpSocket(this);
        if (!this->serverConnect(host, port))
            this->fatalError(tr("Could not connect to volume control server (host: %1, port: %2)").arg(host).arg(port));

        // Maybe not actually needed, since in our case this constructor runs before the event loop
        // is started so the sliders won't be triggering any events.
        if (!socket->waitForConnected(TIMEOUT))
            this->fatalError(tr("Timed out connecting to server"));
    }

    connect(frontSlider,  &LRVolumeSlider::lValueChanged,     [this](int newValue) { this->sendMsgHelper("set", "FL", newValue); });
    connect(frontSlider,  &LRVolumeSlider::rValueChanged,     [this](int newValue) { this->sendMsgHelper("set", "FR", newValue); });
    connect(frontSlider,  &LRVolumeSlider::lMuteStateChanged, [this](bool state)   { this->sendMsgHelper("mutechan", "FL", (int)state); });
    connect(frontSlider,  &LRVolumeSlider::rMuteStateChanged, [this](bool state)   { this->sendMsgHelper("mutechan", "FR", (int)state); });

    connect(censubSlider, &LRVolumeSlider::lValueChanged,     [this](int newValue) { this->sendMsgHelper("set", "CEN", newValue); });
    connect(censubSlider, &LRVolumeSlider::rValueChanged,     [this](int newValue) { this->sendMsgHelper("set", "SUB", newValue); });
    connect(censubSlider, &LRVolumeSlider::lMuteStateChanged, [this](bool state)   { this->sendMsgHelper("mutechan", "CEN", (int)state); });
    connect(censubSlider, &LRVolumeSlider::rMuteStateChanged, [this](bool state)   { this->sendMsgHelper("mutechan", "SUB", (int)state); });

    connect(rearSlider,   &LRVolumeSlider::lValueChanged,     [this](int newValue) { this->sendMsgHelper("set", "RL", newValue); });
    connect(rearSlider,   &LRVolumeSlider::rValueChanged,     [this](int newValue) { this->sendMsgHelper("set", "RR", newValue); });
    connect(rearSlider,   &LRVolumeSlider::lMuteStateChanged, [this](bool state)   { this->sendMsgHelper("mutechan", "RR", (int)state); });
    connect(rearSlider,   &LRVolumeSlider::rMuteStateChanged, [this](bool state)   { this->sendMsgHelper("mutechan", "RL", (int)state); });
}

Window::~Window()
{
    // Not really neccesary since sockets are closed on destruction (which Qt automatizes).
    // Although we might want to be nice and send "byebye" to the server?
    this->serverDisconnect();
}

void Window::error(const QString& _details)
{
    QMessageBox mbox(
        QMessageBox::Critical,
        tr("Error"),
        _details,
        QMessageBox::Ok,
        this);

    mbox.exec();
}

void Window::fatalError(const QString& _details)
{
    QMessageBox mbox(
        QMessageBox::Critical,
        tr("Fatal Error"),
        tr("%1 has encountered an error and cannot continue to work.\n"
           "Please press OK button to quit.").arg(qApp->applicationName()),
        QMessageBox::Ok,
        this);

    mbox.setDetailedText(_details);
    mbox.exec();

    // This might be called during the constructor (before qApp->exec) so defer quit call
    // TODO: figure out how to return non-zero code (QCoreApplication::exit(int) is static...)
    QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
    // QMetaObject::invokeMethod(qApp, "exit", Qt::QueuedConnection, Q_ARG(int, 1));
}

bool Window::serverConnect(const QString &host, quint16 port)
{
    socket->connectToHost(host, port);
    // TODO: check that we've connected/error handling (not neccesarily here if perhaps Qt
    // provides some fancy signal for it)
    // Try connecting the QAbstractSocket::error signal up to a slot in Window!

    // TODO: send "status" and receive state of volume controller as status string and update
    // sliders based on this

    return true;
}

bool Window::serverDisconnect()
{
    if (socket->state() == QTcpSocket::UnconnectedState)
        return true;

    // TODO: send byebye here?

    socket->close();
    return true;
}

// Size used for the temporary on-stack buffers we snprintf to when building commands
#define CMD_BUFFER_SIZE 128

void Window::sendMsgHelper(const char *cmd)
{
    char data[CMD_BUFFER_SIZE];
    snprintf(data, sizeof(data), "%s\n", cmd);
    this->sendMsg(data);
}

void Window::sendMsgHelper(const char *cmd, const char *chan)
{
    char data[CMD_BUFFER_SIZE];
    snprintf(data, sizeof(data), "%s %s\n", cmd, chan);
    this->sendMsg(data);
}

void Window::sendMsgHelper(const char *cmd, const char *chan, int level)
{
    char data[CMD_BUFFER_SIZE];
    snprintf(data, sizeof(data), "%s %s %d\n", cmd, chan, level);
    this->sendMsg(data);
}

void Window::sendMsg(const char *data)
{
    if (NULL != socket)
    {
        socket->write(data);
        if (!socket->waitForBytesWritten(TIMEOUT))
        {
            // TODO: disconnect here
            error(tr("Timed out sending command to server."));
            return;
        }

        // TODO: move reading the socket to a slot instead?
        if (!socket->waitForReadyRead(TIMEOUT))
        {
            // TODO: disconnect here
            error(tr("Timed out reading status message from server."));
            return;
        }
        char status[256];
        qint64 lineLength = socket->readLine(status, sizeof(status));
        if (lineLength == -1)
        {
            // TODO: set disconnected mode here (disabled sliders & all)
            error(tr("Problem reading status message from server."));
            return;
        }

        printf("Got status string: %s", status);

        // TODO: use the read status string here to update state of sliders
    }
    else
    {
        error(tr("Tried to send but socket not initialized"));
    }
}
