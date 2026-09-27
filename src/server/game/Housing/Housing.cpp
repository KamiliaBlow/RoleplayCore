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
#include "DBCEnums.h"
#include "GameTime.h"
#include "HousingMgr.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include <algorithm>
#include "RealmList.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Util.h"
#include "WorldSession.h"
#include <cmath>
#include <queue>
#include <unordered_set>

namespace
{
    // M13: normalize a decor rotation quaternion to a unit quaternion before it
    // is stored. The client sends Euler angles which the handler converts to a
    // quaternion each place/move; normalizing removes any float drift so a decor
    // item at a cardinal angle (0/90/180/270) round-trips through the FLOAT
    // columns to the exact same orientation instead of subtly re-rotating on
    // reload (the retail rotation bug we must not replicate). A degenerate
    // (near-zero) quaternion falls back to identity.
    void NormalizeDecorRotation(float& x, float& y, float& z, float& w)
    {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        if (!std::isfinite(len) || len < 1e-6f)
        {
            x = y = z = 0.0f;
            w = 1.0f;
            return;
        }
        float inv = 1.0f / len;
        x *= inv; y *= inv; z *= inv; w *= inv;
    }

    // House type (HouseExteriorWmoData) of the component a hook hangs on, 0 if unknown.
    uint32 GetHookOwnerWmo(uint32 hookId)
    {
        ExteriorComponentHookEntry const* hook = sExteriorComponentHookStore.LookupEntry(hookId);
        if (!hook)
            return 0;
        ExteriorComponentEntry const* owner = sExteriorComponentStore.LookupEntry(hook->ExteriorComponentID);
        return owner ? owner->HouseExteriorWmoDataID : 0;
    }
}

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
    // Expected columns: houseId, neighborhoodGuid, plotIndex, level, favor, settingsFlags, exteriorLocked, houseSize, houseType, ...
    // fields[0] = houseId (DB2 entry ID) — NOT used as GUID counter.
    // Housing GUID counter must match HousingPlayerHouseEntity GUID (WorldSession.cpp), which uses battlenetAccountId.
    uint32 bnetAccountId = _owner->GetSession()->GetBattlenetAccountId();
    // Take the neighborhood's real GUID from the manager rather than rebuilding it: arg1 is its
    // NeighborhoodMapID, and this GUID goes out to the client in house/neighborhood packets, so a wrong arg1
    // reproduces the client-side NeighborhoodMap.db2 miss on the house path too.
    _neighborhoodGuid.Clear();
    uint32 neighborhoodMapId = 0;
    if (Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhoodByCounter(fields[1].GetUInt64()))
    {
        _neighborhoodGuid = neighborhood->GetGuid();
        neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    }
    else
        TC_LOG_ERROR("housing", "Housing::LoadFromDB: house references neighborhood counter {} which is not loaded",
            fields[1].GetUInt64());
    _houseGuid = MakeHouseGuid(neighborhoodMapId, bnetAccountId);
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

    // Load rooms FIRST so decor can look up roomEntryId for GUID arg2
    //           0         1            2           3       4       5           6            7         8        9              10              11               12             13          14        15              16
    // SELECT roomGuid, roomEntryId, slotIndex, gridX, gridY, floorIndex, orientation, mirrored, themeId, wallTextureId, floorTextureId, ceilingTextureId, colorOverride, doorTypeId, doorSlot, ceilingTypeId, ceilingSlot
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

            // arg2=roomEntryId matches retail GUID format (sniff-verified: arg2=HouseRoomID)
            ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, 0, roomEntryId, roomDbId);

            Room& room = _rooms[roomGuid];
            room.Guid = roomGuid;
            room.RoomEntryId = roomEntryId;
            room.SlotIndex = fields[2].GetUInt32();
            room.GridX = fields[3].GetInt32();
            room.GridY = fields[4].GetInt32();
            room.FloorIndex = fields[5].GetInt32();
            // Backward compat: if gridX looks like old grid index (0-20), convert to yards
            if (room.GridX >= 0 && room.GridX <= 20 && room.GridX == static_cast<int32>(room.SlotIndex) && room.SlotIndex > 0)
                room.GridX = static_cast<int32>(room.SlotIndex) * static_cast<int32>(HOUSING_ROOM_GRID_SPACING);
            // Backward compat: previously FloorIndex was stored as a yard offset (e.g. 12
            // for upper stairwell partner). Retail treats it as a floor NUMBER (0, 1, 2…)
            // multiplied by 12 yards at spawn time. Convert legacy multiples-of-12 values.
            if (room.FloorIndex >= 12 && (room.FloorIndex % 12) == 0)
                room.FloorIndex /= 12;
            room.Orientation = fields[6].GetUInt32();
            room.Mirrored = fields[7].GetBool();
            room.ThemeId = fields[8].GetUInt32();
            room.WallTextureId = fields[9].GetUInt32();
            room.FloorTextureId = fields[10].GetUInt32();
            room.CeilingTextureId = fields[11].GetUInt32();
            room.ColorOverride = fields[12].GetInt32();
            room.DoorTypeId = fields[13].GetUInt32();
            room.DoorSlot = fields[14].GetUInt8();
            room.CeilingTypeId = fields[15].GetUInt32();
            room.CeilingSlot = fields[16].GetUInt8();
            room.WallThemeId = fields[17].GetUInt32();
            room.FloorThemeId = fields[18].GetUInt32();
            room.CeilingThemeId = fields[19].GetUInt32();
            LoadDoorTypes(room, fields[20].GetString());
            LoadComponentStyles(room, fields[21].GetString());
            // Legacy rows (pre-per-surface-theme migration) have all three = 0:
            // seed them from the single ThemeId so old houses keep their look.
            if (!room.WallThemeId && !room.FloorThemeId && !room.CeilingThemeId && room.ThemeId)
            {
                room.WallThemeId = room.ThemeId;
                room.FloorThemeId = room.ThemeId;
                room.CeilingThemeId = room.ThemeId;
            }

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

        // NOTE: Previously replaced non-Room-1 rooms with Room 1. This was too aggressive —
        // it replaced valid user placements (e.g., Stairwell) and lost gridX/gridY coordinates.
        // Rooms placed by the user are valid regardless of entry type. Only add a default
        // visual room if there are NO non-base rooms at all (empty house).

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

    // Load placed decor (after rooms so RoomGuid can use correct arg2=roomEntryId)
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
            placed.Scale = fields[9].GetFloat();
            if (placed.Scale < 0.01f) placed.Scale = 1.0f;
            placed.DyeSlots[0] = fields[10].GetUInt32();
            placed.DyeSlots[1] = fields[11].GetUInt32();
            placed.DyeSlots[2] = fields[12].GetUInt32();
            uint64 roomDbId = fields[13].GetUInt64();
            if (roomDbId)
            {
                // Use the room's actual GUID key from _rooms, not a reconstructed one.
                // Room migration (e.g. entry 18→46) changes RoomEntryId but not the GUID
                // key's arg2 field. Reconstructing with the migrated RoomEntryId would
                // produce a GUID that doesn't match the room's key, breaking AttachParentGUID.
                for (auto const& [rGuid, r] : _rooms)
                {
                    if (rGuid.GetCounter() == roomDbId)
                    {
                        placed.RoomGuid = rGuid;
                        break;
                    }
                }
            }
            placed.Locked = fields[14].GetUInt8() != 0;
            placed.PlacementTime = static_cast<time_t>(fields[15].GetUInt64());
            placed.SourceType = fields[16].GetUInt8();
            placed.SourceValue = fields[17].GetString();
            if (uint64 petCounter = fields[18].GetUInt64())
                placed.PetGuid = ObjectGuid::Create<HighGuid::BattlePet>(petCounter);
            placed.PetFlag = fields[19].GetUInt8();

            uint64 expected = s_nextDecorDbId.load();
            while (decorDbId >= expected && !s_nextDecorDbId.compare_exchange_weak(expected, decorDbId + 1))
                ;

        } while (decor->NextRow());
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

        // A fixture of one house type on a hook of another (left by an old type switch) never spawns - its hook's
        // root is not part of the house - yet it counted as the house's entrance, so no door was placed.
        for (auto itr = _fixtures.begin(); itr != _fixtures.end();)
        {
            ExteriorComponentEntry const* comp = itr->second.OptionId ? sExteriorComponentStore.LookupEntry(itr->second.OptionId) : nullptr;
            uint32 const hookWmo = comp ? GetHookOwnerWmo(itr->first) : 0;
            if (comp && hookWmo && comp->HouseExteriorWmoDataID && comp->HouseExteriorWmoDataID != hookWmo)
            {
                TC_LOG_INFO("housing", "Housing::LoadFromDB: dropping fixture comp {} (wmo {}) on hook {} of wmo {}",
                    itr->second.OptionId, comp->HouseExteriorWmoDataID, itr->first, hookWmo);
                CharacterDatabasePreparedStatement* del = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
                del->setUInt64(0, _owner->GetGUID().GetCounter());
                del->setUInt32(1, itr->first);
                CharacterDatabase.Execute(del);
                itr = _fixtures.erase(itr);
            }
            else
                ++itr;
        }

        // Log all loaded fixtures for debugging
        for (auto const& [pointId, fix] : _fixtures)
        {
            TC_LOG_INFO("housing", "Housing::LoadFromDB: Fixture pointId={} optionId={}", fix.FixturePointId, fix.OptionId);
        }
    }

    // Migration: populate starter fixtures for houses created before persistence was added.
    // Also handles existing houses that have fixtures but are missing starter roots (Base/Roof) or door.
    bool hasBaseRoot = false, hasRoofRoot = false, hasDoor = false;
    for (auto const& [pointId, fix] : _fixtures)
    {
        if (fix.OptionId == 0)
        {
            ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fix.FixturePointId);
            if (!comp)
                continue;
            if (_houseType != 0 && comp->HouseExteriorWmoDataID != static_cast<uint32>(_houseType))
                continue;
            if (comp->Type == HOUSING_FIXTURE_TYPE_BASE) hasBaseRoot = true;
            if (comp->Type == HOUSING_FIXTURE_TYPE_ROOF) hasRoofRoot = true;
        }
    }
    hasDoor = HasCurrentTypeDoor();

    if ((!hasBaseRoot || !hasRoofRoot || !hasDoor) && _houseType != 0)
    {
        TC_LOG_INFO("housing", "Housing::LoadFromDB: Missing starter fixtures (base={}, roof={}, door={}) for house {} — populating (migration)",
            hasBaseRoot, hasRoofRoot, hasDoor, _houseGuid.ToString());
        PopulateStarterFixtures();
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

    // Starter decor is placed once, when the house is bought (NeighborhoodHandler). No top-up here: a house whose owner
    // cleared every item would get the starter set back on each login.

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

// The CHAR_INS_CHARACTER_HOUSING_DECOR column list is spelled out here and nowhere else. It used to
// be repeated at each of the three insert sites, and two of them had drifted: they stopped after
// sourceValue, leaving petGuid and petFlag unbound, and wrote literal zeros where the decor already
// carried its dye slots and lock flag.
static void BindDecorInsert(CharacterDatabasePreparedStatement* stmt, ObjectGuid::LowType ownerGuid,
    ObjectGuid decorGuid, Housing::PlacedDecor const& decor)
{
    uint8 index = 0;
    stmt->setUInt64(index++, ownerGuid);
    stmt->setUInt64(index++, decorGuid.GetCounter());
    stmt->setUInt32(index++, decor.DecorEntryId);
    stmt->setFloat(index++, decor.PosX);
    stmt->setFloat(index++, decor.PosY);
    stmt->setFloat(index++, decor.PosZ);
    stmt->setFloat(index++, decor.RotationX);
    stmt->setFloat(index++, decor.RotationY);
    stmt->setFloat(index++, decor.RotationZ);
    stmt->setFloat(index++, decor.RotationW);
    stmt->setFloat(index++, decor.Scale);
    stmt->setUInt32(index++, decor.DyeSlots[0]);
    stmt->setUInt32(index++, decor.DyeSlots[1]);
    stmt->setUInt32(index++, decor.DyeSlots[2]);
    stmt->setUInt64(index++, decor.RoomGuid.IsEmpty() ? 0 : decor.RoomGuid.GetCounter());
    stmt->setUInt8(index++, decor.Locked ? 1 : 0);
    stmt->setUInt64(index++, static_cast<uint64>(decor.PlacementTime));
    stmt->setUInt8(index++, decor.SourceType);
    stmt->setString(index++, decor.SourceValue);
    stmt->setUInt64(index++, decor.PetGuid.IsEmpty() ? 0 : decor.PetGuid.GetCounter());
    stmt->setUInt8(index++, decor.PetFlag);
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
        BindDecorInsert(stmt, ownerGuid, guid, decor);
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
        stmt->setInt32(index++, room.GridX);
        stmt->setInt32(index++, room.GridY);
        stmt->setInt32(index++, room.FloorIndex);
        stmt->setUInt32(index++, room.Orientation);
        stmt->setBool(index++, room.Mirrored);
        stmt->setUInt32(index++, room.ThemeId);
        stmt->setUInt32(index++, room.WallTextureId);
        stmt->setUInt32(index++, room.FloorTextureId);
        stmt->setUInt32(index++, room.CeilingTextureId);
        stmt->setInt32(index++, room.ColorOverride);
        stmt->setUInt32(index++, room.DoorTypeId);
        stmt->setUInt8(index++, room.DoorSlot);
        stmt->setUInt32(index++, room.CeilingTypeId);
        stmt->setUInt8(index++, room.CeilingSlot);
        stmt->setUInt32(index++, room.WallThemeId);
        stmt->setUInt32(index++, room.FloorThemeId);
        stmt->setUInt32(index++, room.CeilingThemeId);
        stmt->setString(index++, SerializeDoorTypes(room));
        stmt->setString(index++, SerializeComponentStyles(room));
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
    //
    // The field carries the editing context, not HousingEditorMode (12.1.0.69933 sniffs: decor edit = 1, room
    // layout = 2, fixture edit = 3). Layout used to go out as 3, so the client ran the exterior-fixture camera,
    // anchored to the house on the plot, and kept it after leaving the interior.
    HouseEditingContext context = HOUSE_EDITING_CONTEXT_NONE;
    switch (mode)
    {
        case HOUSING_EDITOR_MODE_BASIC_DECOR:
        case HOUSING_EDITOR_MODE_EXPERT_DECOR:
        case HOUSING_EDITOR_MODE_CLEANUP:
            context = HOUSE_EDITING_CONTEXT_DECOR;
            break;
        case HOUSING_EDITOR_MODE_LAYOUT:
        case HOUSING_EDITOR_MODE_CUSTOMIZE:
            context = HOUSE_EDITING_CONTEXT_ROOM;
            break;
        case HOUSING_EDITOR_MODE_EXTERIOR_CUSTOMIZATION:
            context = HOUSE_EDITING_CONTEXT_FIXTURE;
            break;
        default:
            break;
    }

    if (_owner)
    {
        _owner->SetHousingEditorModeUpdateField(static_cast<uint8>(context));

        // The "Disable All the Things" auras belong to one editor each; leaving it (or the map) drops them.
        if (context != HOUSE_EDITING_CONTEXT_DECOR)
            _owner->RemoveAurasDueToSpell(SPELL_HOUSING_EDIT_MODE_AURA);
        if (context != HOUSE_EDITING_CONTEXT_ROOM)
            _owner->RemoveAurasDueToSpell(SPELL_HOUSING_ROOM_EDIT_MODE_AURA);
    }
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
    // Racial house style: Night Elf → 55, Blood Elf → 56, other Alliance → 9, other Horde → 87
    _houseType = HousingMgr::GetRacialWmoDataID(_owner->GetRace(), _owner->GetTeam());
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
    Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    _houseGuid = MakeHouseGuid(neighborhood ? neighborhood->GetNeighborhoodMapID() : 0, bnetAccountId);

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
        // Entry door at +3, Room1 door at -12 → spacing = 3-(-12) = 15 yards
        HousingResult visualResult = PlaceRoom(visualRoom, /*slotIndex*/ 1, /*orientation*/ 0, /*mirrored*/ false, nullptr, /*gridX*/ 15, /*gridY*/ 0);
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

    // Populate starter fixtures: Base + Roof for the racial WMO style.
    // These are persisted to DB so spawning only reads what's stored.
    PopulateStarterFixtures();

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

    // m2/A5: release the neighborhood plot so it becomes vacant and
    // re-purchasable instead of being orphaned forever (the old bug: delete /
    // kiosk-reset removed the character rows but never freed the PlotInfo).
    if (!_neighborhoodGuid.IsEmpty())
    {
        if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
            neighborhood->ReleasePlot(_owner->GetGUID());
    }

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

ObjectGuid Housing::GenerateDecorGuid(uint32 decorEntryId)
{
    return ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 1, /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ decorEntryId, GenerateDecorDbId());
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
        result = HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
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

// HouseDecor.db2 InitialScale: the size the client previews a decor item at before the player resizes it.
static float GetDecorInitialScale(uint32 decorEntryId)
{
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decorEntryId);
    return decorData && decorData->InitialScale >= 0.01f ? decorData->InitialScale : 1.0f;
}

HousingResult Housing::PlaceDecorWithGuid(ObjectGuid decorGuid, uint32 decorEntryId, float x, float y, float z,
    float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid, float scale)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(rotX) || !std::isfinite(rotY) || !std::isfinite(rotZ) || !std::isfinite(rotW))
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    HousingResult validationResult = sHousingMgr.ValidateDecorPlacement(decorEntryId, Position(x, y, z),
        GetDecorPlacementAnchor(roomGuid), _level);
    if (validationResult != HOUSING_RESULT_SUCCESS)
        return validationResult;

    if (HousingResult boundsResult = CheckInteriorDecorBounds(roomGuid, x, y, z); boundsResult != HOUSING_RESULT_SUCCESS)
        return boundsResult;

    uint32 maxDecor = GetMaxDecorCount();
    if (GetDecorCount() >= maxDecor)
        return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;

    // Retail semantics (verified via sniff build 66263, both alliance + horde):
    // the client ALWAYS sends a non-Empty RoomGuid in CMSG_HOUSING_DECOR_PLACE.
    // Exterior placements use Housing-2-arg2=<base-room-entry-id>-counter=X
    // (arg2=18 on 66263). Interior placements use Housing-2-arg2=<visual-room
    // -entry-id>-counter=Y (arg2=1 etc.). AttachParent is always Empty.
    //
    // Detect the plot exterior room identity and route it to the exterior
    // budget/skip the interior _rooms lookup, while preserving the RoomGuid
    // as-sent so downstream consumers (DB row, move/remove round-trips) still
    // see what retail sends.
    bool const isExterior = IsExteriorDecorPlacement(roomGuid);

    // A4: enforce the outdoor "two lights cannot overlap" rule before charging.
    if (HousingResult overlap = CheckLightOverlap(decorEntryId, x, y, z, isExterior);
        overlap != HOUSING_RESULT_SUCCESS)
        return overlap;

    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    if (isExterior)
    {
        if (_exteriorDecorWeightUsed + weightCost > GetMaxExteriorDecorBudget())
            return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
    }
    else
    {
        if (_interiorDecorWeightUsed + weightCost > GetMaxInteriorDecorBudget())
            return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
    }

    if (!isExterior)
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
            return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
    }

    auto catalogItr = _catalog.find(decorEntryId);
    if (catalogItr == _catalog.end() || catalogItr->second.Count == 0)
        return HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;

    // H-09: decorGuid arrives from the client on the fallback path (no matching pending
    // placement), and used to be trusted as the map key and the DB row id without ever
    // being checked. Two consequences, both closed here.
    //
    // 1. _placedDecor[decorGuid] overwrote an existing entry in place. Re-sending the GUID
    //    of something already placed destroyed the old record: its weight was never
    //    returned to the budget (so the budget inflated permanently), the catalog was
    //    still decremented, and the player silently lost the item that had been there.
    //    Placement onto an occupied GUID is now refused instead of overwriting.
    if (_placedDecor.contains(decorGuid))
        return HOUSING_RESULT_INVALID_DECOR_ITEM;

    // 2. A client-chosen counter never advanced s_nextDecorDbId, so the generator would
    //    later hand the same id to a legitimate placement. Bump past it, the same way the
    //    load path reconciles ids it did not issue.
    uint64 const clientDbId = decorGuid.GetCounter();
    uint64 expectedDbId = s_nextDecorDbId.load();
    while (clientDbId >= expectedDbId && !s_nextDecorDbId.compare_exchange_weak(expectedDbId, clientDbId + 1))
        ;

    // Remove from pending placements
    _pendingPlacements.erase(decorGuid);

    // M13: persist a normalized unit quaternion for a lossless cardinal round-trip.
    NormalizeDecorRotation(rotX, rotY, rotZ, rotW);

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
    // The client previews and places at the scale it sends (HouseDecor InitialScale unless resized, e.g. 0.6);
    // dropping it spawned such decor at 1.0, bigger than the preview. Same clamp as MoveDecor.
    decor.Scale = std::isfinite(scale) && scale >= 0.01f ? std::min(scale, 5.0f) : GetDecorInitialScale(decorEntryId);
    decor.DyeSlots = {};
    decor.RoomGuid = roomGuid;
    decor.PlacementTime = GameTime::GetGameTime();
    // Inherit acquisition source from catalog entry
    decor.SourceType = catalogItr->second.SourceType;
    decor.SourceValue = catalogItr->second.SourceValue;

    catalogItr->second.Count--;
    if (catalogItr->second.Count == 0)
        _catalog.erase(catalogItr);

    // M2: charge the SAME budget the CHECK validated (exterior-plot rooms count
    // as exterior, not interior).
    if (isExterior)
        _exteriorDecorWeightUsed += weightCost;
    else
        _interiorDecorWeightUsed += weightCost;

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        BindDecorInsert(stmt, _owner->GetGUID().GetCounter(), decorGuid, decor);
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

    // CriteriaType::PlaceDecor (270, "Place any decor"). miscValue1 = HouseDecor entry so decor-scoped
    // ModifierTree conditions can still discriminate; this is the single commit point for a placement.
    _owner->UpdateCriteria(CriteriaType::PlaceDecor, decorEntryId);

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
    HousingResult validationResult = sHousingMgr.ValidateDecorPlacement(decorEntryId, Position(x, y, z),
        GetDecorPlacementAnchor(roomGuid), _level);
    if (validationResult != HOUSING_RESULT_SUCCESS)
        return validationResult;

    if (HousingResult boundsResult = CheckInteriorDecorBounds(roomGuid, x, y, z); boundsResult != HOUSING_RESULT_SUCCESS)
        return boundsResult;

    // Check decor count limit based on house level
    uint32 maxDecor = GetMaxDecorCount();
    if (GetDecorCount() >= maxDecor)
        return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;

    // Check WeightCost-based budget (exterior vs interior) — M2: classify once.
    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    bool const isExterior = IsExteriorDecorPlacement(roomGuid);

    // A4: enforce the outdoor "two lights cannot overlap" rule before charging.
    if (HousingResult overlap = CheckLightOverlap(decorEntryId, x, y, z, isExterior);
        overlap != HOUSING_RESULT_SUCCESS)
        return overlap;

    if (isExterior)
    {
        // Outdoor decor uses exterior budget
        if (_exteriorDecorWeightUsed + weightCost > GetMaxExteriorDecorBudget())
            return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
    }
    else
    {
        // Indoor decor uses interior budget
        if (_interiorDecorWeightUsed + weightCost > GetMaxInteriorDecorBudget())
            return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
    }

    // Validate room exists if specified, and check per-room decor limit
    if (!isExterior)
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
            return HOUSING_RESULT_MAX_PLACED_DECOR_REACHED;
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

    // M13: persist a normalized unit quaternion for a lossless cardinal round-trip.
    NormalizeDecorRotation(rotX, rotY, rotZ, rotW);

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
    decor.Scale = GetDecorInitialScale(decorEntryId);
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

    // Update budget tracking (route to correct budget based on room) — M2.
    if (isExterior)
        _exteriorDecorWeightUsed += weightCost;
    else
        _interiorDecorWeightUsed += weightCost;

    // Immediate persist for crash safety
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        BindDecorInsert(stmt, _owner->GetGUID().GetCounter(), decorGuid, decor);
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
    Room const* visualRoom = nullptr;
    for (auto const& [guid, room] : _rooms)
    {
        if (!sHousingMgr.IsBaseRoom(room.RoomEntryId))
        {
            visualRoomGuid = guid;
            visualRoom = &room;
            break;
        }
    }

    if (visualRoomGuid.IsEmpty() || !visualRoom)
    {
        TC_LOG_ERROR("housing", "Housing::PlaceStarterDecor: No visual room found for house {} — cannot place starter decor",
            _houseGuid.ToString());
        return 0;
    }

    // PlaceDecor stores a WORLD position in interior-map space, because that is what the
    // client's placement packet carries and it is the same field HouseInteriorMap reads back.
    // The table below is room-local (sniff-derived), so convert it here - otherwise the five
    // starter items are written in a second, incompatible convention and land about a
    // kilometre outside the house, present in the DB but never visible. Proven by decor 726:
    // starter-written as (9.844,-8.013,0.02), then rewritten by the client as
    // (-985.787,-997.955,7.726) the moment the player moved that same item.
    float roomOriginX = -1000.0f, roomOriginY = -1000.0f, roomOriginZ = 0.1f;
    if (NeighborhoodMapData const* nmData = sHousingMgr.GetNeighborhoodMapDataForWorldMap(HOUSE_INTERIOR_MAP_ID))
    {
        roomOriginX = nmData->Origin[0];
        roomOriginY = nmData->Origin[1];
        roomOriginZ = nmData->Origin[2];
    }
    roomOriginX += static_cast<float>(visualRoom->GridX);
    roomOriginY += static_cast<float>(visualRoom->GridY);
    roomOriginZ += static_cast<float>(visualRoom->FloorIndex) * HOUSE_INTERIOR_FLOOR_HEIGHT;

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

        HousingResult result = PlaceDecor(p.DecorEntryId,
            roomOriginX + p.X, roomOriginY + p.Y, roomOriginZ + p.Z,
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
    float rotX, float rotY, float rotZ, float rotW, float scale /*= 1.0f*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Validate coordinate sanity
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(rotX) || !std::isfinite(rotY) || !std::isfinite(rotZ) || !std::isfinite(rotW))
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    // Clamp scale to reasonable range (sniff shows values like 0.45 to 1.62)
    if (!std::isfinite(scale) || scale < 0.01f)
        scale = 1.0f;
    if (scale > 5.0f)
        scale = 5.0f;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    // M1: MoveDecor previously performed NO spatial validation. Route the move
    // target through the same room/plot AABB check as placement so a moved item
    // cannot be flung to arbitrary coordinates.
    HousingResult validationResult = sHousingMgr.ValidateDecorPlacement(itr->second.DecorEntryId, Position(x, y, z),
        GetDecorPlacementAnchor(itr->second.RoomGuid), _level);
    if (validationResult != HOUSING_RESULT_SUCCESS)
        return validationResult;

    if (HousingResult boundsResult = CheckInteriorDecorBounds(itr->second.RoomGuid, x, y, z); boundsResult != HOUSING_RESULT_SUCCESS)
        return boundsResult;

    // A4: a moved light must also honour the "two lights cannot overlap" rule.
    // Exclude the decor being moved so an in-place nudge never collides with itself.
    if (HousingResult overlap = CheckLightOverlap(itr->second.DecorEntryId, x, y, z,
            IsExteriorDecorPlacement(itr->second.RoomGuid), decorGuid);
        overlap != HOUSING_RESULT_SUCCESS)
        return overlap;

    // M13: normalize the rotation quaternion for a lossless cardinal round-trip.
    NormalizeDecorRotation(rotX, rotY, rotZ, rotW);

    PlacedDecor& decor = itr->second;
    decor.PosX = x;
    decor.PosY = y;
    decor.PosZ = z;
    decor.RotationX = rotX;
    decor.RotationY = rotY;
    decor.RotationZ = rotZ;
    decor.RotationW = rotW;
    decor.Scale = scale;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_POSITION);
    stmt->setFloat(0, x);
    stmt->setFloat(1, y);
    stmt->setFloat(2, z);
    stmt->setFloat(3, rotX);
    stmt->setFloat(4, rotY);
    stmt->setFloat(5, rotZ);
    stmt->setFloat(6, rotW);
    stmt->setFloat(7, scale);
    stmt->setUInt64(8, _owner->GetGUID().GetCounter());
    stmt->setUInt64(9, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Housing::MoveDecor: Player {} moved decor {} to ({}, {}, {}) scale={:.2f} in house {}",
        _owner->GetName(), decorGuid.ToString(), x, y, z, scale, _houseGuid.ToString());

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
    if (IsExteriorDecorPlacement(itr->second.RoomGuid))
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

    // CriteriaType::RemoveDecor (271, "Remove any decor"). miscValue1 = the HouseDecor entry removed
    // (captured before the erase invalidated the iterator).
    _owner->UpdateCriteria(CriteriaType::RemoveDecor, decorEntryId);

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

    // Validate each requested dye color exists in the palette (DyeColor.db2). A color of 0
    // clears that slot and is always allowed. Each dye slot maps to a shader channel on the
    // decor (per HousingDecorDyeSlot.channel) - for light-emitting decor a channel drives the
    // emitted light/glow color, so this is also how decor "lighting" is recolored.
    // NOTE: the client documents that dye slots "accept colors of any category"
    // (HousingDecorDyeSlot.dyeColorCategoryID has no functional use), so category is NOT
    // enforced here - only that the color is a real DyeColor record.
    // (Per-account dye OWNERSHIP is a separate, currently wire-unrecovered gate - see
    // HOUSING_DYE_SYSTEM_ANALYSIS_68275.md.)
    for (uint32 const dyeColorId : dyeSlots)
    {
        if (dyeColorId && !sDyeColorStore.LookupEntry(dyeColorId))
            return HOUSING_RESULT_MISSING_DYE; // color does not exist in DyeColor.db2
    }

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

HousingResult Housing::SetDecorPet(ObjectGuid decorGuid, ObjectGuid petGuid, uint8 petFlag)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    itr->second.PetGuid = petGuid;
    itr->second.PetFlag = petFlag;

    // Immediate targeted persist for crash safety (mirrors SetDecorLocked).
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_PET);
    stmt->setUInt64(0, petGuid.IsEmpty() ? 0 : petGuid.GetCounter());
    stmt->setUInt8(1, petFlag);
    stmt->setUInt64(2, _owner->GetGUID().GetCounter());
    stmt->setUInt64(3, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    TC_LOG_DEBUG("housing", "Housing::SetDecorPet: Player {} {} pet {} on decor {} in house {}",
        _owner->GetName(), petGuid.IsEmpty() ? "cleared" : "bound", petGuid.ToString(),
        decorGuid.ToString(), _houseGuid.ToString());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::ResetDecor(uint8 scope, uint32* outRemoved /*= nullptr*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // HousingHouseScope: 1 = Interior, 2 = Exterior. Anything else is rejected.
    if (scope != 1 && scope != 2)
        return HOUSING_RESULT_GENERIC_FAILURE;

    bool wantExterior = (scope == 2);

    // Snapshot the matching guids first — RemoveDecor mutates _placedDecor.
    std::vector<ObjectGuid> toRemove;
    toRemove.reserve(_placedDecor.size());
    for (auto const& [guid, decor] : _placedDecor)
    {
        if (IsExteriorDecorPlacement(decor.RoomGuid) == wantExterior)
            toRemove.push_back(guid);
    }

    uint32 removed = 0;
    for (ObjectGuid const& guid : toRemove)
    {
        // RemoveDecor refunds budget, returns the item to the catalog, deletes the DB row
        // and syncs update fields for each item — full teardown per decor.
        if (RemoveDecor(guid) == HOUSING_RESULT_SUCCESS)
            ++removed;
    }

    if (outRemoved)
        *outRemoved = removed;

    TC_LOG_INFO("housing", "Housing::ResetDecor: Player {} reset {} scope, removed {} decor from house {}",
        _owner->GetName(), wantExterior ? "exterior" : "interior", removed, _houseGuid.ToString());

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

HousingResult Housing::PlaceRoom(uint32 roomEntryId, uint32 slotIndex, uint32 orientation, bool mirrored, ObjectGuid* outRoomGuid, int32 gridX, int32 gridY, int32 floorIndex)
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
    {
        TC_LOG_ERROR("housing", "PlaceRoom: rejected entry {} - count limit ({}/{})",
            roomEntryId, uint32(_rooms.size()), MAX_HOUSING_ROOMS_PER_HOUSE);
        return HOUSING_RESULT_GENERIC_FAILURE;
    }

    // Check WeightCost-based room budget
    uint32 roomWeightCost = GetRoomWeightCost(roomEntryId, gridX, gridY, floorIndex, ObjectGuid::Empty);
    if (_roomWeightUsed + roomWeightCost > GetMaxRoomBudget())
    {
        TC_LOG_ERROR("housing", "PlaceRoom: rejected entry {} - weight budget exceeded (used={} + cost={} > max={})",
            roomEntryId, _roomWeightUsed, roomWeightCost, GetMaxRoomBudget());
        return HOUSING_RESULT_GENERIC_FAILURE;
    }

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
    // arg2=roomEntryId matches retail GUID format (sniff-verified: arg2=HouseRoomID)
    ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, 0, roomEntryId, newDbId);

    Room& room = _rooms[roomGuid];
    room.Guid = roomGuid;
    room.RoomEntryId = roomEntryId;
    room.SlotIndex = slotIndex;
    room.GridX = gridX;
    room.GridY = gridY;
    room.FloorIndex = floorIndex;
    room.Orientation = orientation;
    room.Mirrored = mirrored;
    room.ThemeId = 0;

    if (outRoomGuid)
        *outRoomGuid = roomGuid;

    // Update room budget tracking
    _roomWeightUsed += roomWeightCost;

    TC_LOG_DEBUG("housing", "Housing::PlaceRoom: Player {} placed room entry {} at slot {} in house {} (room budget {}/{})",
        _owner->GetName(), roomEntryId, slotIndex, _houseGuid.ToString(),
        _roomWeightUsed, GetMaxRoomBudget());

    // Account-level notification: room collection update
    if (_owner->GetSession())
    {
        WorldPackets::Housing::AccountRoomCollectionUpdate notif;
        notif.AddSingle(roomEntryId);
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

    // Verify remaining rooms stay connected after removal (BFS from base room) - before touching its decor
    if (!IsRoomGraphConnectedWithout(roomGuid))
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

    // Auto-remove any placed decor in this room (return to catalog)
    std::vector<ObjectGuid> decorToRemove;
    for (auto const& [guid, decor] : _placedDecor)
    {
        if (decor.RoomGuid == roomGuid)
            decorToRemove.push_back(guid);
    }
    for (ObjectGuid const& decorGuid : decorToRemove)
    {
        TC_LOG_DEBUG("housing", "Housing::RemoveRoom: Auto-removing decor {} from room {} before deletion",
            decorGuid.ToString(), roomGuid.ToString());
        RemoveDecor(decorGuid);
    }

    // Refund room WeightCost budget
    uint32 roomWeightCost = GetRoomWeightCost(itr->second.RoomEntryId, itr->second.GridX, itr->second.GridY, itr->second.FloorIndex, roomGuid);
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
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    if (!roomData || roomData->IsBaseRoom())
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

    // Retail (12.1.0.69933 sniffs) turns a room about its own centre, a quarter per step (clockwise = negative
    // yaw), skipping headings where any room it is attached to would lose its door - a square room between two
    // others just turns, an L room hanging off one door jumps to the next heading that still meets it.
    std::vector<Room const*> rooms = GetRooms();
    Room const* partner = FindStairwellPartner(room);
    ObjectGuid const partnerGuid = partner ? partner->Guid : ObjectGuid::Empty;

    auto attachedRooms = [](Room const& r, std::vector<Room const*> const& layout)
    {
        std::vector<ObjectGuid> attached;
        for (RoomDoor const& door : GetRoomDoors(r))
            if (!door.IsVertical())
                if (Room const* other = FindRoomAtDoor(layout, r, door))
                    attached.push_back(other->Guid);
        return attached;
    };
    auto keepsAll = [](std::vector<ObjectGuid> const& before, std::vector<ObjectGuid> const& after)
    {
        return std::all_of(before.begin(), before.end(), [&](ObjectGuid const& guid) { return std::find(after.begin(), after.end(), guid) != after.end(); });
    };

    std::vector<ObjectGuid> const attachedBefore = attachedRooms(room, rooms);
    std::vector<ObjectGuid> const partnerAttachedBefore = partner ? attachedRooms(*partner, rooms) : std::vector<ObjectGuid>();

    int32 const gridX = room.GridX;
    int32 const gridY = room.GridY;
    uint32 orientation = room.Orientation;
    bool placed = false;
    for (uint32 step = 1; step < 4 && !placed; ++step)
    {
        uint32 const candidate = clockwise ? (room.Orientation + 4 - step) % 4 : (room.Orientation + step) % 4;
        if (!RoomFits(rooms, room.RoomEntryId, gridX, gridY, room.FloorIndex, candidate, roomGuid))
            continue;
        if (partner && !RoomFits(rooms, partner->RoomEntryId, gridX, gridY, partner->FloorIndex, candidate, partnerGuid))
            continue;

        Room turned = room;
        turned.Orientation = candidate;
        Room turnedPartner = partner ? *partner : Room();
        turnedPartner.Orientation = candidate;

        std::vector<Room const*> layout;
        for (Room const* r : rooms)
            layout.push_back(r->Guid == roomGuid ? &turned : (partner && r->Guid == partnerGuid ? &turnedPartner : r));

        if (!keepsAll(attachedBefore, attachedRooms(turned, layout)))
            continue;
        if (partner && !keepsAll(partnerAttachedBefore, attachedRooms(turnedPartner, layout)))
            continue;

        orientation = candidate;
        placed = true;
    }

    if (!placed)
        return HOUSING_RESULT_ROOM_UPDATE_FAILED;

    SetRoomPlacement(room, gridX, gridY, orientation);
    if (partner)
        SetRoomPlacement(_rooms[partnerGuid], gridX, gridY, orientation);

    TC_LOG_DEBUG("housing", "Housing::RotateRoom: Player {} rotated room {} to orientation {} at ({}, {}) in house {}",
        _owner->GetName(), roomGuid.ToString(), room.Orientation, room.GridX, room.GridY, _houseGuid.ToString());

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

void Housing::SetRoomPlacement(Room& room, int32 gridX, int32 gridY, uint32 orientation)
{
    constexpr float QUARTER_TURN = 1.57079632679f;
    int32 const oldX = room.GridX;
    int32 const oldY = room.GridY;
    float const turn = float(int32(orientation) - int32(room.Orientation)) * QUARTER_TURN;

    room.GridX = gridX;
    room.GridY = gridY;
    room.Orientation = orientation;
    PersistRoomToDB(room.Guid, room);

    // Placed decor is stored in interior world space; it rides along with its room (the client already moves it,
    // it hangs off the room entity).
    NeighborhoodMapData const* interior = sHousingMgr.GetNeighborhoodMapDataForWorldMap(HOUSE_INTERIOR_MAP_ID);
    if (!interior)
        return;

    float const cosT = std::cos(turn);
    float const sinT = std::sin(turn);
    float const cz = std::cos(turn / 2.0f);
    float const sz = std::sin(turn / 2.0f);
    float const fromX = interior->Origin[0] + float(oldX);
    float const fromY = interior->Origin[1] + float(oldY);
    float const toX = interior->Origin[0] + float(gridX);
    float const toY = interior->Origin[1] + float(gridY);

    for (auto& [decorGuid, decor] : _placedDecor)
    {
        if (decor.RoomGuid != room.Guid)
            continue;

        float const dx = decor.PosX - fromX;
        float const dy = decor.PosY - fromY;
        decor.PosX = toX + dx * cosT - dy * sinT;
        decor.PosY = toY + dx * sinT + dy * cosT;

        // q' = rotZ(turn) * q
        float const qx = decor.RotationX, qy = decor.RotationY, qz = decor.RotationZ, qw = decor.RotationW;
        decor.RotationX = cz * qx - sz * qy;
        decor.RotationY = cz * qy + sz * qx;
        decor.RotationZ = cz * qz + sz * qw;
        decor.RotationW = cz * qw - sz * qz;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_POSITION);
        stmt->setFloat(0, decor.PosX);
        stmt->setFloat(1, decor.PosY);
        stmt->setFloat(2, decor.PosZ);
        stmt->setFloat(3, decor.RotationX);
        stmt->setFloat(4, decor.RotationY);
        stmt->setFloat(5, decor.RotationZ);
        stmt->setFloat(6, decor.RotationW);
        stmt->setFloat(7, decor.Scale);
        stmt->setUInt64(8, _owner->GetGUID().GetCounter());
        stmt->setUInt64(9, decorGuid.GetCounter());
        CharacterDatabase.Execute(stmt);
    }
}

void Housing::RotateRoomOffset(float x, float y, uint32 orientation, float& outX, float& outY)
{
    // Exact quarter turns, counter-clockwise (same sense as the room entity's yaw).
    switch (orientation % 4)
    {
        case 0: outX = x;  outY = y;  break;
        case 1: outX = -y; outY = x;  break;
        case 2: outX = -x; outY = -y; break;
        default: outX = y; outY = -x; break;
    }
}

std::vector<Housing::RoomDoor> Housing::GetRoomDoors(uint32 roomEntryId, float gridX, float gridY, uint32 orientation)
{
    std::vector<RoomDoor> doors;
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(roomEntryId);
    std::vector<RoomComponentData> const* components = roomData ? sHousingMgr.GetRoomComponents(roomData->RoomWmoDataID) : nullptr;
    if (!components)
        return doors;

    for (RoomComponentData const& comp : *components)
    {
        if (comp.ConnectionType == 0)
            continue;

        // Retail FHousingRoomData.Doors: connectable walls with exactly one horizontal offset axis, and for a
        // stairwell also its connectable floor and ceiling (the link between its two stacked halves).
        bool const alongX = std::abs(comp.OffsetPos[0]) > 0.5f;
        bool const alongY = std::abs(comp.OffsetPos[1]) > 0.5f;
        int32 dirZ = 0;
        if (comp.Type == HOUSING_ROOM_COMPONENT_WALL)
        {
            if (alongX == alongY)
                continue;
        }
        else if (roomData->HasStairs() && (comp.Type == HOUSING_ROOM_COMPONENT_FLOOR || comp.Type == HOUSING_ROOM_COMPONENT_CEILING))
            dirZ = comp.Type == HOUSING_ROOM_COMPONENT_CEILING ? 1 : -1;
        else
            continue;

        RoomDoor& door = doors.emplace_back();
        door.ComponentId = comp.ID;
        door.ComponentType = comp.Type;
        door.Local.Relocate(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2]);

        float offsetX, offsetY;
        RotateRoomOffset(comp.OffsetPos[0], comp.OffsetPos[1], orientation, offsetX, offsetY);
        door.X = gridX + offsetX;
        door.Y = gridY + offsetY;

        if (dirZ)
        {
            door.DirZ = dirZ;
            continue;
        }

        float dirX, dirY;
        RotateRoomOffset(alongX ? (comp.OffsetPos[0] > 0.0f ? 1.0f : -1.0f) : 0.0f,
            alongY ? (comp.OffsetPos[1] > 0.0f ? 1.0f : -1.0f) : 0.0f, orientation, dirX, dirY);
        door.DirX = int32(dirX);
        door.DirY = int32(dirY);
    }

    std::sort(doors.begin(), doors.end(), [](RoomDoor const& a, RoomDoor const& b) { return a.ComponentId < b.ComponentId; });
    return doors;
}

Housing::Room const* Housing::FindRoomAtDoor(std::vector<Room const*> const& rooms, Room const& room, RoomDoor const& door, uint32* outComponentId /*= nullptr*/)
{
    constexpr float TOLERANCE = 0.6f;
    for (Room const* other : rooms)
    {
        if (!other || other->Guid == room.Guid)
            continue;

        // Walls meet on one floor; a stairwell's ceiling meets the floor of the half stacked right above it.
        if (door.IsVertical())
        {
            if (other->FloorIndex != room.FloorIndex + door.DirZ || other->GridX != room.GridX || other->GridY != room.GridY)
                continue;
        }
        else if (other->FloorIndex != room.FloorIndex)
            continue;

        for (RoomDoor const& otherDoor : GetRoomDoors(*other))
        {
            if (otherDoor.DirX != -door.DirX || otherDoor.DirY != -door.DirY || otherDoor.DirZ != -door.DirZ)
                continue;

            if (std::abs(otherDoor.X - door.X) > TOLERANCE || std::abs(otherDoor.Y - door.Y) > TOLERANCE)
                continue;

            if (outComponentId)
                *outComponentId = otherDoor.ComponentId;
            return other;
        }
    }

    return nullptr;
}

