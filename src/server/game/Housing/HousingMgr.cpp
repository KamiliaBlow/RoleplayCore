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
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "GameObjectData.h"
#include "Group.h"
#include "Guild.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuaternionData.h"
#include "RaceMask.h"
#include "Random.h"
#include "SharedDefines.h"
#include "SocialMgr.h"
#include <cmath>
#include "StringFormat.h"
#include "Timer.h"
#include "World.h"
#include <algorithm>
#include <unordered_set>

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
    BuildRoomComponentOptionIndex();
    BuildExteriorComponentIndexes();
    BuildRoomComponentTextureIndex();
    EnsureDoorGameObjectTemplates();

    // Initialize global DB ID generators from MAX(id) in character_housing_rooms/decor.
    // Must happen before any Housing objects are loaded to prevent cross-player ID collisions.
    Housing::InitializeDbIdGenerators();

    // Scan for base room entry from DB2 data (look for IsBaseRoom flag)
    for (auto const& [id, roomData] : _houseRoomStore)
    {
        if (roomData.IsBaseRoom())
        {
            _baseRoomEntryId = id;
            break;
        }
    }
    if (!_baseRoomEntryId)
    {
        _baseRoomEntryId = 18; // fallback
        TC_LOG_WARN("housing", "HousingMgr::Initialize: No room with BASE_ROOM flag found, "
            "falling back to entry 18");
    }

    // Scan for interior entry hall room (second BASE_ROOM after exterior geobox).
    // Retail has two base rooms: Room 18 (exterior geobox for SpawnRoomForPlot) and
    // Room 46 (interior entry corridor connecting to the visual room via a door).
    for (auto const& [id, roomData] : _houseRoomStore)
    {
        if (roomData.IsBaseRoom() && id != _baseRoomEntryId)
        {
            _entryHallRoomEntryId = id;
            break;
        }
    }
    if (!_entryHallRoomEntryId)
    {
        _entryHallRoomEntryId = _baseRoomEntryId;
        TC_LOG_WARN("housing", "HousingMgr::Initialize: No second BASE_ROOM found for entry hall, "
            "falling back to base room entry {}", _baseRoomEntryId);
    }

    TC_LOG_INFO("server.loading", ">> Loaded housing data: {} decor, {} levels, "
        "{} rooms, {} themes, {} decor materials, {} exterior wmos, {} level rewards, "
        "{} initiatives, {} neighborhood maps, {} neighborhood plots, "
        "{} decor categories, {} decor subcategories, {} decor dye slots, "
        "{} room component options in {}",
        uint32(_houseDecorStore.size()), uint32(_houseLevelDataStore.size()),
        uint32(_houseRoomStore.size()), uint32(_houseThemeStore.size()),
        uint32(_houseDecorMaterialStore.size()), uint32(_houseExteriorWmoStore.size()),
        uint32(_houseLevelRewardInfoStore.size()), uint32(_neighborhoodInitiativeStore.size()),
        uint32(_neighborhoodMapStore.size()), uint32(_neighborhoodPlotStore.size()),
        uint32(_decorCategoryStore.size()), uint32(_decorSubcategoryStore.size()),
        uint32(_decorDyeSlotStore.size()),
        uint32(sRoomComponentOptionStore.GetNumRows()),
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

}

void HousingMgr::LoadHouseLevelData()
{
    for (HouseLevelDataEntry const* entry : sHouseLevelDataStore)
    {
        HouseLevelData& data = _houseLevelDataStore[entry->ID];
        data.ID = entry->ID;
        data.Level = entry->Level;
        data.QuestID = entry->QuestID;
        // Budget values will be populated from HouseLevelRewardInfo DB2 (RewardType 38-41)
        // after LoadHouseLevelRewardInfoData(). Initialize to 0 here; fallbacks applied later.
        data.InteriorDecorPlacementBudget = 0;
        data.ExteriorDecorPlacementBudget = 0;
        data.RoomPlacementBudget = 0;
        data.ExteriorFixtureBudget = 0;
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
            data.InteriorDecorPlacementBudget = 0;
            data.ExteriorDecorPlacementBudget = 0;
            data.RoomPlacementBudget = 0;
            data.ExteriorFixtureBudget = 0;
        }
    }

    // Build level lookup index (indexed by Level value, not by DB2 row ID)
    for (auto& [id, entry] : _houseLevelDataStore)
        _levelDataByLevel[entry.Level] = &entry;

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
        // 0 is legal (the base room is free; the client's budget math charges 0 as well).
        data.WeightCost = entry->WeightCost >= 0 ? entry->WeightCost : 1;
    }

}

