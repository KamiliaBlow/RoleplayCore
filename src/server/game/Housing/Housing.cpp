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
#include "DBCEnums.h"
#include "GameTime.h"
#include "HousingMgr.h"
#include "HousingPlayerHouseEntity.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Util.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
#include <unordered_set>

namespace
{
    // Normalize to a unit quaternion so cardinal angles round-trip through FLOAT columns exactly; degenerate input falls back to identity.
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

    // Scale clamps shared by the place/move paths (sniffed scale range 0.45 - 1.62).
    constexpr float MIN_DECOR_SCALE = 0.01f;
    constexpr float MAX_DECOR_SCALE = 5.0f;

    // Synthetic storage GUID band for unplaced catalog copies: ownerCounter * 100000 + decorEntryId * 100 + index.
    constexpr uint64 STORAGE_GUID_OWNER_SCALE = 100000;
    constexpr uint64 STORAGE_GUID_ENTRY_SCALE = 100;

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

// Global DB ID generators - initialized from MAX(id) at server startup
std::atomic<uint64> Housing::s_nextDecorDbId{1};
std::atomic<uint64> Housing::s_nextRoomDbId{1};

Housing::Housing(Player* owner, ObjectGuid ownerGuid /*= ObjectGuid::Empty*/)
    : _owner(owner)
    , _ownerGuid(ownerGuid.IsEmpty() ? owner->GetGUID() : ownerGuid)
    , _plotIndex(INVALID_PLOT_INDEX)
    , _level(1)
    , _favor(0)
    , _settingsFlags(HOUSE_SETTING_DEFAULT)
    , _editorMode(HOUSING_EDITOR_MODE_NONE)
{
}

void Housing::InitializeDbIdGenerators()
{
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
    // fields[0] is the DB2 entry id, not a GUID counter: the house GUID is rebuilt from the Battle.net account id.
    uint32 bnetAccountId = _owner->GetSession()->GetBattlenetAccountId();
    // Take the neighborhood's real GUID from the manager: arg1 is its NeighborhoodMapID and goes out to the client.
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

    _housePosX = fields[10].GetFloat();
    _housePosY = fields[11].GetFloat();
    _housePosZ = fields[12].GetFloat();
    _houseFacing = fields[13].GetFloat();
    _hasCustomPosition = (_housePosX != 0.0f || _housePosY != 0.0f || _housePosZ != 0.0f);
    _houseName = fields[14].GetString();
    _houseDescription = fields[15].GetString();

    // Load rooms first so decor can look up roomEntryId for GUID arg2
    if (rooms)
    {
        do
        {
            fields = rooms->Fetch();

            uint64 roomDbId = fields[0].GetUInt64();
            uint32 roomEntryId = fields[1].GetUInt32();

            // Fix up roomDbId=0 from old saves that used ObjectGuid::Empty (all rooms would share one GUID key).
            if (roomDbId == 0)
                roomDbId = GenerateRoomDbId();

            // arg2=roomEntryId matches the retail Housing GUID format
            ObjectGuid roomGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 2, 0, roomEntryId, roomDbId);

            Room& room = _rooms[roomGuid];
            room.Guid = roomGuid;
            room.RoomEntryId = roomEntryId;
            room.SlotIndex = fields[2].GetUInt32();
            room.GridX = fields[3].GetInt32();
            room.GridY = fields[4].GetInt32();
            room.FloorIndex = fields[5].GetInt32();
            // Backward compat: old grid index (0-20) -> yards
            if (room.GridX >= 0 && room.GridX <= 20 && room.GridX == static_cast<int32>(room.SlotIndex) && room.SlotIndex > 0)
                room.GridX = static_cast<int32>(room.SlotIndex) * static_cast<int32>(HOUSING_ROOM_GRID_SPACING);
            // Backward compat: FloorIndex was once stored as a yard offset; convert legacy multiples of 12 to floor numbers
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
            // Legacy rows have all three per-surface themes = 0: seed them from the single ThemeId.
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

    // Runtime fixup for houses saved by older builds: ensure the entry hall and a visual room exist.
    {
        // Migrate the exterior geobox room entry to the entry hall entry (the geobox entry is exterior-plot only).
        uint32 entryHallEntry = sHousingMgr.GetEntryHallRoomEntryId();
        uint32 extGeoboxEntry = sHousingMgr.GetBaseRoomEntryId();
        if (entryHallEntry != extGeoboxEntry)
        {
            for (auto& [guid, room] : _rooms)
            {
                if (room.RoomEntryId == extGeoboxEntry)
                {
                    TC_LOG_INFO("housing", "Housing::LoadFromDB: Migrating interior base room {} -> {} "
                        "in slot {} for house {} (entry hall fixup)",
                        extGeoboxEntry, entryHallEntry, room.SlotIndex, _houseGuid.ToString());
                    room.RoomEntryId = entryHallEntry;
                    break;
                }
            }
        }

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
            TC_LOG_INFO("housing", "Housing::LoadFromDB: Auto-placed entry hall room (entry {}) in slot 0 "
                "for house {} (migration fixup, result={})",
                entryHallRoomEntry, _houseGuid.ToString(), baseResult);
        }

        // Only add a default visual room when the house has no non-base rooms at all.
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
                hasVisualRoom = true; // Multiple visual rooms - don't mess with them
        }

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
            TC_LOG_INFO("housing", "Housing::LoadFromDB: Auto-placed visual room entry {} in slot {} "
                "for house {} (migration fixup, result={})",
                correctVisualRoom, nextSlot, _houseGuid.ToString(), placeResult);
        }
    }

    // Load placed decor (after rooms so RoomGuid can use the room's actual GUID key)
    std::unordered_map<uint64, ObjectGuid> roomGuidByDbId;
    for (auto const& [rGuid, r] : _rooms)
        roomGuidByDbId[rGuid.GetCounter()] = rGuid;

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
            if (placed.Scale < MIN_DECOR_SCALE)
                placed.Scale = 1.0f;
            placed.DyeSlots[0] = fields[10].GetUInt32();
            placed.DyeSlots[1] = fields[11].GetUInt32();
            placed.DyeSlots[2] = fields[12].GetUInt32();
            uint64 roomDbId = fields[13].GetUInt64();
            if (roomDbId)
            {
                // Use the room's actual GUID key from _rooms: room migration changes RoomEntryId but not the key's arg2.
                if (auto itr = roomGuidByDbId.find(roomDbId); itr != roomGuidByDbId.end())
                    placed.RoomGuid = itr->second;
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

        // A fixture of one house type on a hook of another never spawns, yet it counted as the house's entrance.
        for (auto itr = _fixtures.begin(); itr != _fixtures.end();)
        {
            ExteriorComponentEntry const* comp = itr->second.OptionId ? sExteriorComponentStore.LookupEntry(itr->second.OptionId) : nullptr;
            uint32 const hookWmo = comp ? GetHookOwnerWmo(itr->first) : 0;
            if (comp && hookWmo && comp->HouseExteriorWmoDataID && comp->HouseExteriorWmoDataID != hookWmo)
            {
                TC_LOG_INFO("housing", "Housing::LoadFromDB: dropping fixture comp {} (wmo {}) on hook {} of wmo {}",
                    itr->second.OptionId, comp->HouseExteriorWmoDataID, itr->first, hookWmo);
                CharacterDatabasePreparedStatement* del = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
                del->setUInt64(0, _ownerGuid.GetCounter());
                del->setUInt32(1, itr->first);
                CharacterDatabase.Execute(del);
                itr = _fixtures.erase(itr);
            }
            else
                ++itr;
        }
    }

    // A Small-only item facade saved on a bigger house: the client shows no base/roof style when the roots do not match the size.
    if (_houseType && _houseSize > HOUSING_FIXTURE_SIZE_SMALL && !sHousingMgr.IsHouseSizeAvailableForType(_houseType, _houseSize))
    {
        if (uint8 const size = sHousingMgr.GetLargestHouseSizeForType(_houseType, _houseSize - 1))
        {
            TC_LOG_INFO("housing", "Housing::LoadFromDB: house {} type {} has no size {}, shrinking to {}",
                _houseGuid.ToString(), _houseType, _houseSize, size);
            _houseSize = size;
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_HOUSE_SIZE);
            stmt->setUInt8(0, _houseSize);
            stmt->setUInt64(1, _ownerGuid.GetCounter());
            CharacterDatabase.Execute(stmt);
        }
    }
    if (_houseSize >= HOUSING_FIXTURE_SIZE_SMALL)
        RemapFixturesForHouseSize(_houseSize);

    // Migration: houses created before fixture persistence may miss their starter roots (Base/Roof) or door.
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
        TC_LOG_INFO("housing", "Housing::LoadFromDB: Missing starter fixtures (base={}, roof={}, door={}) for house {} - populating (migration)",
            hasBaseRoot, hasRoofRoot, hasDoor, _houseGuid.ToString());
        PopulateStarterFixtures();
    }

    // Load catalog
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

    // Fixup: houses created before the catalog was populated start with the starter decor.
    if (_catalog.empty() && !_houseGuid.IsEmpty() && _owner)
    {
        // The house's neighborhood, not the loading character: another character of the account may be of the other faction.
        auto starterDecorWithQty = sHousingMgr.GetStarterDecorWithQuantities(GetNeighborhoodFaction() == NEIGHBORHOOD_FACTION_HORDE ? HORDE : ALLIANCE);
        if (!starterDecorWithQty.empty())
        {
            for (auto const& [decorId, qty] : starterDecorWithQty)
            {
                CatalogEntry& entry = _catalog[decorId];
                entry.DecorEntryId = decorId;
                entry.Count = qty;
                entry.SourceType = DECOR_SOURCE_DEFERRED; // starter copies retire the client's StartingQuantity credit (see purchase handler)
            }
            TC_LOG_INFO("housing", "Housing::LoadFromDB: Catalog was empty for house {} - auto-populated {} starter decor types for player {}",
                _houseGuid.ToString(), uint32(starterDecorWithQty.size()), _ownerGuid.ToString());

            // Persist the fixup so it only happens once
            if (_owner->GetSession())
            {
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                for (auto const& [entryId, entry] : _catalog)
                {
                    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_CATALOG);
                    uint8 idx = 0;
                    stmt->setUInt64(idx++, _ownerGuid.GetCounter());
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

    // Starter decor is placed once on purchase - no top-up, or a cleared house would get the set back each login.

    RecalculateBudgets();

    // FHousingStorage_C is NOT populated at login - the client only expects it on edit mode / REQUEST_STORAGE.

    SyncUpdateFields();

    return true;
}

// Single place binding the CHAR_INS_CHARACTER_HOUSING_DECOR column list, shared by every insert site.
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
    stmt->setUInt64(index++, !decor.RoomGuid.IsEmpty() && !Housing::IsExteriorDecorPlacement(decor.RoomGuid)
        ? decor.RoomGuid.GetCounter() : 0);
    stmt->setUInt8(index++, decor.Locked ? 1 : 0);
    stmt->setUInt64(index++, static_cast<uint64>(decor.PlacementTime));
    stmt->setUInt8(index++, decor.SourceType);
    stmt->setString(index++, decor.SourceValue);
    stmt->setUInt64(index++, decor.PetGuid.IsEmpty() ? 0 : decor.PetGuid.GetCounter());
    stmt->setUInt8(index++, decor.PetFlag);
}

void Housing::SaveToDB(CharacterDatabaseTransaction trans)
{
    ObjectGuid::LowType ownerGuid = _ownerGuid.GetCounter();

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

    // The update field carries the editing context (decor=1, room=2, fixture=3), not HousingEditorMode.
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
    // Racial house style: Night Elf -> 55, Blood Elf -> 56, other Alliance -> 9, other Horde -> 87
    _houseType = HousingMgr::GetRacialWmoDataID(_owner->GetRace(), _owner->GetTeam());
    _createTime = static_cast<uint32>(GameTime::GetGameTime());
    _hasCustomPosition = false;
    _housePosX = _housePosY = _housePosZ = _houseFacing = 0.0f;

    // HouseGUID.Low must be the Battle.net account counter, not a player GUID counter, or client lookups break.
    uint32 bnetAccountId = _owner->GetSession() ? _owner->GetSession()->GetBattlenetAccountId() : 0;
    if (bnetAccountId == 0)
    {
        TC_LOG_ERROR("housing", "Housing::Create: BNetAccountId is 0 for player {} - falling back to player GUID counter",
            _ownerGuid.ToString());
        bnetAccountId = static_cast<uint32>(_ownerGuid.GetCounter());
    }
    Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    _houseGuid = MakeHouseGuid(neighborhood ? neighborhood->GetNeighborhoodMapID() : 0, bnetAccountId);

    SyncUpdateFields();

    // Every new house starts with an entry hall room (the exterior geobox room is handled by SpawnRoomForPlot).
    PlaceRoom(sHousingMgr.GetEntryHallRoomEntryId(), /*slotIndex*/ 0, /*orientation*/ 0, /*mirrored*/ false);

    // Also place a default visual room so the interior renders walls/floor/ceiling.
    uint32 visualRoom = sHousingMgr.GetDefaultVisualRoomEntry();
    if (visualRoom)
    {
        // Entry door at +3, Room1 door at -12 -> spacing = 3-(-12) = 15 yards
        HousingResult visualResult = PlaceRoom(visualRoom, /*slotIndex*/ 1, /*orientation*/ 0, /*mirrored*/ false, nullptr, /*gridX*/ 15, /*gridY*/ 0);
        if (visualResult == HOUSING_RESULT_SUCCESS)
        {
            TC_LOG_DEBUG("housing", "Housing::Create: Auto-placed visual room entry {} in slot 1 for player {}",
                visualRoom, _owner->GetName());
        }
        else
        {
            TC_LOG_ERROR("housing", "Housing::Create: PlaceRoom FAILED for visual room entry {} - result={} - "
                "interior will be empty for player {}",
                visualRoom, visualResult, _owner->GetName());
        }
    }
    else
    {
        TC_LOG_ERROR("housing", "Housing::Create: No visual room entry found - interior will be empty for player {}",
            _owner->GetName());
    }

    // Starter fixtures (Base + Roof) are persisted so spawning only reads what is stored.
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
    DeleteFromDB(_ownerGuid.GetCounter(), trans);
    CharacterDatabase.CommitTransaction(trans);

    // Release the neighborhood plot so it becomes vacant and re-purchasable instead of being orphaned.
    if (!_neighborhoodGuid.IsEmpty())
    {
        if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
            neighborhood->ReleasePlot(_ownerGuid);
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

    // Must use subType=1 (decor GUID format) - subType=0 returns ObjectGuid::Empty!
    uint64 newDbId = GenerateDecorDbId();
    ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 1, /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ catalogEntryId, newDbId);

    _pendingPlacements[decorGuid] = catalogEntryId;

    result = HOUSING_RESULT_SUCCESS;
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

    // The client always sends a non-Empty RoomGuid; exterior placements use the plot's base room identity,
    // routed to the exterior budget with the RoomGuid preserved as-sent.
    bool const isExterior = IsExteriorDecorPlacement(roomGuid);

    // Enforce the outdoor "two lights cannot overlap" rule before charging.
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

    // Client-supplied GUID (no matching pending placement): refuse an occupied GUID and advance the generator past it.
    if (_placedDecor.contains(decorGuid))
        return HOUSING_RESULT_INVALID_DECOR_ITEM;

    uint64 const clientDbId = decorGuid.GetCounter();
    uint64 expectedDbId = s_nextDecorDbId.load();
    while (clientDbId >= expectedDbId && !s_nextDecorDbId.compare_exchange_weak(expectedDbId, clientDbId + 1))
        ;

    _pendingPlacements.erase(decorGuid);

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
    // Place at the scale the client sent; dropping it would spawn resized decor bigger than the preview.
    decor.Scale = std::isfinite(scale) && scale >= MIN_DECOR_SCALE ? std::min(scale, MAX_DECOR_SCALE) : GetDecorInitialScale(decorEntryId);
    decor.DyeSlots = {};
    decor.RoomGuid = roomGuid;
    decor.PlacementTime = GameTime::GetGameTime();
    // Inherit acquisition source from catalog entry
    decor.SourceType = catalogItr->second.SourceType;
    decor.SourceValue = catalogItr->second.SourceValue;

    catalogItr->second.Count--;
    if (catalogItr->second.Count == 0)
        _catalog.erase(catalogItr);

    // Charge the same budget the check validated.
    if (isExterior)
        _exteriorDecorWeightUsed += weightCost;
    else
        _interiorDecorWeightUsed += weightCost;

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        BindDecorInsert(stmt, _ownerGuid.GetCounter(), decorGuid, decor);
        CharacterDatabase.Execute(stmt);
    }

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        auto catItr = _catalog.find(decorEntryId);
        stmt->setUInt32(0, catItr != _catalog.end() ? catItr->second.Count : 0);
        stmt->setUInt64(1, _ownerGuid.GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    if (_owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().SetHousingDecorStorageEntry(decorGuid, _houseGuid, decor.SourceType, decor.SourceValue,
            isExterior ? std::optional<uint8>(HOUSING_DECOR_PLACED_PLOT) : std::nullopt);

    // CriteriaType::PlaceDecor (270); miscValue1 = HouseDecor entry so decor-scoped conditions can discriminate.
    _owner->UpdateCriteria(CriteriaType::PlaceDecor, decorEntryId);

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::PlaceDecor(uint32 decorEntryId, float x, float y, float z,
    float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid)
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

    uint32 weightCost = sHousingMgr.GetDecorWeightCost(decorEntryId);
    bool const isExterior = IsExteriorDecorPlacement(roomGuid);

    // Enforce the outdoor "two lights cannot overlap" rule before charging.
    if (HousingResult overlap = CheckLightOverlap(decorEntryId, x, y, z, isExterior);
        overlap != HOUSING_RESULT_SUCCESS)
        return overlap;

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

    // Must use subType=1 (decor GUID format) - subType=0 returns ObjectGuid::Empty!
    uint64 newDbId = GenerateDecorDbId();
    ObjectGuid decorGuid = ObjectGuid::Create<HighGuid::Housing>(
        /*subType*/ 1, /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
        /*arg2*/ decorEntryId, newDbId);

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

    catalogItr->second.Count--;
    if (catalogItr->second.Count == 0)
        _catalog.erase(catalogItr);

    // Charge the same budget the check validated.
    if (isExterior)
        _exteriorDecorWeightUsed += weightCost;
    else
        _interiorDecorWeightUsed += weightCost;

    // Immediate persist for crash safety
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_DECOR);
        BindDecorInsert(stmt, _ownerGuid.GetCounter(), decorGuid, decor);
        CharacterDatabase.Execute(stmt);
    }

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        auto catItr = _catalog.find(decorEntryId);
        stmt->setUInt32(0, catItr != _catalog.end() ? catItr->second.Count : 0);
        stmt->setUInt64(1, _ownerGuid.GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    // Update account decor storage UpdateField (only if storage is populated - not during LoadFromDB)
    if (_storagePopulated && _owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().SetHousingDecorStorageEntry(decorGuid, _houseGuid, decor.SourceType, decor.SourceValue,
            isExterior ? std::optional<uint8>(HOUSING_DECOR_PLACED_PLOT) : std::nullopt);

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::MoveDecor(ObjectGuid decorGuid, float x, float y, float z,
    float rotX, float rotY, float rotZ, float rotW, float scale /*= 1.0f*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(rotX) || !std::isfinite(rotY) || !std::isfinite(rotZ) || !std::isfinite(rotW))
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    // Clamp scale to a reasonable range (sniffs show values like 0.45 to 1.62)
    if (!std::isfinite(scale) || scale < MIN_DECOR_SCALE)
        scale = 1.0f;
    if (scale > MAX_DECOR_SCALE)
        scale = MAX_DECOR_SCALE;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    // Validate the move target like a placement so an item cannot be flung to arbitrary coordinates.
    HousingResult validationResult = sHousingMgr.ValidateDecorPlacement(itr->second.DecorEntryId, Position(x, y, z),
        GetDecorPlacementAnchor(itr->second.RoomGuid), _level);
    if (validationResult != HOUSING_RESULT_SUCCESS)
        return validationResult;

    if (HousingResult boundsResult = CheckInteriorDecorBounds(itr->second.RoomGuid, x, y, z); boundsResult != HOUSING_RESULT_SUCCESS)
        return boundsResult;

    // excludeGuid skips the decor being moved so an in-place nudge never collides with itself.
    if (HousingResult overlap = CheckLightOverlap(itr->second.DecorEntryId, x, y, z,
            IsExteriorDecorPlacement(itr->second.RoomGuid), decorGuid);
        overlap != HOUSING_RESULT_SUCCESS)
        return overlap;

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
    stmt->setUInt64(8, _ownerGuid.GetCounter());
    stmt->setUInt64(9, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

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

    // Lock only prevents OTHER editors from modifying; the owner can always remove their own decor.

    // Refund the budget the placement charged
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

    // Immediate persist for crash safety
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_DECOR_SINGLE);
        stmt->setUInt64(0, _ownerGuid.GetCounter());
        stmt->setUInt64(1, decorGuid.GetCounter());
        CharacterDatabase.Execute(stmt);
    }

    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_CATALOG_COUNT);
        stmt->setUInt32(0, _catalog[decorEntryId].Count);
        stmt->setUInt64(1, _ownerGuid.GetCounter());
        stmt->setUInt32(2, decorEntryId);
        CharacterDatabase.Execute(stmt);
    }

    // Remove from account decor storage UpdateField (only if storage is populated)
    if (_storagePopulated && _owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().RemoveHousingDecorStorageEntry(decorGuid);

    // CriteriaType::RemoveDecor (271, "Remove any decor"); miscValue1 = the HouseDecor entry removed.
    _owner->UpdateCriteria(CriteriaType::RemoveDecor, decorEntryId);

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

HousingResult Housing::CommitDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots, bool consumeDyes /*= true*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _placedDecor.find(decorGuid);
    if (itr == _placedDecor.end())
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    // A color of 0 clears the slot; each newly dyed slot consumes one DyeColor.ItemID from the bags.
    std::map<uint32, uint32> dyeItems; // itemId -> count
    for (std::size_t i = 0; i < dyeSlots.size(); ++i)
    {
        uint32 const dyeColorId = dyeSlots[i];
        if (!dyeColorId)
            continue;

        DyeColorEntry const* dyeColor = sDyeColorStore.LookupEntry(dyeColorId);
        if (!dyeColor)
            return HOUSING_RESULT_MISSING_DYE;

        if (consumeDyes && dyeColorId != itr->second.DyeSlots[i] && dyeColor->ItemID > 0)
            ++dyeItems[uint32(dyeColor->ItemID)];
    }

    for (auto const& [itemId, count] : dyeItems)
        if (!_owner->HasItemCount(itemId, count))
            return HOUSING_RESULT_MISSING_DYE;

    for (auto const& [itemId, count] : dyeItems)
        _owner->DestroyItemCount(itemId, count, true);

    itr->second.DyeSlots = dyeSlots;

    // The account storage entry carries the dyes too (FHousingStorage_C Decor[guid].DyeSlots).
    if (_owner->GetSession())
        _owner->GetSession()->GetBattlenetAccount().SetHousingDecorDyeSlots(decorGuid, dyeSlots);

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_DECOR_DYES);
    stmt->setUInt32(0, dyeSlots[0]);
    stmt->setUInt32(1, dyeSlots[1]);
    stmt->setUInt32(2, dyeSlots[2]);
    stmt->setUInt64(3, _ownerGuid.GetCounter());
    stmt->setUInt64(4, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

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
    stmt->setUInt64(1, _ownerGuid.GetCounter());
    stmt->setUInt64(2, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

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
    stmt->setUInt64(2, _ownerGuid.GetCounter());
    stmt->setUInt64(3, decorGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

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

    // Snapshot the matching guids first - RemoveDecor mutates _placedDecor.
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
        // RemoveDecor refunds budget, returns the item to the catalog and deletes the DB row per item.
        if (RemoveDecor(guid) == HOUSING_RESULT_SUCCESS)
            ++removed;
    }

    if (outRemoved)
        *outRemoved = removed;

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
        return HOUSING_RESULT_GENERIC_FAILURE;

    // Check WeightCost-based room budget
    uint32 roomWeightCost = GetRoomWeightCost(roomEntryId, gridX, gridY, floorIndex, ObjectGuid::Empty);
    if (_roomWeightUsed + roomWeightCost > GetMaxRoomBudget())
        return HOUSING_RESULT_GENERIC_FAILURE;

    // Check for slot collision
    for (auto const& [guid, room] : _rooms)
    {
        if (room.SlotIndex == slotIndex)
            return HOUSING_RESULT_PLOT_NOT_FOUND;
    }

    // NOTE: Doorway components (Type 7) are optional in DB2; standard rooms use wall segments instead.

    // arg2=roomEntryId matches the retail Housing GUID format
    uint64 newDbId = GenerateRoomDbId();
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

    _roomWeightUsed += roomWeightCost;

    // No SMSG_ACCOUNT_ROOM_COLLECTION_UPDATE here: retail sends it once per login, never after an add.

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
        RemoveDecor(decorGuid);
    }

    // Refund room WeightCost budget
    uint32 roomWeightCost = GetRoomWeightCost(itr->second.RoomEntryId, itr->second.GridX, itr->second.GridY, itr->second.FloorIndex, roomGuid);
    if (_roomWeightUsed >= roomWeightCost)
        _roomWeightUsed -= roomWeightCost;
    else
        _roomWeightUsed = 0;

    _rooms.erase(itr);

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

    // A room turns a quarter per step, skipping headings where any room attached to it would lose its door.
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

    // Placed decor is stored in interior world space; it rides along with its room.
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

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
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
        stmt->setUInt64(8, _ownerGuid.GetCounter());
        stmt->setUInt64(9, decorGuid.GetCounter());
        trans->Append(stmt);
    }
    CharacterDatabase.CommitTransaction(trans);
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

        // Connectable walls with exactly one horizontal offset axis; a stairwell also lists its connectable floor and ceiling.
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
    // The room placed earlier (lower slot) carries the doorway piece, the entry hall included.
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

    // Rows written before per-door styles carry a single (component, variant) pair.
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
    struct Box { float MinX, MinY, MaxX, MaxY; };

    // Floor footprint from RoomGridLine.db2; the WMO bounding box pokes past a round room's doors, so only rooms without grid lines use it.
    auto getBoxes = [](uint32 entryId, int32 x, int32 y, uint32 turn)
    {
        std::vector<Box> boxes;
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(entryId);
        if (!roomData || !roomData->RoomWmoDataID)
            return boxes;

        for (RoomGridLineEntry const* line : sRoomGridLineStore)
        {
            if (line->RoomWmoDataID != uint32(roomData->RoomWmoDataID))
                continue;

            float centerX, centerY, halfX, halfY;
            RotateRoomOffset(line->Offset.X, line->Offset.Y, turn, centerX, centerY);
            RotateRoomOffset(line->SizeX / 2.0f, line->SizeY / 2.0f, turn, halfX, halfY);
            halfX = std::abs(halfX);
            halfY = std::abs(halfY);
            boxes.push_back({ x + centerX - halfX, y + centerY - halfY, x + centerX + halfX, y + centerY + halfY });
        }

        if (boxes.empty())
        {
            if (RoomWmoDataEntry const* bounds = sRoomWmoDataStore.LookupEntry(roomData->RoomWmoDataID))
            {
                float ax, ay, bx, by;
                RotateRoomOffset(bounds->BoundingBoxMinX, bounds->BoundingBoxMinY, turn, ax, ay);
                RotateRoomOffset(bounds->BoundingBoxMaxX, bounds->BoundingBoxMaxY, turn, bx, by);
                boxes.push_back({ x + std::min(ax, bx), y + std::min(ay, by), x + std::max(ax, bx), y + std::max(ay, by) });
            }
        }
        return boxes;
    };

    std::vector<Box> const boxes = getBoxes(roomEntryId, gridX, gridY, orientation);
    if (boxes.empty())
        return true;

    // RoomGridLine rects include wall thickness: legitimate door-adjacent placements overlap by a few yards, real overlaps by ~11yd.
    constexpr float TOLERANCE = 5.0f;
    for (Room const* other : rooms)
    {
        if (!other || other->Guid == ignoreRoom || other->FloorIndex != floorIndex)
            continue;

        for (Box const& otherBox : getBoxes(other->RoomEntryId, other->GridX, other->GridY, other->Orientation))
            for (Box const& box : boxes)
                if (box.MinX < otherBox.MaxX - TOLERANCE && box.MaxX > otherBox.MinX + TOLERANCE
                    && box.MinY < otherBox.MaxY - TOLERANCE && box.MaxY > otherBox.MinY + TOLERANCE)
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

    // Only the named slots change (a ceiling restyle recreates just that ceiling piece).
    for (uint32 componentId : optionIds)
        itr->second.ComponentThemes[componentId] = themeSetId;

    PersistRoomToDB(roomGuid, itr->second);

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

    // A fixture goes to the free hook of the same fixture type closest to where it hung before (hook order differs between base styles).
    std::vector<std::pair<uint32, uint32>> moves; // old hook -> new hook
    std::unordered_set<uint32> taken;
    for (ExteriorComponentHookEntry const* oldHook : *oldHooks)
    {
        if (!oldHook || !_fixtures.contains(oldHook->ID))
            continue;

        ExteriorComponentHookEntry const* best = nullptr;
        float bestDist = std::numeric_limits<float>::max();
        for (ExteriorComponentHookEntry const* newHook : *newHooks)
        {
            if (!newHook || newHook->ExteriorComponentTypeID != oldHook->ExteriorComponentTypeID || taken.contains(newHook->ID))
                continue;
            if (newHook->ID != oldHook->ID && _fixtures.contains(newHook->ID))
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

    uint64 const ownerGuid = _ownerGuid.GetCounter();
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
    }
}

void Housing::RemapFixturesForHouseSize(uint8 newSize)
{
    if (_houseGuid.IsEmpty())
        return;

    // Size lives on each ExteriorComponent, so a size change invalidates every stored root fixture.
    std::vector<std::pair<uint32, uint32>> rootMoves; // old componentID -> new componentID
    LocaleConstant const locale = sWorld->GetDefaultDbcLocale();

    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId != 0)
            continue;

        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(pointId);
        if (!comp || comp->Size == newSize)
            continue;
        if (comp->Type != HOUSING_FIXTURE_TYPE_BASE && comp->Type != HOUSING_FIXTURE_TYPE_ROOF)
            continue;
        if (_houseType != 0 && comp->HouseExteriorWmoDataID != static_cast<uint32>(_houseType))
            continue;

        uint32 newCompId = 0;
        if (comp->ParentComponentID == 0)
        {
            // Plain structural root: the same style's default at the requested size.
            newCompId = sHousingMgr.GetDefaultFixtureForType(comp->Type, comp->HouseExteriorWmoDataID, newSize);
        }
        else
        {
            // Style/color variant: keep the player's pick by matching the variant name among the new-size candidates.
            char const* oldName = comp->Name[locale];
            if (oldName)
            {
                for (ExteriorComponentEntry const* candidate : sExteriorComponentStore)
                {
                    if (!candidate || candidate->Size != newSize || candidate->Type != comp->Type
                        || uint32(candidate->HouseExteriorWmoDataID) != comp->HouseExteriorWmoDataID)
                        continue;
                    if (char const* candidateName = candidate->Name[locale];
                        candidateName && strcmp(candidateName, oldName) == 0)
                    {
                        newCompId = candidate->ID;
                        break;
                    }
                }
            }

            if (!newCompId)
                newCompId = sHousingMgr.GetDefaultFixtureForType(comp->Type, comp->HouseExteriorWmoDataID, newSize);
        }

        if (newCompId && newCompId != pointId)
            rootMoves.emplace_back(pointId, newCompId);
    }

    uint64 const ownerGuid = _ownerGuid.GetCounter();
    for (auto const& [oldCompId, newCompId] : rootMoves)
    {
        // Hook fixtures hang on the old root's hooks; re-key them before the root row itself moves.
        MoveHookFixtures(oldCompId, newCompId);

        // The new-size root may already be stored (kept from an earlier visit at that size): only drop the old one
        bool const targetStored = _fixtures.contains(newCompId);
        uint32 const optionId = _fixtures[oldCompId].OptionId;
        _fixtures.erase(oldCompId);

        CharacterDatabasePreparedStatement* del = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
        del->setUInt64(0, ownerGuid);
        del->setUInt32(1, oldCompId);
        CharacterDatabase.Execute(del);

        if (targetStored)
            continue;

        Fixture& moved = _fixtures[newCompId];
        moved.FixturePointId = newCompId;
        moved.OptionId = optionId;

        CharacterDatabasePreparedStatement* ins = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        ins->setUInt64(0, ownerGuid);
        ins->setUInt32(1, newCompId);
        ins->setUInt32(2, optionId);
        CharacterDatabase.Execute(ins);
    }
}

HousingResult Housing::SelectFixtureOption(uint32 fixturePointId, uint32 optionId, std::vector<uint32>* removedHookIDs /*= nullptr*/)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // Root fixture selections (optionId == 0) use componentID as fixturePointId - skip hook validation
    if (optionId != 0)
    {
        ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(fixturePointId);
        if (!hookEntry)
        {
            return HOUSING_RESULT_FIXTURE_NOT_FOUND;
        }

        ExteriorComponentEntry const* compEntry = sExteriorComponentStore.LookupEntry(optionId);
        if (!compEntry)
        {
            return HOUSING_RESULT_FIXTURE_NOT_FOUND;
        }

        // The hook must belong to the current house type (hooks are unique per root, so this also bounds the count).
        if (!IsCurrentTypeComponent(sExteriorComponentStore.LookupEntry(hookEntry->ExteriorComponentID)))
        {
            return HOUSING_RESULT_FIXTURE_NOT_FOUND;
        }

        if (compEntry->Type != hookEntry->ExteriorComponentTypeID)
        {
            return HOUSING_RESULT_GENERIC_FAILURE;
        }

        // Enforce one door per house; also remove any existing fixture at the target hook.
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
                    conflictHooks.push_back(pointId);
                }
            }
        }

        // If the target hook already has a different fixture, remove it
        auto existingAtHook = _fixtures.find(fixturePointId);
        if (existingAtHook != _fixtures.end() && existingAtHook->second.OptionId != 0
            && existingAtHook->second.OptionId != optionId)
        {
            conflictHooks.push_back(fixturePointId);
        }

        for (uint32 conflictHookId : conflictHooks)
        {
            _fixtures.erase(conflictHookId);
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
            stmt->setUInt64(0, _ownerGuid.GetCounter());
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
        // Root fixture (optionId == 0): remove any existing root of the same type and house type.
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
                // Roots of other house types stay stored: switching back to that type restores them.
                if (oldComp && oldComp->Type == newType && oldComp->HouseExteriorWmoDataID == newComp->HouseExteriorWmoDataID)
                {
                    toRemove.push_back(pointId);
                }
            }
            for (uint32 oldKey : toRemove)
            {
                _fixtures.erase(oldKey);
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
                stmt->setUInt64(0, _ownerGuid.GetCounter());
                stmt->setUInt32(1, oldKey);
                CharacterDatabase.Execute(stmt);
                if (_fixtureWeightUsed > 0)
                    --_fixtureWeightUsed;

                // Fixtures hang on the old root's hooks; move each to the new root's hook of the same type and rank.
                MoveHookFixtures(oldKey, fixturePointId);
            }
        }
    }

    bool isNew = _fixtures.find(fixturePointId) == _fixtures.end();

    // Enforce fixture budget for new fixtures
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

    // Immediate persist
    if (!isNew)
        PersistFixtureToDB(fixturePointId, optionId);
    else
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        uint8 index = 0;
        stmt->setUInt64(index++, _ownerGuid.GetCounter());
        stmt->setUInt32(index++, fixturePointId);
        stmt->setUInt32(index++, optionId);
        CharacterDatabase.Execute(stmt);
    }

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

    // Immediate persist - delete single fixture from DB
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_FIXTURE_SINGLE);
        stmt->setUInt64(0, _ownerGuid.GetCounter());
        stmt->setUInt32(1, hookID);
        CharacterDatabase.Execute(stmt);
    }

    // Refund fixture budget
    uint32 const fixtureWeightCost = 1;
    if (_fixtureWeightUsed >= fixtureWeightCost)
        _fixtureWeightUsed -= fixtureWeightCost;

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
    // Hook-based fixture selections; root overrides come from GetRootComponentOverrides().
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
    // Player-selected structural roots (Base/Roof, OptionId == 0), only for the current house type.
    std::unordered_map<uint8, uint32> result;

    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId != 0)
            continue;

        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fixture.FixturePointId);
        if (!comp)
            continue;

        if (comp->Type != HOUSING_FIXTURE_TYPE_BASE && comp->Type != HOUSING_FIXTURE_TYPE_ROOF)
            continue;
        if (_houseType != 0 && comp->HouseExteriorWmoDataID != static_cast<uint32>(_houseType))
            continue;

        result[comp->Type] = fixture.FixturePointId;
    }

    return result;
}

