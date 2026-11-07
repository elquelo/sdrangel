///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
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

#include <algorithm>
#include <sstream>

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "jogdialcontrollersettings.h"

const QStringList JogdialControllerSettings::m_channelTypes = {
    QStringLiteral("AISDemod"),
    QStringLiteral("AISMod"),
    QStringLiteral("AMDemod"),
    QStringLiteral("AMMod"),
    QStringLiteral("DABDemod"),
    QStringLiteral("DSDDemod"),
    QStringLiteral("NFMDemod"),
    QStringLiteral("NFMMod"),
    QStringLiteral("PacketDemod"),
    QStringLiteral("PacketMod"),
    QStringLiteral("SSBDemod"),
    QStringLiteral("SSBMod"),
    QStringLiteral("WFMDemod"),
    QStringLiteral("WFMMod"),
    QStringLiteral("BFMMod"),
};

const QStringList JogdialControllerSettings::m_channelURIs = {
    QStringLiteral("sdrangel.channel.aisdemod"),
    QStringLiteral("sdrangel.channel.modais"),
    QStringLiteral("sdrangel.channel.amdemod"),
    QStringLiteral("sdrangel.channeltx.modam"),
    QStringLiteral("sdrangel.channel.dabdemod"),
    QStringLiteral("sdrangel.channel.dsddemod"),
    QStringLiteral("sdrangel.channel.nfmdemod"),
    QStringLiteral("sdrangel.channeltx.modnfm"),
    QStringLiteral("sdrangel.channel.packetdemod"),
    QStringLiteral("sdrangel.channeltx.modpacket"),
    QStringLiteral("sdrangel.channel.ssbdemod"),
    QStringLiteral("sdrangel.channeltx.modssb"),
    QStringLiteral("sdrangel.channel.wfmdemod"),
    QStringLiteral("sdrangel.channeltx.modwfm"),
    QStringLiteral("sdrangel.channeltx.modbfm"),
};

JogdialControllerSettings::MotionParams JogdialControllerSettings::defaultMotionParams(int mode)
{
    MotionParams p;
    p.m_accelMs = 60;
    p.m_decelMs = 60;
    p.m_jogGainMinRate = 6.0;
    p.m_jogGainMax = 25.0;
    p.m_zoomMs = 120;
    p.m_retuneMs = 100;

    if (mode == 0) // device
    {
        p.m_shuttleBase = 3.0;
        p.m_shuttleRatio = 3.1622776601683795;
    }
    else if (mode == 1) // channel
    {
        p.m_shuttleBase = 30.0;
        p.m_shuttleRatio = 4.641588833612778;
    }
    else // spectrum
    {
        p.m_shuttleBase = 0.02;
        p.m_shuttleRatio = 2.0;
    }

    return p;
}

JogdialControllerSettings::JogdialControllerSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void JogdialControllerSettings::resetToDefaults()
{
    m_title = "Jogdial Controller";
    m_rgbColor = QColor(3, 198, 252).rgb();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;

    for (int i = 0; i < m_modeCount; i++) {
        m_motion[i] = defaultMotionParams(i);
    }
}

QByteArray JogdialControllerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(5, m_title);
    s.writeU32(6, m_rgbColor);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIFeatureSetIndex);
    s.writeU32(11, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(12, m_rollupState->serialize());
    }

    s.writeS32(13, m_workspaceIndex);
    s.writeBlob(14, m_geometryBytes);

    for (int i = 0; i < m_modeCount; i++)
    {
        const quint32 id = 20 + 10*i;
        s.writeDouble(id, m_motion[i].m_shuttleBase);
        s.writeDouble(id + 1, m_motion[i].m_shuttleRatio);
        s.writeS32(id + 2, m_motion[i].m_accelMs);
        s.writeS32(id + 3, m_motion[i].m_decelMs);
        s.writeDouble(id + 4, m_motion[i].m_jogGainMinRate);
        s.writeDouble(id + 5, m_motion[i].m_jogGainMax);
        s.writeS32(id + 6, m_motion[i].m_zoomMs);
        s.writeS32(id + 7, m_motion[i].m_retuneMs);
    }

    return s.final();
}

bool JogdialControllerSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;

        d.readString(5, &m_title, "Jogdial Controller");
        d.readU32(6, &m_rgbColor, QColor(3, 198, 252).rgb());
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(12, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(13, &m_workspaceIndex, 0);
        d.readBlob(14, &m_geometryBytes);

        for (int i = 0; i < m_modeCount; i++)
        {
            const quint32 id = 20 + 10*i;
            const MotionParams def = defaultMotionParams(i);
            MotionParams& p = m_motion[i];
            d.readDouble(id, &p.m_shuttleBase, def.m_shuttleBase);
            d.readDouble(id + 1, &p.m_shuttleRatio, def.m_shuttleRatio);
            d.readS32(id + 2, &p.m_accelMs, def.m_accelMs);
            d.readS32(id + 3, &p.m_decelMs, def.m_decelMs);
            d.readDouble(id + 4, &p.m_jogGainMinRate, def.m_jogGainMinRate);
            d.readDouble(id + 5, &p.m_jogGainMax, def.m_jogGainMax);
            d.readS32(id + 6, &p.m_zoomMs, def.m_zoomMs);
            d.readS32(id + 7, &p.m_retuneMs, def.m_retuneMs);
            // Keep the values in the ranges that make sense whatever is stored
            p.m_shuttleBase = std::max(1e-4, std::min(1e6, p.m_shuttleBase));
            p.m_shuttleRatio = std::max(1.05, std::min(10.0, p.m_shuttleRatio));
            p.m_accelMs = std::max(5, std::min(5000, p.m_accelMs));
            p.m_decelMs = std::max(5, std::min(5000, p.m_decelMs));
            p.m_jogGainMinRate = std::max(1.0, std::min(50.0, p.m_jogGainMinRate));
            p.m_jogGainMax = std::max(1.0, std::min(200.0, p.m_jogGainMax));
            p.m_zoomMs = std::max(5, std::min(5000, p.m_zoomMs));
            p.m_retuneMs = std::max(10, std::min(2000, p.m_retuneMs));
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void JogdialControllerSettings::applySettings(const QStringList& settingsKeys, const JogdialControllerSettings& settings)
{
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("motion"))
    {
        for (int i = 0; i < m_modeCount; i++) {
            m_motion[i] = settings.m_motion[i];
        }
    }
}

QString JogdialControllerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    return QString(ostr.str().c_str());
}
