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
#include <atomic>
#include <string>
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
        bool Locked = false;
        time_t PlacementTime = 0;
        uint8 SourceType = DECOR_SOURCE_STANDARD;
        std::string SourceValue;
    };

    struct Room
    {
        ObjectGuid Guid;
        uint32 RoomEntryId = 0;
        uint32 SlotIndex = 0;
        uint32 Orientation = 0;
        bool Mirrored = false;
        uint32 ThemeId = 0;
        uint32 WallpaperId = 0;
        uint32 MaterialId = 0;
        uint32 DoorTypeId = 0;
        uint8 DoorSlot = 0;
        uint32 CeilingTypeId = 0;
        uint8 CeilingSlot = 0;
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
        uint8 SourceType = DECOR_SOURCE_STANDARD;
        std::string SourceValue;
    };

    explicit Housing(Player* owner);

    // Global DB ID generators — must be called once during server startup
    // before any Housing objects are loaded, to prevent cross-player ID collisions.
    static void InitializeDbIdGenerators();

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
    void SetNeighborhoodGuid(ObjectGuid guid) { _neighborhoodGuid = guid; }
    ObjectGuid GetPlotGuid() const;
    uint8 GetPlotIndex() const { return _plotIndex; }
    uint32 GetCreateTime() const { return _createTime; }
    uint32 GetLevel() const { return _level; }
    uint32 GetFavor() const { return _favor; }
    uint32 GetSettingsFlags() const { return _settingsFlags; }
    ObjectGuid GetCosmeticOwnerGuid() const { return _cosmeticOwnerGuid; }
    void SetCosmeticOwnerGuid(ObjectGuid guid) { _cosmeticOwnerGuid = guid; }

    // Editor mode
    void SetEditorMode(HousingEditorMode mode);
    HousingEditorMode GetEditorMode() const { return _editorMode; }

    // Interior state tracking (set by door script, cleared on leave)
    void SetInInterior(bool interior) { _isInInterior = interior; }
    bool IsInInterior() const { return _isInInterior; }

    // Decor operations — StartPlacingNewDecor creates a pending placement, PlaceDecorWithGuid commits it
    ObjectGuid StartPlacingNewDecor(uint32 catalogEntryId, HousingResult& result);
    uint32 GetPendingPlacementEntryId(ObjectGuid decorGuid) const;
    void CancelPendingPlacement(ObjectGuid decorGuid);
    HousingResult PlaceDecorWithGuid(ObjectGuid decorGuid, uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid);
    HousingResult PlaceDecor(uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid);
    HousingResult MoveDecor(ObjectGuid decorGuid, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW);
    HousingResult RemoveDecor(ObjectGuid decorGuid);
    HousingResult CommitDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots);
    HousingResult SetDecorLocked(ObjectGuid decorGuid, bool locked);
    PlacedDecor const* GetPlacedDecor(ObjectGuid decorGuid) const;
    std::vector<PlacedDecor const*> GetAllPlacedDecor() const;
    uint32 GetDecorCount() const { return static_cast<uint32>(_placedDecor.size()); }

    // Auto-place starter decor in the visual room (called after catalog is populated).
    // Sniff-verified: retail pre-places starter items at fixed positions in Room 1.
    // The "Welcome Home" quest requires the player to remove 3 of these items.
    uint32 PlaceStarterDecor();

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
    HousingResult RemoveFixture(uint32 componentID, uint32* outHookID = nullptr);
    std::vector<Fixture const*> GetFixtures() const;
    std::unordered_map<uint32, uint32> GetFixtureOverrideMap() const;
    uint32 GetCoreExteriorComponentID() const;

    // Catalog operations
    HousingResult AddToCatalog(uint32 decorEntryId, uint8 sourceType = DECOR_SOURCE_STANDARD, std::string sourceValue = {});
    HousingResult RemoveFromCatalog(uint32 decorEntryId);
    HousingResult DestroyAllCopies(uint32 decorEntryId);
    std::vector<CatalogEntry const*> GetCatalogEntries() const;

    // House level and favor
    void AddLevel(uint32 amount);
    void AddFavor(uint64 amount, HousingFavorUpdateSource source = HOUSING_FAVOR_SOURCE_UNKNOWN);
    uint64 GetFavor64() const { return _favor64; }
    uint32 GetMaxDecorCount() const;

    // Budget tracking (WeightCost-based)
    uint32 GetInteriorDecorWeightUsed() const { return _interiorDecorWeightUsed; }
    uint32 GetExteriorDecorWeightUsed() const { return _exteriorDecorWeightUsed; }
    uint32 GetRoomWeightUsed() const { return _roomWeightUsed; }
    uint32 GetFixtureWeightUsed() const { return _fixtureWeightUsed; }
    uint32 GetMaxInteriorDecorBudget() const;
    uint32 GetMaxExteriorDecorBudget() const;
    uint32 GetMaxRoomBudget() const;
    uint32 GetMaxFixtureBudget() const;
    void RecalculateBudgets();

    // Level progression (QuestID-based)
    void OnQuestCompleted(uint32 questId);

    // UpdateField synchronization
    void SyncUpdateFields();

    // Settings
    void SaveSettings(uint32 settingsFlags);

    // House name and description
    std::string const& GetHouseName() const { return _houseName; }
    std::string const& GetHouseDescription() const { return _houseDescription; }
    void SetHouseNameDescription(std::string const& name, std::string const& desc);

    // Exterior lock state
    void SetExteriorLocked(bool locked);
    bool IsExteriorLocked() const { return _exteriorLocked; }

    // Photo sharing authorization (per-session, volatile)
    void SetPhotoSharingAuthorized(bool authorized) { _photoSharingAuthorized = authorized; }
    bool IsPhotoSharingAuthorized() const { return _photoSharingAuthorized; }

    // House size (HousingFixtureSize enum)
    void SetHouseSize(uint8 size);
    uint8 GetHouseSize() const { return _houseSize; }

    // House type (HouseExteriorWmoData ID)
    void SetHouseType(uint32 typeId);
    uint32 GetHouseType() const { return _houseType; }

    // House position persistence (player can reposition house on plot)
    bool HasCustomPosition() const { return _hasCustomPosition; }
    Position GetHousePosition() const { return Position(_housePosX, _housePosY, _housePosZ, _houseFacing); }
    void SetHousePosition(float x, float y, float z, float facing);

    // Direct access to placed decor map (for GO spawning)
    std::unordered_map<ObjectGuid, PlacedDecor> const& GetPlacedDecorMap() const { return _placedDecor; }
    bool IsStoragePopulated() const { return _storagePopulated; }
    void ResetStoragePopulated() { _storagePopulated = false; }

    // Populate ALL decor entries (placed + catalog) into the Account entity's FHousingStorage_C.
    // Called on-demand by REQUEST_STORAGE handler. Retail does NOT populate storage at login —
    // FHousingStorage_C is only sent when the player enters edit mode or requests storage.
    void PopulateCatalogStorageEntries();

