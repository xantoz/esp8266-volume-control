#include "window.h"

#include <stdio.h>
#include <functional>

#include <QtGlobal>
#include <QApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

static const quint16 DEFAULT_PORT = 1128;

Window::Window(Protocol *_protocol) :
    protocol(_protocol)
{
    using namespace std::placeholders;

    masterSlider = new VolumeSlider(tr("Master"), this);
    masterSlider->setValue(VolumeSlider::maxVal);
    frontSlider  = new LRVolumeSlider(tr("Front"), this);
    censubSlider = new LRVolumeSlider(tr("Center/Sub"), this, "CEN", "SUB");
    rearSlider   = new LRVolumeSlider(tr("Rear"), this);

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
        // Optimize when both channels same value
        if (lValue == rValue) {
            this->protocol->sendCmd("set", bothChan, lValue);
        } else {
            this->protocol->sendCmd("set", lChan, lValue);
            this->protocol->sendCmd("set", rChan, rValue);
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
            this->protocol->sendCmd("mutechan", bothChan, (int)lState);
        } else {
            this->protocol->sendCmd("mutechan", lChan, (int)lState);
            this->protocol->sendCmd("mutechan", rChan, (int)rState);
        }
    };
    connect(frontSlider,  &LRVolumeSlider::muteStateChanged, std::bind(setMute, "F",      "FL",  "FR",  _1, _2));
    connect(censubSlider, &LRVolumeSlider::muteStateChanged, std::bind(setMute, "CENSUB", "CEN", "SUB", _1, _2));
    connect(rearSlider,   &LRVolumeSlider::muteStateChanged, std::bind(setMute, "R",      "RL",  "RR",  _1, _2));

    connect(masterSlider, &VolumeSlider::valueChanged, [this](int level) { this->protocol->sendCmd("setmaster", level); });
    connect(masterSlider, &VolumeSlider::muteStateChanged, [this](bool state) { this->protocol->sendCmd("mute", (int)state); });

    // Sliders disabled by default
    this->sliderDisable();

    // Set up ConnectionBox
    connectionBox->setValues("", DEFAULT_PORT);
    connect(connectionBox, &ConnectionBox::connect,    protocol, &Protocol::serverConnect);
    connect(connectionBox, &ConnectionBox::disconnect, protocol, &Protocol::serverDisconnect);

    // Set up protocol (but don't connect to server just yet)
    connect(protocol, &Protocol::disconnected, this, &Window::sliderDisable);
    connect(protocol, &Protocol::connected,    this, &Window::sliderEnable);
    connect(protocol, &Protocol::disconnected, connectionBox, &ConnectionBox::setDisconnected);
    connect(protocol, &Protocol::connected,    connectionBox, &ConnectionBox::setConnected);
    connect(protocol, &Protocol::disconnected, []() { qDebug() << "Disconnected"; });
    connect(protocol, &Protocol::connected,    []() { qDebug() << "Connected"; });
    connect(protocol, &Protocol::error, [this](const QString &errorString) {
            this->error(errorString);
            this->connectionBox->setDisconnected(); // Need to reset connectionBox on failure during connection and such
        });
    connect(protocol, &Protocol::statusUpdate, this, &Window::setSliders);
}

Window::Window(Protocol *_protocol, const QString &host) :
    Window(_protocol, host, DEFAULT_PORT)
{
}

Window::Window(Protocol *_protocol, const QString &host, quint16 port) :
    Window(_protocol)
{
    connectionBox->setValues(host, port);
    connectionBox->click();
}

Window::~Window()
{
    // Not really neccesary since sockets are closed on destruction (which Qt automates).
    // Although we might want to be nice and send "byebye" to the server?
    protocol->serverDisconnect();
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

void Window::setSliders(const Protocol::ServerStatus &values)
{
    // We're just adjusting our sliders to server reality, don't send any signals
    QSignalBlocker
        frontBlock(frontSlider),
        censubBlock(censubSlider),
        rearBlock(rearSlider),
        masterBlock(masterSlider);

    frontSlider->setValues(values.fl_level, values.fr_level);
    frontSlider->setMuteBoxes(values.fl_mute, values.fr_mute);
    censubSlider->setValues(values.cen_level, values.sub_level); // NOTE: Argument order!
    censubSlider->setMuteBoxes(values.cen_mute, values.sub_mute);
    rearSlider->setValues(values.rl_level, values.rr_level);
    rearSlider->setMuteBoxes(values.rl_mute, values.rr_mute);
    masterSlider->setValue(values.master);
    masterSlider->setMuteBox(values.global_mute);
}
