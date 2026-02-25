/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TransmogrificationPackets.h"
#include "Log.h"
#include "PacketOperators.h"
#include "Util.h"
#include <algorithm>
#include <cctype>
#include <span>
#include <sstream>

namespace WorldPackets::Transmogrification
{

namespace
{
void CapturePayloadDebugInfo(WorldPacket const& packet, size_t& payloadSize, std::string& payloadPreviewHex)
{
    payloadSize = packet.size();
    size_t previewSize = std::min<size_t>(packet.size(), 128);
    payloadPreviewHex = ByteArrayToHexStr(std::span(packet.data(), previewSize));
}

template <typename T>
T ReadLE(std::span<uint8 const> payload, size_t offset)
{
    T value = 0;
    for (size_t i = 0; i < sizeof(T) && (offset + i) < payload.size(); ++i)
        value |= T(payload[offset + i]) << (i * 8);
    return value;
}

std::string BuildDiagnosticReadTrace(char const* opcodeName, WorldPacket const& packet)
{
    std::span<uint8 const> payload(packet.data(), packet.size());
    std::ostringstream trace;
    trace << opcodeName << " payload-bytes=" << payload.size();

    if (payload.size() >= 1)
        trace << " | u8@0=" << uint32(payload[0]);
    if (payload.size() >= 2)
        trace << " | u16@0=" << ReadLE<uint16>(payload, 0);
    if (payload.size() >= 4)
        trace << " | u32@0=" << ReadLE<uint32>(payload, 0);
    if (payload.size() >= 8)
        trace << " | u64@0=" << ReadLE<uint64>(payload, 0);
    if (payload.size() >= 12)
        trace << " | u32@4=" << ReadLE<uint32>(payload, 4);
    if (payload.size() >= 16)
        trace << " | u64@8=" << ReadLE<uint64>(payload, 8);

    return trace.str();
}



bool ParseOutfitNameAndMiddleFields(WorldPacket& packet, uint8& middleType, uint8& middleFlags, uint32& iconFileDataID, std::string& setName, std::string& parseError)
{
    std::span<uint8 const> remaining(packet.data() + packet.rpos(), packet.size() - packet.rpos());

    if (remaining.size() < 8)
    {
        parseError = "payload too short for middle fields + name trailer";
        return false;
    }

    size_t asciiStart = remaining.size();
    while (asciiStart > 0)
    {
        uint8 byte = remaining[asciiStart - 1];
        if (byte < 0x20 || byte > 0x7E)
            break;

        --asciiStart;
    }

    size_t nameLength = remaining.size() - asciiStart;
    if (nameLength == 0)
    {
        parseError = "no trailing printable outfit name found";
        return false;
    }

    if (asciiStart < 2)
    {
        parseError = "missing name length/alignment bytes before trailing name";
        return false;
    }

    uint8 nameLengthByte = remaining[asciiStart - 2];
    if (nameLengthByte != nameLength)
    {
        parseError = Trinity::StringFormat("name length mismatch (lenByte={} trailingAsciiLen={})", nameLengthByte, nameLength);
        return false;
    }

    size_t middleLength = asciiStart - 2;
    if (middleLength < 6)
    {
        parseError = Trinity::StringFormat("middle section too short ({} bytes)", middleLength);
        return false;
    }

    middleType = remaining[0];
    middleFlags = remaining[1];
    iconFileDataID = ReadLE<uint32>(remaining, 2);

    setName.assign(reinterpret_cast<char const*>(remaining.data() + asciiStart), nameLength);

    packet.rfinish();
    return true;
}

void AssignSlotAppearance(EquipmentSetInfo::EquipmentSetData& set, uint32 slotIndex, int32 appearanceID)
{
    // Captures show this field is a direct equipment slot index (e.g. 15, 16), not a bitmask.
    if (slotIndex < EQUIPMENT_SLOT_END)
        set.Appearances[slotIndex] = appearanceID;
}
}

ByteBuffer& operator>>(ByteBuffer& data, TransmogrifyItem& transmogItem)
{
    data >> transmogItem.ItemModifiedAppearanceID;
    data >> transmogItem.Slot;
    data >> transmogItem.SpellItemEnchantmentID;
    data >> transmogItem.SecondaryItemModifiedAppearanceID;

    return data;
}

void TransmogrifyItems::Read()
{
    _worldPacket >> Size<uint32>(Items);
    _worldPacket >> Npc;
    for (TransmogrifyItem& item : Items)
        _worldPacket >> item;

    _worldPacket >> Bits<1>(CurrentSpecOnly);
}

void TransmogOutfitNew::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

    Set.Type = EquipmentSetInfo::TRANSMOG;
    ParseSuccess = false;

    try
    {
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();
        if (!ParseOutfitNameAndMiddleFields(_worldPacket, MiddleType, MiddleFlags, IconFileDataID, Set.SetName, ParseError))
        {
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
            return;
        }

        Set.SetIcon = std::to_string(IconFileDataID);
        ParseSuccess = true;
        ParseError.clear();

        DiagnosticReadTrace = Trinity::StringFormat("playerGuid={} rposAfterGuid={} middleType={} middleFlags={} iconFileDataId={} name='{}'",
            PlayerGuid.ToString(), rposAfterGuid, MiddleType, MiddleFlags, IconFileDataID, Set.SetName);

        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_NEW diag: {}", DiagnosticReadTrace);
    }
    catch (std::exception const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
        _worldPacket.rfinish();
    }
}

