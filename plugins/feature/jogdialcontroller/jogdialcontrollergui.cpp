///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <algorithm>

#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QShortcut>

#include "device/deviceuiset.h"
#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "gui/glspectrum.h"
#include "maincore.h"
#include "mainwindow.h"

#include "ui_jogdialcontrollergui.h"
#include "jogdialcontroller.h"
#include "jogdialcontrollergui.h"

const QString JogdialControllerGUI::m_controlLabelFocused =
    "QLabel { background-color: rgb(85, 232, 85); color: black; border-radius: 4px; font-size: 18px; font-weight: bold; }";
const QString JogdialControllerGUI::m_controlLabelIdle =
    "QLabel { background-color: gray; color: white; border-radius: 4px; font-size: 18px; font-weight: bold; }";

JogdialControllerGUI* JogdialControllerGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
	JogdialControllerGUI* gui = new JogdialControllerGUI(pluginAPI, featureUISet, feature);
	return gui;
}

void JogdialControllerGUI::destroy()
{
	delete this;
}

void JogdialControllerGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
	applySettings(true);
}

QByteArray JogdialControllerGUI::serialize() const
{
    return m_settings.serialize();
}

bool JogdialControllerGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool JogdialControllerGUI::handleMessage(const Message& message)
{
    if (JogdialController::MsgConfigureJogdialController::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgConfigureJogdialController");
        const JogdialController::MsgConfigureJogdialController& cfg = (JogdialController::MsgConfigureJogdialController&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (JogdialController::MsgReportChannels::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgReportChannels");
        JogdialController::MsgReportChannels& report = (JogdialController::MsgReportChannels&) message;
        m_availableChannels = report.getAvailableChannels();
        updateChannelList();

        return true;
    }
    else if (JogdialController::MsgReportControl::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgReportControl");
        JogdialController::MsgReportControl& report = (JogdialController::MsgReportControl&) message;

        if (report.getControlMode() == JogdialController::ControlMode::Device) {
            ui->controlLabel->setText("D");
        } else if (report.getControlMode() == JogdialController::ControlMode::Channel) {
            ui->controlLabel->setText("C");
        } else {
            ui->controlLabel->setText("S");
        }

        // The motion parameters shown follow the control mode
        m_motionMode->setCurrentIndex(static_cast<int>(report.getControlMode()));

        return true;
    }
    else if (JogdialController::MsgControlSpectrum::match(message))
    {
        JogdialController::MsgControlSpectrum& cmd = (JogdialController::MsgControlSpectrum&) message;
        std::vector<DeviceUISet*>& deviceUISets = MainWindow::getInstance()->getDeviceUISets();
        int deviceSetIndex = cmd.getDeviceSetIndex();

        if ((deviceSetIndex >= 0) && (deviceSetIndex < (int) deviceUISets.size()))
        {
            GLSpectrum *spectrum = deviceUISets[deviceSetIndex]->getSpectrum();

            if (cmd.getAction() == JogdialController::MsgControlSpectrum::ZoomBy)
            {
                if (cmd.hasFrequencyOffset()) {
                    spectrum->zoomFrequencyBy(cmd.getValue(), cmd.getFrequencyOffset());
                } else {
                    spectrum->zoomFrequencyBy(cmd.getValue());
                }
            }
            else if (cmd.getAction() == JogdialController::MsgControlSpectrum::PanBy) {
                spectrum->panFrequencyBy(cmd.getValue());
            } else {
                spectrum->resetFrequencyZooming();
            }
        }
        else
        {
            qWarning("JogdialControllerGUI::handleMessage: MsgControlSpectrum: no device set %d", deviceSetIndex);
        }

        return true;
    }
    else if (JogdialController::MsgSelectChannel::match(message))
    {
        qDebug("JogdialControllerGUI::handleMessage: JogdialController::MsgSelectChannel");
        JogdialController::MsgSelectChannel& report = (JogdialController::MsgSelectChannel&) message;
        int index = report.getIndex();

        if ((index >= 0) && (index < m_availableChannels.size()))
        {
            ui->channels->blockSignals(true);
            ui->channels->setCurrentIndex(index);
            ui->channels->blockSignals(false);
        }

        return true;
    }

	return false;
}

void JogdialControllerGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void JogdialControllerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

JogdialControllerGUI::JogdialControllerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
	FeatureGUI(parent),
	ui(new Ui::JogdialControllerGUI),
	m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
	m_doApplySettings(true),
    m_selectedChannel(nullptr)
{
    m_feature = feature;
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/jogdialcontroller/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);

    // The control type indicator is as big as the start/stop button
    const QSize buttonSize = ui->startStop->sizeHint().expandedTo(QSize(32, 32));
    ui->controlLabel->setFixedSize(buttonSize);
    ui->controlLabel->setStyleSheet(m_controlLabelIdle);
    buildMotionUI();
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_jogdialController = reinterpret_cast<JogdialController*>(feature);
    m_jogdialController->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	connect(m_jogdialController, &Feature::stateChanged, this, &JogdialControllerGUI::updateFeatureState);
	updateFeatureState();

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
    this->installEventFilter(&m_commandKeyReceiver);

    // The controller keys only reach this window when it has the keyboard focus. This shortcut
    // works whichever window of the application has the focus. It is attached to the main window
    // as a shortcut is only active when its parent widget is visible
    m_focusShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_J), MainWindow::getInstance());
    m_focusShortcut->setContext(Qt::ApplicationShortcut);
    m_focusShortcut->setEnabled(false);
    connect(m_focusShortcut, &QShortcut::activated, this, &JogdialControllerGUI::takeKeyboardFocus);

    m_settings.setRollupState(&m_rollupState);

    displaySettings();
	applySettings(true);
    makeUIConnections();
    m_resizer.enableChildMouseTracking();
}

