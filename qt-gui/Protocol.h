// -*- Mode: C++/lah -*-

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>

class Protocol : public QObject
{
    Q_OBJECT

public:

    struct ServerStatus
    {
        int fl_level; int fr_level; int fl_mute; int fr_mute;
        int sub_level; int cen_level; int sub_mute; int cen_mute;
        int rl_level; int rr_level; int rl_mute; int rr_mute;
        int global_mute;
    };

    /**
     *  \brief Helper method to construct and send a command without any parameters. Stores sent
     *         command in object member command.
     */
    void sendCmd(const char *cmd);
    /**
     * \brief Helper method to construct and send a command with an int parameter. Stores sent
     *        command in object member command.
     */
    void sendCmd(const char *cmd, int level);
    /**
     * \brief Helper method to construct and send a command with a channel and level parameter.
     *        Stores sent command in object member command.
     */
    void sendCmd(const char *cmd, const char *chan, int level);

    /**
     * Send data to server
     */
    virtual void sendMsg(const char *data) =0;


public slots:
    virtual void serverConnect(const QString &host, quint16 port) =0;
    virtual void serverDisconnect() =0;

    /// \brief Receive status message from server
    virtual void receiveStatusMessage() =0;

signals:
    void connected();
    void disconnected();
    void error(const QString &msg);
    void statusUpdate(const ServerStatus &values);
    
protected:
    char command[128]; //!< Stores command to send to server/last message sent to server

    /// Called by sub-classes. Sets up the socket/signal connections that are the same for both sub-classes. 
    void socketSetup(QAbstractSocket *socket);

    /// Parse and apply status message from server
    void parseStatusMessage(const char *status);
};

class TcpProtocol : public Protocol
{
    Q_OBJECT
    
public:
    TcpProtocol();

public slots:
    void serverConnect(const QString &host, quint16 port) override;
    void serverDisconnect() override;

    void receiveStatusMessage() override;
    void sendMsg(const char *data) override;

private:
    QTcpSocket *socket;
};

class UdpProtocol : public Protocol
{
    Q_OBJECT

public:
    /**
     * Construct an UdpProtocol.
     *
     * @param updateInterval             How often we should ping the server for status updates.
     *
     * @param pingMissesBeforeDisconnect How many failed pings to allow before we consider the
     *                                   server as gone, and us as disconnected from it. Thus
     *                                   the timeout for our "connection" to the server is
     *                                   updateInterval * pingMissesBeforeDisconnect ms
     */
    UdpProtocol(int updateInterval=1000, unsigned pingMissesBeforeDisconnect=30);
    
public slots:
    void serverConnect(const QString &host, quint16 port) override;
    void serverDisconnect() override;

    void receiveStatusMessage() override;
    void sendMsg(const char *data) override;

private slots:
    void pingServer(); //!< Called by statusUpdateTimer

private:
    QHostAddress host;
    quint16 port;
    bool isConnected;
    QUdpSocket *socket;
    QTimer *statusUpdateTimer; //!< Used to update status periodically

    const unsigned pingMissesBeforeDisconnect;
    unsigned waitingForAnswer; //!< Incremented by pingServer and reset by receiveStatusMessage. Consider us
                               //!as having lost connection with server if we end up in pingServer
                               //!and this is larger than pingMissesBeforeDisconnect.
};

