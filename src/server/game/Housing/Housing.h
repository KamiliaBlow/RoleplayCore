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

#ifndef Housing_h__
#define Housing_h__

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Position.h"
#include <array>
#include <unordered_map>
#include <vector>

class Map;
class Player;

class TC_GAME_API Housing
{
public:
    struct PlacedDecor
    {
        ObjectGuid Guid;
        uint32 DecorEntryId = 0;
        float PosX = 0.0f;
        float PosY = 0.0f;
        float PosZ = 0.0f;
        float RotationX = 0.0f;
        float RotationY = 0.0f;
        float RotationZ = 0.0f;
        float RotationW = 1.0f;
        std::array<uint32, MAX_HOUSING_DYE_SLOTS> DyeSlots = {};
        ObjectGuid RoomGuid;
    };

    struct Room
    {
        ObjectGuid Guid;
        uint32 RoomEntryId = 0;
        uint32 SlotIndex = 0;
        uint32 Orientation = 0;
        bool Mirrored = false;
        uint32 ThemeId = 0;
    };

    struct Fixture
    {
        uint32 FixturePointId = 0;
        uint32 OptionId = 0;
    };

    struct CatalogEntry
    {
        uint32 DecorEntryId = 0;
        uint32 Count = 0;
    };

    explicit Housing(Player* owner);

    bool LoadFromDB(PreparedQueryResult housing, PreparedQueryResult decor,
        PreparedQueryResult rooms, PreparedQueryResult fixtures, PreparedQueryResult catalog);
    void SaveToDB(CharacterDatabaseTransaction trans);
    static void DeleteFromDB(ObjectGuid::LowType ownerGuid, CharacterDatabaseTransaction trans);

    HousingResult Create(ObjectGuid neighborhoodGuid, uint8 plotIndex);
    void Delete();

    // Getters
    Player* GetOwner() const { return _owner; }
    ObjectGuid GetHouseGuid() const { return _houseGuid; }
    ObjectGuid GetNeighborhoodGuid() const { return _neighborhoodGuid; }
    uint8 GetPlotIndex() const { return _plotIndex; }
    uint32 GetLevel() const { return _level; }
    uint32 GetFavor() const { return _favor; }
    uint32 GetSettingsFlags() const { return _settingsFlags; }

    // Editor mode
    void SetEditorMode(HousingEditorMode mode) { _editorMode = mode; }
    HousingEditorMode GetEditorMode() const { return _editorMode; }

    // Decor operations
    HousingResult PlaceDecor(uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid);
    HousingResult MoveDecor(ObjectGuid decorGuid, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW);
    HousingResult RemoveDecor(ObjectGuid decorGuid);
    HousingResult CommitDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots);
    PlacedDecor const* GetPlacedDecor(ObjectGuid decorGuid) const;
    std::vector<PlacedDecor const*> GetAllPlacedDecor() const;
    uint32 GetDecorCount() const { return static_cast<uint32>(_placedDecor.size()); }

    // Room operations
    HousingResult PlaceRoom(uint32 roomEntryId, uint32 slotIndex, uint32 orientation, bool mirrored);
    HousingResult RemoveRoom(ObjectGuid roomGuid);
    HousingResult RotateRoom(ObjectGuid roomGuid, bool clockwise);
    HousingResult MoveRoom(ObjectGuid roomGuid, uint32 newSlotIndex, ObjectGuid swapRoomGuid, uint32 swapSlotIndex);
    HousingResult ApplyRoomTheme(ObjectGuid roomGuid, uint32 themeSetId, std::vector<uint32> const& componentIds);
    HousingResult ApplyRoomWallpaper(ObjectGuid roomGuid, uint32 wallpaperId, uint32 materialId, std::vector<uint32> const& componentIds);
    HousingResult SetDoorType(ObjectGuid roomGuid, uint32 doorTypeId, uint8 doorSlot);
    HousingResult SetCeilingType(ObjectGuid roomGuid, uint32 ceilingTypeId, uint8 ceilingSlot);
    std::vector<Room const*> GetRooms() const;

    // Fixture operations
    HousingResult SelectFixtureOption(uint32 fixturePointId, uint32 optionId);
    HousingResult RemoveFixture(uint32 fixturePointId);
    std::vector<Fixture const*> GetFixtures() const;

    // Catalog operations
    HousingResult AddToCatalog(uint32 decorEntryId);
    HousingResult RemoveFromCatalog(uint32 decorEntryId);
    HousingResult DestroyAllCopies(uint32 decorEntryId);
    std::vector<CatalogEntry const*> GetCatalogEntries() const;

    // House level and favor
    void AddFavor(uint32 amount);
    uint32 GetMaxDecorCount() const;

    // Settings
    void SaveSettings(uint32 settingsFlags);

private:
    uint64 GenerateDecorDbId();
    uint64 GenerateRoomDbId();

    Player* _owner;
    ObjectGuid _houseGuid;
    ObjectGuid _neighborhoodGuid;
    uint8 _plotIndex;
    uint32 _level;
    uint32 _favor;
    uint32 _settingsFlags;
    HousingEditorMode _editorMode;

    std::unordered_map<ObjectGuid, PlacedDecor> _placedDecor;
    std::unordered_map<ObjectGuid, Room> _rooms;
    std::unordered_map<uint32 /*fixturePointId*/, Fixture> _fixtures;
    std::unordered_map<uint32 /*decorEntryId*/, CatalogEntry> _catalog;

    uint64 _decorDbIdGenerator;
    uint64 _roomDbIdGenerator;
};

#endif // Housing_h__
