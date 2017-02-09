#include "VolumeSlider.h"

#include <Qt>
#include <QGridLayout>
#include <QLabel>

// use later if using a stackedlayout with simple slider and double slider switcheable?
//#include <QSignalBlocker>

/* Function to set our sliders up, so that we don't have to repeat ourselves. */
static QSlider *sliderSettings(QSlider *slider)
{
    slider->setRange(0, 99);
    slider->setFocusPolicy(Qt::StrongFocus);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(10);
    slider->setSingleStep(1);

    return slider;
}

LRVolumeSlider::LRVolumeSlider(const QString &title,
                               QWidget *parent) :
    LRVolumeSlider(title, parent, "L", "R")
{
}

LRVolumeSlider::LRVolumeSlider(const QString &title,
                               QWidget *parent,
                               const QString &lLabelString,
                               const QString &rLabelString) :
    QGroupBox(title, parent)
{
    QGridLayout *layout = new QGridLayout(this);

    lSlider = sliderSettings(new QSlider(Qt::Vertical, this));
    rSlider = sliderSettings(new QSlider(Qt::Vertical, this));

    QLabel *lLabel = new QLabel(lLabelString);
    QLabel *rLabel = new QLabel(rLabelString);

    lMuteBox = new QCheckBox(tr("Mute"), this);
    rMuteBox = new QCheckBox(tr("Mute"), this);
    
    lockBox = new QCheckBox(tr("Lock sliders"), this);
    lockBox->setChecked(true); // Locked by default

    layout->addWidget(lSlider,  1, 0, Qt::AlignHCenter);
    layout->addWidget(rSlider,  1, 1, Qt::AlignHCenter);
    layout->addWidget(lMuteBox, 2, 0, Qt::AlignHCenter);
    layout->addWidget(rMuteBox, 2, 1, Qt::AlignHCenter);
    layout->addWidget(lLabel,   0, 0, Qt::AlignHCenter);
    layout->addWidget(rLabel,   0, 1, Qt::AlignHCenter);
    layout->addWidget(lockBox,  3, 0, 1, 2, Qt::AlignHCenter); // Lockbox spanning two columns
    this->setLayout(layout);

    // TODO: somehow macro away the redundancy here using a generic function?
    //       alternatively connect/disconnect signal-slot in lockBox stateChanged
    //       QSignalTransition?
    connect(lSlider, &QSlider::valueChanged, [this](int newValue) {
            if (this->lockBox->isChecked())
                this->rSlider->setValue(newValue);
        });
    connect(rSlider, &QSlider::valueChanged, [this](int newValue) {
            if (this->lockBox->isChecked())
                this->lSlider->setValue(newValue);
        });
    connect(lMuteBox, &QCheckBox::stateChanged, [this](int state) {
            if (this->lockBox->isChecked())
                this->rMuteBox->setCheckState(static_cast<Qt::CheckState>(state));
        });
    connect(rMuteBox, &QCheckBox::stateChanged, [this](int state) {
            if (this->lockBox->isChecked())
                this->lMuteBox->setCheckState(static_cast<Qt::CheckState>(state));
        });

    connect(lSlider, &QSlider::valueChanged, this, &LRVolumeSlider::lValueChanged);
    connect(rSlider, &QSlider::valueChanged, this, &LRVolumeSlider::rValueChanged);
    connect(lMuteBox, &QCheckBox::stateChanged, [this](int state) {
            emit lMuteStateChanged(state != Qt::Unchecked);
            });
    connect(rMuteBox, &QCheckBox::stateChanged, [this](int state) {
            emit rMuteStateChanged(state != Qt::Unchecked);
            });
}
