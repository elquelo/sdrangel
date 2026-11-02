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

#ifndef INCLUDE_WSJTXUDPSENDER_H
#define INCLUDE_WSJTXUDPSENDER_H

#include <memory>
#include <vector>

#include <QString>
#include <QList>
#include <QMutex>
#include <QHostAddress>
#include <QDateTime>

#include "util/ft8message.h"
#include "ft8demodsettings.h"

class QUdpSocket;

// Sends the decoded FT8 / FT4 messages as WSJT-X UDP packets (Heartbeat, Status and Decode)
// so that GridTracker, JTAlert and similar programs can use SDRangel as a WSJT-X instance.
//
// The socket is created on first use and must always be used from the same thread
// (the FT8 demod worker thread). The configuration setters may be called from any thread.
class WsjtxUdpSender
{
public:
    WsjtxUdpSender();
    ~WsjtxUdpSender();

    void setDestination(const QString& address, quint16 port);
    //! Instance identifier, the unique key used by the receiving program to tell instances apart
    void setInstanceId(const QString& instanceId);
    //! Reporting (own) station that goes in the DE call and DE grid fields of the Status message
    void setReportingStation(const QString& callsign, const QString& locator);

    //! Send Heartbeat, Status and one Decode message per message of the decoding period
    void sendPeriod(
        const QDateTime& periodTS,
        qint64 baseFrequency,
        FT8DemodSettings::DecoderMode decoderMode,
        const QList<FT8Message>& messages
    );
    //! Tell the receiving program that this instance goes away
    void sendClose();

    //! Text of an FT8 message as it appears in a WSJT-X decode: "CALL1 CALL2 LOC"
    static QString getMessageText(const FT8Message& message);

private:
    void send(const std::vector<uint8_t>& packet);

    QMutex m_mutex; //!< Protects the configuration
    QHostAddress m_address;
    quint16 m_port;
    QString m_instanceId;
    QString m_callsign;
    QString m_locator;
    std::unique_ptr<QUdpSocket> m_socket;
    bool m_sentSomething;
};

#endif // INCLUDE_WSJTXUDPSENDER_H
