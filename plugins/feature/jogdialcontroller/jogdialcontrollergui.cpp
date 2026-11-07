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
struct MotionFieldDef
{
    const char *m_name;
    int m_min;
    int m_max;
    const char *m_tooltip;
};

const MotionFieldDef motionFieldDefs[] = {
    {"Speed 1", 0, 350, "Ring: speed at position 1 (logarithmic scale)"},
    {"Ratio", 110, 800, "Ring: speed factor between two successive positions"},
    {"Accel", 10, 1000, "Ring: time to follow the ring when speeding up, in ms (lower is quicker)"},
    {"Brake", 10, 1000, "Ring: time to follow the ring when slowing down, in ms (lower is quicker)"},
    {"Start at", 2, 20, "Jog: clicks per second above which the jog accelerates"},
    {"Max gain", 1, 100, "Jog: maximum acceleration of the jog"},
    {"Zoom smooth", 20, 500, "Jog: smoothing time of the zoom animation, in ms (spectrum only)"},
    {"Retune", 20, 500, "Minimum delay between two retunings of the device, in ms (device only): applies to jog and ring"},
};

const double motionBaseMin[] = {0.1, 1.0, 0.001}; //!< Lowest value of the speed slider: kHz/s, Hz/s, window widths/s
}

//! Build the sliders of the motion parameters in the "Motion" section
void JogdialControllerGUI::buildMotionUI()
{
    QVBoxLayout *layout = ui->motionLayout;

    QHBoxLayout *top = new QHBoxLayout();
    QLabel *modeLabel = new QLabel(tr("Mode"));
    m_motionMode = new QComboBox();
    m_motionMode->addItems({tr("D device"), tr("C channel"), tr("S spectrum")});
    m_motionMode->setFocusPolicy(Qt::NoFocus); // the keys must stay with the controller
    m_motionMode->setToolTip(tr("Control mode whose jog and ring parameters are edited (follows the mode in use)"));
    m_motionDefaults = new QPushButton(tr("Defaults"));
    m_motionDefaults->setFocusPolicy(Qt::NoFocus);
    m_motionDefaults->setToolTip(tr("Restore the default parameters of this mode"));
    top->addWidget(modeLabel);
    top->addWidget(m_motionMode, 1);
    top->addWidget(m_motionDefaults);
    layout->addLayout(top);

    QGridLayout *grid = new QGridLayout();
    grid->setColumnStretch(1, 1);

    int gridRow = 0;

    for (int f = 0; f < MfCount; f++)
    {
        const char *header = nullptr;

        if (f == MfBase) {
            header = "Ring (shuttle)";
        } else if (f == MfJogRate) {
            header = "Jog wheel";
        } else if (f == MfRetune) {
            header = "Jog and ring";
        }

        if (header)
        {
            QLabel *title = new QLabel(tr(header));
            QFont font = title->font();
            font.setBold(true);
            title->setFont(font);
            grid->addWidget(title, gridRow++, 0, 1, 3);
        }

        MotionRow& row = m_motionRows[f];
        row.m_label = new QLabel(tr(motionFieldDefs[f].m_name));
        row.m_label->setToolTip(tr(motionFieldDefs[f].m_tooltip));
        row.m_slider = new QSlider(Qt::Horizontal);
        row.m_slider->setRange(motionFieldDefs[f].m_min, motionFieldDefs[f].m_max);
        row.m_slider->setFocusPolicy(Qt::NoFocus);
        row.m_slider->setToolTip(tr(motionFieldDefs[f].m_tooltip));
        row.m_value = new QLabel();
        row.m_value->setMinimumWidth(80);
        row.m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(row.m_label, gridRow, 0);
        grid->addWidget(row.m_slider, gridRow, 1);
        grid->addWidget(row.m_value, gridRow, 2);
        gridRow++;
        connect(row.m_slider, &QSlider::valueChanged, this, [this, f](int value) { onMotionSliderChanged(f, value); });
    }

    layout->addLayout(grid);

    m_motionSpeeds = new QLabel();
    m_motionSpeeds->setWordWrap(true);
    m_motionSpeeds->setToolTip(tr("Resulting speed of ring positions 1 to 7"));
    layout->addWidget(m_motionSpeeds);

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

double JogdialControllerGUI::motionFieldValue(int field, int sliderValue) const
{
    switch (field)
    {
    case MfBase: return motionBaseMin[m_displayedMode] * std::pow(10.0, sliderValue / 100.0);
    case MfRatio: return sliderValue / 100.0;
    default: return sliderValue;
    }
}

int JogdialControllerGUI::motionFieldSlider(int field, double value) const
{
    int v;

    switch (field)
    {
    case MfBase: v = (int) std::lround(100.0 * std::log10(std::max(value, 1e-9) / motionBaseMin[m_displayedMode])); break;
    case MfRatio: v = (int) std::lround(value * 100.0); break;
    default: v = (int) std::lround(value); break;
    }

    return std::max(motionFieldDefs[field].m_min, std::min(motionFieldDefs[field].m_max, v));
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

QString JogdialControllerGUI::motionFieldText(int field, double value) const
{
    switch (field)
    {
    case MfBase: return formatSpeed(m_displayedMode, value);
    case MfRatio: return QString("x%1").arg(value, 0, 'f', 2);
    case MfJogRate: return QString("%1 /s").arg(value, 0, 'f', 0);
    case MfJogMax: return QString("x%1").arg(value, 0, 'f', 0);
    default: return QString("%1 ms").arg(value, 0, 'f', 0);
    }
}

//! Show the parameters of the selected mode on the sliders
void JogdialControllerGUI::displayMotion()
{
    const JogdialControllerSettings::MotionParams& p = m_settings.m_motion[m_displayedMode];
    const double values[MfCount] = {
        p.m_shuttleBase, p.m_shuttleRatio, (double) p.m_accelMs, (double) p.m_decelMs,
        p.m_jogGainMinRate, p.m_jogGainMax, (double) p.m_zoomMs, (double) p.m_retuneMs
    };

    for (int f = 0; f < MfCount; f++)
    {
        MotionRow& row = m_motionRows[f];
        row.m_slider->blockSignals(true);
        row.m_slider->setValue(motionFieldSlider(f, values[f]));
        row.m_slider->blockSignals(false);
        row.m_value->setText(motionFieldText(f, values[f]));
    }

    // Only the zoom (spectrum) and the retuning (device) have a meaning in one mode
    const bool enabled[MfCount] = {true, true, true, true, true, true, m_displayedMode == 2, m_displayedMode == 0};

    for (int f = 0; f < MfCount; f++)
    {
        m_motionRows[f].m_label->setEnabled(enabled[f]);
        m_motionRows[f].m_slider->setEnabled(enabled[f]);
        m_motionRows[f].m_value->setEnabled(enabled[f]);
    }

    QStringList speeds;

    for (int position = 1; position <= 7; position++) {
        speeds.append(QString("%1: %2").arg(position).arg(formatSpeed(m_displayedMode, p.m_shuttleBase * std::pow(p.m_shuttleRatio, position - 1))));
    }

    m_motionSpeeds->setText(tr("Ring positions: ") + speeds.join(tr("  ")));

    if (m_motionMode->currentIndex() != m_displayedMode)
    {
        m_motionMode->blockSignals(true);
        m_motionMode->setCurrentIndex(m_displayedMode);
        m_motionMode->blockSignals(false);
    }
}

void JogdialControllerGUI::onMotionSliderChanged(int field, int sliderValue)
{
    JogdialControllerSettings::MotionParams& p = m_settings.m_motion[m_displayedMode];
    const double value = motionFieldValue(field, sliderValue);

    switch (field)
    {
    case MfBase: p.m_shuttleBase = value; break;
    case MfRatio: p.m_shuttleRatio = value; break;
    case MfAccel: p.m_accelMs = (int) value; break;
    case MfDecel: p.m_decelMs = (int) value; break;
    case MfJogRate: p.m_jogGainMinRate = value; break;
    case MfJogMax: p.m_jogGainMax = value; break;
    case MfZoom: p.m_zoomMs = (int) value; break;
    case MfRetune: p.m_retuneMs = (int) value; break;
    default: return;
    }

    m_motionRows[field].m_value->setText(motionFieldText(field, value));

    // Refresh the speeds of the ring positions without touching the sliders being dragged
    QStringList speeds;

    for (int position = 1; position <= 7; position++) {
        speeds.append(QString("%1: %2").arg(position).arg(formatSpeed(m_displayedMode, p.m_shuttleBase * std::pow(p.m_shuttleRatio, position - 1))));
    }

    m_motionSpeeds->setText(tr("Ring positions: ") + speeds.join(tr("  ")));

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
    ui->focusIndicator->setStyleSheet("QLabel { background-color: rgb(85, 232, 85); border-radius: 8px; }"); // green
    ui->focusIndicator->setToolTip("Active");
}

void JogdialControllerGUI::focusOutEvent(QFocusEvent*)
{
    qDebug("JogdialControllerGUI::focusOutEvent");
    ui->focusIndicator->setStyleSheet("QLabel { background-color: gray; border-radius: 8px; }"); // gray
    ui->focusIndicator->setToolTip("Idle");
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
