// -*- Mode: C++ -*-

#ifndef __CONNECTIONBOX_H
#define __CONNECTIONBOX_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QMetaObject>

class ConnectionBox : public QWidget
{
    Q_OBJECT

public:
    ConnectionBox();

    /// \brief Returns true if ConnectionBox is in connected state, false otherwise
    bool getConnected() const;

signals:
    /// \brief Emitted when the push-button is pushed in non-connected state
    void connect(const QString &host, quint16 port);
    /// \brief Emitted when the push-button is pushed in connected state
    void disconnect();

public slots:
    /// \brief Set values of the textboxes
    void setValues(const QString &host, quint16 port);

    /// \brief Clicks button
    void click();

    /**
     * \brief Inform widget that we're connected now and change the pushbutton to a disconnect
     *        button. State is not changed automatically by the PushButton. Should be connected
     *        to something like the connect signal of a QTcpSocket, so that the state of the
     *        ConnectionBox only changes if we truly managed to connect.
     */
    void setConnected();
    /**
     * \brief Inform widget that we're disconnected now and change the pushbutton to a connect.
     *        State is not changed automatically by the PushButton. Should be connected to
     *        something like the disconnect signal of a QTcpSocket, so that the state of the
     *        ConnectionBox changes even if we disconnect for other reasons than pressing the
     *        button.
     */
    void setDisconnected();

private slots:
    void emitConnect();

private:
    bool connected;

    QLineEdit *hostBox;
    QLineEdit *portBox;
    QPushButton *button;
    QMetaObject::Connection buttonConnection;
};

#endif
