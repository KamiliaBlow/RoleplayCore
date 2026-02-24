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

void TransmogOutfitUpdateInfo::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

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

void TransmogOutfitUpdateSlots::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

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

void TransmogOutfitUpdateSituations::Read()
{
    CapturePayloadDebugInfo(_worldPacket, PayloadSize, PayloadPreviewHex);

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