uint32 Housing::GetCoreExteriorComponentID() const
{
    // The core fixture is the primary root selected via SetCoreFixture (OptionId == 0).
    for (auto const& [pointId, fixture] : _fixtures)
    {
        if (fixture.OptionId == 0)
        {
            ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(fixture.FixturePointId);
            // Only a fixture of the current house type counts; a previous type's must not override the new default.
            if (comp && comp->ParentComponentID == 0 && (_houseType == 0 || comp->HouseExteriorWmoDataID == _houseType))
                return fixture.FixturePointId;
        }
    }
    // No explicit core fixture set - find first default root component for this house's WMO data ID.
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
        TC_LOG_ERROR("housing", "Housing::GetCoreExteriorComponentID: No fixtures and houseType=0 - cannot determine base component");
    }
    return 0;
}

HousingResult Housing::AddToCatalog(uint32 decorEntryId, uint8 sourceType, std::string sourceValue)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    // "First time" is exactly "no row existed before this add" (CriteriaType::CollectUniqueDecor 272).
    bool const firstTimeAcquired = !_catalog.contains(decorEntryId);

    CatalogEntry& entry = _catalog[decorEntryId];
    entry.DecorEntryId = decorEntryId;
    entry.Count++;
    // Store the most recent source info (all instances of an entry share it).
    if (sourceType != DECOR_SOURCE_STANDARD || !sourceValue.empty())
    {
        entry.SourceType = sourceType;
        entry.SourceValue = std::move(sourceValue);
    }

    // Persist to DB immediately (crash safety); REPLACE INTO handles both first-add and count-increment.
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHARACTER_HOUSING_CATALOG);
    stmt->setUInt64(0, _ownerGuid.GetCounter());
    stmt->setUInt32(1, decorEntryId);
    stmt->setUInt32(2, entry.Count);
    stmt->setUInt8(3, entry.SourceType);
    stmt->setString(4, entry.SourceValue);
    CharacterDatabase.Execute(stmt);

    // CriteriaType::CollectUniqueDecor (272) - counted once per distinct HouseDecor entry ever acquired.
    if (firstTimeAcquired)
        _owner->UpdateCriteria(CriteriaType::CollectUniqueDecor, decorEntryId);

    // Storage truth travels ONLY through the FHousingStorage_C fragment; a DECOR_ADD_TO_HOUSE_CHEST
    // response here would double-count every acquisition.

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

    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