bool Housing::OwnsDoorway(Room const& room, Room const& other)
{
    // Sniffed layout edits (12.1.0.69933): the room placed earlier (lower slot) carries the doorway piece, the entry
    // hall included - a square room added on the entry got 739 on the entry side and only its wall (373) itself.
    return room.SlotIndex < other.SlotIndex;
}

uint8 Housing::GetDoorwayVariant(Room const& room, uint32 componentId, Room const& other, uint32 otherComponentId)
{
    // SET_DOOR_TYPE writes both sides of a connection; older rows may carry it on one side only.
    auto itr = room.DoorTypes.find(componentId);
    if (itr != room.DoorTypes.end() && itr->second != 0)
        return itr->second;
    itr = other.DoorTypes.find(otherComponentId);
    if (itr != other.DoorTypes.end() && itr->second != 0)
        return itr->second;
    return 2;
}

std::string Housing::SerializeDoorTypes(Room const& room)
{
    std::string result;
    for (auto const& [componentId, variant] : room.DoorTypes)
    {
        if (!result.empty())
            result += ',';
        result += Trinity::StringFormat("{}:{}", componentId, uint32(variant));
    }
    return result;
}

void Housing::LoadDoorTypes(Room& room, std::string const& doorTypes)
{
    room.DoorTypes.clear();
    for (std::string_view entry : Trinity::Tokenize(doorTypes, ',', false))
    {
        std::vector<std::string_view> parts = Trinity::Tokenize(entry, ':', false);
        if (parts.size() != 2)
            continue;

        Optional<uint32> componentId = Trinity::StringTo<uint32>(parts[0]);
        Optional<uint32> variant = Trinity::StringTo<uint32>(parts[1]);
        if (componentId && variant && *variant)
            room.DoorTypes[*componentId] = uint8(*variant);
    }

    // Rows written before per-door styles: the single (component, variant) pair
    if (room.DoorTypeId && room.DoorSlot && !room.DoorTypes.contains(room.DoorTypeId))
        room.DoorTypes[room.DoorTypeId] = room.DoorSlot;
}

