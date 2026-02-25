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

#include "HousingMap.h"
#include <algorithm>
#include <set>
#include "AreaTrigger.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "Housing.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MeshObject.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectGridLoader.h"
#include "ObjectGuid.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RealmList.h"
#include "SocialMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldStateMgr.h"

HousingMap::HousingMap(uint32 id, time_t expiry, uint32 instanceId, Difficulty spawnMode, uint32 neighborhoodId)
    : Map(id, expiry, instanceId, spawnMode), _neighborhoodId(neighborhoodId), _neighborhood(nullptr)
{
    // Prevent the map from being unloaded — housing maps are persistent
    // Map::CanUnload() returns false when m_unloadTimer == 0
    m_unloadTimer = 0;
    HousingMap::InitVisibilityDistance();
}

HousingMap::~HousingMap()
{
    _playerHousings.clear();
}

void HousingMap::InitVisibilityDistance()
{
    // Use instance visibility settings for housing maps
    m_VisibleDistance = sWorld->getFloatConfig(CONFIG_MAX_VISIBILITY_DISTANCE_INSTANCE);
    m_VisibilityNotifyPeriod = sWorld->getIntConfig(CONFIG_VISIBILITY_NOTIFY_PERIOD_INSTANCE);
}

void HousingMap::LoadGridObjects(NGridType* grid, Cell const& cell)
{
    Map::LoadGridObjects(grid, cell);
}

