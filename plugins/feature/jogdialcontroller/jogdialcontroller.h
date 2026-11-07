///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_FEATURE_JOGDIALCONTROLLER_H_
#define INCLUDE_FEATURE_JOGDIALCONTROLLER_H_

#include <QList>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QTimer>

#include "feature/feature.h"
#include "util/message.h"

#include "jogdialcontrollersettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class JogdialController : public Feature
{
	Q_OBJECT
public:
    //! What the jog wheel and the shuttle wheel act on
    enum class ControlMode {
        Device,  //!< Center frequency of the device of the selected channel
        Channel, //!< Frequency offset of the selected channel
        Spectrum //!< Frequency zoom (jog) and pan (shuttle) of the spectrum of the device set of the selected channel
    };

    class MsgConfigureJogdialController : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const JogdialControllerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureJogdialController* create(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureJogdialController(settings, settingsKeys, force);
        }

    private:
        JogdialControllerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureJogdialController(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgRefreshChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRefreshChannels* create() {
            return new MsgRefreshChannels();
        }

    protected:
        MsgRefreshChannels() :
            Message()
        { }
    };

    class MsgReportChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<JogdialControllerSettings::AvailableChannel>& getAvailableChannels() { return m_availableChannels; }

        static MsgReportChannels* create() {
            return new MsgReportChannels();
        }

    private:
        QList<JogdialControllerSettings::AvailableChannel> m_availableChannels;

        MsgReportChannels() :
            Message()
        {}
    };

    class MsgSelectChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }
        static MsgSelectChannel* create(int index) {
            return new MsgSelectChannel(index);
        }

    protected:
        int m_index;

        MsgSelectChannel(int index) :
            Message(),
            m_index(index)
        { }
    };

    class MsgReportControl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        ControlMode getControlMode() const { return m_controlMode; }

        static MsgReportControl* create(ControlMode controlMode) {
            return new MsgReportControl(controlMode);
        }

    protected:
        ControlMode m_controlMode;

        MsgReportControl(ControlMode controlMode) :
            Message(),
            m_controlMode(controlMode)
        { }
    };

    class MsgControlSpectrum : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        enum Action {
            ZoomBy,   //!< Multiply the frequency zoom factor by the value, keeping the frequency offset in place if there is one
            PanBy,    //!< Move the zoomed window by the value (fraction of its width)
            ResetZoom //!< Back to the full span (the value is not used)
        };

        int getDeviceSetIndex() const { return m_deviceSetIndex; }
        Action getAction() const { return m_action; }
        float getValue() const { return m_value; }
        bool hasFrequencyOffset() const { return m_hasFrequencyOffset; }
        qint64 getFrequencyOffset() const { return m_frequencyOffset; }

        static MsgControlSpectrum* create(int deviceSetIndex, Action action, float value) {
            return new MsgControlSpectrum(deviceSetIndex, action, value, false, 0);
        }

        //! Frequency offset from the device center that stays in place when zooming (the selected channel)
        static MsgControlSpectrum* create(int deviceSetIndex, Action action, float value, qint64 frequencyOffset) {
            return new MsgControlSpectrum(deviceSetIndex, action, value, true, frequencyOffset);
        }

    protected:
        int m_deviceSetIndex;
        Action m_action;
        float m_value;
        bool m_hasFrequencyOffset;
        qint64 m_frequencyOffset;

        MsgControlSpectrum(int deviceSetIndex, Action action, float value, bool hasFrequencyOffset, qint64 frequencyOffset) :
            Message(),
            m_deviceSetIndex(deviceSetIndex),
            m_action(action),
            m_value(value),
            m_hasFrequencyOffset(hasFrequencyOffset),
            m_frequencyOffset(frequencyOffset)
        { }
    };

    JogdialController(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~JogdialController();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    void resetChannelFrequency();
    void stepFrequency(int step);
    void flushDeviceSteps(bool force = false);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const JogdialControllerSettings& settings);

    static void webapiUpdateFeatureSettings(
            JogdialControllerSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

public slots:
    void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);

private:
    JogdialControllerSettings m_settings;
    QList<JogdialControllerSettings::AvailableChannel> m_availableChannels;
    DeviceAPI *m_selectedDevice;
    ChannelAPI *m_selectedChannel;
    int m_selectedIndex;
    ControlMode m_controlMode;
    QTimer m_motionTimer;                //!< Drives the shuttle motion, the zoom animation and the device retuning
    QElapsedTimer m_motionClock;         //!< Time elapsed since the last motion update
    QElapsedTimer m_jogClock;            //!< Time elapsed since the last jog click
    QElapsedTimer m_deviceRetuneClock;   //!< Time elapsed since the last retuning of the device
    int m_shuttlePosition;               //!< Position of the shuttle ring: -7 to 7 (0: centered)
    double m_velocity;                   //!< Current shuttle speed, smoothed. Steps/s (frequency control) or window widths/s (spectrum control)
    double m_steps;                      //!< Fraction of a step not yet applied
    double m_zoomPending;                //!< Natural logarithm of the zoom ratio requested and not yet applied
    qint64 m_pendingDeviceStep;          //!< Steps requested on the device and not yet applied
    int m_lastJogDirection;              //!< Sign of the last jog click

    static const int m_motionIntervalMs = 30;          //!< Period of the motion updates
    static const int m_deviceRetuneMinMs = 100;        //!< Minimum delay between two retunings of the device
    static const int m_shuttleMaxPosition = 7;
    static constexpr double m_zoomPendingMax = 2.772588722239781; //!< Maximum pending zoom: ln(16)

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void updateChannels();
    bool isDeviceValid(const DeviceAPI *device) const;
    bool isChannelValid(const ChannelAPI *channel) const;
    bool dropStaleSelection();
    void channelUp();
    void channelDown();
    void setControlMode(ControlMode controlMode);
    void nextControlMode();
    int getSelectedDeviceSetIndex() const;
    void jogStep(int step);
    double jogGain(int direction);
    void zoomSpectrum(int step, double gain);
    void setShuttlePosition(int position);
    static bool shuttlePositionFromKey(Qt::Key key, int& position);
    double shuttleTargetVelocity() const;
    double shuttleBaseVelocity() const;
    const JogdialControllerSettings::MotionParams& motion() const { return m_settings.m_motion[static_cast<int>(m_controlMode)]; } //!< Parameters of the current control mode
    void startMotion();
    void stopMotion();
    void applyDeviceStep(qint64 step);
    void controlSpectrum(MsgControlSpectrum::Action action, float value);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const JogdialControllerSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue *messageQueues);
    void handleMotion();
};

#endif // INCLUDE_FEATURE_DEMODANALYZER_H_
