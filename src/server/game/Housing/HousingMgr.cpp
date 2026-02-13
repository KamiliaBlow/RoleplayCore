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

#include "HousingMgr.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Log.h"
#include "Random.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>

HousingMgr::HousingMgr() = default;
HousingMgr::~HousingMgr() = default;

HousingMgr& HousingMgr::Instance()
{
    static HousingMgr instance;
    return instance;
}

void HousingMgr::Initialize()
{
    uint32 oldMSTime = getMSTime();

    LoadHouseDecorData();
    LoadHouseLevelData();
    LoadHouseRoomData();
    LoadHouseThemeData();
    LoadHouseDecorThemeSetData();
    LoadNeighborhoodMapData();
    LoadNeighborhoodPlotData();
    LoadNeighborhoodNameGenData();
    LoadHouseDecorMaterialData();
    LoadHouseExteriorWmoData();
    LoadHouseLevelRewardInfoData();
    LoadNeighborhoodInitiativeData();
    LoadNeighborhoodInitiativeRewardData();
    LoadNeighborhoodInitiativeTaskData();
    LoadNeighborhoodInitiativeXTaskData();
    LoadRoomComponentData();

    TC_LOG_INFO("server.loading", ">> Loaded housing data: {} decor, {} levels, "
        "{} rooms, {} themes, {} decor materials, {} exterior wmos, {} level rewards, "
        "{} initiatives, {} initiative tasks, {} neighborhood maps, {} neighborhood plots in {}",
        uint32(_houseDecorStore.size()), uint32(_houseLevelDataStore.size()),
        uint32(_houseRoomStore.size()), uint32(_houseThemeStore.size()),
        uint32(_houseDecorMaterialStore.size()), uint32(_houseExteriorWmoStore.size()),
        uint32(_houseLevelRewardInfoStore.size()), uint32(_neighborhoodInitiativeStore.size()),
        uint32(_neighborhoodInitiativeTaskStore.size()),
        uint32(_neighborhoodMapStore.size()), uint32(_neighborhoodPlotStore.size()),
        GetMSTimeDiffToNow(oldMSTime));
}

