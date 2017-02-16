#include "ConnectionBox.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QLabel>

ConnectionBox::ConnectionBox() :
    connected(false)
{
    QHBoxLayout *layout = new QHBoxLayout();

    layout->setAlignment(Qt::AlignRight);

    QLabel *hostLabel = new QLabel(tr("Host:"), this);
    QLabel *portLabel = new QLabel(tr("Port:"), this);
    hostLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    portLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    hostBox = new QLineEdit(this);
    portBox = new QLineEdit(this);
    button  = new QPushButton(tr("Connect"), this);

    hostBox->setMaximumWidth(150);

    portBox->setMaximumWidth(60);
    portBox->setMaxLength(5);
    portBox->setInputMask("D0000"); // First has to be non-zero + empty portNbr not ok

    button->setMaximumWidth(110);

    layout->addWidget(hostLabel);
    layout->addWidget(hostBox);
    layout->addWidget(portLabel);
    layout->addWidget(portBox);
    layout->addWidget(button);

    // TODO: only enable button if QLineEdits contain sanity
    // button->setEnabled(false);
    // connect(hostBox, &QLineEdit::editingFinished, )

    QObject::connect(hostBox, &QLineEdit::returnPressed, button, &QPushButton::click);
    QObject::connect(portBox, &QLineEdit::returnPressed, button, &QPushButton::click);

    buttonConnection = QObject::connect(button, &QPushButton::clicked, this, &ConnectionBox::emitConnect);

    this->setLayout(layout);

    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

bool ConnectionBox::getConnected() const
{
    return connected;
}

void ConnectionBox::setValues(const QString &host, quint16 port)
{
    hostBox->setText(host);
    portBox->setText(QString::number(port));
}

void ConnectionBox::click()
{
    button->click();
}

void ConnectionBox::emitConnect()
{
    // Show that we're connecting + disallow more clicks for the time being to avoid being able
    // to click connect multiple times before we have a stable connection (setConnected gets
    // called from QTcpSocket::connected)
    button->setText(tr("Connecting..."));
    button->setEnabled(false);

    hostBox->setEnabled(false);
    portBox->setEnabled(false);


    emit connect(hostBox->text(), portBox->text().toShort());
}

void ConnectionBox::setConnected()
{
    connected = true;
    hostBox->setEnabled(false);
    portBox->setEnabled(false);

    QObject::disconnect(buttonConnection);
    buttonConnection = QObject::connect(button, &QPushButton::clicked, this, &ConnectionBox::disconnect);
    button->setText(tr("Disconnect"));
    button->setEnabled(true);
}

void ConnectionBox::setDisconnected()
{
    connected = false;
    hostBox->setEnabled(true);
    portBox->setEnabled(true);

    QObject::disconnect(buttonConnection);
    buttonConnection = QObject::connect(button, &QPushButton::clicked, this, &ConnectionBox::emitConnect);
    button->setText(tr("Connect"));
    button->setEnabled(true);
}