ObjectGuid Housing::MakeHouseGuid(uint32 neighborhoodMapId, uint32 bnetAccountId)
{
    return ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 3, /*arg1*/ neighborhoodMapId, /*arg2*/ 7, uint64(bnetAccountId));
}

std::string Housing::SerializeComponentStyles(Room const& room)
{
    // "componentId:themeId:textureId,..." - 0 where the slot keeps the surface default
    std::map<uint32, std::pair<uint32, uint32>> styles;
    for (auto const& [componentId, themeId] : room.ComponentThemes)
        styles[componentId].first = themeId;
    for (auto const& [componentId, textureId] : room.ComponentTextures)
        styles[componentId].second = textureId;

    std::string result;
    for (auto const& [componentId, style] : styles)
    {
        if (!result.empty())
            result += ',';
        result += Trinity::StringFormat("{}:{}:{}", componentId, style.first, style.second);
    }
    return result;
}

void Housing::LoadComponentStyles(Room& room, std::string const& componentStyles)
{
    room.ComponentThemes.clear();
    room.ComponentTextures.clear();
    for (std::string_view entry : Trinity::Tokenize(componentStyles, ',', false))
    {
        std::vector<std::string_view> parts = Trinity::Tokenize(entry, ':', false);
        if (parts.size() != 3)
            continue;

        Optional<uint32> componentId = Trinity::StringTo<uint32>(parts[0]);
        Optional<uint32> themeId = Trinity::StringTo<uint32>(parts[1]);
        Optional<uint32> textureId = Trinity::StringTo<uint32>(parts[2]);
        if (!componentId)
            continue;
        if (themeId && *themeId)
            room.ComponentThemes[*componentId] = *themeId;
        if (textureId && *textureId)
            room.ComponentTextures[*componentId] = *textureId;
    }
}