JogdialControllerGUI::~JogdialControllerGUI()
{
	delete m_focusShortcut.data();
	delete ui;
}

void JogdialControllerGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

void JogdialControllerGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void JogdialControllerGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
    displayMotion();
}

namespace {
//! Speed axis of the range slider: lowest speed and number of decades, per mode (kHz/s, Hz/s, window widths/s)
const double ringAxisMin[] = {0.1, 1.0, 0.001};
const int ringAxisDecades[] = {5, 6, 4};
}

//! Build the controls of the motion parameters in the "Motion" section: for each mode the speed range of the
//! ring (one slider with two handles), how quickly it reacts, the size of a jog click and the jog acceleration
void JogdialControllerGUI::buildMotionUI()
{
    QVBoxLayout *layout = ui->motionLayout;

    // The columns are sized from the text they hold so that nothing is cut whatever the font
    const QFontMetrics metrics(font());
    const int nameWidth = std::max({metrics.horizontalAdvance("Acceleration"), metrics.horizontalAdvance("Reaction"), metrics.horizontalAdvance("Mode")}) + 12;
    const int valueWidth = std::max({metrics.horizontalAdvance("300 kHz/s"), metrics.horizontalAdvance("100 kHz"), metrics.horizontalAdvance("1000 Hz"), metrics.horizontalAdvance("Defaults") + 30}) + 12;

    QFont bold = font();
    bold.setBold(true);
    QGridLayout *grid = new QGridLayout();
    grid->setColumnMinimumWidth(0, nameWidth);
    grid->setColumnMinimumWidth(2, valueWidth);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(3);
    int row = 0;

    QLabel *modeLabel = new QLabel(tr("Mode"));
    m_motionMode = new QComboBox();
    m_motionMode->addItems({tr("D device"), tr("C channel"), tr("S spectrum")});
    m_motionMode->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_motionMode->setMinimumContentsLength(12);
    m_motionMode->setFocusPolicy(Qt::NoFocus); // the keys must stay with the controller
    m_motionMode->setToolTip(tr("Control mode whose parameters are edited (follows the mode in use)"));
    m_motionDefaults = new QPushButton(tr("Defaults"));
    m_motionDefaults->setFocusPolicy(Qt::NoFocus);
    m_motionDefaults->setToolTip(tr("Restore the default parameters of this mode"));
    grid->addWidget(modeLabel, row, 0);
    grid->addWidget(m_motionMode, row, 1);
    grid->addWidget(m_motionDefaults, row, 2);
    row++;

    auto addTitle = [&](const QString& text) {
        QLabel *title = new QLabel(text);
        title->setFont(bold);
        grid->addWidget(title, row++, 0, 1, 3);
    };

    auto addSlider = [&](const QString& name, const QString& tip, QSlider*& slider, QLabel*& value) {
        QLabel *label = new QLabel(name);
        label->setToolTip(tip);
        slider = new QSlider(Qt::Horizontal);
        slider->setFocusPolicy(Qt::NoFocus);
        slider->setToolTip(tip);
        value = new QLabel();
        value->setMinimumWidth(valueWidth);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(label, row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(value, row, 2);
        row++;
    };

    addTitle(tr("Ring"));

    QLabel *rangeLabel = new QLabel(tr("Speed"));
    const QString rangeTip = tr("Left handle: speed of ring position 1. Right handle: speed of position 7. The positions in between follow a regular progression");
    rangeLabel->setToolTip(rangeTip);
    m_ringRange = new JogdialRangeSlider();
    m_ringRange->setToolTip(rangeTip);
    m_ringRangeValue = new QLabel();
    m_ringRangeValue->setMinimumWidth(valueWidth);
    m_ringRangeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(rangeLabel, row, 0);
    grid->addWidget(m_ringRange, row, 1);
    grid->addWidget(m_ringRangeValue, row, 2);
    row++;
    m_speedScale = new JogdialScale(10);
    grid->addWidget(m_speedScale, row++, 1);

    addSlider(tr("Reaction"), tr("How quickly the movement follows the ring: smooth on the left, snappy on the right"), m_reaction, m_reactionValue);
    m_reaction->setRange(0, 100);
    m_reactionScale = new JogdialScale();
    grid->addWidget(m_reactionScale, row++, 1);

    addTitle(tr("Jog wheel"));
    addSlider(tr("Click"), tr("Size of one jog click: Hz in channel mode, kHz in device mode, zoom per click in spectrum mode"), m_jogStep, m_jogStepValue);
    m_clickScale = new JogdialScale();
    grid->addWidget(m_clickScale, row++, 1);
    addSlider(tr("Acceleration"), tr("How much the jog accelerates when turned fast: off on the left, strong on the right"), m_jogAccel, m_jogAccelValue);
    m_jogAccel->setRange(0, 100);
    m_accelScale = new JogdialScale();
    grid->addWidget(m_accelScale, row++, 1);

    layout->addLayout(grid);

    m_ringRange->onChanged = [this](int low, int high) {
        JogdialControllerSettings::MotionParams& p = m_settings.m_motion[m_displayedMode];

        // Only the handle that moved changes its value: the other one keeps its exact setting
        if (low != ringAxis(p.m_ringMin)) {
            p.m_ringMin = ringValue(low);
        }
        if (high != ringAxis(p.m_ringMax)) {
            p.m_ringMax = ringValue(high);
        }

        motionChanged();
    };
    connect(m_reaction, &QSlider::valueChanged, this, [this](int value) {
        m_settings.m_motion[m_displayedMode].m_reaction = value;
        motionChanged();
    });
    connect(m_jogStep, &QSlider::valueChanged, this, [this](int value) {
        m_settings.m_motion[m_displayedMode].m_jogStep = jogStepValue(value);
        motionChanged();
    });
    connect(m_jogAccel, &QSlider::valueChanged, this, [this](int value) {
        m_settings.m_motion[m_displayedMode].m_jogAccel = value;
        motionChanged();
    });
    connect(m_motionMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_displayedMode = std::max(0, std::min(JogdialControllerSettings::m_modeCount - 1, index));
        displayMotion();
    });
    connect(m_motionDefaults, &QPushButton::clicked, this, [this]() {
        m_settings.m_motion[m_displayedMode] = JogdialControllerSettings::defaultMotionParams(m_displayedMode);
        displayMotion();
        m_settingsKeys.append("motion");
        applySettings();
    });
}