void HousingMgr::LoadHouseThemeData()
{
    for (HouseThemeEntry const* entry : sHouseThemeStore)
    {
        HouseThemeData& data = _houseThemeStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.Flags = entry->Flags;
        data.ParentThemeID = entry->ParentThemeID;
    }

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
        data.EntryRotation = entry->EntryRotation;
        data.UiTextureKitID = entry->UiTextureKitID;
        data.Flags = entry->Flags;
    }

    // Build reverse lookup: world MapID -> NeighborhoodMap ID
    for (auto const& [id, data] : _neighborhoodMapStore)
    {
        _worldMapToNeighborhoodMap[data.MapID] = id;
        // NeighborhoodMapFlags (IDA-confirmed): AlliancePurchasable=0x1, HordePurchasable=0x2, CanSystemGenerate=0x4
    }

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

        // The plot's centre is its PlotGameObjectID row of GameObjects.db2 ("Plot - Plot N"): retail puts the plot room
        // there to the centimetre (12.1.0.69933, Alliance plots 7 and 9). NeighborhoodPlot.HousePosition matches it on
        // most Alliance plots, but on the Horde map it belongs to other plots (53 of 55 rows are hundreds of yards off,
        // some out at sea), so a new Horde house was built in the water.
        if (GameObjectsEntry const* plotGo = sGameObjectsStore.LookupEntry(entry->PlotGameObjectID))
        {
            data.HousePosition[0] = plotGo->Pos.X;
            data.HousePosition[1] = plotGo->Pos.Y;
            data.HousePosition[2] = plotGo->Pos.Z;
        }
    }

    // Build map index
    for (auto const& [id, plot] : _neighborhoodPlotStore)
        _plotsByMap[plot.NeighborhoodMapID].push_back(&plot);

    // Validate that all CornerstoneGameObjectID entries have matching gameobject_template entries.
    // Templates are loaded from GameObjects.db2 (CASC) + gameobject_template SQL table.
    // Per-plot entries that are missing from CASC need world_housing_go_templates.sql applied.
    uint32 missingCornerstone = 0, missingPlotGO = 0, dynamicAdded = 0;
    for (auto const& [id, plot] : _neighborhoodPlotStore)
    {
        if (plot.CornerstoneGameObjectID)
        {
            uint32 entry = static_cast<uint32>(plot.CornerstoneGameObjectID);
            if (!sObjectMgr->GetGameObjectTemplate(entry))
            {
                // Dynamically register the missing cornerstone template.
                // All cornerstones are identical: type=48 (UILink), displayId=110660, scale=1.0
                // Data0=4 (CornerstoneInteraction), Data4=10 (radius), Data7=70 (PlayerInteractionType), Data8=1266097 (spell)
                GameObjectTemplate& got = const_cast<ObjectMgr*>(sObjectMgr)->GetGameObjectTemplateStoreForHotfix()[entry];
                got.entry = entry;
                got.type = 48; // GAMEOBJECT_TYPE_UI_LINK
                got.displayId = 110660;
                got.name = Trinity::StringFormat("Cornerstone Plot {} Map {}", plot.PlotIndex, plot.NeighborhoodMapID);
                got.IconName = "buy";
                got.size = 1.0f;
                memset(got.raw.data, 0, sizeof(got.raw.data));
                got.raw.data[0] = 4;       // UILinkType = CornerstoneInteraction
                got.raw.data[2] = 1;       // GiganticAOI
                got.raw.data[4] = 10;      // radius
                got.raw.data[7] = 70;      // PlayerInteractionType = CornerstoneInteraction
                got.raw.data[8] = 1266097; // spell = [DNT] Trigger Convo for Unowned Plot
                got.ContentTuningId = 0;
                got.RequiredLevel = 0;
                got.ScriptId = 0;
                got.InitializeQueryData();
                ++dynamicAdded;
                ++missingCornerstone;
            }
        }
        if (plot.PlotGameObjectID)
        {
            uint32 entry = static_cast<uint32>(plot.PlotGameObjectID);
            if (!sObjectMgr->GetGameObjectTemplate(entry))
            {
                // Register missing plot marker template.
                // All plot markers are identical: type=5 (Generic), displayId=113004, scale=1.0
                GameObjectTemplate& got = const_cast<ObjectMgr*>(sObjectMgr)->GetGameObjectTemplateStoreForHotfix()[entry];
                got.entry = entry;
                got.type = 5; // GAMEOBJECT_TYPE_GENERIC
                got.displayId = 113004;
                got.name = Trinity::StringFormat("Plot {} Map {}", plot.PlotIndex, plot.NeighborhoodMapID);
                got.size = 1.0f;
                memset(got.raw.data, 0, sizeof(got.raw.data));
                got.raw.data[1] = 1; // Data1
                got.ContentTuningId = 0;
                got.RequiredLevel = 0;
                got.ScriptId = 0;
                got.InitializeQueryData();
                ++dynamicAdded;
                ++missingPlotGO;
            }
        }
    }

    // The shared cornerstone entry itself: normally provided by GameObjects.db2 (official
    // "Cornerstone" row) or gameobject_template. If the server data has neither, register it
    // so SpawnPlotGameObjects can still create the GO with the retail interaction data.
    if (!sObjectMgr->GetGameObjectTemplate(HOUSING_CORNERSTONE_GAMEOBJECT_ENTRY))
    {
        GameObjectTemplate& got = const_cast<ObjectMgr*>(sObjectMgr)->GetGameObjectTemplateStoreForHotfix()[HOUSING_CORNERSTONE_GAMEOBJECT_ENTRY];
        got.entry = HOUSING_CORNERSTONE_GAMEOBJECT_ENTRY;
        got.type = 48; // GAMEOBJECT_TYPE_UI_LINK
        got.displayId = 110660;
        got.name = "Cornerstone";
        got.IconName = "buy";
        got.size = 1.0f;
        memset(got.raw.data, 0, sizeof(got.raw.data));
        got.raw.data[0] = 4;       // UILinkType = CornerstoneInteraction
        got.raw.data[2] = 1;       // GiganticAOI
        got.raw.data[4] = 10;      // radius
        got.raw.data[7] = 70;      // PlayerInteractionType = CornerstoneInteraction
        got.raw.data[8] = 1266097; // spell = [DNT] Trigger Convo for Unowned Plot
        got.ContentTuningId = 0;
        got.RequiredLevel = 0;
        got.ScriptId = 0;
        got.InitializeQueryData();
        ++dynamicAdded;
        ++missingCornerstone;
    }

    if (dynamicAdded > 0)
    {
        TC_LOG_ERROR("housing", "HousingMgr::LoadNeighborhoodPlotData: {} cornerstone + {} plot marker GO templates were MISSING from gameobject_template and GameObjects.db2. "
            "Dynamically registered them. Re-extract GameObjects.db2 or add the templates to gameobject_template.",
            missingCornerstone, missingPlotGO);
    }
}

