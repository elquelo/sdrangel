///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_JOGDIALCONTROLLERSETTINGS_H_
#define INCLUDE_FEATURE_JOGDIALCONTROLLERSETTINGS_H_

#include <cmath>

#include <QByteArray>
#include <QString>

class Serializable;
class ChannelAPI;
class DeviceAPI;

struct JogdialControllerSettings
{
    struct AvailableChannel
    {
        bool m_tx{false};
        int m_deviceSetIndex{0};
        int m_channelIndex{0};
        DeviceAPI *m_deviceAPI{nullptr};
        ChannelAPI *m_channelAPI{nullptr};
        QString m_deviceId;
        QString m_channelId;

        AvailableChannel() = default;
        AvailableChannel(const AvailableChannel&) = default;
        AvailableChannel& operator=(const AvailableChannel&) = default;
    };

    //! Parameters of the jog wheel and the shuttle ring in one control mode. Four user settings, the rest is derived
    struct MotionParams
    {
        double m_ringMin;  //!< Speed at ring position 1: kHz/s (device), Hz/s (channel) or window widths/s (spectrum)
        double m_ringMax;  //!< Speed at ring position 7, same unit. The positions in between follow a geometric progression
        int m_reaction;    //!< 0 (smooth) to 100 (snappy): how quickly the movement follows the ring
        double m_jogStep;  //!< Size of a jog click: Hz (channel), kHz (device) or fraction of zoom (spectrum)
        int m_jogAccel;    //!< 0 (none) to 100: how much the jog accelerates when turned fast

        //! Speed ratio between two successive ring positions (position 1 to 7: 6 steps)
        double ringRatio() const { return std::pow(m_ringMax / m_ringMin, 1.0 / 6.0); }
        //! Time constant of the movement in ms, from 250 (smooth) to 20 (snappy), geometric in between
        int reactionMs() const { return (int) std::lround(250.0 * std::pow(20.0 / 250.0, m_reaction / 100.0)); }
        //! Animated zoom is a bit slower than the movement
        int zoomMs() const { return 2 * reactionMs(); }
        //! Gain of the jog for a click rate above jogAccelStartRate(): 1 + slope * (rate / start rate - 1)
        double jogAccelSlope() const { return m_jogAccel / 25.0; }
        static double jogAccelStartRate() { return 6.0; }  //!< Jog clicks per second above which the jog accelerates
        static double jogGainMax() { return 50.0; }
    };

    static const int m_modeCount = 3; //!< Device, channel, spectrum: same order as JogdialController::ControlMode
    static MotionParams defaultMotionParams(int mode);

    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    MotionParams m_motion[m_modeCount];

    JogdialControllerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const JogdialControllerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static const QStringList m_channelTypes;
    static const QStringList m_channelURIs;
};

#endif // INCLUDE_FEATURE_JOGDIALCONTROLLERSETTINGS_H_
