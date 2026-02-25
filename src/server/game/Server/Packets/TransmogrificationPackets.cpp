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
<<<<<<< ours
#include <span>
=======
#include <cctype>
#include <span>
#include <sstream>
>>>>>>> theirs

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

<<<<<<< ours
void ReadTransmogOutfitAsEquipmentSetData(WorldPacket& worldPacket, EquipmentSetInfo::EquipmentSetData& set)
{
    worldPacket >> As<int32>(set.Type);
    worldPacket >> set.Guid;
    worldPacket >> set.SetID;
    worldPacket >> set.IgnoreMask;

    for (uint8 i = 0; i < EQUIPMENT_SET_SLOTS; ++i)
    {
        worldPacket >> set.Pieces[i];
        worldPacket >> set.Appearances[i];
    }

    worldPacket >> set.Enchants[0];
    worldPacket >> set.Enchants[1];

    worldPacket >> set.SecondaryShoulderApparanceID;
    worldPacket >> set.SecondaryShoulderSlot;
    worldPacket >> set.SecondaryWeaponAppearanceID;
    worldPacket >> set.SecondaryWeaponSlot;

    worldPacket >> OptionalInit(set.AssignedSpecIndex);
    worldPacket >> SizedString::BitsSize<8>(set.SetName);
    worldPacket >> SizedString::BitsSize<9>(set.SetIcon);

    if (set.AssignedSpecIndex)
        worldPacket >> *set.AssignedSpecIndex;

    worldPacket >> SizedString::Data(set.SetName);
    worldPacket >> SizedString::Data(set.SetIcon);
=======
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



uint8 TransmogOutfitSlotToEquipSlot(uint8 transmogSlot)
{
    switch (transmogSlot)
    {
        case 0:  return EQUIPMENT_SLOT_HEAD;
        case 1:  return EQUIPMENT_SLOT_SHOULDERS;
        case 2:  return EQUIPMENT_SLOT_SHOULDERS;
        case 3:  return EQUIPMENT_SLOT_BACK;
        case 4:  return EQUIPMENT_SLOT_CHEST;
        case 5:  return EQUIPMENT_SLOT_TABARD;
        case 6:  return EQUIPMENT_SLOT_BODY;
        case 7:  return EQUIPMENT_SLOT_WRISTS;
        case 8:  return EQUIPMENT_SLOT_HANDS;
        case 9:  return EQUIPMENT_SLOT_WAIST;
        case 10: return EQUIPMENT_SLOT_LEGS;
        case 11: return EQUIPMENT_SLOT_FEET;
        case 12: return EQUIPMENT_SLOT_MAINHAND;
        case 13: return EQUIPMENT_SLOT_OFFHAND;
        case 14: return EQUIPMENT_SLOT_RANGED;
        default: return EQUIPMENT_SLOT_END;
    }
>>>>>>> theirs
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

<<<<<<< ours

=======
>>>>>>> theirs
void TransmogOutfitNew::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

<<<<<<< ours
    try
    {
        ReadTransmogOutfitAsEquipmentSetData(_worldPacket, Set);
    }
    catch (ByteBufferException const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        _worldPacket.rfinish();
    }
=======
    Set.Type = EquipmentSetInfo::TRANSMOG;
    ParseSuccess = false;

    try
    {
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();
        std::span<uint8 const> remaining(_worldPacket.data() + rposAfterGuid, _worldPacket.size() - rposAfterGuid);
        if (remaining.size() < 8)
        {
            ParseError = Trinity::StringFormat("payload too short for CMSG_TRANSMOG_OUTFIT_NEW (remaining={})", remaining.size());
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
            return;
        }

        // NEW middle format: [type:u8][flags:u8][icon:u32], then [nameLen:u8][padding:u8][name bytes]
        MiddleType = remaining[0];
        MiddleFlags = remaining[1];
        IconFileDataID = ReadLE<uint32>(remaining, 2);

        std::size_t asciiStart = remaining.size();
        while (asciiStart > 0)
        {
            uint8 b = remaining[asciiStart - 1];
            if (b < 0x20 || b > 0x7E)
                break;
            --asciiStart;
        }

        std::size_t nameLength = remaining.size() - asciiStart;
        if (nameLength == 0 || asciiStart < 2)
        {
            ParseError = "missing trailing outfit name or name trailer";
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
            return;
        }

        uint8 nameLengthByte = remaining[asciiStart - 2];
        if (nameLengthByte != nameLength)
        {
            ParseError = Trinity::StringFormat("name length mismatch (lenByte={} trailingAsciiLen={})", nameLengthByte, nameLength);
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
            return;
        }

        std::size_t middleLength = asciiStart - 2;
        if (middleLength != 6)
        {
            ParseError = Trinity::StringFormat("unexpected middle size for OUTFIT_NEW (got={} expected=6)", middleLength);
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
            return;
        }

        Set.SetName.assign(reinterpret_cast<char const*>(remaining.data() + asciiStart), nameLength);
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
    }

    _worldPacket.rfinish();
>>>>>>> theirs
}

void TransmogOutfitUpdateInfo::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

<<<<<<< ours
    try
    {
        ReadTransmogOutfitAsEquipmentSetData(_worldPacket, Set);
    }
    catch (ByteBufferException const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        _worldPacket.rfinish();
    }
}

