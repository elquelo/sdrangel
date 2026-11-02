///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#include <QUdpSocket>
#include <QMutexLocker>
#include <QtGlobal>

#include "wsjtxudpencoder.h"
#include "wsjtxudpsender.h"

WsjtxUdpSender::WsjtxUdpSender() :
    m_address(QHostAddress::LocalHost),
    m_port(2237),
    m_instanceId("SDRangel"),
    m_sentSomething(false)
{}

WsjtxUdpSender::~WsjtxUdpSender()
{
    sendClose();
}

void WsjtxUdpSender::setDestination(const QString& address, quint16 port)
{
    QMutexLocker locker(&m_mutex);
    QHostAddress hostAddress;

    if (hostAddress.setAddress(address)) {
        m_address = hostAddress;
    } else {
        qWarning("WsjtxUdpSender::setDestination: invalid IP address: %s", qPrintable(address));
    }

    m_port = port;
}

void WsjtxUdpSender::setInstanceId(const QString& instanceId)
{
    QMutexLocker locker(&m_mutex);
    m_instanceId = instanceId;
}

void WsjtxUdpSender::setReportingStation(const QString& callsign, const QString& locator)
{
    QMutexLocker locker(&m_mutex);
    m_callsign = callsign;
    m_locator = locator;
}

QString WsjtxUdpSender::getMessageText(const FT8Message& message)
{
    if (!message.text.isEmpty()) {
        return message.text;
    }

    // Fallback (e.g. DXpedition split messages): rebuild from the parsed fields
    QString text = message.call1;

    if (!message.call2.isEmpty()) {
        text += " " + message.call2;
    }

    if (!message.loc.isEmpty()) {
        text += " " + message.loc;
    }

    return text.trimmed();
}

void WsjtxUdpSender::send(const std::vector<uint8_t>& packet)
{
    QHostAddress address;
    quint16 port;

    {
        QMutexLocker locker(&m_mutex);
        address = m_address;
        port = m_port;
    }

    if (!m_socket) {
        m_socket = std::make_unique<QUdpSocket>();
    }

    qint64 sent = m_socket->writeDatagram(
        reinterpret_cast<const char*>(packet.data()),
        static_cast<qint64>(packet.size()),
        address,
        port
    );

    if (sent < 0)
    {
        qWarning("WsjtxUdpSender::send: failed to send to %s:%u: %s",
            qPrintable(address.toString()), port, qPrintable(m_socket->errorString()));
    }
    else
    {
        m_sentSomething = true;
    }
}

void WsjtxUdpSender::sendPeriod(
    const QDateTime& periodTS,
    qint64 baseFrequency,
    FT8DemodSettings::DecoderMode decoderMode,
    const QList<FT8Message>& messages)
{
    std::string id;
    WsjtxUdp::StatusInfo status;

    {
        QMutexLocker locker(&m_mutex);
        id = m_instanceId.toStdString();
        status.deCall = m_callsign.toStdString();
        status.deGrid = m_locator.toStdString();
    }

    const std::string modeName = FT8DemodSettings::getDecoderModeString(decoderMode).toStdString();
    // Mode character used in the Decode message: "~" is FT8 and "+" is FT4 in WSJT-X
    const std::string modeChar = (decoderMode == FT8DemodSettings::DecoderModeFT4) ? "+" : "~";

    status.dialFrequency = static_cast<uint64_t>(baseFrequency < 0 ? 0 : baseFrequency);
    status.mode = modeName;
    status.txMode = modeName;
    status.trPeriod = static_cast<uint32_t>(FT8DemodSettings::getDecoderFrameDurationMs(decoderMode) / 1000);
    status.configurationName = "Default";

    send(WsjtxUdp::encodeHeartbeat(id, "SDRangel", "FT8 Demod"));
    send(WsjtxUdp::encodeStatus(id, status));

    const uint32_t msSinceMidnight = static_cast<uint32_t>(periodTS.toUTC().time().msecsSinceStartOfDay());

    for (const auto& message : messages)
    {
        if (message.call1 == "UNK") { // message that could not be unpacked
            continue;
        }

        WsjtxUdp::DecodeInfo decode;
        decode.isNew = true;
        decode.msSinceMidnightUTC = msSinceMidnight;
        decode.snr = message.snr;
        decode.deltaTime = message.dt;
        decode.deltaFrequency = static_cast<uint32_t>(message.df < 0.0f ? 0.0f : message.df);
        decode.mode = modeChar;
        const QString text = getMessageText(message);

        if (text.isEmpty()) {
            continue;
        }

        decode.message = text.toStdString();
        send(WsjtxUdp::encodeDecode(id, decode));
    }
}

void WsjtxUdpSender::sendClose()
{
    if (!m_sentSomething) { // nothing was ever announced so nothing to close
        return;
    }

    std::string id;

    {
        QMutexLocker locker(&m_mutex);
        id = m_instanceId.toStdString();
    }

    send(WsjtxUdp::encodeClose(id));
    m_sentSomething = false;
}