void HousingMap::SpawnPlotGameObjects()
{
    if (!_neighborhood)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: _neighborhood is NULL for map {} instanceId {} neighborhoodId {}",
            GetId(), GetInstanceId(), _neighborhoodId);
        return;
    }

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: map={} instanceId={} neighborhoodMapId={} plotCount={}",
        GetId(), GetInstanceId(), neighborhoodMapId, uint32(plots.size()));

    if (plots.empty())
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: NO plots found for neighborhoodMapId={} (neighborhood='{}') - check DB2 NeighborhoodPlot data",
            neighborhoodMapId, _neighborhood->GetName());
        return;
    }

    uint32 goCount = 0;
    uint32 noEntryCount = 0;

    for (NeighborhoodPlotData const* plot : plots)
    {
        float x = plot->CornerstonePosition[0];
        float y = plot->CornerstonePosition[1];
        float z = plot->CornerstonePosition[2];

        // Ensure the grid at this position is loaded so we can add GOs
        LoadGrid(x, y);

        // Retail always uses CornerstoneGameObjectID (457142) for ALL plots.
        // Ownership state via GOState: 0 (ACTIVE) = Owned/Claimed, 1 (READY) = ForSale sign.
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
        uint32 goEntry = static_cast<uint32>(plot->CornerstoneGameObjectID);
        bool isOwned = plotInfo && !plotInfo->OwnerGuid.IsEmpty();

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} at ({:.1f}, {:.1f}, {:.1f}) -> goEntry={} (Cornerstone={}, owned={})",
            plot->PlotIndex, x, y, z, goEntry, plot->CornerstoneGameObjectID,
            isOwned ? "yes" : "no");

        if (!goEntry)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} has CornerstoneGameObjectID=0 - skipping",
                plot->PlotIndex);
            ++noEntryCount;
            continue;
        }

        // Build rotation from the stored euler angles
        float rotZ = plot->CornerstoneRotation[2];
        QuaternionData rot = QuaternionData::fromEulerAnglesZYX(rotZ, plot->CornerstoneRotation[1], plot->CornerstoneRotation[0]);

        // GOState 0 (ACTIVE) = Owned/Claimed cornerstone, GOState 1 (READY) = ForSale sign
        GOState plotState = isOwned ? GO_STATE_ACTIVE : GO_STATE_READY;

        Position pos(x, y, z, rotZ);
        GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, plotState);
        if (!go)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create GO entry {} at ({}, {}, {}) for plot {} in neighborhood '{}'",
                goEntry, x, y, z, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // Retail sniff: all Cornerstone GOs have Flags=32 (GO_FLAG_NODESPAWN)
        go->SetFlag(GO_FLAG_NODESPAWN);

        // Housing objects are dynamically spawned (no DB spawn record), so they have no
        // phase_area association. Explicitly mark them as universally visible so they're
        // seen by players regardless of what phases the player has from area-based phasing.
        PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), 0, 0, 0);

        // Populate the FJamHousingCornerstone_C entity fragment so the client
        // knows this is a Cornerstone and can render the "For Sale" / owned UI
        go->InitHousingCornerstoneData(plot->Cost, static_cast<int32>(plot->PlotIndex));

        if (!AddToMap(go))
        {
            delete go;
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to add GO entry {} to map for plot {} in neighborhood '{}'",
                goEntry, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // Track the plot GO for later swap (purchase/eviction)
        _plotGameObjects[static_cast<uint8>(plot->PlotIndex)] = go->GetGUID();

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} GO entry={} displayId={} type={} name='{}' guid={}",
            plot->PlotIndex, goEntry, go->GetGOInfo()->displayId, go->GetGOInfo()->type,
            go->GetGOInfo()->name, go->GetGUID().ToString());

        ++goCount;

        // Spawn a plot AreaTrigger at the HousePosition for plot enter/exit detection
        uint8 plotIndex = static_cast<uint8>(plot->PlotIndex);
        if (_plotAreaTriggers.find(plotIndex) == _plotAreaTriggers.end())
        {
            AreaTriggerCreatePropertiesId atId = { .Id = 37358, .IsCustom = false };
            Position atPos(plot->HousePosition[0], plot->HousePosition[1], plot->HousePosition[2]);

            // Ensure the grid at the AT position is loaded too
            LoadGrid(atPos.GetPositionX(), atPos.GetPositionY());

            AreaTrigger* at = AreaTrigger::CreateStaticAreaTrigger(atId, this, atPos);
            if (at)
            {
                // Mark AT as universally visible (same rationale as cornerstones above)
                PhasingHandler::InitDbPhaseShift(at->GetPhaseShift(), 0, 0, 0);

                ObjectGuid ownerGuid;
                ObjectGuid houseGuid;
                ObjectGuid ownerBnetGuid;
                if (plotInfo)
                {
                    ownerGuid = plotInfo->OwnerGuid;
                    houseGuid = plotInfo->HouseGuid;
                    ownerBnetGuid = plotInfo->OwnerBnetGuid;
                }

                at->InitHousingPlotData(plotIndex, ownerGuid, houseGuid, ownerBnetGuid);
                _plotAreaTriggers[plotIndex] = at->GetGUID();
                _neighborhood->SetPlotAreaTriggerGuid(plotIndex, at->GetGUID());

                TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Spawned plot AT for plot {} at ({}, {}, {}) guid={} in neighborhood '{}'",
                    plotIndex, atPos.GetPositionX(), atPos.GetPositionY(), atPos.GetPositionZ(), at->GetGUID().ToString(), _neighborhood->GetName());
            }
            else
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create plot AT (entry 37358) for plot {} in neighborhood '{}'",
                    plotIndex, _neighborhood->GetName());
            }
        }
    }

    // Set per-plot WorldState values from DB2 so the client can render plot status on the map
    // WorldState values use HousingPlotOwnerType: 0=None, 1=Stranger, 2=Friend, 3=Self
    // The map-global value is set to STRANGER (1) for occupied plots as the default.
    // When a specific player enters the map, SendPerPlayerPlotWorldStates() sends
    // personalized corrections (SELF/FRIEND) based on the player's relationship to each plot owner.
    // IMPORTANT: Must use Map::SetWorldStateValue() (not sWorldStateMgr->SetValue()) because:
    // 1. The same WorldState IDs are shared across maps (2735/2736) - each instance needs its own values
    // 2. These IDs have no template in world_state SQL table, so sWorldStateMgr stores them
    //    realm-wide which is wrong for instanced housing maps
    // 3. Map-scoped values are included in SMSG_INIT_WORLD_STATES via FillInitialWorldStates()
    uint32 wsSetCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->WorldState != 0)
        {
            Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
            bool isOccupied = plotInfo && !plotInfo->OwnerGuid.IsEmpty();

            // Default: NONE (0) for unoccupied, STRANGER (1) for occupied
            // Per-player corrections (SELF/FRIEND) are sent in SendPerPlayerPlotWorldStates()
            int32 wsValue = isOccupied ? HOUSING_PLOT_OWNER_STRANGER : HOUSING_PLOT_OWNER_NONE;
            SetWorldStateValue(plot->WorldState, wsValue, false);
            ++wsSetCount;
        }
    }

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Spawned {} GOs, {} ATs, set {} WorldStates for {} plots in neighborhood '{}' (noEntry={})",
        goCount, uint32(_plotAreaTriggers.size()), wsSetCount, uint32(plots.size()), _neighborhood->GetName(), noEntryCount);

    // Spawn house structure GOs for owned plots
    uint32 houseCount = 0;
    uint32 houseSuccessCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIdx);
        if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
            continue;

        TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} is owned by {} - attempting house spawn (HousePos: {:.1f}, {:.1f}, {:.1f})",
            plotIdx, plotInfo->OwnerGuid.ToString(),
            plot->HousePosition[0], plot->HousePosition[1], plot->HousePosition[2]);

        // Try to get persisted position and fixture data from the player's Housing object (if online)
        Housing* housing = GetHousingForPlayer(plotInfo->OwnerGuid);
        int32 exteriorComponentID = 141; // Default: Stucco Base
        int32 houseExteriorWmoDataID = 9; // Default: Human theme
        if (housing)
        {
            uint32 coreComponent = housing->GetCoreExteriorComponentID();
            if (coreComponent)
                exteriorComponentID = static_cast<int32>(coreComponent);
            uint32 houseType = housing->GetHouseType();
            if (houseType)
                houseExteriorWmoDataID = static_cast<int32>(houseType);
            TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} using persisted fixture data: ExteriorComponentID={}, WmoDataID={}",
                plotIdx, exteriorComponentID, houseExteriorWmoDataID);
        }

        GameObject* houseGo = nullptr;
        if (housing && housing->HasCustomPosition())
        {
            Position customPos = housing->GetHousePosition();
            houseGo = SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID);
        }
        else
        {
            houseGo = SpawnHouseForPlot(plotIdx, nullptr, exteriorComponentID, houseExteriorWmoDataID);
        }
        ++houseCount;
        if (houseGo)
            ++houseSuccessCount;
        else
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: FAILED to spawn house for plot {} owned by {}",
                plotIdx, plotInfo->OwnerGuid.ToString());

        // Spawn decor GOs if the player's Housing data is loaded
        if (housing)
            SpawnAllDecorForPlot(plotIdx, housing);
    }

    TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: House spawn results: {}/{} successful for neighborhood '{}'",
        houseSuccessCount, houseCount, _neighborhood->GetName());
}

void HousingMap::LockPlotGrids()
{
    if (!_neighborhood)
        return;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
    std::set<std::pair<uint32, uint32>> lockedGrids;

    for (NeighborhoodPlotData const* plot : plots)
    {
        // Lock grid for cornerstone position
        GridCoord cornerstoneGrid = Trinity::ComputeGridCoord(plot->CornerstonePosition[0], plot->CornerstonePosition[1]);
        if (lockedGrids.insert({ cornerstoneGrid.x_coord, cornerstoneGrid.y_coord }).second)
            GridMarkNoUnload(cornerstoneGrid.x_coord, cornerstoneGrid.y_coord);

        // Lock grid for house position (may be a different grid)
        GridCoord houseGrid = Trinity::ComputeGridCoord(plot->HousePosition[0], plot->HousePosition[1]);
        if (lockedGrids.insert({ houseGrid.x_coord, houseGrid.y_coord }).second)
            GridMarkNoUnload(houseGrid.x_coord, houseGrid.y_coord);
    }

    TC_LOG_DEBUG("housing", "HousingMap::LockPlotGrids: Locked {} grids for {} plots in neighborhood '{}'",
        lockedGrids.size(), plots.size(), _neighborhood->GetName());
}

