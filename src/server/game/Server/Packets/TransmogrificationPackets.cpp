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

    ParseSuccess = false;
    Set.Type = EquipmentSetInfo::TRANSMOG;

    try
    {
        _worldPacket >> PlayerGuid;

        std::size_t rposAfterGuid = _worldPacket.rpos();

        std::span<uint8 const> payload(_worldPacket.data(), _worldPacket.size());
        uint8 b8 = payload.size() > 8 ? payload[8] : 0;
        uint8 b9 = payload.size() > 9 ? payload[9] : 0;
        uint8 b10 = payload.size() > 10 ? payload[10] : 0;
        uint8 b11 = payload.size() > 11 ? payload[11] : 0;
        uint8 b12 = payload.size() > 12 ? payload[12] : 0;
        uint8 b13 = payload.size() > 13 ? payload[13] : 0;

        uint16 u16_8 = payload.size() >= 10 ? ReadLE<uint16>(payload, 8) : 0;
        uint32 u32_10 = payload.size() >= 14 ? ReadLE<uint32>(payload, 10) : 0;
        uint32 u32_8 = payload.size() >= 12 ? ReadLE<uint32>(payload, 8) : 0;
        uint16 u16_12 = payload.size() >= 14 ? ReadLE<uint16>(payload, 12) : 0;

        uint32 nameLength = _worldPacket.ReadBits(8);
        _worldPacket.FlushBits();
        Set.SetName = _worldPacket.ReadString(nameLength, false);

        TC_LOG_ERROR("network", "CMSG_TRANSMOG_OUTFIT_NEW diag: playerGuid={} rposAfterGuid={} raw[8..13]=[{}, {}, {}, {}, {}, {}] as(u16+u32)=[{}, {}] as(u32+u16)=[{}, {}] nameLen={} name='{}' payloadSize={} payloadHex={}",
            PlayerGuid.ToString(), rposAfterGuid, b8, b9, b10, b11, b12, b13, u16_8, u32_10, u32_8, u16_12, nameLength, Set.SetName, PayloadSize, PayloadPreviewHex);

        ParseError = "structured diagnostic parser active: NEW middle fields unresolved";
        std::ostringstream trace;
        trace << "playerGuid=" << PlayerGuid.ToString() << " name='" << Set.SetName << "'";
        DiagnosticReadTrace = trace.str();
    }
    catch (std::exception const& ex)
    {
        ParseError = ex.what();
        DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_NEW", _worldPacket);
    }

    _worldPacket.rfinish();
}

void TransmogOutfitUpdateInfo::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);
    DiagnosticReadTrace = BuildDiagnosticReadTrace("CMSG_TRANSMOG_OUTFIT_UPDATE_INFO", _worldPacket);
    ParseSuccess = false;
    ParseError = "diagnostic parser active: transmog outfit UPDATE_INFO layout still unknown";
    _worldPacket.rfinish();
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
            _worldPacket >> slot.Unknown1;
            _worldPacket >> slot.SlotFlags;
            _worldPacket >> slot.Unknown2;

            AssignSlotAppearance(Set, slot.SlotFlags, int32(slot.AppearanceID));

            TC_LOG_DEBUG("network.opcode.transmog", "CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS slot entry: appearance={} unknown1={} slotIndex={} unknown2={}",
                slot.AppearanceID, slot.Unknown1, slot.SlotFlags, slot.Unknown2);
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
