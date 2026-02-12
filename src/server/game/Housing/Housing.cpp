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

#include "Housing.h"
#include "DatabaseEnv.h"
#include "HousingMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include <cmath>

Housing::Housing(Player* owner)
    : _owner(owner)
    , _plotIndex(INVALID_PLOT_INDEX)
    , _level(1)
    , _favor(0)
    , _settingsFlags(HOUSE_SETTING_ALLOW_VISITORS)
    , _editorMode(HOUSING_EDITOR_MODE_NONE)
    , _decorDbIdGenerator(1)
    , _roomDbIdGenerator(1)
{
}

bool Housing::LoadFromDB(PreparedQueryResult housing, PreparedQueryResult decor,
    PreparedQueryResult rooms, PreparedQueryResult fixtures, PreparedQueryResult catalog)
{
    if (!housing)
        return false;

    Field* fields = housing->Fetch();
    // Expected columns: houseGuid, neighborhoodGuid, plotIndex, level, favor, settingsFlags
    _houseGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, fields[0].GetUInt64());
    _neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, fields[1].GetUInt64());
    _plotIndex = fields[2].GetUInt8();
    _level = fields[3].GetUInt32();
    _favor = fields[4].GetUInt32();
    _settingsFlags = fields[5].GetUInt32();

    // Load placed decor
    //           0        1             2     3     4     5          6          7          8          9       10       11       12
    // SELECT decorGuid, decorEntryId, posX, posY, posZ, rotationX, rotationY, rotationZ, rotationW, dyeSlot0, dyeSlot1, dyeSlot2, roomGuid
    // FROM character_housing_decor WHERE ownerGuid = ?
    if (decor)
    {
        do
        {
            fields = decor->Fetch();

            uint64 decorDbId = fields[0].GetUInt64();
            ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, decorDbId);

            PlacedDecor& placed = _placedDecor[decorGuid];
            placed.Guid = decorGuid;
            placed.DecorEntryId = fields[1].GetUInt32();
            placed.PosX = fields[2].GetFloat();
            placed.PosY = fields[3].GetFloat();
            placed.PosZ = fields[4].GetFloat();
            placed.RotationX = fields[5].GetFloat();
            placed.RotationY = fields[6].GetFloat();
            placed.RotationZ = fields[7].GetFloat();
            placed.RotationW = fields[8].GetFloat();
            placed.DyeSlots[0] = fields[9].GetUInt32();
            placed.DyeSlots[1] = fields[10].GetUInt32();
            placed.DyeSlots[2] = fields[11].GetUInt32();
            uint64 roomDbId = fields[12].GetUInt64();
            if (roomDbId)
                placed.RoomGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, roomDbId);

            if (decorDbId >= _decorDbIdGenerator)
                _decorDbIdGenerator = decorDbId + 1;

        } while (decor->NextRow());
    }

    // Load rooms
    //           0         1            2           3            4         5        6             7           8           9         10              11
    // SELECT roomGuid, roomEntryId, slotIndex, orientation, mirrored, themeId, wallpaperId, materialId, doorTypeId, doorSlot, ceilingTypeId, ceilingSlot
    // FROM character_housing_rooms WHERE ownerGuid = ?
    if (rooms)
    {
        do
        {
            fields = rooms->Fetch();

            uint64 roomDbId = fields[0].GetUInt64();
            ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, roomDbId);

            Room& room = _rooms[roomGuid];
            room.Guid = roomGuid;
            room.RoomEntryId = fields[1].GetUInt32();
            room.SlotIndex = fields[2].GetUInt32();
            room.Orientation = fields[3].GetUInt32();
            room.Mirrored = fields[4].GetBool();
            room.ThemeId = fields[5].GetUInt32();
            room.WallpaperId = fields[6].GetUInt32();
            room.MaterialId = fields[7].GetUInt32();
            room.DoorTypeId = fields[8].GetUInt32();
            room.DoorSlot = fields[9].GetUInt8();
            room.CeilingTypeId = fields[10].GetUInt32();
            room.CeilingSlot = fields[11].GetUInt8();

            if (roomDbId >= _roomDbIdGenerator)
                _roomDbIdGenerator = roomDbId + 1;

        } while (rooms->NextRow());
    }

    // Load fixtures
    //           0               1
    // SELECT fixturePointId, optionId
    // FROM character_housing_fixtures WHERE ownerGuid = ?
    if (fixtures)
    {
        do
        {
            fields = fixtures->Fetch();

            uint32 fixturePointId = fields[0].GetUInt32();
            Fixture& fixture = _fixtures[fixturePointId];
            fixture.FixturePointId = fixturePointId;
            fixture.OptionId = fields[1].GetUInt32();

        } while (fixtures->NextRow());
    }

    // Load catalog
    //           0             1
    // SELECT decorEntryId, count
    // FROM character_housing_catalog WHERE ownerGuid = ?
    if (catalog)
    {
        do
        {
            fields = catalog->Fetch();

            uint32 decorEntryId = fields[0].GetUInt32();
            CatalogEntry& entry = _catalog[decorEntryId];
            entry.DecorEntryId = decorEntryId;
            entry.Count = fields[1].GetUInt32();

        } while (catalog->NextRow());
    }

    // Recalculate budget weights from loaded data
    RecalculateBudgets();

    TC_LOG_DEBUG("housing", "Housing::LoadFromDB: Loaded house for player {} (GUID {}): "
        "{} decor, {} rooms, {} fixtures, {} catalog entries (interior budget {}/{}, room budget {}/{})",
        _owner->GetName(), _owner->GetGUID().GetCounter(),
        uint32(_placedDecor.size()), uint32(_rooms.size()),
        uint32(_fixtures.size()), uint32(_catalog.size()),
        _interiorDecorWeightUsed, GetMaxInteriorDecorBudget(),
        _roomWeightUsed, GetMaxRoomBudget());

    return true;
}

