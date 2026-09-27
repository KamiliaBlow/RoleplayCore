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
#include <map>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

class Map;
class Player;
struct ExteriorComponentEntry;

// Placed decor is stored in world space but sent to the client relative to the room entity it attaches to
// (FMirroredPositionData_C.PositionLocalSpace): subtract the room origin and undo the room's yaw.
inline Position HousingWorldToRoomLocal(Position const& roomWorldPos, Position const& worldPos)
{
    float dx = worldPos.GetPositionX() - roomWorldPos.GetPositionX();
    float dy = worldPos.GetPositionY() - roomWorldPos.GetPositionY();
    float cosF = std::cos(roomWorldPos.GetOrientation());
    float sinF = std::sin(roomWorldPos.GetOrientation());
    return Position(cosF * dx + sinF * dy, -sinF * dx + cosF * dy, worldPos.GetPositionZ() - roomWorldPos.GetPositionZ());
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
        // Look of each component slot (12.1.0.69933: SET_COMPONENT_THEME / APPLY_COMPONENT_MATERIALS name the slots
        // they change). Slots without an entry fall back to the per-surface fields above.
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

    // Decor operations — StartPlacingNewDecor creates a pending placement, PlaceDecorWithGuid commits it
    // Mint a decor GUID from the global generator. Every decor GUID must come
    // from here or from StartPlacingNewDecor/PlaceDecor, all of which draw the
    // counter from s_nextDecorDbId, so ids stay unique across players and across
    // a reload. The deferred-redeem path used to compute its own from
    // (playerGuid * 100000 + entryId * 100 + index), which overflows its band
    // into another character's range as soon as the entry id passes 999.
    ObjectGuid GenerateDecorGuid(uint32 decorEntryId);
    ObjectGuid StartPlacingNewDecor(uint32 catalogEntryId, HousingResult& result);
    uint32 GetPendingPlacementEntryId(ObjectGuid decorGuid) const;
    void CancelPendingPlacement(ObjectGuid decorGuid);
    // scale: what the client placed with (HouseDecor InitialScale unless the player resized it); <= 0 means InitialScale.
    HousingResult PlaceDecorWithGuid(ObjectGuid decorGuid, uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid, float scale);
    HousingResult PlaceDecor(uint32 decorEntryId, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, ObjectGuid roomGuid);
    HousingResult MoveDecor(ObjectGuid decorGuid, float x, float y, float z,
        float rotX, float rotY, float rotZ, float rotW, float scale = 1.0f);
    HousingResult RemoveDecor(ObjectGuid decorGuid);
    // M2: single source of truth for exterior-vs-interior decor budget routing.
    // A placement is exterior (charged to the yard budget) when it has no room
    // (empty RoomGuid) OR its RoomGuid is the plot's base/exterior room identity
    // (HighGuid::Housing subType==2 whose low arg2 == base room entry id). Every
    // budget CHECK, CHARGE, refund and RecalculateBudgets classifies through this
    // so they can never disagree (the old bug: CHECK counted exterior-plot rooms
    // as exterior but CHARGE routed them to interior → exterior budget unlimited).
    static bool IsExteriorDecorPlacement(ObjectGuid roomGuid);
    // Reference point for the decor spatial sanity check: interior origin for interior decor, the owner's
    // position (who must stand on the plot to place) for plot decor.
    Position GetDecorPlacementAnchor(ObjectGuid roomGuid) const;
    // Interior decor must end up inside one of the house's rooms (RoomWmoData bounding box). The client lets a
    // player push an item through a wall with collision disabled; without this it was saved outside every room.
    HousingResult CheckInteriorDecorBounds(ObjectGuid roomGuid, float x, float y, float z) const;
    HousingResult CommitDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots);
    HousingResult SetDecorLocked(ObjectGuid decorGuid, bool locked);
    // Bind (or, with an empty petGuid, clear) a battle pet on a placed decor slot.
    HousingResult SetDecorPet(ObjectGuid decorGuid, ObjectGuid petGuid, uint8 petFlag);
    // Wipe placed decor for a HousingHouseScope (1=Interior, 2=Exterior). Returns the count removed.
    HousingResult ResetDecor(uint8 scope, uint32* outRemoved = nullptr);
    PlacedDecor const* GetPlacedDecor(ObjectGuid decorGuid) const;
    std::vector<PlacedDecor const*> GetAllPlacedDecor() const;
    uint32 GetDecorCount() const { return static_cast<uint32>(_placedDecor.size()); }

    // Auto-place starter decor in the visual room (called after catalog is populated).
    // Sniff-verified: retail pre-places starter items at fixed positions in Room 1.
    // The "Welcome Home" quest requires the player to remove 3 of these items.
    uint32 PlaceStarterDecor();

    // Room operations
    HousingResult PlaceRoom(uint32 roomEntryId, uint32 slotIndex, uint32 orientation, bool mirrored, ObjectGuid* outRoomGuid = nullptr, int32 gridX = 0, int32 gridY = 0, int32 floorIndex = 0);
    HousingResult RemoveRoom(ObjectGuid roomGuid);
    HousingResult RotateRoom(ObjectGuid roomGuid, bool clockwise);
    HousingResult MoveRoom(ObjectGuid roomGuid, uint32 newSlotIndex, ObjectGuid swapRoomGuid, uint32 swapSlotIndex);
    HousingResult ApplyRoomTheme(ObjectGuid roomGuid, uint32 themeSetId, std::vector<uint32> const& optionIds);
    HousingResult ApplyRoomMaterial(ObjectGuid roomGuid, uint32 textureId, int32 colorOverride, std::vector<uint32> const& optionIds);
    HousingResult SetDoorType(ObjectGuid roomGuid, uint32 doorTypeId, uint8 doorSlot);
    HousingResult SetCeilingType(ObjectGuid roomGuid, uint32 ceilingTypeId, uint8 ceilingSlot);
    std::vector<Room const*> GetRooms() const;
    Room const* GetRoom(ObjectGuid roomGuid) const;
    ObjectGuid GetBaseRoomGuid() const { return FindBaseRoomGuid(); }
    uint32 GetNextRoomSlotIndex() const;
    // Blueprint imports: drop every room but the base room (their decor must already be removed), copy a room's theme,
    // material, door and ceiling choices, replace every fixture choice.
    void RemoveAllNonBaseRooms();
    void SetRoomAppearance(ObjectGuid roomGuid, Room const& appearance);
    void ReplaceFixtures(std::vector<Fixture> const& fixtures);
    std::unordered_map<ObjectGuid, Room> const& GetRoomsMap() const { return _rooms; }

    // Interior layout geometry (retail 12.1.0.69933). A room sits at (GridX, GridY) yards from the interior origin and
    // turns counter-clockwise in quarter steps (Orientation * pi/2). Its doors are the connectable wall slots on the
    // horizontal plane; two rooms on one floor are connected where a door of each meets at the same point. A stairwell
    // also lists its connectable floor and ceiling: those link the two stacked halves of the stairwell.
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
    /// Housing/3 house GUID (12.1.0.69933 sniff): arg1 = the neighborhood's NeighborhoodMapID, arg2 = 7, low = the
    /// Battle.net account. One account's Alliance and Horde houses differ only in arg1 (0x...8007 / 0x...10007).
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

    // Fixture operations
    HousingResult SelectFixtureOption(uint32 fixturePointId, uint32 optionId, std::vector<uint32>* removedHookIDs = nullptr);
    // Re-keys the fixtures on oldCompId's hooks to the equivalent hooks (same fixture type and rank) of newCompId.
    void MoveHookFixtures(uint32 oldCompId, uint32 newCompId);
    HousingResult RemoveFixture(uint32 componentID, uint32* outHookID = nullptr);
    std::vector<Fixture const*> GetFixtures() const;
    std::unordered_map<uint32, uint32> GetFixtureOverrideMap() const;
    uint32 GetCoreExteriorComponentID() const;
    std::unordered_map<uint8, uint32> GetRootComponentOverrides() const;

    // Catalog operations
    HousingResult AddToCatalog(uint32 decorEntryId, uint8 sourceType = DECOR_SOURCE_STANDARD, std::string sourceValue = {});
    HousingResult RemoveFromCatalog(uint32 decorEntryId);
    HousingResult DestroyAllCopies(uint32 decorEntryId);
    std::vector<CatalogEntry const*> GetCatalogEntries() const;

    // House level and favor
    void AddLevel(uint32 amount);
    void AddFavor(uint64 amount, HousingFavorUpdateSource source = HOUSING_FAVOR_SOURCE_UNKNOWN, bool emitUpdate = true);
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
    // Back to the plot's DB2 house position (e.g. after moving to another plot).
    void ResetHousePosition();
    // Exterior decor is stored in world space: when the house moves to another plot, carry every yard piece over,
    // keeping its place relative to the plot room (fromFrame/toFrame: plot room position and yaw).
    void RelocateExteriorDecor(Position const& fromFrame, Position const& toFrame);

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
    // Room budget cost of a room at this spot: the upper half of a stairwell (a stairs room stacked on another one) is free,
    // the stairwell was paid for once.
    uint32 GetRoomWeightCost(uint32 roomEntryId, int32 gridX, int32 gridY, int32 floorIndex, ObjectGuid self) const;
    // Re-places a room (grid position + orientation) and carries its placed decor along. Persists both.
    void SetRoomPlacement(Room& room, int32 gridX, int32 gridY, uint32 orientation);

    // Immediate DB persistence helpers
    void PersistRoomToDB(ObjectGuid roomGuid, Room const& room);
    void PersistFixtureToDB(uint32 fixturePointId, uint32 optionId);

    // Populate starter fixtures (Base + Roof) on house creation
    void PopulateStarterFixtures();

    // Fixtures of every house type stay stored (switching back restores them); these pick out the current one's.
    bool IsCurrentTypeComponent(ExteriorComponentEntry const* comp) const;
    bool HasCurrentTypeDoor() const;

    // #16 Outdoor Lighting (A4): enforce the 12.0.7 "two lights cannot overlap"
    // rule. Only applies when placing/moving a Lighting-category decor on the
    // exterior/plot scope; rejects with HOUSING_RESULT_INVALID_LIGHT_OVERLAP if
    // another exterior light sits within HOUSING_LIGHT_OVERLAP_RADIUS. excludeGuid
    // skips the decor being moved so an in-place move never collides with itself.
    HousingResult CheckLightOverlap(uint32 decorEntryId, float x, float y, float z,
        bool isExterior, ObjectGuid excludeGuid = ObjectGuid::Empty) const;

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