void HousingMgr::LoadHouseDecorData()
{
    for (HouseDecorEntry const* entry : sHouseDecorStore)
    {
        HouseDecorData& data = _houseDecorStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.BoundingBox[0] = entry->Scale.X;
        data.BoundingBox[1] = entry->Scale.Y;
        data.BoundingBox[2] = entry->Scale.Z;
        data.PlacementFlags = entry->PlacementFlags;
        data.CategoryID = entry->CategoryID;
        data.Quality = entry->QualityTier;
        data.MaxDyeSlots = entry->SubCategoryID;
        data.SpellVisualID = entry->SpellVisualKitID;
        data.GameObjectDisplayInfoID = entry->FileDataID;
        data.CurrencyCost = entry->CurrencyCost;
        data.SourceTypeEnum = entry->CollisionType;
        data.Scale = entry->InteractionRadius;
        data.SourceID = entry->SoundKitID;
        data.SourceDifficultyID = entry->AnimKitID;
        data.UiModelSceneID = entry->RequiredHouseLevel;
        data.HouseDecorCategoryID = entry->PlayerConditionID;
        data.WeightCost = entry->WeightCost > 0 ? entry->WeightCost : 1;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseDecorData: Loaded {} HouseDecor entries", uint32(_houseDecorStore.size()));
}

void HousingMgr::LoadHouseLevelData()
{
    for (HouseLevelDataEntry const* entry : sHouseLevelDataStore)
    {
        HouseLevelData& data = _houseLevelDataStore[entry->ID];
        data.ID = entry->ID;
        data.MaxDecorCount = entry->MaxDecorCount;
        data.FavorRequired = entry->RequiredFavor;
        data.QuestID = entry->QuestID;
        data.InteriorDecorPlacementBudget = entry->InteriorDecorPlacementBudget;
        data.ExteriorDecorPlacementBudget = entry->ExteriorDecorPlacementBudget;
        data.RoomPlacementBudget = entry->RoomPlacementBudget;
        data.ExteriorFixtureBudget = entry->ExteriorFixtureBudget;
    }

    // Fallback defaults if no DB2 data available
    if (_houseLevelDataStore.empty())
    {
        for (uint32 level = 1; level <= 10; ++level)
        {
            HouseLevelData& data = _houseLevelDataStore[level];
            data.ID = level;
            data.MaxDecorCount = static_cast<int32>(level * 25);
            data.FavorRequired = static_cast<int32>((level - 1) * 100);
            data.QuestID = 0;
            data.InteriorDecorPlacementBudget = static_cast<int32>(50 + level * 25);
            data.ExteriorDecorPlacementBudget = static_cast<int32>(25 + level * 15);
            data.RoomPlacementBudget = static_cast<int32>(20 + level * 10);
            data.ExteriorFixtureBudget = static_cast<int32>(10 + level * 5);
        }
    }

    // Build level lookup index
    for (auto& [id, entry] : _houseLevelDataStore)
        _levelDataByLevel[id] = &entry;

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseLevelData: Loaded {} HouseLevelData entries", uint32(_houseLevelDataStore.size()));
}

void HousingMgr::LoadHouseRoomData()
{
    for (HouseRoomEntry const* entry : sHouseRoomStore)
    {
        HouseRoomData& data = _houseRoomStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.Size = entry->Size;
        data.WmoID = entry->WmoFileDataID;
        data.DoorCount = entry->DoorCount;
        data.Flags = entry->BaseRoomFlags;
        data.RequiredHouseLevel = entry->RequiredHouseLevel;
        data.CurrencyCost = entry->CurrencyCost;
        data.WeightCost = entry->WeightCost > 0 ? entry->WeightCost : 1;
        data.RoomWmoDataID = entry->RoomWmoDataID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseRoomData: Loaded {} HouseRoom entries", uint32(_houseRoomStore.size()));
}

void HousingMgr::LoadHouseThemeData()
{
    for (HouseThemeEntry const* entry : sHouseThemeStore)
    {
        HouseThemeData& data = _houseThemeStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.ThemeSetID = entry->IconFileDataID;
        data.UiModelSceneID = entry->CategoryID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseThemeData: Loaded {} HouseTheme entries", uint32(_houseThemeStore.size()));
}

void HousingMgr::LoadHouseDecorThemeSetData()
{
    for (HouseDecorThemeSetEntry const* entry : sHouseDecorThemeSetStore)
    {
        HouseDecorThemeSetData& data = _houseDecorThemeSetStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.HouseThemeID = entry->ThemeID;
        data.HouseDecorCategoryID = entry->IconFileDataID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseDecorThemeSetData: Loaded {} HouseDecorThemeSet entries", uint32(_houseDecorThemeSetStore.size()));
}

void HousingMgr::LoadNeighborhoodMapData()
{
    for (NeighborhoodMapEntry const* entry : sNeighborhoodMapStore)
    {
        NeighborhoodMapData& data = _neighborhoodMapStore[entry->ID];
        data.ID = entry->ID;
        data.Origin[0] = entry->Position.X;
        data.Origin[1] = entry->Position.Y;
        data.Origin[2] = entry->Position.Z;
        data.MapID = entry->MapID;
        data.PlotSpacing = entry->Radius;
        data.MaxPlots = entry->PlotCount;
        data.UiMapID = entry->FactionRestriction;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodMapData: Loaded {} NeighborhoodMap entries", uint32(_neighborhoodMapStore.size()));
}

void HousingMgr::LoadNeighborhoodPlotData()
{
    for (NeighborhoodPlotEntry const* entry : sNeighborhoodPlotStore)
    {
        NeighborhoodPlotData& data = _neighborhoodPlotStore[entry->ID];
        data.ID = entry->ID;
        data.ContentTuningID = entry->PlotGUID;
        data.InternalName = entry->InternalName ? entry->InternalName : "";
        data.Position[0] = entry->Position.X;
        data.Position[1] = entry->Position.Y;
        data.Position[2] = entry->Position.Z;
        data.Rotation[0] = entry->Orientation.X;
        data.Rotation[1] = entry->Orientation.Y;
        data.Rotation[2] = entry->Orientation.Z;
        data.Scale[0] = entry->BoundsMin.X;
        data.Scale[1] = entry->BoundsMin.Y;
        data.Scale[2] = entry->BoundsMin.Z;
        data.BoundingBoxMin[0] = entry->BoundsMin.X;
        data.BoundingBoxMin[1] = entry->BoundsMin.Y;
        data.BoundingBoxMin[2] = entry->BoundsMin.Z;
        data.BoundingBoxMax[0] = entry->BoundsMax.X;
        data.BoundingBoxMax[1] = entry->BoundsMax.Y;
        data.BoundingBoxMax[2] = entry->BoundsMax.Z;
        data.NeighborhoodMapID = entry->NeighborhoodMapID;
        data.PlotSize = entry->PlotSize;
        data.PlotIndex = entry->PlotFlags;
        data.AreaTriggerID = entry->AreaTriggerID;
        data.Flags = entry->PhaseID;
    }

    // Build map index
    for (auto const& [id, plot] : _neighborhoodPlotStore)
        _plotsByMap[plot.NeighborhoodMapID].push_back(&plot);

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodPlotData: Loaded {} NeighborhoodPlot entries across {} maps",
        uint32(_neighborhoodPlotStore.size()), uint32(_plotsByMap.size()));
}

void HousingMgr::LoadNeighborhoodNameGenData()
{
    for (NeighborhoodNameGenEntry const* entry : sNeighborhoodNameGenStore)
    {
        NeighborhoodNameGenData data;
        data.ID = entry->ID;
        data.Prefix = entry->Prefix[sWorld->GetDefaultDbcLocale()];
        data.Middle = entry->Suffix[sWorld->GetDefaultDbcLocale()];
        data.Suffix = entry->FullName[sWorld->GetDefaultDbcLocale()];
        data.NeighborhoodMapID = entry->NeighborhoodMapID;
        _nameGenByMap[entry->NeighborhoodMapID].push_back(std::move(data));
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodNameGenData: Loaded name gen data for {} maps",
        uint32(_nameGenByMap.size()));
}

HouseDecorData const* HousingMgr::GetHouseDecorData(uint32 id) const
{
    auto itr = _houseDecorStore.find(id);
    if (itr != _houseDecorStore.end())
        return &itr->second;

    return nullptr;
}

HouseLevelData const* HousingMgr::GetLevelData(uint32 level) const
{
    auto itr = _levelDataByLevel.find(level);
    if (itr != _levelDataByLevel.end())
        return itr->second;

    return nullptr;
}

HouseRoomData const* HousingMgr::GetHouseRoomData(uint32 id) const
{
    auto itr = _houseRoomStore.find(id);
    if (itr != _houseRoomStore.end())
        return &itr->second;

    return nullptr;
}

HouseThemeData const* HousingMgr::GetHouseThemeData(uint32 id) const
{
    auto itr = _houseThemeStore.find(id);
    if (itr != _houseThemeStore.end())
        return &itr->second;

    return nullptr;
}

HouseDecorThemeSetData const* HousingMgr::GetHouseDecorThemeSetData(uint32 id) const
{
    auto itr = _houseDecorThemeSetStore.find(id);
    if (itr != _houseDecorThemeSetStore.end())
        return &itr->second;

    return nullptr;
}

NeighborhoodMapData const* HousingMgr::GetNeighborhoodMapData(uint32 id) const
{
    auto itr = _neighborhoodMapStore.find(id);
    if (itr != _neighborhoodMapStore.end())
        return &itr->second;

    return nullptr;
}

std::vector<NeighborhoodPlotData const*> HousingMgr::GetPlotsForMap(uint32 neighborhoodMapId) const
{
    auto itr = _plotsByMap.find(neighborhoodMapId);
    if (itr != _plotsByMap.end())
        return itr->second;

    return {};
}

std::string HousingMgr::GenerateNeighborhoodName(uint32 neighborhoodMapId) const
{
    auto itr = _nameGenByMap.find(neighborhoodMapId);
    if (itr == _nameGenByMap.end() || itr->second.empty())
        return "Unnamed Neighborhood";

    std::vector<NeighborhoodNameGenData> const& nameGens = itr->second;

    // Pick a random name gen entry from the set for this map
    uint32 index = urand(0, static_cast<uint32>(nameGens.size()) - 1);
    NeighborhoodNameGenData const& entry = nameGens[index];

    // Combine prefix + middle + suffix
    std::string name;
    if (!entry.Prefix.empty())
        name += entry.Prefix;
    if (!entry.Middle.empty())
        name += entry.Middle;
    if (!entry.Suffix.empty())
        name += entry.Suffix;

    if (name.empty())
        return "Unnamed Neighborhood";

    return name;
}

uint32 HousingMgr::GetMaxDecorForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData)
        return static_cast<uint32>(levelData->MaxDecorCount);

    return 0;
}

uint32 HousingMgr::GetQuestForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->QuestID > 0)
        return static_cast<uint32>(levelData->QuestID);

    return 0;
}

