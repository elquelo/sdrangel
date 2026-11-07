///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGDeviceState.h"

#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "jogdialcontroller.h"

MESSAGE_CLASS_DEFINITION(JogdialController::MsgConfigureJogdialController, Message)
MESSAGE_CLASS_DEFINITION(JogdialController::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(JogdialController::MsgRefreshChannels, Message)
MESSAGE_CLASS_DEFINITION(JogdialController::MsgReportChannels, Message)
MESSAGE_CLASS_DEFINITION(JogdialController::MsgReportControl, Message)
MESSAGE_CLASS_DEFINITION(JogdialController::MsgControlSpectrum, Message)
MESSAGE_CLASS_DEFINITION(JogdialController::MsgSelectChannel, Message)

const char* const JogdialController::m_featureIdURI = "sdrangel.feature.jogdialcontroller";
const char* const JogdialController::m_featureId = "JogdialController";

JogdialController::JogdialController(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_selectedDevice(nullptr),
    m_selectedChannel(nullptr),
    m_selectedIndex(-1),
    m_controlMode(ControlMode::Device),
    m_shuttlePosition(0),
    m_velocity(0.0),
    m_steps(0.0),
    m_zoomPending(0.0),
    m_pendingDeviceStep(0),
    m_lastJogDirection(0)
{
    qDebug("JogdialController::JogdialController: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    setState(StIdle);
    m_errorMessage = "JogdialController error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &JogdialController::networkManagerFinished
    );
    connect(&m_motionTimer, SIGNAL(timeout()), this, SLOT(handleMotion()));
}

JogdialController::~JogdialController()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &JogdialController::networkManagerFinished
    );
    delete m_networkManager;
}

void JogdialController::start()
{
	qDebug("JogdialController::start");
    setState(StRunning);
}

void JogdialController::stop()
{
    qDebug("JogdialController::stop");
    setState(StIdle);
}

