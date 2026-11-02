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

#ifndef INCLUDE_WSJTXUDPENCODER_H
#define INCLUDE_WSJTXUDPENCODER_H

// Encoder for the WSJT-X UDP "NetworkMessage" protocol (schema 3, QDataStream Qt_5_4 layout)
// as documented in NetworkMessage.hpp of the WSJT-X sources. This lets programs such as
// GridTracker or JTAlert treat the FT8 demodulator as a WSJT-X instance.
//
// This file deliberately does not depend on Qt so that it can be unit tested standalone.
// Everything is big endian. Strings are utf8 byte arrays: a quint32 length followed by the
// bytes. Only the "Out" messages sent by a client are produced here.

#include <cstdint>
#include <string>
#include <vector>

namespace WsjtxUdp
{

static const uint32_t MAGIC_NUMBER = 0xadbccbda;
static const uint32_t SCHEMA_NUMBER = 3;
static const uint32_t NOT_APPLICABLE = 0xffffffff; // Frequency tolerance and T/R period unused

enum MessageType : uint32_t
{
    TypeHeartbeat = 0,
    TypeStatus    = 1,
    TypeDecode    = 2,
    TypeClear     = 3,
    TypeClose     = 6
};

struct StatusInfo
{
    uint64_t    dialFrequency = 0;      //!< Hz
    std::string mode;                   //!< e.g. "FT8"
    std::string dxCall;
    std::string report;
    std::string txMode;                 //!< e.g. "FT8"
    bool        txEnabled = false;
    bool        transmitting = false;
    bool        decoding = false;
    uint32_t    rxDF = 0;               //!< Hz
    uint32_t    txDF = 0;               //!< Hz
    std::string deCall;                 //!< Reporting (own) station
    std::string deGrid;
    std::string dxGrid;
    bool        txWatchdog = false;
    std::string subMode;
    bool        fastMode = false;
    uint8_t     specialOperationMode = 0; //!< 0 = NONE
    uint32_t    frequencyTolerance = NOT_APPLICABLE;
    uint32_t    trPeriod = 15;          //!< seconds
    std::string configurationName;
    std::string txMessage;
};

struct DecodeInfo
{
    bool        isNew = true;
    uint32_t    msSinceMidnightUTC = 0; //!< QTime of the decode (UTC)
    int32_t     snr = 0;                //!< dB
    double      deltaTime = 0.0;        //!< s
    uint32_t    deltaFrequency = 0;     //!< Hz offset in the audio passband
    std::string mode;                   //!< Decode mode character: "~" FT8, "+" FT4
    std::string message;                //!< Full message text, e.g. "CQ DX K1ABC FN42"
    bool        lowConfidence = false;
    bool        offAir = false;
};

class PacketBuilder
{
public:
    PacketBuilder(MessageType type, const std::string& id, uint32_t schema = SCHEMA_NUMBER)
    {
        addU32(MAGIC_NUMBER);
        addU32(schema);
        addU32(static_cast<uint32_t>(type));
        addUtf8(id);
    }

    void addBool(bool value) { m_data.push_back(value ? 1 : 0); }
    void addU8(uint8_t value) { m_data.push_back(value); }

    void addU32(uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8) {
            m_data.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
        }
    }

    void addI32(int32_t value) { addU32(static_cast<uint32_t>(value)); }

    void addU64(uint64_t value)
    {
        for (int shift = 56; shift >= 0; shift -= 8) {
            m_data.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
        }
    }

    void addDouble(double value)
    {
        // QDataStream default (DoublePrecision) is IEEE 754 binary64, big endian
        union { double d; uint64_t u; } conv;
        conv.d = value;
        addU64(conv.u);
    }

    void addUtf8(const std::string& value)
    {
        addU32(static_cast<uint32_t>(value.size()));
        m_data.insert(m_data.end(), value.begin(), value.end());
    }

    void addQTime(uint32_t msSinceMidnight) { addU32(msSinceMidnight); }

    const std::vector<uint8_t>& data() const { return m_data; }

private:
    std::vector<uint8_t> m_data;
};

inline std::vector<uint8_t> encodeHeartbeat(
    const std::string& id,
    const std::string& version,
    const std::string& revision,
    uint32_t maxSchema = SCHEMA_NUMBER)
{
    PacketBuilder b(TypeHeartbeat, id);
    b.addU32(maxSchema);
    b.addUtf8(version);
    b.addUtf8(revision);
    return b.data();
}

inline std::vector<uint8_t> encodeStatus(const std::string& id, const StatusInfo& s)
{
    PacketBuilder b(TypeStatus, id);
    b.addU64(s.dialFrequency);
    b.addUtf8(s.mode);
    b.addUtf8(s.dxCall);
    b.addUtf8(s.report);
    b.addUtf8(s.txMode);
    b.addBool(s.txEnabled);
    b.addBool(s.transmitting);
    b.addBool(s.decoding);
    b.addU32(s.rxDF);
    b.addU32(s.txDF);
    b.addUtf8(s.deCall);
    b.addUtf8(s.deGrid);
    b.addUtf8(s.dxGrid);
    b.addBool(s.txWatchdog);
    b.addUtf8(s.subMode);
    b.addBool(s.fastMode);
    b.addU8(s.specialOperationMode);
    b.addU32(s.frequencyTolerance);
    b.addU32(s.trPeriod);
    b.addUtf8(s.configurationName);
    b.addUtf8(s.txMessage);
    return b.data();
}

inline std::vector<uint8_t> encodeDecode(const std::string& id, const DecodeInfo& d)
{
    PacketBuilder b(TypeDecode, id);
    b.addBool(d.isNew);
    b.addQTime(d.msSinceMidnightUTC);
    b.addI32(d.snr);
    b.addDouble(d.deltaTime);
    b.addU32(d.deltaFrequency);
    b.addUtf8(d.mode);
    b.addUtf8(d.message);
    b.addBool(d.lowConfidence);
    b.addBool(d.offAir);
    return b.data();
}

//! Clear (Out direction): has no payload, the Window field exists in the In direction only
inline std::vector<uint8_t> encodeClear(const std::string& id)
{
    PacketBuilder b(TypeClear, id);
    return b.data();
}

inline std::vector<uint8_t> encodeClose(const std::string& id)
{
    PacketBuilder b(TypeClose, id);
    return b.data();
}

} // namespace WsjtxUdp

#endif // INCLUDE_WSJTXUDPENCODER_H