void Housing::SaveToDB(CharacterDatabaseTransaction trans)
{
    ObjectGuid::LowType ownerGuid = _owner->GetGUID().GetCounter();

    DeleteFromDB(ownerGuid, trans);

    // Save main housing record
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING);
    stmt->setUInt64(0, ownerGuid);
    stmt->setUInt64(1, _houseGuid.GetCounter());
    stmt->setUInt64(2, _neighborhoodGuid.GetCounter());
    stmt->setUInt8(3, _plotIndex);
    stmt->setUInt32(4, _level);
    stmt->setUInt32(5, _favor);
    stmt->setUInt32(6, _settingsFlags);
    trans->Append(stmt);

    // Save placed decor
    for (auto const& [guid, decor] : _placedDecor)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        uint8 index = 0;
        stmt->setUInt64(index++, ownerGuid);
        stmt->setUInt64(index++, guid.GetCounter());
        stmt->setUInt32(index++, decor.DecorEntryId);
        stmt->setFloat(index++, decor.PosX);
        stmt->setFloat(index++, decor.PosY);
        stmt->setFloat(index++, decor.PosZ);
        stmt->setFloat(index++, decor.RotationX);
        stmt->setFloat(index++, decor.RotationY);
        stmt->setFloat(index++, decor.RotationZ);
        stmt->setFloat(index++, decor.RotationW);
        stmt->setUInt32(index++, decor.DyeSlots[0]);
        stmt->setUInt32(index++, decor.DyeSlots[1]);
        stmt->setUInt32(index++, decor.DyeSlots[2]);
        stmt->setUInt64(index++, decor.RoomGuid.IsEmpty() ? 0 : decor.RoomGuid.GetCounter());
        trans->Append(stmt);
    }

    // Save rooms
    for (auto const& [guid, room] : _rooms)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_ROOMS);
        uint8 index = 0;
        stmt->setUInt64(index++, ownerGuid);
        stmt->setUInt64(index++, guid.GetCounter());
        stmt->setUInt32(index++, room.RoomEntryId);
        stmt->setUInt32(index++, room.SlotIndex);
        stmt->setUInt32(index++, room.Orientation);
        stmt->setBool(index++, room.Mirrored);
        stmt->setUInt32(index++, room.ThemeId);
        stmt->setUInt32(index++, room.WallpaperId);
        stmt->setUInt32(index++, room.MaterialId);
        stmt->setUInt32(index++, room.DoorTypeId);
        stmt->setUInt8(index++, room.DoorSlot);
        stmt->setUInt32(index++, room.CeilingTypeId);
        stmt->setUInt8(index++, room.CeilingSlot);
        trans->Append(stmt);
    }

    // Save fixtures
    for (auto const& [pointId, fixture] : _fixtures)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        uint8 index = 0;
        stmt->setUInt64(index++, ownerGuid);
        stmt->setUInt32(index++, fixture.FixturePointId);
        stmt->setUInt32(index++, fixture.OptionId);
        trans->Append(stmt);
    }

    // Save catalog
    for (auto const& [entryId, entry] : _catalog)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_CATALOG);
        uint8 index = 0;
        stmt->setUInt64(index++, ownerGuid);
        stmt->setUInt32(index++, entry.DecorEntryId);
        stmt->setUInt32(index++, entry.Count);
        trans->Append(stmt);
    }
}