// TransmogOutfitSlot (client enum 0-14) -> EQUIPMENT_SLOT (server 0-18) mapping.
// From TransmogOutfitConstantsDocumentation.lua
static uint8 TransmogOutfitSlotToEquipSlot(uint8 transmogSlot)
{
    static constexpr uint8 map[] = {
         0, // Head(0)            -> EQUIPMENT_SLOT_HEAD(0)
         2, // ShoulderRight(1)   -> EQUIPMENT_SLOT_SHOULDERS(2)
         2, // ShoulderLeft(2)    -> EQUIPMENT_SLOT_SHOULDERS(2)
        14, // Back(3)            -> EQUIPMENT_SLOT_BACK(14)
         4, // Chest(4)           -> EQUIPMENT_SLOT_CHEST(4)
        18, // Tabard(5)          -> EQUIPMENT_SLOT_TABARD(18)
         3, // Body(6)            -> EQUIPMENT_SLOT_BODY(3)
         8, // Wrist(7)           -> EQUIPMENT_SLOT_WRISTS(8)
         9, // Hand(8)            -> EQUIPMENT_SLOT_HANDS(9)
         5, // Waist(9)           -> EQUIPMENT_SLOT_WAIST(5)
         6, // Legs(10)           -> EQUIPMENT_SLOT_LEGS(6)
         7, // Feet(11)           -> EQUIPMENT_SLOT_FEET(7)
        15, // WeaponMainHand(12) -> EQUIPMENT_SLOT_MAINHAND(15)
        16, // WeaponOffHand(13)  -> EQUIPMENT_SLOT_OFFHAND(16)
        17, // WeaponRanged(14)   -> EQUIPMENT_SLOT_RANGED(17)
    };
    if (transmogSlot < std::size(map))
        return map[transmogSlot];
    return transmogSlot;
=======
    ParseSuccess = false;
    Set.Type = EquipmentSetInfo::TRANSMOG;

    try
    {
        _worldPacket >> Set.SetID;
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();
        std::span<uint8 const> remaining(_worldPacket.data() + rposAfterGuid, _worldPacket.size() - rposAfterGuid);
        if (remaining.size() < 7)
        {
            ParseError = Trinity::StringFormat("payload too short for CMSG_TRANSMOG_OUTFIT_UPDATE_INFO (remaining={})", remaining.size());
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
            return;
        }

        // UPDATE_INFO middle format: [type:u8][icon:u32], then [nameLen:u8][padding:u8][name bytes]
        MiddleType = remaining[0];
        MiddleFlags = 0;
        IconFileDataID = ReadLE<uint32>(remaining, 1);

        std::size_t asciiStart = remaining.size();
        while (asciiStart > 0)
        {
            uint8 b = remaining[asciiStart - 1];
            if (b < 0x20 || b > 0x7E)
                break;
            --asciiStart;
        }

        std::size_t nameLength = remaining.size() - asciiStart;
        if (nameLength == 0 || asciiStart < 2)
        {
            ParseError = "missing trailing outfit name or name trailer";
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
            return;
        }

        uint8 nameLengthByte = remaining[asciiStart - 2];
        if (nameLengthByte != nameLength)
        {
            ParseError = Trinity::StringFormat("name length mismatch (lenByte={} trailingAsciiLen={})", nameLengthByte, nameLength);
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
            return;
        }

        std::size_t middleLength = asciiStart - 2;
        if (middleLength != 5)
        {
            ParseError = Trinity::StringFormat("unexpected middle size for UPDATE_INFO (got={} expected=5)", middleLength);
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
            return;
        }

        Set.SetName.assign(reinterpret_cast<char const*>(remaining.data() + asciiStart), nameLength);
        Set.SetIcon = std::to_string(IconFileDataID);
        ParseSuccess = true;
        ParseError.clear();

        DiagnosticReadTrace = Trinity::StringFormat("setId={} playerGuid={} rposAfterGuid={} middleType={} iconFileDataId={} name='{}'",
            Set.SetID, PlayerGuid.ToString(), rposAfterGuid, MiddleType, IconFileDataID, Set.SetName);

        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_INFO diag: {}", DiagnosticReadTrace);
    }
    catch (std::exception const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
    }

    _worldPacket.rfinish();
>>>>>>> theirs
}

