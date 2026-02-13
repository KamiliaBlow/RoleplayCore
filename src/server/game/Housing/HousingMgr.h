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
#include "Position.h"
#include <string>
#include <unordered_map>
#include <vector>

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

struct HouseThemeData
{
    uint32 ID = 0;
    std::string Name;
    int32 ThemeSetID = 0;
    int32 UiModelSceneID = 0;
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
    float Origin[3] = {};
    int32 MapID = 0;
    float PlotSpacing = 0.0f;
    uint32 MaxPlots = 0;
    int32 UiMapID = 0;
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
    uint64 MaterialGUID = 0;
    int32 HouseDecorID = 0;
    int32 MaterialIndex = 0;
    int32 DefaultDyeID = 0;
    int32 AllowedDyeMask = 0;
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
    int32 HouseLevelID = 0;
    int32 RewardType = 0;
    int32 RewardValue = 0;
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
    int32 RewardCurrencyAmount = 0;
    int32 PlayerConditionID = 0;
};

struct NeighborhoodInitiativeRewardData
{
    uint32 ID = 0;
    int32 InitiativeID = 0;
    float ChanceWeight = 0.0f;
    int32 RewardValue = 0;
};

struct NeighborhoodInitiativeTaskData
{
    uint32 ID = 0;
    std::string Name;
    std::string Description;
    int32 TaskType = 0;
    int32 RequiredCount = 0;
    int32 TargetID = 0;
    float ProgressWeight = 0.0f;
    int32 PlayerConditionID = 0;
};

struct NeighborhoodInitiativeXTaskData
{
    uint32 ID = 0;
    int32 TaskID = 0;
    int32 InitiativeID = 0;
};

struct DecorCategoryData
{
    uint32 ID = 0;
    std::string Name;
    int32 IconFileDataID = 0;
    int32 DisplayIndex = 0;
};

struct DecorSubcategoryData
{
    uint32 ID = 0;
    std::string Name;
    int32 IconFileDataID = 0;
    int32 DecorCategoryID = 0;
    int32 DisplayIndex = 0;
};

struct DecorDyeSlotData
{
    uint32 ID = 0;
    int32 SlotIndex = 0;
    int32 HouseDecorID = 0;
    int32 DyeChannelType = 0;
    int32 DefaultDyeRecordID = 0;
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
    HouseDecorMaterialData const* GetHouseDecorMaterialData(uint32 id) const;
    HouseExteriorWmoData const* GetHouseExteriorWmoData(uint32 id) const;
    HouseLevelRewardInfoData const* GetHouseLevelRewardInfoData(uint32 id) const;
    NeighborhoodInitiativeData const* GetNeighborhoodInitiativeData(uint32 id) const;
    NeighborhoodInitiativeRewardData const* GetNeighborhoodInitiativeRewardData(uint32 id) const;
    NeighborhoodInitiativeTaskData const* GetNeighborhoodInitiativeTaskData(uint32 id) const;
    DecorCategoryData const* GetDecorCategoryData(uint32 id) const;
    DecorSubcategoryData const* GetDecorSubcategoryData(uint32 id) const;

    // Indexed lookups
    std::vector<DecorSubcategoryData const*> GetSubcategoriesForCategory(uint32 categoryId) const;
    std::vector<uint32> GetDecorIdsForSubcategory(uint32 subcategoryId) const;
    std::vector<DecorDyeSlotData const*> GetDyeSlotsForDecor(uint32 houseDecorId) const;
    std::vector<HouseDecorMaterialData const*> GetMaterialsForDecor(uint32 houseDecorId) const;
    std::vector<HouseLevelRewardInfoData const*> GetRewardsForLevel(uint32 houseLevelId) const;
    std::vector<NeighborhoodInitiativeTaskData const*> GetTasksForInitiative(uint32 initiativeId) const;

    // Neighborhood plot lookups
    std::vector<NeighborhoodPlotData const*> GetPlotsForMap(uint32 neighborhoodMapId) const;

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
    void LoadNeighborhoodInitiativeRewardData();
    void LoadNeighborhoodInitiativeTaskData();
    void LoadNeighborhoodInitiativeXTaskData();
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
    std::unordered_map<uint32, NeighborhoodInitiativeRewardData> _neighborhoodInitiativeRewardStore;
    std::unordered_map<uint32, NeighborhoodInitiativeTaskData> _neighborhoodInitiativeTaskStore;
    std::unordered_map<uint32, DecorCategoryData> _decorCategoryStore;
    std::unordered_map<uint32, DecorSubcategoryData> _decorSubcategoryStore;
    std::unordered_map<uint32, DecorDyeSlotData> _decorDyeSlotStore;

    // Lookup indexes
    std::unordered_map<uint32 /*neighborhoodMapId*/, std::vector<NeighborhoodPlotData const*>> _plotsByMap;
    std::unordered_map<uint32 /*neighborhoodMapId*/, std::vector<NeighborhoodNameGenData>> _nameGenByMap;
    std::unordered_map<uint32 /*level*/, HouseLevelData const*> _levelDataByLevel;
    std::unordered_map<uint32 /*houseDecorId*/, std::vector<HouseDecorMaterialData const*>> _materialsByDecor;
    std::unordered_map<uint32 /*houseLevelId*/, std::vector<HouseLevelRewardInfoData const*>> _rewardsByLevel;
    std::unordered_map<uint32 /*initiativeId*/, std::vector<NeighborhoodInitiativeTaskData const*>> _tasksByInitiative;
    std::unordered_map<uint32 /*categoryId*/, std::vector<DecorSubcategoryData const*>> _subcategoriesByCategory;
    std::unordered_map<uint32 /*subcategoryId*/, std::vector<uint32 /*houseDecorId*/>> _decorsBySubcategory;
    std::unordered_map<uint32 /*houseDecorId*/, std::vector<DecorDyeSlotData const*>> _dyeSlotsByDecor;

    // Room doorway map: RoomWmoDataID -> list of doorway components
    std::unordered_map<uint32 /*roomWmoDataId*/, std::vector<RoomDoorInfo>> _roomDoorMap;
};

#define sHousingMgr HousingMgr::Instance()

#endif // HousingMgr_h__