void Housing::DeleteFromDB(ObjectGuid::LowType ownerGuid, CharacterDatabaseTransaction trans)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING);
    stmt->setUInt64(0, ownerGuid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_DECOR);
    stmt->setUInt64(0, ownerGuid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_ROOMS);
    stmt->setUInt64(0, ownerGuid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURES);
    stmt->setUInt64(0, ownerGuid);
    trans->Append(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_CATALOG);
    stmt->setUInt64(0, ownerGuid);
    trans->Append(stmt);
}

HousingResult Housing::Create(ObjectGuid neighborhoodGuid, uint8 plotIndex)
{
    if (!_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_ALREADY_EXISTS;

    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        return HOUSING_RESULT_INVALID_PLOT;

    _neighborhoodGuid = neighborhoodGuid;
    _plotIndex = plotIndex;
    _level = 1;
    _favor = 0;
    _settingsFlags = HOUSE_SETTING_ALLOW_VISITORS;
    _editorMode = HOUSING_EDITOR_MODE_NONE;

    // Generate a new house guid using the owner's low guid as a base
    _houseGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, _owner->GetGUID().GetCounter());

    TC_LOG_DEBUG("housing", "Housing::Create: Player {} (GUID {}) created house on plot {} in neighborhood {}",
        _owner->GetName(), _owner->GetGUID().GetCounter(), plotIndex, _neighborhoodGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

void Housing::Delete()
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    DeleteFromDB(_owner->GetGUID().GetCounter(), trans);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Housing::Delete: Player {} (GUID {}) deleted house {}",
        _owner->GetName(), _owner->GetGUID().GetCounter(), _houseGuid.ToString());

    _houseGuid.Clear();
    _neighborhoodGuid.Clear();
    _plotIndex = INVALID_PLOT_INDEX;
    _level = 1;
    _favor = 0;
    _settingsFlags = HOUSE_SETTING_ALLOW_VISITORS;
    _editorMode = HOUSING_EDITOR_MODE_NONE;
    _placedDecor.clear();
    _rooms.clear();
    _fixtures.clear();
    _catalog.clear();
}