uint32 HousingMgr::GetInteriorDecorBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->InteriorDecorPlacementBudget > 0)
        return static_cast<uint32>(levelData->InteriorDecorPlacementBudget);

    // Fallback: 50 base + 25 per level
    return 50 + level * 25;
}

uint32 HousingMgr::GetExteriorDecorBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->ExteriorDecorPlacementBudget > 0)
        return static_cast<uint32>(levelData->ExteriorDecorPlacementBudget);

    // Fallback: 25 base + 15 per level
    return 25 + level * 15;
}

uint32 HousingMgr::GetRoomBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->RoomPlacementBudget > 0)
        return static_cast<uint32>(levelData->RoomPlacementBudget);

    // Fallback: 20 base + 10 per level
    return 20 + level * 10;
}

uint32 HousingMgr::GetFixtureBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->ExteriorFixtureBudget > 0)
        return static_cast<uint32>(levelData->ExteriorFixtureBudget);

    // Fallback: 10 base + 5 per level
    return 10 + level * 5;
}

uint32 HousingMgr::GetDecorWeightCost(uint32 decorEntryId) const
{
    HouseDecorData const* decorData = GetHouseDecorData(decorEntryId);
    if (decorData)
        return static_cast<uint32>(std::max<int32>(decorData->WeightCost, 1));

    return 1;
}

