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
#include "GameObjectData.h"
#include "Group.h"
#include "Guild.h"
#include "Housing.h"
#include "Log.h"
#include "Neighborhood.h"
#include "ObjectMgr.h"
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
    BuildRoomComponentOptionIndex();
    BuildExteriorComponentIndexes();
    BuildRoomComponentTextureIndex();
    DumpExteriorComponentDiagnostics();
    DumpRoomComponentTextureDiagnostics();

    // Initialize global DB ID generators from MAX(id) in character_housing_rooms/decor.
    // Must happen before any Housing objects are loaded to prevent cross-player ID collisions.
    Housing::InitializeDbIdGenerators();

    // Scan for base room entry from DB2 data (look for IsBaseRoom flag)
    for (auto const& [id, roomData] : _houseRoomStore)
    {
        if (roomData.IsBaseRoom())
        {
            _baseRoomEntryId = id;
            TC_LOG_INFO("housing", "HousingMgr::Initialize: Base room entry from DB2 flag: {} ('{}')",
                id, roomData.Name);
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
            TC_LOG_INFO("housing", "HousingMgr::Initialize: Entry hall room entry from DB2: {} ('{}')",
                id, roomData.Name);
            break;
        }
    }
    if (!_entryHallRoomEntryId)
    {
        _entryHallRoomEntryId = _baseRoomEntryId;
        TC_LOG_WARN("housing", "HousingMgr::Initialize: No second BASE_ROOM found for entry hall, "
            "falling back to base room entry {}", _baseRoomEntryId);
    }

    // Room grid spacing (sniff-verified: 15.0f). Log diagnostic with base room WMO bounding box.
    _roomGridSpacing = 15.0f;
    if (_baseRoomEntryId)
    {
        HouseRoomData const* baseRoom = GetHouseRoomData(_baseRoomEntryId);
        if (baseRoom)
        {
            RoomWmoDataEntry const* wmo = baseRoom->RoomWmoDataID
                ? sRoomWmoDataStore.LookupEntry(baseRoom->RoomWmoDataID) : nullptr;
            if (wmo)
            {
                float bbWidth = wmo->BoundingBoxMaxX - wmo->BoundingBoxMinX;
                float bbDepth = wmo->BoundingBoxMaxY - wmo->BoundingBoxMinY;
                TC_LOG_INFO("housing", "HousingMgr::Initialize: Room grid spacing = {:.1f}yd, "
                    "base room WMO bbox = ({:.1f},{:.1f},{:.1f})->({:.1f},{:.1f},{:.1f}), "
                    "width={:.1f} depth={:.1f}",
                    _roomGridSpacing,
                    wmo->BoundingBoxMinX, wmo->BoundingBoxMinY, wmo->BoundingBoxMinZ,
                    wmo->BoundingBoxMaxX, wmo->BoundingBoxMaxY, wmo->BoundingBoxMaxZ,
                    bbWidth, bbDepth);
            }
        }
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
        data.Flags = entry->Flags;
        data.ParentThemeID = entry->ParentThemeID;
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
        data.EntryRotation = entry->EntryRotation;
        data.UiTextureKitID = entry->UiTextureKitID;
        data.Flags = entry->Flags;
    }

    // Build reverse lookup: world MapID -> NeighborhoodMap ID
    for (auto const& [id, data] : _neighborhoodMapStore)
    {
        _worldMapToNeighborhoodMap[data.MapID] = id;
        // NeighborhoodMapFlags (IDA-confirmed): AlliancePurchasable=0x1, HordePurchasable=0x2, CanSystemGenerate=0x4
        TC_LOG_DEBUG("housing", "  NeighborhoodMap ID={} MapID={} EntryRotation={} UiTextureKitID={} Flags=0x{:X} (Alliance={} Horde={} SysGen={})",
            data.ID, data.MapID, data.EntryRotation, data.UiTextureKitID, data.Flags,
            (data.Flags & 0x1) != 0, (data.Flags & 0x2) != 0, (data.Flags & 0x4) != 0);
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

    if (dynamicAdded > 0)
    {
        TC_LOG_ERROR("housing", "HousingMgr::LoadNeighborhoodPlotData: {} cornerstone + {} plot marker GO templates were MISSING from gameobject_template and GameObjects.db2. "
            "Dynamically registered them. Apply sql/housing/world_housing_go_templates.sql to the world DB to fix permanently.",
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
    {
        TC_LOG_ERROR("housing", "HousingMgr::ResolvePlotIndex: neighborhood is null");
        return -1;
    }

    // Only GameObject GUIDs encode a GO entry that can be matched against cornerstone entries.
    // Housing/Neighborhood GUIDs (HighGuid 55) don't have a GO entry — callers sometimes
    // pass these for diagnostic purposes; silently return -1.
    if (cornerstoneGuid.GetHigh() != HighGuid::GameObject)
    {
        TC_LOG_DEBUG("housing", "HousingMgr::ResolvePlotIndex: GUID {} is not a GameObject (HighGuid: {}), skipping",
            cornerstoneGuid.ToString(), static_cast<uint32>(cornerstoneGuid.GetHigh()));
        return -1;
    }

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
        TC_LOG_ERROR("housing", "HousingMgr::ResolvePlotIndex: No plot found for goEntry={} in neighborhoodMapId={} (GUID: {})",
            goEntry, neighborhoodMapId, cornerstoneGuid.ToString());
        return -1;
    }

    TC_LOG_DEBUG("housing", "HousingMgr::ResolvePlotIndex: Resolved GUID {} (entry={}) -> PlotIndex {} (DB2 ID {})",
        cornerstoneGuid.ToString(), goEntry, plotData->PlotIndex, plotData->ID);
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
    // Sniff-observed sets (unique IDs only, 7-8 per faction):
    //   Alliance: 389, 726, 1994, 1435, 9144
    //   Horde:    1700, 81, 10952, 2549, 8910
    // FirstTimeDecorAcquisition sends one packet per UNIQUE decor ID.
    // StartingQuantity determines catalog count, NOT notification count.
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
            result.push_back(id);  // One entry per unique decor ID
    }
    return result;
}

