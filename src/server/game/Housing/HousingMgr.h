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

#ifndef HousingMgr_h__
#define HousingMgr_h__

#include "Define.h"
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <string>
#include <unordered_map>
#include <vector>

class Neighborhood;
struct ExteriorComponentEntry;
struct ExteriorComponentExitPointEntry;
struct ExteriorComponentHookEntry;
struct RoomComponentOptionEntry;
struct RoomComponentTextureEntry;

struct HouseDecorData
{
    uint32 ID = 0;
    std::string Name;
    float InitialRotation[3] = {};
    int32 Field_003 = 0;
    int32 GameObjectID = 0;
    int32 Flags = 0;
    uint8 Type = 0;
    uint8 ModelType = 0;
    int32 ModelFileDataID = 0;
    int32 ThumbnailFileDataID = 0;
    int32 WeightCost = 1;
    int32 ItemID = 0;
    float InitialScale = 1.0f;
    int32 FirstAcquisitionBonus = 0;     // House XP gained on first acquisition (from Lua API)
    int32 OrderIndex = 0;
    int8 Size = 0;                       // HousingCatalogEntrySize (inferred from Lua API)
    int32 StartingQuantity = 0;
    int32 UiModelSceneID = 0;
};

struct HouseLevelData
{
    uint32 ID = 0;
    int32 Level = 0;
    int32 QuestID = 0;
    // Budget fields populated from fallback defaults (not in HouseLevelData DB2)
    int32 InteriorDecorPlacementBudget = 0;
    int32 ExteriorDecorPlacementBudget = 0;
    int32 RoomPlacementBudget = 0;
    int32 ExteriorFixtureBudget = 0;
};

struct HouseRoomData
{
    uint32 ID = 0;
    std::string Name;
    int8 Size = 0;
    int32 Flags = 0;            // HousingRoomFlags bitmask
    int32 Field_002 = 0;
    int32 RoomWmoDataID = 0;
    int32 UiTextureAtlasElementID = 0;
    int32 WeightCost = 1;

    bool IsBaseRoom() const { return (Flags & HOUSING_ROOM_FLAG_BASE_ROOM) != 0; }
    bool HasStairs() const { return (Flags & HOUSING_ROOM_FLAG_HAS_STAIRS) != 0; }
};

struct RoomDoorInfo
{
    uint32 RoomComponentID = 0;
    float OffsetPos[3] = {};
    float OffsetRot[3] = {};
    uint8 ConnectionType = 0;   // RoomConnectionType
};

/// Full room component data (all types: wall, floor, ceiling, stairs, pillar, doorway)
struct RoomComponentData
{
    uint32 ID = 0;
    uint32 RoomWmoDataID = 0;
    float OffsetPos[3] = {};
    float OffsetRot[3] = {};
    int32 ModelFileDataID = 0;  // The WMO/mesh file to spawn
    uint8 Type = 0;             // HousingRoomComponentType
    int32 MeshStyleFilterID = 0;
    uint8 ConnectionType = 0;   // RoomConnectionType (for doorways)
    int32 Flags = 0;
};

struct HouseThemeData
{
    uint32 ID = 0;
    std::string Name;
    int32 Flags = 0;                // DB2: Flags
    int32 ParentThemeID = 0;        // DB2: ParentThemeID FK->HouseTheme
};

struct HouseDecorThemeSetData
{
    uint32 ID = 0;
    std::string Name;
    int32 HouseThemeID = 0;
    int32 HouseDecorCategoryID = 0;
};

struct NeighborhoodMapData
{
    uint32 ID = 0;
    float Origin[3] = {};           // DB2: Position[3]
    int32 MapID = 0;                // DB2: MapID
    float EntryRotation = 0.0f;     // DB2: EntryRotation
    uint32 UiTextureKitID = 0;      // DB2: UiTextureKitID
    int32 Flags = 0;                // DB2: Flags (bitmask: 0x1=Alliance, 0x2=Horde, 0x4=SystemGenerate)
};

struct NeighborhoodPlotData
{
    uint32 ID = 0;
    uint64 Cost = 0;
    std::string Name;
    float HousePosition[3] = {};         // Was named HousePosition before 12.0.0.64975
    float HouseRotation[3] = {};         // Was named HouseRotation before 12.0.0.64975
    float CornerstonePosition[3] = {};
    float CornerstoneRotation[3] = {};
    float TeleportPosition[3] = {};
    int32 NeighborhoodMapID = 0;
    int32 Field_010 = 0;
    int32 CornerstoneGameObjectID = 0;
    int32 PlotIndex = 0;
    int32 WorldState = 0;
    int32 PlotGameObjectID = 0;
    float TeleportFacing = 0.0f;         // Facing angle at TeleportPosition
    int32 Field_016 = 0;
};