void HousingMgr::LoadNeighborhoodNameGenData()
{
    for (NeighborhoodNameGenEntry const* entry : sNeighborhoodNameGenStore)
    {
        NeighborhoodNameGenData data;
        data.ID = entry->ID;
        data.Prefix = SafeStr(entry->Prefix[sWorld->GetDefaultDbcLocale()]);
        data.Middle = SafeStr(entry->Middle[sWorld->GetDefaultDbcLocale()]);
        data.Suffix = SafeStr(entry->Suffix[sWorld->GetDefaultDbcLocale()]);
        data.NeighborhoodMapID = entry->NeighborhoodMapID;
        _nameGenByMap[entry->NeighborhoodMapID].push_back(std::move(data));
    }

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

NeighborhoodMapData const* HousingMgr::GetNeighborhoodMapDataForWorldMap(uint32 mapId) const
{
    uint32 nmId = GetNeighborhoodMapIdByWorldMap(mapId);
    return nmId ? GetNeighborhoodMapData(nmId) : nullptr;
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

uint32 HousingMgr::GetWorldMapIdByNeighborhoodMapId(uint32 neighborhoodMapId) const
{
    for (auto const& [worldMapId, nmId] : _worldMapToNeighborhoodMap)
    {
        if (nmId == neighborhoodMapId)
            return static_cast<uint32>(worldMapId);
    }
    return 0;
}

WorldLocation HousingMgr::GetPlotTeleportLocation(uint32 worldMapId, NeighborhoodPlotData const& plot)
{
    return WorldLocation(worldMapId, plot.TeleportPosition[0], plot.TeleportPosition[1], plot.TeleportPosition[2],
        plot.CornerstoneRotation[2]);
}

void HousingMgr::SetPendingPlotTeleport(ObjectGuid playerGuid, WorldLocation const& dest, uint32 neighborhoodId)
{
    std::lock_guard<std::mutex> lock(_pendingPlotTeleportsLock);
    _pendingPlotTeleports[playerGuid] = { dest, neighborhoodId };
}

Optional<HousingMgr::PendingPlotTeleport> HousingMgr::TakePendingPlotTeleport(ObjectGuid playerGuid)
{
    std::lock_guard<std::mutex> lock(_pendingPlotTeleportsLock);
    auto itr = _pendingPlotTeleports.find(playerGuid);
    if (itr == _pendingPlotTeleports.end())
        return {};
    PendingPlotTeleport pending = itr->second;
    _pendingPlotTeleports.erase(itr);
    return pending;
}

Position HousingMgr::GetDefaultHousePosition(NeighborhoodPlotData const& plot) const
{
    // 12.1.0.69933 sniffs (both factions): a house root that was never moved sits at local (0,0,0) with no rotation under
    // the plot room, and the plot room is the plot's GameObjects.db2 row turned half a revolution.
    if (GameObjectsEntry const* plotGo = sGameObjectsStore.LookupEntry(plot.PlotGameObjectID))
    {
        float goYaw = 0.0f, unusedY = 0.0f, unusedX = 0.0f;
        QuaternionData(plotGo->Rot[0], plotGo->Rot[1], plotGo->Rot[2], plotGo->Rot[3]).toEulerAnglesZYX(goYaw, unusedY, unusedX);
        return Position(plotGo->Pos.X, plotGo->Pos.Y, plotGo->Pos.Z, Position::NormalizeOrientation(goYaw + float(M_PI)));
    }

    // No plot GameObject: the plot's own position, facing the cornerstone.
    return Position(plot.HousePosition[0], plot.HousePosition[1], plot.HousePosition[2],
        std::atan2(plot.CornerstonePosition[1] - plot.HousePosition[1], plot.CornerstonePosition[0] - plot.HousePosition[0]));
}

std::vector<NeighborhoodPlotData const*> const& HousingMgr::GetPlotsForMap(uint32 neighborhoodMapId) const
{
    auto itr = _plotsByMap.find(neighborhoodMapId);
    if (itr != _plotsByMap.end())
        return itr->second;

    TC_LOG_ERROR("housing", "HousingMgr::GetPlotsForMap: No plots found for neighborhoodMapId={}. Available map IDs:", neighborhoodMapId);
    for (auto const& [id, vec] : _plotsByMap)
        TC_LOG_ERROR("housing", "  neighborhoodMapId={} ({} plots)", id, uint32(vec.size()));

    static std::vector<NeighborhoodPlotData const*> const empty;
    return empty;
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

int32 HousingMgr::ResolvePlotIndex(WorldObject const* searcher, ObjectGuid cornerstoneGuid, Neighborhood const* neighborhood) const
{
    if (!neighborhood)
    {
        TC_LOG_ERROR("housing", "HousingMgr::ResolvePlotIndex: neighborhood is null");
        return -1;
    }

    // Only GameObject GUIDs encode a GO entry that can be matched against cornerstone entries.
    // Housing/Neighborhood GUIDs (HighGuid 55) don't have a GO entry — callers sometimes
    // pass these for diagnostic purposes; silently return -1.
    if (cornerstoneGuid.GetHigh() != HighGuid::GameObject)
    {
        return -1;
    }

    // Preferred: the PlotIndex carried in the GO's FJamHousingCornerstone_C fragment (all plots
    // share one Cornerstone GO entry, so the entry no longer identifies the plot).
    if (GameObject const* cornerstone = searcher ? ObjectAccessor::GetGameObject(*searcher, cornerstoneGuid) : nullptr)
    {
        int32 const fragmentPlot = cornerstone->GetHousingCornerstonePlotIndex();
        if (fragmentPlot >= 0)
        {
            return fragmentPlot;
        }
    }

    // Fallback: legacy per-plot GO entries (NeighborhoodPlot.CornerstoneGameObjectID).
    uint32 goEntry = cornerstoneGuid.GetEntry();
    if (!goEntry)
    {
        TC_LOG_ERROR("housing", "HousingMgr::ResolvePlotIndex: GetEntry() returned 0 for GUID {} (HighGuid: {})",
            cornerstoneGuid.ToString(), static_cast<uint32>(cornerstoneGuid.GetHigh()));
        return -1;
    }

    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    NeighborhoodPlotData const* plotData = GetPlotByCornerstoneEntry(neighborhoodMapId, goEntry);
    if (!plotData)
    {
        return -1;
    }

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

// Per-level house values are GlobalCurve.db2 curves (types 37-41) evaluated at the house level; every
// value matches the Housing/3 entities of the 12.1.0.69933 retail sniffs for levels 1-12:
//   37 cumulative favor to reach the level, 38 interior decor, 39 exterior decor,
//   40 room placement (19..134), 41 exterior fixture (1000..5000) budget.
static uint32 GetHouseLevelCurveValue(GlobalCurve curve, uint32 level)
{
    uint32 const curveId = sDB2Manager.GetGlobalCurveId(curve);
    if (!curveId)
        return 0;

    return static_cast<uint32>(std::lround(sDB2Manager.GetCurveValueAt(curveId, float(std::max<uint32>(level, 1)))));
}

uint32 HousingMgr::GetFavorThresholdForLevel(uint32 level) const
{
    return GetHouseLevelCurveValue(GlobalCurve::HouseLevelFavorForLevel, level);
}

uint32 HousingMgr::GetInteriorDecorBudgetForLevel(uint32 level) const
{
    return GetHouseLevelCurveValue(GlobalCurve::HouseInteriorDecorBudget, level);
}

uint32 HousingMgr::GetExteriorDecorBudgetForLevel(uint32 level) const
{
    return GetHouseLevelCurveValue(GlobalCurve::HouseExteriorDecorBudget, level);
}

uint32 HousingMgr::GetRoomBudgetForLevel(uint32 level) const
{
    return GetHouseLevelCurveValue(GlobalCurve::HouseRoomPlacementBudget, level);
}

uint32 HousingMgr::GetFixtureBudgetForLevel(uint32 level) const
{
    return GetHouseLevelCurveValue(GlobalCurve::HouseFixtureBudget, level);
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
    // The upper half of a stairwell is free through Housing::GetRoomWeightCost's stairwell rule.
    HouseRoomData const* roomData = GetHouseRoomData(roomEntryId);
    if (roomData)
        return static_cast<uint32>(std::max<int32>(roomData->WeightCost, 0));

    return 1;
}

std::vector<std::pair<uint32, int32>> HousingMgr::GetStarterDecorWithQuantities(uint32 /*teamId*/) const
{
    // Returns {DecorID, StartingQuantity} pairs for the purchase grant. NO faction filter:
    //
    // 1. The client credits HouseDecor.StartingQuantity as "remaining redeemable" for every
    //    SQ > 0 row regardless of the player's faction — proven by decor 9144/10952 (Flags 34,
    //    doors) showing as an unretirable phantom "1" on an ALLIANCE character whose catalog
    //    had no row for them: the previous faction filter left those credits unretired forever.
    //    The only thing that retires a credit is a SourceType 3 storage entry, i.e. granting
    //    the copy. Every SQ row must therefore be granted to everyone.
    // 2. Retail keeps no faction split of the collection either: the Horde account of the
    //    2026-09-26 retail capture owned the "Alliance" starter decor (389, 726, 1994, 1435)
    //    alongside both front doors (9144, 10952). Sets that looked faction-specific in
    //    12.0.1 sniffs are server-side curation Blizzard no longer expresses through these
    //    flags in 69933 (most SQ rows are Flags 3 = both factions).
    // teamId is kept in the signature so call sites read naturally; it selects nothing.
    std::vector<std::pair<uint32, int32>> result;
    for (auto const& [id, decor] : _houseDecorStore)
    {
        if (decor.StartingQuantity <= 0)
            continue;

        result.push_back({ id, decor.StartingQuantity });
    }
    return result;
}

bool HousingMgr::CanVisitorAccessPlot(Player const* visitor, ObjectGuid ownerGuid, uint32 settingsFlags, bool isInterior) const
{
    if (!visitor || ownerGuid.IsEmpty())
        return false;

    // Houses belong to the account: every character of the owner's account has owner access.
    if (visitor->GetGUID() == ownerGuid || visitor->GetHousingByOwner(ownerGuid))
        return true;

    uint32 anyoneFlag    = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_ANYONE    : HOUSE_SETTING_PLOT_ACCESS_ANYONE;
    uint32 neighborsFlag = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_NEIGHBORS : HOUSE_SETTING_PLOT_ACCESS_NEIGHBORS;
    uint32 guildFlag     = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_GUILD     : HOUSE_SETTING_PLOT_ACCESS_GUILD;
    uint32 friendsFlag   = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_FRIENDS   : HOUSE_SETTING_PLOT_ACCESS_FRIENDS;
    uint32 partyFlag     = isInterior ? HOUSE_SETTING_HOUSE_ACCESS_PARTY     : HOUSE_SETTING_PLOT_ACCESS_PARTY;

    uint32 accessMask = isInterior
        ? (HOUSE_SETTING_HOUSE_ACCESS_ANYONE | HOUSE_SETTING_HOUSE_ACCESS_NEIGHBORS |
           HOUSE_SETTING_HOUSE_ACCESS_GUILD | HOUSE_SETTING_HOUSE_ACCESS_FRIENDS | HOUSE_SETTING_HOUSE_ACCESS_PARTY)
        : (HOUSE_SETTING_PLOT_ACCESS_ANYONE | HOUSE_SETTING_PLOT_ACCESS_NEIGHBORS |
           HOUSE_SETTING_PLOT_ACCESS_GUILD | HOUSE_SETTING_PLOT_ACCESS_FRIENDS | HOUSE_SETTING_PLOT_ACCESS_PARTY);

    // NO "no bits = open to all" fallback here: unchecked boxes must deny. The owner
    // clearing every dropdown (flags 0) means "nobody" — treating that as open made the
    // access settings appear to do nothing (guest walked the revoked plot freely).

    if (settingsFlags & anyoneFlag)
        return true;

    Player* ownerPlayer = ObjectAccessor::FindPlayer(ownerGuid);

    if (settingsFlags & partyFlag)
    {
        // Party requires both online — same Group instance.
        if (ownerPlayer && visitor->GetGroup() && visitor->GetGroup() == ownerPlayer->GetGroup())
            return true;
    }

    if (settingsFlags & guildFlag)
    {
        ObjectGuid::LowType ownerGuildId = ownerPlayer
            ? ownerPlayer->GetGuildId()
            : sCharacterCache->GetCharacterGuildIdByGuid(ownerGuid);
        if (ownerGuildId != 0 && visitor->GetGuildId() == ownerGuildId)
            return true;
    }

    if (settingsFlags & friendsFlag)
    {
        // Friends are mutual on retail — visitor's social manager has the same record.
        if (visitor->GetSocial() && visitor->GetSocial()->HasFriend(ownerGuid))
            return true;
    }

    if (settingsFlags & neighborsFlag)
    {
        // Both are residents of the same neighborhood. Works offline because
        // neighborhood membership is stored on Neighborhood objects, not Player.
        for (Neighborhood const* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(ownerGuid))
            if (nbh->IsMember(visitor->GetGUID()))
                return true;
    }

    return false;
}

bool HousingMgr::CanVisitorExportBlueprint(Player const* visitor, ObjectGuid ownerGuid, uint32 settingsFlags) const
{
    if (!visitor || ownerGuid.IsEmpty())
        return false;

    if (visitor->GetGUID() == ownerGuid || visitor->GetHousingByOwner(ownerGuid))
        return true;

    if (settingsFlags & HOUSE_SETTING_BLUEPRINT_EXPORT_ANYONE)
        return true;

    Player* ownerPlayer = ObjectAccessor::FindPlayer(ownerGuid);

    if ((settingsFlags & HOUSE_SETTING_BLUEPRINT_EXPORT_PARTY) && ownerPlayer && visitor->GetGroup()
        && visitor->GetGroup() == ownerPlayer->GetGroup())
        return true;

    if (settingsFlags & HOUSE_SETTING_BLUEPRINT_EXPORT_GUILD)
    {
        ObjectGuid::LowType ownerGuildId = ownerPlayer ? ownerPlayer->GetGuildId() : sCharacterCache->GetCharacterGuildIdByGuid(ownerGuid);
        if (ownerGuildId != 0 && visitor->GetGuildId() == ownerGuildId)
            return true;
    }

    if ((settingsFlags & HOUSE_SETTING_BLUEPRINT_EXPORT_FRIENDS) && visitor->GetSocial() && visitor->GetSocial()->HasFriend(ownerGuid))
        return true;

    if (settingsFlags & HOUSE_SETTING_BLUEPRINT_EXPORT_NEIGHBORS)
        for (Neighborhood const* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(ownerGuid))
            if (nbh->IsMember(visitor->GetGUID()))
                return true;

    return false;
}

HousingResult HousingMgr::ValidateDecorPlacement(uint32 decorId, Position const& pos, Position const& anchor, uint32 houseLevel) const
{
    HouseDecorData const* decorEntry = GetHouseDecorData(decorId);
    if (!decorEntry)
        return HOUSING_RESULT_DECOR_NOT_FOUND;

    // Validate position is finite / not obviously corrupt.
    if (!pos.IsPositionValid())
        return HOUSING_RESULT_BOUNDS_FAILURE_ROOM;

    // M1/A4: reject placements far from the house they belong to (arbitrary-coordinate spam). Decor positions
    // are world coordinates; measuring them from 0,0,0 refused every plot placement (plots sit at x ~ 3000).
    if (std::fabs(pos.GetPositionX() - anchor.GetPositionX()) > HOUSING_MAX_DECOR_LOCAL_EXTENT ||
        std::fabs(pos.GetPositionY() - anchor.GetPositionY()) > HOUSING_MAX_DECOR_LOCAL_EXTENT ||
        std::fabs(pos.GetPositionZ() - anchor.GetPositionZ()) > HOUSING_MAX_DECOR_LOCAL_EXTENT)
        return HOUSING_RESULT_BOUNDS_FAILURE_PLOT;

    // Validate house level meets decor requirements (if any level restriction exists)
    // For now, all decor is available at any level; future DB2 fields may add restrictions
    (void)houseLevel;

    // #16 Outdoor Lighting: there is intentionally NO category gate here — 12.0.7
    // permits DecorCategory 4 "Lighting" decor in the exterior/plot scope, so it
    // flows through the same exterior placement path as any other yard decor. The
    // light-specific "two lights cannot overlap" rule (A4) is enforced with the
    // placed-decor context in Housing::CheckLightOverlap, not here (this validator
    // is per-decor and stateless). Client-side indoor/outdoor gating via
    // HouseDecor.Flags is tracked separately as audit item M10.

    return HOUSING_RESULT_SUCCESS;
}

// --- 7 new DB2 Load functions ---

void HousingMgr::LoadHouseDecorMaterialData()
{
    for (HouseDecorMaterialEntry const* entry : sHouseDecorMaterialStore)
    {
        HouseDecorMaterialData& data = _houseDecorMaterialStore[entry->ID];
        data.ID = entry->ID;
        data.WMOMaterialReference = entry->WMOMaterialReference;
        data.MaterialTextureIndex = entry->MaterialTextureIndex;
        data.HouseThemeID = entry->HouseThemeID;
        data.TextureAFileDataID = entry->TextureAFileDataID;
        data.TextureBFileDataID = entry->TextureBFileDataID;
    }

    // Build decor material index
    for (auto const& [id, mat] : _houseDecorMaterialStore)
        _materialsByTheme[mat.HouseThemeID].push_back(&mat);

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

}

void HousingMgr::LoadHouseLevelRewardInfoData()
{
    for (HouseLevelRewardInfoEntry const* entry : sHouseLevelRewardInfoStore)
    {
        HouseLevelRewardInfoData& data = _houseLevelRewardInfoStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.Description = SafeStr(entry->Description[sWorld->GetDefaultDbcLocale()]);
        data.HouseLevelDataID = entry->HouseLevelDataID;
        data.Field_4 = entry->Field_4;
        data.IconFileDataID = entry->IconFileDataID;
    }

    // Build level reward index
    for (auto const& [id, reward] : _houseLevelRewardInfoStore)
        _rewardsByLevel[reward.HouseLevelDataID].push_back(&reward);

    // HouseLevelRewardInfo: HouseLevelDataID points at the HouseLevelData row ID, Field_4 is the display
    // order inside a level. The rewards are display text only; budgets come from GlobalCurve 37-41
    // (see GetHouseLevelCurveValue) and rooms/decor from the level's award quest (HouseLevelData.QuestID).
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

}

void HousingMgr::LoadRoomComponentData()
{
    uint32 doorwayCount = 0;
    uint32 totalCount = 0;

    for (RoomComponentEntry const* entry : sRoomComponentStore)
    {
        // Store all components indexed by RoomWmoDataID for room spawning
        RoomComponentData compData;
        compData.ID = entry->ID;
        compData.RoomWmoDataID = entry->RoomWmoDataID;
        compData.OffsetPos[0] = entry->OffsetPos.X;
        compData.OffsetPos[1] = entry->OffsetPos.Y;
        compData.OffsetPos[2] = entry->OffsetPos.Z;
        compData.OffsetRot[0] = entry->OffsetRot.X;
        compData.OffsetRot[1] = entry->OffsetRot.Y;
        compData.OffsetRot[2] = entry->OffsetRot.Z;
        compData.ModelFileDataID = entry->ModelFileDataID;
        compData.Type = entry->Type;
        compData.MeshStyleFilterID = entry->MeshStyleFilterID;
        compData.ConnectionType = entry->ConnectionType;
        compData.Flags = entry->Flags;

        _roomComponentsByWmoData[entry->RoomWmoDataID].push_back(compData);
        ++totalCount;

        // Also index doorway components separately for connectivity checks
        if (entry->Type == HOUSING_ROOM_COMPONENT_DOORWAY)
        {
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
    }

}

std::vector<RoomComponentData> const* HousingMgr::GetRoomComponents(uint32 roomWmoDataId) const
{
    auto itr = _roomComponentsByWmoData.find(roomWmoDataId);
    if (itr != _roomComponentsByWmoData.end())
        return &itr->second;

    return nullptr;
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

std::vector<HouseDecorMaterialData const*> HousingMgr::GetMaterialsForTheme(uint32 houseThemeId) const
{
    auto itr = _materialsByTheme.find(houseThemeId);
    if (itr != _materialsByTheme.end())
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
        data.UiTextureAtlasElementID = entry->UiTextureAtlasElementID;
        data.OrderIndex = entry->OrderIndex;
    }

}

void HousingMgr::LoadDecorSubcategoryData()
{
    for (DecorSubcategoryEntry const* entry : sDecorSubcategoryStore)
    {
        DecorSubcategoryData& data = _decorSubcategoryStore[entry->ID];
        data.ID = entry->ID;
        data.Name = SafeStr(entry->Name[sWorld->GetDefaultDbcLocale()]);
        data.UiTextureAtlasElementID = entry->UiTextureAtlasElementID;
        data.DecorCategoryID = entry->DecorCategoryID;
        data.OrderIndex = entry->OrderIndex;

        _subcategoriesByCategory[entry->DecorCategoryID].push_back(&_decorSubcategoryStore[entry->ID]);
    }

}

void HousingMgr::LoadDecorDyeSlotData()
{
    for (DecorDyeSlotEntry const* entry : sDecorDyeSlotStore)
    {
        DecorDyeSlotData& data = _decorDyeSlotStore[entry->ID];
        data.ID = entry->ID;
        data.DyeColorCategoryID = entry->DyeColorCategoryID;
        data.HouseDecorID = entry->HouseDecorID;
        data.OrderIndex = entry->OrderIndex;
        data.Channel = entry->Channel;

        _dyeSlotsByDecor[entry->HouseDecorID].push_back(&_decorDyeSlotStore[entry->ID]);
    }

}

void HousingMgr::LoadDecorXDecorSubcategoryData()
{
    uint32 count = 0;
    for (DecorXDecorSubcategoryEntry const* entry : sDecorXDecorSubcategoryStore)
    {
        _decorsBySubcategory[entry->DecorSubcategoryID].push_back(entry->HouseDecorID);
        // #16 Outdoor Lighting: build a decor -> parent-category reverse index so
        // the placement path can classify a decor (Lighting = category 4) in O(1)
        // without walking subcategories. DecorSubcategory rows are already loaded
        // (LoadDecorSubcategoryData runs before this pass).
        if (DecorSubcategoryData const* sub = GetDecorSubcategoryData(entry->DecorSubcategoryID))
            _categoryByDecor[entry->HouseDecorID] = uint32(sub->DecorCategoryID);
        ++count;
    }

}

void HousingMgr::BuildRoomComponentOptionIndex()
{
    _roomCompOptionIndex.clear();
    uint32 count = 0;
    for (RoomComponentOptionEntry const* entry : sRoomComponentOptionStore)
    {
        if (!entry)
            continue;
        // Index by (MeshStyleFilterID, HouseThemeID).
        // Multiple options exist per key with different Types:
        //   Type=0 (Cosmetic) = normal solid wall (DEFAULT)
        //   Type=1 (DoorwayWall) = wall with sealed doorway frame
        //   Type=2 (Doorway) = open doorway passage
        // Prefer Type=0 (Cosmetic) as the default wall model. Type 1/2 are
        // used for doorway-capable walls based on connection state.
        uint64 key = (uint64(uint32(entry->MeshStyleFilterID)) << 32) | uint32(entry->HouseThemeID);
        auto existing = _roomCompOptionIndex.find(key);
        if (existing == _roomCompOptionIndex.end())
            _roomCompOptionIndex[key] = entry;
        else if (entry->Type == 0 && existing->second->Type != 0)
            _roomCompOptionIndex[key] = entry; // Replace non-Cosmetic with Cosmetic
        ++count;
    }
}

void HousingMgr::BuildRoomComponentTextureIndex()
{
    _textureByOptionId.clear();
    _textureByComponentType.clear();

    // Build option→texture link from RoomComponentOptionTexture join table
    for (RoomComponentOptionTextureEntry const* link : sRoomComponentOptionTextureStore)
    {
        if (!link)
            continue;
        _textureByOptionId[link->RoomComponentOptionID] = link->RoomComponentTextureID;
    }

    // Build type→texture fallback from RoomComponentTexture
    // "Type" in RoomComponentTexture maps to component type (1=wall, 2=floor, 3=ceiling)
    for (RoomComponentTextureEntry const* tex : sRoomComponentTextureStore)
    {
        if (!tex || tex->Type <= 0)
            continue;
        uint8 compType = static_cast<uint8>(tex->Type);
        if (!_textureByComponentType.contains(compType))
            _textureByComponentType[compType] = static_cast<int32>(tex->ID);
    }

}

int32 HousingMgr::GetTextureIdForComponentOption(int32 roomComponentOptionID) const
{
    auto itr = _textureByOptionId.find(roomComponentOptionID);
    return itr != _textureByOptionId.end() ? itr->second : 0;
}

int32 HousingMgr::GetTextureIdForComponentType(uint8 componentType) const
{
    auto itr = _textureByComponentType.find(componentType);
    return itr != _textureByComponentType.end() ? itr->second : 0;
}

void HousingMgr::EnsureDoorGameObjectTemplates()
{
    // Auto-create missing GO templates for door components referenced in DB2.
    // ExteriorComponent entries with Type=11 (Door) have a GameObjectID for the
    // clickable entrance GO. If the template doesn't exist, create one based on
    // the known working template (entry 586576, type 10/GOOBER).
    uint32 created = 0;
    uint32 scripted = 0;

    GameObjectTemplate const* referenceTemplate = sObjectMgr->GetGameObjectTemplate(586576);
    uint32 referenceDisplayId = referenceTemplate ? referenceTemplate->displayId : 116973;

    // Every door - whether it comes from the world DB or is synthesised below - must carry
    // go_housing_door, because that script's OnGossipHello IS the "enter the house" handler.
    // GameObject::Use() calls AI()->OnGossipHello() for every GO type, so with no ScriptName
    // the click reaches the default AI, falls through to the GOOBER branch, plays the open
    // animation and does nothing else: the door looked interactive (gear cursor, client sends
    // CMSG_GAME_OBJ_USE) but never teleported anyone.
    uint32 const doorScriptId = sObjectMgr->GetScriptId("go_housing_door", false);

    for (ExteriorComponentEntry const* entry : sExteriorComponentStore)
    {
        if (!entry || entry->Type != 11 || entry->GameObjectID <= 0) // Type 11 = Door
            continue;

        uint32 goEntry = static_cast<uint32>(entry->GameObjectID);
        if (GameObjectTemplate const* existing = sObjectMgr->GetGameObjectTemplate(goEntry))
        {
            // Present already (world DB). Attach the door script unless something
            // deliberate is bound there - never clobber an explicit ScriptName.
            if (!existing->ScriptId)
            {
                sObjectMgr->GetGameObjectTemplateStoreForHotfix()[goEntry].ScriptId = doorScriptId;
                ++scripted;
            }
            continue;
        }

        // Create a GOOBER template (type=10) — clickable interaction object for house entry
        std::string name = entry->Name[DEFAULT_LOCALE] ? entry->Name[DEFAULT_LOCALE] : "Housing Door";

        // Insert directly into ObjectMgr's in-memory store (no DB write needed — these are derived from DB2)
        // Sniff-verified retail values: Lock=4296 (Opening cast bar), autoClose=3000ms, startOpen=1
        GameObjectTemplate& goTemplate = sObjectMgr->GetGameObjectTemplateStoreForHotfix()[goEntry];
        goTemplate.entry = goEntry;
        goTemplate.type = GAMEOBJECT_TYPE_GOOBER;
        goTemplate.displayId = referenceDisplayId;
        goTemplate.name = name;
        goTemplate.size = 1.0f;
        goTemplate.goober.open = 4296;          // Lock_ ID for "Opening" cast bar
        goTemplate.goober.autoClose = 3000;     // 3 seconds auto-close
        goTemplate.goober.startOpen = 1;        // start in open state
        goTemplate.ScriptId = doorScriptId;
        goTemplate.InitializeQueryData();

        ++created;
    }

    // The INTERIOR doors are not reachable from ExteriorComponent - HouseInteriorMap picks them
    // by faction in code (Alliance 575017 / Horde 587318), so the DB2 walk above never sees them
    // and they stayed unscripted after the exterior doors were fixed: the front door worked, the
    // one inside the house did nothing. Keep this list in step with HouseInteriorMap.
    for (uint32 goEntry : { INTERIOR_DOOR_GO_ALLIANCE, INTERIOR_DOOR_GO_HORDE })
    {
        GameObjectTemplate const* existing = sObjectMgr->GetGameObjectTemplate(goEntry);
        if (!existing)
        {
            TC_LOG_ERROR("housing", "EnsureDoorGameObjectTemplates: interior door GO {} has no template - the interior door will not work", goEntry);
            continue;
        }

        if (!existing->ScriptId)
        {
            sObjectMgr->GetGameObjectTemplateStoreForHotfix()[goEntry].ScriptId = doorScriptId;
            ++scripted;
        }
    }

    if (created)
        TC_LOG_INFO("server.loading", ">> Auto-created {} missing door GO templates from ExteriorComponent DB2", created);
    if (scripted)
        TC_LOG_INFO("server.loading", ">> Bound go_housing_door to {} door GO templates", scripted);
}

void HousingMgr::BuildExteriorComponentIndexes()
{
    _hooksByExtComp.clear();
    _exitPointByExtComp.clear();
    _groupByExtComp.clear();
    _extCompsByGroup.clear();
    _childrenByExtComp.clear();
    _rootCompsByWmoDataId.clear();
    _defaultFixtureByTypeWmo.clear();

    // 1. Build hook index: which hooks are parented to each component.
    //    ExteriorComponentHook has IndexField=2 (ID in data) and ParentIndexField=4.
    //    Use the store's range-based iterator which correctly iterates unique entries.
    for (ExteriorComponentHookEntry const* hook : sExteriorComponentHookStore)
    {
        if (!hook)
            continue;
        _hooksByExtComp[hook->ExteriorComponentID].push_back(hook);
    }
    // 1a. Build child index and root-by-WMO index from ExteriorComponent.
    //     ParentComponentID > 0 → color/dye variant of that component.
    //     ParentComponentID == 0 → base variant, and if the Type is a structural root,
    //     it's indexed by HouseExteriorWmoDataID for independent spawning.
    //
    //     Structural root check: look up ExteriorComponentType by comp->Type.
    //     Only types with ParentComponentType == 0 are structural roots (Base=9, Roof=10).
    //     Types like Door(11), Window(12), Chimney(16) have ParentComponentType > 0
    //     and are spawned as hook children, NOT as independent roots.
    //
    //     ExteriorComponent uses ParentIndexField (HouseExteriorWmoDataID).
    //     Use range-based iterator, NOT LookupEntry(i), which maps by parent ID.
    std::unordered_set<uint8> structuralRootTypes;
    for (ExteriorComponentEntry const* comp : sExteriorComponentStore)
    {
        if (!comp)
            continue;

        if (comp->ParentComponentID > 0)
            _childrenByExtComp[static_cast<uint32>(comp->ParentComponentID)].push_back(comp->ID);

        if (comp->ParentComponentID == 0 && comp->ModelFileDataID > 0 && comp->HouseExteriorWmoDataID > 0)
        {
            // Check if this component's Type is a structural root.
            // ExteriorComponentType DB2: Base(9) and Roof(10) have ParentComponentType=0.
            // All fixture types (Door=11, Window=12, etc.) have ParentComponentType > 0.
            // Try DB2 lookup first; fall back to known root types if store is unreliable.
            bool isStructuralRoot = false;
            ExteriorComponentTypeEntry const* typeEntry = sExteriorComponentTypeStore.LookupEntry(comp->Type);
            if (typeEntry)
                isStructuralRoot = (typeEntry->ParentComponentType == 0);
            else
                isStructuralRoot = (comp->Type == 9 || comp->Type == 10); // Base, Roof

            if (isStructuralRoot)
            {
                _rootCompsByWmoDataId[comp->HouseExteriorWmoDataID].push_back(comp->ID);
                structuralRootTypes.insert(comp->Type);
            }
        }
    }

    // 1c. Build group indexes from ExteriorComponentXGroup (for UI fixture panels)
    for (ExteriorComponentXGroupEntry const* xg : sExteriorComponentXGroupStore)
    {
        if (!xg)
            continue;
        uint32 compID = static_cast<uint32>(xg->ExteriorComponentID);
        int32 groupID = xg->ExteriorComponentGroupID;
        _groupByExtComp[compID] = groupID;
        _extCompsByGroup[groupID].push_back(compID);
    }

    // 2. Build fixture resolution index: (componentType, wmoDataID) → default component ID.
    //    For each hook on a parent component, the fixture that goes there is determined by:
    //      - Hook's ExteriorComponentTypeID (e.g., Door=11, Window=12, Chimney=16)
    //      - Parent component's HouseExteriorWmoDataID (e.g., 9=Human, 55=NightElf)
    //    The default fixture is the root component (ParentComponentID==0) with matching
    //    Type and WmoDataID that has Flags & 0x1 (IsDefault).
    //
    //    This replaces the old GroupXHook→Group→XGroup chain which was incorrect —
    //    groups are for UI organization, not fixture resolution.
    for (ExteriorComponentEntry const* comp : sExteriorComponentStore)
    {
        if (!comp || comp->ParentComponentID != 0 || comp->HouseExteriorWmoDataID == 0)
            continue;

        // Key includes size so different house sizes get the right defaults
        uint64 key = (uint64(comp->Type) << 40) | (uint64(comp->HouseExteriorWmoDataID) << 8) | comp->Size;
        bool isDefault = (comp->Flags & 0x1) != 0;

        auto existing = _defaultFixtureByTypeWmo.find(key);
        if (existing == _defaultFixtureByTypeWmo.end())
        {
            // First component for this (type, wmo, size) — insert it
            _defaultFixtureByTypeWmo[key] = comp->ID;
        }
        else if (isDefault)
        {
            // This component is the default — override any non-default already stored
            _defaultFixtureByTypeWmo[key] = comp->ID;
        }
    }

    // 3. Build exit point index
    for (ExteriorComponentExitPointEntry const* exitPt : sExteriorComponentExitPointStore)
    {
        if (!exitPt)
            continue;
        _exitPointByExtComp[exitPt->ExteriorComponentID] = exitPt;
    }

}

std::vector<ExteriorComponentHookEntry const*> const* HousingMgr::GetHooksOnComponent(uint32 extCompID) const
{
    auto itr = _hooksByExtComp.find(extCompID);
    if (itr != _hooksByExtComp.end())
        return &itr->second;

    // DB2 only hangs hooks on base shapes (ParentComponentID == 0); a colour variant of a base or roof shares its
    // shape's hooks. Without this a recoloured base lost its door and windows.
    if (ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(extCompID))
        if (comp->ParentComponentID > 0 && static_cast<uint32>(comp->ParentComponentID) != extCompID)
            return GetHooksOnComponent(static_cast<uint32>(comp->ParentComponentID));
    return nullptr;
}

uint32 HousingMgr::GetDefaultFixtureForType(uint8 componentType, uint32 wmoDataID, uint8 houseSize /*= 0*/) const
{
    // Try exact size match first
    if (houseSize > 0)
    {
        uint64 key = (uint64(componentType) << 40) | (uint64(wmoDataID) << 8) | houseSize;
        auto itr = _defaultFixtureByTypeWmo.find(key);
        if (itr != _defaultFixtureByTypeWmo.end())
            return itr->second;
    }

    // Fallback: scan all sizes for this (type, wmo) — useful when caller doesn't know the size
    for (uint8 sz = 1; sz <= 4; ++sz)
    {
        if (sz == houseSize)
            continue; // already tried
        uint64 key = (uint64(componentType) << 40) | (uint64(wmoDataID) << 8) | sz;
        auto itr = _defaultFixtureByTypeWmo.find(key);
        if (itr != _defaultFixtureByTypeWmo.end())
            return itr->second;
    }

    // Also try size=0 in case any components have Size=0
    uint64 key = (uint64(componentType) << 40) | (uint64(wmoDataID) << 8);
    auto itr = _defaultFixtureByTypeWmo.find(key);
    return itr != _defaultFixtureByTypeWmo.end() ? itr->second : 0;
}

uint32 HousingMgr::GetRacialWmoDataID(uint8 race, uint32 teamId)
{
    switch (race)
    {
        case RACE_NIGHTELF: return 55;  // Woodland
        case RACE_BLOODELF: return 56;  // Engraved
        default:
            return (teamId == HORDE) ? 87 : 9; // Orc / Human
    }
}

bool HousingMgr::IsHouseSizeAvailableForType(uint32 wmoDataID, uint8 houseSize) const
{
    // Exact size only: GetDefaultFixtureForType falls back to any size, which let a Small-only facade stay Medium/Large
    auto hasRoot = [&](uint8 componentType)
    {
        return _defaultFixtureByTypeWmo.contains((uint64(componentType) << 40) | (uint64(wmoDataID) << 8) | houseSize);
    };
    return hasRoot(HOUSING_FIXTURE_TYPE_BASE) && hasRoot(HOUSING_FIXTURE_TYPE_ROOF);
}

uint8 HousingMgr::GetLargestHouseSizeForType(uint32 wmoDataID, uint8 maxSize) const
{
    for (uint8 size = maxSize; size >= HOUSING_FIXTURE_SIZE_SMALL; --size)
        if (IsHouseSizeAvailableForType(wmoDataID, size))
            return size;
    return HOUSING_FIXTURE_SIZE_NONE;
}

bool HousingMgr::IsHouseTypeAllowedInNeighborhood(int32 wmoDataFlags, int32 neighborhoodFaction)
{
    uint32 const factionFlags = uint32(wmoDataFlags) & (HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS | HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS);
    if (!factionFlags || neighborhoodFaction == NEIGHBORHOOD_FACTION_NONE)
        return true;

    return (neighborhoodFaction == NEIGHBORHOOD_FACTION_HORDE && (factionFlags & HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_HORDE_NEIGHBORHOODS))
        || (neighborhoodFaction == NEIGHBORHOOD_FACTION_ALLIANCE && (factionFlags & HOUSE_EXTERIOR_WMO_FLAG_ALLOWED_IN_ALLIANCE_NEIGHBORHOODS));
}

ExteriorComponentExitPointEntry const* HousingMgr::GetExitPoint(uint32 extCompID) const
{
    auto itr = _exitPointByExtComp.find(extCompID);
    return itr != _exitPointByExtComp.end() ? itr->second : nullptr;
}

int32 HousingMgr::GetGroupForComponent(uint32 extCompID) const
{
    auto itr = _groupByExtComp.find(extCompID);
    return itr != _groupByExtComp.end() ? itr->second : 0;
}

std::vector<uint32> const* HousingMgr::GetChildComponents(uint32 parentCompID) const
{
    auto itr = _childrenByExtComp.find(parentCompID);
    return itr != _childrenByExtComp.end() ? &itr->second : nullptr;
}

std::vector<uint32> const* HousingMgr::GetRootComponentsForWmoData(uint32 wmoDataID) const
{
    auto itr = _rootCompsByWmoDataId.find(wmoDataID);
    return itr != _rootCompsByWmoDataId.end() ? &itr->second : nullptr;
}

std::vector<uint32> const* HousingMgr::GetComponentsInGroup(int32 groupID) const
{
    auto itr = _extCompsByGroup.find(groupID);
    return itr != _extCompsByGroup.end() ? &itr->second : nullptr;
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

uint32 HousingMgr::GetDecorCategoryForDecor(uint32 decorId) const
{
    auto itr = _categoryByDecor.find(decorId);
    return itr != _categoryByDecor.end() ? itr->second : 0;
}

bool HousingMgr::IsLightingDecor(uint32 decorId) const
{
    return GetDecorCategoryForDecor(decorId) == HOUSING_DECOR_CATEGORY_LIGHTING;
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

int32 HousingMgr::GetFactionDefaultThemeID(int32 factionRestriction) const
{
    // Returns the BASE theme for DB2 RoomComponentOption lookups.
    // Folk=1 (Alliance), Rugged=2 (Horde). These are the root themes in HouseTheme
    // with ParentThemeID=0. The DB2 option entries are keyed by base theme.
    if (factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE)
        return 1; // Folk
    if (factionRestriction == NEIGHBORHOOD_FACTION_HORDE)
        return 2; // Rugged
    return 1;
}

int32 HousingMgr::GetDefaultSubThemeID(int32 baseThemeID) const
{
    // The sub-theme written to FHousingRoomComponentMesh_C.HouseThemeID for a base theme: its "(neutral)" child in
    // HouseTheme.db2, which is always the lowest child ID (Folk 6, Rugged 8, Bel'ameth 10, Silvermoon 12). A theme
    // without children (Standard 3) stands for itself.
    int32 defaultSubTheme = 0;
    for (auto const& [id, theme] : _houseThemeStore)
        if (theme.ParentThemeID == baseThemeID && (!defaultSubTheme || int32(id) < defaultSubTheme))
            defaultSubTheme = int32(id);
    return defaultSubTheme ? defaultSubTheme : baseThemeID;
}

int32 HousingMgr::GetBaseThemeID(int32 themeID) const
{
    // RoomComponentOption rows are keyed by base theme; a sub-theme resolves through HouseTheme.ParentThemeID.
    auto itr = _houseThemeStore.find(themeID);
    if (itr != _houseThemeStore.end() && itr->second.ParentThemeID != 0)
        return itr->second.ParentThemeID;
    return themeID;
}

RoomComponentOptionEntry const* HousingMgr::FindRoomComponentOption(int32 meshStyleFilterID, int32 houseThemeID) const
{
    // Returns the Type=0 (Cosmetic) option for the given (MSFID, theme).
    uint64 key = (uint64(uint32(meshStyleFilterID)) << 32) | uint32(houseThemeID);
    auto itr = _roomCompOptionIndex.find(key);
    return itr != _roomCompOptionIndex.end() ? itr->second : nullptr;
}

std::vector<RoomComponentOptionEntry const*> HousingMgr::FindAllRoomComponentOptions(int32 meshStyleFilterID, int32 houseThemeID) const
{
    // Returns ALL options for the given (MSFID, theme): Cosmetic, DoorwayWall, Doorway.
    // Alliance sniff shows corners have 2 Cosmetic options (SubType 0+1) and
    // doorway walls have DoorwayWall (Type=1) + Doorway (Type=2) options.
    std::vector<RoomComponentOptionEntry const*> results;
    for (RoomComponentOptionEntry const* entry : sRoomComponentOptionStore)
    {
        if (!entry)
            continue;
        if (entry->MeshStyleFilterID == meshStyleFilterID && (!houseThemeID || entry->HouseThemeID == houseThemeID))
            results.push_back(entry);
    }
    return results;
}

uint32 HousingMgr::GetDefaultVisualRoomEntry() const
{
    // Sniff-verified: both alliance and horde use HouseRoomID=1 ("Square Room Small")
    // as the primary interior room. The faction theme (themeID 1=Folk/Alliance, 2=Rugged/Horde)
    // controls wall/floor textures via RoomComponentOption, not the room shape.
    // Pick the lowest-ID non-base room with UNLOCKED_BY_DEFAULT + visual components.
    uint32 bestId = 0;
    uint32 fallbackId = 0;

    for (auto const& [id, roomData] : _houseRoomStore)
    {
        if (roomData.IsBaseRoom())
            continue;

        auto const* comps = GetRoomComponents(roomData.RoomWmoDataID);
        if (!comps || comps->size() <= 1)
            continue;

        if (roomData.Flags & HOUSING_ROOM_FLAG_UNLOCKED_BY_DEFAULT)
        {
            // Pick lowest ID for determinism (room 1 = Square Room Small, the sniff default)
            if (!bestId || id < bestId)
                bestId = id;
        }
        else if (!fallbackId || id < fallbackId)
        {
            fallbackId = id;
        }
    }

    uint32 result = bestId ? bestId : fallbackId;
    TC_LOG_ERROR("housing", "HousingMgr::GetDefaultVisualRoomEntry: bestId={} fallbackId={} -> returning {}",
        bestId, fallbackId, result);
    return result;
}

// ---------------------------------------------------------------------------
// House-finder per-player ignore list
// (CMSG_HOUSING_SVCS_HOUSE_FINDER_IGNORE_NEIGHBORHOOD, build 12.1.0.69497)
// Lazily loaded from character_housing_ignored_neighborhood, cached, write-through.
// ---------------------------------------------------------------------------
std::unordered_set<ObjectGuid>& HousingMgr::EnsureIgnoredNeighborhoodsLoaded(ObjectGuid playerGuid)
{
    auto itr = _ignoredNeighborhoods.find(playerGuid);
    if (itr != _ignoredNeighborhoods.end())
        return itr->second;

    std::unordered_set<ObjectGuid>& set = _ignoredNeighborhoods[playerGuid];

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOUSING_IGNORED_NEIGHBORHOOD);
    stmt->setUInt64(0, playerGuid.GetCounter());
    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
    {
        do
        {
            uint64 counter = (*result)[0].GetUInt64();
            // Neighborhood GUIDs are HighGuid::Housing subType 4; only the counter is stored.
            // Resolve the live neighborhood to reconstruct the exact full GUID for comparison.
            if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhoodByCounter(counter))
                set.insert(neighborhood->GetGuid());
        } while (result->NextRow());
    }

    return set;
}

bool HousingMgr::IsNeighborhoodIgnored(ObjectGuid playerGuid, ObjectGuid neighborhoodGuid)
{
    std::unordered_set<ObjectGuid> const& set = EnsureIgnoredNeighborhoodsLoaded(playerGuid);
    return set.find(neighborhoodGuid) != set.end();
}

void HousingMgr::AddIgnoredNeighborhood(ObjectGuid playerGuid, ObjectGuid neighborhoodGuid)
{
    std::unordered_set<ObjectGuid>& set = EnsureIgnoredNeighborhoodsLoaded(playerGuid);
    if (!set.insert(neighborhoodGuid).second)
        return; // already ignored

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER_HOUSING_IGNORED_NEIGHBORHOOD);
    stmt->setUInt64(0, playerGuid.GetCounter());
    stmt->setUInt64(1, neighborhoodGuid.GetCounter());
    CharacterDatabase.Execute(stmt);

}

void HousingMgr::RemoveIgnoredNeighborhood(ObjectGuid playerGuid, ObjectGuid neighborhoodGuid)
{
    std::unordered_set<ObjectGuid>& set = EnsureIgnoredNeighborhoodsLoaded(playerGuid);
    if (!set.erase(neighborhoodGuid))
        return;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING_IGNORED_NEIGHBORHOOD);
    stmt->setUInt64(0, playerGuid.GetCounter());
    stmt->setUInt64(1, neighborhoodGuid.GetCounter());
    CharacterDatabase.Execute(stmt);
}
