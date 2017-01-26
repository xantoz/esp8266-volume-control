#include "window.h"

#include <stdio.h>

#include <QApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QHBoxLayout>

static QSlider *sliderSettings(QSlider *slider)
{
    slider->setRange(0, 99);
    slider->setFocusPolicy(Qt::StrongFocus);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(10);
    slider->setSingleStep(1);

    return slider;
}

Window::Window(const char *host, unsigned port)
{
    if (!this->serverConnect(host, port))
        this->fatalError(tr("Could not connect to volume control server (host: %1, port: %2)").arg(host).arg(port));

    frontSlider  = sliderSettings(new QSlider(Qt::Vertical, this));
    censubSlider = sliderSettings(new QSlider(Qt::Vertical, this));
    rearSlider   = sliderSettings(new QSlider(Qt::Vertical, this));

    connect(frontSlider,  &QSlider::valueChanged, this, &Window::sliderValueUpdate);
    connect(censubSlider, &QSlider::valueChanged, this, &Window::sliderValueUpdate);
    connect(rearSlider,   &QSlider::valueChanged, this, &Window::sliderValueUpdate);
    
    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(frontSlider);
    layout->addWidget(censubSlider);
    layout->addWidget(rearSlider);
    
    this->setLayout(layout);
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

void Window::sliderValueUpdate(int value)
{
    const char *chan;
    char data[128];

    // TODO: support changing L/R channels independently (requires making custom sliders too)
    
    if (sender() == frontSlider)
    {
        chan = "F";
        printf("F %d\n", value);
    }
    else if (sender() == censubSlider)
    {
        chan = "CENSUB";
        printf("CS %d\n", value);
    }
    else if (sender() == rearSlider)
    {
        chan = "R";
        printf("R %d\n", value);
    }
    else 
    {
        // This shouldn't happen
        printf("U %d\n", value);
        error("Unknown slider?!!");
        return;
    }

    // TODO: send command over socket here

    socket->flush();


    // TODO: use the read status string here to update state of sliders

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