bool JogdialController::handleMessage(const Message& cmd)
{
	if (MsgConfigureJogdialController::match(cmd))
	{
        MsgConfigureJogdialController& cfg = (MsgConfigureJogdialController&) cmd;
        qDebug() << "JogdialController::handleMessage: MsgConfigureJogdialController";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "JogdialController::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgRefreshChannels::match(cmd))
    {
        qDebug() << "JogdialController::handleMessage: MsgRefreshChannels";
        updateChannels();
        return true;
    }
    else if (MsgSelectChannel::match(cmd))
    {
        MsgSelectChannel& cfg = (MsgSelectChannel&) cmd;
        int index = cfg.getIndex();

        if ((index >= 0) && (index < m_availableChannels.size()))
        {
            DeviceAPI *selectedDevice = m_availableChannels[cfg.getIndex()].m_deviceAPI;
            ChannelAPI *selectedChannel = m_availableChannels[cfg.getIndex()].m_channelAPI;

            // The list is only refreshed on demand so the device or the channel may have been removed since.
            // Never dereference them before checking they still exist.
            if (!isDeviceValid(selectedDevice) || !isChannelValid(selectedChannel))
            {
                qWarning("JogdialController::handleMessage: MsgSelectChannel: stale selection (device or channel removed): refreshing");
                m_selectedDevice = nullptr;
                m_selectedChannel = nullptr;
                m_selectedIndex = -1;
                updateChannels();
                return true;
            }

            qDebug() << "JogdialController::handleMessage: MsgSelectChannel"
                << "device:" << selectedDevice->getHardwareId()
                << "channel:" << selectedChannel->getIdentifier();
            m_selectedDevice = selectedDevice;
            m_selectedChannel = selectedChannel;
            m_selectedIndex = index;
        }
        else
        {
            qWarning("JogdialController::handleMessage: MsgSelectChannel: index out of range: %d", index);
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray JogdialController::serialize() const
{
    return m_settings.serialize();
}

bool JogdialController::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureJogdialController *msg = MsgConfigureJogdialController::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureJogdialController *msg = MsgConfigureJogdialController::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void JogdialController::applySettings(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "JogdialController::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

bool JogdialController::isDeviceValid(const DeviceAPI *device) const
{
    if (!device) {
        return false;
    }

    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    for (const DeviceSet *deviceSet : deviceSets)
    {
        if (deviceSet->m_deviceAPI == device) {
            return true;
        }
    }

    return false;
}

bool JogdialController::isChannelValid(const ChannelAPI *channel) const
{
    if (!channel) {
        return false;
    }

    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    for (DeviceSet *deviceSet : deviceSets)
    {
        for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
        {
            if (deviceSet->getChannelAt(chi) == channel) {
                return true;
            }
        }
    }

    return false;
}

//! Forget the selected device and channel if they no longer exist. Returns true if something was dropped.
bool JogdialController::dropStaleSelection()
{
    bool dropped = false;

    if (m_selectedDevice && !isDeviceValid(m_selectedDevice))
    {
        m_selectedDevice = nullptr;
        dropped = true;
    }

    if (m_selectedChannel && !isChannelValid(m_selectedChannel))
    {
        m_selectedChannel = nullptr;
        dropped = true;
    }

    if (dropped)
    {
        qWarning("JogdialController::dropStaleSelection: selected device or channel was removed");
        m_selectedIndex = -1;
    }

    return dropped;
}

void JogdialController::updateChannels()
{
    dropStaleSelection();
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    std::vector<DeviceSet*>::const_iterator it = deviceSets.begin();
    m_availableChannels.clear();

    int deviceIndex = 0;

    for (; it != deviceSets.end(); ++it, deviceIndex++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  (*it)->m_deviceSourceEngine;
        DSPDeviceSinkEngine *deviceSinkEngine =  (*it)->m_deviceSinkEngine;
        DSPDeviceMIMOEngine *deviceMIMOEngine =  (*it)->m_deviceMIMOEngine;
        DeviceAPI *device = (*it)->m_deviceAPI;
        device->getHardwareId();

        if (deviceSourceEngine || deviceSinkEngine || deviceMIMOEngine)
        {
            for (int chi = 0; chi < (*it)->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = (*it)->getChannelAt(chi);
                // On a MIMO device set each channel is either Rx or Tx
                const bool tx = deviceMIMOEngine ?
                    (channel->getStreamType() == ChannelAPI::StreamSingleSource) :
                    (deviceSinkEngine != nullptr);
                JogdialControllerSettings::AvailableChannel availableChannel =
                    JogdialControllerSettings::AvailableChannel{
                        tx,
                        deviceIndex,
                        chi,
                        device,
                        channel,
                        device->getHardwareId(),
                        channel->getIdentifier()
                    };
                m_availableChannels.push_back(availableChannel);
            }
        }
    }

    // The list has been rebuilt: find again the selected channel in it, if it still exists
    m_selectedIndex = -1;

    for (int i = 0; i < m_availableChannels.size(); i++)
    {
        if (m_selectedChannel && (m_availableChannels.at(i).m_channelAPI == m_selectedChannel))
        {
            m_selectedIndex = i;
            break;
        }
    }

    if (getMessageQueueToGUI())
    {
        MsgReportChannels *msgToGUI = MsgReportChannels::create();
        QList<JogdialControllerSettings::AvailableChannel>& msgAvailableChannels = msgToGUI->getAvailableChannels();
        msgAvailableChannels = m_availableChannels;
        getMessageQueueToGUI()->push(msgToGUI);
    }
}

void JogdialController::channelUp()
{
    if ((m_selectedIndex < 0) || (m_availableChannels.size() == 0)) {
        return;
    }

    m_selectedIndex++;

    if (m_selectedIndex >= m_availableChannels.size()) {
        m_selectedIndex = 0;
    }

    if (!isDeviceValid(m_availableChannels.at(m_selectedIndex).m_deviceAPI) ||
        !isChannelValid(m_availableChannels.at(m_selectedIndex).m_channelAPI))
    {
        // stale entry: some channel was removed since the last refresh
        m_selectedDevice = nullptr;
        m_selectedChannel = nullptr;
        m_selectedIndex = -1;
        updateChannels();
        return;
    }

    m_selectedDevice = m_availableChannels.at(m_selectedIndex).m_deviceAPI;
    m_selectedChannel =  m_availableChannels.at(m_selectedIndex).m_channelAPI;

    if (getMessageQueueToGUI())
    {
        MsgSelectChannel *msgToGUI = MsgSelectChannel::create(m_selectedIndex);
        getMessageQueueToGUI()->push(msgToGUI);
    }
}

void JogdialController::channelDown()
{
    if ((m_selectedIndex < 0) || (m_availableChannels.size() == 0)) {
        return;
    }

    m_selectedIndex--;

    if (m_selectedIndex < 0) {
        m_selectedIndex = m_availableChannels.size() - 1;
    }

    if (!isDeviceValid(m_availableChannels.at(m_selectedIndex).m_deviceAPI) ||
        !isChannelValid(m_availableChannels.at(m_selectedIndex).m_channelAPI))
    {
        // stale entry: some channel was removed since the last refresh
        m_selectedDevice = nullptr;
        m_selectedChannel = nullptr;
        m_selectedIndex = -1;
        updateChannels();
        return;
    }

    m_selectedDevice = m_availableChannels.at(m_selectedIndex).m_deviceAPI;
    m_selectedChannel =  m_availableChannels.at(m_selectedIndex).m_channelAPI;

    if (getMessageQueueToGUI())
    {
        MsgSelectChannel *msgToGUI = MsgSelectChannel::create(m_selectedIndex);
        getMessageQueueToGUI()->push(msgToGUI);
    }
}

int JogdialController::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int JogdialController::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setJogdialControllerSettings(new SWGSDRangel::SWGJogdialControllerSettings());
    response.getJogdialControllerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int JogdialController::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    JogdialControllerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureJogdialController *msg = MsgConfigureJogdialController::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("JogdialController::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureJogdialController *msgToGUI = MsgConfigureJogdialController::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void JogdialController::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const JogdialControllerSettings& settings)
{
    if (response.getJogdialControllerSettings()->getTitle()) {
        *response.getJogdialControllerSettings()->getTitle() = settings.m_title;
    } else {
        response.getJogdialControllerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getJogdialControllerSettings()->setRgbColor(settings.m_rgbColor);
    response.getJogdialControllerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getJogdialControllerSettings()->getReverseApiAddress()) {
        *response.getJogdialControllerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getJogdialControllerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getJogdialControllerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getJogdialControllerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getJogdialControllerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getJogdialControllerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getJogdialControllerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getJogdialControllerSettings()->setRollupState(swgRollupState);
        }
    }
}

void JogdialController::webapiUpdateFeatureSettings(
    JogdialControllerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getJogdialControllerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getJogdialControllerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getJogdialControllerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getJogdialControllerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getJogdialControllerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getJogdialControllerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getJogdialControllerSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getJogdialControllerSettings()->getRollupState());
    }
}

void JogdialController::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const JogdialControllerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("JogdialAnalyzer"));
    swgFeatureSettings->setJogdialControllerSettings(new SWGSDRangel::SWGJogdialControllerSettings());
    SWGSDRangel::SWGJogdialControllerSettings *swgJogdialControllerSettings = swgFeatureSettings->getJogdialControllerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgJogdialControllerSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgJogdialControllerSettings->setRgbColor(settings.m_rgbColor);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void JogdialController::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "JogdialController::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("JogdialController::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void JogdialController::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void JogdialController::commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release)
{
    (void) release;

    // On macOS Qt also sets KeypadModifier on arrow key events, which defeats
    // the exact modifier comparisons for Key_Up and Key_Down below
    keyModifiers &= ~Qt::KeypadModifier;

    if (key == Qt::Key_C)
    {
        setControlMode(ControlMode::Channel);
    }
    else if (key == Qt::Key_D)
    {
        setControlMode(ControlMode::Device);
    }
    else if (key == Qt::Key_S)
    {
        setControlMode(ControlMode::Spectrum);
    }
    else if (key == Qt::Key_M)
    {
        nextControlMode();
    }
    else if (key == Qt::Key_Left)
    {
        channelDown();
    }
    else if (key == Qt::Key_Right)
    {
        channelUp();
    }
    else if (key == Qt::Key_Up)
    {
        setShuttlePosition(0);

        if (keyModifiers == Qt::NoModifier) {
            jogStep(1);
        } else if (keyModifiers == Qt::ControlModifier) {
            jogStep(10);
        } else if (keyModifiers == Qt::ShiftModifier) {
            jogStep(100);
        } else if (keyModifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
            jogStep(1000);
        }
    }
    else if (key == Qt::Key_Down)
    {
        setShuttlePosition(0);

        if (keyModifiers == Qt::NoModifier) {
            jogStep(-1);
        } else if (keyModifiers == Qt::ControlModifier) {
            jogStep(-10);
        } else if (keyModifiers == Qt::ShiftModifier) {
            jogStep(-100);
        } else if (keyModifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
            jogStep(-1000);
        }
    }
    else if (key == Qt::Key_Home)
    {
        if (m_controlMode == ControlMode::Spectrum)
        {
            m_zoomPending = 0.0;
            controlSpectrum(MsgControlSpectrum::ResetZoom, 0.0f);
        }
        else
        {
            resetChannelFrequency();
        }
    }
    else if (key == Qt::Key_0)
    {
        setShuttlePosition(0);
    }
    else
    {
        int position;

        if (shuttlePositionFromKey(key, position)) {
            setShuttlePosition(position);
        }
    }
}

//! The shuttle ring is mapped on the digits (right) and shifted digits (left) of the keyboard
bool JogdialController::shuttlePositionFromKey(Qt::Key key, int& position)
{
    switch (key)
    {
    case Qt::Key_1: position = 1; return true;
    case Qt::Key_2: position = 2; return true;
    case Qt::Key_3: position = 3; return true;
    case Qt::Key_4: position = 4; return true;
    case Qt::Key_5: position = 5; return true;
    case Qt::Key_6: position = 6; return true;
    case Qt::Key_7: position = 7; return true;
    case Qt::Key_Exclam: position = -1; return true;
    case Qt::Key_At: position = -2; return true;
    case Qt::Key_NumberSign: position = -3; return true;
    case Qt::Key_Dollar: position = -4; return true;
    case Qt::Key_Percent: position = -5; return true;
    case Qt::Key_Dead_Circumflex:
    case Qt::Key_AsciiCircum: position = -6; return true;
    case Qt::Key_Ampersand: position = -7; return true;
    default: return false;
    }
}

void JogdialController::resetChannelFrequency()
{
    if (dropStaleSelection()) {
        updateChannels();
    }

    if (m_selectedChannel) {
        m_selectedChannel->setCenterFrequency(0);
    }
}

//! Step a frequency: 1 Hz for a channel, 1 kHz for a device.
//! The device is retuned at a limited rate: the steps requested in between are added up
void JogdialController::stepFrequency(int step)
{
    qDebug("JogdialController::stepFrequency: step: %d", step);

    if (dropStaleSelection()) {
        updateChannels();
    }

    if (m_controlMode == ControlMode::Device)
    {
        m_pendingDeviceStep += step;
        flushDeviceSteps();
    }
    else
    {
        if (m_selectedChannel)
        {
            qint64 frequency = m_selectedChannel->getCenterFrequency();
            m_selectedChannel->setCenterFrequency(frequency + step);
        }
    }
}

//! Retune the device with the steps added up since the last retuning, unless this was done less than
//! the retune delay of the device parameters ago and force is not set: the motion timer will do it then
void JogdialController::flushDeviceSteps(bool force)
{
    if (m_pendingDeviceStep == 0) {
        return;
    }

    if (dropStaleSelection())
    {
        m_pendingDeviceStep = 0; // the device is gone
        updateChannels();
        return;
    }

    if (!force && m_deviceRetuneClock.isValid() && (m_deviceRetuneClock.elapsed() < motion().m_retuneMs))
    {
        startMotion(); // the retuning is for the next ticks
        return;
    }

    const qint64 step = m_pendingDeviceStep;
    m_pendingDeviceStep = 0;
    m_deviceRetuneClock.start();
    applyDeviceStep(step);
}

void JogdialController::applyDeviceStep(qint64 step)
{
    if (!m_selectedDevice) {
        return;
    }

    DSPDeviceSourceEngine *sourceEngine = m_selectedDevice->getDeviceSourceEngine();
    DSPDeviceSinkEngine *sinkEngine = m_selectedDevice->getDeviceSinkEngine();
    DSPDeviceMIMOEngine *mimoEngine = m_selectedDevice->getDeviceMIMOEngine();

    if (mimoEngine && mimoEngine->getMIMO())
    {
        // MIMO: act on the Rx or Tx stream of the selected channel (stream 0 if no channel is selected)
        DeviceSampleMIMO *mimo = mimoEngine->getMIMO();
        const int streamIndex = m_selectedChannel ? m_selectedChannel->getStreamIndex() : 0;

        if (m_selectedChannel && (m_selectedChannel->getStreamType() == ChannelAPI::StreamSingleSource))
        {
            qint64 frequency = mimo->getSinkCenterFrequency(streamIndex);
            mimo->setSinkCenterFrequency(frequency + step*1000LL, streamIndex);
        }
        else
        {
            qint64 frequency = mimo->getSourceCenterFrequency(streamIndex);
            mimo->setSourceCenterFrequency(frequency + step*1000LL, streamIndex);
        }
    }

    if (sourceEngine)
    {
        quint64 frequency = sourceEngine->getSource()->getCenterFrequency();
        qDebug("JogdialController::applyDeviceStep: frequency: %llu", frequency);
        sourceEngine->getSource()->setCenterFrequency(frequency + step*1000LL);
    }

    if (sinkEngine)
    {
        quint64 frequency = sinkEngine->getSink()->getCenterFrequency();
        sinkEngine->getSink()->setCenterFrequency(frequency + step*1000LL);
    }
}

//! The jog wheel zooms the spectrum in spectrum control and steps a frequency otherwise.
//! The faster the wheel is turned the more each click counts
void JogdialController::jogStep(int step)
{
    const double gain = jogGain((step > 0) ? 1 : -1);

    if (m_controlMode == ControlMode::Spectrum) {
        zoomSpectrum(step, gain);
    } else {
        const int scaled = qRound(step * gain);
        stepFrequency((scaled != 0) ? scaled : ((step > 0) ? 1 : -1));
    }
}

//! Acceleration of the jog wheel: 1 up to a number of clicks per second then proportional to the click rate
//! up to a maximum (both set per control mode). A change of direction starts again from 1
double JogdialController::jogGain(int direction)
{
    double gain = 1.0;

    if (m_jogClock.isValid() && (direction == m_lastJogDirection))
    {
        const qint64 interval = m_jogClock.elapsed();

        if (interval > 0)
        {
            const double rate = 1000.0 / interval;

            if (rate > motion().m_jogGainMinRate) {
                gain = std::min(motion().m_jogGainMax, rate / motion().m_jogGainMinRate);
            }
        }
    }

    m_jogClock.start();
    m_lastJogDirection = direction;
    return gain;
}

//! step is +/- 1, 10, 100 or 1000 according to the modifier keys: the bigger the step the more each click zooms.
//! The zoom is not applied at once: it is requested as a distance to cover that the motion timer animates
void JogdialController::zoomSpectrum(int step, double gain)
{
    const int magnitude = qAbs(step);
    double ratio;

    if (magnitude >= 1000) {
        ratio = 2.0;
    } else if (magnitude >= 100) {
        ratio = 1.5;
    } else if (magnitude >= 10) {
        ratio = 1.25;
    } else {
        ratio = 1.1;
    }

    const double logRatio = std::log(ratio) * gain * ((step > 0) ? 1.0 : -1.0);
    m_zoomPending = std::max(-m_zoomPendingMax, std::min(m_zoomPendingMax, m_zoomPending + logRatio));
    startMotion();
}

//! Send the zoom or pan command to the GUI that owns the spectrum of the device set of the selected channel.
//! A zoom keeps the frequency of the selected channel in place on the spectrum
void JogdialController::controlSpectrum(MsgControlSpectrum::Action action, float value)
{
    if (dropStaleSelection()) {
        updateChannels();
    }

    const int deviceSetIndex = getSelectedDeviceSetIndex();

    if ((deviceSetIndex < 0) || !getMessageQueueToGUI()) {
        return;
    }

    if ((action == MsgControlSpectrum::ZoomBy) && m_selectedChannel) {
        getMessageQueueToGUI()->push(MsgControlSpectrum::create(deviceSetIndex, action, value, m_selectedChannel->getCenterFrequency()));
    } else {
        getMessageQueueToGUI()->push(MsgControlSpectrum::create(deviceSetIndex, action, value));
    }
}

void JogdialController::setControlMode(ControlMode controlMode)
{
    // A movement in progress was meant for the previous mode
    flushDeviceSteps(true);
    stopMotion();
    m_controlMode = controlMode;

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgReportControl::create(m_controlMode));
    }
}

