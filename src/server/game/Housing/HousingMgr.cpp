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

    TC_LOG_INFO("server.loading", ">> Loaded housing data: {} decor entries, {} level entries, "
        "{} room entries, {} theme entries, {} neighborhood maps, {} neighborhood plots in {}",
        uint32(_houseDecorStore.size()), uint32(_houseLevelDataStore.size()),
        uint32(_houseRoomStore.size()), uint32(_houseThemeStore.size()),
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
        data.MaxDecorCount = entry->DoorCount;
        data.DoorSlots = entry->BaseRoomFlags;
        data.CeilingSlots = entry->RequiredHouseLevel;
        data.WallSlots = entry->CurrencyCost;
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

HousingResult HousingMgr::ValidateDecorPlacement(uint32 decorId, Position const& pos, uint32 houseLevel) const
{
    HouseDecorData const* decorEntry = GetHouseDecorData(decorId);
    if (!decorEntry)
        return HOUSING_RESULT_INVALID_DECOR;

    // Validate position is within reasonable bounds
    if (!pos.IsPositionValid())
        return HOUSING_RESULT_INVALID_POSITION;

    // Validate house level meets decor requirements (if any level restriction exists)
    // For now, all decor is available at any level; future DB2 fields may add restrictions
    (void)houseLevel;

    return HOUSING_RESULT_SUCCESS;
}