AreaTrigger* HousingMap::GetPlotAreaTrigger(uint8 plotIndex)
{
    auto itr = _plotAreaTriggers.find(plotIndex);
    if (itr == _plotAreaTriggers.end())
        return nullptr;

    return GetAreaTrigger(itr->second);
}

GameObject* HousingMap::GetPlotGameObject(uint8 plotIndex)
{
    auto itr = _plotGameObjects.find(plotIndex);
    if (itr == _plotGameObjects.end())
        return nullptr;

    return GetGameObject(itr->second);
}

void HousingMap::SetPlotOwnershipState(uint8 plotIndex, bool owned)
{
    if (!_neighborhood)
        return;

    // Toggle GOState on the existing Cornerstone GO.
    // GOState 0 (ACTIVE) = Owned/Claimed cornerstone, GOState 1 (READY) = ForSale sign
    GOState newState = owned ? GO_STATE_ACTIVE : GO_STATE_READY;

    auto itr = _plotGameObjects.find(plotIndex);
    if (itr != _plotGameObjects.end())
    {
        if (GameObject* go = GetGameObject(itr->second))
        {
            go->SetGoState(newState);

            TC_LOG_DEBUG("housing", "HousingMap::SetPlotOwnershipState: Plot {} GOState -> {} ({}) in neighborhood '{}'",
                plotIndex, uint32(newState), owned ? "owned" : "for-sale", _neighborhood->GetName());
        }
        else
        {
            TC_LOG_ERROR("housing", "HousingMap::SetPlotOwnershipState: Plot {} GO guid {} not found on map in neighborhood '{}'",
                plotIndex, itr->second.ToString(), _neighborhood->GetName());
        }
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::SetPlotOwnershipState: Plot {} has no tracked GO in neighborhood '{}'",
            plotIndex, _neighborhood->GetName());
    }

    // Update the per-plot WorldState using HousingPlotOwnerType values
    // Default map-global: STRANGER (1) for occupied, NONE (0) for unoccupied
    // Then send per-player corrections to all online players on this map
    // Must use Map::SetWorldStateValue (see SpawnPlotGameObjects comment for rationale)
    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
    for (NeighborhoodPlotData const* plotData : plots)
    {
        if (plotData->PlotIndex == static_cast<int32>(plotIndex))
        {
            if (plotData->WorldState != 0)
            {
                int32 wsValue = owned ? HOUSING_PLOT_OWNER_STRANGER : HOUSING_PLOT_OWNER_NONE;
                SetWorldStateValue(plotData->WorldState, wsValue, false);

                // Send personalized corrections to each player on the map
                // (the map-global value only shows STRANGER; each player needs SELF/FRIEND override)
                for (MapReference const& ref : GetPlayers())
                {
                    if (Player* mapPlayer = ref.GetSource())
                    {
                        HousingPlotOwnerType ownerType = GetPlotOwnerTypeForPlayer(mapPlayer, plotIndex);
                        mapPlayer->SendUpdateWorldState(plotData->WorldState, static_cast<uint32>(ownerType), false);
                    }
                }
            }
            break;
        }
    }
}

Housing* HousingMap::GetHousingForPlayer(ObjectGuid playerGuid) const
{
    auto itr = _playerHousings.find(playerGuid);
    if (itr != _playerHousings.end())
        return itr->second;

    return nullptr;
}

void HousingMap::LoadNeighborhoodData()
{
    ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ sRealmList->GetCurrentRealmId().Realm, /*arg2*/ 0, static_cast<uint64>(_neighborhoodId));
    _neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);

    if (!_neighborhood)
        TC_LOG_ERROR("housing", "HousingMap::LoadNeighborhoodData: Failed to load neighborhood {} for map {} instanceId {}",
            _neighborhoodId, GetId(), GetInstanceId());
    else
        TC_LOG_DEBUG("housing", "HousingMap::LoadNeighborhoodData: Loaded neighborhood '{}' (id: {}) for map {} instanceId {}",
            _neighborhood->GetName(), _neighborhoodId, GetId(), GetInstanceId());
}

