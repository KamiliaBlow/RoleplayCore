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
#include "Group.h"
#include "Guild.h"
#include "Housing.h"
#include "Log.h"
#include "Neighborhood.h"
#include "Player.h"
#include "Random.h"
#include "SocialMgr.h"
#include "StringFormat.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>

namespace
{
    char const* SafeStr(char const* str) { return str ? str : ""; }
}

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
    LoadRoomComponentData();
    LoadDecorCategoryData();
    LoadDecorSubcategoryData();
    LoadDecorDyeSlotData();
    LoadDecorXDecorSubcategoryData();

    TC_LOG_INFO("server.loading", ">> Loaded housing data: {} decor, {} levels, "
        "{} rooms, {} themes, {} decor materials, {} exterior wmos, {} level rewards, "
        "{} initiatives, {} neighborhood maps, {} neighborhood plots, "
        "{} decor categories, {} decor subcategories, {} decor dye slots in {}",
        uint32(_houseDecorStore.size()), uint32(_houseLevelDataStore.size()),
        uint32(_houseRoomStore.size()), uint32(_houseThemeStore.size()),
        uint32(_houseDecorMaterialStore.size()), uint32(_houseExteriorWmoStore.size()),
        uint32(_houseLevelRewardInfoStore.size()), uint32(_neighborhoodInitiativeStore.size()),
        uint32(_neighborhoodMapStore.size()), uint32(_neighborhoodPlotStore.size()),
        uint32(_decorCategoryStore.size()), uint32(_decorSubcategoryStore.size()),
        uint32(_decorDyeSlotStore.size()),
        GetMSTimeDiffToNow(oldMSTime));
}

