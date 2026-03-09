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
#include "HousingPlayerHouseEntity.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "HousingMgr.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "WorldSession.h"
#include <cmath>
#include <queue>
#include <unordered_set>

// Global DB ID generators — initialized from MAX(id) at server startup
std::atomic<uint64> Housing::s_nextDecorDbId{1};
std::atomic<uint64> Housing::s_nextRoomDbId{1};

Housing::Housing(Player* owner)
    : _owner(owner)
    , _plotIndex(INVALID_PLOT_INDEX)
    , _level(1)
    , _favor(0)
    , _settingsFlags(HOUSE_SETTING_DEFAULT)
    , _editorMode(HOUSING_EDITOR_MODE_NONE)
{
}

void Housing::InitializeDbIdGenerators()
{
    // Initialize global ID generators from current MAX(id) in the database.
    // Must be called during server startup before any Housing objects are loaded.
    {
        QueryResult result = CharacterDatabase.Query("SELECT COALESCE(MAX(id), 0) FROM character_housing_decor");
        uint64 maxDecorId = result ? (*result)[0].GetUInt64() : 0;
        s_nextDecorDbId.store(maxDecorId + 1);
        TC_LOG_INFO("housing", "Housing::InitializeDbIdGenerators: Decor ID generator starting at {} (MAX in DB: {})",
            maxDecorId + 1, maxDecorId);
    }
    {
        QueryResult result = CharacterDatabase.Query("SELECT COALESCE(MAX(id), 0) FROM character_housing_rooms");
        uint64 maxRoomId = result ? (*result)[0].GetUInt64() : 0;
        s_nextRoomDbId.store(maxRoomId + 1);
        TC_LOG_INFO("housing", "Housing::InitializeDbIdGenerators: Room ID generator starting at {} (MAX in DB: {})",
            maxRoomId + 1, maxRoomId);
    }
}

