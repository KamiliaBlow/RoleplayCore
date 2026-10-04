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
#include "Position.h"
#include "QuaternionData.h"
#include <array>
#include <atomic>
#include <cmath>
#include <map>
#include <unordered_set>
#include <string>
#include <unordered_map>
#include <vector>

class Map;
class Player;
struct ExteriorComponentEntry;

// Placed decor is sent to the client relative to its room (FMirroredPositionData_C): subtract the room origin and yaw.
inline Position HousingWorldToRoomLocal(Position const& roomWorldPos, Position const& worldPos)
{
    float dx = worldPos.GetPositionX() - roomWorldPos.GetPositionX();
    float dy = worldPos.GetPositionY() - roomWorldPos.GetPositionY();
    float cosF = std::cos(roomWorldPos.GetOrientation());
    float sinF = std::sin(roomWorldPos.GetOrientation());
    return Position(cosF * dx + sinF * dy, -sinF * dx + cosF * dy, worldPos.GetPositionZ() - roomWorldPos.GetPositionZ());
}

// Rotation counterpart of HousingWorldToRoomLocal: the client composes worldRot = roomRot ⊗ localRot.
inline QuaternionData HousingWorldRotationToRoomLocal(float roomWorldYaw, QuaternionData const& worldRot)
{
    float z, y, x;
    worldRot.toEulerAnglesZYX(z, y, x);
    return QuaternionData::fromEulerAnglesZYX(z - roomWorldYaw, y, x);
}

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
        float Scale = 1.0f;
        std::array<uint32, MAX_HOUSING_DYE_SLOTS> DyeSlots = {};
        ObjectGuid RoomGuid;
        bool Locked = false;
        time_t PlacementTime = 0;
        uint8 SourceType = DECOR_SOURCE_STANDARD;
        std::string SourceValue;
        ObjectGuid PetGuid;         // battle-pet bound to this decor slot (empty = none)
        uint8 PetFlag = 0;          // client-sent flag accompanying the pet binding
        ObjectGuid ParentDecorGuid; // decor this item is stacked on (empty = none; client groups moves by it)
    };

    struct Room
    {
        ObjectGuid Guid;
        uint32 RoomEntryId = 0;
        uint32 SlotIndex = 0;
        int32 GridX = 0;        // 2D grid position (yard offsets from origin)
        int32 GridY = 0;
        int32 FloorIndex = 0;  // 0=ground, 1+=upper floors
        uint32 Orientation = 0;
        bool Mirrored = false;
        uint32 ThemeId = 0;           // Legacy single-theme; kept for back-compat
        uint32 WallThemeId = 0;       // Per-surface theme (HouseTheme ID)
        uint32 FloorThemeId = 0;
        uint32 CeilingThemeId = 0;
        uint32 WallTextureId = 0;     // RoomComponentTexture ID for walls
        uint32 FloorTextureId = 0;    // RoomComponentTexture ID for floors
        uint32 CeilingTextureId = 0;  // RoomComponentTexture ID for ceilings
        int32 ColorOverride = -1;     // Shared color override (-1 = default)
        uint32 DoorTypeId = 0;        // last SET_DOOR_TYPE (component, variant); blueprints carry this pair
        uint8 DoorSlot = 0;
        std::map<uint32 /*componentId*/, uint8 /*variant*/> DoorTypes; // doorway style of each door
        // Look of each component slot; falls back to the per-surface fields above.
        std::map<uint32 /*componentId*/, uint32 /*HouseThemeID*/> ComponentThemes;
        std::map<uint32 /*componentId*/, uint32 /*RoomComponentTextureID*/> ComponentTextures;
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

    // ownerGuid: the character that bought the house and keys its rows (retail CosmeticOwner); defaults to the player.
    explicit Housing(Player* owner, ObjectGuid ownerGuid = ObjectGuid::Empty);

    // Must run once at startup, before any Housing objects are loaded, to prevent cross-player ID collisions.
    static void InitializeDbIdGenerators();

    bool LoadFromDB(PreparedQueryResult housing, PreparedQueryResult decor,
        PreparedQueryResult rooms, PreparedQueryResult fixtures, PreparedQueryResult catalog);
    void SaveToDB(CharacterDatabaseTransaction trans);
    static void DeleteFromDB(ObjectGuid::LowType ownerGuid, CharacterDatabaseTransaction trans);

    HousingResult Create(ObjectGuid neighborhoodGuid, uint8 plotIndex);
    void Delete();

    Player* GetOwner() const { return _owner; }
    // Character that bought the house (row key, retail CosmeticOwner).
    ObjectGuid GetOwnerGuid() const { return _ownerGuid; }
    // House settings owner change: the house, its rows and its plot move to another character of the account.
    HousingResult ChangeOwner(ObjectGuid newOwnerGuid);
    ObjectGuid GetHouseGuid() const { return _houseGuid; }
    ObjectGuid GetNeighborhoodGuid() const { return _neighborhoodGuid; }
    // NEIGHBORHOOD_FACTION_* of the neighborhood the house stands in; the owner's team only when unknown.
    int32 GetNeighborhoodFaction() const;
    void SetNeighborhoodGuid(ObjectGuid guid) { _neighborhoodGuid = guid; }
    ObjectGuid GetPlotGuid() const;
    uint8 GetPlotIndex() const { return _plotIndex; }
    void SetPlotIndex(uint8 plotIndex) { _plotIndex = plotIndex; }
    uint32 GetCreateTime() const { return _createTime; }
    uint32 GetLevel() const { return _level; }
    uint32 GetFavor() const { return _favor; }
    uint32 GetSettingsFlags() const { return _settingsFlags; }
    ObjectGuid GetCosmeticOwnerGuid() const { return _cosmeticOwnerGuid; }
    void SetCosmeticOwnerGuid(ObjectGuid guid) { _cosmeticOwnerGuid = guid; }

    // Editor mode
    void SetEditorMode(HousingEditorMode mode);
    HousingEditorMode GetEditorMode() const { return _editorMode; }
    // SMSG_HOUSING_HOUSE_STATUS_RESPONSE trailing bits: 0x80 decor, 0x40 layout, 0x20 fixture edit mode active.
    uint8 GetEditModeStatusFlags() const
    {
        switch (_editorMode)
        {
            case HOUSING_EDITOR_MODE_BASIC_DECOR:
            case HOUSING_EDITOR_MODE_EXPERT_DECOR:
                return 0x80;
            case HOUSING_EDITOR_MODE_LAYOUT:
                return 0x40;
            case HOUSING_EDITOR_MODE_EXTERIOR_CUSTOMIZATION:
                return 0x20;
            default:
                return 0x00;
        }
    }

    // Interior state tracking (set by door script, cleared on leave)
    void SetInInterior(bool interior) { _isInInterior = interior; }
    bool IsInInterior() const { return _isInInterior; }

    // Decor operations - StartPlacingNewDecor creates a pending placement, PlaceDecorWithGuid commits it.
    // Mint decor GUIDs from the global generator so ids stay unique across players and reloads.
    ObjectGuid GenerateDecorGuid(uint32 decorEntryId);
    ObjectGuid StartPlacingNewDecor(uint32 catalogEntryId, HousingResult& result);
    uint32 GetPendingPlacementEntryId(ObjectGuid decorGuid) const;
    void CancelPendingPlacement(ObjectGuid decorGuid);
    // scale: what the client placed with (HouseDecor InitialScale unless the player resized it); <= 0 means InitialScale.
    HousingResult PlaceDecorWithGuid(ObjectGuid decorGuid, uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid, float scale,
        ObjectGuid parentDecorGuid = ObjectGuid::Empty);
    HousingResult PlaceDecor(uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid);
    // movedChildren (optional) receives the stacked decor transformed along with the moved item.
    HousingResult MoveDecor(ObjectGuid decorGuid, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, float scale = 1.0f,
        ObjectGuid parentDecorGuid = ObjectGuid::Empty,
        std::vector<ObjectGuid>* movedChildren = nullptr);
    // removedChildren (optional) receives the stacked decor returned to storage along with the item
    // (guid + acquisition source for its storage entry).
    HousingResult RemoveDecor(ObjectGuid decorGuid,
        std::vector<std::pair<ObjectGuid, std::pair<uint8, std::string>>>* removedChildren = nullptr);
    // Exterior (yard-budget) placements: empty RoomGuid or the plot's base/exterior room identity; every budget path classifies through this.
    static bool IsExteriorDecorPlacement(ObjectGuid roomGuid);
    // Interior origin for interior decor; the owner's position (must stand on the plot) for plot decor.
    Position GetDecorPlacementAnchor(ObjectGuid roomGuid) const;
    // Interior decor must end up inside one of the house's rooms (RoomWmoData bounding box).
    HousingResult CheckInteriorDecorBounds(ObjectGuid roomGuid, float x, float y, float z) const;

    // The room whose footprint contains the position (room GUIDs arrive empty from the client).
    ObjectGuid FindRoomGuidAtPosition(float x, float y, float z) const;
    // consumeDyes: take one DyeColor.ItemID per newly dyed slot (player dyeing; blueprint imports pass false).
    HousingResult CommitDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots, bool consumeDyes = true);
    HousingResult SetDecorLocked(ObjectGuid decorGuid, bool locked);
    // Bind (or, with an empty petGuid, clear) a battle pet on a placed decor slot.
    HousingResult SetDecorPet(ObjectGuid decorGuid, ObjectGuid petGuid, uint8 petFlag);
    // Wipe placed decor for a HousingHouseScope (1=Interior, 2=Exterior). Returns the count removed.
    HousingResult ResetDecor(uint8 scope, uint32* outRemoved = nullptr);
    PlacedDecor const* GetPlacedDecor(ObjectGuid decorGuid) const;
    std::vector<PlacedDecor const*> GetAllPlacedDecor() const;

    // Room operations
    HousingResult PlaceRoom(uint32 roomEntryId, uint32 slotIndex, uint32 orientation, bool mirrored, ObjectGuid* outRoomGuid = nullptr, int32 gridX = 0, int32 gridY = 0, int32 floorIndex = 0);
    HousingResult RemoveRoom(ObjectGuid roomGuid);
    HousingResult RotateRoom(ObjectGuid roomGuid, bool clockwise);
    HousingResult MoveRoom(ObjectGuid roomGuid, ObjectGuid targetRoomGuid, uint32 sourceDoorComponentId, uint32 targetDoorComponentId);
    HousingResult ApplyRoomTheme(ObjectGuid roomGuid, uint32 themeSetId, std::vector<uint32> const& optionIds);
    HousingResult ApplyRoomMaterial(ObjectGuid roomGuid, uint32 textureId, int32 colorOverride, std::vector<uint32> const& optionIds);
    HousingResult SetDoorType(ObjectGuid roomGuid, uint32 doorTypeId, uint8 doorSlot);
    HousingResult SetCeilingType(ObjectGuid roomGuid, uint32 ceilingTypeId, uint8 ceilingSlot);
    std::vector<Room const*> GetRooms() const;
    Room const* GetRoom(ObjectGuid roomGuid) const;
    ObjectGuid GetBaseRoomGuid() const { return FindBaseRoomGuid(); }
    uint32 GetNextRoomSlotIndex() const;
    // Blueprint imports: drop every room but the base room (their decor must already be removed).
    void RemoveAllNonBaseRooms();
    void SetRoomAppearance(ObjectGuid roomGuid, Room const& appearance);
    void ReplaceFixtures(std::vector<Fixture> const& fixtures);
    std::unordered_map<ObjectGuid, Room> const& GetRoomsMap() const { return _rooms; }

    // Interior layout grid: rooms sit at (GridX, GridY) yards, Orientation in quarter turns; two rooms connect where a door of each meets.
    struct RoomDoor
    {
        uint32 ComponentId = 0;
        uint8 ComponentType = 0; // HousingRoomComponentType (FHousingDoorData.RoomComponentType: 1 wall, 2 floor, 3 ceiling)
        Position Local;          // RoomComponent.OffsetPos (FHousingDoorData.RoomComponentOffset)
        float X = 0.0f;          // door point, grid space
        float Y = 0.0f;
        int32 DirX = 0;          // outward direction, grid space
        int32 DirY = 0;
        int32 DirZ = 0;          // -1 floor / +1 ceiling of a stairwell, 0 for walls

        bool IsVertical() const { return DirZ != 0; }
    };

    // character_housing_rooms.doorTypes: "componentId:variant,componentId:variant"
    static std::string SerializeDoorTypes(Room const& room);
    static void LoadDoorTypes(Room& room, std::string const& doorTypes);
    /// Housing/3 house GUID: arg1 = the neighborhood's NeighborhoodMapID, arg2 = 7, low = the Battle.net account.
    static ObjectGuid MakeHouseGuid(uint32 neighborhoodMapId, uint32 bnetAccountId);
    static std::string SerializeComponentStyles(Room const& room);
    static void LoadComponentStyles(Room& room, std::string const& componentStyles);

    static void RotateRoomOffset(float x, float y, uint32 orientation, float& outX, float& outY);
    static std::vector<RoomDoor> GetRoomDoors(uint32 roomEntryId, float gridX, float gridY, uint32 orientation);
    static std::vector<RoomDoor> GetRoomDoors(Room const& room) { return GetRoomDoors(room.RoomEntryId, float(room.GridX), float(room.GridY), room.Orientation); }
    // The room whose door meets `door` of `room` (and that door's component), or nullptr.
    static Room const* FindRoomAtDoor(std::vector<Room const*> const& rooms, Room const& room, RoomDoor const& door, uint32* outComponentId = nullptr);
    // Which side of a connection builds the doorway: never the base room, otherwise the lower slot.
    static bool OwnsDoorway(Room const& room, Room const& other);
    // Door variant of a connection (RoomComponentOption.RoomComponentID): the one picked for either side, else 2.
    static uint8 GetDoorwayVariant(Room const& room, uint32 componentId, Room const& other, uint32 otherComponentId);
    // True when a room of this kind at this placement does not overlap any other room on its floor.
    static bool RoomFits(std::vector<Room const*> const& rooms, uint32 roomEntryId, int32 gridX, int32 gridY, int32 floorIndex,
        uint32 orientation, ObjectGuid ignoreRoom = ObjectGuid::Empty);
    // Places a room of this kind, turned to `orientation`, so that one of its doors faces `target`. Fills the grid position.
    static bool FitRoomToDoor(std::vector<Room const*> const& rooms, uint32 roomEntryId, int32 floorIndex, RoomDoor const& target,
        uint32 orientation, ObjectGuid ignoreRoom, int32& gridX, int32& gridY);
    // Stairwells are two stacked rooms at one XY; returns the other half, or nullptr.
    Room const* FindStairwellPartner(Room const& room) const;
    // The whole stack of stairwell halves in one column (same cell, contiguous floors), bottom first.
    std::vector<Room const*> GetStairwellColumn(Room const& room) const;

    // Fixture operations
    HousingResult SelectFixtureOption(uint32 fixturePointId, uint32 optionId, std::vector<uint32>* removedHookIDs = nullptr);
    // Re-keys the fixtures on oldCompId's hooks to the equivalent hooks (same fixture type and rank) of newCompId.
    void MoveHookFixtures(uint32 oldCompId, uint32 newCompId);
    // Re-resolves Base/Roof root fixtures stored at the old size to the same style at newSize; call after _houseSize changes.
    void RemapFixturesForHouseSize(uint8 newSize);
    HousingResult RemoveFixture(uint32 componentID, uint32* outHookID = nullptr);
    std::vector<Fixture const*> GetFixtures() const;
    std::unordered_map<uint32, uint32> GetFixtureOverrideMap() const;
    uint32 GetCoreExteriorComponentID() const;
    std::unordered_map<uint8, uint32> GetRootComponentOverrides() const;

    // Catalog operations
    HousingResult AddToCatalog(uint32 decorEntryId, uint8 sourceType = DECOR_SOURCE_STANDARD, std::string sourceValue = {});
    HousingResult RemoveFromCatalog(uint32 decorEntryId);
    HousingResult DestroyAllCopies(uint32 decorEntryId);
    // Resolves an unplaced catalog copy to its synthetic storage GUID (the band PopulateOwnStorageEntries emits); grants no new copies.
    ObjectGuid MintStorageDecorInstance(uint32 decorEntryId, HousingResult& result);
    std::vector<CatalogEntry const*> GetCatalogEntries() const;

    // House level and favor
    void AddLevel(uint32 amount);
    void SetLevel(uint32 level);
    void AddFavor(uint64 amount, HousingFavorUpdateSource source = HOUSING_FAVOR_SOURCE_UNKNOWN, bool emitUpdate = true);
    uint64 GetFavor64() const { return _favor64; }

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

    // Rewards of HouseLevelData levels fromLevel..toLevel not granted yet (award quest RewardSpell).
    void GrantLevelAwards(uint32 fromLevel, uint32 toLevel);
    // SMSG_HOUSING_SVCS_UPDATE_HOUSES_LEVEL_FAVOR for this house; -1 = unchanged.
    void SendLevelFavorUpdate(int32 level, int32 favor) const;

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
    // Size the player picked in the exterior editor; not stored, after a relog it is the current size.
    uint8 GetPreferredHouseSize() const { return _preferredHouseSize ? _preferredHouseSize : _houseSize; }
    void SetPreferredHouseSize(uint8 size) { _preferredHouseSize = size; }

    // House type (HouseExteriorWmoData ID)
    void SetHouseType(uint32 typeId);
    uint32 GetHouseType() const { return _houseType; }

    // House position persistence (player can reposition house on plot)
    bool HasCustomPosition() const { return _hasCustomPosition; }
    Position GetHousePosition() const { return Position(_housePosX, _housePosY, _housePosZ, _houseFacing); }
    void SetHousePosition(float x, float y, float z, float facing);
    // Back to the plot's DB2 house position (e.g. after moving to another plot).
    void ResetHousePosition();
    // Carry every yard piece over when the house moves to another plot, keeping its place relative to the plot room.
    void RelocateExteriorDecor(Position const& fromFrame, Position const& toFrame);

    // Direct access to placed decor map (for GO spawning)
    std::unordered_map<ObjectGuid, PlacedDecor> const& GetPlacedDecorMap() const { return _placedDecor; }
    bool IsStoragePopulated() const { return _storagePopulated; }
    void ResetStoragePopulated() { _storagePopulated = false; }

    // Houses from before the door grant get the front door placed into the entry hall once;
    // a door already placed or withdrawn to storage is never re-placed. Returns the placed row.
    PlacedDecor const* EnsureStarterDoorPlaced();
    // Interior arrival point: anchored to the placed front door, entry hall origin without one.
    Position GetInteriorEntryPosition() const;

    // Fills the account's FHousingStorage_C with the decor of every house of the account, on demand (REQUEST_STORAGE / edit mode).
    void PopulateCatalogStorageEntries();
    void PopulateOwnStorageEntries();