//! Device -> channel -> spectrum -> device
void JogdialController::nextControlMode()
{
    switch (m_controlMode)
    {
    case ControlMode::Device:
        setControlMode(ControlMode::Channel);
        break;
    case ControlMode::Channel:
        setControlMode(ControlMode::Spectrum);
        break;
    default:
        setControlMode(ControlMode::Device);
        break;
    }
}

int JogdialController::getSelectedDeviceSetIndex() const
{
    if (!m_selectedDevice) {
        return -1;
    }

    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    for (unsigned int i = 0; i < deviceSets.size(); i++)
    {
        if (deviceSets[i]->m_deviceAPI == m_selectedDevice) {
            return i;
        }
    }

    return -1;
}

//! Shuttle ring position from -7 to 7. The speed follows it progressively, see handleMotion
void JogdialController::setShuttlePosition(int position)
{
    m_shuttlePosition = std::max(-m_shuttleMaxPosition, std::min(m_shuttleMaxPosition, position));

    if (m_shuttlePosition != 0) {
        startMotion();
    }
}

//! Speed of the first ring position: steps/s (Hz/s for a channel, kHz/s for a device) or window widths/s
double JogdialController::shuttleBaseVelocity() const
{
    return motion().m_shuttleBase;
}

//! Speed the ring position asks for: it grows geometrically from a position to the next
double JogdialController::shuttleTargetVelocity() const
{
    if (m_shuttlePosition == 0) {
        return 0.0;
    }

    const double velocity = shuttleBaseVelocity() * std::pow(motion().m_shuttleRatio, std::abs(m_shuttlePosition) - 1);
    return (m_shuttlePosition > 0) ? velocity : -velocity;
}