bool Housing::LoadFromDB(PreparedQueryResult housing, PreparedQueryResult decor,
    PreparedQueryResult rooms, PreparedQueryResult fixtures, PreparedQueryResult catalog)
{
    if (!housing)
        return false;

    Field* fields = housing->Fetch();
    // Expected columns: houseGuid, neighborhoodGuid, plotIndex, level, favor, settingsFlags, exteriorLocked, houseSize, houseType
    _houseGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 3, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 7, fields[0].GetUInt64());
    _neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, fields[1].GetUInt64());
    _plotIndex = fields[2].GetUInt8();
    _level = fields[3].GetUInt32();
    _favor = fields[4].GetUInt32();
    _settingsFlags = fields[5].GetUInt32();
    _exteriorLocked = fields[6].GetUInt8() != 0;
    _houseSize = fields[7].GetUInt8();
    _houseType = fields[8].GetUInt32();
    _createTime = fields[9].GetUInt32();

    TC_LOG_ERROR("housing", "Housing::LoadFromDB: Loaded house HouseGuid={} NeighborhoodGuid={} PlotIndex={} Level={} HouseType={} for player {}",
        _houseGuid.ToString(), _neighborhoodGuid.ToString(), _plotIndex, _level, _houseType, _owner->GetGUID().ToString());
    _housePosX = fields[10].GetFloat();
    _housePosY = fields[11].GetFloat();
    _housePosZ = fields[12].GetFloat();
    _houseFacing = fields[13].GetFloat();
    _hasCustomPosition = (_housePosX != 0.0f || _housePosY != 0.0f || _housePosZ != 0.0f);
    _houseName = fields[14].GetString();
    _houseDescription = fields[15].GetString();

    // Load placed decor
    //           0        1             2     3     4     5          6          7          8          9       10       11       12        13     14              15          16
    // SELECT decorGuid, decorEntryId, posX, posY, posZ, rotationX, rotationY, rotationZ, rotationW, dyeSlot0, dyeSlot1, dyeSlot2, roomGuid, locked, placementTime, sourceType, sourceValue
    // FROM character_housing_decor WHERE ownerGuid = ?
    if (decor)
    {
        do
        {
            fields = decor->Fetch();

            uint64 decorDbId = fields[0].GetUInt64();
            uint32 decorEntryId = fields[1].GetUInt32();
            // Reconstruct the full Housing GUID with subType=1 + decorEntryId.
            // The client uses subType=1 GUIDs (Housing-1-realmId-entryId-counter) for decor;
            // storing only the counter and recreating with subType=0 produces a GUID mismatch
            // that makes the Account entity storage stale after relog.
            ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(
                /*subType*/ 1,
                /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
                /*arg2*/ decorEntryId,
                decorDbId);

            PlacedDecor& placed = _placedDecor[decorGuid];
            placed.Guid = decorGuid;
            placed.DecorEntryId = decorEntryId;
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
                placed.RoomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, 0, 0, roomDbId);
            placed.Locked = fields[13].GetUInt8() != 0;
            placed.PlacementTime = static_cast<time_t>(fields[14].GetUInt64());
            placed.SourceType = fields[15].GetUInt8();
            placed.SourceValue = fields[16].GetString();

            // Advance global generator if this loaded ID is at or above current value
            // (safety net in case InitializeDbIdGenerators ran before data was loaded)
            uint64 expected = s_nextDecorDbId.load();
            while (decorDbId >= expected && !s_nextDecorDbId.compare_exchange_weak(expected, decorDbId + 1))
                ;

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
            uint32 roomEntryId = fields[1].GetUInt32();

            // Fix up roomDbId=0 from old saves that used ObjectGuid::Empty (subType=0 produced Empty GUID).
            // Without this, all rooms get the same GUID key and overwrite each other in _rooms.
            if (roomDbId == 0)
                roomDbId = GenerateRoomDbId();

            ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, 0, 0, roomDbId);

            Room& room = _rooms[roomGuid];
            room.Guid = roomGuid;
            room.RoomEntryId = roomEntryId;
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

            // Advance global generator if needed (safety net)
            uint64 expected = s_nextRoomDbId.load();
            while (roomDbId >= expected && !s_nextRoomDbId.compare_exchange_weak(expected, roomDbId + 1))
                ;

        } while (rooms->NextRow());
    }

    // Runtime fixup: ensure entry hall room (46) + correct visual room exist.
    // Due to the old subType=0 GUID bug, both rooms shared ObjectGuid::Empty
    // as their key and only the last one survived in the DB. Fix that first.
    {
        // Step 0: Migrate exterior geobox room (18) to entry hall room (46) in existing houses.
        // Room 18 was incorrectly used as the interior base room. It should only be used
        // for the exterior plot geobox (handled by SpawnRoomForPlot independently).
        uint32 entryHallEntry = sHousingMgr.GetEntryHallRoomEntryId();
        uint32 extGeoboxEntry = sHousingMgr.GetBaseRoomEntryId();
        if (entryHallEntry != extGeoboxEntry)
        {
            for (auto& [guid, room] : _rooms)
            {
                if (room.RoomEntryId == extGeoboxEntry)
                {
                    TC_LOG_ERROR("housing", "Housing::LoadFromDB: Migrating interior base room {} -> {} "
                        "in slot {} for house {} (entry hall fixup)",
                        extGeoboxEntry, entryHallEntry, room.SlotIndex, _houseGuid.ToString());
                    room.RoomEntryId = entryHallEntry;
                    break;
                }
            }
        }

        // Step 1: Ensure base room (entry hall) exists
        bool hasBaseRoom = false;
        for (auto const& [guid, room] : _rooms)
        {
            if (sHousingMgr.IsBaseRoom(room.RoomEntryId))
            {
                hasBaseRoom = true;
                break;
            }
        }

        if (!hasBaseRoom)
        {
            uint32 entryHallRoomEntry = sHousingMgr.GetEntryHallRoomEntryId();
            HousingResult baseResult = PlaceRoom(entryHallRoomEntry, /*slotIndex*/ 0, /*orientation*/ 0, /*mirrored*/ false);
            TC_LOG_ERROR("housing", "Housing::LoadFromDB: Auto-placed entry hall room (entry {}) in slot 0 "
                "for house {} (migration fixup, result={})",
                entryHallRoomEntry, _houseGuid.ToString(), baseResult);
        }

        // Step 2: Ensure correct visual room exists
        uint32 correctVisualRoom = sHousingMgr.GetDefaultVisualRoomEntry();
        bool hasVisualRoom = false;
        ObjectGuid wrongRoomGuid;

        for (auto const& [guid, room] : _rooms)
        {
            if (sHousingMgr.IsBaseRoom(room.RoomEntryId))
                continue;

            if (room.RoomEntryId == correctVisualRoom)
            {
                hasVisualRoom = true;
                break;
            }

            // If not the correct one and it's the only non-base room, replace it
            if (wrongRoomGuid.IsEmpty())
                wrongRoomGuid = guid;
            else
                hasVisualRoom = true; // Multiple visual rooms — don't mess with them
        }

        // Replace wrong room with correct one
        if (!hasVisualRoom && !wrongRoomGuid.IsEmpty() && correctVisualRoom)
        {
            auto wrongItr = _rooms.find(wrongRoomGuid);
            if (wrongItr != _rooms.end())
            {
                uint32 oldEntry = wrongItr->second.RoomEntryId;
                uint32 oldSlot = wrongItr->second.SlotIndex;

                // Erase from map — SaveToDB will persist the change on next save
                _rooms.erase(wrongItr);

                // Place new room in same slot
                HousingResult placeResult = PlaceRoom(correctVisualRoom, oldSlot, 0, false);
                TC_LOG_ERROR("housing", "Housing::LoadFromDB: Replaced visual room {} with {} in slot {} "
                    "for house {} (migration fixup, result={})",
                    oldEntry, correctVisualRoom, oldSlot, _houseGuid.ToString(), placeResult);
            }
        }

        // No visual room at all — add one
        if (!hasVisualRoom && wrongRoomGuid.IsEmpty() && correctVisualRoom)
        {
            // Find the next free slot (slot 0 is base room)
            uint32 nextSlot = 1;
            for (auto const& [guid, room] : _rooms)
            {
                if (room.SlotIndex >= nextSlot)
                    nextSlot = room.SlotIndex + 1;
            }

            HousingResult placeResult = PlaceRoom(correctVisualRoom, nextSlot, /*orientation*/ 0, /*mirrored*/ false);
            TC_LOG_ERROR("housing", "Housing::LoadFromDB: Auto-placed visual room entry {} in slot {} "
                "for house {} (migration fixup, result={})",
                correctVisualRoom, nextSlot, _houseGuid.ToString(), placeResult);
        }
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
    //           0             1        2           3
    // SELECT houseDecorId, quantity, sourceType, sourceValue
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
            entry.SourceType = fields[2].GetUInt8();
            entry.SourceValue = fields[3].GetString();

        } while (catalog->NextRow());
    }

    // Fixup: if house exists but catalog is empty, populate with starter decor.
    // This handles houses created before the catalog-population fix was added.
    if (_catalog.empty() && !_houseGuid.IsEmpty() && _owner)
    {
        auto starterDecorWithQty = sHousingMgr.GetStarterDecorWithQuantities(_owner->GetTeam());
        if (!starterDecorWithQty.empty())
        {
            for (auto const& [decorId, qty] : starterDecorWithQty)
            {
                CatalogEntry& entry = _catalog[decorId];
                entry.DecorEntryId = decorId;
                entry.Count = qty;
            }
            TC_LOG_ERROR("housing", "Housing::LoadFromDB: Catalog was empty for house {} — auto-populated {} starter decor types for player {}",
                _houseGuid.ToString(), uint32(starterDecorWithQty.size()), _owner->GetGUID().ToString());

            // Persist the fixup to DB so it only happens once
            if (_owner->GetSession())
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                for (auto const& [entryId, entry] : _catalog)
                {
                    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_CATALOG);
                    uint8 idx = 0;
                    stmt->setUInt64(idx++, _owner->GetGUID().GetCounter());
                    stmt->setUInt32(idx++, entry.DecorEntryId);
                    stmt->setUInt32(idx++, entry.Count);
                    stmt->setUInt8(idx++, entry.SourceType);
                    stmt->setString(idx++, entry.SourceValue);
                    trans->Append(stmt);
                }
                CharacterDatabase.CommitTransaction(trans);
            }
        }
    }

    // Auto-place starter decor for existing houses with catalog but no placed decor.
    // This handles houses created before the starter decor placement was added.
    if (_placedDecor.empty() && !_catalog.empty() && !_houseGuid.IsEmpty() && _owner)
    {
        uint32 placed = PlaceStarterDecor();
        if (placed > 0)
            TC_LOG_ERROR("housing", "Housing::LoadFromDB: Auto-placed {} starter decor items for house {} (migration fixup)",
                placed, _houseGuid.ToString());
    }

    // Recalculate budget weights from loaded data
    RecalculateBudgets();

    // NOTE: FHousingStorage_C is NOT populated at login — retail flow confirms it is only sent
    // when the player enters edit mode or sends REQUEST_STORAGE. Populating it at login causes
    // client crashes (BLZ_ALLOC for HouseDecorGUID) because the client doesn't expect storage
    // data in the initial Account entity CREATE. Storage entries (both placed and catalog) are
    // populated on-demand by PopulateCatalogStorageEntries() called from REQUEST_STORAGE handler.

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
    stmt->setUInt8(7, _exteriorLocked ? 1 : 0);
    stmt->setUInt8(8, _houseSize);
    stmt->setUInt32(9, _houseType);
    stmt->setFloat(10, _housePosX);
    stmt->setFloat(11, _housePosY);
    stmt->setFloat(12, _housePosZ);
    stmt->setFloat(13, _houseFacing);
    stmt->setString(14, _houseName);
    stmt->setString(15, _houseDescription);
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
        stmt->setUInt64(index++, static_cast<uint64>(decor.PlacementTime));
        stmt->setUInt8(index++, decor.SourceType);
        stmt->setString(index++, decor.SourceValue);
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
        stmt->setUInt8(index++, entry.SourceType);
        stmt->setString(index++, entry.SourceValue);
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

    // Sniff-verified: retail sends EditorMode via UPDATE_OBJECT alongside
    // UNIT_FLAG_PACIFIED, UNIT_FLAG2_NO_ACTIONS and SilencedSchoolMask=127.
    // The client reads EditorMode from PlayerHouseInfoComponentData to set
    // the internal editor state (ClientHousingDecorSystem +329) which gates
    // ClickTarget (flag 16) for decor selection.
    if (_owner)
        _owner->SetHousingEditorModeUpdateField(static_cast<uint8>(mode));
}

