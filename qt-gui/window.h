// -*- Mode: C++/lah -*-

#include <QWidget>
#include <QTcpSocket>

#include "VolumeSlider.h"
#include "ConnectionBox.h"

class Window : public QWidget
{
    Q_OBJECT

public:
    Window();
    Window(const QString &host);
    Window(const QString &host, quint16 port);
    virtual ~Window();

public slots:
    void error(const QString& _details);
    void fatalError(const QString& _details);
    /// \brief Disables volume sliders
    void sliderDisable();
    /// \brief Enables volume sliders
    void sliderEnable();
    /// \brief Read status message from server
    void readStatusMessage();

private slots:
    void serverConnect(const QString &host, quint16 port);
    void serverDisconnect();

private:
    /// \brief Internal method to setup socket object
    void socketSetup();

    /// \brief Parse and apply status message from server
    void parseStatusMessage(const char *status);

    char command[128]; // Stores command to send to server/last message sent to server

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
    void sendMsg(const char *data);

    ConnectionBox *connectionBox;

    VolumeSlider *masterSlider;
    LRVolumeSlider *frontSlider;
    LRVolumeSlider *censubSlider;
    LRVolumeSlider *rearSlider;

    QTcpSocket *socket;
};
