// -*- Mode: C++/lah -*-

#include <QWidget>
#include <QTcpSocket>

#include "VolumeSlider.h"

class Window : public QWidget
{
    Q_OBJECT

public:
    Window(const QString &host, quint16 port);
    virtual ~Window();

public slots:
    void error(const QString& _details);
    void fatalError(const QString& _details);

private:
    bool serverConnect(const QString &host, quint16 port);
    bool serverDisconnect();

    /** Helper method to construct and send a command without any parameters */
    void sendMsgHelper(const char *cmd);
    /** Helper method to construct and send a command with an int parameter */
    void sendMsgHelper(const char *cmd, int level);
    /** Helper method to construct and send a command with a channel and level parameter */
    void sendMsgHelper(const char *cmd, const char *chan, int level);

    /** Send data to server */
    void sendMsg(const char *data);

    VolumeSlider *masterSlider;
    LRVolumeSlider *frontSlider;
    LRVolumeSlider *censubSlider;
    LRVolumeSlider *rearSlider;

    QTcpSocket *socket;
};