ObjectGuid Housing::MintStorageDecorInstance(uint32 decorEntryId, HousingResult& result)
{
    result = HOUSING_RESULT_SUCCESS;

    if (_houseGuid.IsEmpty())
    {
        result = HOUSING_RESULT_HOUSE_NOT_FOUND;
        return ObjectGuid::Empty;
    }

    uint32 placedOfType = 0;
    for (auto const& [guid, decor] : _placedDecor)
        if (decor.DecorEntryId == decorEntryId)
            ++placedOfType;

    auto itr = _catalog.find(decorEntryId);
    if (itr == _catalog.end() || itr->second.Count <= placedOfType)
    {
        result = HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;
        return ObjectGuid::Empty;
    }

    // Must reuse the synthetic band PopulateOwnStorageEntries emits, or the client's Decor map accumulates it forever.
    uint64 const catalogGuidBase = _ownerGuid.GetCounter() * STORAGE_GUID_OWNER_SCALE;
    for (uint32 i = 0; i < itr->second.Count; ++i)
    {
        ObjectGuid candidate = ObjectGuid::Create<HighGuid::Housing>(
            /*subType*/ 1,
            /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
            /*arg2*/ decorEntryId,
            catalogGuidBase + uint64(decorEntryId) * STORAGE_GUID_ENTRY_SCALE + i);
        if (!_placedDecor.contains(candidate))
            return candidate;
    }

    // Unreachable when Count > placedOfType, kept as a safe fallback.
    result = HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;
    return ObjectGuid::Empty;
}