//! Value (kHz/s, Hz/s or window widths per second) at a position of the speed axis (100 units per decade)
double JogdialControllerGUI::ringValue(int axis) const
{
    return roundToOneDigit(ringAxisMin[m_displayedMode] * std::pow(10.0, axis / 100.0));
}

int JogdialControllerGUI::ringAxis(double value) const
{
    const int axis = (int) std::lround(100.0 * std::log10(std::max(value, 1e-9) / ringAxisMin[m_displayedMode]));
    return std::max(0, std::min(100 * ringAxisDecades[m_displayedMode], axis));
}

//! Click size: logarithmic 1 to 1000 in channel and device mode, 2 to 50 percent in spectrum mode
double JogdialControllerGUI::jogStepValue(int slider) const
{
    return (m_displayedMode == 2) ? slider / 100.0 : std::pow(10.0, slider / 100.0);
}

int JogdialControllerGUI::jogStepSlider(double value) const
{
    return (m_displayedMode == 2) ? (int) std::lround(value * 100.0) : (int) std::lround(100.0 * std::log10(std::max(value, 1.0)));
}

//! value: kHz/s (device), Hz/s (channel), window widths per second (spectrum)
QString JogdialControllerGUI::formatSpeed(int mode, double value)
{
    if (mode == 2) {
        return QString("%1 %/s").arg(value * 100.0, 0, 'g', 3);
    }

    const double hz = (mode == 0) ? value * 1000.0 : value;

    if (hz >= 1e6) {
        return QString("%1 MHz/s").arg(hz / 1e6, 0, 'g', 3);
    } else if (hz >= 1e3) {
        return QString("%1 kHz/s").arg(hz / 1e3, 0, 'g', 3);
    } else {
        return QString("%1 Hz/s").arg(hz, 0, 'g', 3);
    }
}