HousingResult Housing::Create(ObjectGuid neighborhoodGuid, uint8 plotIndex)
{
    if (!_houseGuid.IsEmpty())
        return HOUSING_RESULT_INVALID_HOUSE;

    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        return HOUSING_RESULT_PLOT_NOT_FOUND;

    _neighborhoodGuid = neighborhoodGuid;
    _plotIndex = plotIndex;
    _level = 1;
    _favor = 0;
    _settingsFlags = HOUSE_SETTING_DEFAULT;
    _editorMode = HOUSING_EDITOR_MODE_NONE;
    _exteriorLocked = false;
    _houseSize = HOUSING_FIXTURE_SIZE_SMALL;
    // Sniff-verified WmoDataIDs: 9=Alliance, 87=Horde
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (neighborhood && neighborhood->GetFactionRestriction() == NEIGHBORHOOD_FACTION_HORDE)
        _houseType = 87;
    else
        _houseType = 9;
    _createTime = static_cast<uint32>(GameTime::GetGameTime());
    _hasCustomPosition = false;
    _housePosX = _housePosY = _housePosZ = _houseFacing = 0.0f;

    // Generate a new house guid using BNetAccountId as the counter.
    // Retail-verified: HouseGUID.Low always equals BNetAccountGUID.Low (the BNet account ID).
    // Using player GUID counter produces small values that don't match the retail pattern
    // and may cause the client's AABB/DB2 lookup to fail during decor placement bounds checks.
    uint32 bnetAccountId = _owner->GetSession() ? _owner->GetSession()->GetBattlenetAccountId() : 0;
    if (bnetAccountId == 0)
    {
        TC_LOG_ERROR("housing", "Housing::Create: BNetAccountId is 0 for player {} — falling back to player GUID counter",
            _owner->GetGUID().ToString());
        bnetAccountId = static_cast<uint32>(_owner->GetGUID().GetCounter());
    }
    _houseGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 3, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 7, uint64(bnetAccountId));

    TC_LOG_ERROR("housing", "Housing::Create: Player {} (BNetAcct {}) created house on plot {} in neighborhood {} — HouseGuid={}",
        _owner->GetName(), bnetAccountId, plotIndex, _neighborhoodGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();

    // Every new house starts with an entry hall room (interior base room).
    // Room 18 is the EXTERIOR geobox only (handled by SpawnRoomForPlot independently).
    // Room 46 is the proper interior entry hall (sniff-verified: BASE_ROOM flag, door to visual room).
    PlaceRoom(sHousingMgr.GetEntryHallRoomEntryId(), /*slotIndex*/ 0, /*orientation*/ 0, /*mirrored*/ false);

    // Also place a default visual room so the interior renders walls/floor/ceiling.
    // Base room (18) only provides the geobox boundary — visual geometry needs a separate room.
    uint32 visualRoom = sHousingMgr.GetDefaultVisualRoomEntry();
    if (visualRoom)
    {
        HousingResult visualResult = PlaceRoom(visualRoom, /*slotIndex*/ 1, /*orientation*/ 0, /*mirrored*/ false);
        if (visualResult == HOUSING_RESULT_SUCCESS)
        {
            TC_LOG_ERROR("housing", "Housing::Create: Auto-placed visual room entry {} in slot 1 for player {}",
                visualRoom, _owner->GetName());
        }
        else
        {
            TC_LOG_ERROR("housing", "Housing::Create: PlaceRoom FAILED for visual room entry {} — result={} — "
                "interior will be empty for player {}",
                visualRoom, visualResult, _owner->GetName());
        }
    }
    else
    {
        TC_LOG_ERROR("housing", "Housing::Create: No visual room entry found — interior will be empty for player {}",
            _owner->GetName());
    }

    return HOUSING_RESULT_SUCCESS;
}

ObjectGuid Housing::GetPlotGuid() const
{
    // Deterministic PlotGUID: subType=2 encodes neighborhood + plot index
    return ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 2,
        /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ _plotIndex,
        _neighborhoodGuid.GetCounter());
}

void Housing::Delete()
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    DeleteFromDB(_owner->GetGUID().GetCounter(), trans);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Housing::Delete: Player {} (GUID {}) deleted house {}",
        _owner->GetName(), _owner->GetGUID().GetCounter(), _houseGuid.ToString());

    // Remove all decor storage entries from account UpdateField (only if storage is populated)
    if (_storagePopulated && _owner->GetSession() && !_placedDecor.empty())
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
    _settingsFlags = HOUSE_SETTING_DEFAULT;
    _editorMode = HOUSING_EDITOR_MODE_NONE;
    _exteriorLocked = false;
    _houseSize = HOUSING_FIXTURE_SIZE_SMALL;
    _houseType = 0;
    _hasCustomPosition = false;
    _housePosX = _housePosY = _housePosZ = _houseFacing = 0.0f;
    _placedDecor.clear();
    _rooms.clear();
    _fixtures.clear();
    _catalog.clear();
}

ObjectGuid Housing::StartPlacingNewDecor(uint32 catalogEntryId, HousingResult& result)
{
    if (_houseGuid.IsEmpty())
    {
        result = HOUSING_RESULT_HOUSE_NOT_FOUND;
        return ObjectGuid::Empty;
    }

    // Validate entry exists in catalog
    auto catalogItr = _catalog.find(catalogEntryId);
    if (catalogItr == _catalog.end() || catalogItr->second.Count == 0)
    {
        result = HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;
        return ObjectGuid::Empty;
    }

    // Check decor count limit
    uint32 maxDecor = GetMaxDecorCount();
    if (GetDecorCount() >= maxDecor)
    {
        result = HOUSING_RESULT_MAX_DECOR_REACHED;
        return ObjectGuid::Empty;
    }

    // Generate a GUID for this pending placement.
    // Must use subType=1 (decor GUID format) — subType=0 returns ObjectGuid::Empty!
    uint64 newDbId = GenerateDecorDbId();
    ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 1, /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ catalogEntryId, newDbId);

    _pendingPlacements[decorGuid] = catalogEntryId;

    result = HOUSING_RESULT_SUCCESS;
    TC_LOG_DEBUG("housing", "Housing::StartPlacingNewDecor: Created pending placement {} for entry {} (catalog count: {})",
        decorGuid.ToString(), catalogEntryId, catalogItr->second.Count);
    return decorGuid;
}