bool HousingMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    if (!_neighborhood)
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: No neighborhood loaded for map {} instanceId {}",
            GetId(), GetInstanceId());
        return false;
    }

    // Do NOT auto-add the player as a neighborhood member here.
    // Membership is granted when the player buys a plot or is invited.
    // Auto-adding causes the client to resolve neighborhoodOwnerType as
    // Self instead of None, which prevents the "For Sale" Cornerstone UI.

    // Track player housing if they own a house in this neighborhood.
    // First try exact GUID match, then fall back to checking all housings
    // (handles legacy data where neighborhood GUID counter was from client's DB2 ID
    // instead of the server's canonical counter).
    TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Looking up housing for player {} in neighborhood '{}' (guid={})",
        player->GetGUID().ToString(), _neighborhood->GetName(), _neighborhood->GetGuid().ToString());

    Housing* housing = player->GetHousingForNeighborhood(_neighborhood->GetGuid());
    if (!housing)
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: No housing found via GetHousingForNeighborhood. Checking fallback (allHousings count: {})",
            uint32(player->GetAllHousings().size()));

        // Fallback: check if any of the player's housings has a plot in this neighborhood
        for (Housing const* h : player->GetAllHousings())
        {
            TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Fallback check: housing plotIndex={} neighborhoodGuid={} houseGuid={}",
                h ? h->GetPlotIndex() : 255,
                h ? h->GetNeighborhoodGuid().ToString() : "null",
                h ? h->GetHouseGuid().ToString() : "null");

            if (h && _neighborhood->GetPlotInfo(h->GetPlotIndex()))
            {
                housing = const_cast<Housing*>(h);
                TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Fixed neighborhood GUID mismatch for player {} (stored={}, canonical={})",
                    player->GetGUID().ToString(), h->GetNeighborhoodGuid().ToString(), _neighborhood->GetGuid().ToString());
                // Fix the stored GUID so future lookups work
                housing->SetNeighborhoodGuid(_neighborhood->GetGuid());
                break;
            }
        }
    }

    if (housing)
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Player {} has housing: plotIndex={} houseType={} houseGuid={}",
            player->GetGUID().ToString(), housing->GetPlotIndex(), housing->GetHouseType(), housing->GetHouseGuid().ToString());

        AddPlayerHousing(player->GetGUID(), housing);

        // Ensure the neighborhood PlotInfo has the HouseGuid (may be missing after server restart
        // since LoadFromDB only populates it if character_housing row exists)
        uint8 plotIdx = housing->GetPlotIndex();
        if (!housing->GetHouseGuid().IsEmpty())
        {
            _neighborhood->UpdatePlotHouseInfo(plotIdx, housing->GetHouseGuid(),
                player->GetSession() ? player->GetSession()->GetBattlenetAccountGUID() : ObjectGuid::Empty);
        }

        // Update PlayerMirrorHouse.MapID so the client knows this house is on the current map.
        // Without this, MapID stays at 0 (set during login) and the client rejects
        // edit mode with HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT (error 1 = first non-success code).
        player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(GetId()));

        // Spawn house GO if not already present (handles offline → online transition)
        bool alreadySpawned = _houseGameObjects.find(plotIdx) != _houseGameObjects.end();
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: House GO for plot {}: alreadySpawned={} _houseGameObjects.size={}",
            plotIdx, alreadySpawned, uint32(_houseGameObjects.size()));

        if (!alreadySpawned)
        {
            // Read persisted fixture data from Housing
            int32 exteriorComponentID = 141; // Default: Stucco Base
            int32 houseExteriorWmoDataID = 9; // Default: Human theme
            uint32 coreComponent = housing->GetCoreExteriorComponentID();
            if (coreComponent)
                exteriorComponentID = static_cast<int32>(coreComponent);
            uint32 houseType = housing->GetHouseType();
            if (houseType)
                houseExteriorWmoDataID = static_cast<int32>(houseType);

            TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Plot {} spawning house with ExteriorComponentID={}, WmoDataID={}",
                plotIdx, exteriorComponentID, houseExteriorWmoDataID);

            GameObject* go = nullptr;
            if (housing->HasCustomPosition())
            {
                Position customPos = housing->GetHousePosition();
                go = SpawnHouseForPlot(plotIdx, &customPos, exteriorComponentID, houseExteriorWmoDataID);
            }
            else
                go = SpawnHouseForPlot(plotIdx, nullptr, exteriorComponentID, houseExteriorWmoDataID);

            TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: SpawnHouseForPlot result for plot {}: {}",
                plotIdx, go ? go->GetGUID().ToString() : "FAILED");
        }
        else
        {
            // House GO was spawned during map init (before HouseGuid was available).
            // Apply fixture data and spawn MeshObject if not yet done.
            bool hasMeshObjects = _meshObjects.find(plotIdx) != _meshObjects.end() && !_meshObjects[plotIdx].empty();
            if (!hasMeshObjects && !housing->GetHouseGuid().IsEmpty())
            {
                // Get the house GO position for the MeshObject
                if (GameObject* houseGo = GetHouseGameObject(plotIdx))
                {
                    // NOTE: Do NOT call InitHousingFixtureData on the GO.
                    // In retail, only MeshObjects carry FHousingFixture_C — attaching it to a GO
                    // causes a client crash at +0x64 (GO entity factory doesn't allocate a housing
                    // fixture component, so the GUID resolver returns null and dereferences it).

                    Position pos = houseGo->GetPosition();
                    QuaternionData rot = houseGo->GetLocalRotation();
                    SpawnFullHouseMeshObjects(plotIdx, pos, rot,
                        housing->GetHouseGuid(), 141, 9);

                    TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Late-spawned MeshObjects for plot {} (house GO {} already existed)",
                        plotIdx, houseGo->GetGUID().ToString());
                }
            }
        }

        // Spawn decor GOs if not already spawned for this plot
        SpawnAllDecorForPlot(plotIdx, housing);
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Player {} has NO housing in this neighborhood (no house will spawn)",
            player->GetGUID().ToString());
    }

    if (!Map::AddPlayerToMap(player, initPlayer))
        return false;

    // Send neighborhood context so the client can call SetViewingNeighborhood()
    // and enable Cornerstone purchase UI interaction
    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfo;
    if (housing)
    {
        // Sniff-verified: SecondaryOwnerGuid=NeighborhoodGUID (context for edit mode), PlotGuid=PlotGUID
        houseInfo.HouseInfo.OwnerGuid = housing->GetHouseGuid();
        houseInfo.HouseInfo.SecondaryOwnerGuid = housing->GetNeighborhoodGuid();
        houseInfo.HouseInfo.PlotGuid = housing->GetPlotGuid();
        houseInfo.HouseInfo.Flags = housing->GetPlotIndex();
        houseInfo.HouseInfo.HouseTypeId = 32;
        houseInfo.HouseInfo.StatusFlags = 0;
    }
    else if (_neighborhood)
    {
        // No house — provide neighborhood GUID in SecondaryOwnerGuid field for context
        houseInfo.HouseInfo.SecondaryOwnerGuid = _neighborhood->GetGuid();
    }
    houseInfo.ResponseFlags = 0;
    player->SendDirectMessage(houseInfo.Write());

    // Proactively send ENTER_PLOT for the player's own plot so the client
    // has plot context immediately (the AT-based ENTER_PLOT may fire later
    // when the player physically moves into AT radius, but sniff shows
    // retail sends this early in the map-enter sequence).
    if (housing)
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto atItr = _plotAreaTriggers.find(plotIndex);
        if (atItr != _plotAreaTriggers.end())
        {
            WorldPackets::Neighborhood::NeighborhoodPlayerEnterPlot enterPlot;
            enterPlot.PlotAreaTriggerGuid = atItr->second;
            player->SendDirectMessage(enterPlot.Write());

            TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Sent proactive ENTER_PLOT for plot {} AT {} to player {}",
                plotIndex, atItr->second.ToString(), player->GetGUID().ToString());
        }
        else
        {
            TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: No AT tracked for plot {} - ENTER_PLOT NOT sent (player {})",
                plotIndex, player->GetGUID().ToString());
        }
    }

    // Send personalized per-plot WorldState values for this specific player.
    // The init world states (sent during Map::AddPlayerToMap) use map-global defaults
    // (STRANGER for occupied, NONE for unoccupied). This corrects them to SELF/FRIEND
    // based on the player's relationship to each plot owner.
    SendPerPlayerPlotWorldStates(player);

    TC_LOG_ERROR("housing", "HousingMap::AddPlayerToMap: Sent neighborhood context to player {} (neighborhood='{}', hasHouse={}, plotATs={})",
        player->GetGUID().ToString(), _neighborhood->GetName(), housing ? "yes" : "no", uint32(_plotAreaTriggers.size()));

    return true;
}