//! One significant digit: 39.8 becomes 40
double JogdialControllerGUI::roundToOneDigit(double value)
{
    if (value <= 0.0) {
        return value;
    }

    const double scale = std::pow(10.0, std::floor(std::log10(value)));
    return std::round(value / scale) * scale;
}

//! Texts next to the controls, from the parameters of the displayed mode
void JogdialControllerGUI::displayMotionTexts()
{
    const JogdialControllerSettings::MotionParams& p = m_settings.m_motion[m_displayedMode];

    m_ringRangeValue->setText(formatSpeed(m_displayedMode, p.m_ringMin) + "\n" + formatSpeed(m_displayedMode, p.m_ringMax));
    m_reactionValue->setText(QString("%1 ms").arg(p.reactionMs()));

    if (m_displayedMode == 2) {
        m_jogStepValue->setText(QString("%1 %").arg(p.m_jogStep * 100.0, 0, 'f', 0));
    } else {
        m_jogStepValue->setText(QString("%1 %2").arg(p.m_jogStep, 0, 'f', 0).arg(m_displayedMode == 0 ? "kHz" : "Hz"));
    }

    m_jogAccelValue->setText((p.m_jogAccel == 0) ? tr("off") : QString("%1 %").arg(p.m_jogAccel));
}

namespace {
QString shortNumber(double value)
{
    return QString::number(value, 'g', 3);
}

QString shortHz(double hz)
{
    if (hz >= 1e6) {
        return shortNumber(hz / 1e6) + "M";
    } else if (hz >= 1e3) {
        return shortNumber(hz / 1e3) + "k";
    } else {
        return shortNumber(hz);
    }
}
}

