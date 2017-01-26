// -*- Mode: C++/lah -*-

#include <QWidget>
#include <QTcpSocket>

/* TODO: replace sliders with own subclass of groupbox containing a nifty 2-channel slider of
 * some sort */
#include <QSlider>

class Window : public QWidget
{
    Q_OBJECT

public:
    Window(const char *host, unsigned port);
    virtual ~Window();

public slots:
    void error(const QString& _details);
    void fatalError(const QString& _details);
    void sliderValueUpdate(int value);

private:
    bool serverConnect(const char *host, unsigned port);
    bool serverDisconnect();
    
    // VolumeSlider *frontSlider;
    // VolumeSlider *censubSlider;
    // VolumeSlider *rearSlider;

    QSlider *frontSlider;
    QSlider *censubSlider;
    QSlider *rearSlider;

    QTcpSocket *socket;
};