void JogdialController::startMotion()
{
    if (!m_motionTimer.isActive())
    {
        m_motionClock.start();
        m_motionTimer.start(m_motionIntervalMs);
    }
}

void JogdialController::stopMotion()
{
    m_motionTimer.stop();
    m_shuttlePosition = 0;
    m_velocity = 0.0;
    m_steps = 0.0;
    m_zoomPending = 0.0;
}

//! Periodic update of everything that moves progressively:
//! - the shuttle speed approaches the speed of the ring position and moves a frequency or pans the spectrum accordingly
//! - the zoom requested by the jog wheel is animated
//! - the device is retuned with the steps added up since the last time
void JogdialController::handleMotion()
{
    // Limit the time step so that a stalled event loop does not result in a jump
    const double dt = std::min(0.1, m_motionClock.restart() / 1000.0);
    const double target = shuttleTargetVelocity();

    const int timeConstantMs = (std::abs(target) < std::abs(m_velocity)) ? motion().m_decelMs : motion().m_accelMs;
    m_velocity += (target - m_velocity) * (1.0 - std::exp(-dt * 1000.0 / timeConstantMs));

    if ((target == 0.0) && (std::abs(m_velocity) < 0.5 * shuttleBaseVelocity())) {
        m_velocity = 0.0;
    }

    if (m_velocity != 0.0)
    {
        if (m_controlMode == ControlMode::Spectrum)
        {
            controlSpectrum(MsgControlSpectrum::PanBy, (float) (m_velocity * dt));
        }
        else
        {
            m_steps += m_velocity * dt;
            const int whole = (int) m_steps; // truncated towards 0
            m_steps -= whole;

            if (whole != 0) {
                stepFrequency(whole);
            }
        }
    }

    if (m_zoomPending != 0.0)
    {
        double applied = m_zoomPending * (1.0 - std::exp(-dt * 1000.0 / motion().m_zoomMs));

        if (std::abs(m_zoomPending - applied) < 0.002) { // close enough: finish
            applied = m_zoomPending;
        }

        m_zoomPending -= applied;
        controlSpectrum(MsgControlSpectrum::ZoomBy, (float) std::exp(applied));
    }

    flushDeviceSteps();

    if ((m_velocity == 0.0) && (m_shuttlePosition == 0) && (m_zoomPending == 0.0) && (m_pendingDeviceStep == 0)) {
        m_motionTimer.stop();
    }
}