std::vector<std::pair<uint32, int32>> HousingMgr::GetStarterDecorWithQuantities(uint32 teamId) const
{
    // Returns {DecorID, StartingQuantity} pairs for populating the catalog
    static constexpr int32 HOUSE_DECOR_FLAG_FACTION_ALLIANCE = 0x1;
    static constexpr int32 HOUSE_DECOR_FLAG_FACTION_HORDE    = 0x2;
    static constexpr int32 HOUSE_DECOR_FLAG_FACTION_MASK     = 0x3;

    int32 factionBit = (teamId == ALLIANCE) ? HOUSE_DECOR_FLAG_FACTION_ALLIANCE : HOUSE_DECOR_FLAG_FACTION_HORDE;

    std::vector<std::pair<uint32, int32>> result;
    for (auto const& [id, decor] : _houseDecorStore)
    {
        if (decor.StartingQuantity <= 0)
            continue;

        int32 decorFaction = decor.Flags & HOUSE_DECOR_FLAG_FACTION_MASK;
        if (decorFaction == 0 || decorFaction == HOUSE_DECOR_FLAG_FACTION_MASK || (decorFaction & factionBit))
            result.push_back({ id, decor.StartingQuantity });
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
        data.WMOMaterialReference = entry->WMOMaterialReference;
        data.MaterialTextureIndex = entry->MaterialTextureIndex;
        data.HouseThemeID = entry->HouseThemeID;
        data.TextureAFileDataID = entry->TextureAFileDataID;
        data.TextureBFileDataID = entry->TextureBFileDataID;
    }

    // Build decor material index
    for (auto const& [id, mat] : _houseDecorMaterialStore)
        _materialsByTheme[mat.HouseThemeID].push_back(&mat);

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
        data.HouseLevelDataID = entry->HouseLevelDataID;
        data.Field_4 = entry->Field_4;
        data.IconFileDataID = entry->IconFileDataID;
    }

    // Build level reward index
    for (auto const& [id, reward] : _houseLevelRewardInfoStore)
        _rewardsByLevel[reward.HouseLevelDataID].push_back(&reward);

    // HouseLevelRewardInfo DB2 fields verified from runtime data + IDA:
    //   Field_4 = HouseLevelRewardType enum: Value(0) or Object(1)
    //   IconFileDataID = actual FileData icon reference (values: 135769, 4217590, 7252953, 7487068)
    //   DB2 does NOT contain budget type (ExteriorDecor/InteriorDecor/Rooms/Fixtures) or budget values.
    //   Budget capacities come entirely from the hardcoded fallback table below.
    //   Client enum HouseLevelRewardValueType(0-3) is used in Lua UI, not stored in this DB2.
    uint32 budgetWired = 0;

    // Apply fallback budgets for any levels still at 0 (DB2 missing budget rewards)
    static constexpr int32 FallbackInteriorByLevel[] = { 0, 910, 1155, 1450, 1750, 2050 };
    for (auto& [id, levelData] : _houseLevelDataStore)
    {
        uint32 lvl = static_cast<uint32>(std::max<int32>(levelData.Level, 1));
        if (levelData.InteriorDecorPlacementBudget <= 0)
        {
            if (lvl <= 5)
                levelData.InteriorDecorPlacementBudget = FallbackInteriorByLevel[lvl];
            else
                levelData.InteriorDecorPlacementBudget = 2050 + static_cast<int32>((lvl - 5) * 300);
        }
        if (levelData.ExteriorDecorPlacementBudget <= 0)
            levelData.ExteriorDecorPlacementBudget = 200;
        if (levelData.RoomPlacementBudget <= 0)
            levelData.RoomPlacementBudget = 19;
        if (levelData.ExteriorFixtureBudget <= 0)
            levelData.ExteriorFixtureBudget = 1000;
    }

    TC_LOG_INFO("housing", "HousingMgr::LoadHouseLevelRewardInfoData: Loaded {} HouseLevelRewardInfo entries, wired {} budget values from DB2",
        uint32(_houseLevelRewardInfoStore.size()), budgetWired);

    // Log final budget values per level for verification
    for (auto const& [id, levelData] : _houseLevelDataStore)
    {
        TC_LOG_INFO("housing", "  Level {} (ID {}): Interior={} Exterior={} Room={} Fixture={}{}",
            levelData.Level, id,
            levelData.InteriorDecorPlacementBudget, levelData.ExteriorDecorPlacementBudget,
            levelData.RoomPlacementBudget, levelData.ExteriorFixtureBudget,
            budgetWired > 0 ? " (from DB2)" : " (fallback)");
    }
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

    TC_LOG_INFO("housing", "HousingMgr::LoadNeighborhoodInitiativeData: Loaded {} NeighborhoodInitiative entries", uint32(_neighborhoodInitiativeStore.size()));
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

    TC_LOG_DEBUG("housing", "HousingMgr::LoadRoomComponentData: Indexed {} total components ({} doorways) "
        "across {} room types from {} DB2 entries",
        totalCount, doorwayCount, uint32(_roomComponentsByWmoData.size()),
        uint32(sRoomComponentStore.GetNumRows()));

    // Diagnostic: log HouseRoom entries with their component counts
    for (auto const& [roomId, roomData] : _houseRoomStore)
    {
        auto const* comps = GetRoomComponents(roomData.RoomWmoDataID);
        uint32 compCount = comps ? uint32(comps->size()) : 0;

        // Count component types
        uint32 wallCount = 0, floorCount = 0, ceilCount = 0, doorwayCount2 = 0, otherCount = 0;
        if (comps)
        {
            for (auto const& c : *comps)
            {
                switch (c.Type)
                {
                    case HOUSING_ROOM_COMPONENT_WALL: ++wallCount; break;
                    case HOUSING_ROOM_COMPONENT_FLOOR: ++floorCount; break;
                    case HOUSING_ROOM_COMPONENT_CEILING: ++ceilCount; break;
                    case HOUSING_ROOM_COMPONENT_DOORWAY:
                    case HOUSING_ROOM_COMPONENT_DOORWAY_WALL: ++doorwayCount2; break;
                    default: ++otherCount; break;
                }
            }
        }

        TC_LOG_INFO("housing", "  HouseRoom [ID={} '{}' RoomWmoDataID={} Flags=0x{:X}{}] -> {} components "
            "({} wall, {} floor, {} ceiling, {} doorway, {} other)",
            roomId, roomData.Name, roomData.RoomWmoDataID, roomData.Flags,
            roomData.IsBaseRoom() ? " BASE_ROOM" : "",
            compCount, wallCount, floorCount, ceilCount, doorwayCount2, otherCount);
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

    TC_LOG_DEBUG("housing", "HousingMgr::LoadDecorCategoryData: Loaded {} decor categories", uint32(_decorCategoryStore.size()));
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

    TC_LOG_DEBUG("housing", "HousingMgr::LoadDecorSubcategoryData: Loaded {} decor subcategories", uint32(_decorSubcategoryStore.size()));
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

void HousingMgr::BuildRoomComponentOptionIndex()
{
    _roomCompOptionIndex.clear();
    uint32 count = 0;
    for (RoomComponentOptionEntry const* entry : sRoomComponentOptionStore)
    {
        if (!entry)
            continue;
        uint64 key = (uint64(entry->RoomComponentID) << 32) | uint32(entry->HouseThemeID);
        _roomCompOptionIndex[key] = entry;
        ++count;
    }
    TC_LOG_INFO("housing", "HousingMgr::BuildRoomComponentOptionIndex: Indexed {} RoomComponentOption entries", count);
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

    TC_LOG_INFO("housing", "HousingMgr::BuildRoomComponentTextureIndex: "
        "{} option→texture links, {} type→texture fallbacks "
        "(RoomComponentTexture store: {} entries, RoomComponentOptionTexture store: {} entries)",
        uint32(_textureByOptionId.size()), uint32(_textureByComponentType.size()),
        sRoomComponentTextureStore.GetNumRows(), sRoomComponentOptionTextureStore.GetNumRows());
}

void HousingMgr::DumpRoomComponentTextureDiagnostics()
{
    TC_LOG_INFO("housing", "=== RoomComponentTexture Diagnostic Dump ===");
    TC_LOG_INFO("housing", "  RoomComponentTexture store:       {} entries", sRoomComponentTextureStore.GetNumRows());
    TC_LOG_INFO("housing", "  RoomComponentOptionTexture store:  {} entries", sRoomComponentOptionTextureStore.GetNumRows());

    for (RoomComponentTextureEntry const* tex : sRoomComponentTextureStore)
    {
        if (!tex)
            continue;
        TC_LOG_INFO("housing", "  Texture [{}] Name='{}' Type={} FileDataID={} Flags={} UiOrder={} RoomComponentID={}",
            tex->ID,
            SafeStr(tex->Name[sWorld->GetDefaultDbcLocale()]),
            tex->Type, tex->FileDataID, tex->Flags, tex->UiOrder, tex->RoomComponentID);
    }

    for (RoomComponentOptionTextureEntry const* link : sRoomComponentOptionTextureStore)
    {
        if (!link)
            continue;
        TC_LOG_INFO("housing", "  OptionTexture [{}] OptionID={} → TextureID={}",
            link->ID, link->RoomComponentOptionID, link->RoomComponentTextureID);
    }

    // Log the hardcoded values we're replacing and their DB2 equivalents
    TC_LOG_INFO("housing", "  --- Texture ID Resolution ---");
    TC_LOG_INFO("housing", "  Wall  (type=1): DB2={} (was hardcoded 24)",
        _textureByComponentType.contains(1) ? _textureByComponentType[1] : 0);
    TC_LOG_INFO("housing", "  Floor (type=2): DB2={} (was hardcoded 40)",
        _textureByComponentType.contains(2) ? _textureByComponentType[2] : 0);
    TC_LOG_INFO("housing", "  Ceil  (type=3): DB2={} (was hardcoded 54)",
        _textureByComponentType.contains(3) ? _textureByComponentType[3] : 0);
    TC_LOG_INFO("housing", "=== End RoomComponentTexture Dump ===");
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

void HousingMgr::BuildExteriorComponentIndexes()
{
    _hooksByExtComp.clear();
    _extCompByHookId.clear();
    _exitPointByExtComp.clear();
    _groupByExtComp.clear();
    _extCompsByGroup.clear();
    _childrenByExtComp.clear();
    _rootCompsByWmoDataId.clear();

    // 1. Build hook index: which hooks are parented to each component
    //    Note: store iteration only yields a subset of entries due to DB2 ParentIndexField
    //    sparse indexing. Use LookupEntry(i) over GetNumRows() to reach all entries.
    for (uint32 i = 0; i < sExteriorComponentHookStore.GetNumRows(); ++i)
    {
        ExteriorComponentHookEntry const* hook = sExteriorComponentHookStore.LookupEntry(i);
        if (!hook)
            continue;
        _hooksByExtComp[hook->ExteriorComponentID].push_back(hook);
    }

    // 1a. Build child index and root-by-WMO index from ExteriorComponent.
    //     ParentComponentID > 0 → child of that component.
    //     ParentComponentID == 0 → root component, indexed by HouseExteriorWmoDataID.
    //     ExteriorComponent uses ParentIndexField (HouseExteriorWmoDataID), so use
    //     LookupEntry over GetNumRows() to reach all entries.
    for (uint32 i = 0; i < sExteriorComponentStore.GetNumRows(); ++i)
    {
        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(i);
        if (!comp)
            continue;

        if (comp->ParentComponentID > 0)
            _childrenByExtComp[static_cast<uint32>(comp->ParentComponentID)].push_back(comp->ID);

        // Root components (ParentComponentID=0) with a model, indexed by WMO data ID
        if (comp->ParentComponentID == 0 && comp->ModelFileDataID > 0 && comp->HouseExteriorWmoDataID > 0)
            _rootCompsByWmoDataId[comp->HouseExteriorWmoDataID].push_back(comp->ID);
    }

    // 1b. Build group indexes from ExteriorComponentXGroup (needed by step 2)
    for (ExteriorComponentXGroupEntry const* xg : sExteriorComponentXGroupStore)
    {
        if (!xg)
            continue;
        uint32 compID = static_cast<uint32>(xg->ExteriorComponentID);
        int32 groupID = xg->ExteriorComponentGroupID;
        _groupByExtComp[compID] = groupID;
        _extCompsByGroup[groupID].push_back(compID);
    }

    // 2. Build reverse lookup: hookID → default component that should be placed there.
    //    Chain: ExteriorComponentGroupXHook maps (GroupID → HookID), meaning
    //    "this group of components can be installed at this hook."
    //    For each hook, we find which groups link to it, then pick the IsDefault
    //    component from those groups with matching ExteriorComponentTypeID.
    //
    //    NOTE: Hook IDs are NOT globally unique — the ExteriorComponentHook DB2
    //    uses ExteriorComponentID as a ParentIndexField, so multiple parent
    //    components can share the same hook ID. The _extCompByHookId map uses
    //    a composite key (hookID, parentCompID) to disambiguate.

    // Step 2a: Build reverse index from GroupXHook: hookID → list of groupIDs
    std::unordered_map<int32, std::vector<int32>> groupsByHookId; // hookID → [groupID, ...]
    for (ExteriorComponentGroupXHookEntry const* gxh : sExteriorComponentGroupXHookStore)
    {
        if (!gxh)
            continue;
        groupsByHookId[gxh->ExteriorComponentHookID].push_back(gxh->ExteriorComponentGroupID);
    }

    // Step 2b: For each hookID in GroupXHook, look up the hook entry via LookupEntry
    //          (store iteration only yields a subset; LookupEntry reaches all 23k+ entries)
    for (auto const& [hookId, groupList] : groupsByHookId)
    {
        ExteriorComponentHookEntry const* hook = sExteriorComponentHookStore.LookupEntry(hookId);
        if (!hook)
            continue;

        int32 hookType = hook->ExteriorComponentTypeID;
        int32 parentCompID = hook->ExteriorComponentID;
        int64 compositeKey = (int64(hookId) << 32) | uint32(parentCompID);
        ExteriorComponentEntry const* bestComp = nullptr;
        ExteriorComponentEntry const* fallbackComp = nullptr;

        // Find default component from linked groups
        for (int32 groupID : groupList)
        {
            auto compItr = _extCompsByGroup.find(groupID);
            if (compItr == _extCompsByGroup.end())
                continue;

            for (uint32 compID : compItr->second)
            {
                ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(compID);
                if (!comp || comp->Type != hookType)
                    continue;

                if (!fallbackComp)
                    fallbackComp = comp;

                if (comp->Flags & 0x1) // IsDefaultFixture
                {
                    bestComp = comp;
                    break;
                }
            }
            if (bestComp)
                break;
        }

        if (!bestComp)
            bestComp = fallbackComp;

        if (bestComp)
        {
            _extCompByHookId[compositeKey] = bestComp;
            TC_LOG_DEBUG("housing", "HookID {} (parent={}, type={}) → comp {} '{}' (Flags=0x{:X}, group-resolved)",
                hook->ID, parentCompID, hookType, bestComp->ID,
                bestComp->Name[DEFAULT_LOCALE] ? bestComp->Name[DEFAULT_LOCALE] : "",
                bestComp->ParentComponentID, bestComp->Flags);
        }
    }

    // 3. Build exit point index
    for (ExteriorComponentExitPointEntry const* exitPt : sExteriorComponentExitPointStore)
    {
        if (!exitPt)
            continue;
        _exitPointByExtComp[exitPt->ExteriorComponentID] = exitPt;
    }

    TC_LOG_INFO("housing", "HousingMgr::BuildExteriorComponentIndexes: "
        "hooks={} compByHook={} exitPoints={} groups={} compsInGroups={} parentChildren={} wmoRoots={}",
        uint32(_hooksByExtComp.size()), uint32(_extCompByHookId.size()),
        uint32(_exitPointByExtComp.size()), uint32(_groupByExtComp.size()),
        uint32(_extCompsByGroup.size()), uint32(_childrenByExtComp.size()),
        uint32(_rootCompsByWmoDataId.size()));

}

std::vector<ExteriorComponentHookEntry const*> const* HousingMgr::GetHooksOnComponent(uint32 extCompID) const
{
    auto itr = _hooksByExtComp.find(extCompID);
    return itr != _hooksByExtComp.end() ? &itr->second : nullptr;
}

ExteriorComponentEntry const* HousingMgr::GetComponentAtHook(int32 hookID, uint32 parentCompID) const
{
    int64 compositeKey = (int64(hookID) << 32) | uint32(parentCompID);
    auto itr = _extCompByHookId.find(compositeKey);
    return itr != _extCompByHookId.end() ? itr->second : nullptr;
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

void HousingMgr::DumpExteriorComponentDiagnostics()
{
    TC_LOG_INFO("housing", "=== ExteriorComponent Diagnostic Dump ===");
    TC_LOG_INFO("housing", "  ExteriorComponent store:        {} entries", sExteriorComponentStore.GetNumRows());
    TC_LOG_INFO("housing", "  ExteriorComponentHook store:    {} entries", sExteriorComponentHookStore.GetNumRows());
    TC_LOG_INFO("housing", "  ExteriorComponentExitPoint:     {} entries", sExteriorComponentExitPointStore.GetNumRows());
    TC_LOG_INFO("housing", "  ExteriorComponentGroup store:   {} entries", sExteriorComponentGroupStore.GetNumRows());
    TC_LOG_INFO("housing", "  ExteriorComponentGroupXHook:    {} entries", sExteriorComponentGroupXHookStore.GetNumRows());
    TC_LOG_INFO("housing", "  ExteriorComponentType store:    {} entries", sExteriorComponentTypeStore.GetNumRows());
    TC_LOG_INFO("housing", "  ExteriorComponentXGroup store:  {} entries", sExteriorComponentXGroupStore.GetNumRows());

    // Dump known components from both alliance and horde sniff data
    static constexpr uint32 knownCompIDs[] = {
        141, 1505, 3811, 1003, 1436, 1417, 1448, 1452, 976, 980, 2445, 2476, 1011
    };

    TC_LOG_INFO("housing", "  --- Known ExteriorComponents ---");
    for (uint32 compID : knownCompIDs)
    {
        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(compID);
        if (!comp)
        {
            TC_LOG_INFO("housing", "    [{}] NOT FOUND in DB2", compID);
            continue;
        }
        TC_LOG_INFO("housing", "    [{}] Name='{}' ModelFileDataID={} Type={} Size={} Flags={} ParentCompID={} GameObjID={}",
            compID,
            SafeStr(comp->Name[sWorld->GetDefaultDbcLocale()]),
            comp->ModelFileDataID, comp->Type, comp->Size, comp->Flags, comp->ParentComponentID, comp->GameObjectID);
    }

    // Dump hooks parented to known components
    TC_LOG_INFO("housing", "  --- ExteriorComponentHooks parented to known components ---");
    for (ExteriorComponentHookEntry const* hook : sExteriorComponentHookStore)
    {
        if (!hook)
            continue;
        // Check if this hook belongs to a known component
        bool isKnown = false;
        for (uint32 compID : knownCompIDs)
            if (hook->ExteriorComponentID == compID)
            { isKnown = true; break; }

        if (isKnown)
        {
            TC_LOG_INFO("housing", "    Hook [{}] on comp={} pos=({:.2f},{:.2f},{:.2f}) "
                "rot=({:.2f},{:.2f},{:.2f}) typeID={}",
                hook->ID, hook->ExteriorComponentID,
                hook->Position[0], hook->Position[1], hook->Position[2],
                hook->Rotation[0], hook->Rotation[1], hook->Rotation[2],
                hook->ExteriorComponentTypeID);
        }
    }

    // Dump exit points for known components
    TC_LOG_INFO("housing", "  --- ExteriorComponentExitPoints for known components ---");
    for (ExteriorComponentExitPointEntry const* exitPt : sExteriorComponentExitPointStore)
    {
        if (!exitPt)
            continue;
        bool isKnown = false;
        for (uint32 compID : knownCompIDs)
            if (exitPt->ExteriorComponentID == compID)
            { isKnown = true; break; }

        if (isKnown)
        {
            TC_LOG_INFO("housing", "    ExitPoint [{}] on comp={} pos=({:.2f},{:.2f},{:.2f}) "
                "rot=({:.2f},{:.2f},{:.2f})",
                exitPt->ID, exitPt->ExteriorComponentID,
                exitPt->Position[0], exitPt->Position[1], exitPt->Position[2],
                exitPt->Rotation[0], exitPt->Rotation[1], exitPt->Rotation[2]);
        }
    }

    // Dump component→parent relationship (ParentComponentID)
    TC_LOG_INFO("housing", "  --- Component parent relationships ---");
    for (uint32 compID : knownCompIDs)
    {
        ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(compID);
        if (!comp || comp->ParentComponentID <= 0)
            continue;
        TC_LOG_INFO("housing", "    comp {} → parent comp {}", compID, comp->ParentComponentID);
    }

    // Dump ExteriorComponentXGroup mappings
    TC_LOG_INFO("housing", "  --- ExteriorComponentXGroup mappings ---");
    for (ExteriorComponentXGroupEntry const* xg : sExteriorComponentXGroupStore)
    {
        if (!xg)
            continue;
        bool isKnown = false;
        for (uint32 compID : knownCompIDs)
            if (static_cast<uint32>(xg->ExteriorComponentID) == compID)
            { isKnown = true; break; }

        if (isKnown)
        {
            TC_LOG_INFO("housing", "    XGroup [{}] comp={} → group={}",
                xg->ID, xg->ExteriorComponentID, xg->ExteriorComponentGroupID);
        }
    }

    TC_LOG_INFO("housing", "=== End ExteriorComponent Diagnostic Dump ===");
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

int32 HousingMgr::GetFactionDefaultThemeID(int32 factionRestriction) const
{
    // Sniff-verified: Alliance theme=6, Horde theme=2
    if (factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE)
        return 6;
    if (factionRestriction == NEIGHBORHOOD_FACTION_HORDE)
        return 2;
    return 6; // fallback to alliance
}

RoomComponentOptionEntry const* HousingMgr::FindRoomComponentOption(uint32 roomComponentID, int32 houseThemeID) const
{
    uint64 key = (uint64(roomComponentID) << 32) | uint32(houseThemeID);
    auto itr = _roomCompOptionIndex.find(key);
    return itr != _roomCompOptionIndex.end() ? itr->second : nullptr;
}

uint32 HousingMgr::GetDefaultVisualRoomEntry() const
{
    // Sniff-verified: both alliance and horde use HouseRoomID=1 ("Square Room Small")
    // as the primary interior room. The faction theme (themeID 6=Alliance, 2=Horde)
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