struct NeighborhoodNameGenData
{
    uint32 ID = 0;
    std::string Prefix;
    std::string Middle;
    std::string Suffix;
    int32 NeighborhoodMapID = 0;
};

struct HouseDecorMaterialData
{
    uint32 ID = 0;
    uint64 WMOMaterialReference = 0;    // DB2: WMOMaterialReference
    int32 MaterialTextureIndex = 0;     // DB2: MaterialTextureIndex
    int32 HouseThemeID = 0;             // DB2: HouseThemeID FK->HouseTheme
    int32 TextureAFileDataID = 0;       // DB2: TextureAFileDataID FK->FileData
    int32 TextureBFileDataID = 0;       // DB2: TextureBFileDataID FK->FileData
};

struct HouseExteriorWmoData
{
    uint32 ID = 0;
    std::string Name;
    int32 Flags = 0;
};

struct HouseLevelRewardInfoData
{
    uint32 ID = 0;
    std::string Name;
    std::string Description;
    int32 HouseLevelDataID = 0;         // DB2: HouseLevelDataID FK->HouseLevelData
    int32 Field_4 = 0;                  // DB2: Field_12_0_0_63967_004
    int32 IconFileDataID = 0;           // DB2: IconFileDataID FK->FileData
};

struct NeighborhoodInitiativeData
{
    uint32 ID = 0;
    std::string Name;
    std::string Description;
    int32 InitiativeType = 0;
    int32 Duration = 0;
    int32 RequiredParticipants = 0;
    int32 RewardCurrencyID = 0;
};

struct DecorCategoryData
{
    uint32 ID = 0;
    std::string Name;
    int32 UiTextureAtlasElementID = 0;  // DB2: UiTextureAtlasElementID
    int32 OrderIndex = 0;               // DB2: OrderIndex
};

struct DecorSubcategoryData
{
    uint32 ID = 0;
    std::string Name;
    int32 UiTextureAtlasElementID = 0;  // DB2: UiTextureAtlasElementID
    int32 DecorCategoryID = 0;
    int32 OrderIndex = 0;               // DB2: OrderIndex
};

struct DecorDyeSlotData
{
    uint32 ID = 0;
    int32 DyeColorCategoryID = 0;       // DB2: DyeColorCategoryID FK->DyeColorCategory
    int32 HouseDecorID = 0;
    int32 OrderIndex = 0;               // DB2: OrderIndex
    int32 Channel = 0;                  // DB2: Channel
};

class TC_GAME_API HousingMgr
{
public:
    HousingMgr();
    HousingMgr(HousingMgr const&) = delete;
    HousingMgr(HousingMgr&&) = delete;
    HousingMgr& operator=(HousingMgr const&) = delete;
    HousingMgr& operator=(HousingMgr&&) = delete;
    ~HousingMgr();

    static HousingMgr& Instance();

    void Initialize();

    // DB2 data accessors
    HouseDecorData const* GetHouseDecorData(uint32 id) const;
    HouseLevelData const* GetLevelData(uint32 level) const;
    HouseRoomData const* GetHouseRoomData(uint32 id) const;
    HouseThemeData const* GetHouseThemeData(uint32 id) const;
    HouseDecorThemeSetData const* GetHouseDecorThemeSetData(uint32 id) const;
    NeighborhoodMapData const* GetNeighborhoodMapData(uint32 id) const;
    std::unordered_map<uint32, NeighborhoodMapData> const& GetAllNeighborhoodMapData() const { return _neighborhoodMapStore; }
    HouseDecorMaterialData const* GetHouseDecorMaterialData(uint32 id) const;
    HouseExteriorWmoData const* GetHouseExteriorWmoData(uint32 id) const;
    HouseLevelRewardInfoData const* GetHouseLevelRewardInfoData(uint32 id) const;
    NeighborhoodInitiativeData const* GetNeighborhoodInitiativeData(uint32 id) const;
    DecorCategoryData const* GetDecorCategoryData(uint32 id) const;
    DecorSubcategoryData const* GetDecorSubcategoryData(uint32 id) const;

