#include <QGroupBox>
#include <QSlider>
#include <QCheckBox>

class LRVolumeSlider : public QGroupBox
{
    Q_OBJECT

public:
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

signals:
    // TODO: replace lValueChanged/rValueChanged etc. with single signal that instead sends two
    // values (makes it easier to transition to stackedlayout version where we can switch
    // between dual/single sliders)
    void lValueChanged(int newValue);
    void rValueChanged(int newValue);
    void rMuteStateChanged(bool newValue);
    void lMuteStateChanged(bool newValue);

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
    VolumeSlider(const QString &title, QWidget *parent);

signals:
    void valueChanged(int newValue);
    void muteStateChanged(bool state);

private:
    QSlider *slider;
    QCheckBox *muteBox;
};