void HousingMap::RemovePlayerFromMap(Player* player, bool remove)
{
    RemovePlayerHousing(player->GetGUID());

    TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerFromMap: Player {} leaving housing map {} instanceId {}",
        player->GetGUID().ToString(), GetId(), GetInstanceId());

    Map::RemovePlayerFromMap(player, remove);
}

HousingPlotOwnerType HousingMap::GetPlotOwnerTypeForPlayer(Player const* player, uint8 plotIndex) const
{
    if (!_neighborhood || !player)
        return HOUSING_PLOT_OWNER_NONE;

    Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIndex);
    if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
        return HOUSING_PLOT_OWNER_NONE;

    // Check if the player owns this plot
    if (plotInfo->OwnerGuid == player->GetGUID())
        return HOUSING_PLOT_OWNER_SELF;

    // Check if it's an alt on the same BNet account (same person, different character)
    if (!plotInfo->OwnerBnetGuid.IsEmpty() && player->GetSession())
    {
        if (plotInfo->OwnerBnetGuid == player->GetSession()->GetBattlenetAccountGUID())
            return HOUSING_PLOT_OWNER_SELF;
    }

    // Check character-level friendship
    if (PlayerSocial* social = player->GetSocial())
    {
        if (social->HasFriend(plotInfo->OwnerGuid))
            return HOUSING_PLOT_OWNER_FRIEND;
    }

    return HOUSING_PLOT_OWNER_STRANGER;
}

void HousingMap::SendPerPlayerPlotWorldStates(Player* player)
{
    if (!_neighborhood || !player)
        return;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    uint32 correctionCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->WorldState == 0)
            continue;

        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        HousingPlotOwnerType ownerType = GetPlotOwnerTypeForPlayer(player, plotIdx);
        player->SendUpdateWorldState(plot->WorldState, static_cast<uint32>(ownerType), false);

        if (ownerType != HOUSING_PLOT_OWNER_NONE && ownerType != HOUSING_PLOT_OWNER_STRANGER)
            ++correctionCount;
    }

    if (correctionCount > 0)
    {
        TC_LOG_DEBUG("housing", "HousingMap::SendPerPlayerPlotWorldStates: Sent {} personalized WorldState corrections to player {} (SELF/FRIEND)",
            correctionCount, player->GetGUID().ToString());
    }
}

void HousingMap::AddPlayerHousing(ObjectGuid playerGuid, Housing* housing)
{
    if (!housing)
    {
        TC_LOG_ERROR("housing", "HousingMap::AddPlayerHousing: Attempted to add null housing for player {} on map {} instanceId {}",
            playerGuid.ToString(), GetId(), GetInstanceId());
        return;
    }

    _playerHousings[playerGuid] = housing;

    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerHousing: Added housing for player {} on map {} instanceId {} (total: {})",
        playerGuid.ToString(), GetId(), GetInstanceId(), static_cast<uint32>(_playerHousings.size()));
}

void HousingMap::RemovePlayerHousing(ObjectGuid playerGuid)
{
    auto itr = _playerHousings.find(playerGuid);
    if (itr != _playerHousings.end())
    {
        _playerHousings.erase(itr);

        TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerHousing: Removed housing for player {} on map {} instanceId {} (remaining: {})",
            playerGuid.ToString(), GetId(), GetInstanceId(), static_cast<uint32>(_playerHousings.size()));
    }
    else
    {
        TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerHousing: No housing found for player {} on map {} instanceId {}",
            playerGuid.ToString(), GetId(), GetInstanceId());
    }
}

// ============================================================
// House Structure GO Management
// ============================================================