    // Indexed lookups
    std::vector<DecorSubcategoryData const*> GetSubcategoriesForCategory(uint32 categoryId) const;
    std::vector<uint32> GetDecorIdsForSubcategory(uint32 subcategoryId) const;
    std::vector<DecorDyeSlotData const*> GetDyeSlotsForDecor(uint32 houseDecorId) const;
    std::vector<HouseDecorMaterialData const*> GetMaterialsForTheme(uint32 houseThemeId) const;
    std::vector<HouseLevelRewardInfoData const*> GetRewardsForLevel(uint32 houseLevelId) const;

    // Neighborhood plot lookups
    uint32 GetPlotStoreSize() const { return uint32(_neighborhoodPlotStore.size()); }
    std::vector<NeighborhoodPlotData const*> GetPlotsForMap(uint32 neighborhoodMapId) const;
    // Find a plot by its cornerstone GO entry within a specific neighborhood map
    NeighborhoodPlotData const* GetPlotByCornerstoneEntry(uint32 neighborhoodMapId, uint32 cornerstoneGoEntry) const;

    // Resolve the canonical DB2 PlotIndex from a client-supplied GUID.
    // The client sends the cornerstone GO GUID as "NeighborhoodGuid" in many CMSGs.
    // We extract the GO entry from that GUID and look up the DB2 plot data.
    // Returns the DB2 PlotIndex, or -1 if resolution failed (caller should use clientPlotIndex as fallback).
    int32 ResolvePlotIndex(ObjectGuid cornerstoneGuid, Neighborhood const* neighborhood) const;

    // Get the NeighborhoodMapData for a world MapID (returns nullptr if not a neighborhood)
    NeighborhoodMapData const* GetNeighborhoodMapDataForWorldMap(uint32 mapId) const;

    // Check if a world MapID corresponds to a neighborhood map
    bool IsNeighborhoodWorldMap(uint32 mapId) const;
    // Get the NeighborhoodMapID for a world MapID (returns 0 if not a neighborhood)
    uint32 GetNeighborhoodMapIdByWorldMap(uint32 mapId) const;
    // Get the world MapID for a NeighborhoodMapID (reverse lookup, returns 0 if not found)
    uint32 GetWorldMapIdByNeighborhoodMapId(uint32 neighborhoodMapId) const;

    // Name generation
    std::string GenerateNeighborhoodName(uint32 neighborhoodMapId) const;

    // Level-based limits
    uint32 GetMaxDecorForLevel(uint32 level) const;

    // Budget accessors (WeightCost-based)
    uint32 GetQuestForLevel(uint32 level) const;
    uint32 GetInteriorDecorBudgetForLevel(uint32 level) const;
    uint32 GetExteriorDecorBudgetForLevel(uint32 level) const;
    uint32 GetRoomBudgetForLevel(uint32 level) const;
    uint32 GetFixtureBudgetForLevel(uint32 level) const;
    uint32 GetDecorWeightCost(uint32 decorEntryId) const;
    uint32 GetRoomWeightCost(uint32 roomEntryId) const;

    // Room connectivity
    bool IsBaseRoom(uint32 roomEntryId) const;
    uint32 GetRoomDoorCount(uint32 roomEntryId) const;
    std::vector<RoomDoorInfo> const* GetRoomDoors(uint32 roomWmoDataId) const;

    // Room component data (all types: wall, floor, ceiling, stairs, doorway, etc.)
    std::vector<RoomComponentData> const* GetRoomComponents(uint32 roomWmoDataId) const;

    // Faction-to-theme mapping (sniff-verified: Alliance=6, Horde=2)
    int32 GetFactionDefaultThemeID(int32 factionRestriction) const;

    // Find a RoomComponentOption matching a specific component + theme
    // Returns nullptr if no match found
    RoomComponentOptionEntry const* FindRoomComponentOption(uint32 roomComponentID, int32 houseThemeID) const;

    // Get the base room entry ID (exterior geobox room, from DB2 IsBaseRoom flag, fallback 18)
    uint32 GetBaseRoomEntryId() const { return _baseRoomEntryId; }

    // Get the entry hall room entry ID (interior base room, sniff-verified: Room 46)
    // Room 18 = exterior plot geobox (SpawnRoomForPlot), Room 46 = interior entry hall
    uint32 GetEntryHallRoomEntryId() const { return _entryHallRoomEntryId; }

    // Room grid spacing for interior layout (sniff-verified: 15.0f)
    float GetRoomGridSpacing() const { return _roomGridSpacing; }