void TransmogOutfitUpdateInfo::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

    ParseSuccess = false;
    Set.Type = EquipmentSetInfo::TRANSMOG;

    try
    {
        _worldPacket >> Set.SetID;
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();
        if (!ParseOutfitNameAndMiddleFields(_worldPacket, MiddleType, MiddleFlags, IconFileDataID, Set.SetName, ParseError))
        {
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
            return;
        }

        Set.SetIcon = std::to_string(IconFileDataID);
        ParseSuccess = true;
        ParseError.clear();

        DiagnosticReadTrace = Trinity::StringFormat("setId={} playerGuid={} rposAfterGuid={} middleType={} middleFlags={} iconFileDataId={} name='{}'",
            Set.SetID, PlayerGuid.ToString(), rposAfterGuid, MiddleType, MiddleFlags, IconFileDataID, Set.SetName);

        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO diag: {}", DiagnosticReadTrace);
    }
    catch (std::exception const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
        _worldPacket.rfinish();
    }
}

void TransmogOutfitUpdateSlots::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

    try
    {
        _worldPacket >> Set.SetID;

        uint32 slotCount = 0;
        _worldPacket >> slotCount;
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();
        uint8 guidTrailingByte = 0;
        if (_worldPacket.rpos() < _worldPacket.size())
            _worldPacket >> guidTrailingByte;

        Slots.resize(slotCount);

        Set.Type = EquipmentSetInfo::TRANSMOG;
        Set.IgnoreMask = 0;

        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS diag: setId={} slotCount={} playerGuid={} rposAfterGuid={} guidTrailingByte=0x{:X}",
            Set.SetID, slotCount, PlayerGuid.ToString(), rposAfterGuid, guidTrailingByte);

        for (TransmogOutfitSlotEntry& slot : Slots)
        {
            _worldPacket >> slot.AppearanceID;
            _worldPacket >> slot.RawSlotField;
            _worldPacket >> slot.Reserved1;
            _worldPacket >> slot.Reserved2;

            uint8 slotIndex = slot.GetSlotIndex();
            AssignSlotAppearance(Set, slotIndex, int32(slot.AppearanceID));

            TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS slot entry: appearance={} rawSlotField=0x{:X} slotIndex={} reserved1={} reserved2={}",
                slot.AppearanceID, slot.RawSlotField, slotIndex, slot.Reserved1, slot.Reserved2);
        }

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (!Set.Appearances[slot])
                Set.IgnoreMask |= (1u << slot);

        ParseSuccess = true;
        ParseError.clear();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS", _worldPacket);
    }
    catch (std::exception const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS", _worldPacket);
    }

    _worldPacket.rfinish();
}

void TransmogOutfitUpdateSituations::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

    try
    {
        _worldPacket >> SetID;
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();

        uint32 count = 0;
        _worldPacket >> count;

        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS diag: setId={} playerGuid={} rposAfterGuid={} count={}",
            SetID, PlayerGuid.ToString(), rposAfterGuid, count);

        Situations.resize(count);
        for (TransmogOutfitSituationEntry& entry : Situations)
        {
            _worldPacket >> entry.SituationID;
            _worldPacket >> entry.SpecID;
            _worldPacket >> entry.LoadoutID;
            _worldPacket >> entry.EquipmentSetID;
        }

        ParseSuccess = true;
        ParseError.clear();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS", _worldPacket);
    }
    catch (std::exception const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS", _worldPacket);
    }

    _worldPacket.rfinish();
}

WorldPacket const* TransmogOutfitInfoUpdated::Write()
{
    _worldPacket << uint32(SetID);
    _worldPacket << uint64(Guid);
    return &_worldPacket;
}

WorldPacket const* TransmogOutfitNewEntryAdded::Write()
{
    _worldPacket << uint32(SetID);
    _worldPacket << uint64(Guid);
    return &_worldPacket;
}

WorldPacket const* TransmogOutfitSituationsUpdated::Write()
{
    _worldPacket << uint32(SetID);
    _worldPacket << uint64(Guid);
    return &_worldPacket;
}

WorldPacket const* TransmogOutfitSlotsUpdated::Write()
{
    _worldPacket << uint32(SetID);
    _worldPacket << uint64(Guid);
    return &_worldPacket;
}

WorldPacket const* AccountTransmogUpdate::Write()
{
    _worldPacket << Bits<1>(IsFullUpdate);
    _worldPacket << Bits<1>(IsSetFavorite);
    _worldPacket << Size<uint32>(FavoriteAppearances);
    _worldPacket << Size<uint32>(NewAppearances);
    if (!FavoriteAppearances.empty())
        _worldPacket.append(FavoriteAppearances.data(), FavoriteAppearances.size());

    if (!NewAppearances.empty())
        _worldPacket.append(NewAppearances.data(), NewAppearances.size());

    return &_worldPacket;
}
}