HousingResult Housing::DestroyAllCopies(uint32 decorEntryId)
{
    if (_houseGuid.IsEmpty())
        return HOUSING_RESULT_HOUSE_NOT_FOUND;

    auto itr = _catalog.find(decorEntryId);
    if (itr == _catalog.end())
        return HOUSING_RESULT_DECOR_NOT_FOUND_IN_STORAGE;

    _catalog.erase(itr);

    // Also remove all placed decor of this entry
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

    uint32 const oldLevel = _level;
    _level = newLevel;

    // Persist level/favor to DB immediately
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_LEVEL_FAVOR);
    stmt->setUInt32(0, _level);
    stmt->setUInt32(1, _favor);
    stmt->setUInt64(2, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    RecalculateBudgets();
    SyncUpdateFields();

    GrantLevelAwards(oldLevel + 1, _level);

    _owner->UpdateHousingLevelFavor(_houseGuid, _level, _favor);
    SendLevelFavorUpdate(int32(_level), -1);
}

void Housing::AddFavor(uint64 amount, HousingFavorUpdateSource source /*= HOUSING_FAVOR_SOURCE_UNKNOWN*/, bool emitUpdate /*= true*/)
{
    _favor64 += amount;
    _favor = static_cast<uint32>(std::min<uint64>(_favor64, std::numeric_limits<uint32>::max()));

    // Persist level/favor to DB immediately
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_LEVEL_FAVOR);
    stmt->setUInt32(0, _level);
    stmt->setUInt32(1, _favor);
    stmt->setUInt64(2, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();
    _owner->UpdateHousingLevelFavor(_houseGuid, _level, _favor);

    if (emitUpdate)
        SendLevelFavorUpdate(-1, int32(_favor));
}