void HousingMgr::LoadHouseDecorData()
{
    for (HouseDecorEntry const* entry : sHouseDecorStore)
    {
        HouseDecorData& data = _houseDecorStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.InitialRotation[0] = entry->InitialRotation.X;
        data.InitialRotation[1] = entry->InitialRotation.Y;
        data.InitialRotation[2] = entry->InitialRotation.Z;
        data.Field_003 = entry->Field_003;
        data.GameObjectID = entry->GameObjectID;
        data.Flags = entry->Flags;
        data.Type = entry->Type;
        data.ModelType = entry->ModelType;
        data.ModelFileDataID = entry->ModelFileDataID;
        data.ThumbnailFileDataID = entry->ThumbnailFileDataID;
        data.WeightCost = entry->WeightCost > 0 ? entry->WeightCost : 1;
        data.ItemID = entry->ItemID;
        data.InitialScale = entry->InitialScale;
        data.FirstAcquisitionBonus = entry->FirstAcquisitionBonus;
        data.OrderIndex = entry->OrderIndex;
        data.Size = entry->Size;
        data.StartingQuantity = entry->StartingQuantity;
        data.UiModelSceneID = entry->UiModelSceneID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseDecorData: Loaded {} HouseDecor entries", uint32(_houseDecorStore.size()));
}

void HousingMgr::LoadHouseLevelData()
{
    for (HouseLevelDataEntry const* entry : sHouseLevelDataStore)
    {
        HouseLevelData& data = _houseLevelDataStore[entry->ID];
        data.ID = entry->ID;
        data.Level = entry->Level;
        data.QuestID = entry->QuestID;
        // Budget values from captured game data; interior scales per level, others are constant
        static constexpr int32 InteriorBudgetByLevel[] = { 0, 910, 1155, 1450, 1750, 2050 };
        uint32 lvl = static_cast<uint32>(std::max<int32>(entry->Level, 1));
        if (lvl <= 5)
            data.InteriorDecorPlacementBudget = InteriorBudgetByLevel[lvl];
        else
            data.InteriorDecorPlacementBudget = 2050 + static_cast<int32>((lvl - 5) * 300); // +300/level extrapolation
        data.ExteriorDecorPlacementBudget = 200;
        data.RoomPlacementBudget = 19;
        data.ExteriorFixtureBudget = 1000;
    }

    // Fallback defaults if no DB2 data available
    if (_houseLevelDataStore.empty())
    {
        for (uint32 level = 1; level <= 10; ++level)
        {
            HouseLevelData& data = _houseLevelDataStore[level];
            data.ID = level;
            data.Level = static_cast<int32>(level);
            data.QuestID = 0;
            static constexpr int32 InteriorBudgetByLevel[] = { 0, 910, 1155, 1450, 1750, 2050 };
            if (level <= 5)
                data.InteriorDecorPlacementBudget = InteriorBudgetByLevel[level];
            else
                data.InteriorDecorPlacementBudget = 2050 + static_cast<int32>((level - 5) * 300);
            data.ExteriorDecorPlacementBudget = 200;
            data.RoomPlacementBudget = 19;
            data.ExteriorFixtureBudget = 1000;
        }
    }

    // Build level lookup index (indexed by Level value, not by DB2 row ID)
    for (auto& [id, entry] : _houseLevelDataStore)
        _levelDataByLevel[entry.Level] = &entry;

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseLevelData: Loaded {} HouseLevelData entries", uint32(_houseLevelDataStore.size()));
}

void HousingMgr::LoadHouseRoomData()
{
    for (HouseRoomEntry const* entry : sHouseRoomStore)
    {
        HouseRoomData& data = _houseRoomStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.Size = entry->Size;
        data.Flags = entry->Flags;
        data.Field_002 = entry->Field_002;
        data.RoomWmoDataID = entry->RoomWmoDataID;
        data.UiTextureAtlasElementID = entry->UiTextureAtlasElementID;
        data.WeightCost = entry->WeightCost > 0 ? entry->WeightCost : 1;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseRoomData: Loaded {} HouseRoom entries", uint32(_houseRoomStore.size()));
}

void HousingMgr::LoadHouseThemeData()
{
    for (HouseThemeEntry const* entry : sHouseThemeStore)
    {
        HouseThemeData& data = _houseThemeStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
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
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
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
        data.Radius = entry->Radius;
        data.PlotCount = entry->PlotCount;
        data.FactionRestriction = entry->FactionRestriction;
    }

    // Build reverse lookup: world MapID -> NeighborhoodMap ID
    for (auto const& [id, data] : _neighborhoodMapStore)
    {
        _worldMapToNeighborhoodMap[data.MapID] = id;
        TC_LOG_DEBUG("housing", "  NeighborhoodMap ID={} MapID={} Origin=({}, {}, {}) Radius={} PlotCount={} FactionRestriction=0x{:X}",
            data.ID, data.MapID, data.Origin[0], data.Origin[1], data.Origin[2], data.Radius, data.PlotCount, data.FactionRestriction);
    }

    TC_LOG_INFO("housing", "HousingMgr::LoadNeighborhoodMapData: Loaded {} NeighborhoodMap entries", uint32(_neighborhoodMapStore.size()));
}

void HousingMgr::LoadNeighborhoodPlotData()
{
    for (NeighborhoodPlotEntry const* entry : sNeighborhoodPlotStore)
    {
        NeighborhoodPlotData& data = _neighborhoodPlotStore[entry->ID];
        data.ID = entry->ID;
        data.Cost = entry->Cost;
        data.Name = entry->Name ? entry->Name : "";
        data.HousePosition[0] = entry->HousePosition.X;
        data.HousePosition[1] = entry->HousePosition.Y;
        data.HousePosition[2] = entry->HousePosition.Z;
        data.HouseRotation[0] = entry->HouseRotation.X;
        data.HouseRotation[1] = entry->HouseRotation.Y;
        data.HouseRotation[2] = entry->HouseRotation.Z;
        data.CornerstonePosition[0] = entry->CornerstonePosition.X;
        data.CornerstonePosition[1] = entry->CornerstonePosition.Y;
        data.CornerstonePosition[2] = entry->CornerstonePosition.Z;
        data.CornerstoneRotation[0] = entry->CornerstoneRotation.X;
        data.CornerstoneRotation[1] = entry->CornerstoneRotation.Y;
        data.CornerstoneRotation[2] = entry->CornerstoneRotation.Z;
        data.TeleportPosition[0] = entry->TeleportPosition.X;
        data.TeleportPosition[1] = entry->TeleportPosition.Y;
        data.TeleportPosition[2] = entry->TeleportPosition.Z;
        data.NeighborhoodMapID = entry->NeighborhoodMapID;
        data.Field_010 = entry->Field_010;
        data.CornerstoneGameObjectID = entry->CornerstoneGameObjectID;
        data.PlotIndex = entry->PlotIndex;
        data.WorldState = entry->WorldState;
        data.PlotGameObjectID = entry->PlotGameObjectID;
        data.TeleportFacing = entry->TeleportFacing;
        data.Field_016 = entry->Field_016;
    }

    // Build map index
    for (auto const& [id, plot] : _neighborhoodPlotStore)
        _plotsByMap[plot.NeighborhoodMapID].push_back(&plot);

    TC_LOG_INFO("housing", "HousingMgr::LoadNeighborhoodPlotData: Loaded {} NeighborhoodPlot entries across {} maps",
        uint32(_neighborhoodPlotStore.size()), uint32(_plotsByMap.size()));

    // Dump per-map plot counts and sample GO entries for debugging
    for (auto const& [mapId, plotVec] : _plotsByMap)
    {
        uint32 hasForSale = 0, hasCornerstone = 0;
        for (auto const* p : plotVec)
        {
            if (p->PlotGameObjectID) ++hasForSale;
            if (p->CornerstoneGameObjectID) ++hasCornerstone;
        }
        TC_LOG_INFO("housing", "  NeighborhoodMapID={}: {} plots, {} with ForSaleGO, {} with CornerstoneGO",
            mapId, uint32(plotVec.size()), hasForSale, hasCornerstone);

        // Log all plots with their WorldState IDs
        for (auto const* p : plotVec)
        {
            TC_LOG_INFO("housing", "    Plot[{}]: ID={} ForSaleGO={} CornerstoneGO={} WorldState={} Cost={} HousePos=({:.4f}, {:.4f}, {:.4f}) HouseRot=({:.4f}, {:.4f}, {:.4f})",
                p->PlotIndex, p->ID, p->PlotGameObjectID, p->CornerstoneGameObjectID,
                p->WorldState, p->Cost,
                p->HousePosition[0], p->HousePosition[1], p->HousePosition[2],
                p->HouseRotation[0], p->HouseRotation[1], p->HouseRotation[2]);
        }
    }
}

void HousingMgr::LoadNeighborhoodNameGenData()
{
    for (NeighborhoodNameGenEntry const* entry : sNeighborhoodNameGenStore)
    {
        NeighborhoodNameGenData data;
        data.ID = entry->ID;
        data.Prefix = SafeStr(entry->Prefix[sWorld->GetDefaultDbcLocale()]);
        data.Middle = SafeStr(entry->Suffix[sWorld->GetDefaultDbcLocale()]);
        data.Suffix = SafeStr(entry->FullName[sWorld->GetDefaultDbcLocale()]);
        data.NeighborhoodMapID = entry->NeighborhoodMapID;
        _nameGenByMap[entry->NeighborhoodMapID].push_back(std::move(data));
    }

    uint32 totalEntries = 0;
    for (auto const& [mapId, entries] : _nameGenByMap)
        totalEntries += static_cast<uint32>(entries.size());

    TC_LOG_INFO("housing", "HousingMgr::LoadNeighborhoodNameGenData: Loaded {} entries across {} maps from base DB2",
        totalEntries, uint32(_nameGenByMap.size()));
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

bool HousingMgr::IsNeighborhoodWorldMap(uint32 mapId) const
{
    return _worldMapToNeighborhoodMap.contains(static_cast<int32>(mapId));
}

uint32 HousingMgr::GetNeighborhoodMapIdByWorldMap(uint32 mapId) const
{
    auto itr = _worldMapToNeighborhoodMap.find(static_cast<int32>(mapId));
    if (itr != _worldMapToNeighborhoodMap.end())
        return itr->second;
    return 0;
}

std::vector<NeighborhoodPlotData const*> HousingMgr::GetPlotsForMap(uint32 neighborhoodMapId) const
{
    auto itr = _plotsByMap.find(neighborhoodMapId);
    if (itr != _plotsByMap.end())
        return itr->second;

    TC_LOG_ERROR("housing", "HousingMgr::GetPlotsForMap: No plots found for neighborhoodMapId={}. Available map IDs:", neighborhoodMapId);
    for (auto const& [id, vec] : _plotsByMap)
        TC_LOG_ERROR("housing", "  neighborhoodMapId={} ({} plots)", id, uint32(vec.size()));

    return {};
}

NeighborhoodPlotData const* HousingMgr::GetPlotByCornerstoneEntry(uint32 neighborhoodMapId, uint32 cornerstoneGoEntry) const
{
    auto itr = _plotsByMap.find(neighborhoodMapId);
    if (itr == _plotsByMap.end())
        return nullptr;

    for (NeighborhoodPlotData const* plot : itr->second)
        if (static_cast<uint32>(plot->CornerstoneGameObjectID) == cornerstoneGoEntry)
            return plot;

    return nullptr;
}

int32 HousingMgr::ResolvePlotIndex(ObjectGuid cornerstoneGuid, Neighborhood const* neighborhood) const
{
    if (!neighborhood)
        return -1;

    uint32 goEntry = cornerstoneGuid.GetEntry();
    if (!goEntry)
        return -1;

    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    NeighborhoodPlotData const* plotData = GetPlotByCornerstoneEntry(neighborhoodMapId, goEntry);
    if (!plotData)
        return -1;

    return plotData->PlotIndex;
}

std::string HousingMgr::GenerateNeighborhoodName(uint32 neighborhoodMapId) const
{
    auto itr = _nameGenByMap.find(neighborhoodMapId);
    if (itr == _nameGenByMap.end() || itr->second.empty())
        return "Unnamed Neighborhood";

    std::vector<NeighborhoodNameGenData> const& nameGens = itr->second;
    uint32 count = static_cast<uint32>(nameGens.size());

    // Retail neighborhood names use hyphen-separated NeighborhoodNameGen entry IDs
    // (e.g., "75-78-61", "86-90-6"). The client resolves each token to localized
    // text from its local NeighborhoodNameGen.db2 (Prefix, Suffix, FullName fields).
    // Pick 3 random entries from this map's pool and combine their IDs.
    uint32 id1 = nameGens[urand(0, count - 1)].ID;
    uint32 id2 = nameGens[urand(0, count - 1)].ID;
    uint32 id3 = nameGens[urand(0, count - 1)].ID;

    return Trinity::StringFormat("{}-{}-{}", id1, id2, id3);
}

uint32 HousingMgr::GetMaxDecorForLevel(uint32 level) const
{
    // MaxDecorCount not in HouseLevelData DB2; use fallback formula
    return level * 25;
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

    // Fallback: sniff-confirmed interior budgets
    static constexpr uint32 InteriorBudgetByLevel[] = { 0, 910, 1155, 1450, 1750, 2050 };
    if (level >= 1 && level <= 5)
        return InteriorBudgetByLevel[level];
    if (level > 5)
        return 2050 + (level - 5) * 300;
    return 910;
}

uint32 HousingMgr::GetExteriorDecorBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->ExteriorDecorPlacementBudget > 0)
        return static_cast<uint32>(levelData->ExteriorDecorPlacementBudget);

    // Fallback: sniff-confirmed exterior budget (constant across levels)
    return 200;
}

uint32 HousingMgr::GetRoomBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->RoomPlacementBudget > 0)
        return static_cast<uint32>(levelData->RoomPlacementBudget);

    // Fallback: sniff-confirmed room budget (constant across levels)
    return 19;
}