GameObject* HousingMap::SpawnHouseForPlot(uint8 plotIndex, Position const* customPos /*= nullptr*/,
    int32 exteriorComponentID /*= 141*/, int32 houseExteriorWmoDataID /*= 9*/)
{
    if (!_neighborhood)
        return nullptr;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    NeighborhoodPlotData const* targetPlot = nullptr;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (static_cast<uint8>(plot->PlotIndex) == plotIndex)
        {
            targetPlot = plot;
            break;
        }
    }

    if (!targetPlot)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: No plot data for plotIndex {} in neighborhood '{}'",
            plotIndex, _neighborhood->GetName());
        return nullptr;
    }

    // Determine position: use customPos (persisted player position) or DB2 defaults
    float x, y, z, facing;
    if (customPos)
    {
        x = customPos->GetPositionX();
        y = customPos->GetPositionY();
        z = customPos->GetPositionZ();
        facing = customPos->GetOrientation();
    }
    else
    {
        x = targetPlot->HousePosition[0];
        y = targetPlot->HousePosition[1];
        z = targetPlot->HousePosition[2];
        facing = targetPlot->HouseRotation[2]; // Z euler angle as facing
    }

    LoadGrid(x, y);

    // Build rotation quaternion from euler angles
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(facing,
        customPos ? 0.0f : targetPlot->HouseRotation[1],
        customPos ? 0.0f : targetPlot->HouseRotation[0]);

    // DO NOT spawn GO 574432 (Housing - Generic - Ground WMO) here.
    // The gameobject table already has 23 properly-rotated 574432 spawns (one per plot),
    // loaded by Map::LoadGridObjects. Dynamically spawning another one creates a duplicate
    // with identity rotation that renders as a brown rectangle on the floor.

    Position pos(x, y, z, facing);
    Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIndex);

    TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: plot={} pos=({:.2f}, {:.2f}, {:.2f}) facing={:.3f} rot=({:.3f}, {:.3f}, {:.3f}, {:.3f}) hasPlotInfo={} hasHouseGuid={}",
        plotIndex, x, y, z, facing, rot.x, rot.y, rot.z, rot.w,
        plotInfo != nullptr, plotInfo && !plotInfo->HouseGuid.IsEmpty());

    // Spawn all house MeshObjects (sniff-verified: 10 structural pieces for Stucco Small alliance house)
    // Pieces have a parent-child hierarchy: base piece (0) and door piece (1) are roots,
    // other pieces attach to them with local-space positions/rotations.
    if (plotInfo && !plotInfo->HouseGuid.IsEmpty())
    {
        SpawnFullHouseMeshObjects(plotIndex, pos, rot, plotInfo->HouseGuid,
            exteriorComponentID, houseExteriorWmoDataID);
    }

    // Spawn the front door GO (entry 602702, type Goober, displayId 116971)
    // Sniff confirms: door is a separate interactive GO, NOT a MeshObject.
    // It is spawned at the house position with the house facing.
    GameObject* doorGo = nullptr;
    uint32 doorEntry = 602702;
    GameObjectTemplate const* doorTemplate = sObjectMgr->GetGameObjectTemplate(doorEntry);
    if (doorTemplate)
    {
        doorGo = GameObject::CreateGameObject(doorEntry, this, pos, rot, 255, GO_STATE_ACTIVE);
        if (doorGo)
        {
            doorGo->SetFlag(GO_FLAG_NODESPAWN);
            PhasingHandler::InitDbPhaseShift(doorGo->GetPhaseShift(), 0, 0, 0);

            if (AddToMap(doorGo))
            {
                _houseGameObjects[plotIndex] = doorGo->GetGUID();
                TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Door GO spawned - entry={} guid={} displayId={} at ({:.1f}, {:.1f}, {:.1f}) for plot {}",
                    doorEntry, doorGo->GetGUID().ToString(), doorGo->GetDisplayId(), x, y, z, plotIndex);
            }
            else
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Door GO AddToMap FAILED for plot {}", plotIndex);
                delete doorGo;
                doorGo = nullptr;
            }
        }
        else
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: CreateGameObject FAILED for door entry {} at plot {}", doorEntry, plotIndex);
        }
    }
    else
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Door GO template {} NOT FOUND - door will not spawn for plot {}", doorEntry, plotIndex);
    }

    return doorGo;
}

MeshObject* HousingMap::SpawnHouseMeshObject(uint8 plotIndex, int32 fileDataID, bool isWMO,
    Position const& pos, QuaternionData const& rot, float scale,
    ObjectGuid houseGuid, int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    uint8 exteriorComponentType /*= 9*/, uint8 houseSize /*= 2*/, int32 exteriorComponentHookID /*= -1*/,
    ObjectGuid attachParent /*= ObjectGuid::Empty*/, uint8 attachFlags /*= 0*/,
    Position const* worldPos /*= nullptr*/)
{
    // For child pieces, worldPos contains the parent's world position for grid placement.
    // Use it for LoadGrid so the grid cell near the house is loaded (not the local-space origin).
    if (worldPos)
        LoadGrid(worldPos->GetPositionX(), worldPos->GetPositionY());
    else
        LoadGrid(pos.GetPositionX(), pos.GetPositionY());

    MeshObject* mesh = MeshObject::CreateMeshObject(this, pos, rot, scale, fileDataID, isWMO,
        attachParent, attachFlags, worldPos);
    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseMeshObject: CreateMeshObject failed for plot {} fileDataID {}",
            plotIndex, fileDataID);
        return nullptr;
    }

    // Set up all entity fragments BEFORE AddToMap (create packet is sent during AddToMap)
    mesh->InitHousingFixtureData(houseGuid, exteriorComponentID, houseExteriorWmoDataID,
        exteriorComponentType, houseSize, exteriorComponentHookID);

    // Now add to map — this triggers the create packet with all fragments included
    if (!AddToMap(mesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseMeshObject: AddToMap failed for plot {} fileDataID {}",
            plotIndex, fileDataID);
        delete mesh;
        return nullptr;
    }

    _meshObjects[plotIndex].push_back(mesh->GetGUID());

    TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseMeshObject: plot={} guid={} fileDataID={} isWMO={} "
        "localPos=({:.1f}, {:.1f}, {:.1f}) gridPos=({:.1f}, {:.1f}, {:.1f}) exteriorComponentID={} wmoDataID={}",
        plotIndex, mesh->GetGUID().ToString(), fileDataID, isWMO,
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(),
        mesh->GetPositionX(), mesh->GetPositionY(), mesh->GetPositionZ(),
        exteriorComponentID, houseExteriorWmoDataID);

    return mesh;
}