void Housing::GrantLevelAwards(uint32 fromLevel, uint32 toLevel)
{
    // HouseLevelData.QuestID is a hidden award quest whose RewardSpell grants the room/decor for the level.
    for (uint32 level = std::max<uint32>(fromLevel, 2); level <= toLevel; ++level)
    {
        uint32 const questId = sHousingMgr.GetQuestForLevel(level);
        if (!questId || _owner->IsQuestRewarded(questId))
            continue;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
            continue;

        if (quest->GetRewSpell())
            _owner->CastSpell(_owner, quest->GetRewSpell(), true);
        _owner->SetRewardedQuest(questId);
    }
}

void Housing::SendLevelFavorUpdate(int32 level, int32 favor) const
{
    if (!_owner || !_owner->GetSession())
        return;

    // Header Result 0 / 0xFFFFFFFF / 0xFFFFFFFF; per house HouseLevel and FavorValue, -1 = unchanged.
    WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor update;
    update.Result = 0;
    update.ChangeAmount = 0xFFFFFFFF;
    update.Reason = 0xFFFFFFFF;
    auto& house = update.Houses.emplace_back();
    house.HouseGUID = _houseGuid;
    house.HouseLevel = level;
    house.FavorValue = favor;
    _owner->SendDirectMessage(update.Write());
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

    // Room placement as in HouseInteriorMap::SpawnRoomMeshObjects; the tolerance covers wall decor on the wall plane.
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
    // No room -> yard/exterior placement.
    if (roomGuid.IsEmpty())
        return true;

    // The plot's base room identity: retail always sends this RoomGuid for exterior decor.
    return roomGuid.GetHigh() == HighGuid::Housing
        && uint32((roomGuid.GetRawValue(1) >> 53) & 0x1F) == 2
        && uint32(roomGuid.GetRawValue(1) & 0xFFFFFFFFULL) == sHousingMgr.GetBaseRoomEntryId();
}