void TransmogOutfitUpdateSlots::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

<<<<<<< ours
    try
    {
        ReadTransmogOutfitAsEquipmentSetData(_worldPacket, Set);
    }
    catch (ByteBufferException const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        _worldPacket.rfinish();
    }
=======
    ParseSuccess = false;

    try
    {
        _worldPacket >> Set.SetID;

        uint32 slotCount = 0;
        _worldPacket >> slotCount;
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();

        std::size_t expectedSlotBytes = std::size_t(slotCount) * 16;
        std::size_t bytesRemainingAfterGuid = _worldPacket.size() - _worldPacket.rpos();
        if (bytesRemainingAfterGuid < expectedSlotBytes)
        {
            ParseError = Trinity::StringFormat("slot payload truncated (slotCount={} expectedSlotBytes={} remaining={})", slotCount, expectedSlotBytes, bytesRemainingAfterGuid);
            DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS", _worldPacket);
            _worldPacket.rfinish();
            return;
        }

        // UPDATE_SLOTS has optional trailing alignment bytes between packed guid and slot entries.
        std::size_t bytesBeforeSlots = bytesRemainingAfterGuid - expectedSlotBytes;
        for (std::size_t i = 0; i < bytesBeforeSlots; ++i)
            _worldPacket.read_skip<uint8>();

        Slots.resize(slotCount);

        Set.Type = EquipmentSetInfo::TRANSMOG;
        Set.IgnoreMask = 0;

        TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS diag: setId={} slotCount={} playerGuid={} rposAfterGuid={} bytesBeforeSlots={}",
            Set.SetID, slotCount, PlayerGuid.ToString(), rposAfterGuid, bytesBeforeSlots);

        for (TransmogOutfitSlotEntry& slot : Slots)
        {
            _worldPacket >> slot.AppearanceID;
            _worldPacket >> slot.RawSlotField;
            _worldPacket >> slot.Reserved1;
            _worldPacket >> slot.Reserved2;

            uint8 transmogSlot = slot.GetSlotIndex();
            uint8 equipSlot = TransmogOutfitSlotToEquipSlot(transmogSlot);
            if (equipSlot < EQUIPMENT_SLOT_END)
                Set.Appearances[equipSlot] = int32(slot.AppearanceID);

            TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS slot entry: appearance={} rawSlotField=0x{:X} transmogSlot={} equipSlot={} reserved1={} reserved2={}",
                slot.AppearanceID, slot.RawSlotField, transmogSlot, equipSlot, slot.Reserved1, slot.Reserved2);
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
>>>>>>> theirs
}

void TransmogOutfitUpdateSituations::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

<<<<<<< ours
    try
    {
        _worldPacket >> Guid;
        _worldPacket >> SetID;

        if (_worldPacket.rpos() < _worldPacket.size())
        {
            uint32 count = _worldPacket.read<uint32>();
            uint32 maxCountByPayload = uint32((_worldPacket.size() - _worldPacket.rpos()) / (sizeof(uint32) * 4));
            count = std::min(count, maxCountByPayload);
            Situations.reserve(count);
            for (uint32 i = 0; i < count; ++i)
            {
                TransmogOutfitSituationEntry entry;
                _worldPacket >> entry.SituationID;
                _worldPacket >> entry.SpecID;
                _worldPacket >> entry.LoadoutID;
                _worldPacket >> entry.EquipmentSetID;
                Situations.push_back(entry);
            }
        }
    }
    catch (ByteBufferException const& ex)
    {
        ParseSuccess = false;
        ParseError = ex.what();
        _worldPacket.rfinish();
    }
=======
    ParseSuccess = false;

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
>>>>>>> theirs
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