uint32 HousingMgr::GetFixtureBudgetForLevel(uint32 level) const
{
    HouseLevelData const* levelData = GetLevelData(level);
    if (levelData && levelData->ExteriorFixtureBudget > 0)
        return static_cast<uint32>(levelData->ExteriorFixtureBudget);

    // Fallback: sniff-confirmed fixture budget (constant across levels)
    return 1000;
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

std::vector<uint32> HousingMgr::GetStarterDecorIds(uint32 teamId) const
{
    // Sniff 12.0.1 verified: Alliance and Horde receive different starter decor sets.
    // HouseDecor.Flags encodes faction availability:
    //   bit 0 (0x1) = Alliance, bit 1 (0x2) = Horde, 0 or 0x3 = both factions
    // Sniff-observed sets:
    //   Alliance: 389, 726, 1994, 1435, 9144
    //   Horde:    1700, 81, 10952, 2549, 8910
    static constexpr int32 HOUSE_DECOR_FLAG_FACTION_ALLIANCE = 0x1;
    static constexpr int32 HOUSE_DECOR_FLAG_FACTION_HORDE    = 0x2;
    static constexpr int32 HOUSE_DECOR_FLAG_FACTION_MASK     = 0x3;

    int32 factionBit = (teamId == ALLIANCE) ? HOUSE_DECOR_FLAG_FACTION_ALLIANCE : HOUSE_DECOR_FLAG_FACTION_HORDE;

    std::vector<uint32> result;
    for (auto const& [id, decor] : _houseDecorStore)
    {
        if (decor.StartingQuantity <= 0)
            continue;

        int32 decorFaction = decor.Flags & HOUSE_DECOR_FLAG_FACTION_MASK;
        // Include decor if: no faction restriction (0 or both bits set), or matches player's faction
        if (decorFaction == 0 || decorFaction == HOUSE_DECOR_FLAG_FACTION_MASK || (decorFaction & factionBit))
        {
            for (int32 i = 0; i < decor.StartingQuantity; ++i)
                result.push_back(id);
        }
    }
    return result;
}

bool HousingMgr::CanVisitorAccess(Player const* visitor, Player const* owner, uint32 settingsFlags, bool isInterior) const
{
    if (!visitor || !owner)
        return false;

    // Owner always has access
    if (visitor->GetGUID() == owner->GetGUID())
        return true;

    // Select the correct flag group based on access type
    uint32 anyoneFlag    = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_ANYONE    : HOUSE_SETTING_PLOT_ACCESS_ANYONE;
    uint32 neighborsFlag = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_NEIGHBORS : HOUSE_SETTING_PLOT_ACCESS_NEIGHBORS;
    uint32 guildFlag     = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_GUILD     : HOUSE_SETTING_PLOT_ACCESS_GUILD;
    uint32 friendsFlag   = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_FRIENDS   : HOUSE_SETTING_PLOT_ACCESS_FRIENDS;
    uint32 partyFlag     = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_PARTY     : HOUSE_SETTING_PLOT_ACCESS_PARTY;

    // If no flags are set at all, default to open access (sniff behavior: plots are public by default)
    uint32 accessMask = isInterior
        ? (HOUSE_SETTING_HOUSE_ACCESS_ANYONE | HOUSE_SETTING_HOUSE_ACCESS_NEIGHBORS |
           HOUSE_SETTING_HOUSE_ACCESS_GUILD | HOUSE_SETTING_HOUSE_ACCESS_FRIENDS | HOUSE_SETTING_HOUSE_ACCESS_PARTY)
        : (HOUSE_SETTING_PLOT_ACCESS_ANYONE | HOUSE_SETTING_PLOT_ACCESS_NEIGHBORS |
           HOUSE_SETTING_PLOT_ACCESS_GUILD | HOUSE_SETTING_PLOT_ACCESS_FRIENDS | HOUSE_SETTING_PLOT_ACCESS_PARTY);

    if ((settingsFlags & accessMask) == 0)
        return true; // No restrictions configured — open to all

    if (settingsFlags & anyoneFlag)
        return true;

    if ((settingsFlags & partyFlag) && visitor->GetGroup() && visitor->GetGroup() == owner->GetGroup())
        return true;

    if ((settingsFlags & guildFlag) && visitor->GetGuildId() != 0 && visitor->GetGuildId() == owner->GetGuildId())
        return true;

    if ((settingsFlags & friendsFlag) && owner->GetSocial() && owner->GetSocial()->HasFriend(visitor->GetGUID()))
        return true;

    if ((settingsFlags & neighborsFlag))
    {
        // Check if both players are in the same neighborhood
        Housing const* ownerHousing = owner->GetHousing();
        Housing const* visitorHousing = visitor->GetHousing();
        if (ownerHousing && visitorHousing &&
            ownerHousing->GetNeighborhoodGuid() == visitorHousing->GetNeighborhoodGuid())
            return true;
    }

    return false;
}

HousingResult HousingMgr::ValidateDecorPlacement(uint32 decorId, Position const& pos, uint32 houseLevel) const
{
    HouseDecorData const* decorEntry = GetHouseDecorData(decorId);
    if (!decorEntry)
        return HOUSING_RESULT_DECOR_NOT_FOUND;

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
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.Flags = entry->Flags;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadHouseExteriorWmoData: Loaded {} HouseExteriorWmoData entries", uint32(_houseExteriorWmoStore.size()));
}

void HousingMgr::LoadHouseLevelRewardInfoData()
{
    for (HouseLevelRewardInfoEntry const* entry : sHouseLevelRewardInfoStore)
    {
        HouseLevelRewardInfoData& data = _houseLevelRewardInfoStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.Description = SafeStr(entry->Description[sWorld->GetDefaultDbcLocale()]);
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
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.Description = SafeStr(entry->Description[sWorld->GetDefaultDbcLocale()]);
        data.InitiativeType = entry->InitiativeType;
        data.Duration = entry->Duration;
        data.RequiredParticipants = entry->RequiredParticipants;
        data.RewardCurrencyID = entry->RewardCurrencyID;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadNeighborhoodInitiativeData: Loaded {} NeighborhoodInitiative entries", uint32(_neighborhoodInitiativeStore.size()));
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

    return 0;
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

// --- 2 indexed lookup accessors ---

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

void HousingMgr::LoadDecorCategoryData()
{
    for (DecorCategoryEntry const* entry : sDecorCategoryStore)
    {
        DecorCategoryData& data = _decorCategoryStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.IconFileDataID = entry->IconFileDataID;
        data.DisplayIndex = entry->DisplayIndex;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadDecorCategoryData: Loaded {} decor categories", uint32(_decorCategoryStore.size()));
}

void HousingMgr::LoadDecorSubcategoryData()
{
    for (DecorSubcategoryEntry const* entry : sDecorSubcategoryStore)
    {
        DecorSubcategoryData& data = _decorSubcategoryStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.IconFileDataID = entry->IconFileDataID;
        data.DecorCategoryID = entry->DecorCategoryID;
        data.DisplayIndex = entry->DisplayIndex;

        _subcategoriesByCategory[entry->DecorCategoryID].push_back(&_decorSubcategoryStore[entry->ID]);
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadDecorSubcategoryData: Loaded {} decor subcategories", uint32(_decorSubcategoryStore.size()));
}

void HousingMgr::LoadDecorDyeSlotData()
{
    for (DecorDyeSlotEntry const* entry : sDecorDyeSlotStore)
    {
        DecorDyeSlotData& data = _decorDyeSlotStore[entry->ID];
        data.ID = entry->ID;
        data.SlotIndex = entry->SlotIndex;
        data.HouseDecorID = entry->HouseDecorID;
        data.DyeChannelType = entry->DyeChannelType;
        data.DefaultDyeRecordID = entry->DefaultDyeRecordID;

        _dyeSlotsByDecor[entry->HouseDecorID].push_back(&_decorDyeSlotStore[entry->ID]);
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadDecorDyeSlotData: Loaded {} decor dye slots", uint32(_decorDyeSlotStore.size()));
}

void HousingMgr::LoadDecorXDecorSubcategoryData()
{
    uint32 count = 0;
    for (DecorXDecorSubcategoryEntry const* entry : sDecorXDecorSubcategoryStore)
    {
        _decorsBySubcategory[entry->DecorSubcategoryID].push_back(entry->HouseDecorID);
        ++count;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::LoadDecorXDecorSubcategoryData: Loaded {} decor-to-subcategory links", count);
}

DecorCategoryData const* HousingMgr::GetDecorCategoryData(uint32 id) const
{
    auto itr = _decorCategoryStore.find(id);
    return itr != _decorCategoryStore.end() ? &itr->second : nullptr;
}

DecorSubcategoryData const* HousingMgr::GetDecorSubcategoryData(uint32 id) const
{
    auto itr = _decorSubcategoryStore.find(id);
    return itr != _decorSubcategoryStore.end() ? &itr->second : nullptr;
}

std::vector<DecorSubcategoryData const*> HousingMgr::GetSubcategoriesForCategory(uint32 categoryId) const
{
    auto itr = _subcategoriesByCategory.find(categoryId);
    if (itr != _subcategoriesByCategory.end())
        return itr->second;

    return {};
}

std::vector<uint32> HousingMgr::GetDecorIdsForSubcategory(uint32 subcategoryId) const
{
    auto itr = _decorsBySubcategory.find(subcategoryId);
    if (itr != _decorsBySubcategory.end())
        return itr->second;

    return {};
}

std::vector<DecorDyeSlotData const*> HousingMgr::GetDyeSlotsForDecor(uint32 houseDecorId) const
{
    auto itr = _dyeSlotsByDecor.find(houseDecorId);
    if (itr != _dyeSlotsByDecor.end())
        return itr->second;

    return {};
}