HousingResult Housing::CheckLightOverlap(uint32 decorEntryId, float x, float y, float z,
    bool isExterior, ObjectGuid excludeGuid /*= ObjectGuid::Empty*/) const
{
    // Only exterior Lighting-category decor participates; non-lights and interior placements pass through.
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
    houseEntity.SetCosmeticOwner(_ownerGuid);
    houseEntity.SetEntityGUID(_houseGuid);
    // HouseType and HouseSize are NOT part of this fragment (IDA-verified).
    houseEntity.SetPlotIndex(static_cast<int32>(_plotIndex));
    houseEntity.SetLevel(_level);
    houseEntity.SetFavor(_favor64);
    // Send MAX budgets - the client computes remaining locally by summing placed decor weight.
    houseEntity.SetBudgets(
        GetMaxInteriorDecorBudget(),
        GetMaxExteriorDecorBudget(),
        GetMaxRoomBudget(),
        GetMaxFixtureBudget()
    );
}

void Housing::PopulateCatalogStorageEntries()
{
    if (!_owner)
        return;

    PopulateOwnStorageEntries();
    for (Housing const* accountHousing : _owner->GetAllHousings())
        if (accountHousing != this)
            const_cast<Housing*>(accountHousing)->PopulateOwnStorageEntries();
}