bool Housing::RoomFits(std::vector<Room const*> const& rooms, uint32 roomEntryId, int32 gridX, int32 gridY, int32 floorIndex,
    uint32 orientation, ObjectGuid ignoreRoom /*= ObjectGuid::Empty*/)
{
    auto getBox = [](uint32 entryId, int32 x, int32 y, uint32 turn, float& minX, float& minY, float& maxX, float& maxY)
    {
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(entryId);
        RoomWmoDataEntry const* bounds = roomData && roomData->RoomWmoDataID ? sRoomWmoDataStore.LookupEntry(roomData->RoomWmoDataID) : nullptr;
        if (!bounds)
            return false;

        float ax, ay, bx, by;
        RotateRoomOffset(bounds->BoundingBoxMinX, bounds->BoundingBoxMinY, turn, ax, ay);
        RotateRoomOffset(bounds->BoundingBoxMaxX, bounds->BoundingBoxMaxY, turn, bx, by);
        minX = x + std::min(ax, bx);
        maxX = x + std::max(ax, bx);
        minY = y + std::min(ay, by);
        maxY = y + std::max(ay, by);
        return true;
    };

    float minX, minY, maxX, maxY;
    if (!getBox(roomEntryId, gridX, gridY, orientation, minX, minY, maxX, maxY))
        return true;

    // Neighbouring walls stand on (almost) the same line.
    constexpr float TOLERANCE = 0.5f;
    for (Room const* other : rooms)
    {
        if (!other || other->Guid == ignoreRoom || other->FloorIndex != floorIndex)
            continue;

        float otherMinX, otherMinY, otherMaxX, otherMaxY;
        if (!getBox(other->RoomEntryId, other->GridX, other->GridY, other->Orientation, otherMinX, otherMinY, otherMaxX, otherMaxY))
            continue;

        if (minX < otherMaxX - TOLERANCE && maxX > otherMinX + TOLERANCE && minY < otherMaxY - TOLERANCE && maxY > otherMinY + TOLERANCE)
            return false;
    }

    return true;
}