private:
    uint64 GenerateDecorDbId();
    uint64 GenerateRoomDbId();

    // Room connectivity helpers
    ObjectGuid FindBaseRoomGuid() const;
    bool IsRoomGraphConnectedWithout(ObjectGuid excludeRoomGuid) const;
    bool IsRoomGraphConnectedWithout(std::unordered_set<ObjectGuid> const& excludeRoomGuids) const;
    // Room budget cost at this spot: the upper half of a stairwell is free, the stairwell was paid for once.
    uint32 GetRoomWeightCost(uint32 roomEntryId, int32 gridX, int32 gridY, int32 floorIndex, ObjectGuid self) const;
    // Re-places a room (grid position + orientation) and carries its placed decor along. Persists both.
    void SetRoomPlacement(Room& room, int32 gridX, int32 gridY, uint32 orientation, int32 floorIndex = INT32_MIN);

    // Immediate DB persistence helpers
    void PersistRoomToDB(ObjectGuid roomGuid, Room const& room);
    void PersistFixtureToDB(uint32 fixturePointId, uint32 optionId);

    // Populate starter fixtures (Base + Roof) on house creation
    void PopulateStarterFixtures();

    // Fixtures of every house type stay stored (switching back restores them); these pick out the current one's.
    bool IsCurrentTypeComponent(ExteriorComponentEntry const* comp) const;
    bool HasCurrentTypeDoor() const;

    // Rejects exterior Lighting decor within HOUSING_LIGHT_OVERLAP_RADIUS of another exterior light; excludeGuid skips the decor being moved.
    HousingResult CheckLightOverlap(uint32 decorEntryId, float x, float y, float z,
        bool isExterior, ObjectGuid excludeGuid = ObjectGuid::Empty) const;

    Player* _owner;
    ObjectGuid _ownerGuid;
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
    uint8 _preferredHouseSize = HOUSING_FIXTURE_SIZE_NONE;
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