void Housing::PopulateOwnStorageEntries()
{
    if (!_owner || !_owner->GetSession())
        return;

    if (_storagePopulated)
        return;

    Battlenet::Account& account = _owner->GetSession()->GetBattlenetAccount();

    // 1. Placed decor -> HouseGUID=_houseGuid, SourceType from decor instance
    for (auto const& [decorGuid, decor] : _placedDecor)
    {
        account.SetHousingDecorStorageEntry(decorGuid, _houseGuid, decor.SourceType, decor.SourceValue,
            IsExteriorDecorPlacement(decor.RoomGuid) ? std::optional<uint8>(HOUSING_DECOR_PLACED_PLOT) : std::nullopt);
        account.SetHousingDecorDyeSlots(decorGuid, decor.DyeSlots);
    }

    // 2. Catalog (unplaced) entries -> HouseGUID=Empty. Catalog Count IS the free stock; emit exactly Count
    // entries, skipping synthetic GUIDs currently occupied by placed instances of this entry.
    std::unordered_map<uint32, uint32> placedCountByEntry;
    for (auto const& [decorGuid, decor] : _placedDecor)
        ++placedCountByEntry[decor.DecorEntryId];

    uint64 catalogGuidBase = _ownerGuid.GetCounter() * STORAGE_GUID_OWNER_SCALE;
    for (auto const& [entryId, entry] : _catalog)
    {
        auto const placedItr = placedCountByEntry.find(entryId);
        uint32 const placedOfType = placedItr != placedCountByEntry.end() ? placedItr->second : 0;

        uint32 emitted = 0;
        for (uint32 i = 0; emitted < entry.Count && i < entry.Count + placedOfType; ++i)
        {
            uint64 uniqueId = catalogGuidBase + uint64(entryId) * STORAGE_GUID_ENTRY_SCALE + i;
            ObjectGuid catalogDecorGuid = ObjectGuid::Create<HighGuid::Housing>(
                /*subType*/ 1,
                /*arg1*/ sRealmList->GetCurrentRealmId().Realm,
                /*arg2*/ entryId,
                uniqueId);
            if (_placedDecor.contains(catalogDecorGuid))
                continue;
            account.SetHousingDecorStorageEntry(catalogDecorGuid, ObjectGuid::Empty, entry.SourceType, entry.SourceValue);
            ++emitted;
        }
    }

    _storagePopulated = true;
}