uint32 Housing::GetPendingPlacementEntryId(ObjectGuid decorGuid) const
{
    auto itr = _pendingPlacements.find(decorGuid);
    return itr != _pendingPlacements.end() ? itr->second : 0;
}

void Housing::CancelPendingPlacement(ObjectGuid decorGuid)
{
    _pendingPlacements.erase(decorGuid);
}

HousingResult Housing::PlaceDecorWithGuid(ObjectGuid decorGuid, uint32 decorEntryId, float x, float y, float z,
    float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(rotX) || !std::isfinite(rotY) || !std::isfinite(rotZ) || !std::isfinite(rotW))
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    HousingResult validationResult = sHousingMgr.ValidateDecorPlacement(decorEntryId, Position(x, y, z), _level);
    if (validationResult != HOUSING_RESULT_SUCCESS)
        return validationResult;

    uint32 maxDecor = GetMaxDecorCount();
    if (GetDecorCount() >= maxDecor)
        return HOUSING_RESULT_MAX_DECOR_REACHED;

    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    if (roomGuid.IsEmpty())
    {
        if (_exteriorDecorWeightUsed + weightCost > GetMaxExteriorDecorBudget())
            return HOUSING_RESULT_MAX_DECOR_REACHED;
    }
    else
    {
        if (_interiorDecorWeightUsed + weightCost > GetMaxInteriorDecorBudget())
            return HOUSING_RESULT_MAX_DECOR_REACHED;
    }

    if (!roomGuid.IsEmpty())
    {
        auto roomItr = _rooms.find(roomGuid);
        if (roomItr == _rooms.end())
            return HOUSING_RESULT_ROOM_NOT_FOUND;

        uint32 roomDecorCount = 0;
        for (auto const& [guid, decor] : _placedDecor)
        {
            if (decor.RoomGuid == roomGuid)
                ++roomDecorCount;
        }
        if (roomDecorCount >= MAX_HOUSING_DECOR_PER_ROOM)
            return HOUSING_RESULT_MAX_DECOR_REACHED;
    }

    auto catalogItr = _catalog.find(decorEntryId);
    if (catalogItr == _catalog.end() || catalogItr->second.Count == 0)
        return HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;

    // Remove from pending placements
    _pendingPlacements.erase(decorGuid);

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
    decor.PlacementTime = GameTime::GetGameTime();
    // Inherit acquisition source from catalog entry
    decor.SourceType = catalogItr->second.SourceType;
    decor.SourceValue = catalogItr->second.SourceValue;

    catalogItr->second.Count--;
    if (catalogItr->second.Count == 0)
        _catalog.erase(catalogItr);

    if (roomGuid.IsEmpty())
        _exteriorDecorWeightUsed += weightCost;
    else
        _interiorDecorWeightUsed += weightCost;

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        uint8 index = 0;
        stmt->setUInt64(index++, _owner->GetGUID().GetCounter());
        stmt->setUInt64(index++, decorGuid.GetCounter());
        stmt->setUInt32(index++, decorEntryId);
        stmt->setFloat(index++, x);
        stmt->setFloat(index++, y);
        stmt->setFloat(index++, z);
        stmt->setFloat(index++, rotX);
        stmt->setFloat(index++, rotY);
        stmt->setFloat(index++, rotZ);
        stmt->setFloat(index++, rotW);
        stmt->setUInt32(index++, 0);
        stmt->setUInt32(index++, 0);
        stmt->setUInt32(index++, 0);
        stmt->setUInt64(index++, roomGuid.IsEmpty() ? 0 : roomGuid.GetCounter());
        stmt->setUInt8(index++, 0);
        stmt->setUInt64(index++, static_cast<uint64>(decor.PlacementTime));
        stmt->setUInt8(index++, decor.SourceType);
        stmt->setString(index++, decor.SourceValue);
        CharacterDatabase.Execute(stmt);
    }

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        auto catItr = _catalog.find(decorEntryId);
        stmt->setUInt32(0, catItr != _catalog.end() ? catItr->second.Count : 0);
        stmt->setUInt64(1, _owner->GetGUID().GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    if (_owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().SetHousingDecorStorageEntry(decorGuid, _houseGuid, decor.SourceType, decor.SourceValue);

    TC_LOG_DEBUG("housing", "Housing::PlaceDecorWithGuid: Player {} placed decor entry {} (GUID: {}) at ({}, {}, {}) in house {}",
        _owner->GetName(), decorEntryId, decorGuid.ToString(), x, y, z, _houseGuid.ToString());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
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
        return HOUSING_RESULT_MAX_DECOR_REACHED;

    // Check WeightCost-based budget (exterior vs interior)
    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    if (roomGuid.IsEmpty())
    {
        // Outdoor decor uses exterior budget
        if (_exteriorDecorWeightUsed + weightCost > GetMaxExteriorDecorBudget())
            return HOUSING_RESULT_MAX_DECOR_REACHED;
    }
    else
    {
        // Indoor decor uses interior budget
        if (_interiorDecorWeightUsed + weightCost > GetMaxInteriorDecorBudget())
            return HOUSING_RESULT_MAX_DECOR_REACHED;
    }

    // Validate room exists if specified, and check per-room decor limit
    if (!roomGuid.IsEmpty())
    {
        auto roomItr = _rooms.find(roomGuid);
        if (roomItr == _rooms.end())
            return HOUSING_RESULT_ROOM_NOT_FOUND;

        // Enforce per-room decor limit
        uint32 roomDecorCount = 0;
        for (auto const& [guid, decor] : _placedDecor)
        {
            if (decor.RoomGuid == roomGuid)
                ++roomDecorCount;
        }
        if (roomDecorCount >= MAX_HOUSING_DECOR_PER_ROOM)
            return HOUSING_RESULT_MAX_DECOR_REACHED;
    }

    // Check catalog for available copies
    auto catalogItr = _catalog.find(decorEntryId);
    if (catalogItr == _catalog.end() || catalogItr->second.Count == 0)
        return HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;

    // Generate a new decor guid.
    // Must use subType=1 (decor GUID format) — subType=0 returns ObjectGuid::Empty!
    uint64 newDbId = GenerateDecorDbId();
    ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 1, /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ decorEntryId, newDbId);

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
    decor.PlacementTime = GameTime::GetGameTime();
    // Inherit acquisition source from catalog entry
    decor.SourceType = catalogItr->second.SourceType;
    decor.SourceValue = catalogItr->second.SourceValue;

    // Decrement catalog count
    catalogItr->second.Count--;
    if (catalogItr->second.Count == 0)
        _catalog.erase(catalogItr);

    // Update budget tracking (route to correct budget based on room)
    if (roomGuid.IsEmpty())
        _exteriorDecorWeightUsed += weightCost;
    else
        _interiorDecorWeightUsed += weightCost;

    // Immediate persist for crash safety
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        uint8 index = 0;
        stmt->setUInt64(index++, _owner->GetGUID().GetCounter());
        stmt->setUInt64(index++, decorGuid.GetCounter());
        stmt->setUInt32(index++, decorEntryId);
        stmt->setFloat(index++, x);
        stmt->setFloat(index++, y);
        stmt->setFloat(index++, z);
        stmt->setFloat(index++, rotX);
        stmt->setFloat(index++, rotY);
        stmt->setFloat(index++, rotZ);
        stmt->setFloat(index++, rotW);
        stmt->setUInt32(index++, 0); // dyeSlot0
        stmt->setUInt32(index++, 0); // dyeSlot1
        stmt->setUInt32(index++, 0); // dyeSlot2
        stmt->setUInt64(index++, roomGuid.IsEmpty() ? 0 : roomGuid.GetCounter());
        stmt->setUInt8(index++, 0);  // locked
        stmt->setUInt64(index++, static_cast<uint64>(decor.PlacementTime));
        stmt->setUInt8(index++, decor.SourceType);
        stmt->setString(index++, decor.SourceValue);
        CharacterDatabase.Execute(stmt);
    }

    // Also persist updated catalog count
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        auto catItr = _catalog.find(decorEntryId);
        stmt->setUInt32(0, catItr != _catalog.end() ? catItr->second.Count : 0);
        stmt->setUInt64(1, _owner->GetGUID().GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    // Update account decor storage UpdateField (only if storage is populated — not during LoadFromDB)
    if (_storagePopulated && _owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().SetHousingDecorStorageEntry(decorGuid, _houseGuid, decor.SourceType, decor.SourceValue);

    TC_LOG_DEBUG("housing", "Housing::PlaceDecor: Player {} placed decor entry {} at ({}, {}, {}) in house {} (interior {}/{}, exterior {}/{})",
        _owner->GetName(), decorEntryId, x, y, z, _houseGuid.ToString(),
        _interiorDecorWeightUsed, GetMaxInteriorDecorBudget(),
        _exteriorDecorWeightUsed, GetMaxExteriorDecorBudget());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

uint32 Housing::PlaceStarterDecor()
{
    if (_houseGuid.IsEmpty() || !_owner)
        return 0;

    // Don't place if decor already exists (house already has items)
    if (!_placedDecor.empty())
        return 0;

    // Find the visual room (non-base room, typically Room 1)
    ObjectGuid visualRoomGuid;
    for (auto const& [guid, room] : _rooms)
    {
        if (!sHousingMgr.IsBaseRoom(room.RoomEntryId))
        {
            visualRoomGuid = guid;
            break;
        }
    }

    if (visualRoomGuid.IsEmpty())
    {
        TC_LOG_ERROR("housing", "Housing::PlaceStarterDecor: No visual room found for house {} — cannot place starter decor",
            _houseGuid.ToString());
        return 0;
    }

    // Sniff-verified starter decor positions (room-local coordinates in the visual room).
    // Both factions use the same Room 1 geometry — only the DecorEntryIDs differ.
    // Positions from horde_housing sniff: painting on wall, table on floor, chandelier on ceiling,
    // 2nd painting on opposite wall, fireplace against wall.
    struct StarterDecorPlacement
    {
        uint32 DecorEntryId;
        float X, Y, Z;
        float RotX, RotY, RotZ, RotW;
    };

    std::vector<StarterDecorPlacement> placements;
    uint32 teamId = _owner->GetTeam();

    if (teamId == HORDE)
    {
        // Horde starter decor (sniff-verified positions in Room 1)
        placements = {
            { 1700, 11.458f,  7.588f, 2.984f, 0.0f, 0.0f, -0.9999962f, 0.0027621f },  // painting
            { 2549,  9.844f, -8.013f, 0.020f, 0.0f, 0.0f,  0.9914417f, 0.1305500f },  // table
            { 8910,  6.836f, -5.971f, 8.137f, 0.0f, 0.0f, -0.9999962f, 0.0027621f },  // chandelier
            { 1700, -7.528f,-11.480f, 3.029f, 0.0f, 0.0f,  0.7071018f, 0.7071118f },  // painting 2
            {   81,  0.074f, 10.788f, 0.020f, 0.0f, 0.0f, -0.7071047f, 0.7071089f },  // fireplace
        };
    }
    else
    {
        // Alliance starter decor — same room geometry, faction-specific items.
        // Using equivalent positions (wall art, table, ceiling fixture, wall art, hearth).
        placements = {
            {  389, 11.458f,  7.588f, 2.984f, 0.0f, 0.0f, -0.9999962f, 0.0027621f },  // wall art
            {  726,  9.844f, -8.013f, 0.020f, 0.0f, 0.0f,  0.9914417f, 0.1305500f },  // table
            { 1994,  6.836f, -5.971f, 8.137f, 0.0f, 0.0f, -0.9999962f, 0.0027621f },  // ceiling
            { 1435, -7.528f,-11.480f, 3.029f, 0.0f, 0.0f,  0.7071018f, 0.7071118f },  // wall art 2
            { 9144,  0.074f, 10.788f, 0.020f, 0.0f, 0.0f, -0.7071047f, 0.7071089f },  // hearth
        };
    }

    uint32 placedCount = 0;
    for (auto const& p : placements)
    {
        // Check that decor exists in catalog before placing
        auto catalogItr = _catalog.find(p.DecorEntryId);
        if (catalogItr == _catalog.end() || catalogItr->second.Count == 0)
            continue;

        HousingResult result = PlaceDecor(p.DecorEntryId, p.X, p.Y, p.Z,
            p.RotX, p.RotY, p.RotZ, p.RotW, visualRoomGuid);

        if (result == HOUSING_RESULT_SUCCESS)
            ++placedCount;
        else
            TC_LOG_ERROR("housing", "Housing::PlaceStarterDecor: Failed to place decor entry {} — result={}",
                p.DecorEntryId, result);
    }

    TC_LOG_ERROR("housing", "Housing::PlaceStarterDecor: Placed {}/{} starter decor items in visual room {} "
        "for house {} (player {})",
        placedCount, uint32(placements.size()), visualRoomGuid.ToString(),
        _houseGuid.ToString(), _owner->GetName());

    return placedCount;
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
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    // Sniff-verified: Lock→Move is valid (the locker is the one moving).
    // Lock only prevents OTHER editors from modifying — not the owner.
    // TODO: When multi-editor support is added, track LockedByGuid and check here.

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
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    // Sniff-verified: Lock→Remove is a valid retail flow (packet #27117 LOCK then
    // #27139 REMOVE with Result=0). The house owner can always remove their own decor.
    // Lock only prevents OTHER editors from modifying — not the owner.

    // Refund WeightCost budget (route to correct budget based on room)
    uint32 decorEntryId = itr->second.DecorEntryId;
    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    if (itr->second.RoomGuid.IsEmpty())
    {
        if (_exteriorDecorWeightUsed >= weightCost)
            _exteriorDecorWeightUsed -= weightCost;
        else
            _exteriorDecorWeightUsed = 0;
    }
    else
    {
        if (_interiorDecorWeightUsed >= weightCost)
            _interiorDecorWeightUsed -= weightCost;
        else
            _interiorDecorWeightUsed = 0;
    }

    // Return decor to catalog, preserving source info
    CatalogEntry& catEntry = _catalog[decorEntryId];
    catEntry.DecorEntryId = decorEntryId;
    catEntry.Count++;
    if (itr->second.SourceType != DECOR_SOURCE_STANDARD || !itr->second.SourceValue.empty())
    {
        catEntry.SourceType = itr->second.SourceType;
        catEntry.SourceValue = itr->second.SourceValue;
    }

    _placedDecor.erase(itr);

    // Immediate persist for crash safety — delete the placed decor row
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_DECOR_SINGLE);
        stmt->setUInt64(0, _owner->GetGUID().GetCounter());
        stmt->setUInt64(1, decorGuid.GetCounter());
        CharacterDatabase.Execute(stmt);
    }

    // Persist updated catalog count
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        stmt->setUInt32(0, _catalog[decorEntryId].Count);
        stmt->setUInt64(1, _owner->GetGUID().GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    // Remove from account decor storage UpdateField (only if storage is populated)
    if (_storagePopulated && _owner->GetSession())
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
        return HOUSING_RESULT_DECOR_NOT_FOUND;

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
        return HOUSING_RESULT_DECOR_NOT_FOUND;

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
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // Validate orientation (0-3 for cardinal directions)
    if (orientation > 3)
        return HOUSING_RESULT_PLOT_NOT_FOUND;

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
                return HOUSING_RESULT_ROOM_UPDATE_FAILED;
        }
    }

    // Non-base rooms require at least one existing room in the house
    if (!roomData->IsBaseRoom() && _rooms.empty())
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

    // Check room count limit
    if (_rooms.size() >= MAX_HOUSING_ROOMS_PER_HOUSE)
        return HOUSING_RESULT_GENERIC_FAILURE;

    // Check WeightCost-based room budget
    uint32 roomWeightCost = sHousingMgr.GetRoomWeightCost(roomEntryId);
    if (_roomWeightUsed + roomWeightCost > GetMaxRoomBudget())
        return HOUSING_RESULT_GENERIC_FAILURE;

    // Check for slot collision
    for (auto const& [guid, room] : _rooms)
    {
        if (room.SlotIndex == slotIndex)
            return HOUSING_RESULT_PLOT_NOT_FOUND;
    }

    // NOTE: Doorway components (Type 7) are OPTIONAL in the DB2.
    // Standard rooms (1-15) have 0 doorway components — they use wall segments (Type 1) instead.
    // Only prefab/custom rooms (113+) have explicit doorway components.
    // Retail places rooms without doorways, so we don't enforce this check.

    // Generate a new room guid
    uint64 newDbId = GenerateRoomDbId();
    ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, 0, 0, newDbId);

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

    // Account-level notification: room instance added
    if (_owner->GetSession())
    {
        WorldPackets::Housing::AccountHousingRoomAdded notif;
        notif.RoomGuid = roomGuid;
        _owner->GetSession()->SendPacket(notif.Write());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveRoom(ObjectGuid roomGuid)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // Can't remove the base room
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(itr->second.RoomEntryId);
    if (roomData && roomData->IsBaseRoom())
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

    // Can't remove the last room
    if (_rooms.size() <= 1)
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

    // Check if any placed decor references this room
    for (auto const& [guid, decor] : _placedDecor)
    {
        if (decor.RoomGuid == roomGuid)
            return HOUSING_RESULT_ROOM_UPDATE_FAILED;
    }

    // Verify remaining rooms stay connected after removal (BFS from base room)
    if (!IsRoomGraphConnectedWithout(roomGuid))
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

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
        return HOUSING_RESULT_ROOM_NOT_FOUND;

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
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // If swapping with another room
    if (!swapRoomGuid.IsEmpty())
    {
        auto swapItr = _rooms.find(swapRoomGuid);
        if (swapItr == _rooms.end())
            return HOUSING_RESULT_ROOM_NOT_FOUND;

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
                return HOUSING_RESULT_PLOT_NOT_FOUND;
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
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // Validate componentIds if provided
    for (uint32 componentId : componentIds)
    {
        if (!sRoomComponentStore.LookupEntry(componentId))
        {
            TC_LOG_DEBUG("housing", "Housing::ApplyRoomTheme: Invalid RoomComponent ID {} for room {}",
                componentId, roomGuid.ToString());
            return HOUSING_RESULT_ROOM_NOT_FOUND;
        }
    }

    itr->second.ThemeId = themeSetId;

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomTheme: Player {} applied theme {} to room {} ({} components) in house {}",
        _owner->GetName(), themeSetId, roomGuid.ToString(), componentIds.size(), _houseGuid.ToString());

    // Account-level notification: theme applied
    if (_owner->GetSession())
    {
        WorldPackets::Housing::AccountHousingThemeAdded notif;
        notif.ThemeGuid = roomGuid;
        _owner->GetSession()->SendPacket(notif.Write());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::ApplyRoomWallpaper(ObjectGuid roomGuid, uint32 wallpaperId, uint32 materialId, std::vector<uint32> const& componentIds)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // Validate componentIds if provided
    for (uint32 componentId : componentIds)
    {
        if (!sRoomComponentStore.LookupEntry(componentId))
        {
            TC_LOG_DEBUG("housing", "Housing::ApplyRoomWallpaper: Invalid RoomComponent ID {} for room {}",
                componentId, roomGuid.ToString());
            return HOUSING_RESULT_ROOM_NOT_FOUND;
        }
    }

    itr->second.WallpaperId = wallpaperId;
    itr->second.MaterialId = materialId;

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomWallpaper: Player {} applied wallpaper {} (material {}) to room {} ({} components) in house {}",
        _owner->GetName(), wallpaperId, materialId, roomGuid.ToString(), componentIds.size(), _houseGuid.ToString());

    // Account-level notification: wallpaper/material texture changed
    if (_owner->GetSession())
    {
        WorldPackets::Housing::AccountHousingRoomComponentTextureAdded notif;
        notif.TextureGuid = roomGuid;
        _owner->GetSession()->SendPacket(notif.Write());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::SetDoorType(ObjectGuid roomGuid, uint32 doorTypeId, uint8 doorSlot)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_ROOM_NOT_FOUND;

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
        return HOUSING_RESULT_ROOM_NOT_FOUND;

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
        return HOUSING_RESULT_FIXTURE_NOT_FOUND;

    // Enforce fixture budget for new fixtures (WeightCost = 1 per fixture by default)
    uint32 const fixtureWeightCost = 1;
    if (isNew)
    {
        if (_fixtureWeightUsed + fixtureWeightCost > GetMaxFixtureBudget())
            return HOUSING_RESULT_GENERIC_FAILURE;
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

    // Account-level notification: new fixture added
    if (isNew && _owner->GetSession())
    {
        WorldPackets::Housing::AccountHousingFixtureAdded notif;
        notif.FixtureGuid = _houseGuid;
        _owner->GetSession()->SendPacket(notif.Write());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveFixture(uint32 fixturePointId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _fixtures.find(fixturePointId);
    if (itr == _fixtures.end())
        return HOUSING_RESULT_FIXTURE_NOT_FOUND;

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

uint32 Housing::GetCoreExteriorComponentID() const
{
    // The core fixture is the one set via SetCoreFixture (OptionId == 0).
    // Its FixturePointId IS the ExteriorComponentID.
    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId == 0)
            return fixture.FixturePointId;
    }
    return 0; // No core fixture set
}

HousingResult Housing::AddToCatalog(uint32 decorEntryId, uint8 sourceType, std::string sourceValue)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    CatalogEntry& entry = _catalog[decorEntryId];
    entry.DecorEntryId = decorEntryId;
    entry.Count++;
    // Store the most recent source info for this entry type.
    // All instances of the same decorEntryId share the same source since catalog is quantity-based.
    if (sourceType != DECOR_SOURCE_STANDARD || !sourceValue.empty())
    {
        entry.SourceType = sourceType;
        entry.SourceValue = std::move(sourceValue);
    }

    // Persist to DB immediately (crash safety).
    // Uses REPLACE INTO to handle both first-add and count-increment cases.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHARACTER_HOUSING_CATALOG);
    stmt->setUInt64(0, _owner->GetGUID().GetCounter());
    stmt->setUInt32(1, decorEntryId);
    stmt->setUInt32(2, entry.Count);
    stmt->setUInt8(3, entry.SourceType);
    stmt->setString(4, entry.SourceValue);
    CharacterDatabase.Execute(stmt);

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
        return HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;

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
        return HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;

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

    // Remove from account decor storage UpdateField (only if storage is populated)
    if (_storagePopulated && _owner->GetSession() && !removedGuids.empty())
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

void Housing::AddLevel(uint32 amount)
{
    uint32 newLevel = std::min(_level + amount, MAX_HOUSE_LEVEL);
    if (newLevel == _level)
        return;

    _level = newLevel;

    TC_LOG_DEBUG("housing", "Housing::AddLevel: Player {} house leveled up to {} (added {}) in house {}",
        _owner->GetName(), _level, amount, _houseGuid.ToString());

    // Persist level/favor to DB immediately
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_LEVEL_FAVOR);
    stmt->setUInt32(0, _level);
    stmt->setUInt32(1, _favor);
    stmt->setUInt64(2, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    RecalculateBudgets();
    SyncUpdateFields();

    // Broadcast level/favor update to the owner
    if (_owner && _owner->GetSession())
    {
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelUpdate;
        levelUpdate.Type = 0;
        levelUpdate.Field1 = _favor;
        levelUpdate.Field2 = _level;
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor::LevelFavorEntry entry;
        entry.OwnerGUID = _owner->GetGUID();
        entry.HouseGUID = _houseGuid;
        entry.FavorAmount = _favor;
        entry.Level = _level;
        levelUpdate.Entries.push_back(std::move(entry));
        _owner->SendDirectMessage(levelUpdate.Write());
    }
}

void Housing::AddFavor(uint64 amount, HousingFavorUpdateSource source /*= HOUSING_FAVOR_SOURCE_UNKNOWN*/)
{
    _favor64 += amount;
    _favor = static_cast<uint32>(std::min<uint64>(_favor64, std::numeric_limits<uint32>::max()));

    TC_LOG_DEBUG("housing", "Housing::AddFavor: Player {} favor now {} (source {}) in house {}",
        _owner->GetName(), _favor64, uint8(source), _houseGuid.ToString());

    // Persist level/favor to DB immediately
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_LEVEL_FAVOR);
    stmt->setUInt32(0, _level);
    stmt->setUInt32(1, _favor);
    stmt->setUInt64(2, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();

    // Broadcast level/favor update to the owner
    if (_owner && _owner->GetSession())
    {
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor favorUpdate;
        favorUpdate.Type = static_cast<uint8>(source);
        favorUpdate.Field1 = _favor;
        favorUpdate.Field2 = _level;
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor::LevelFavorEntry entry;
        entry.OwnerGUID = _owner->GetGUID();
        entry.HouseGUID = _houseGuid;
        entry.FavorAmount = _favor;
        entry.Level = _level;
        favorUpdate.Entries.push_back(std::move(entry));
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
        uint32 previousLevel = _level;
        _level++;
        TC_LOG_DEBUG("housing", "Housing::OnQuestCompleted: Player {} house leveled up to {} (quest {}) in house {}",
            _owner->GetName(), _level, questId, _houseGuid.ToString());

        // Persist level change and recalculate budgets
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_LEVEL_FAVOR);
        stmt->setUInt32(0, _level);
        stmt->setUInt32(1, _favor);
        stmt->setUInt64(2, _owner->GetGUID().GetCounter());
        CharacterDatabase.Execute(stmt);

        RecalculateBudgets();
        SyncUpdateFields();

        // Broadcast level/favor update to the owner
        if (_owner && _owner->GetSession())
        {
            WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelUpdate;
            levelUpdate.Type = 0;
            levelUpdate.Field1 = _favor;
            levelUpdate.Field2 = _level;
            WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor::LevelFavorEntry entry;
            entry.OwnerGUID = _owner->GetGUID();
            entry.HouseGUID = _houseGuid;
            entry.FavorAmount = _favor;
            entry.Level = _level;
            levelUpdate.Entries.push_back(std::move(entry));
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

    // Sum WeightCost of all placed decor, routing to interior or exterior budget
    for (auto const& [guid, decor] : _placedDecor)
    {
        uint32 weightCost = sHousingMgr.GetDecorWeightCost(decor.DecorEntryId);
        if (decor.RoomGuid.IsEmpty())
            _exteriorDecorWeightUsed += weightCost;
        else
            _interiorDecorWeightUsed += weightCost;
    }

    // Sum WeightCost of all placed rooms
    for (auto const& [guid, room] : _rooms)
    {
        uint32 weightCost = sHousingMgr.GetRoomWeightCost(room.RoomEntryId);
        _roomWeightUsed += weightCost;
    }

    TC_LOG_DEBUG("housing", "Housing::RecalculateBudgets: Interior decor weight: {}/{}, Exterior decor weight: {}/{}, Room weight: {}/{}",
        _interiorDecorWeightUsed, GetMaxInteriorDecorBudget(),
        _exteriorDecorWeightUsed, GetMaxExteriorDecorBudget(),
        _roomWeightUsed, GetMaxRoomBudget());
}

void Housing::SyncUpdateFields()
{
    if (!_owner || !_owner->GetSession())
        return;

    // FHousingPlayerHouse_C belongs on the Housing/3 entity, NOT the BNetAccount entity.
    HousingPlayerHouseEntity& houseEntity = _owner->GetSession()->GetHousingPlayerHouseEntity();
    houseEntity.SetBnetAccount(_owner->GetSession()->GetBattlenetAccountGUID());
    houseEntity.SetEntityGUID(_houseGuid);
    houseEntity.SetHouseType(_houseType);
    houseEntity.SetHouseSize(static_cast<uint32>(_houseSize));
    houseEntity.SetPlotIndex(static_cast<int32>(_plotIndex));
    houseEntity.SetLevel(_level);
    houseEntity.SetFavor(_favor64);
    // Send MAX budgets — the client computes remaining locally by summing placed decor weight
    // from FHousingStorage_C entries. Sending (max - used) would cause double-subtraction.
    houseEntity.SetBudgets(
        GetMaxInteriorDecorBudget(),
        GetMaxExteriorDecorBudget(),
        GetMaxRoomBudget(),
        GetMaxFixtureBudget()
    );

    TC_LOG_ERROR("network", "Housing::SyncUpdateFields: EntityGUID={} BnetAccount={} HouseType={} HouseSize={} PlotIndex={} Level={} Favor={} Budgets=[{},{},{},{}]",
        _houseGuid.ToString(), _owner->GetSession()->GetBattlenetAccountGUID().ToString(),
        _houseType, _houseSize, _plotIndex, _level, _favor64,
        GetMaxInteriorDecorBudget(), GetMaxExteriorDecorBudget(), GetMaxRoomBudget(), GetMaxFixtureBudget());
}

void Housing::PopulateCatalogStorageEntries()
{
    if (!_owner || !_owner->GetSession())
        return;

    if (_storagePopulated)
        return;

    Battlenet::Account& account = _owner->GetSession()->GetBattlenetAccount();

    // 1. Placed decor → HouseGUID=_houseGuid, SourceType from decor instance
    for (auto const& [decorGuid, decor] : _placedDecor)
        account.SetHousingDecorStorageEntry(decorGuid, _houseGuid, decor.SourceType, decor.SourceValue);

    // 2. Catalog (unplaced/available) entries → HouseGUID=Empty, SourceType=0
    // Sniff-verified: items in storage have HouseGUID=Empty, placed items have non-empty HouseGUID.
    // Catalog Count includes placed instances, so subtract them to get the storage-only count.
    std::unordered_map<uint32, uint32> placedCountByEntry;
    for (auto const& [decorGuid, decor] : _placedDecor)
        placedCountByEntry[decor.DecorEntryId]++;

    uint64 catalogGuidBase = _owner->GetGUID().GetCounter() * 100000;
    uint32 totalStorageItems = 0;
    for (auto const& [entryId, entry] : _catalog)
    {
        uint32 placedOfType = 0;
        auto pIt = placedCountByEntry.find(entryId);
        if (pIt != placedCountByEntry.end())
            placedOfType = pIt->second;

        uint32 storageCount = entry.Count > placedOfType ? entry.Count - placedOfType : 0;
        for (uint32 i = 0; i < storageCount; ++i)
        {
            uint64 uniqueId = catalogGuidBase + entryId * 100 + i;
            ObjectGuid catalogDecorGuid = ObjectGuid::Create<HighGuid::Housing>(
                /*subType*/ 1,
                /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
                /*arg2*/ entryId,
                uniqueId);
            account.SetHousingDecorStorageEntry(catalogDecorGuid, ObjectGuid::Empty, entry.SourceType, entry.SourceValue);
        }
        totalStorageItems += storageCount;
    }

    _storagePopulated = true;

    TC_LOG_INFO("housing", "Housing::PopulateCatalogStorageEntries: Pushed {} placed + {} storage items ({} catalog types) for player {}",
        uint32(_placedDecor.size()), totalStorageItems, uint32(_catalog.size()), _owner->GetGUID().ToString());
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

void Housing::SetHouseNameDescription(std::string const& name, std::string const& desc)
{
    _houseName = name.substr(0, HOUSING_MAX_NAME_LENGTH);
    _houseDescription = desc.substr(0, 256);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_NAME_DESC);
    stmt->setString(0, _houseName);
    stmt->setString(1, _houseDescription);
    stmt->setUInt64(2, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();

    TC_LOG_DEBUG("housing", "Housing::SetHouseNameDescription: Player {} set house name='{}' desc='{}' in house {}",
        _owner->GetName(), _houseName, _houseDescription, _houseGuid.ToString());
}

void Housing::SetExteriorLocked(bool locked)
{
    _exteriorLocked = locked;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_EXTERIOR_LOCKED);
    stmt->setUInt8(0, locked ? 1 : 0);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();

    TC_LOG_DEBUG("housing", "Housing::SetExteriorLocked: Player {} {} exterior of house {}",
        _owner->GetName(), locked ? "locked" : "unlocked", _houseGuid.ToString());
}

void Housing::SetHouseSize(uint8 size)
{
    _houseSize = size;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_HOUSE_SIZE);
    stmt->setUInt8(0, size);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();

    TC_LOG_DEBUG("housing", "Housing::SetHouseSize: Player {} set house {} size to {}",
        _owner->GetName(), _houseGuid.ToString(), size);
}

void Housing::SetHouseType(uint32 typeId)
{
    _houseType = typeId;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_HOUSE_TYPE);
    stmt->setUInt32(0, typeId);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();

    TC_LOG_DEBUG("housing", "Housing::SetHouseType: Player {} set house {} type to {}",
        _owner->GetName(), _houseGuid.ToString(), typeId);
}

void Housing::SetHousePosition(float x, float y, float z, float facing)
{
    _housePosX = x;
    _housePosY = y;
    _housePosZ = z;
    _houseFacing = facing;
    _hasCustomPosition = true;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_POSITION);
    stmt->setFloat(0, x);
    stmt->setFloat(1, y);
    stmt->setFloat(2, z);
    stmt->setFloat(3, facing);
    stmt->setUInt64(4, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Housing::SetHousePosition: Player {} positioned house at ({}, {}, {}, {}) in house {}",
        _owner->GetName(), x, y, z, facing, _houseGuid.ToString());
}

uint64 Housing::GenerateDecorDbId()
{
    return s_nextDecorDbId.fetch_add(1);
}

uint64 Housing::GenerateRoomDbId()
{
    return s_nextRoomDbId.fetch_add(1);
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