    // RoomComponentTexture lookup: given a RoomComponentOption ID, find the texture ID
    // Returns the first matched RoomComponentTexture ID, or 0 if no link exists in DB2.
    int32 GetTextureIdForComponentOption(int32 roomComponentOptionID) const;
    // RoomComponentTexture by component type: fallback when per-option data is missing
    // Returns the first matching texture for a given component type (1=wall, 2=floor, 3=ceiling)
    int32 GetTextureIdForComponentType(uint8 componentType) const;

    // ExteriorComponent indexed lookups
    std::vector<ExteriorComponentHookEntry const*> const* GetHooksOnComponent(uint32 extCompID) const;
    ExteriorComponentExitPointEntry const* GetExitPoint(uint32 extCompID) const;
    int32 GetGroupForComponent(uint32 extCompID) const;
    std::vector<uint32> const* GetChildComponents(uint32 parentCompID) const;
    std::vector<uint32> const* GetRootComponentsForWmoData(uint32 wmoDataID) const;
    std::vector<uint32> const* GetComponentsInGroup(int32 groupID) const;

    // Fixture resolution: given a hook's component type, the house's WmoDataID, and
    // optionally the house size, returns the default fixture component ID
    // (Flags & 0x1, ParentComponentID == 0).
    // If houseSize is 0, returns any size match; otherwise filters to exact size.
    uint32 GetDefaultFixtureForType(uint8 componentType, uint32 wmoDataID, uint8 houseSize = 0) const;

    // Racial house style: maps player race to the appropriate HouseExteriorWmoDataID.
    // Night Elf → 55, Blood Elf → 56, other Alliance → 9 (Human), other Horde → 87 (Orc).
    static uint32 GetRacialWmoDataID(uint8 race, uint32 teamId);

    // Find the first HouseRoom entry with visual components (not the base room 18)
    uint32 GetDefaultVisualRoomEntry() const;

    // Starter decor (items granted on first house purchase)
    // Returns starter decor IDs filtered by faction (teamId: ALLIANCE=469, HORDE=67)
    // Sniff-verified: Alliance and Horde receive different starter decor sets
    std::vector<uint32> GetStarterDecorIds(uint32 teamId) const;
    // Returns {DecorID, StartingQuantity} pairs for populating the catalog on purchase
    std::vector<std::pair<uint32, int32>> GetStarterDecorWithQuantities(uint32 teamId) const;

    // Access control — checks if visitor can access a plot/house based on owner's settings
    // accessMask = HOUSE_SETTING_HOUSE_ACCESS_* for interior, HOUSE_SETTING_PLOT_ACCESS_* for exterior
    bool CanVisitorAccess(Player const* visitor, Player const* owner, uint32 settingsFlags, bool isInterior) const;

    // Validation
    HousingResult ValidateDecorPlacement(uint32 decorId, Position const& pos, uint32 houseLevel) const;

private:
    void LoadHouseDecorData();
    void LoadHouseLevelData();
    void LoadHouseRoomData();
    void LoadHouseThemeData();
    void LoadHouseDecorThemeSetData();
    void LoadNeighborhoodMapData();
    void LoadNeighborhoodPlotData();
    void LoadNeighborhoodNameGenData();
    void LoadHouseDecorMaterialData();
    void LoadHouseExteriorWmoData();
    void LoadHouseLevelRewardInfoData();
    void LoadNeighborhoodInitiativeData();
    void LoadRoomComponentData();
    void LoadDecorCategoryData();
    void LoadDecorSubcategoryData();
    void LoadDecorDyeSlotData();
    void LoadDecorXDecorSubcategoryData();

    // DB2 data stores indexed by ID
    std::unordered_map<uint32, HouseDecorData> _houseDecorStore;
    std::unordered_map<uint32, HouseLevelData> _houseLevelDataStore;
    std::unordered_map<uint32, HouseRoomData> _houseRoomStore;
    std::unordered_map<uint32, HouseThemeData> _houseThemeStore;
    std::unordered_map<uint32, HouseDecorThemeSetData> _houseDecorThemeSetStore;
    std::unordered_map<uint32, NeighborhoodMapData> _neighborhoodMapStore;
    std::unordered_map<uint32, NeighborhoodPlotData> _neighborhoodPlotStore;
    std::unordered_map<uint32, HouseDecorMaterialData> _houseDecorMaterialStore;
    std::unordered_map<uint32, HouseExteriorWmoData> _houseExteriorWmoStore;
    std::unordered_map<uint32, HouseLevelRewardInfoData> _houseLevelRewardInfoStore;
    std::unordered_map<uint32, NeighborhoodInitiativeData> _neighborhoodInitiativeStore;
    std::unordered_map<uint32, DecorCategoryData> _decorCategoryStore;
    std::unordered_map<uint32, DecorSubcategoryData> _decorSubcategoryStore;
    std::unordered_map<uint32, DecorDyeSlotData> _decorDyeSlotStore;