bool Housing::FitRoomToDoor(std::vector<Room const*> const& rooms, uint32 roomEntryId, int32 floorIndex, RoomDoor const& target,
    uint32 orientation, ObjectGuid ignoreRoom, int32& gridX, int32& gridY)
{
    if (target.IsVertical())
        return false;

    for (RoomDoor const& door : GetRoomDoors(roomEntryId, 0.0f, 0.0f, orientation))
    {
        if (door.IsVertical() || door.DirX != -target.DirX || door.DirY != -target.DirY)
            continue;

        int32 const x = int32(std::lround(target.X - door.X));
        int32 const y = int32(std::lround(target.Y - door.Y));
        if (!RoomFits(rooms, roomEntryId, x, y, floorIndex, orientation, ignoreRoom))
            continue;

        gridX = x;
        gridY = y;
        return true;
    }

    return false;
}

uint32 Housing::GetRoomWeightCost(uint32 roomEntryId, int32 gridX, int32 gridY, int32 floorIndex, ObjectGuid self) const
{
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(roomEntryId);
    if (roomData && roomData->HasStairs())
    {
        for (auto const& [guid, other] : _rooms)
        {
            if (guid == self || other.GridX != gridX || other.GridY != gridY || other.FloorIndex != floorIndex - 1)
                continue;

            HouseRoomData const* otherData = sHousingMgr.GetHouseRoomData(other.RoomEntryId);
            if (otherData && otherData->HasStairs())
                return 0;
        }
    }

    return sHousingMgr.GetRoomWeightCost(roomEntryId);
}

Housing::Room const* Housing::FindStairwellPartner(Room const& room) const
{
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    if (!roomData || !roomData->HasStairs())
        return nullptr;

    for (auto const& [guid, other] : _rooms)
    {
        if (guid == room.Guid || other.GridX != room.GridX || other.GridY != room.GridY || std::abs(other.FloorIndex - room.FloorIndex) != 1)
            continue;

        HouseRoomData const* otherData = sHousingMgr.GetHouseRoomData(other.RoomEntryId);
        if (otherData && otherData->HasStairs())
            return &other;
    }

    return nullptr;
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

    // Remaining rooms (the stairwell half stacked on the removed room goes with it)
    std::vector<Room const*> remaining;
    Room const* excluded = GetRoom(excludeRoomGuid);
    Room const* excludedPartner = excluded ? FindStairwellPartner(*excluded) : nullptr;
    for (auto const& [guid, room] : _rooms)
        if (guid != excludeRoomGuid && (!excludedPartner || guid != excludedPartner->Guid))
            remaining.push_back(&room);

    // BFS from the base room through rooms whose doors meet, plus the two halves of each stairwell
    std::unordered_set<ObjectGuid> visited;
    std::queue<Room const*> queue;

    visited.insert(baseRoomGuid);
    queue.push(GetRoom(baseRoomGuid));

    while (!queue.empty())
    {
        Room const* current = queue.front();
        queue.pop();
        if (!current)
            continue;

        std::vector<Room const*> neighbours;
        for (RoomDoor const& door : GetRoomDoors(*current))
            if (Room const* other = FindRoomAtDoor(remaining, *current, door))
                neighbours.push_back(other);
        if (Room const* partner = FindStairwellPartner(*current))
            neighbours.push_back(partner);

        for (Room const* neighbour : neighbours)
            if (std::find(remaining.begin(), remaining.end(), neighbour) != remaining.end() && visited.insert(neighbour->Guid).second)
                queue.push(neighbour);
    }

    // All remaining rooms must be reachable from the base room
    return visited.size() == remaining.size();
}