void Housing::SaveSettings(uint32 settingsFlags)
{
    _settingsFlags = settingsFlags;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_SETTINGS);
    stmt->setUInt32(0, _settingsFlags);
    stmt->setUInt64(1, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    // Mirror onto the in-memory neighborhood plot so visitor checks work while the owner is offline.
    if (Neighborhood* nbh = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        nbh->UpdatePlotSettingsFlags(_ownerGuid, _settingsFlags);

    SyncUpdateFields();
}

void Housing::SetHouseNameDescription(std::string const& name, std::string const& desc)
{
    _houseName = name.substr(0, HOUSING_MAX_NAME_LENGTH);
    _houseDescription = desc.substr(0, 256);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_NAME_DESC);
    stmt->setString(0, _houseName);
    stmt->setString(1, _houseDescription);
    stmt->setUInt64(2, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();
}

void Housing::SetExteriorLocked(bool locked)
{
    _exteriorLocked = locked;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_EXTERIOR_LOCKED);
    stmt->setUInt8(0, locked ? 1 : 0);
    stmt->setUInt64(1, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    SyncUpdateFields();
}

HousingResult Housing::ChangeOwner(ObjectGuid newOwnerGuid)
{
    if (newOwnerGuid == _ownerGuid)
        return HOUSING_RESULT_SUCCESS;

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid);
    if (!neighborhood)
        return HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    if (!neighborhood->TransferPlot(_ownerGuid, newOwnerGuid, trans))
        return HOUSING_RESULT_PLOT_NOT_FOUND;

    auto rekey = [&](CharacterDatabaseStatements index)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(index);
        stmt->setUInt64(0, newOwnerGuid.GetCounter());
        stmt->setUInt64(1, _ownerGuid.GetCounter());
        trans->Append(stmt);
    };
    rekey(CHAR_UPD_CHARACTER_HOUSING_OWNER);
    rekey(CHAR_UPD_CHARACTER_HOUSING_DECOR_OWNER);
    rekey(CHAR_UPD_CHARACTER_HOUSING_ROOMS_OWNER);
    rekey(CHAR_UPD_CHARACTER_HOUSING_FIXTURES_OWNER);

    // The catalog travels with the house; rows left from an earlier house of the new owner would collide
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_CATALOG);
    stmt->setUInt64(0, newOwnerGuid.GetCounter());
    trans->Append(stmt);
    rekey(CHAR_UPD_CHARACTER_HOUSING_CATALOG_OWNER);

    CharacterDatabase.CommitTransaction(trans);

    _ownerGuid = newOwnerGuid;
    SyncUpdateFields();
    return HOUSING_RESULT_SUCCESS;
}

void Housing::SetHouseSize(uint8 size)
{
    bool const changed = _houseSize != size;
    _houseSize = size;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_HOUSE_SIZE);
    stmt->setUInt8(0, size);
    stmt->setUInt64(1, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    // Stored root fixtures reference old-size components; re-resolve them to the same style at the new size.
    if (changed)
        RemapFixturesForHouseSize(size);

    SyncUpdateFields();
}

int32 Housing::GetNeighborhoodFaction() const
{
    if (Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        if (neighborhood->GetFactionRestriction() != NEIGHBORHOOD_FACTION_NONE)
            return neighborhood->GetFactionRestriction();

    return _owner && _owner->GetTeamId() == TEAM_HORDE ? NEIGHBORHOOD_FACTION_HORDE : NEIGHBORHOOD_FACTION_ALLIANCE;
}

void Housing::SetHouseType(uint32 typeId)
{
    bool const changed = _houseType != typeId;
    _houseType = typeId;

    // Immediate persist for crash safety
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_HOUSE_TYPE);
    stmt->setUInt32(0, typeId);
    stmt->setUInt64(1, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    // Keep the neighborhood's mirror in step: it builds this house at map load when the owner is offline.
    if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        neighborhood->UpdatePlotHouseType(_ownerGuid, typeId);

    // Without starter fixtures for the new type the rebuilt house had no roof selection and no entrance.
    if (changed)
    {
        if (_houseSize >= HOUSING_FIXTURE_SIZE_SMALL)
            RemapFixturesForHouseSize(_houseSize);
        PopulateStarterFixtures();
    }

    SyncUpdateFields();
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
    stmt->setUInt64(4, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    // Keep the neighborhood's mirror in step: it builds this house at map load when the owner is offline.
    if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        neighborhood->UpdatePlotHousePosition(_ownerGuid, Position(x, y, z, facing));
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
    stmt->setUInt64(4, _ownerGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

    if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(_neighborhoodGuid))
        neighborhood->UpdatePlotHousePosition(_ownerGuid, {});
}

void Housing::RelocateExteriorDecor(Position const& fromFrame, Position const& toFrame)
{
    float const turn = toFrame.GetOrientation() - fromFrame.GetOrientation();
    float const cz = std::cos(turn / 2.0f);
    float const sz = std::sin(turn / 2.0f);
    float const cosTo = std::cos(toFrame.GetOrientation());
    float const sinTo = std::sin(toFrame.GetOrientation());

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
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
        stmt->setUInt64(8, _ownerGuid.GetCounter());
        stmt->setUInt64(9, decorGuid.GetCounter());
        trans->Append(stmt);
    }
    CharacterDatabase.CommitTransaction(trans);
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
    stmt->setUInt64(index++, _ownerGuid.GetCounter());
    stmt->setUInt64(index++, roomGuid.GetCounter());
    CharacterDatabase.Execute(stmt);
}

void Housing::PersistFixtureToDB(uint32 fixturePointId, uint32 optionId)
{
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHARACTER_HOUSING_FIXTURE);
    stmt->setUInt32(0, optionId);
    stmt->setUInt64(1, _ownerGuid.GetCounter());
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

        // The door only exists if its hook is on the current house.
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
    // Starter house = Base + Roof roots ({ FixturePointId = componentID, OptionId = 0 }); only missing types are added.
    static constexpr uint8 starterTypes[] = { HOUSING_FIXTURE_TYPE_BASE, HOUSING_FIXTURE_TYPE_ROOF };

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

    uint64 ownerGuid = _ownerGuid.GetCounter();

    for (uint8 fixtureType : starterTypes)
    {
        if (existingRootTypes.contains(fixtureType))
            continue;

        uint32 compID = sHousingMgr.GetDefaultFixtureForType(fixtureType, _houseType, _houseSize);
        if (!compID)
        {
            TC_LOG_ERROR("housing", "Housing::PopulateStarterFixtures: No default component for type={} wmo={} size={} - skipping",
                fixtureType, _houseType, _houseSize);
            continue;
        }

        Fixture& fixture = _fixtures[compID];
        fixture.FixturePointId = compID;
        fixture.OptionId = 0;

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_FIXTURES);
        uint8 index = 0;
        stmt->setUInt64(index++, ownerGuid);
        stmt->setUInt32(index++, compID);
        stmt->setUInt32(index++, 0); // OptionId = 0 (default)
        CharacterDatabase.Execute(stmt);
    }

    // Every new house starts with a door at the first door hook on the base component; only this type's fixtures count.
    if (!HasCurrentTypeDoor())
    {
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