    // Lookup indexes
    std::unordered_map<uint32 /*neighborhoodMapId*/, std::vector<NeighborhoodPlotData const*>> _plotsByMap;
    std::unordered_map<uint32 /*neighborhoodMapId*/, std::vector<NeighborhoodNameGenData>> _nameGenByMap;
    std::unordered_map<uint32 /*level*/, HouseLevelData const*> _levelDataByLevel;
    std::unordered_map<uint32 /*houseThemeId*/, std::vector<HouseDecorMaterialData const*>> _materialsByTheme;
    std::unordered_map<uint32 /*houseLevelId*/, std::vector<HouseLevelRewardInfoData const*>> _rewardsByLevel;
    std::unordered_map<uint32 /*categoryId*/, std::vector<DecorSubcategoryData const*>> _subcategoriesByCategory;
    std::unordered_map<uint32 /*subcategoryId*/, std::vector<uint32 /*houseDecorId*/>> _decorsBySubcategory;
    std::unordered_map<uint32 /*houseDecorId*/, std::vector<DecorDyeSlotData const*>> _dyeSlotsByDecor;

    // Reverse lookup: world MapID -> NeighborhoodMap ID
    std::unordered_map<int32 /*worldMapId*/, uint32 /*neighborhoodMapId*/> _worldMapToNeighborhoodMap;

    // Room doorway map: RoomWmoDataID -> list of doorway components
    std::unordered_map<uint32 /*roomWmoDataId*/, std::vector<RoomDoorInfo>> _roomDoorMap;

    // All room components indexed by RoomWmoDataID (walls, floors, ceilings, stairs, doorways)
    std::unordered_map<uint32 /*roomWmoDataId*/, std::vector<RoomComponentData>> _roomComponentsByWmoData;

    // O(1) RoomComponentOption lookup: key = (uint64(RoomComponentID) << 32) | uint32(HouseThemeID)
    std::unordered_map<uint64, RoomComponentOptionEntry const*> _roomCompOptionIndex;
    void BuildRoomComponentOptionIndex();
    void BuildExteriorComponentIndexes();
    void BuildRoomComponentTextureIndex();
    void DumpExteriorComponentDiagnostics();
    void DumpRoomComponentTextureDiagnostics();

    // Base room entry ID — exterior geobox (from DB2 IsBaseRoom flag scan, fallback 18)
    uint32 _baseRoomEntryId = 0;

    // Entry hall room entry ID — interior base room (second BASE_ROOM in DB2, fallback to _baseRoomEntryId)
    // Sniff-verified: Room 46 is the entry corridor with door connecting to the visual room
    uint32 _entryHallRoomEntryId = 0;

    // Room grid spacing (sniff-verified: 15.0f)
    float _roomGridSpacing = 15.0f;

    // RoomComponentTexture indexes
    // RoomComponentOptionID → RoomComponentTextureID (from RoomComponentOptionTexture join)
    std::unordered_map<int32 /*optionID*/, int32 /*textureID*/> _textureByOptionId;
    // componentType → textureID (first matching texture per type, fallback)
    std::unordered_map<uint8 /*type*/, int32 /*textureID*/> _textureByComponentType;

    // ExteriorComponent indexes
    std::unordered_map<uint32 /*extCompID*/, std::vector<ExteriorComponentHookEntry const*>> _hooksByExtComp;
    std::unordered_map<uint32 /*extCompID*/, ExteriorComponentExitPointEntry const*> _exitPointByExtComp;
    std::unordered_map<uint32 /*extCompID*/, int32 /*groupID*/> _groupByExtComp;
    std::unordered_map<int32 /*groupID*/, std::vector<uint32 /*extCompID*/>> _extCompsByGroup;
    std::unordered_map<uint32 /*parentCompID*/, std::vector<uint32 /*childCompID*/>> _childrenByExtComp;
    std::unordered_map<uint32 /*wmoDataID*/, std::vector<uint32 /*compID*/>> _rootCompsByWmoDataId;

    // Fixture resolution: (componentType, wmoDataID, size) → default component ID
    // Key = (uint64(componentType) << 40) | (uint64(wmoDataID) << 8) | size
    std::unordered_map<uint64, uint32> _defaultFixtureByTypeWmo;
};

#define sHousingMgr HousingMgr::Instance()

#endif // HousingMgr_h__