uint32 HousingMgr::GetRoomWeightCost(uint32 roomEntryId) const
{
    HouseRoomData const* roomData = GetHouseRoomData(roomEntryId);
    if (roomData)
        return static_cast<uint32>(std::max<int32>(roomData->WeightCost, 1));

    return 1;
}

HousingResult HousingMgr::ValidateDecorPlacement(uint32 decorId, Position const& pos, uint32 houseLevel) const
{
    HouseDecorData const* decorEntry = GetHouseDecorData(decorId);
    if (!decorEntry)
        return HOUSING_RESULT_DECOR_INVALID_GUID;

    // Validate position is within reasonable bounds
    if (!pos.IsPositionValid())
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    // Validate house level meets decor requirements (if any level restriction exists)
    // For now, all decor is available at any level; future DB2 fields may add restrictions
    (void)houseLevel;

    return HOUSING_RESULT_SUCCESS;
}

// --- 7 new DB2 Load functions ---

void HousingMgr::LoadHouseDecorMaterialData()
{
    for (HouseDecorMaterialEntry const* entry : sHouseDecorMaterialStore)
    {
        HouseDecorMaterialData& data = _houseDecorMaterialStore[entry->ID];
        data.ID = entry->ID;
        data.MaterialGUID = entry->MaterialGUID;
        data.HouseDecorID = entry->HouseDecorID;
        data.MaterialIndex = entry->MaterialIndex;
        data.DefaultDyeID = entry->DefaultDyeID;
        data.AllowedDyeMask = entry->AllowedDyeMask;
    }

    // Build decor material index
    for (auto const& [id, mat] : _houseDecorMaterialStore)
        _materialsByDecor[mat.HouseDecorID].push_back(&mat);

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseDecorMaterialData: Loaded {} HouseDecorMaterial entries", uint32(_houseDecorMaterialStore.size()));
}