private:
    uint64 GenerateDecorDbId();
    uint64 GenerateRoomDbId();

    // Room connectivity helpers
    ObjectGuid FindBaseRoomGuid() const;
    bool IsRoomGraphConnectedWithout(ObjectGuid excludeRoomGuid) const;

    // Immediate DB persistence helpers
    void PersistRoomToDB(ObjectGuid roomGuid, Room const& room);
    void PersistFixtureToDB(uint32 fixturePointId, uint32 optionId);

    Player* _owner;
    ObjectGuid _houseGuid;
    ObjectGuid _neighborhoodGuid;
    uint8 _plotIndex;
    uint32 _level;
    uint32 _favor;
    uint64 _favor64 = 0;
    uint32 _settingsFlags;
    HousingEditorMode _editorMode;
    bool _exteriorLocked = false;
    bool _isInInterior = false;
    uint8 _houseSize = HOUSING_FIXTURE_SIZE_SMALL;
    uint32 _houseType = 0;
    uint32 _createTime = 0;
    std::string _houseName;
    std::string _houseDescription;

    // Persisted house position on plot
    float _housePosX = 0.0f;
    float _housePosY = 0.0f;
    float _housePosZ = 0.0f;
    float _houseFacing = 0.0f;
    ObjectGuid _cosmeticOwnerGuid; // Display owner for guild housing
    bool _hasCustomPosition = false;
    bool _storagePopulated = false; // True after PopulateCatalogStorageEntries() — gates Account entity updates
    bool _photoSharingAuthorized = false; // Per-session photo sharing authorization state

    // WeightCost-based budget tracking
    uint32 _interiorDecorWeightUsed = 0;
    uint32 _exteriorDecorWeightUsed = 0;
    uint32 _roomWeightUsed = 0;
    uint32 _fixtureWeightUsed = 0;

    std::unordered_map<ObjectGuid, PlacedDecor> _placedDecor;
    std::unordered_map<ObjectGuid, uint32 /*decorEntryId*/> _pendingPlacements;
    std::unordered_map<ObjectGuid, Room> _rooms;
    std::unordered_map<uint32 /*fixturePointId*/, Fixture> _fixtures;
    std::unordered_map<uint32 /*decorEntryId*/, CatalogEntry> _catalog;

    // Global DB ID generators (atomic, shared across all Housing instances)
    static std::atomic<uint64> s_nextDecorDbId;
    static std::atomic<uint64> s_nextRoomDbId;
};

#endif // Housing_h__