void HousingMap::SpawnFullHouseMeshObjects(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid,
    int32 exteriorComponentID, int32 houseExteriorWmoDataID)
{
    // Sniff-verified: Alliance starter house (Stucco Small) consists of 10 structural MeshObjects.
    // Two root pieces (base + door) positioned at the house location, and 8 child pieces
    // attached to roots with local-space offsets.
    //
    // Parent-child hierarchy:
    //   Root 0 (base, 6648736) - ExteriorComponentID 141, type 9
    //     ├── Child: Left side wall (7448531), hookID 2514
    //     ├── Child: Right side wall (7448531), hookID 2511
    //     ├── Child: Rear-left corner (7448532), hookID 2512
    //     ├── Child: Rear-right corner (7448532), hookID 2513
    //     └── Child: Front-right corner (7448532), hookID 2509
    //   Root 1 (door, 7420613) - ExteriorComponentID 1505, type 10
    //     ├── Child: Chimney (7118952), hookID 14931
    //     ├── Child: Window back-left (7450830), hookID 17202
    //     └── Child: Window back-right (7450830), hookID 14929

    // Spawn root piece 0: Base structure (uses the passed exteriorComponentID, default 141 = Stucco Base)
    MeshObject* basePiece = SpawnHouseMeshObject(plotIndex, 6648736, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, exteriorComponentID, houseExteriorWmoDataID,
        /*exteriorComponentType*/ 9, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Spawn root piece 1: Door/entrance assembly
    MeshObject* doorPiece = SpawnHouseMeshObject(plotIndex, 7420613, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 1505, houseExteriorWmoDataID,
        /*exteriorComponentType*/ 10, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Children of base piece (root 0) — local-space positions/rotations from sniff
    if (basePiece)
    {
        ObjectGuid baseGuid = basePiece->GetGUID();

        // Left side wall
        SpawnHouseMeshObject(plotIndex, 7448531, /*isWMO*/ true,
            Position(0.0417f, -6.9833f, 4.1722f, 0.0f),
            QuaternionData(0.0f, 0.0f, -0.7071066f, 0.70710695f), 1.0f,
            houseGuid, 1436, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 2514,
            baseGuid, /*attachFlags*/ 3, &housePos);

        // Right side wall
        SpawnHouseMeshObject(plotIndex, 7448531, /*isWMO*/ true,
            Position(0.0778f, 7.0028f, 4.1722f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.7071066f, 0.70710695f), 1.0f,
            houseGuid, 1436, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 2511,
            baseGuid, /*attachFlags*/ 3, &housePos);

        // Rear-left corner
        SpawnHouseMeshObject(plotIndex, 7448532, /*isWMO*/ true,
            Position(-5.3722f, 3.4167f, 4.1444f, 0.0f),
            QuaternionData(0.0f, 0.0f, -1.0f, 0.0f), 1.0f,
            houseGuid, 1417, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 2512,
            baseGuid, /*attachFlags*/ 3, &housePos);

        // Rear-right corner
        SpawnHouseMeshObject(plotIndex, 7448532, /*isWMO*/ true,
            Position(-5.3917f, -3.5084f, 4.1639f, 0.0f),
            QuaternionData(0.0f, 0.0f, -1.0f, 0.0f), 1.0f,
            houseGuid, 1417, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 2513,
            baseGuid, /*attachFlags*/ 3, &housePos);

        // Front-right corner
        SpawnHouseMeshObject(plotIndex, 7448532, /*isWMO*/ true,
            Position(5.3805f, 3.4694f, 4.1639f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 1417, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 2509,
            baseGuid, /*attachFlags*/ 3, &housePos);
    }

    // Children of door piece (root 1) — local-space positions/rotations from sniff
    if (doorPiece)
    {
        ObjectGuid doorGuid = doorPiece->GetGUID();

        // Chimney (back-left)
        SpawnHouseMeshObject(plotIndex, 7118952, /*isWMO*/ true,
            Position(-3.6472f, -5.6444f, 12.3556f, 0.0f),
            QuaternionData(0.0f, 0.0f, -0.7071066f, 0.70710695f), 1.0f,
            houseGuid, 1452, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 16, /*houseSize*/ 2, /*hookID*/ 14931,
            doorGuid, /*attachFlags*/ 3, &housePos);

        // Window back-left
        SpawnHouseMeshObject(plotIndex, 7450830, /*isWMO*/ true,
            Position(-3.025f, -0.0222f, 11.35f, 0.0f),
            QuaternionData(0.0f, 0.0f, -1.0f, 0.0f), 1.0f,
            houseGuid, 1448, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 14, /*houseSize*/ 2, /*hookID*/ 17202,
            doorGuid, /*attachFlags*/ 3, &housePos);

        // Window back-right
        SpawnHouseMeshObject(plotIndex, 7450830, /*isWMO*/ true,
            Position(3.0305f, -0.0222f, 11.35f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 1448, houseExteriorWmoDataID,
            /*exteriorComponentType*/ 14, /*houseSize*/ 2, /*hookID*/ 14929,
            doorGuid, /*attachFlags*/ 3, &housePos);
    }

    uint32 meshCount = 0;
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr != _meshObjects.end())
        meshCount = static_cast<uint32>(meshItr->second.size());

    TC_LOG_ERROR("housing", "HousingMap::SpawnFullHouseMeshObjects: Spawned {} MeshObjects for plot {} in neighborhood '{}' "
        "(base={} door={})",
        meshCount, plotIndex, _neighborhood ? _neighborhood->GetName() : "?",
        basePiece ? "OK" : "FAIL", doorPiece ? "OK" : "FAIL");
}

void HousingMap::DespawnAllMeshObjectsForPlot(uint8 plotIndex)
{
    auto itr = _meshObjects.find(plotIndex);
    if (itr == _meshObjects.end())
        return;

    for (ObjectGuid const& guid : itr->second)
    {
        if (MeshObject* mesh = GetMeshObject(guid))
            mesh->AddObjectToRemoveList();
    }

    TC_LOG_DEBUG("housing", "HousingMap::DespawnAllMeshObjectsForPlot: Despawned {} MeshObject(s) for plot {}",
        itr->second.size(), plotIndex);
    _meshObjects.erase(itr);
}

void HousingMap::DespawnHouseForPlot(uint8 plotIndex)
{
    // Despawn MeshObjects first, then the GO
    DespawnAllMeshObjectsForPlot(plotIndex);

    auto itr = _houseGameObjects.find(plotIndex);
    if (itr == _houseGameObjects.end())
        return;

    if (GameObject* go = GetGameObject(itr->second))
        go->AddObjectToRemoveList();

    TC_LOG_DEBUG("housing", "HousingMap::DespawnHouseForPlot: Despawned house GO for plot {}", plotIndex);
    _houseGameObjects.erase(itr);
}

GameObject* HousingMap::GetHouseGameObject(uint8 plotIndex)
{
    auto itr = _houseGameObjects.find(plotIndex);
    if (itr == _houseGameObjects.end())
        return nullptr;

    return GetGameObject(itr->second);
}

int8 HousingMap::GetPlotIndexForHouseGO(ObjectGuid goGuid) const
{
    for (auto const& [plotIndex, guid] : _houseGameObjects)
    {
        if (guid == goGuid)
            return static_cast<int8>(plotIndex);
    }
    return -1;
}

// ============================================================
// Decor GO Management
// ============================================================

GameObject* HousingMap::SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid)
{
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: No HouseDecorData for entry {} (decorGuid={})",
            decor.DecorEntryId, decor.Guid.ToString());
        return nullptr;
    }

    uint32 goEntry = decorData->GameObjectID > 0 ? static_cast<uint32>(decorData->GameObjectID) : 0;
    if (!goEntry)
    {
        TC_LOG_DEBUG("housing", "HousingMap::SpawnDecorItem: Decor entry {} has GameObjectID=0 (CLIENT_MODEL type), skipping GO spawn",
            decor.DecorEntryId);
        return nullptr;
    }

    float x = decor.PosX;
    float y = decor.PosY;
    float z = decor.PosZ;

    LoadGrid(x, y);

    // Decor rotation is stored as quaternion
    QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

    Position pos(x, y, z);
    GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, GO_STATE_ACTIVE);
    if (!go)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to create decor GO entry {} at ({}, {}, {}) for decor {}",
            goEntry, x, y, z, decor.Guid.ToString());
        return nullptr;
    }

    go->SetFlag(GO_FLAG_NODESPAWN);

    // Universally visible (same rationale as cornerstones — no DB spawn, no phase_area)
    PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), 0, 0, 0);

    // Populate the FHousingDecor_C entity fragment
    go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0);

    if (!AddToMap(go))
    {
        delete go;
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to add decor GO to map for decor {}", decor.Guid.ToString());
        return nullptr;
    }

    // Track the decor GO
    _decorGameObjects[plotIndex].push_back(go->GetGUID());
    _decorGuidToGoGuid[decor.Guid] = go->GetGUID();
    _decorGuidToPlotIndex[decor.Guid] = plotIndex;

    TC_LOG_DEBUG("housing", "HousingMap::SpawnDecorItem: Spawned decor GO entry={} goGuid={} decorGuid={} at ({:.1f}, {:.1f}, {:.1f}) for plot {}",
        goEntry, go->GetGUID().ToString(), decor.Guid.ToString(), x, y, z, plotIndex);

    return go;
}