void HousingMgr::LoadHouseExteriorWmoData()
{
    for (HouseExteriorWmoDataEntry const* entry : sHouseExteriorWmoDataStore)
    {
        HouseExteriorWmoData& data = _houseExteriorWmoStore[entry->ID];
        data.ID = entry->ID;
        data.WmoFilePath = entry->WmoFilePath ? entry->WmoFilePath : "";
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseExteriorWmoData: Loaded {} HouseExteriorWmoData entries", uint32(_houseExteriorWmoStore.size()));
}

void HousingMgr::LoadHouseLevelRewardInfoData()
{
    for (HouseLevelRewardInfoEntry const* entry : sHouseLevelRewardInfoStore)
    {
        HouseLevelRewardInfoData& data = _houseLevelRewardInfoStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.Description = entry->Description[sWorld->GetDefaultDbcLocale()];
        data.HouseLevelID = entry->HouseLevelID;
        data.RewardType = entry->RewardType;
        data.RewardValue = entry->RewardValue;
    }

    // Build level reward index
    for (auto const& [id, reward] : _houseLevelRewardInfoStore)
        _rewardsByLevel[reward.HouseLevelID].push_back(&reward);

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseLevelRewardInfoData: Loaded {} HouseLevelRewardInfo entries", uint32(_houseLevelRewardInfoStore.size()));
}

void HousingMgr::LoadNeighborhoodInitiativeData()
{
    for (NeighborhoodInitiativeEntry const* entry : sNeighborhoodInitiativeStore)
    {
        NeighborhoodInitiativeData& data = _neighborhoodInitiativeStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.Description = entry->Description[sWorld->GetDefaultDbcLocale()];
        data.InitiativeType = entry->InitiativeType;
        data.Duration = entry->Duration;
        data.RequiredParticipants = entry->RequiredParticipants;
        data.RewardCurrencyID = entry->RewardCurrencyID;
        data.RewardCurrencyAmount = entry->RewardCurrencyAmount;
        data.PlayerConditionID = entry->PlayerConditionID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodInitiativeData: Loaded {} NeighborhoodInitiative entries", uint32(_neighborhoodInitiativeStore.size()));
}

void HousingMgr::LoadNeighborhoodInitiativeRewardData()
{
    for (NeighborhoodInitiativeRewardEntry const* entry : sNeighborhoodInitiativeRewardStore)
    {
        NeighborhoodInitiativeRewardData& data = _neighborhoodInitiativeRewardStore[entry->ID];
        data.ID = entry->ID;
        data.InitiativeID = entry->InitiativeID;
        data.ChanceWeight = entry->ChanceWeight;
        data.RewardValue = entry->RewardValue;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodInitiativeRewardData: Loaded {} NeighborhoodInitiativeReward entries", uint32(_neighborhoodInitiativeRewardStore.size()));
}

void HousingMgr::LoadNeighborhoodInitiativeTaskData()
{
    for (NeighborhoodInitiativeTaskEntry const* entry : sNeighborhoodInitiativeTaskStore)
    {
        NeighborhoodInitiativeTaskData& data = _neighborhoodInitiativeTaskStore[entry->ID];
        data.ID = entry->ID;
        data.Name = entry->Name[sWorld->GetDefaultDbcLocale()];
        data.Description = entry->Description[sWorld->GetDefaultDbcLocale()];
        data.TaskType = entry->TaskType;
        data.RequiredCount = entry->RequiredCount;
        data.TargetID = entry->TargetID;
        data.ProgressWeight = entry->ProgressWeight;
        data.PlayerConditionID = entry->PlayerConditionID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodInitiativeTaskData: Loaded {} NeighborhoodInitiativeTask entries", uint32(_neighborhoodInitiativeTaskStore.size()));
}

void HousingMgr::LoadNeighborhoodInitiativeXTaskData()
{
    for (NeighborhoodInitiativeXTaskEntry const* entry : sNeighborhoodInitiativeXTaskStore)
    {
        // This is a mapping table: link tasks to initiatives
        if (auto itr = _neighborhoodInitiativeTaskStore.find(entry->TaskID); itr != _neighborhoodInitiativeTaskStore.end())
            _tasksByInitiative[entry->InitiativeID].push_back(&itr->second);
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodInitiativeXTaskData: Built task index for {} initiatives", uint32(_tasksByInitiative.size()));
}

void HousingMgr::LoadRoomComponentData()
{
    uint32 doorwayCount = 0;

    for (RoomComponentEntry const* entry : sRoomComponentStore)
    {
        // Only index doorway components (Type == 7)
        if (entry->Type != HOUSING_ROOM_COMPONENT_DOORWAY)
            continue;

        RoomDoorInfo door;
        door.RoomComponentID = entry->ID;
        door.OffsetPos[0] = entry->OffsetPos.X;
        door.OffsetPos[1] = entry->OffsetPos.Y;
        door.OffsetPos[2] = entry->OffsetPos.Z;
        door.OffsetRot[0] = entry->OffsetRot.X;
        door.OffsetRot[1] = entry->OffsetRot.Y;
        door.OffsetRot[2] = entry->OffsetRot.Z;
        door.ConnectionType = entry->ConnectionType;

        _roomDoorMap[entry->RoomWmoDataID].push_back(door);
        ++doorwayCount;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadRoomComponentData: Indexed {} doorway components across {} room types from {} total RoomComponent entries",
        doorwayCount, uint32(_roomDoorMap.size()), uint32(sRoomComponentStore.GetNumRows()));
}

bool HousingMgr::IsBaseRoom(uint32 roomEntryId) const
{
    HouseRoomData const* roomData = GetHouseRoomData(roomEntryId);
    return roomData && roomData->IsBaseRoom();
}

uint32 HousingMgr::GetRoomDoorCount(uint32 roomEntryId) const
{
    HouseRoomData const* roomData = GetHouseRoomData(roomEntryId);
    if (!roomData)
        return 0;

    auto itr = _roomDoorMap.find(roomData->RoomWmoDataID);
    if (itr != _roomDoorMap.end())
        return static_cast<uint32>(itr->second.size());

    // Fall back to DB2 DoorCount if no RoomComponent data available
    return roomData->DoorCount > 0 ? static_cast<uint32>(roomData->DoorCount) : 0;
}

std::vector<RoomDoorInfo> const* HousingMgr::GetRoomDoors(uint32 roomWmoDataId) const
{
    auto itr = _roomDoorMap.find(roomWmoDataId);
    if (itr != _roomDoorMap.end())
        return &itr->second;

    return nullptr;
}

// --- 6 new ID-based accessors ---

HouseDecorMaterialData const* HousingMgr::GetHouseDecorMaterialData(uint32 id) const
{
    auto itr = _houseDecorMaterialStore.find(id);
    if (itr != _houseDecorMaterialStore.end())
        return &itr->second;

    return nullptr;
}

HouseExteriorWmoData const* HousingMgr::GetHouseExteriorWmoData(uint32 id) const
{
    auto itr = _houseExteriorWmoStore.find(id);
    if (itr != _houseExteriorWmoStore.end())
        return &itr->second;

    return nullptr;
}

HouseLevelRewardInfoData const* HousingMgr::GetHouseLevelRewardInfoData(uint32 id) const
{
    auto itr = _houseLevelRewardInfoStore.find(id);
    if (itr != _houseLevelRewardInfoStore.end())
        return &itr->second;

    return nullptr;
}

NeighborhoodInitiativeData const* HousingMgr::GetNeighborhoodInitiativeData(uint32 id) const
{
    auto itr = _neighborhoodInitiativeStore.find(id);
    if (itr != _neighborhoodInitiativeStore.end())
        return &itr->second;

    return nullptr;
}

NeighborhoodInitiativeRewardData const* HousingMgr::GetNeighborhoodInitiativeRewardData(uint32 id) const
{
    auto itr = _neighborhoodInitiativeRewardStore.find(id);
    if (itr != _neighborhoodInitiativeRewardStore.end())
        return &itr->second;

    return nullptr;
}

NeighborhoodInitiativeTaskData const* HousingMgr::GetNeighborhoodInitiativeTaskData(uint32 id) const
{
    auto itr = _neighborhoodInitiativeTaskStore.find(id);
    if (itr != _neighborhoodInitiativeTaskStore.end())
        return &itr->second;

    return nullptr;
}

// --- 3 indexed lookup accessors ---

std::vector<HouseDecorMaterialData const*> HousingMgr::GetMaterialsForDecor(uint32 houseDecorId) const
{
    auto itr = _materialsByDecor.find(houseDecorId);
    if (itr != _materialsByDecor.end())
        return itr->second;

    return {};
}

std::vector<HouseLevelRewardInfoData const*> HousingMgr::GetRewardsForLevel(uint32 houseLevelId) const
{
    auto itr = _rewardsByLevel.find(houseLevelId);
    if (itr != _rewardsByLevel.end())
        return itr->second;

    return {};
}

std::vector<NeighborhoodInitiativeTaskData const*> HousingMgr::GetTasksForInitiative(uint32 initiativeId) const
{
    auto itr = _tasksByInitiative.find(initiativeId);
    if (itr != _tasksByInitiative.end())
        return itr->second;

    return {};
}
