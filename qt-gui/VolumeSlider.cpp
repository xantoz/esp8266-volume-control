#include "VolumeSlider.h"

#include <Qt>
#include <QGridLayout>
#include <QLabel>

// use later if using a stackedlayout with simple slider and double slider switcheable?
#include <QSignalBlocker>

/* Function to set our sliders up, so that we don't have to repeat ourselves. */
static QSlider *sliderSettings(QSlider *slider)
{
    slider->setRange(VolumeSlider::minVal, VolumeSlider::maxVal);
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
            if (this->lockBox->isChecked()) {
                QSignalBlocker block(this->rSlider);
                this->rSlider->setValue(newValue);
            }
            emitValueChanged();
        });
    connect(rSlider, &QSlider::valueChanged, [this](int newValue) {
            if (this->lockBox->isChecked()) {
                QSignalBlocker block(this->lSlider);
                this->lSlider->setValue(newValue);
            }
            emitValueChanged();
        });
    connect(lMuteBox, &QCheckBox::stateChanged, [this](int state) {
            if (this->lockBox->isChecked()) {
                QSignalBlocker block(this->rMuteBox);
                this->rMuteBox->setCheckState(static_cast<Qt::CheckState>(state));
            }
            emitMuteStateChanged();
        });
    connect(rMuteBox, &QCheckBox::stateChanged, [this](int state) {
            if (this->lockBox->isChecked()) {
                QSignalBlocker block(this->lMuteBox);
                this->lMuteBox->setCheckState(static_cast<Qt::CheckState>(state));
            }
            emitMuteStateChanged();
        });
}

void LRVolumeSlider::value(int &lValue, int &rValue) const
{
    lValue = lSlider->value();
    rValue = rSlider->value();
}

void LRVolumeSlider::emitValueChanged()
{
    emit valueChanged(this->lSlider->value(), this->rSlider->value());
}

void LRVolumeSlider::emitMuteStateChanged()
{
    emit muteStateChanged(this->lMuteBox->isChecked(), this->rMuteBox->isChecked());
}

void LRVolumeSlider::setValues(int lValue, int rValue)
{
    // valueChanged signal would be sent twice, let's instead handle it manually
    QSignalBlocker lBlock(this->lSlider), rBlock(this->rSlider);

    // Force-uncheck lockBox if we're setting uneven values
    if (lValue != rValue)
        this->lockBox->setChecked(false);

    this->lSlider->setValue(lValue);
    this->rSlider->setValue(rValue);

    emitValueChanged();
}

void LRVolumeSlider::setMuteBoxes(bool lTicked, bool rTicked)
{
    // muteStateChanged signal would be sent twice, let's instead send it manually
    QSignalBlocker lBlock(this->lMuteBox), rBlock(this->rMuteBox);

    // Force-uncheck lockBox if mute state differs
    if (lTicked != rTicked)
        this->lockBox->setChecked(false);

    this->lMuteBox->setChecked(lTicked);
    this->rMuteBox->setChecked(rTicked);

    emitMuteStateChanged();
}

VolumeSlider::VolumeSlider(const QString &title, QWidget *parent) :
    QGroupBox(title, parent)
{
    QGridLayout *layout = new QGridLayout(this);

    slider = sliderSettings(new QSlider(this));
    muteBox = new QCheckBox(tr("Mute"), this);

    layout->addWidget(slider, 0, 0, Qt::AlignHCenter);
    layout->addWidget(muteBox, 1, 0, Qt::AlignHCenter);

    this->setLayout(layout);

    connect(slider, &QSlider::valueChanged, this, &VolumeSlider::valueChanged);
    connect(muteBox, &QCheckBox::stateChanged, [this](int state) {
            emit muteStateChanged(state != Qt::Unchecked);
        });
}

int VolumeSlider::value() const
{
    return slider->value();
}

void VolumeSlider::setValue(int newValue)
{
    slider->setValue(newValue);
}

void VolumeSlider::setMuteBox(bool ticked)
{
    muteBox->setChecked(ticked);
}
