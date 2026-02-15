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
#include "Account.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include <cmath>
#include <queue>
#include <unordered_set>

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
    //           0        1             2     3     4     5          6          7          8          9       10       11       12        13
    // SELECT decorGuid, decorEntryId, posX, posY, posZ, rotationX, rotationY, rotationZ, rotationW, dyeSlot0, dyeSlot1, dyeSlot2, roomGuid, locked
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
            placed.Locked = fields[13].GetUInt8() != 0;

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

    // Populate HousingStorageData::Decor UpdateField for all placed decor
    if (_owner && _owner->GetSession())
    {
        Battlenet::Account& account = _owner->GetSession()->GetBattlenetAccount();
        for (auto const& [decorGuid, decor] : _placedDecor)
            account.SetHousingDecorStorageEntry(decorGuid, _houseGuid, 0);
    }

    SyncUpdateFields();

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
        stmt->setUInt8(index++, decor.Locked ? 1 : 0);
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

void Housing::SetEditorMode(HousingEditorMode mode)
{
    _editorMode = mode;

    // Sync to Player's PlayerHouseInfoComponentData UpdateField so the client knows the mode changed
    if (_owner)
        _owner->SetHousingEditorModeUpdateField(static_cast<uint8>(mode));
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

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

void Housing::Delete()
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    DeleteFromDB(_owner->GetGUID().GetCounter(), trans);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Housing::Delete: Player {} (GUID {}) deleted house {}",
        _owner->GetName(), _owner->GetGUID().GetCounter(), _houseGuid.ToString());

    // Remove all decor storage entries from account UpdateField
    if (_owner->GetSession() && !_placedDecor.empty())
    {
        Battlenet::Account& account = _owner->GetSession()->GetBattlenetAccount();
        for (auto const& [decorGuid, decor] : _placedDecor)
            account.RemoveHousingDecorStorageEntry(decorGuid);
    }

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

    // Update account decor storage UpdateField
    if (_owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().SetHousingDecorStorageEntry(decorGuid, _houseGuid, 0);

    TC_LOG_DEBUG("housing", "Housing::PlaceDecor: Player {} placed decor entry {} at ({}, {}, {}) in house {} (budget {}/{})",
        _owner->GetName(), decorEntryId, x, y, z, _houseGuid.ToString(),
        _interiorDecorWeightUsed, GetMaxInteriorDecorBudget());

    SyncUpdateFields();
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

    if (itr->second.Locked)
        return HOUSING_RESULT_LOCK_DENIED;

    PlacedDecor& decor = itr->second;
    decor.PosX = x;
    decor.PosY = y;
    decor.PosZ = z;
    decor.RotationX = rotX;
    decor.RotationY = rotY;
    decor.RotationZ = rotZ;
    decor.RotationW = rotW;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_POSITION);
    stmt->setFloat(0, x);
    stmt->setFloat(1, y);
    stmt->setFloat(2, z);
    stmt->setFloat(3, rotX);
    stmt->setFloat(4, rotY);
    stmt->setFloat(5, rotZ);
    stmt->setFloat(6, rotW);
    stmt->setUInt64(7, _owner->GetGUID().GetCounter());
    stmt->setUInt64(8, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Housing::MoveDecor: Player {} moved decor {} to ({}, {}, {}) in house {}",
        _owner->GetName(), decorGuid.ToString(), x, y, z, _houseGuid.ToString());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveDecor(ObjectGuid decorGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_INVALID_GUID;

    if (itr->second.Locked)
        return HOUSING_RESULT_LOCK_DENIED;

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

    // Remove from account decor storage UpdateField
    if (_owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().RemoveHousingDecorStorageEntry(decorGuid);

    TC_LOG_DEBUG("housing", "Housing::RemoveDecor: Player {} removed decor {} from house {}, returned to catalog",
        _owner->GetName(), decorGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
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

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_DYES);
    stmt->setUInt32(0, dyeSlots[0]);
    stmt->setUInt32(1, dyeSlots[1]);
    stmt->setUInt32(2, dyeSlots[2]);
    stmt->setUInt64(3, _owner->GetGUID().GetCounter());
    stmt->setUInt64(4, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Housing::CommitDecorDyes: Player {} updated dyes on decor {} in house {}",
        _owner->GetName(), decorGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::SetDecorLocked(ObjectGuid decorGuid, bool locked)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_INVALID_GUID;

    itr->second.Locked = locked;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_LOCKED);
    stmt->setUInt8(0, locked ? 1 : 0);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    stmt->setUInt64(2, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Housing::SetDecorLocked: Player {} {} decor {} in house {}",
        _owner->GetName(), locked ? "locked" : "unlocked", decorGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
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

    // Validate room entry exists
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(roomEntryId);
    if (!roomData)
        return HOUSING_RESULT_INVALID_ROOM;

    // Validate orientation (0-3 for cardinal directions)
    if (orientation > 3)
        return HOUSING_RESULT_INVALID_PLOT;

    // First room placed must be a base room
    if (_rooms.empty() && !roomData->IsBaseRoom())
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // Only one base room allowed
    if (roomData->IsBaseRoom())
    {
        for (auto const& [guid, existingRoom] : _rooms)
        {
            HouseRoomData const* existingData = sHousingMgr.GetHouseRoomData(existingRoom.RoomEntryId);
            if (existingData && existingData->IsBaseRoom())
                return HOUSING_RESULT_ROOM_IS_BASE_ROOM;
        }
    }

    // Non-base rooms require at least one existing room in the house
    if (!roomData->IsBaseRoom() && _rooms.empty())
        return HOUSING_RESULT_ROOM_UNREACHABLE;

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

    // Room must have at least one doorway to connect to the house layout
    if (!roomData->IsBaseRoom())
    {
        uint32 doorCount = sHousingMgr.GetRoomDoorCount(roomEntryId);
        if (doorCount == 0)
        {
            TC_LOG_ERROR("housing", "Housing::PlaceRoom: Room entry {} has no doorways, cannot connect to house layout",
                roomEntryId);
            return HOUSING_RESULT_ROOM_UNREACHABLE;
        }
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

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveRoom(ObjectGuid roomGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    // Can't remove the base room
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(itr->second.RoomEntryId);
    if (roomData && roomData->IsBaseRoom())
        return HOUSING_RESULT_ROOM_IS_BASE_ROOM;

    // Can't remove the last room
    if (_rooms.size() <= 1)
        return HOUSING_RESULT_ROOM_LAST;

    // Check if any placed decor references this room
    for (auto const& [guid, decor] : _placedDecor)
    {
        if (decor.RoomGuid == roomGuid)
            return HOUSING_RESULT_ROOM_NOT_LEAF;
    }

    // Verify remaining rooms stay connected after removal (BFS from base room)
    if (!IsRoomGraphConnectedWithout(roomGuid))
        return HOUSING_RESULT_ROOM_UNREACHABLE;

    // Refund room WeightCost budget
    uint32 roomWeightCost = sHousingMgr.GetRoomWeightCost(itr->second.RoomEntryId);
    if (_roomWeightUsed >= roomWeightCost)
        _roomWeightUsed -= roomWeightCost;
    else
        _roomWeightUsed = 0;

    _rooms.erase(itr);

    TC_LOG_DEBUG("housing", "Housing::RemoveRoom: Player {} removed room {} from house {}",
        _owner->GetName(), roomGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
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

    PersistRoomToDB(roomGuid, room);

    TC_LOG_DEBUG("housing", "Housing::RotateRoom: Player {} rotated room {} to orientation {} in house {}",
        _owner->GetName(), roomGuid.ToString(), room.Orientation, _houseGuid.ToString());

    SyncUpdateFields();
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

        PersistRoomToDB(roomGuid, itr->second);
        PersistRoomToDB(swapRoomGuid, swapItr->second);

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

        PersistRoomToDB(roomGuid, itr->second);

        TC_LOG_DEBUG("housing", "Housing::MoveRoom: Player {} moved room {} to slot {} in house {}",
            _owner->GetName(), roomGuid.ToString(), newSlotIndex, _houseGuid.ToString());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

ObjectGuid Housing::FindBaseRoomGuid() const
{
    for (auto const& [guid, room] : _rooms)
    {
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
        if (roomData && roomData->IsBaseRoom())
            return guid;
    }

    return ObjectGuid::Empty;
}

bool Housing::IsRoomGraphConnectedWithout(ObjectGuid excludeRoomGuid) const
{
    // If only the excluded room would remain (or nothing), the graph is trivially connected
    if (_rooms.size() <= 2)
        return true;

    // Find the base room as BFS start
    ObjectGuid baseRoomGuid = FindBaseRoomGuid();
    if (baseRoomGuid.IsEmpty() || baseRoomGuid == excludeRoomGuid)
        return false; // No base room available after exclusion

    // Build slot-to-guid map for remaining rooms (excluding the removed one)
    std::unordered_map<uint32 /*slotIndex*/, ObjectGuid> slotToRoom;
    for (auto const& [guid, room] : _rooms)
    {
        if (guid != excludeRoomGuid)
            slotToRoom[room.SlotIndex] = guid;
    }

    // BFS from the base room through adjacent slots
    // Adjacency: rooms with slot index difference of 1 are considered connected
    // This is a simplified model; the client validates geometric doorway alignment
    std::unordered_set<ObjectGuid> visited;
    std::queue<ObjectGuid> queue;

    visited.insert(baseRoomGuid);
    queue.push(baseRoomGuid);

    while (!queue.empty())
    {
        ObjectGuid currentGuid = queue.front();
        queue.pop();

        auto currentItr = _rooms.find(currentGuid);
        if (currentItr == _rooms.end())
            continue;

        uint32 currentSlot = currentItr->second.SlotIndex;

        // Check adjacent slots (slot ± 1)
        for (int32 offset : { -1, 1 })
        {
            uint32 adjacentSlot = currentSlot + offset;
            // Guard against underflow for slot 0 with offset -1
            if (offset < 0 && currentSlot == 0)
                continue;

            auto adjItr = slotToRoom.find(adjacentSlot);
            if (adjItr != slotToRoom.end() && visited.find(adjItr->second) == visited.end())
            {
                visited.insert(adjItr->second);
                queue.push(adjItr->second);
            }
        }
    }

    // All remaining rooms must be reachable from the base room
    return visited.size() == slotToRoom.size();
}

HousingResult Housing::ApplyRoomTheme(ObjectGuid roomGuid, uint32 themeSetId, std::vector<uint32> const& componentIds)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    // Validate componentIds if provided
    for (uint32 componentId : componentIds)
    {
        if (!sRoomComponentStore.LookupEntry(componentId))
        {
            TC_LOG_DEBUG("housing", "Housing::ApplyRoomTheme: Invalid RoomComponent ID {} for room {}",
                componentId, roomGuid.ToString());
            return HOUSING_RESULT_INVALID_ROOM;
        }
    }

    itr->second.ThemeId = themeSetId;

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomTheme: Player {} applied theme {} to room {} ({} components) in house {}",
        _owner->GetName(), themeSetId, roomGuid.ToString(), componentIds.size(), _houseGuid.ToString());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::ApplyRoomWallpaper(ObjectGuid roomGuid, uint32 wallpaperId, uint32 materialId, std::vector<uint32> const& componentIds)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_INVALID_ROOM;

    // Validate componentIds if provided
    for (uint32 componentId : componentIds)
    {
        if (!sRoomComponentStore.LookupEntry(componentId))
        {
            TC_LOG_DEBUG("housing", "Housing::ApplyRoomWallpaper: Invalid RoomComponent ID {} for room {}",
                componentId, roomGuid.ToString());
            return HOUSING_RESULT_INVALID_ROOM;
        }
    }

    itr->second.WallpaperId = wallpaperId;
    itr->second.MaterialId = materialId;

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomWallpaper: Player {} applied wallpaper {} (material {}) to room {} ({} components) in house {}",
        _owner->GetName(), wallpaperId, materialId, roomGuid.ToString(), componentIds.size(), _houseGuid.ToString());

    SyncUpdateFields();
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

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::SetDoorType: Player {} set door type {} (slot {}) on room {} in house {}",
        _owner->GetName(), doorTypeId, doorSlot, roomGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
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

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::SetCeilingType: Player {} set ceiling type {} (slot {}) on room {} in house {}",
        _owner->GetName(), ceilingTypeId, ceilingSlot, roomGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
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

    bool isNew = _fixtures.find(fixturePointId) == _fixtures.end();
    if (isNew && _fixtures.size() >= MAX_HOUSING_FIXTURES_PER_HOUSE)
        return HOUSING_RESULT_FIXTURE_INVALID_DATA;

    // Enforce fixture budget for new fixtures (WeightCost = 1 per fixture by default)
    uint32 const fixtureWeightCost = 1;
    if (isNew)
    {
        if (_fixtureWeightUsed + fixtureWeightCost > GetMaxFixtureBudget())
            return HOUSING_RESULT_FIXTURE_BUDGET_EXCEEDED;
        _fixtureWeightUsed += fixtureWeightCost;
    }

    Fixture& fixture = _fixtures[fixturePointId];
    fixture.FixturePointId = fixturePointId;
    fixture.OptionId = optionId;

    // Immediate persist — use REPLACE semantics (delete old + insert new)
    if (!isNew)
        PersistFixtureToDB(fixturePointId, optionId);
    else
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        uint8 index = 0;
        stmt->setUInt64(index++, _owner->GetGUID().GetCounter());
        stmt->setUInt32(index++, fixturePointId);
        stmt->setUInt32(index++, optionId);
        CharacterDatabase.Execute(stmt);
    }

    TC_LOG_DEBUG("housing", "Housing::SelectFixtureOption: Player {} set fixture point {} to option {} in house {} (budget: {}/{})",
        _owner->GetName(), fixturePointId, optionId, _houseGuid.ToString(),
        _fixtureWeightUsed, GetMaxFixtureBudget());

    SyncUpdateFields();
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

    // Immediate persist — delete single fixture from DB
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
        stmt->setUInt64(0, _owner->GetGUID().GetCounter());
        stmt->setUInt32(1, fixturePointId);
        CharacterDatabase.Execute(stmt);
    }

    // Refund fixture budget
    uint32 const fixtureWeightCost = 1;
    if (_fixtureWeightUsed >= fixtureWeightCost)
        _fixtureWeightUsed -= fixtureWeightCost;

    TC_LOG_DEBUG("housing", "Housing::RemoveFixture: Player {} removed fixture at point {} in house {} (budget: {}/{})",
        _owner->GetName(), fixturePointId, _houseGuid.ToString(),
        _fixtureWeightUsed, GetMaxFixtureBudget());

    SyncUpdateFields();
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

    SyncUpdateFields();
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

    SyncUpdateFields();
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

    // Also remove all placed decor of this entry and their storage entries
    std::vector<ObjectGuid> removedGuids;
    for (auto it = _placedDecor.begin(); it != _placedDecor.end(); )
    {
        if (it->second.DecorEntryId == decorEntryId)
        {
            removedGuids.push_back(it->first);
            it = _placedDecor.erase(it);
        }
        else
            ++it;
    }

    // Remove from account decor storage UpdateField
    if (_owner->GetSession() && !removedGuids.empty())
    {
        Battlenet::Account& account = _owner->GetSession()->GetBattlenetAccount();
        for (ObjectGuid const& guid : removedGuids)
            account.RemoveHousingDecorStorageEntry(guid);
    }

    TC_LOG_DEBUG("housing", "Housing::DestroyAllCopies: Player {} destroyed all copies ({}) of decor entry {} in house {}",
        _owner->GetName(), destroyedCount, decorEntryId, _houseGuid.ToString());

    SyncUpdateFields();
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

    SyncUpdateFields();

    // Broadcast level/favor update to the owner
    if (_owner && _owner->GetSession())
    {
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor favorUpdate;
        favorUpdate.HouseGuid = _houseGuid;
        favorUpdate.Level = _level;
        favorUpdate.Favor = _favor64;
        _owner->SendDirectMessage(favorUpdate.Write());
    }
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

        SyncUpdateFields();

        // Broadcast level/favor update to the owner
        if (_owner && _owner->GetSession())
        {
            WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelUpdate;
            levelUpdate.HouseGuid = _houseGuid;
            levelUpdate.Level = _level;
            levelUpdate.Favor = _favor64;
            _owner->SendDirectMessage(levelUpdate.Write());
        }
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

void Housing::SyncUpdateFields()
{
    if (!_owner || !_owner->GetSession())
        return;

    Battlenet::Account& account = _owner->GetSession()->GetBattlenetAccount();
    account.SetHousingBnetAccount(_owner->GetSession()->GetBattlenetAccountGUID());
    account.SetHousingEntityGUID(_houseGuid);
    account.SetHousingPlotIndex(static_cast<int32>(_plotIndex));
    account.SetHousingLevel(_level);
    account.SetHousingFavor(_favor64);
    account.SetHousingBudgets(
        GetMaxInteriorDecorBudget() - _interiorDecorWeightUsed,
        GetMaxExteriorDecorBudget() - _exteriorDecorWeightUsed,
        GetMaxRoomBudget() - _roomWeightUsed,
        GetMaxFixtureBudget() - _fixtureWeightUsed
    );
}

void Housing::SaveSettings(uint32 settingsFlags)
{
    _settingsFlags = settingsFlags;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_SETTINGS);
    stmt->setUInt32(0, _settingsFlags);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();

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

void Housing::PersistRoomToDB(ObjectGuid roomGuid, Room const& room)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_ROOM);
    uint8 index = 0;
    stmt->setUInt32(index++, room.SlotIndex);
    stmt->setUInt32(index++, room.Orientation);
    stmt->setUInt8(index++, room.Mirrored ? 1 : 0);
    stmt->setUInt32(index++, room.ThemeId);
    stmt->setUInt32(index++, room.WallpaperId);
    stmt->setUInt32(index++, room.MaterialId);
    stmt->setUInt32(index++, room.DoorTypeId);
    stmt->setUInt8(index++, room.DoorSlot);
    stmt->setUInt32(index++, room.CeilingTypeId);
    stmt->setUInt8(index++, room.CeilingSlot);
    stmt->setUInt64(index++, _owner->GetGUID().GetCounter());
    stmt->setUInt64(index++, roomGuid.GetCounter());
    CharacterDatabase.Execute(stmt);
}

void Housing::PersistFixtureToDB(uint32 fixturePointId, uint32 optionId)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_FIXTURE);
    stmt->setUInt32(0, optionId);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    stmt->setUInt32(2, fixturePointId);
    CharacterDatabase.Execute(stmt);
}