void HousingMap::DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid)
{
    auto goItr = _decorGuidToGoGuid.find(decorGuid);
    if (goItr == _decorGuidToGoGuid.end())
        return;

    ObjectGuid goGuid = goItr->second;
    if (GameObject* go = GetGameObject(goGuid))
        go->AddObjectToRemoveList();

    // Remove from tracking
    auto& plotDecor = _decorGameObjects[plotIndex];
    plotDecor.erase(std::remove(plotDecor.begin(), plotDecor.end(), goGuid), plotDecor.end());
    _decorGuidToGoGuid.erase(goItr);
    _decorGuidToPlotIndex.erase(decorGuid);

    TC_LOG_DEBUG("housing", "HousingMap::DespawnDecorItem: Despawned decor GO for decorGuid={} plot={}", decorGuid.ToString(), plotIndex);
}

void HousingMap::DespawnAllDecorForPlot(uint8 plotIndex)
{
    auto itr = _decorGameObjects.find(plotIndex);
    if (itr == _decorGameObjects.end())
        return;

    for (ObjectGuid const& goGuid : itr->second)
    {
        if (GameObject* go = GetGameObject(goGuid))
            go->AddObjectToRemoveList();
    }

    // Clean up all tracking for this plot's decor
    std::vector<ObjectGuid> decorGuidsToRemove;
    for (auto const& [decorGuid, pIdx] : _decorGuidToPlotIndex)
    {
        if (pIdx == plotIndex)
            decorGuidsToRemove.push_back(decorGuid);
    }
    for (ObjectGuid const& decorGuid : decorGuidsToRemove)
    {
        _decorGuidToGoGuid.erase(decorGuid);
        _decorGuidToPlotIndex.erase(decorGuid);
    }

    itr->second.clear();
    _decorSpawnedPlots.erase(plotIndex);

    TC_LOG_DEBUG("housing", "HousingMap::DespawnAllDecorForPlot: Despawned all decor GOs for plot {}", plotIndex);
}

void HousingMap::SpawnAllDecorForPlot(uint8 plotIndex, Housing const* housing)
{
    if (!housing)
        return;

    if (_decorSpawnedPlots.count(plotIndex))
        return; // Already spawned

    ObjectGuid houseGuid = housing->GetHouseGuid();
    uint32 spawnCount = 0;
    for (auto const& [decorGuid, decor] : housing->GetPlacedDecorMap())
    {
        if (SpawnDecorItem(plotIndex, decor, houseGuid))
            ++spawnCount;
    }

    _decorSpawnedPlots.insert(plotIndex);

    TC_LOG_INFO("housing", "HousingMap::SpawnAllDecorForPlot: Spawned {} decor GOs for plot {} in neighborhood '{}'",
        spawnCount, plotIndex, _neighborhood ? _neighborhood->GetName() : "?");
}

void HousingMap::UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot)
{
    auto goItr = _decorGuidToGoGuid.find(decorGuid);
    if (goItr == _decorGuidToGoGuid.end())
        return;

    if (GameObject* go = GetGameObject(goItr->second))
    {
        go->Relocate(pos);
        go->SetLocalRotation(rot.x, rot.y, rot.z, rot.w);

        TC_LOG_DEBUG("housing", "HousingMap::UpdateDecorPosition: Moved decor GO {} to ({:.1f}, {:.1f}, {:.1f}) for plot {}",
            decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), plotIndex);
    }
}
