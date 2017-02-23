// -*- Mode: C++/lah -*-

#include <QWidget>


#include "VolumeSlider.h"
#include "ConnectionBox.h"
#include "Protocol.h"

class Window : public QWidget
{
    Q_OBJECT

public:
    Window(Protocol *protocol);
    Window(Protocol *protocol, const QString &host);
    Window(Protocol *protocol, const QString &host, quint16 port);
    virtual ~Window();

public slots:
    void error(const QString &message);
    void error(const QString &message, const QString &details);
    void fatalError(const QString &details);
    /// \brief Disables volume sliders
    void sliderDisable();
    /// \brief Enables volume sliders
    void sliderEnable();

    /// Set all sliders at once
    void setSliders(const Protocol::ServerStatus &values);

private:
    ConnectionBox *connectionBox;

    Protocol *protocol;

    VolumeSlider *masterSlider;
    LRVolumeSlider *frontSlider;
    LRVolumeSlider *censubSlider;
    LRVolumeSlider *rearSlider;
};
