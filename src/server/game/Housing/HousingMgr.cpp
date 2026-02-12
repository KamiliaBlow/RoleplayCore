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
#include "Log.h"
#include "Random.h"
#include "Timer.h"

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
    // HouseDecor DB2 has 18 fields as defined in HouseDecorMeta:
    //  0: Name (string)
    //  1: BoundingBox[3] (float)
    //  2: ID (uint32, indexed)
    //  3: PlacementFlags (int32)
    //  4: CategoryID (int32)
    //  5: CurrencyTypeID (int32)
    //  6: Quality (uint8)
    //  7: MaxDyeSlots (uint8)
    //  8: SpellVisualID (int32)
    //  9: GameObjectDisplayInfoID (int32)
    // 10: CurrencyCost (int32)
    // 11: SourceTypeEnum (int32)
    // 12: Scale (float)
    // 13: SourceID (int32)
    // 14: SourceDifficultyID (int32)
    // 15: UiModelSceneID (int8)
    // 16: HouseDecorCategoryID (int32)
    // 17: unknown (int32)
    //
    // Data is loaded from DB2 hotfix storage when available.
    // For now, this is a placeholder that will be populated once DB2 stores
    // for housing are registered in DB2Stores.cpp.

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseDecorData: Loaded {} HouseDecor entries", uint32(_houseDecorStore.size()));
}

void HousingMgr::LoadHouseLevelData()
{
    // HouseLevelData DB2 has 3 fields as defined in HouseLevelDataMeta:
    //  0: ID (uint32, indexed)
    //  1: MaxDecorCount (int32)
    //  2: FavorRequired (int32)
    //
    // Provide fallback defaults until DB2 stores are registered.
    // Default level progression: each level allows 25 more decor items

    if (_houseLevelDataStore.empty())
    {
        for (uint32 level = 1; level <= 10; ++level)
        {
            HouseLevelData& entry = _houseLevelDataStore[level];
            entry.ID = level;
            entry.MaxDecorCount = static_cast<int32>(level * 25);
            entry.FavorRequired = static_cast<int32>((level - 1) * 100);
        }
    }

    // Build level lookup index
    for (auto& [id, entry] : _houseLevelDataStore)
        _levelDataByLevel[id] = &entry;

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseLevelData: Loaded {} HouseLevelData entries", uint32(_houseLevelDataStore.size()));
}

void HousingMgr::LoadHouseRoomData()
{
    // HouseRoom DB2 has 7 fields as defined in HouseRoomMeta:
    //  0: Name (string)
    //  1: Size (int8)
    //  2: WmoID (int32)
    //  3: MaxDecorCount (int32)
    //  4: DoorSlots (int32)
    //  5: CeilingSlots (int32)
    //  6: WallSlots (int32)
    //
    // Will be populated from DB2 store when registered.

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseRoomData: Loaded {} HouseRoom entries", uint32(_houseRoomStore.size()));
}

void HousingMgr::LoadHouseThemeData()
{
    // HouseTheme DB2 has 4 fields as defined in HouseThemeMeta:
    //  0: Name (string)
    //  1: ID (uint32, indexed)
    //  2: ThemeSetID (int32)
    //  3: UiModelSceneID (int32)
    //
    // Will be populated from DB2 store when registered.

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseThemeData: Loaded {} HouseTheme entries", uint32(_houseThemeStore.size()));
}

void HousingMgr::LoadHouseDecorThemeSetData()
{
    // HouseDecorThemeSet DB2 has 3 fields as defined in HouseDecorThemeSetMeta:
    //  0: Name (string)
    //  1: HouseThemeID (int32)
    //  2: HouseDecorCategoryID (int32)
    //
    // Will be populated from DB2 store when registered.

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseDecorThemeSetData: Loaded {} HouseDecorThemeSet entries", uint32(_houseDecorThemeSetStore.size()));
}

void HousingMgr::LoadNeighborhoodMapData()
{
    // NeighborhoodMap DB2 has 6 fields as defined in NeighborhoodMapMeta:
    //  0: Origin[3] (float)
    //  1: ID (uint32, indexed)
    //  2: MapID (int32)
    //  3: PlotSpacing (float)
    //  4: MaxPlots (uint32)
    //  5: UiMapID (int32)
    //
    // Will be populated from DB2 store when registered.

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodMapData: Loaded {} NeighborhoodMap entries", uint32(_neighborhoodMapStore.size()));
}

void HousingMgr::LoadNeighborhoodPlotData()
{
    // NeighborhoodPlot DB2 has 16 fields as defined in NeighborhoodPlotMeta.
    // Parent index is field 8 (NeighborhoodMapID).
    //
    // Will be populated from DB2 store when registered.

    // Build map index
    for (auto const& [id, plot] : _neighborhoodPlotStore)
        _plotsByMap[plot.NeighborhoodMapID].push_back(&plot);

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodPlotData: Loaded {} NeighborhoodPlot entries across {} maps",
        uint32(_neighborhoodPlotStore.size()), uint32(_plotsByMap.size()));
}

void HousingMgr::LoadNeighborhoodNameGenData()
{
    // NeighborhoodNameGen DB2 has 4 fields as defined in NeighborhoodNameGenMeta:
    //  0: Prefix (string)
    //  1: Middle (string)
    //  2: Suffix (string)
    //  3: NeighborhoodMapID (int32) - parent index
    //
    // Will be populated from DB2 store when registered.

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
