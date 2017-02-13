#include <QGroupBox>
#include <QSlider>
#include <QCheckBox>

class LRVolumeSlider : public QGroupBox
{
    Q_OBJECT

public:
    static const int maxVal = 99; //!< max value of volume slider
    static const int minVal = 0;  //!< min value of volume slider

    /**
     * \brief Construct an LRVolumeSlider with default names for the
     *        channels.
     *
     * \param title  the title of the LRVolumeSlider
     * \param parent the parent of the widget
     */
    LRVolumeSlider(const QString &title,
                   QWidget *parent);

    /**
     * \brief Construct an LRVolumeSlider.
     *
     * \param title         the title of the LRVolumeSlider
     * \param parent        the parent of the widget
     * \param lLabelString  label for the left channel
     * \param rLabelString  label for the right channel
     */
    LRVolumeSlider(const QString &title,
                   QWidget *parent,
                   const QString &lLabelString,
                   const QString &rLabelString);

    /**
     * \brief get value of sliders
     *
     * \param [out] lValue   store value of left slider here
     * \param [out] rValue   store value of right slider here
     */
    void value(int &lValue, int &rValue) const;

signals:
    void valueChanged(int lValue, int rValue);
    void muteStateChanged(bool lState, bool rState);

public slots:
    // These two slots are a bit of an ugly hack that can be used to force the valueChanged
    // signal to be emitted, even though the actual value hasn't changed
    void emitValueChanged();
    void emitMuteStateChanged();

private:
    QSlider *lSlider;
    QSlider *rSlider;
    QCheckBox *lMuteBox;
    QCheckBox *rMuteBox;
    QCheckBox *lockBox;
};

class VolumeSlider : public QGroupBox
{
    Q_OBJECT

public:
    static const int maxVal = LRVolumeSlider::maxVal; //!< max value of volume slider
    static const int minVal = LRVolumeSlider::minVal; //!< min value of volume slider

    VolumeSlider(const QString &title, QWidget *parent);

    int value() const;

signals:
    void valueChanged(int newValue);
    void muteStateChanged(bool state);

private:
    QSlider *slider;
    QCheckBox *muteBox;
};