//! Graduations under the sliders, in proportion to the values. Light ticks have no label
void JogdialControllerGUI::displayMotionScales()
{
    typedef JogdialScale::Tick Tick;
    const int decades = ringAxisDecades[m_displayedMode];

    // Speed: a main tick per decade, light ticks at 2 and 5
    QVector<Tick> speed;

    for (int d = 0; d <= decades; d++)
    {
        const double value = ringAxisMin[m_displayedMode] * std::pow(10.0, d);
        const QString label = (m_displayedMode == 2) ? shortNumber(value * 100.0) + "%" : shortHz((m_displayedMode == 0) ? value * 1000.0 : value);
        speed.append({(double) d / decades, label});

        if (d < decades)
        {
            speed.append({(d + 0.301) / decades, QString()});
            speed.append({(d + 0.699) / decades, QString()});
        }
    }

    m_speedScale->setTicks(speed);

    // Reaction: the slider is linear in the logarithm of the time constant (250 ms to 20 ms)
    QVector<Tick> reaction;
    const int mainMs[] = {250, 150, 100, 60, 40, 20};
    const int lightMs[] = {200, 80, 30};

    for (int ms : mainMs) {
        reaction.append({std::log(ms / 250.0) / std::log(20.0 / 250.0), QString::number(ms)});
    }
    for (int ms : lightMs) {
        reaction.append({std::log(ms / 250.0) / std::log(20.0 / 250.0), QString()});
    }

    m_reactionScale->setTicks(reaction);

    // Click: logarithmic 1 to 1000 (channel, device) or linear 2 to 50 percent (spectrum)
    QVector<Tick> click;

    if (m_displayedMode == 2)
    {
        for (int v = 2; v <= 50; v += 4) {
            click.append({(v - 2) / 48.0, QString()});
        }
        for (int v : {2, 10, 20, 30, 40, 50}) {
            click.append({(v - 2) / 48.0, QString("%1%").arg(v)});
        }
    }
    else
    {
        for (int d = 0; d <= 3; d++)
        {
            click.append({d / 3.0, shortHz(std::pow(10.0, d))});

            if (d < 3)
            {
                click.append({(d + 0.301) / 3.0, QString()});
                click.append({(d + 0.699) / 3.0, QString()});
            }
        }
    }

    m_clickScale->setTicks(click);

    QVector<Tick> accel;

    for (int v = 0; v <= 100; v += 10) {
        accel.append({v / 100.0, (v == 0) ? tr("off") : (((v == 50) || (v == 100)) ? QString::number(v) : QString())});
    }

    m_accelScale->setTicks(accel);
}

//! Show the parameters of the selected mode on the controls
void JogdialControllerGUI::displayMotion()
{
    const JogdialControllerSettings::MotionParams& p = m_settings.m_motion[m_displayedMode];

    // The handles stop on values of one significant digit: 1, 2, 3 ... 9 times a power of ten
    QVector<int> candidates;

    for (int d = 0; d < ringAxisDecades[m_displayedMode]; d++)
    {
        for (int k = 1; k <= 9; k++) {
            candidates.append((int) std::lround(100.0 * (d + std::log10((double) k))));
        }
    }

    candidates.append(100 * ringAxisDecades[m_displayedMode]);
    m_ringRange->setCandidates(candidates);
    m_ringRange->setBounds(0, 100 * ringAxisDecades[m_displayedMode], 40);
    m_ringRange->setValues(ringAxis(p.m_ringMin), ringAxis(p.m_ringMax));

    m_reaction->blockSignals(true);
    m_reaction->setValue(p.m_reaction);
    m_reaction->blockSignals(false);

    m_jogStep->blockSignals(true);

    if (m_displayedMode == 2) {
        m_jogStep->setRange(2, 50);
    } else {
        m_jogStep->setRange(0, 300);
    }

    m_jogStep->setValue(jogStepSlider(p.m_jogStep));
    m_jogStep->blockSignals(false);

    m_jogAccel->blockSignals(true);
    m_jogAccel->setValue(p.m_jogAccel);
    m_jogAccel->blockSignals(false);

    if (m_motionMode->currentIndex() != m_displayedMode)
    {
        m_motionMode->blockSignals(true);
        m_motionMode->setCurrentIndex(m_displayedMode);
        m_motionMode->blockSignals(false);
    }

    displayMotionTexts();
    displayMotionScales();
}

//! A control was moved: show the consequences and send the parameters to the controller
void JogdialControllerGUI::motionChanged()
{
    displayMotionTexts();
    m_settingsKeys.append("motion");
    applySettings();
}

