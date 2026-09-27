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

#include "HousingBlueprintPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Housing
{
// The client's senders write a zero length for an empty string (and no bytes); SizedCString::BitsSize would write 1.
static void WriteCStringSize(WorldPacket& data, std::string const& value, uint32 bits)
{
    data.WriteBits(value.empty() ? 0 : uint32(value.size() + 1), bits);
}

void HousingBlueprintExport::Read()
{
    _worldPacket >> SizedCString::BitsSize<6>(Name);
    _worldPacket.ResetBitPos();
    _worldPacket >> BlueprintType;
    _worldPacket >> RoomGuid;
    _worldPacket >> SizedCString::Data(Name);
}

void HousingBlueprintRename::Read()
{
    _worldPacket >> BlueprintID;
    _worldPacket >> SizedCString::BitsSize<6>(Name);
    _worldPacket.ResetBitPos();
    _worldPacket >> SizedCString::Data(Name);
}

void HousingBlueprintDelete::Read()
{
    _worldPacket >> BlueprintID;
}

void HousingBlueprintImport::Read()
{
    _worldPacket >> SizedCString::BitsSize<24>(Uuid);
    _worldPacket >> Bits<1>(Flag);
    _worldPacket.ResetBitPos();
    _worldPacket >> BlueprintType;
    _worldPacket >> SourceRoomGuid;
    _worldPacket >> TargetDoorComponentID;
    _worldPacket >> SizedCString::Data(Uuid);
}

void HousingBlueprintRequestContents::Read()
{
    _worldPacket >> SizedCString::BitsSize<24>(Uuid);
    _worldPacket >> Bits<1>(Flag);
    _worldPacket.ResetBitPos();
    _worldPacket >> BlueprintType;
    _worldPacket >> TargetHouseGuid;
    _worldPacket >> SizedCString::Data(Uuid);
}

WorldPacket const* HousingBlueprintExportResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(BlueprintType);
    WriteCStringSize(_worldPacket, Uuid, 24);
    _worldPacket.FlushBits();
    _worldPacket << SizedCString::Data(Uuid);

    return &_worldPacket;
}

WorldPacket const* HousingBlueprintCollection::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint32(Blueprints.size());
    for (JamHousingBlueprint const& blueprint : Blueprints)
    {
        _worldPacket << uint64(blueprint.ID);
        _worldPacket << uint8(blueprint.Type);
        _worldPacket << int64(blueprint.DateCreated);
        _worldPacket << int64(blueprint.DateDeleted);
        _worldPacket << uint8(blueprint.Flags);
        _worldPacket << uint8(blueprint.DeleteReason);
        WriteCStringSize(_worldPacket, blueprint.Uuid, 24);
        WriteCStringSize(_worldPacket, blueprint.Name, 24);
        _worldPacket.FlushBits();
        _worldPacket << SizedCString::Data(blueprint.Uuid);
        _worldPacket << SizedCString::Data(blueprint.Name);
    }

    return &_worldPacket;
}

WorldPacket const* HousingBlueprintRenameResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint64(BlueprintID);
    WriteCStringSize(_worldPacket, Name, 6);
    _worldPacket.FlushBits();
    _worldPacket << SizedCString::Data(Name);

    return &_worldPacket;
}

WorldPacket const* HousingBlueprintDeleteResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint64(BlueprintID);

    return &_worldPacket;
}

WorldPacket const* HousingBlueprintImportResponse::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(BlueprintType);
    _worldPacket << uint32(Unused);
    WriteCStringSize(_worldPacket, Uuid, 24);
    _worldPacket.FlushBits();
    _worldPacket << SizedCString::Data(Uuid);

    return &_worldPacket;
}

static void WriteContentLists(WorldPacket& data, JamBlueprintContentLists const& lists)
{
    // All four counts come first, then the four arrays.
    data << uint32(lists.Decor.size());
    data << uint32(lists.Dyes.size());
    data << uint32(lists.Rooms.size());
    data << uint32(lists.Fixtures.size());
    for (auto const& [id, count] : lists.Decor)
        data << uint32(id) << uint32(count);
    for (auto const& [id, count] : lists.Dyes)
        data << uint32(id) << uint32(count);
    for (uint32 id : lists.Rooms)
        data << uint32(id);
    for (uint32 id : lists.Fixtures)
        data << uint32(id);
}

static void WriteBudgetEntry(WorldPacket& data, JamBlueprintBudgetEntry const& entry)
{
    data << uint8(entry.BudgetType);
    data << int32(entry.Max);
    data << int32(entry.Current);
    data << int32(entry.Cost);
}

WorldPacket const* HousingBlueprintContents::Write()
{
    _worldPacket << uint8(Result);
    _worldPacket << uint8(BlueprintType);
    _worldPacket << TargetHouseGuid;
    _worldPacket << uint32(UnmetRequirementFlags);
    WriteContentLists(_worldPacket, Missing);
    WriteContentLists(_worldPacket, Invalid);
    _worldPacket << uint32(InteriorBudgets.size());
    _worldPacket << uint32(ExteriorBudgets.size());
    for (JamBlueprintBudgetEntry const& entry : InteriorBudgets)
        WriteBudgetEntry(_worldPacket, entry);
    for (JamBlueprintBudgetEntry const& entry : ExteriorBudgets)
        WriteBudgetEntry(_worldPacket, entry);
    _worldPacket << OptionalInit(Contents);
    _worldPacket.FlushBits();
    if (Contents)
        WriteContentLists(_worldPacket, *Contents);
    WriteCStringSize(_worldPacket, Uuid, 24);
    _worldPacket.FlushBits();
    _worldPacket << SizedCString::Data(Uuid);

    return &_worldPacket;
}

WorldPacket const* HousingHouseBudgetsUpdate::Write()
{
    // JamHouseBudgets: interiorBudgets@0, exteriorBudgets@24 (spec §2). HouseGuid is a
    // convenience prefix so the client can associate the update; framing is [INF].
    _worldPacket << HouseGuid;
    _worldPacket << uint32(InteriorBudgets.size());
    for (JamHouseBudgetEntry const& e : InteriorBudgets)
    {
        _worldPacket << uint32(e.BudgetType);
        _worldPacket << int32(e.Max);
        _worldPacket << int32(e.Current);
        _worldPacket << int32(e.Cost);
    }
    _worldPacket << uint32(ExteriorBudgets.size());
    for (JamHouseBudgetEntry const& e : ExteriorBudgets)
    {
        _worldPacket << uint32(e.BudgetType);
        _worldPacket << int32(e.Max);
        _worldPacket << int32(e.Current);
        _worldPacket << int32(e.Cost);
    }
    return &_worldPacket;
}
}