HousingResult Housing::ApplyRoomTheme(ObjectGuid roomGuid, uint32 themeSetId, std::vector<uint32> const& optionIds)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // Only the named slots change (12.1.0.69933 sniff: a ceiling restyle recreates just that ceiling piece).
    for (uint32 componentId : optionIds)
        itr->second.ComponentThemes[componentId] = themeSetId;

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomTheme: Player {} applied theme {} to room {} ({} options) in house {}",
        _owner->GetName(), themeSetId, roomGuid.ToString(), optionIds.size(), _houseGuid.ToString());

    // Account-level notification: theme collection update
    if (_owner->GetSession())
    {
        WorldPackets::Housing::AccountRoomThemeCollectionUpdate notif;
        notif.AddSingle(themeSetId);
        _owner->GetSession()->SendPacket(notif.Write());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::ApplyRoomMaterial(ObjectGuid roomGuid, uint32 textureId, int32 colorOverride, std::vector<uint32> const& optionIds)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return HOUSING_RESULT_ROOM_NOT_FOUND;

    // The client names RoomComponent DB2 IDs (not RoomComponentOption IDs); only those slots change.
    for (uint32 componentId : optionIds)
        itr->second.ComponentTextures[componentId] = textureId;

    itr->second.ColorOverride = colorOverride;

    PersistRoomToDB(roomGuid, itr->second);

    TC_LOG_DEBUG("housing", "Housing::ApplyRoomMaterial: Player {} applied texture {} (color {}) to room {} "
        "({} components) in house {}",
        _owner->GetName(), textureId, colorOverride, roomGuid.ToString(), optionIds.size(), _houseGuid.ToString());

    // Account-level notification: material collection update
    if (_owner->GetSession())
    {
        WorldPackets::Housing::AccountRoomMaterialCollectionUpdate notif;
        notif.AddSingle(textureId);
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

    // doorTypeId is the door's RoomComponent ID, doorSlot the doorway variant. One doorway is shared by the two
    // rooms it joins, so the style goes to both sides.
    auto setSide = [this](Room& room, uint32 componentId, uint8 variant)
    {
        room.DoorTypeId = componentId;
        room.DoorSlot = variant;
        room.DoorTypes[componentId] = variant;
        PersistRoomToDB(room.Guid, room);
    };

    Room& room = itr->second;
    setSide(room, doorTypeId, doorSlot);

    std::vector<Room const*> rooms = GetRooms();
    for (RoomDoor const& door : GetRoomDoors(room))
    {
        if (door.ComponentId != doorTypeId || door.IsVertical())
            continue;

        uint32 otherComponentId = 0;
        if (Room const* other = FindRoomAtDoor(rooms, room, door, &otherComponentId))
            setSide(_rooms[other->Guid], otherComponentId, doorSlot);
        break;
    }

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

void Housing::MoveHookFixtures(uint32 oldCompId, uint32 newCompId)
{
    auto const* oldHooks = sHousingMgr.GetHooksOnComponent(oldCompId);
    auto const* newHooks = sHousingMgr.GetHooksOnComponent(newCompId);
    if (!oldHooks || !newHooks)
        return;

    // Both roots share the house frame, so a fixture goes to the free hook of the same fixture type closest to
    // where it hung before (hook order differs between base styles: rank matching put the door on another wall).
    std::vector<std::pair<uint32, uint32>> moves; // old hook -> new hook
    std::unordered_set<uint32> taken;
    for (ExteriorComponentHookEntry const* oldHook : *oldHooks)
    {
        if (!oldHook || !_fixtures.count(oldHook->ID))
            continue;

        ExteriorComponentHookEntry const* best = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        for (ExteriorComponentHookEntry const* newHook : *newHooks)
        {
            if (!newHook || newHook->ExteriorComponentTypeID != oldHook->ExteriorComponentTypeID || taken.count(newHook->ID))
                continue;
            if (newHook->ID != oldHook->ID && _fixtures.count(newHook->ID))
                continue;

            float const dx = newHook->Position[0] - oldHook->Position[0];
            float const dy = newHook->Position[1] - oldHook->Position[1];
            float const dz = newHook->Position[2] - oldHook->Position[2];
            float const dist = dx * dx + dy * dy + dz * dz;
            if (dist < bestDist)
            {
                bestDist = dist;
                best = newHook;
            }
        }

        if (!best)
            continue;
        taken.insert(best->ID);
        if (best->ID != oldHook->ID)
            moves.emplace_back(oldHook->ID, best->ID);
    }

    uint64 const ownerGuid = _owner->GetGUID().GetCounter();
    for (auto const& [oldHookId, newHookId] : moves)
    {
        uint32 const optionId = _fixtures[oldHookId].OptionId;
        _fixtures.erase(oldHookId);
        Fixture& moved = _fixtures[newHookId];
        moved.FixturePointId = newHookId;
        moved.OptionId = optionId;

        CharacterDatabasePreparedStatement* del = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
        del->setUInt64(0, ownerGuid);
        del->setUInt32(1, oldHookId);
        CharacterDatabase.Execute(del);

        CharacterDatabasePreparedStatement* ins = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        ins->setUInt64(0, ownerGuid);
        ins->setUInt32(1, newHookId);
        ins->setUInt32(2, optionId);
        CharacterDatabase.Execute(ins);

        TC_LOG_DEBUG("housing", "Housing::MoveHookFixtures: fixture comp {} moved from hook {} (comp {}) to hook {} (comp {})",
            optionId, oldHookId, oldCompId, newHookId, newCompId);
    }
}

HousingResult Housing::SelectFixtureOption(uint32 fixturePointId, uint32 optionId, std::vector<uint32>* removedHookIDs /*= nullptr*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Root fixture selections (optionId == 0) use componentID as fixturePointId — skip hook validation
    if (optionId != 0)
    {
        // Validate hook exists in DB2
        ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(fixturePointId);
        if (!hookEntry)
        {
            TC_LOG_DEBUG("housing", "SelectFixtureOption: hookID {} not found in DB2", fixturePointId);
            return HOUSING_RESULT_FIXTURE_NOT_FOUND;
        }

        // Validate component exists in DB2
        ExteriorComponentEntry const* compEntry = sExteriorComponentStore.LookupEntry(optionId);
        if (!compEntry)
        {
            TC_LOG_DEBUG("housing", "SelectFixtureOption: componentID {} not found in DB2", optionId);
            return HOUSING_RESULT_FIXTURE_NOT_FOUND;
        }

        // The hook must belong to the current house type (hooks are unique per root, so this also bounds the count).
        if (!IsCurrentTypeComponent(sExteriorComponentStore.LookupEntry(hookEntry->ExteriorComponentID)))
        {
            TC_LOG_DEBUG("housing", "SelectFixtureOption: hook {} is on comp {}, not part of house type {}",
                fixturePointId, hookEntry->ExteriorComponentID, _houseType);
            return HOUSING_RESULT_FIXTURE_NOT_FOUND;
        }

        // Validate component type matches hook's expected type
        if (compEntry->Type != hookEntry->ExteriorComponentTypeID)
        {
            TC_LOG_DEBUG("housing", "SelectFixtureOption: type mismatch — component {} type {} vs hook {} expected type {}",
                optionId, compEntry->Type, fixturePointId, hookEntry->ExteriorComponentTypeID);
            return HOUSING_RESULT_GENERIC_FAILURE;
        }

        // Enforce one door (entrance) per house: if placing a door, remove any existing door at other hooks.
        // Also remove any existing fixture at the TARGET hook (only one fixture per hook).
        std::vector<uint32> conflictHooks;

        if (compEntry->Type == HOUSING_FIXTURE_TYPE_DOOR)
        {
            // Doors of other house types stay: they belong to those types' stored roots.
            uint32 const hookWmo = GetHookOwnerWmo(fixturePointId);
            for (auto const& [pointId, fixture] : _fixtures)
            {
                if (pointId == fixturePointId || fixture.OptionId == 0 || GetHookOwnerWmo(pointId) != hookWmo)
                    continue;
                ExteriorComponentEntry const* existingComp = sExteriorComponentStore.LookupEntry(fixture.OptionId);
                if (existingComp && existingComp->Type == HOUSING_FIXTURE_TYPE_DOOR)
                {
                    TC_LOG_INFO("housing", "SelectFixtureOption: replacing existing door at hook {} (comp {}) — moving entrance to hook {}",
                        pointId, fixture.OptionId, fixturePointId);
                    conflictHooks.push_back(pointId);
                }
            }
        }

        // If the target hook already has a different fixture, remove it
        auto existingAtHook = _fixtures.find(fixturePointId);
        if (existingAtHook != _fixtures.end() && existingAtHook->second.OptionId != 0
            && existingAtHook->second.OptionId != optionId)
        {
            TC_LOG_INFO("housing", "SelectFixtureOption: removing existing fixture (comp {}) at hook {} to place new comp {}",
                existingAtHook->second.OptionId, fixturePointId, optionId);
            conflictHooks.push_back(fixturePointId);
        }

        // Remove all conflicting fixtures (data + DB)
        for (uint32 conflictHookId : conflictHooks)
        {
            _fixtures.erase(conflictHookId);
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
            stmt->setUInt64(0, _owner->GetGUID().GetCounter());
            stmt->setUInt32(1, conflictHookId);
            CharacterDatabase.Execute(stmt);
            if (_fixtureWeightUsed > 0)
                --_fixtureWeightUsed;

            // Signal the removed hookID to the caller for mesh despawning
            if (removedHookIDs)
                removedHookIDs->push_back(conflictHookId);
        }
    }
    else
    {
        // Root fixture (optionId == 0): fixturePointId is a componentID.
        // Remove any existing root fixture of the SAME type to prevent accumulation.
        // E.g., switching Base from Stucco(142) to Cottage(3797) must remove the old 142 entry.
        ExteriorComponentEntry const* newComp = sExteriorComponentStore.LookupEntry(fixturePointId);
        if (newComp)
        {
            uint8 newType = newComp->Type;
            std::vector<uint32> toRemove;
            for (auto const& [pointId, fixture] : _fixtures)
            {
                if (fixture.OptionId != 0 || pointId == fixturePointId)
                    continue;
                ExteriorComponentEntry const* oldComp = sExteriorComponentStore.LookupEntry(fixture.FixturePointId);
                // Roots of other house types are kept: a human base choice deleted the stored elf base, and switching
                // back to elf fell back to the plain default base, its fixtures dragged onto human hooks.
                if (oldComp && oldComp->Type == newType && oldComp->HouseExteriorWmoDataID == newComp->HouseExteriorWmoDataID)
                {
                    TC_LOG_INFO("housing", "SelectFixtureOption: replacing root type {} — removing old comp {} in favor of new comp {}",
                        newType, pointId, fixturePointId);
                    toRemove.push_back(pointId);
                }
            }
            for (uint32 oldKey : toRemove)
            {
                _fixtures.erase(oldKey);
                // Delete old entry from DB
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
                stmt->setUInt64(0, _owner->GetGUID().GetCounter());
                stmt->setUInt32(1, oldKey);
                CharacterDatabase.Execute(stmt);
                if (_fixtureWeightUsed > 0)
                    --_fixtureWeightUsed;

                // Fixtures (door, windows, ...) are keyed by the hooks of the component they hang on, and every
                // root has its own hook IDs: left on the old root's hooks they vanished with it (a new base style
                // lost its entrance). Move each to the new root's hook of the same type and rank.
                MoveHookFixtures(oldKey, fixturePointId);
            }
        }
    }

    bool isNew = _fixtures.find(fixturePointId) == _fixtures.end();

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

    // Account-level notification: fixture collection update
    if (isNew && _owner->GetSession())
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate notif;
        notif.AddSingle(optionId);
        _owner->GetSession()->SendPacket(notif.Write());
    }

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::RemoveFixture(uint32 componentID, uint32* outHookID /*= nullptr*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Try direct key lookup first (covers core fixtures where key == componentID)
    auto itr = _fixtures.find(componentID);

    // If not found by key, search by OptionId (for hook-based fixtures, key=hookID, OptionId=componentID)
    if (itr == _fixtures.end())
    {
        for (auto it = _fixtures.begin(); it != _fixtures.end(); ++it)
        {
            if (it->second.OptionId == componentID)
            {
                itr = it;
                break;
            }
        }
    }

    if (itr == _fixtures.end())
        return HOUSING_RESULT_FIXTURE_NOT_FOUND;

    uint32 hookID = itr->first; // the key is either hookID or componentID for core fixtures
    if (outHookID)
        *outHookID = hookID;

    _fixtures.erase(itr);

    // Immediate persist — delete single fixture from DB
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
        stmt->setUInt64(0, _owner->GetGUID().GetCounter());
        stmt->setUInt32(1, hookID);
        CharacterDatabase.Execute(stmt);
    }

    // Refund fixture budget
    uint32 const fixtureWeightCost = 1;
    if (_fixtureWeightUsed >= fixtureWeightCost)
        _fixtureWeightUsed -= fixtureWeightCost;

    TC_LOG_DEBUG("housing", "Housing::RemoveFixture: Player {} removed fixture {} (hook {}) in house {} (budget: {}/{})",
        _owner->GetName(), componentID, hookID, _houseGuid.ToString(),
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

std::unordered_map<uint32, uint32> Housing::GetFixtureOverrideMap() const
{
    // Build override map from player's hook-based fixture selections.
    // These are fixtures at hooks (doors, windows, etc.) where OptionId != 0.
    // Root overrides (base, roof variants) are handled separately via GetRootComponentOverrides().
    std::unordered_map<uint32, uint32> result;

    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId != 0)
            result[fixture.FixturePointId] = fixture.OptionId;
    }
    return result;
}

std::unordered_map<uint8, uint32> Housing::GetRootComponentOverrides() const
{
    // Build override map for player-selected root components per type.
    // Core fixtures (OptionId == 0) represent the player's choice for a structural root type.
    // These include both base variants (ParentComponentID == 0) and color/style variants
    // (ParentComponentID != 0) — color variants are valid selections via SetCoreFixture.
    std::unordered_map<uint8, uint32> result;

    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId != 0)
            continue;

        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fixture.FixturePointId);
        if (!comp)
        {
            TC_LOG_DEBUG("housing", "GetRootComponentOverrides: fixturePointId={} — DB2 lookup failed", fixture.FixturePointId);
            continue;
        }
        // Only structural root types (Base=9, Roof=10) are valid here.
        // Fixture types (Door=11, Window=12, etc.) stored with OptionId=0 would be invalid.
        if (comp->Type != HOUSING_FIXTURE_TYPE_BASE && comp->Type != HOUSING_FIXTURE_TYPE_ROOF)
        {
            TC_LOG_DEBUG("housing", "GetRootComponentOverrides: comp={} type={} — not a structural root type, skipping",
                comp->ID, comp->Type);
            continue;
        }
        if (_houseType != 0 && comp->HouseExteriorWmoDataID != static_cast<uint32>(_houseType))
        {
            TC_LOG_DEBUG("housing", "GetRootComponentOverrides: comp={} type={} — wmo={} != houseType={} (wrong style)",
                comp->ID, comp->Type, comp->HouseExteriorWmoDataID, _houseType);
            continue;
        }

        result[comp->Type] = fixture.FixturePointId;
        TC_LOG_DEBUG("housing", "GetRootComponentOverrides: type={} → comp={} (wmo={}, parentComp={})",
            comp->Type, fixture.FixturePointId, comp->HouseExteriorWmoDataID, comp->ParentComponentID);
    }

    TC_LOG_INFO("housing", "GetRootComponentOverrides: {} types resolved from {} fixtures (houseType={})",
        uint32(result.size()), uint32(_fixtures.size()), _houseType);
    return result;
}

uint32 Housing::GetCoreExteriorComponentID() const
{
    // The core fixture is the primary component set via SetCoreFixture (OptionId == 0).
    // It can be any root type — Base (9) for Alliance, or different types for Horde.
    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId == 0)
        {
            ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fixture.FixturePointId);
            // Only return this fixture if it matches the current house type's WMO data.
            // When the player switches house type, old fixtures from the previous type
            // must not override the new type's default component.
            if (comp && comp->ParentComponentID == 0 && (_houseType == 0 || comp->HouseExteriorWmoDataID == _houseType))
                return fixture.FixturePointId;
        }
    }
    // No explicit core fixture set — find first default root component for this house's WMO data ID.
    if (_houseType > 0)
    {
        auto const* roots = sHousingMgr.GetRootComponentsForWmoData(static_cast<uint32>(_houseType));
        if (roots)
        {
            uint32 fallbackComp = 0;
            for (uint32 compID : *roots)
            {
                ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(compID);
                if (!comp)
                    continue;
                if (!fallbackComp)
                    fallbackComp = compID;
                if (comp->Flags & 0x1) // IsDefault
                    return compID;
            }
            if (fallbackComp)
                return fallbackComp;
        }
        TC_LOG_ERROR("housing", "Housing::GetCoreExteriorComponentID: No root component found for houseType={}", _houseType);
    }
    else
    {
        TC_LOG_ERROR("housing", "Housing::GetCoreExteriorComponentID: No fixtures and houseType=0 — cannot determine base component");
    }
    return 0;
}

HousingResult Housing::AddToCatalog(uint32 decorEntryId, uint8 sourceType, std::string sourceValue)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // A decor entry the player has never owned before is a NEW unique collection entry
    // (CriteriaType::CollectUniqueDecor 272). The catalog is quantity-based, so "first time" is exactly
    // "no row existed before this add"; a second copy of the same entry must NOT count again.
    bool const firstTimeAcquired = !_catalog.contains(decorEntryId);

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

    // CriteriaType::CollectUniqueDecor (272) - counted once per distinct HouseDecor entry ever acquired.
    if (firstTimeAcquired)
        _owner->UpdateCriteria(CriteriaType::CollectUniqueDecor, decorEntryId);

    // SMSG_HOUSING_DECOR_ADD_TO_HOUSE_CHEST_RESPONSE (0x510008): retail emits this on EVERY
    // decor acquisition, carrying the GUID of the freshly minted decor instance.
    //
    // Capture-verified against the 12.0.7 (68275/68453) sniffs:
    //   "garrison and hall of class table quest.pkt" — SMSG_HOUSING_FIRST_TIME_DECOR_ACQUISITION
    //   (decorID 0x1E8E) is immediately followed by 0x510008 whose PackedGUID decodes to
    //   HighGuid::Housing with arg2 == 0x1E8E, i.e. the same decor entry.
    //   "garrisonlevel2upgrade.pkt" — two more 0x510008 with NO preceding FIRST_TIME packet
    //   (re-acquisition of already-known decor), which is why this lives here, at the
    //   acquisition choke point, and not next to the first-time notification.
    // Negative control: "housing12.0.7.pkt" carries 10 SMSG_HOUSING_DECOR_REMOVE_RESPONSE and
    //   zero 0x510008, so this packet is NOT the decor-removal / return-to-storage response.
    // Wire: uint8(0x80 = success) + uint32(count=1) + PackedGUID — all three captures parse to
    //   exactly the packet length with no trailing bytes.
    // GUID scheme matches PopulateCatalogStorageEntries()/EffectCollectHousingDecor():
    //   subType=1, arg1=realm, arg2=decorEntryId, counter=ownerBase + entry*100 + instanceIndex.
    if (_owner && _owner->GetSession())
    {
        uint64 uniqueId = _owner->GetGUID().GetCounter() * 100000 + uint64(decorEntryId) * 100
            + (entry.Count > 0 ? entry.Count - 1 : 0);

        WorldPackets::Housing::HousingDecorAddToHouseChestResponse chestResponse;
        chestResponse.Success = true;
        chestResponse.DecorGuids.push_back(ObjectGuid::Create<HighGuid::Housing>(
            /*subType*/ 1,
            /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
            /*arg2*/ decorEntryId,
            uniqueId));
        _owner->SendDirectMessage(chestResponse.Write());
    }

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

    // Broadcast level/favor update to the owner.
    // Wire format: header (Type, ChangeAmount, Reason, Count) + per-entry
    // (EntryFlags, EntryTimestamp, HouseGUID, NewFavorTotal, Reserved, Terminator).
    if (_owner && _owner->GetSession())
    {
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelUpdate;
        levelUpdate.Result = 0;
        levelUpdate.ChangeAmount = _favor;
        levelUpdate.Reason = _level;
        auto& fav = levelUpdate.Houses.emplace_back();
        fav.HouseGUID = _houseGuid;
        fav.HouseLevel = static_cast<int32>(_favor);
        _owner->SendDirectMessage(levelUpdate.Write());
    }
}