void JogdialControllerGUI::updateChannelList()
{
    ui->channels->blockSignals(true);
    ui->channels->clear();

    QList<JogdialControllerSettings::AvailableChannel>::const_iterator it = m_availableChannels.begin();
    int selectedItem = -1;

    for (int i = 0; it != m_availableChannels.end(); ++it, i++)
    {
        ui->channels->addItem(tr("%1%2:%3 %4")
            .arg(it->m_tx ? "T" : "R")
            .arg(it->m_deviceSetIndex)
            .arg(it->m_channelIndex)
            .arg(it->m_channelId)
        );

        if (it->m_channelAPI == m_selectedChannel) {
            selectedItem = i;
        }
    }

    int currentSelectedChannelIndex = ui->channels->currentIndex();
    ui->channels->blockSignals(false);

    if (m_availableChannels.size() > 0)
    {
        if (selectedItem >= 0) {
            ui->channels->setCurrentIndex(selectedItem);
        } else {
            ui->channels->setCurrentIndex(0);
        }
    }

    if (currentSelectedChannelIndex == ui->channels->currentIndex()) {
        on_channels_currentIndexChanged(ui->channels->currentIndex()); // force sending
    }
}

void JogdialControllerGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void JogdialControllerGUI::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        setFocus();
        setFocusPolicy(Qt::StrongFocus);
        // The captured keys must not be passed on, else they also reach the widgets
        // that contain this one (for example arrow keys scrolling the window)
        m_commandKeyReceiver.setPass(false);
        m_focusShortcut->setEnabled(true);
        connect(&m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
            m_jogdialController, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
    }
    else
    {
        disconnect(&m_commandKeyReceiver, SIGNAL(capturedKey(Qt::Key, Qt::KeyboardModifiers, bool)),
            m_jogdialController, SLOT(commandKeyPressed(Qt::Key, Qt::KeyboardModifiers, bool)));
        m_commandKeyReceiver.setPass(true);
        m_focusShortcut->setEnabled(false);
        setFocusPolicy(Qt::NoFocus);
        clearFocus();
    }

    JogdialController::MsgStartStop *message = JogdialController::MsgStartStop::create(checked);
    m_jogdialController->getInputMessageQueue()->push(message);
}

void JogdialControllerGUI::on_devicesRefresh_clicked()
{
    JogdialController::MsgRefreshChannels *msg = JogdialController::MsgRefreshChannels::create();
    m_jogdialController->getInputMessageQueue()->push(msg);
}

void JogdialControllerGUI::on_channels_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_availableChannels.size()))
    {
        m_selectedChannel = m_availableChannels[index].m_channelAPI;
        JogdialController::MsgSelectChannel *msg = JogdialController::MsgSelectChannel::create(index);
        m_jogdialController->getInputMessageQueue()->push(msg);
    }
}

void JogdialControllerGUI::tick()
{
}

void JogdialControllerGUI::updateFeatureState()
{
    updateStartStopButton(ui->startStop);
}

void JogdialControllerGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
	    JogdialController::MsgConfigureJogdialController* message = JogdialController::MsgConfigureJogdialController::create( m_settings, m_settingsKeys, force);
	    m_jogdialController->getInputMessageQueue()->push(message);
	}

    m_settingsKeys.clear();
}

void JogdialControllerGUI::focusInEvent(QFocusEvent*)
{
    qDebug("JogdialControllerGUI::focusInEvent");
    ui->controlLabel->setStyleSheet(m_controlLabelFocused); // green
}

void JogdialControllerGUI::focusOutEvent(QFocusEvent*)
{
    qDebug("JogdialControllerGUI::focusOutEvent");
    ui->controlLabel->setStyleSheet(m_controlLabelIdle); // gray
}

void JogdialControllerGUI::takeKeyboardFocus()
{
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

void JogdialControllerGUI::makeUIConnections()
{
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &JogdialControllerGUI::on_startStop_toggled);
	QObject::connect(ui->devicesRefresh, &QPushButton::clicked, this, &JogdialControllerGUI::on_devicesRefresh_clicked);
	QObject::connect(ui->channels, qOverload<int>(&QComboBox::currentIndexChanged), this, &JogdialControllerGUI::on_channels_currentIndexChanged);
}