HousingResult Housing::PlaceDecor(uint32 decorEntryId, float x, float y, float z,
    float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Validate coordinate sanity (reject NaN/Inf and extreme values)
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(rotX) || !std::isfinite(rotY) || !std::isfinite(rotZ) || !std::isfinite(rotW))
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    // Validate decor entry exists in the HousingMgr DB2 data
    HousingResult validationResult = sHousingMgr.ValidateDecorPlacement(decorEntryId, Position(x, y, z), _level);
    if (validationResult != HOUSING_RESULT_SUCCESS)
        return validationResult;

    // Check decor count limit based on house level
    uint32 maxDecor = GetMaxDecorCount();
    if (GetDecorCount() >= maxDecor)
        return HOUSING_RESULT_DECOR_COUNT_EXCEEDED;

    // Check WeightCost-based budget
    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    if (_interiorDecorWeightUsed + weightCost > GetMaxInteriorDecorBudget())
        return HOUSING_RESULT_DECOR_BUDGET_EXCEEDED;

    // Validate room exists if specified, and check per-room decor limit
    if (!roomGuid.IsEmpty())
    {
        auto roomItr = _rooms.find(roomGuid);
        if (roomItr == _rooms.end())
            return HOUSING_RESULT_INVALID_ROOM;

        // Enforce per-room decor limit
        uint32 roomDecorCount = 0;
        for (auto const& [guid, decor] : _placedDecor)
        {
            if (decor.RoomGuid == roomGuid)
                ++roomDecorCount;
        }
        if (roomDecorCount >= MAX_HOUSING_DECOR_PER_ROOM)
            return HOUSING_RESULT_DECOR_COUNT_EXCEEDED;
    }

    // Check catalog for available copies
    auto catalogItr = _catalog.find(decorEntryId);
    if (catalogItr == _catalog.end() || catalogItr->second.Count == 0)
        return HOUSING_RESULT_STORAGE_EMPTY;

    // Generate a new decor guid
    uint64 newDbId = GenerateDecorDbId();
    ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, newDbId);

    PlacedDecor& decor = _placedDecor[decorGuid];
    decor.Guid = decorGuid;
    decor.DecorEntryId = decorEntryId;
    decor.PosX = x;
    decor.PosY = y;
    decor.PosZ = z;
    decor.RotationX = rotX;
    decor.RotationY = rotY;
    decor.RotationZ = rotZ;
    decor.RotationW = rotW;
    decor.DyeSlots = {};
    decor.RoomGuid = roomGuid;

    // Decrement catalog count
    catalogItr->second.Count--;
    if (catalogItr->second.Count == 0)
        _catalog.erase(catalogItr);

    // Update budget tracking
    _interiorDecorWeightUsed += weightCost;

    TC_LOG_DEBUG("housing", "Housing::PlaceDecor: Player {} placed decor entry {} at ({}, {}, {}) in house {} (budget {}/{})",
        _owner->GetName(), decorEntryId, x, y, z, _houseGuid.ToString(),
        _interiorDecorWeightUsed, GetMaxInteriorDecorBudget());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::MoveDecor(ObjectGuid decorGuid, float x, float y, float z,
    float rotX, float rotY, float rotZ, float rotW)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Validate coordinate sanity
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(rotX) || !std::isfinite(rotY) || !std::isfinite(rotZ) || !std::isfinite(rotW))
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_INVALID_GUID;

    PlacedDecor& decor = itr->second;
    decor.PosX = x;
    decor.PosY = y;
    decor.PosZ = z;
    decor.RotationX = rotX;
    decor.RotationY = rotY;
    decor.RotationZ = rotZ;
    decor.RotationW = rotW;

    TC_LOG_DEBUG("housing", "Housing::MoveDecor: Player {} moved decor {} to ({}, {}, {}) in house {}",
        _owner->GetName(), decorGuid.ToString(), x, y, z, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveDecor(ObjectGuid decorGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_INVALID_GUID;

    // Refund WeightCost budget
    uint32 decorEntryId = itr->second.DecorEntryId;
    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    if (_interiorDecorWeightUsed >= weightCost)
        _interiorDecorWeightUsed -= weightCost;
    else
        _interiorDecorWeightUsed = 0;

    // Return decor to catalog
    _catalog[decorEntryId].DecorEntryId = decorEntryId;
    _catalog[decorEntryId].Count++;

    _placedDecor.erase(itr);

    TC_LOG_DEBUG("housing", "Housing::RemoveDecor: Player {} removed decor {} from house {}, returned to catalog",
        _owner->GetName(), decorGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::CommitDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_INVALID_GUID;

    itr->second.DyeSlots = dyeSlots;

    TC_LOG_DEBUG("housing", "Housing::CommitDecorDyes: Player {} updated dyes on decor {} in house {}",
        _owner->GetName(), decorGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

Housing::PlacedDecor const* Housing::GetPlacedDecor(ObjectGuid decorGuid) const
{
    auto itr = _placedDecor.find(decorGuid);
    if (itr != _placedDecor.end())
        return &itr->second;

    return nullptr;
}

std::vector<Housing::PlacedDecor const*> Housing::GetAllPlacedDecor() const
{
    std::vector<PlacedDecor const*> result;
    result.reserve(_placedDecor.size());
    for (auto const& [guid, decor] : _placedDecor)
        result.push_back(&decor);
    return result;
}

HousingResult Housing::PlaceRoom(uint32 roomEntryId, uint32 slotIndex, uint32 orientation, bool mirrored)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Validate orientation (0-3 for cardinal directions)
    if (orientation > 3)
        return HOUSING_RESULT_INVALID_PLOT;

    // Check room count limit
    if (_rooms.size() >= MAX_HOUSING_ROOMS_PER_HOUSE)
        return HOUSING_RESULT_ROOM_BUDGET_EXCEEDED;

    // Check WeightCost-based room budget
    uint32 roomWeightCost = sHousingMgr.GetRoomWeightCost(roomEntryId);
    if (_roomWeightUsed + roomWeightCost > GetMaxRoomBudget())
        return HOUSING_RESULT_ROOM_BUDGET_EXCEEDED;

    // Check for slot collision
    for (auto const& [guid, room] : _rooms)
    {
        if (room.SlotIndex == slotIndex)
            return HOUSING_RESULT_INVALID_PLOT;
    }

    // Generate a new room guid
    uint64 newDbId = GenerateRoomDbId();
    ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, newDbId);

    Room& room = _rooms[roomGuid];
    room.Guid = roomGuid;
    room.RoomEntryId = roomEntryId;
    room.SlotIndex = slotIndex;
    room.Orientation = orientation;
    room.Mirrored = mirrored;
    room.ThemeId = 0;

    // Update room budget tracking
    _roomWeightUsed += roomWeightCost;

    TC_LOG_DEBUG("housing", "Housing::PlaceRoom: Player {} placed room entry {} at slot {} in house {} (room budget {}/{})",
        _owner->GetName(), roomEntryId, slotIndex, _houseGuid.ToString(),
        _roomWeightUsed, GetMaxRoomBudget());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveRoom(ObjectGuid roomGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    // Check if any placed decor references this room
    for (auto const& [guid, decor] : _placedDecor)
    {
        if (decor.RoomGuid == roomGuid)
            return HOUSING_RESULT_ROOM_IS_LEAF;
    }

    // Refund room WeightCost budget
    uint32 roomWeightCost = sHousingMgr.GetRoomWeightCost(itr->second.RoomEntryId);
    if (_roomWeightUsed >= roomWeightCost)
        _roomWeightUsed -= roomWeightCost;
    else
        _roomWeightUsed = 0;

    _rooms.erase(itr);

    TC_LOG_DEBUG("housing", "Housing::RemoveRoom: Player {} removed room {} from house {}",
        _owner->GetName(), roomGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RotateRoom(ObjectGuid roomGuid, bool clockwise)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    Room& room = itr->second;
    if (clockwise)
        room.Orientation = (room.Orientation + 1) % 4;
    else
        room.Orientation = (room.Orientation + 3) % 4; // +3 mod 4 == -1 mod 4

    TC_LOG_DEBUG("housing", "Housing::RotateRoom: Player {} rotated room {} to orientation {} in house {}",
        _owner->GetName(), roomGuid.ToString(), room.Orientation, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::MoveRoom(ObjectGuid roomGuid, uint32 newSlotIndex, ObjectGuid swapRoomGuid, uint32 /*swapSlotIndex*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    // If swapping with another room
    if (!swapRoomGuid.IsEmpty())
    {
        auto swapItr = _rooms.find(swapRoomGuid);
        if (swapItr == _rooms.end())
            return HOUSING_RESULT_INVALID_ROOM;

        // Swap slot indices
        uint32 tempSlot = itr->second.SlotIndex;
        itr->second.SlotIndex = swapItr->second.SlotIndex;
        swapItr->second.SlotIndex = tempSlot;

        TC_LOG_DEBUG("housing", "Housing::MoveRoom: Player {} swapped room {} and room {} in house {}",
            _owner->GetName(), roomGuid.ToString(), swapRoomGuid.ToString(), _houseGuid.ToString());
    }
    else
    {
        // Check that target slot is not occupied
        for (auto const& [guid, room] : _rooms)
        {
            if (guid != roomGuid && room.SlotIndex == newSlotIndex)
                return HOUSING_RESULT_INVALID_PLOT;
        }

        itr->second.SlotIndex = newSlotIndex;

        TC_LOG_DEBUG("housing", "Housing::MoveRoom: Player {} moved room {} to slot {} in house {}",
            _owner->GetName(), roomGuid.ToString(), newSlotIndex, _houseGuid.ToString());
    }

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::ApplyRoomTheme(ObjectGuid roomGuid, uint32 themeSetId, std::vector<uint32> const& /*componentIds*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    itr->second.ThemeId = themeSetId;

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomTheme: Player {} applied theme {} to room {} in house {}",
        _owner->GetName(), themeSetId, roomGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::ApplyRoomWallpaper(ObjectGuid roomGuid, uint32 wallpaperId, uint32 materialId, std::vector<uint32> const& /*componentIds*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    itr->second.WallpaperId = wallpaperId;
    itr->second.MaterialId = materialId;

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomWallpaper: Player {} applied wallpaper {} (material {}) to room {} in house {}",
        _owner->GetName(), wallpaperId, materialId, roomGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::SetDoorType(ObjectGuid roomGuid, uint32 doorTypeId, uint8 doorSlot)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    itr->second.DoorTypeId = doorTypeId;
    itr->second.DoorSlot = doorSlot;

    TC_LOG_DEBUG("housing", "Housing::SetDoorType: Player {} set door type {} (slot {}) on room {} in house {}",
        _owner->GetName(), doorTypeId, doorSlot, roomGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::SetCeilingType(ObjectGuid roomGuid, uint32 ceilingTypeId, uint8 ceilingSlot)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    itr->second.CeilingTypeId = ceilingTypeId;
    itr->second.CeilingSlot = ceilingSlot;

    TC_LOG_DEBUG("housing", "Housing::SetCeilingType: Player {} set ceiling type {} (slot {}) on room {} in house {}",
        _owner->GetName(), ceilingTypeId, ceilingSlot, roomGuid.ToString(), _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

std::vector<Housing::Room const*> Housing::GetRooms() const
{
    std::vector<Room const*> result;
    result.reserve(_rooms.size());
    for (auto const& [guid, room] : _rooms)
        result.push_back(&room);
    return result;
}

HousingResult Housing::SelectFixtureOption(uint32 fixturePointId, uint32 optionId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    if (_fixtures.size() >= MAX_HOUSING_FIXTURES_PER_HOUSE && _fixtures.find(fixturePointId) == _fixtures.end())
        return HOUSING_RESULT_FIXTURE_INVALID_DATA;

    Fixture& fixture = _fixtures[fixturePointId];
    fixture.FixturePointId = fixturePointId;
    fixture.OptionId = optionId;

    TC_LOG_DEBUG("housing", "Housing::SelectFixtureOption: Player {} set fixture point {} to option {} in house {}",
        _owner->GetName(), fixturePointId, optionId, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveFixture(uint32 fixturePointId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _fixtures.find(fixturePointId);
    if (itr == _fixtures.end())
        return HOUSING_RESULT_FIXTURE_INVALID_DATA;

    _fixtures.erase(itr);

    TC_LOG_DEBUG("housing", "Housing::RemoveFixture: Player {} removed fixture at point {} in house {}",
        _owner->GetName(), fixturePointId, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

std::vector<Housing::Fixture const*> Housing::GetFixtures() const
{
    std::vector<Fixture const*> result;
    result.reserve(_fixtures.size());
    for (auto const& [pointId, fixture] : _fixtures)
        result.push_back(&fixture);
    return result;
}

HousingResult Housing::AddToCatalog(uint32 decorEntryId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    CatalogEntry& entry = _catalog[decorEntryId];
    entry.DecorEntryId = decorEntryId;
    entry.Count++;

    TC_LOG_DEBUG("housing", "Housing::AddToCatalog: Player {} added decor entry {} to catalog (count: {}) in house {}",
        _owner->GetName(), decorEntryId, entry.Count, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveFromCatalog(uint32 decorEntryId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _catalog.find(decorEntryId);
    if (itr == _catalog.end() || itr->second.Count == 0)
        return HOUSING_RESULT_STORAGE_EMPTY;

    itr->second.Count--;
    if (itr->second.Count == 0)
        _catalog.erase(itr);

    TC_LOG_DEBUG("housing", "Housing::RemoveFromCatalog: Player {} removed one copy of decor entry {} from catalog in house {}",
        _owner->GetName(), decorEntryId, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::DestroyAllCopies(uint32 decorEntryId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _catalog.find(decorEntryId);
    if (itr == _catalog.end())
        return HOUSING_RESULT_STORAGE_EMPTY;

    uint32 destroyedCount = itr->second.Count;
    _catalog.erase(itr);

    // Also remove all placed decor of this entry
    for (auto it = _placedDecor.begin(); it != _placedDecor.end(); )
    {
        if (it->second.DecorEntryId == decorEntryId)
            it = _placedDecor.erase(it);
        else
            ++it;
    }

    TC_LOG_DEBUG("housing", "Housing::DestroyAllCopies: Player {} destroyed all copies ({}) of decor entry {} in house {}",
        _owner->GetName(), destroyedCount, decorEntryId, _houseGuid.ToString());

    return HOUSING_RESULT_SUCCESS;
}

std::vector<Housing::CatalogEntry const*> Housing::GetCatalogEntries() const
{
    std::vector<CatalogEntry const*> result;
    result.reserve(_catalog.size());
    for (auto const& [entryId, entry] : _catalog)
        result.push_back(&entry);
    return result;
}

void Housing::AddFavor(uint64 amount)
{
    _favor64 += amount;
    _favor = static_cast<uint32>(std::min<uint64>(_favor64, std::numeric_limits<uint32>::max()));

    TC_LOG_DEBUG("housing", "Housing::AddFavor: Player {} favor now {} in house {}",
        _owner->GetName(), _favor64, _houseGuid.ToString());
}

void Housing::OnQuestCompleted(uint32 questId)
{
    // QuestID-based level progression
    // Check if this quest matches the next HouseLevelData entry
    uint32 nextLevelQuestId = sHousingMgr.GetQuestForLevel(_level + 1);
    if (nextLevelQuestId > 0 && nextLevelQuestId == questId)
    {
        _level++;
        TC_LOG_DEBUG("housing", "Housing::OnQuestCompleted: Player {} house leveled up to {} (quest {}) in house {}",
            _owner->GetName(), _level, questId, _houseGuid.ToString());
    }
}

uint32 Housing::GetMaxDecorCount() const
{
    return sHousingMgr.GetMaxDecorForLevel(_level);
}

uint32 Housing::GetMaxInteriorDecorBudget() const
{
    return sHousingMgr.GetInteriorDecorBudgetForLevel(_level);
}

uint32 Housing::GetMaxExteriorDecorBudget() const
{
    return sHousingMgr.GetExteriorDecorBudgetForLevel(_level);
}

uint32 Housing::GetMaxRoomBudget() const
{
    return sHousingMgr.GetRoomBudgetForLevel(_level);
}

uint32 Housing::GetMaxFixtureBudget() const
{
    return sHousingMgr.GetFixtureBudgetForLevel(_level);
}

void Housing::RecalculateBudgets()
{
    _interiorDecorWeightUsed = 0;
    _exteriorDecorWeightUsed = 0;
    _roomWeightUsed = 0;
    _fixtureWeightUsed = 0;

    // Sum WeightCost of all placed decor
    for (auto const& [guid, decor] : _placedDecor)
    {
        uint32 weightCost = sHousingMgr.GetDecorWeightCost(decor.DecorEntryId);
        // For now, all placed decor counts as interior
        _interiorDecorWeightUsed += weightCost;
    }

    // Sum WeightCost of all placed rooms
    for (auto const& [guid, room] : _rooms)
    {
        uint32 weightCost = sHousingMgr.GetRoomWeightCost(room.RoomEntryId);
        _roomWeightUsed += weightCost;
    }

    TC_LOG_DEBUG("housing", "Housing::RecalculateBudgets: Interior decor weight: {}/{}, Room weight: {}/{}",
        _interiorDecorWeightUsed, GetMaxInteriorDecorBudget(),
        _roomWeightUsed, GetMaxRoomBudget());
}

void Housing::SaveSettings(uint32 settingsFlags)
{
    _settingsFlags = settingsFlags;

    TC_LOG_DEBUG("housing", "Housing::SaveSettings: Player {} updated house settings to {} in house {}",
        _owner->GetName(), settingsFlags, _houseGuid.ToString());
}

uint64 Housing::GenerateDecorDbId()
{
    return _decorDbIdGenerator++;
}

uint64 Housing::GenerateRoomDbId()
{
    return _roomDbIdGenerator++;
}