void Housing::AddFavor(uint64 amount, HousingFavorUpdateSource source /*= HOUSING_FAVOR_SOURCE_UNKNOWN*/, bool emitUpdate /*= true*/)
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

    // Broadcast level/favor update to the owner.
    // Type field carries the favor source enum (matches retail's "change reason" semantic).
    // Skipped when the caller emits its own (sniff-verified) packet sequence — e.g. BuyHouse.
    if (emitUpdate && _owner && _owner->GetSession())
    {
        WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor favorUpdate;
        favorUpdate.Result = static_cast<uint8>(source);
        favorUpdate.ChangeAmount = _favor;
        favorUpdate.Reason = _level;
        auto& fav = favorUpdate.Houses.emplace_back();
        fav.HouseGUID = _houseGuid;
        fav.HouseLevel = static_cast<int32>(_favor);
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

        // Broadcast level/favor update to the owner.
        // Wire format: header (Type, ChangeAmount, Reason, Count) + per-entry
        // (EntryFlags, EntryTimestamp, HouseGUID, NewFavorTotal, Reserved, Terminator).
        if (_owner && _owner->GetSession())
        {
            WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelUpdate;
            levelUpdate.Result = 0;
            levelUpdate.ChangeAmount = _favor;
            levelUpdate.Reason = _level;
            auto& fav = levelUpdate.Houses.emplace_back();
            fav.HouseGUID = _houseGuid;
            fav.HouseLevel = static_cast<int32>(_favor);
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

Position Housing::GetDecorPlacementAnchor(ObjectGuid roomGuid) const
{
    if (!IsExteriorDecorPlacement(roomGuid))
    {
        if (NeighborhoodMapData const* interior = sHousingMgr.GetNeighborhoodMapDataForWorldMap(HOUSE_INTERIOR_MAP_ID))
            return Position(interior->Origin[0], interior->Origin[1], interior->Origin[2]);
        return Position();
    }

    return _owner ? _owner->GetPosition() : Position();
}

HousingResult Housing::CheckInteriorDecorBounds(ObjectGuid roomGuid, float x, float y, float z) const
{
    if (IsExteriorDecorPlacement(roomGuid) || _rooms.empty())
        return HOUSING_RESULT_SUCCESS;

    NeighborhoodMapData const* interior = sHousingMgr.GetNeighborhoodMapDataForWorldMap(HOUSE_INTERIOR_MAP_ID);
    if (!interior)
        return HOUSING_RESULT_SUCCESS;

    // Room placement as in HouseInteriorMap::SpawnRoomMeshObjects: origin + grid offset, 12 yards per floor,
    // quarter-turn orientation. Any room counts, so an item may be moved into a neighbouring room; the
    // tolerance covers wall decor whose origin sits on the wall plane.
    constexpr float FLOOR_HEIGHT = 12.0f;
    constexpr float TOLERANCE = 0.5f;
    constexpr float QUARTER_TURN = 1.57079632679f;
    Position const decorPos(x, y, z);
    bool anyBounds = false;
    for (auto const& [guid, room] : _rooms)
    {
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
        RoomWmoDataEntry const* bounds = roomData && roomData->RoomWmoDataID
            ? sRoomWmoDataStore.LookupEntry(roomData->RoomWmoDataID) : nullptr;
        if (!bounds)
            continue;

        anyBounds = true;
        Position const roomPos(interior->Origin[0] + float(room.GridX), interior->Origin[1] + float(room.GridY),
            interior->Origin[2] + float(room.FloorIndex) * FLOOR_HEIGHT, float(room.Orientation) * QUARTER_TURN);
        Position const local = HousingWorldToRoomLocal(roomPos, decorPos);
        if (local.GetPositionX() >= bounds->BoundingBoxMinX - TOLERANCE && local.GetPositionX() <= bounds->BoundingBoxMaxX + TOLERANCE
            && local.GetPositionY() >= bounds->BoundingBoxMinY - TOLERANCE && local.GetPositionY() <= bounds->BoundingBoxMaxY + TOLERANCE
            && local.GetPositionZ() >= bounds->BoundingBoxMinZ - TOLERANCE && local.GetPositionZ() <= bounds->BoundingBoxMaxZ + TOLERANCE)
            return HOUSING_RESULT_SUCCESS;
    }

    return anyBounds ? HOUSING_RESULT_BOUNDS_FAILURE_ROOM : HOUSING_RESULT_SUCCESS;
}

bool Housing::IsExteriorDecorPlacement(ObjectGuid roomGuid)
{
    // No room → yard/exterior placement.
    if (roomGuid.IsEmpty())
        return true;

    // The plot's base (exterior) room identity: HighGuid::Housing, subType==2,
    // low 32 bits of the high word == the base room entry id. Retail always
    // sends this RoomGuid for exterior decor even though it is "on a room".
    return roomGuid.GetHigh() == HighGuid::Housing
        && uint32((roomGuid.GetRawValue(1) >> 53) & 0x1F) == 2
        && uint32(roomGuid.GetRawValue(1) & 0xFFFFFFFFULL) == sHousingMgr.GetBaseRoomEntryId();
}

HousingResult Housing::CheckLightOverlap(uint32 decorEntryId, float x, float y, float z,
    bool isExterior, ObjectGuid excludeGuid /*= ObjectGuid::Empty*/) const
{
    // A4 / 12.0.7 "two lights cannot overlap". The rule is scoped to the exterior
    // (outdoor-lighting) placement scope and only Lighting-category decor (cat 4)
    // participates — non-lights and interior placements pass through untouched so
    // ordinary decorating is never affected.
    if (!isExterior || !sHousingMgr.IsLightingDecor(decorEntryId))
        return HOUSING_RESULT_SUCCESS;

    float const radiusSq = HOUSING_LIGHT_OVERLAP_RADIUS * HOUSING_LIGHT_OVERLAP_RADIUS;
    for (auto const& [guid, decor] : _placedDecor)
    {
        if (guid == excludeGuid)
            continue;
        // Compare only against other EXTERIOR lights.
        if (!IsExteriorDecorPlacement(decor.RoomGuid))
            continue;
        if (!sHousingMgr.IsLightingDecor(decor.DecorEntryId))
            continue;

        float const dx = decor.PosX - x;
        float const dy = decor.PosY - y;
        float const dz = decor.PosZ - z;
        if ((dx * dx + dy * dy + dz * dz) < radiusSq)
            return HOUSING_RESULT_INVALID_LIGHT_OVERLAP;
    }
    return HOUSING_RESULT_SUCCESS;
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
        if (IsExteriorDecorPlacement(decor.RoomGuid))
            _exteriorDecorWeightUsed += weightCost;
        else
            _interiorDecorWeightUsed += weightCost;
    }

    // Sum WeightCost of all placed rooms
    for (auto const& [guid, room] : _rooms)
    {
        uint32 weightCost = GetRoomWeightCost(room.RoomEntryId, room.GridX, room.GridY, room.FloorIndex, guid);
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
    // The session's house entity is this character's house (retail: the Horde character gets its Horde house GUID)
    if (houseEntity.GetGUID() != _houseGuid)
        houseEntity.SetGuid(_houseGuid);
    houseEntity.SetBnetAccount(_owner->GetSession()->GetBattlenetAccountGUID());
    houseEntity.SetEntityGUID(_houseGuid);
    // HouseType and HouseSize are NOT part of this fragment (IDA-verified).
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

    TC_LOG_DEBUG("housing", "Housing::SyncUpdateFields: EntityGUID={} BnetAccount={} PlotIndex={} Level={} Favor={} Budgets=[{},{},{},{}]",
        _houseGuid.ToString(), _owner->GetSession()->GetBattlenetAccountGUID().ToString(),
        _plotIndex, _level, _favor64,
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

    // Mirror onto the in-memory neighborhood plot so visitor permission checks
    // (CanVisitorAccessPlot) work correctly when the owner is offline.
    if (Neighborhood* nbh = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        nbh->UpdatePlotSettingsFlags(_owner->GetGUID(), _settingsFlags);

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
    bool const changed = _houseType != typeId;
    _houseType = typeId;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_HOUSE_TYPE);
    stmt->setUInt32(0, typeId);
    stmt->setUInt64(1, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    // The previous type's roots and hook fixtures belong to other components; without starter fixtures for
    // the new type the rebuilt house had no roof selection and no entrance.
    if (changed)
        PopulateStarterFixtures();

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

    // Keep the neighborhood's mirror in step: it builds this house at map load when the owner is offline.
    if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        neighborhood->UpdatePlotHousePosition(_owner->GetGUID(), Position(x, y, z, facing));

    TC_LOG_DEBUG("housing", "Housing::SetHousePosition: Player {} positioned house at ({}, {}, {}, {}) in house {}",
        _owner->GetName(), x, y, z, facing, _houseGuid.ToString());
}

void Housing::ResetHousePosition()
{
    _hasCustomPosition = false;
    _housePosX = _housePosY = _housePosZ = _houseFacing = 0.0f;

    // All-zero coordinates load back as "no custom position" (Housing::LoadFromDB).
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_POSITION);
    stmt->setFloat(0, 0.0f);
    stmt->setFloat(1, 0.0f);
    stmt->setFloat(2, 0.0f);
    stmt->setFloat(3, 0.0f);
    stmt->setUInt64(4, _owner->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        neighborhood->UpdatePlotHousePosition(_owner->GetGUID(), {});
}

void Housing::RelocateExteriorDecor(Position const& fromFrame, Position const& toFrame)
{
    float const turn = toFrame.GetOrientation() - fromFrame.GetOrientation();
    float const cz = std::cos(turn / 2.0f);
    float const sz = std::sin(turn / 2.0f);
    float const cosTo = std::cos(toFrame.GetOrientation());
    float const sinTo = std::sin(toFrame.GetOrientation());

    uint32 moved = 0;
    for (auto& [decorGuid, decor] : _placedDecor)
    {
        if (!decor.RoomGuid.IsEmpty())
            continue; // interior

        Position const local = HousingWorldToRoomLocal(fromFrame, Position(decor.PosX, decor.PosY, decor.PosZ));
        decor.PosX = toFrame.GetPositionX() + local.GetPositionX() * cosTo - local.GetPositionY() * sinTo;
        decor.PosY = toFrame.GetPositionY() + local.GetPositionX() * sinTo + local.GetPositionY() * cosTo;
        decor.PosZ = toFrame.GetPositionZ() + local.GetPositionZ();

        // Turn the piece with the plot: q' = rotZ(turn) * q
        float const qx = decor.RotationX, qy = decor.RotationY, qz = decor.RotationZ, qw = decor.RotationW;
        decor.RotationX = cz * qx - sz * qy;
        decor.RotationY = cz * qy + sz * qx;
        decor.RotationZ = cz * qz + sz * qw;
        decor.RotationW = cz * qw - sz * qz;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_POSITION);
        stmt->setFloat(0, decor.PosX);
        stmt->setFloat(1, decor.PosY);
        stmt->setFloat(2, decor.PosZ);
        stmt->setFloat(3, decor.RotationX);
        stmt->setFloat(4, decor.RotationY);
        stmt->setFloat(5, decor.RotationZ);
        stmt->setFloat(6, decor.RotationW);
        stmt->setFloat(7, decor.Scale);
        stmt->setUInt64(8, _owner->GetGUID().GetCounter());
        stmt->setUInt64(9, decorGuid.GetCounter());
        CharacterDatabase.Execute(stmt);
        ++moved;
    }

    TC_LOG_DEBUG("housing", "Housing::RelocateExteriorDecor: moved {} exterior decor pieces for player {}", moved, _owner->GetName());
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
    stmt->setInt32(index++, room.GridX);
    stmt->setInt32(index++, room.GridY);
    stmt->setInt32(index++, room.FloorIndex);
    stmt->setUInt32(index++, room.Orientation);
    stmt->setUInt8(index++, room.Mirrored ? 1 : 0);
    stmt->setUInt32(index++, room.ThemeId);
    stmt->setUInt32(index++, room.WallTextureId);
    stmt->setUInt32(index++, room.FloorTextureId);
    stmt->setUInt32(index++, room.CeilingTextureId);
    stmt->setInt32(index++, room.ColorOverride);
    stmt->setUInt32(index++, room.DoorTypeId);
    stmt->setUInt8(index++, room.DoorSlot);
    stmt->setUInt32(index++, room.CeilingTypeId);
    stmt->setUInt8(index++, room.CeilingSlot);
    stmt->setUInt32(index++, room.WallThemeId);
    stmt->setUInt32(index++, room.FloorThemeId);
    stmt->setUInt32(index++, room.CeilingThemeId);
    stmt->setString(index++, SerializeDoorTypes(room));
    stmt->setString(index++, SerializeComponentStyles(room));
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

bool Housing::IsCurrentTypeComponent(ExteriorComponentEntry const* comp) const
{
    return comp && (_houseType == 0 || comp->HouseExteriorWmoDataID == 0
        || comp->HouseExteriorWmoDataID == static_cast<uint32>(_houseType));
}

bool Housing::HasCurrentTypeDoor() const
{
    for (auto const& [pointId, fix] : _fixtures)
    {
        if (fix.OptionId == 0)
            continue;
        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fix.OptionId);
        if (!comp || comp->Type != HOUSING_FIXTURE_TYPE_DOOR || !IsCurrentTypeComponent(comp))
            continue;

        // The door only exists if its hook is on the current house: a stored root of this type, or a child
        // component of this type.
        ExteriorComponentHookEntry const* hook = sExteriorComponentHookStore.LookupEntry(pointId);
        ExteriorComponentEntry const* owner = hook ? sExteriorComponentStore.LookupEntry(hook->ExteriorComponentID) : nullptr;
        if (!owner || !IsCurrentTypeComponent(owner))
            continue;
        if (owner->Type == HOUSING_FIXTURE_TYPE_BASE || owner->Type == HOUSING_FIXTURE_TYPE_ROOF)
        {
            bool const rootStored = std::any_of(_fixtures.begin(), _fixtures.end(), [owner](auto const& entry)
            {
                if (entry.second.OptionId != 0)
                    return false;
                if (entry.first == owner->ID)
                    return true;
                ExteriorComponentEntry const* root = sExteriorComponentStore.LookupEntry(entry.first);
                return root && static_cast<uint32>(root->ParentComponentID) == owner->ID;
            });
            if (!rootStored)
                continue;
        }
        return true;
    }
    return false;
}

void Housing::PopulateStarterFixtures()
{
    // Starter house = Base(9) + Roof(10) as root components.
    // Door(11) auto-resolves from hook system via GetDefaultFixtureForType.
    // Root components are stored as { FixturePointId = componentID, OptionId = 0 }.
    // Only add types that don't already have a valid root in _fixtures.
    static constexpr uint8 starterTypes[] = { HOUSING_FIXTURE_TYPE_BASE, HOUSING_FIXTURE_TYPE_ROOF };

    // Determine which root types already exist
    std::unordered_set<uint8> existingRootTypes;
    for (auto const& [pointId, fix] : _fixtures)
    {
        if (fix.OptionId != 0)
            continue;
        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fix.FixturePointId);
        if (!comp)
            continue;
        if (_houseType != 0 && comp->HouseExteriorWmoDataID != static_cast<uint32>(_houseType))
            continue;
        existingRootTypes.insert(comp->Type);
    }

    uint64 ownerGuid = _owner->GetGUID().GetCounter();

    for (uint8 fixtureType : starterTypes)
    {
        if (existingRootTypes.count(fixtureType))
        {
            TC_LOG_INFO("housing", "Housing::PopulateStarterFixtures: type={} already has a root — skipping",
                fixtureType);
            continue;
        }

        uint32 compID = sHousingMgr.GetDefaultFixtureForType(fixtureType, _houseType, _houseSize);
        if (!compID)
        {
            TC_LOG_ERROR("housing", "Housing::PopulateStarterFixtures: No default component for type={} wmo={} size={} — skipping",
                fixtureType, _houseType, _houseSize);
            continue;
        }

        // Insert into in-memory map
        Fixture& fixture = _fixtures[compID];
        fixture.FixturePointId = compID;
        fixture.OptionId = 0;

        // Persist to DB
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        uint8 index = 0;
        stmt->setUInt64(index++, ownerGuid);
        stmt->setUInt32(index++, compID);
        stmt->setUInt32(index++, 0); // OptionId = 0 (default)
        CharacterDatabase.Execute(stmt);

        TC_LOG_INFO("housing", "Housing::PopulateStarterFixtures: Added type={} compID={} wmo={} for player {}",
            fixtureType, compID, _houseType, _owner->GetName());
    }

    // --- Starter door ---
    // Every new house starts with a door at the first door hook on the base component.
    // Only this house type's fixtures count: fixtures of a previous type stay stored (switching back restores
    // them) but must neither suppress the new type's door nor supply its base.
    if (!HasCurrentTypeDoor())
    {
        // Find the base component to get its door hooks
        uint32 baseCompID = 0;
        for (auto const& [pointId, fix] : _fixtures)
        {
            if (fix.OptionId != 0)
                continue;
            ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fix.FixturePointId);
            if (comp && comp->Type == HOUSING_FIXTURE_TYPE_BASE && IsCurrentTypeComponent(comp))
            {
                baseCompID = fix.FixturePointId;
                break;
            }
        }

        if (baseCompID)
        {
            auto const* hooks = sHousingMgr.GetHooksOnComponent(baseCompID);
            if (hooks)
            {
                // Find the first door hook (ExteriorComponentTypeID == 11)
                uint32 doorHookID = 0;
                for (ExteriorComponentHookEntry const* hook : *hooks)
                {
                    if (hook && hook->ExteriorComponentTypeID == HOUSING_FIXTURE_TYPE_DOOR)
                    {
                        doorHookID = hook->ID;
                        break;
                    }
                }

                if (doorHookID)
                {
                    uint32 doorCompID = sHousingMgr.GetDefaultFixtureForType(HOUSING_FIXTURE_TYPE_DOOR, _houseType, _houseSize);
                    if (doorCompID)
                    {
                        Fixture& doorFixture = _fixtures[doorHookID];
                        doorFixture.FixturePointId = doorHookID;
                        doorFixture.OptionId = doorCompID;

                        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
                        uint8 index = 0;
                        stmt->setUInt64(index++, ownerGuid);
                        stmt->setUInt32(index++, doorHookID);
                        stmt->setUInt32(index++, doorCompID);
                        CharacterDatabase.Execute(stmt);

                        TC_LOG_INFO("housing", "Housing::PopulateStarterFixtures: Added starter door compID={} at hookID={} for player {}",
                            doorCompID, doorHookID, _owner->GetName());
                    }
                    else
                    {
                        TC_LOG_ERROR("housing", "Housing::PopulateStarterFixtures: No default door component for wmo={} size={}", _houseType, _houseSize);
                    }
                }
                else
                {
                    TC_LOG_ERROR("housing", "Housing::PopulateStarterFixtures: No door hooks found on base component {}", baseCompID);
                }
            }
        }
    }
}

Housing::Room const* Housing::GetRoom(ObjectGuid roomGuid) const
{
    auto itr = _rooms.find(roomGuid);
    return itr != _rooms.end() ? &itr->second : nullptr;
}

uint32 Housing::GetNextRoomSlotIndex() const
{
    uint32 nextSlot = 0;
    for (auto const& [guid, room] : _rooms)
        nextSlot = std::max(nextSlot, room.SlotIndex + 1);
    return nextSlot;
}

void Housing::RemoveAllNonBaseRooms()
{
    for (auto itr = _rooms.begin(); itr != _rooms.end();)
    {
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(itr->second.RoomEntryId);
        if (roomData && roomData->IsBaseRoom())
            ++itr;
        else
            itr = _rooms.erase(itr);
    }

    RecalculateBudgets();
    SyncUpdateFields();
}

void Housing::SetRoomAppearance(ObjectGuid roomGuid, Room const& appearance)
{
    auto itr = _rooms.find(roomGuid);
    if (itr == _rooms.end())
        return;

    Room& room = itr->second;
    room.ThemeId = appearance.ThemeId;
    room.WallThemeId = appearance.WallThemeId;
    room.FloorThemeId = appearance.FloorThemeId;
    room.CeilingThemeId = appearance.CeilingThemeId;
    room.WallTextureId = appearance.WallTextureId;
    room.FloorTextureId = appearance.FloorTextureId;
    room.CeilingTextureId = appearance.CeilingTextureId;
    room.ColorOverride = appearance.ColorOverride;
    room.DoorTypeId = appearance.DoorTypeId;
    room.DoorSlot = appearance.DoorSlot;
    room.DoorTypes = appearance.DoorTypes;
    if (appearance.DoorTypeId && appearance.DoorSlot)
        room.DoorTypes[appearance.DoorTypeId] = appearance.DoorSlot;
    room.CeilingTypeId = appearance.CeilingTypeId;
    room.CeilingSlot = appearance.CeilingSlot;
    room.ComponentThemes = appearance.ComponentThemes;
    room.ComponentTextures = appearance.ComponentTextures;
    PersistRoomToDB(roomGuid, room);
}

void Housing::ReplaceFixtures(std::vector<Fixture> const& fixtures)
{
    _fixtures.clear();
    for (Fixture const& fixture : fixtures)
        _fixtures[fixture.FixturePointId] = fixture;

    SyncUpdateFields();
}
