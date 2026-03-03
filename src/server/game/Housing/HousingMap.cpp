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
#include <cmath>
#include <set>
#include "AreaTrigger.h"
#include "EventProcessor.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MeshObject.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGridLoader.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RealmList.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellPackets.h"
#include "UpdateData.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldStateMgr.h"

namespace
{
    std::string HexDumpPacket(WorldPacket const* packet, size_t maxBytes = 128)
    {
        if (!packet || packet->size() == 0)
            return "(empty)";
        size_t len = std::min(packet->size(), maxBytes);
        std::string result;
        result.reserve(len * 3 + 32);
        uint8 const* raw = packet->data();
        for (size_t i = 0; i < len; ++i)
        {
            if (i > 0 && i % 32 == 0)
                result += "\n  ";
            else if (i > 0)
                result += ' ';
            result += fmt::format("{:02X}", raw[i]);
        }
        if (len < packet->size())
            result += fmt::format(" ...({} more)", packet->size() - len);
        return result;
    }

    // Recurring event that sends housing WorldState counters every ~300ms.
    // Sniff-verified: 5 continuous counters throughout the entire housing map session.
    // Counters 1-3 (13436/13437/13438) increment by ~1333 each tick.
    // Counters 4-5 (16035/16711) increment by ~7233 each tick.
    class HousingWorldStateCounterEvent : public BasicEvent
    {
    public:
        HousingWorldStateCounterEvent(ObjectGuid playerGuid,
            uint32 counter1, uint32 counter2, uint32 counter3,
            uint32 counter4, uint32 counter5)
            : _playerGuid(playerGuid)
            , _counter1(counter1), _counter2(counter2), _counter3(counter3)
            , _counter4(counter4), _counter5(counter5) { }

        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
        {
            Player* player = ObjectAccessor::FindPlayer(_playerGuid);
            if (!player || !player->IsInWorld())
                return true; // delete event — player gone

            // Send all five counter WorldState updates
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_1, _counter1);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_2, _counter2);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_3, _counter3);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_4, _counter4);
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_COUNTER_5, _counter5);

            // Increment for next tick (different rates per sniff)
            _counter1 += HOUSING_WORLDSTATE_INCREMENT;
            _counter2 += HOUSING_WORLDSTATE_INCREMENT;
            _counter3 += HOUSING_WORLDSTATE_INCREMENT;
            _counter4 += HOUSING_WORLDSTATE_INCREMENT_2;
            _counter5 += HOUSING_WORLDSTATE_INCREMENT_2;

            // Re-schedule self for next tick
            player->m_Events.AddEventAtOffset(
                new HousingWorldStateCounterEvent(_playerGuid,
                    _counter1, _counter2, _counter3, _counter4, _counter5),
                Milliseconds(HOUSING_WORLDSTATE_INTERVAL_MS));

            return true; // delete this instance (new one scheduled)
        }

    private:
        ObjectGuid _playerGuid;
        uint32 _counter1;
        uint32 _counter2;
        uint32 _counter3;
        uint32 _counter4;
        uint32 _counter5;
    };
}

HousingMap::HousingMap(uint32 id, time_t expiry, uint32 instanceId, Difficulty spawnMode, uint32 neighborhoodId)
    : Map(id, expiry, instanceId, spawnMode), _neighborhoodId(neighborhoodId), _neighborhood(nullptr)
{
    // Prevent the map from being unloaded — housing maps are persistent
    // Map::CanUnload() returns false when m_unloadTimer == 0
    m_unloadTimer = 0;
    HousingMap::InitVisibilityDistance();

    // Verify InstanceType is MAP_HOUSE_NEIGHBORHOOD (8).
    // The client's "Airlock" system sets field_32=2 ONLY when InstanceType==8.
    // Without this, IsInsidePlot() always returns false → OutsidePlotBounds on ALL decor placement.
    if (GetEntry()->InstanceType != MAP_HOUSE_NEIGHBORHOOD)
    {
        TC_LOG_ERROR("housing", "CRITICAL: HousingMap {} '{}' has InstanceType={}, expected {} (MAP_HOUSE_NEIGHBORHOOD). "
            "Client will NOT allow decor placement — OutsidePlotBounds will always fire!",
            id, GetEntry()->MapName[sWorld->GetDefaultDbcLocale()],
            GetEntry()->InstanceType, MAP_HOUSE_NEIGHBORHOOD);
    }
    else
    {
        TC_LOG_DEBUG("housing", "HousingMap::ctor: mapId={} neighborhoodId={} instanceId={} "
            "InstanceType={} (MAP_HOUSE_NEIGHBORHOOD) — OK",
            id, neighborhoodId, instanceId, GetEntry()->InstanceType);
    }
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

    TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: DB2 PlotIndex→GOEntry mapping for {} plots on map {}:",
        uint32(plots.size()), neighborhoodMapId);
    for (NeighborhoodPlotData const* plot : plots)
    {
        TC_LOG_DEBUG("housing", "  DB2 ID={} PlotIndex={} CornerstoneGOEntry={} Cost={} WorldState={} HousePos=({:.1f},{:.1f},{:.1f})",
            plot->ID, plot->PlotIndex, plot->CornerstoneGameObjectID, plot->Cost, plot->WorldState,
            plot->HousePosition[0], plot->HousePosition[1], plot->HousePosition[2]);
    }

    for (NeighborhoodPlotData const* plot : plots)
    {
        float x = plot->CornerstonePosition[0];
        float y = plot->CornerstonePosition[1];
        float z = plot->CornerstonePosition[2];

        // Ensure the grid at this position is loaded so we can add GOs
        LoadGrid(x, y);

        // Retail uses a UNIQUE CornerstoneGameObjectID per plot so the server can
        // identify which plot a player interacted with.  All share type=48, displayId=110660.
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
        PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

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

        // Spawn plot AreaTrigger (entry 37358) at the house position (plot center).
        // Sniff-verified: Box shape 35x30x94, DecalPropertiesId=621 (plot boundary visual),
        // SpellForVisuals=1282351, FHousingPlotAreaTrigger_C entity fragment with owner data.
        // The AT is required for the client to show the edit menu and plot boundary decal.
        {
            float hx = plot->HousePosition[0];
            float hy = plot->HousePosition[1];
            float hz = plot->HousePosition[2];
            float hFacing = plot->HouseRotation[2];

            LoadGrid(hx, hy);

            Position atPos(hx, hy, hz, hFacing);
            AreaTrigger* plotAt = AreaTrigger::CreateStaticAreaTrigger({ .Id = 37358, .IsCustom = false }, this, atPos);
            if (plotAt)
            {
                PhasingHandler::InitDbPhaseShift(plotAt->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                ObjectGuid ownerGuid = plotInfo ? plotInfo->OwnerGuid : ObjectGuid::Empty;
                ObjectGuid houseGuid = plotInfo ? plotInfo->HouseGuid : ObjectGuid::Empty;
                ObjectGuid ownerBnetGuid = plotInfo ? plotInfo->OwnerBnetGuid : ObjectGuid::Empty;

                plotAt->InitHousingPlotData(static_cast<uint32>(plot->PlotIndex), ownerGuid, houseGuid, ownerBnetGuid);

                _plotAreaTriggers[static_cast<uint8>(plot->PlotIndex)] = plotAt->GetGUID();

                TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} AT entry=37358 guid={} at ({:.1f},{:.1f},{:.1f}) owner={}",
                    plot->PlotIndex, plotAt->GetGUID().ToString(), hx, hy, hz,
                    ownerGuid.IsEmpty() ? "none" : ownerGuid.ToString());
            }
            else
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create plot AT (entry 37358) for plot {} at ({:.1f},{:.1f},{:.1f})",
                    plot->PlotIndex, hx, hy, hz);
            }
        }
    }

    // Sniff-verified: SMSG_INIT_WORLD_STATES for housing maps (2735/2736) has Field Count = 0.
    // Per-plot ownership worldstates are sent as individual SMSG_UPDATE_WORLD_STATE packets
    // in SendPerPlayerPlotWorldStates() after the player joins — NOT in the init packet.
    // Do NOT use Map::SetWorldStateValue() here as it pollutes INIT_WORLD_STATES.
    uint32 wsCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->WorldState != 0)
            ++wsCount;
    }

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Spawned {} GOs, {} plots have WorldState IDs for {} plots in neighborhood '{}' (noEntry={})",
        goCount, wsCount, uint32(plots.size()), _neighborhood->GetName(), noEntryCount);

    // Spawn house structure GOs for owned plots
    uint32 houseCount = 0;
    uint32 houseSuccessCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIdx);
        if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
            continue;

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} is owned by {} - attempting house spawn (HousePos: {:.1f}, {:.1f}, {:.1f})",
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

    TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: House spawn results: {}/{} successful for neighborhood '{}'",
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

    // Update the plot AT's owner data so the client sees current ownership
    if (AreaTrigger* plotAt = GetPlotAreaTrigger(plotIndex))
    {
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIndex);
        if (owned && plotInfo)
            plotAt->UpdateHousingPlotOwnerData(plotInfo->OwnerGuid, plotInfo->HouseGuid, plotInfo->OwnerBnetGuid);
        else
            plotAt->UpdateHousingPlotOwnerData(ObjectGuid::Empty, ObjectGuid::Empty, ObjectGuid::Empty);
    }

    // Send per-plot WorldState update to all players on the map.
    // Sniff-verified: housing maps use individual SMSG_UPDATE_WORLD_STATE packets
    // (NOT map-scoped SetWorldStateValue, which pollutes INIT_WORLD_STATES).
    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    TC_LOG_ERROR("housing", "SetPlotOwnershipState: Broadcasting WorldState for plot {} "
        "(neighborhoodMapId={}, owned={}, numPlots={})",
        plotIndex, neighborhoodMapId, owned, plots.size());

    for (NeighborhoodPlotData const* plotData : plots)
    {
        if (plotData->PlotIndex == static_cast<int32>(plotIndex))
        {
            if (plotData->WorldState != 0)
            {
                TC_LOG_ERROR("housing", "SetPlotOwnershipState: Found plot {} with WorldState={}, "
                    "broadcasting to {} players",
                    plotIndex, plotData->WorldState,
                    std::distance(GetPlayers().begin(), GetPlayers().end()));

                // Send personalized value to each player on the map
                for (MapReference const& ref : GetPlayers())
                {
                    if (Player* mapPlayer = ref.GetSource())
                    {
                        HousingPlotOwnerType ownerType = GetPlotOwnerTypeForPlayer(mapPlayer, plotIndex);
                        mapPlayer->SendUpdateWorldState(plotData->WorldState, static_cast<uint32>(ownerType), false);

                        TC_LOG_ERROR("housing", "SetPlotOwnershipState: Sent WorldState {} = {} ({}) to player {}",
                            plotData->WorldState, uint32(ownerType),
                            ownerType == HOUSING_PLOT_OWNER_SELF ? "SELF" :
                            ownerType == HOUSING_PLOT_OWNER_FRIEND ? "FRIEND" :
                            ownerType == HOUSING_PLOT_OWNER_STRANGER ? "STRANGER" : "NONE",
                            mapPlayer->GetGUID().ToString());
                    }
                }
            }
            else
            {
                TC_LOG_ERROR("housing", "SetPlotOwnershipState: Plot {} matched but WorldState=0, skipping broadcast",
                    plotIndex);
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
    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Looking up housing for player {} in neighborhood '{}' (guid={})",
        player->GetGUID().ToString(), _neighborhood->GetName(), _neighborhood->GetGuid().ToString());

    Housing* housing = player->GetHousingForNeighborhood(_neighborhood->GetGuid());
    if (!housing)
    {
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: No housing found via GetHousingForNeighborhood. Checking fallback (allHousings count: {})",
            uint32(player->GetAllHousings().size()));

        // Fallback: check if any of the player's housings has a plot in this neighborhood
        for (Housing const* h : player->GetAllHousings())
        {
            TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Fallback check: housing plotIndex={} neighborhoodGuid={} houseGuid={}",
                h ? h->GetPlotIndex() : 255,
                h ? h->GetNeighborhoodGuid().ToString() : "null",
                h ? h->GetHouseGuid().ToString() : "null");

            if (h && _neighborhood->GetPlotInfo(h->GetPlotIndex()))
            {
                housing = const_cast<Housing*>(h);
                TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Fixed neighborhood GUID mismatch for player {} (stored={}, canonical={})",
                    player->GetGUID().ToString(), h->GetNeighborhoodGuid().ToString(), _neighborhood->GetGuid().ToString());
                // Fix the stored GUID so future lookups work
                housing->SetNeighborhoodGuid(_neighborhood->GetGuid());
                break;
            }
        }
    }

    if (housing)
    {
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Player {} has housing: plotIndex={} houseType={} houseGuid={}",
            player->GetGUID().ToString(), housing->GetPlotIndex(), housing->GetHouseType(), housing->GetHouseGuid().ToString());

        AddPlayerHousing(player->GetGUID(), housing);

        // Ensure the neighborhood PlotInfo has the HouseGuid (may be missing after server restart
        // since LoadFromDB only populates it if character_housing row exists)
        uint8 plotIdx = housing->GetPlotIndex();
        ObjectGuid ownerBnetGuid = player->GetSession() ? player->GetSession()->GetBattlenetAccountGUID() : ObjectGuid::Empty;
        if (!housing->GetHouseGuid().IsEmpty())
        {
            _neighborhood->UpdatePlotHouseInfo(plotIdx, housing->GetHouseGuid(), ownerBnetGuid);

            // Update the plot AT's owner data (may have been spawned with empty GUIDs
            // if the player was offline during SpawnPlotGameObjects)
            if (AreaTrigger* plotAt = GetPlotAreaTrigger(plotIdx))
                plotAt->UpdateHousingPlotOwnerData(player->GetGUID(), housing->GetHouseGuid(), ownerBnetGuid);
        }

        // Update PlayerMirrorHouse.MapID so the client knows this house is on the current map.
        // Without this, MapID stays at 0 (set during login) and the client rejects
        // edit mode with HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT (error 1 = first non-success code).
        player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(GetId()));

        // Spawn house GO if not already present (handles offline → online transition)
        bool alreadySpawned = _houseGameObjects.find(plotIdx) != _houseGameObjects.end();
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: House GO for plot {}: alreadySpawned={} _houseGameObjects.size={}",
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

            TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: SpawnHouseForPlot result for plot {}: {}",
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
                    int32 faction = _neighborhood ? _neighborhood->GetFactionRestriction()
                        : NEIGHBORHOOD_FACTION_ALLIANCE;
                    SpawnFullHouseMeshObjects(plotIdx, pos, rot,
                        housing->GetHouseGuid(), 141, 9, faction);

                    // Also spawn room entity + Geobox if not already present
                    if (_roomEntities.find(plotIdx) == _roomEntities.end())
                        SpawnRoomForPlot(plotIdx, pos, rot, housing->GetHouseGuid());

                    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Late-spawned MeshObjects for plot {} (house GO {} already existed)",
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

    // === DIAGNOSTIC: Report plot GO state when player enters ===
    {
        TC_LOG_DEBUG("housing", "=== HOUSING DIAGNOSTIC for player {} entering map {} ===", player->GetGUID().ToString(), GetId());
        TC_LOG_DEBUG("housing", "  Player position: ({:.1f}, {:.1f}, {:.1f})", player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
        TC_LOG_DEBUG("housing", "  _plotGameObjects.size={} Map InstanceType={} (expected {} for MAP_HOUSE_NEIGHBORHOOD)",
            uint32(_plotGameObjects.size()), GetEntry()->InstanceType, MAP_HOUSE_NEIGHBORHOOD);

        uint32 shown = 0;
        for (auto const& [plotIdx, goGuid] : _plotGameObjects)
        {
            if (shown >= 3) break;
            if (GameObject* go = GetGameObject(goGuid))
            {
                float dist = player->GetDistance(go);
                TC_LOG_DEBUG("housing", "  Plot[{}] GO: guid={} entry={} pos=({:.1f},{:.1f},{:.1f}) dist={:.1f}yd",
                    plotIdx, goGuid.ToString(), go->GetEntry(),
                    go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), dist);
            }
            ++shown;
        }
        TC_LOG_DEBUG("housing", "=== END HOUSING DIAGNOSTIC ===");
    }

    // Send neighborhood context so the client can call SetViewingNeighborhood()
    // and enable Cornerstone purchase UI interaction
    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfo;
    if (housing)
    {
        houseInfo.House.HouseGuid = housing->GetHouseGuid();
        houseInfo.House.OwnerGuid = player->GetGUID();
        houseInfo.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
        houseInfo.House.PlotId = housing->GetPlotIndex();
        houseInfo.House.AccessFlags = housing->GetSettingsFlags();
        houseInfo.House.HasMoveOutTime = false;
    }
    else if (_neighborhood)
    {
        // No house — send player's GUID (client expects HighGuid::Player type)
        houseInfo.House.OwnerGuid = player->GetGUID();
        houseInfo.House.NeighborhoodGuid = _neighborhood->GetGuid();
    }
    houseInfo.Result = 0;
    WorldPacket const* houseInfoPkt = houseInfo.Write();
    TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap CURRENT_HOUSE_INFO ({} bytes): {}",
        houseInfoPkt->size(), HexDumpPacket(houseInfoPkt));
    TC_LOG_DEBUG("housing", "  PlotId={}, HouseGuid={}, OwnerGuid={}, NeighborhoodGuid={}, AccessFlags={}, hasHouse={}",
        houseInfo.House.PlotId, houseInfo.House.HouseGuid.ToString(),
        houseInfo.House.OwnerGuid.ToString(), houseInfo.House.NeighborhoodGuid.ToString(),
        houseInfo.House.AccessFlags, housing ? "yes" : "no");
    player->SendDirectMessage(houseInfoPkt);

    // ENTER_PLOT must be sent AFTER SMSG_UPDATE_OBJECT creates the AT on the client.
    // UPDATE_OBJECT is flushed after AddPlayerToMap returns, so sending ENTER_PLOT
    // here synchronously would reference an AT GUID the client doesn't know yet.
    // Solution: schedule a deferred event that sends ENTER_PLOT after a short delay,
    // giving the UPDATE_OBJECT time to flush to the client first.
    // The at_housing_plot AT overlap script also sends ENTER_PLOT when the player
    // physically enters the box, but on login the AT overlap check may not fire
    // on the first tick (player already inside the AT when it was created).
    // SetPlayerCurrentPlot is called here so the AT script's alreadyOnPlot guard
    // prevents duplicate ENTER_PLOT sends if both paths fire.
    if (housing)
    {
        uint8 plotIndex = housing->GetPlotIndex();
        SetPlayerCurrentPlot(player->GetGUID(), plotIndex);

        ObjectGuid playerGuid = player->GetGUID();
        uint8 deferredPlotIndex = plotIndex;
        player->m_Events.AddEventAtOffset([playerGuid, deferredPlotIndex]()
        {
            Player* p = ObjectAccessor::FindPlayer(playerGuid);
            if (!p || !p->IsInWorld())
                return;

            HousingMap* hMap = dynamic_cast<HousingMap*>(p->GetMap());
            if (!hMap)
                return;

            AreaTrigger* plotAt = hMap->GetPlotAreaTrigger(deferredPlotIndex);
            if (!plotAt)
            {
                TC_LOG_ERROR("housing", "HousingMap deferred ENTER_PLOT: No AT for plot {} player {}",
                    deferredPlotIndex, playerGuid.ToString());
                return;
            }

            WorldPackets::Neighborhood::NeighborhoodPlayerEnterPlot enterPlot;
            enterPlot.PlotAreaTriggerGuid = plotAt->GetGUID();
            p->SendDirectMessage(enterPlot.Write());

            // Post-tutorial auras first (slots 8,9,50), then plot enter auras (slots 50,56,9).
            // Retail applies tutorial-done auras once (at quest reward) but we re-send each
            // map entry since they don't exist in DB2 and can't persist as real auras.
            hMap->SendPostTutorialAuras(p);
            hMap->SendPlotEnterSpellPackets(p, deferredPlotIndex);

            // Diagnostic: print AT position vs player position for OutsidePlotBounds debugging
            float dist2d = p->GetExactDist2d(plotAt);
            float dist3d = p->GetExactDist(plotAt);
            bool inBox = p->IsWithinBox(*plotAt, 35.0f, 30.0f, 47.0f);  // half-extents from SQL ShapeData

            TC_LOG_DEBUG("housing", "HousingMap deferred ENTER_PLOT: player {} plot {} AT {}\n"
                "  AT pos: ({:.1f}, {:.1f}, {:.1f}, facing={:.3f})\n"
                "  Player pos: ({:.1f}, {:.1f}, {:.1f})\n"
                "  Dist2D={:.1f} Dist3D={:.1f} InBox={} HasPlayers={}",
                playerGuid.ToString(), deferredPlotIndex, plotAt->GetGUID().ToString(),
                plotAt->GetPositionX(), plotAt->GetPositionY(), plotAt->GetPositionZ(), plotAt->GetOrientation(),
                p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
                dist2d, dist3d, inBox,
                plotAt->HasAreaTriggerFlag(AreaTriggerFieldFlags::HasPlayers));
        }, Milliseconds(500));
    }

    // Start the periodic housing WorldState counter timer.
    // Sniff-verified: 5 counters sent as individual SMSG_UPDATE_WORLD_STATE packets.
    // Counters 1-3 increment by ~1333, counters 4-5 by ~7233, every ~300ms.
    // Seed with getMSTime()-based values (retail uses opaque server-tick values;
    // the exact seed doesn't matter as long as the increment pattern is correct).
    {
        uint32 baseSeed = getMSTime();
        player->m_Events.AddEventAtOffset(
            new HousingWorldStateCounterEvent(player->GetGUID(),
                baseSeed, baseSeed / 3, baseSeed + 55758738,
                baseSeed * 2, baseSeed + 123456789),
            Milliseconds(HOUSING_WORLDSTATE_INTERVAL_MS));
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Started WorldState counter timer (5 counters) for player {}",
            player->GetGUID().ToString());
    }

    // Send personalized per-plot WorldState values for this specific player.
    // The init world states (sent during Map::AddPlayerToMap) use map-global defaults
    // (STRANGER for occupied, NONE for unoccupied). This corrects them to SELF/FRIEND
    // based on the player's relationship to each plot owner.
    SendPerPlayerPlotWorldStates(player);

    // Comprehensive summary of all packets sent during map entry (for sniff comparison)
    TC_LOG_DEBUG("housing", "=== AddPlayerToMap COMPLETE for player {} ===\n"
        "  Map: {} InstanceType={} NeighborhoodId={}\n"
        "  HasHouse: {} PlotIndex: {}\n"
        "  Packets sent: CURRENT_HOUSE_INFO, 3xAURA+3xSTART+3xGO, "
        "deferred ENTER_PLOT (500ms), WorldState timer started, PerPlayerPlotWorldStates\n"
        "  Player pos: ({:.1f}, {:.1f}, {:.1f})",
        player->GetGUID().ToString(),
        GetId(), GetEntry()->InstanceType, _neighborhoodId,
        housing ? "yes" : "no",
        housing ? housing->GetPlotIndex() : 255,
        player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    return true;
}

void HousingMap::RemovePlayerFromMap(Player* player, bool remove)
{
    // Remove plot auras before removing housing data.
    if (Housing const* housing = GetHousingForPlayer(player->GetGUID()))
    {
        // Remove all plot enter/presence auras (manual packets — spells not in DB2)
        SendPlotLeaveAuraRemoval(player);

        // Clear plot tracking
        ClearPlayerCurrentPlot(player->GetGUID());
    }

    RemovePlayerHousing(player->GetGUID());

    TC_LOG_DEBUG("housing", "HousingMap::RemovePlayerFromMap: Player {} leaving housing map {} instanceId {}",
        player->GetGUID().ToString(), GetId(), GetInstanceId());

    Map::RemovePlayerFromMap(player, remove);
}

void HousingMap::SendPostTutorialAuras(Player* player)
{
    // Sniff-verified: After quest 94455 "Home at Last" completion, three "post-tutorial" auras
    // are applied at slots 8, 9, 50. These replace old tutorial-phase auras.
    // These persist for the rest of the session. Since they don't exist in DB2, we send
    // manual SMSG_AURA_UPDATE packets each time the player enters the housing map.
    // Slot 8: spell 1285428 (NoCaster, ActiveFlags=1)
    // Slot 9: spell 1285424 (NoCaster, ActiveFlags=1) — will be overwritten by plot enter aura
    // Slot 50: spell 1266699 (NoCaster|Scalable, ActiveFlags=1, Points=1) — overwritten by plot enter

    // Spell 1285428 at slot 8
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_1,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 8;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;  // 15
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;  // 781
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // Spell 1285424 at slot 9
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_2,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 9;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // Spell 1266699 at slot 50 (same ID as SPELL_HOUSING_PLOT_ENTER_2, different slot + Points)
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_3,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 50;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER | AFLAG_SCALABLE;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraInfo.AuraData->Points.push_back(1.0f);
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    TC_LOG_DEBUG("housing", "SendPostTutorialAuras: Sent 3 post-tutorial aura sequences "
        "(1285428@s8, 1285424@s9, 1266699@s50) for player {}",
        player->GetGUID().ToString());
}

void HousingMap::SendPlotEnterSpellPackets(Player* player, uint8 plotIndex)
{
    // Sniff-verified plot enter spell sequence (packets 26530-26541):
    //   AURA_UPDATE+SPELL_START+SPELL_GO (1239847, slot 50)
    //   → HasPlayers flag on AT (Flags=1024)
    //   → AURA_UPDATE+SPELL_START+SPELL_GO (469226, slot 56)
    //   → AURA_UPDATE removal (slot 9) → AURA_UPDATE+SPELL_START+SPELL_GO (1266699, slot 9)
    // These spells don't exist in DB2, so CastSpell() fails silently. Manual packets required.

    TC_LOG_DEBUG("housing", "SendPlotEnterSpellPackets: BEGIN for player {} plot {} map {}",
        player->GetGUID().ToString(), plotIndex, GetId());

    // 1. Spell 1239847 — plot enter tracking aura (slot 50)
    // SPELL_START CastFlags=524302, SPELL_GO CastFlags=525068
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_PLOT_ENTER,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 50;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_PLOT_ENTER;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 2;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER;
        spellStart.Cast.CastFlags = CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_VISUAL_CHAIN;  // 524302 = 0x8000E
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER;
        spellGo.Cast.CastFlags = CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10 | CAST_FLAG_VISUAL_CHAIN;  // 525068 = 0x8030C
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // 2. Set HasPlayers flag (Flags=1024) on the plot AreaTrigger.
    // Sniff-verified: this UPDATE_OBJECT goes out between the first spell set (1239847)
    // and the second (469226). It tells the client that players are inside this AT.
    if (AreaTrigger* plotAt = GetPlotAreaTrigger(plotIndex))
    {
        plotAt->SetAreaTriggerFlag(AreaTriggerFieldFlags::HasPlayers);
        TC_LOG_DEBUG("housing", "SendPlotEnterSpellPackets: Set HasPlayers on AT {} for player {} plot {}",
            plotAt->GetGUID().ToString(), player->GetGUID().ToString(), plotIndex);
    }

    // 3. Spell 469226 — plot presence aura (slot 56)
    {
        ObjectGuid castId2 = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_PLOT_PRESENCE,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 56;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId2;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_PLOT_PRESENCE;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId2;
        spellStart.Cast.SpellID = SPELL_HOUSING_PLOT_PRESENCE;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_24 | CAST_FLAG_UNKNOWN_30;  // 0x2080000B
        spellStart.Cast.CastFlagsEx = 0x2000200;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId2;
        spellGo.Cast.SpellID = SPELL_HOUSING_PLOT_PRESENCE;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10 | CAST_FLAG_UNKNOWN_24 | CAST_FLAG_UNKNOWN_30;  // 0x20800309
        spellGo.Cast.CastFlagsEx = 0x2000210;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // 4. Spell 1266699 — slot 9 replacement (preceded by slot 9 removal)
    // CastFlags: START=15, GO=781, GoEx=16, GoEx2=4
    {
        // Remove existing slot 9 aura
        WorldPackets::Spells::AuraUpdate auraRemove;
        auraRemove.UpdateAll = false;
        auraRemove.UnitGUID = player->GetGUID();
        WorldPackets::Spells::AuraInfo removeInfo;
        removeInfo.Slot = 9;
        auraRemove.Auras.push_back(std::move(removeInfo));
        player->SendDirectMessage(auraRemove.Write());

        ObjectGuid castId3 = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_PLOT_ENTER_2,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        // Apply 1266699 at slot 9 (Flags=NoCaster|Scalable=9, PointsCount=1, Points[0]=1)
        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();
        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 9;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId3;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_PLOT_ENTER_2;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER | AFLAG_SCALABLE;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraInfo.AuraData->Points.push_back(1.0f);
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId3;
        spellStart.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER_2;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;  // 15
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId3;
        spellGo.Cast.SpellID = SPELL_HOUSING_PLOT_ENTER_2;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;  // 781
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    TC_LOG_DEBUG("housing", "SendPlotEnterSpellPackets: END — sent 3 spell sequences "
        "(1239847@s50, 469226@s56, 1266699@s9) + AT HasPlayers flag for player {} plot {}",
        player->GetGUID().ToString(), plotIndex);
}

void HousingMap::SendPlotLeaveAuraRemoval(Player* player)
{
    // Remove all plot enter/presence auras (slots 50, 56, 9)
    // Send aura removal packets (empty AuraData = HasAura=False)
    for (uint8 slot : { uint8(50), uint8(56), uint8(9) })
    {
        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = slot;
        auraUpdate.Auras.push_back(std::move(auraInfo));

        player->SendDirectMessage(auraUpdate.Write());
    }
    TC_LOG_DEBUG("housing", "HousingMap::SendPlotLeaveAuraRemoval: Removed auras (slots 50, 56, 9) for player {}",
        player->GetGUID().ToString());
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

    uint32 sentCount = 0;
    uint32 noWsCount = 0;
    uint32 selfCount = 0;
    uint32 friendCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->WorldState == 0)
        {
            ++noWsCount;
            continue;
        }

        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        HousingPlotOwnerType ownerType = GetPlotOwnerTypeForPlayer(player, plotIdx);
        player->SendUpdateWorldState(plot->WorldState, static_cast<uint32>(ownerType), false);
        ++sentCount;

        if (ownerType == HOUSING_PLOT_OWNER_SELF)
            ++selfCount;
        else if (ownerType == HOUSING_PLOT_OWNER_FRIEND)
            ++friendCount;
    }

    TC_LOG_INFO("housing", "HousingMap::SendPerPlayerPlotWorldStates: Player {} — sent {} WorldState updates "
        "(self={}, friend={}, {} plots had no WorldState ID in DB2)",
        player->GetGUID().ToString(), sentCount, selfCount, friendCount, noWsCount);
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

    // Ground-clamp: the DB2 HousePosition Z can be below terrain level.
    // Snap Z to the terrain height so the house sits on the surface.
    {
        PhaseShift emptyPhase;
        PhasingHandler::InitDbPhaseShift(emptyPhase, 0, 0, 0);
        float groundZ = GetStaticHeight(emptyPhase, x, y, z + 20.0f, true, 50.0f);
        if (groundZ > INVALID_HEIGHT && std::abs(groundZ - z) > 1.0f)
        {
            TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: Ground-clamping plot {} Z from {:.2f} to {:.2f} (delta={:.2f})",
                plotIndex, z, groundZ, groundZ - z);
            z = groundZ;
        }
    }

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

    TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: plot={} pos=({:.2f}, {:.2f}, {:.2f}) facing={:.3f} "
        "rot=({:.3f}, {:.3f}, {:.3f}, {:.3f}) hasPlotInfo={} hasHouseGuid={} extCompID={} wmoDataID={}",
        plotIndex, x, y, z, facing, rot.x, rot.y, rot.z, rot.w,
        plotInfo != nullptr, plotInfo && !plotInfo->HouseGuid.IsEmpty(),
        exteriorComponentID, houseExteriorWmoDataID);

    if (!plotInfo)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: plotInfo is NULL for plot {} — "
            "skipping MeshObject spawn (IsOccupied check failed?)", plotIndex);
    }
    else if (plotInfo->HouseGuid.IsEmpty())
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: HouseGuid is EMPTY for plot {} — "
            "skipping MeshObject spawn (UpdatePlotHouseInfo not called?)", plotIndex);
    }

    // Spawn all house MeshObjects (sniff-verified: 10 structural pieces for alliance, different for horde)
    // Pieces have a parent-child hierarchy: base piece (0) and door piece (1) are roots,
    // other pieces attach to them with local-space positions/rotations.
    if (plotInfo && !plotInfo->HouseGuid.IsEmpty())
    {
        int32 faction = _neighborhood ? _neighborhood->GetFactionRestriction()
            : NEIGHBORHOOD_FACTION_ALLIANCE;
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Spawning MeshObjects for plot {} — "
            "HouseGuid={} faction={} ({})",
            plotIndex, plotInfo->HouseGuid.ToString(), faction,
            faction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");

        SpawnFullHouseMeshObjects(plotIndex, pos, rot, plotInfo->HouseGuid,
            exteriorComponentID, houseExteriorWmoDataID, faction);

        // Spawn room entity + component mesh with Geobox for this plot.
        // The client uses the MeshObject Geobox to validate decor placement bounds.
        // Without this, ALL placement attempts fail with OutsidePlotBounds.
        SpawnRoomForPlot(plotIndex, pos, rot, plotInfo->HouseGuid);
    }

    // Spawn the front door GO (entry 602702, type Goober, displayId 116971)
    // Sniff confirms: door is a separate interactive GO, NOT a MeshObject.
    // The door must be offset from the house center to the front entrance.
    // Try to get the offset from DB2 ExteriorComponentExitPoint, falling back to 5.5f.
    GameObject* doorGo = nullptr;
    uint32 doorEntry = 602702;
    float doorForwardOffset = 5.5f; // fallback

    // Find the door component by scanning the group for one with an exit point
    int32 groupID = sHousingMgr.GetGroupForComponent(static_cast<uint32>(exteriorComponentID));
    if (groupID != 0)
    {
        std::vector<uint32> const* groupComps = sHousingMgr.GetComponentsInGroup(groupID);
        if (groupComps)
        {
            for (uint32 compID : *groupComps)
            {
                ExteriorComponentExitPointEntry const* exitPt = sHousingMgr.GetExitPoint(compID);
                if (exitPt)
                {
                    // Exit point position is in local space — use the forward (X) component as offset
                    doorForwardOffset = exitPt->Position[0];
                    TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: Door offset from DB2 "
                        "ExitPoint (comp={}) = {:.2f}",
                        compID, doorForwardOffset);
                    break;
                }
            }
        }
    }

    float doorX = x + doorForwardOffset * std::cos(facing);
    float doorY = y + doorForwardOffset * std::sin(facing);
    Position doorPos(doorX, doorY, z, facing);

    GameObjectTemplate const* doorTemplate = sObjectMgr->GetGameObjectTemplate(doorEntry);
    if (doorTemplate)
    {
        doorGo = GameObject::CreateGameObject(doorEntry, this, doorPos, rot, 255, GO_STATE_READY);
        if (doorGo)
        {
            doorGo->SetFlag(GO_FLAG_NODESPAWN);
            PhasingHandler::InitDbPhaseShift(doorGo->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

            if (AddToMap(doorGo))
            {
                _houseGameObjects[plotIndex] = doorGo->GetGUID();
                TC_LOG_DEBUG("housing", "HousingMap::SpawnHouseForPlot: Door GO spawned - entry={} guid={} displayId={} at ({:.1f}, {:.1f}, {:.1f}) (house center: {:.1f}, {:.1f}, {:.1f}) for plot {}",
                    doorEntry, doorGo->GetGUID().ToString(), doorGo->GetDisplayId(), doorX, doorY, z, x, y, z, plotIndex);
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

void HousingMap::SpawnRoomForPlot(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid)
{
    // The retail client requires a Room entity with an attached MeshObject that has a Geobox
    // (axis-aligned bounding box) to define the plot's placement boundary. Without these,
    // the client reports "OutsidePlotBounds" for ALL decor placement attempts.
    //
    // Sniff-verified structure (12.0.1 retail):
    // 1. Room entity:      FHousingRoom_C fragment with HouseGUID, HouseRoomID=18, Flags=1
    // 2. Room MeshObject:  FHousingRoomComponentMesh_C + Geobox attached to room at local (0,0,0)
    //    Geobox bounds are identical for ALL factions/themes: (-35,-30,-1.01)→(35,30,125.01)
    //
    // In retail, the Room entity is a lightweight entity (type 18) without CGObject/FMeshObjectData_C.
    // Our implementation uses MeshObjects for both, adding room data as extra entity fragments.
    // The client processes fragments independently, so the extra fragments are harmless.

    int32 HOUSE_ROOM_ID = static_cast<int32>(sHousingMgr.GetBaseRoomEntryId());
    static constexpr int32 ROOM_FLAGS    = 1;     // BASE_ROOM
    static constexpr int32 ROOM_COMPONENT_ID = 196; // Sniff-verified: same for alliance+horde

    // Clean up any existing room entities for this plot
    DespawnRoomForPlot(plotIndex);

    // --- DB2 lookups for room component data ---

    // 1. HouseRoom → RoomWmoDataID
    HouseRoomEntry const* houseRoomEntry = sHouseRoomStore.LookupEntry(HOUSE_ROOM_ID);
    int32 roomWmoDataID = houseRoomEntry ? houseRoomEntry->RoomWmoDataID : 0;

    // 2. RoomWmoData → Geobox bounds (bounding box for OutsidePlotBounds check)
    //    Sniff fallback: (-35,-30,-1.01)→(35,30,125.01)
    float geoMinX = -35.0f, geoMinY = -30.0f, geoMinZ = -1.01f;
    float geoMaxX =  35.0f, geoMaxY =  30.0f, geoMaxZ = 125.01f;
    RoomWmoDataEntry const* wmoData = roomWmoDataID ? sRoomWmoDataStore.LookupEntry(roomWmoDataID) : nullptr;
    if (wmoData)
    {
        geoMinX = wmoData->BoundingBoxMinX;
        geoMinY = wmoData->BoundingBoxMinY;
        geoMinZ = wmoData->BoundingBoxMinZ;
        geoMaxX = wmoData->BoundingBoxMaxX;
        geoMaxY = wmoData->BoundingBoxMaxY;
        geoMaxZ = wmoData->BoundingBoxMaxZ;
    }
    else
    {
        TC_LOG_WARN("housing", "HousingMap::SpawnRoomForPlot: No RoomWmoData for roomWmoDataID={} "
            "(plot {}), using fallback geobox (-35,-30,-1.01)->(35,30,125.01)",
            roomWmoDataID, plotIndex);
    }

    // 3. RoomComponent → ModelFileDataID, Type
    //    Sniff fallback: FileDataID=6322976, Type=2
    int32 fileDataID = 6322976;
    uint8 roomComponentType = 2;
    RoomComponentEntry const* compEntry = sRoomComponentStore.LookupEntry(ROOM_COMPONENT_ID);
    if (compEntry)
    {
        if (compEntry->ModelFileDataID > 0)
            fileDataID = compEntry->ModelFileDataID;
        roomComponentType = compEntry->Type;
    }

    // 4. RoomComponentOption → theme-specific cosmetic data (varies by faction/house style)
    //    Alliance sniff: optionID=874, themeID=6, field24=1, textureID=3
    //    Horde sniff:    optionID=420, themeID=2, field24=2, textureID=40
    //    These are cosmetic only (don't affect Geobox/bounds check).
    int32 roomComponentOptionID = 0;
    int32 houseThemeID = 0;
    int32 roomComponentTextureID = 0;
    int32 field24 = 0;

    // Use faction-aware theme lookup
    int32 factionThemeID = _neighborhood
        ? sHousingMgr.GetFactionDefaultThemeID(_neighborhood->GetFactionRestriction())
        : 6;
    RoomComponentOptionEntry const* optEntry = sHousingMgr.FindRoomComponentOption(ROOM_COMPONENT_ID, factionThemeID);
    if (optEntry)
    {
        roomComponentOptionID = static_cast<int32>(optEntry->ID);
        houseThemeID = optEntry->HouseThemeID;
        field24 = static_cast<int32>(optEntry->SubType);
    }
    // If no DB2 entry found, use sniff-verified alliance defaults
    if (roomComponentOptionID == 0)
    {
        roomComponentOptionID = 874;
        houseThemeID = 6;
        field24 = 1;
        roomComponentTextureID = 3;
    }

    TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: plot={} DB2 lookup: "
        "roomWmoDataID={} geobox=({:.2f},{:.2f},{:.2f})→({:.2f},{:.2f},{:.2f}) "
        "fileDataID={} compType={} optionID={} themeID={} field24={} textureID={}",
        plotIndex, roomWmoDataID,
        geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ,
        fileDataID, roomComponentType,
        roomComponentOptionID, houseThemeID, field24, roomComponentTextureID);

    // --- Create entities ---

    // 1. Create room entity (MeshObject with FHousingRoom_C + Tag_HousingRoom fragments)
    MeshObject* roomEntity = MeshObject::CreateMeshObject(this, housePos, houseRot, 1.0f,
        fileDataID, /*isWMO*/ true,
        ObjectGuid::Empty, /*attachFlags*/ 3, nullptr);

    if (!roomEntity)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to create room entity for plot {}", plotIndex);
        return;
    }

    PhasingHandler::InitDbPhaseShift(roomEntity->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    roomEntity->InitHousingRoomData(houseGuid, HOUSE_ROOM_ID, ROOM_FLAGS, /*floorIndex*/ 0);

    // 2. Create room component MeshObject (with Geobox for OutsidePlotBounds check)
    Position componentPos(0.0f, 0.0f, 0.0f, 0.0f);
    QuaternionData componentRot;
    componentRot.x = 0.0f;
    componentRot.y = 0.0f;
    componentRot.z = 0.0f;
    componentRot.w = 1.0f;

    MeshObject* componentMesh = MeshObject::CreateMeshObject(this, componentPos, componentRot, 1.0f,
        fileDataID, /*isWMO*/ true,
        roomEntity->GetGUID(), /*attachFlags*/ 3, &housePos);

    if (!componentMesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to create room component mesh for plot {}", plotIndex);
        delete roomEntity;
        return;
    }

    PhasingHandler::InitDbPhaseShift(componentMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    componentMesh->InitHousingRoomComponentData(roomEntity->GetGUID(),
        roomComponentOptionID, ROOM_COMPONENT_ID,
        roomComponentType, field24,
        houseThemeID, roomComponentTextureID,
        /*roomComponentTypeParam*/ 0,
        geoMinX, geoMinY, geoMinZ,
        geoMaxX, geoMaxY, geoMaxZ);

    // 3. Link: add component GUID to room entity's MeshObjects array
    roomEntity->AddRoomMeshObject(componentMesh->GetGUID());

    // 4. Add both to map (room entity first since component references it)
    if (!AddToMap(roomEntity))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to add room entity to map for plot {}", plotIndex);
        delete roomEntity;
        delete componentMesh;
        return;
    }

    if (!AddToMap(componentMesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: Failed to add room component mesh to map for plot {}", plotIndex);
        roomEntity->AddObjectToRemoveList();
        delete componentMesh;
        return;
    }

    _roomEntities[plotIndex] = roomEntity->GetGUID();
    _roomComponentMeshes[plotIndex] = componentMesh->GetGUID();

    TC_LOG_ERROR("housing", "HousingMap::SpawnRoomForPlot: plot={} room={} component={} "
        "at ({:.1f},{:.1f},{:.1f}) geobox=({:.2f},{:.2f},{:.2f})→({:.2f},{:.2f},{:.2f})",
        plotIndex, roomEntity->GetGUID().ToString(), componentMesh->GetGUID().ToString(),
        housePos.GetPositionX(), housePos.GetPositionY(), housePos.GetPositionZ(),
        geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ);
}

void HousingMap::DespawnRoomForPlot(uint8 plotIndex)
{
    auto roomItr = _roomEntities.find(plotIndex);
    if (roomItr != _roomEntities.end())
    {
        if (MeshObject* mesh = GetMeshObject(roomItr->second))
            mesh->AddObjectToRemoveList();
        _roomEntities.erase(roomItr);
    }

    auto compItr = _roomComponentMeshes.find(plotIndex);
    if (compItr != _roomComponentMeshes.end())
    {
        if (MeshObject* mesh = GetMeshObject(compItr->second))
            mesh->AddObjectToRemoveList();
        _roomComponentMeshes.erase(compItr);
    }
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
    int32 exteriorComponentID, int32 houseExteriorWmoDataID,
    int32 factionRestriction /*= NEIGHBORHOOD_FACTION_ALLIANCE*/)
{
    // === DATA-DRIVEN EXTERIOR SPAWNING ===
    // Try to build the house from DB2 ExteriorComponent tree first.
    // If the data-driven approach yields 0 meshes, fall back to hardcoded methods.

    uint32 coreExtCompID = static_cast<uint32>(exteriorComponentID);
    int32 groupID = sHousingMgr.GetGroupForComponent(coreExtCompID);

    if (groupID != 0)
    {
        // Find all root components in this group (those with HookID <= 0 are roots)
        std::vector<uint32> const* groupComps = sHousingMgr.GetComponentsInGroup(groupID);
        if (groupComps && !groupComps->empty())
        {
            uint32 totalSpawned = 0;
            for (uint32 compID : *groupComps)
            {
                ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(compID);
                if (!comp || comp->HookID > 0) // skip children — they'll be spawned recursively
                    continue;

                totalSpawned += SpawnExtCompTree(plotIndex, compID,
                    housePos, houseRot,
                    houseGuid, houseExteriorWmoDataID,
                    ObjectGuid::Empty, nullptr);
            }

            if (totalSpawned > 0)
            {
                TC_LOG_INFO("housing", "HousingMap::SpawnFullHouseMeshObjects: Data-driven spawn "
                    "for plot {} group {} — {} MeshObjects (faction={})",
                    plotIndex, groupID, totalSpawned,
                    factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");
                return;
            }

            TC_LOG_WARN("housing", "HousingMap::SpawnFullHouseMeshObjects: Data-driven spawn "
                "yielded 0 meshes for plot {} group {} — falling back to hardcoded",
                plotIndex, groupID);
        }
    }
    else
    {
        TC_LOG_DEBUG("housing", "HousingMap::SpawnFullHouseMeshObjects: No group found for "
            "exteriorComponentID {} — using hardcoded spawn for plot {}",
            exteriorComponentID, plotIndex);
    }

    // === HARDCODED FALLBACK ===
    if (factionRestriction == NEIGHBORHOOD_FACTION_HORDE)
    {
        SpawnHordeHouseMeshObjects(plotIndex, housePos, houseRot, houseGuid,
            exteriorComponentID, houseExteriorWmoDataID);
        return;
    }

    // === ALLIANCE EXTERIOR (Stucco Small) ===
    // Sniff-verified: Alliance starter house consists of 10 structural MeshObjects.
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

    TC_LOG_DEBUG("housing", "HousingMap::SpawnFullHouseMeshObjects: Spawned {} alliance MeshObjects for plot {} in neighborhood '{}' "
        "(base={} door={})",
        meshCount, plotIndex, _neighborhood ? _neighborhood->GetName() : "?",
        basePiece ? "OK" : "FAIL", doorPiece ? "OK" : "FAIL");
}

void HousingMap::SpawnHordeHouseMeshObjects(uint8 plotIndex, Position const& housePos,
    QuaternionData const& houseRot, ObjectGuid houseGuid,
    int32 /*exteriorComponentID*/, int32 /*houseExteriorWmoDataID*/)
{
    // === HORDE EXTERIOR ===
    // Sniff-verified: Horde starter house with HouseExteriorWmoDataID=87.
    // Two root pieces at the house position, children with local-space offsets.
    //
    // Parent-child hierarchy:
    //   Root 0 (main structure, 7118906) - ExteriorComponentID 3811, type 10
    //     ├── Child: Door/entrance (7118912), hookID 17245, extCompID 976
    //     ├── Child: Wall element (7460531), hookID -1, extCompID 2476
    //     ├── Child: Wall variant (7118901), hookID -1, extCompID 1011
    //     ├── Child: Roof piece A (7462686), hookID 17294, extCompID 2445
    //     ├── Child: Structure detail (7118918), hookID 17286, extCompID 980
    //     └── Child: Roof piece B (7462686), hookID 17285, extCompID 2445
    //   Root 1 (base, 6648685) - ExteriorComponentID 1003, type 9

    int32 hordeWmoDataID = HORDE_HOUSE_EXTERIOR_WMO_DATA_ID; // 87

    // Root piece 0: Main structure
    MeshObject* rootPiece = SpawnHouseMeshObject(plotIndex, 7118906, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 3811, hordeWmoDataID,
        /*exteriorComponentType*/ 10, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Root piece 1: Base structure
    MeshObject* basePiece = SpawnHouseMeshObject(plotIndex, 6648685, /*isWMO*/ true,
        housePos, houseRot, 1.0f,
        houseGuid, 1003, hordeWmoDataID,
        /*exteriorComponentType*/ 9, /*houseSize*/ 2, /*hookID*/ -1,
        ObjectGuid::Empty, /*attachFlags*/ 0);

    // Children of root piece 0
    if (rootPiece)
    {
        ObjectGuid rootGuid = rootPiece->GetGUID();

        // Door/entrance
        SpawnHouseMeshObject(plotIndex, 7118912, /*isWMO*/ true,
            Position(14.2722f, -8.6194f, 0.0f, 0.0f),
            QuaternionData(0.0f, 0.0f, -0.2873478f, 0.9578263f), 1.0f,
            houseGuid, 976, hordeWmoDataID,
            /*exteriorComponentType*/ 11, /*houseSize*/ 2, /*hookID*/ 17245,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Wall element
        SpawnHouseMeshObject(plotIndex, 7460531, /*isWMO*/ true,
            Position(0.0f, 0.0f, 0.0f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 2476, hordeWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ -1,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Wall variant
        SpawnHouseMeshObject(plotIndex, 7118901, /*isWMO*/ true,
            Position(0.0f, 0.0f, 0.0f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.0f, 1.0f), 1.0f,
            houseGuid, 1011, hordeWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ -1,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Roof piece A (right side)
        SpawnHouseMeshObject(plotIndex, 7462686, /*isWMO*/ true,
            Position(6.2889f, -4.4556f, 0.0833f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.95782566f, 0.28735f), 1.0f,
            houseGuid, 2445, hordeWmoDataID,
            /*exteriorComponentType*/ 13, /*houseSize*/ 2, /*hookID*/ 17294,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Structure detail
        SpawnHouseMeshObject(plotIndex, 7118918, /*isWMO*/ true,
            Position(-0.1389f, 8.6806f, 5.4139f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.7071066f, 0.70710695f), 1.0f,
            houseGuid, 980, hordeWmoDataID,
            /*exteriorComponentType*/ 12, /*houseSize*/ 2, /*hookID*/ 17286,
            rootGuid, /*attachFlags*/ 3, &housePos);

        // Roof piece B (left side)
        SpawnHouseMeshObject(plotIndex, 7462686, /*isWMO*/ true,
            Position(-7.0611f, -3.7361f, 0.0833f, 0.0f),
            QuaternionData(0.0f, 0.0f, 0.2873478f, 0.9578263f), 1.0f,
            houseGuid, 2445, hordeWmoDataID,
            /*exteriorComponentType*/ 13, /*houseSize*/ 2, /*hookID*/ 17285,
            rootGuid, /*attachFlags*/ 3, &housePos);
    }

    uint32 meshCount = 0;
    auto meshItr = _meshObjects.find(plotIndex);
    if (meshItr != _meshObjects.end())
        meshCount = static_cast<uint32>(meshItr->second.size());

    TC_LOG_DEBUG("housing", "HousingMap::SpawnHordeHouseMeshObjects: Spawned {} MeshObjects for plot {} in neighborhood '{}' "
        "(root={} base={})",
        meshCount, plotIndex, _neighborhood ? _neighborhood->GetName() : "?",
        rootPiece ? "OK" : "FAIL", basePiece ? "OK" : "FAIL");
}

uint32 HousingMap::SpawnExtCompTree(uint8 plotIndex, uint32 extCompID,
    Position const& pos, QuaternionData const& rot,
    ObjectGuid houseGuid, int32 houseExteriorWmoDataID,
    ObjectGuid parentGuid, Position const* worldPos, int32 depth /*= 0*/)
{
    if (depth > 10) // safety limit against infinite recursion
        return 0;

    ExteriorComponentEntry const* comp = sExteriorComponentStore.LookupEntry(extCompID);
    if (!comp)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnExtCompTree: ExteriorComponent {} not found", extCompID);
        return 0;
    }

    if (comp->FileDataID <= 0)
    {
        TC_LOG_WARN("housing", "HousingMap::SpawnExtCompTree: ExteriorComponent {} has no FileDataID", extCompID);
        return 0;
    }

    // Determine attach flags: root pieces (no parent) use 0, children use 3
    uint8 attachFlags = parentGuid.IsEmpty() ? 0 : 3;

    MeshObject* mesh = SpawnHouseMeshObject(plotIndex, comp->FileDataID, /*isWMO*/ true,
        pos, rot, 1.0f,
        houseGuid, static_cast<int32>(extCompID), houseExteriorWmoDataID,
        comp->Type, /*houseSize*/ 2, comp->HookID,
        parentGuid, attachFlags, worldPos);

    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnExtCompTree: Failed to spawn mesh for comp {} "
            "(fileDataID={}) at depth {}",
            extCompID, comp->FileDataID, depth);
        return 0;
    }

    uint32 count = 1;
    ObjectGuid meshGuid = mesh->GetGUID();

    // Use worldPos for children: root's worldPos is itself, child's is the root's position
    Position const* childWorldPos = worldPos ? worldPos : &pos;

    // Recurse into hooks on this component
    auto const* hooks = sHousingMgr.GetHooksOnComponent(extCompID);
    if (hooks)
    {
        for (ExteriorComponentHookEntry const* hook : *hooks)
        {
            if (!hook)
                continue;

            // Find what component attaches at this hook
            ExteriorComponentEntry const* childComp = sHousingMgr.GetComponentAtHook(static_cast<int32>(hook->ID));
            if (!childComp)
                continue;

            // Hook position/rotation are local-space offsets relative to the parent
            Position hookPos(hook->Position[0], hook->Position[1], hook->Position[2], 0.0f);
            QuaternionData hookRot;
            // Hook rotation is in degrees — convert to quaternion
            static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
            float rx = hook->Rotation[0] * DEG_TO_RAD;
            float ry = hook->Rotation[1] * DEG_TO_RAD;
            float rz = hook->Rotation[2] * DEG_TO_RAD;
            hookRot.x = std::sin(rx / 2.0f) * std::cos(ry / 2.0f) * std::cos(rz / 2.0f);
            hookRot.y = std::cos(rx / 2.0f) * std::sin(ry / 2.0f) * std::cos(rz / 2.0f);
            hookRot.z = std::cos(rx / 2.0f) * std::cos(ry / 2.0f) * std::sin(rz / 2.0f);
            hookRot.w = std::cos(rx / 2.0f) * std::cos(ry / 2.0f) * std::cos(rz / 2.0f);

            count += SpawnExtCompTree(plotIndex, childComp->ID,
                hookPos, hookRot,
                houseGuid, houseExteriorWmoDataID,
                meshGuid, childWorldPos, depth + 1);
        }
    }

    return count;
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
    // Despawn room entities, MeshObjects, then the GO
    DespawnRoomForPlot(plotIndex);
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
// Decor Management (all decor is MeshObject — sniff-verified)
// ============================================================

MeshObject* HousingMap::SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid)
{
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: No HouseDecorData for entry {} (decorGuid={})",
            decor.DecorEntryId, decor.Guid.ToString());
        return nullptr;
    }

    // Sniff-verified: ALL retail placed decor is MeshObject (never GO).
    // FHousingDecor_C on a GameObject crashes the client (same issue as FHousingFixture_C on GOs).
    // Determine FileDataID: prefer ModelFileDataID, fall back to GO template displayInfo.
    int32 fileDataID = decorData->ModelFileDataID;
    if (fileDataID <= 0 && decorData->GameObjectID > 0)
    {
        GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(
            static_cast<uint32>(decorData->GameObjectID));
        if (goTemplate)
        {
            GameObjectDisplayInfoEntry const* displayInfo =
                sGameObjectDisplayInfoStore.LookupEntry(goTemplate->displayId);
            if (displayInfo && displayInfo->FileDataID > 0)
                fileDataID = displayInfo->FileDataID;
        }

        if (fileDataID <= 0)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Cannot derive FileDataID for decor entry {} "
                "(GameObjectID={}, ModelFileDataID={}), skipping",
                decor.DecorEntryId, decorData->GameObjectID, decorData->ModelFileDataID);
            return nullptr;
        }

        TC_LOG_DEBUG("housing", "HousingMap::SpawnDecorItem: Derived FileDataID={} from GameObjectID={} displayId for entry {}",
            fileDataID, decorData->GameObjectID, decor.DecorEntryId);
    }
    else if (fileDataID <= 0)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Decor entry {} has no ModelFileDataID and no GameObjectID, skipping",
            decor.DecorEntryId);
        return nullptr;
    }

    // Sniff-verified: Decor MeshObjects are attached to the plot's base room entity
    // (Housing/18) with attachFlags=3. Position is room-local space.
    // Get the room entity for this plot (spawned by SpawnRoomForPlot).
    ObjectGuid roomEntityGuid = ObjectGuid::Empty;
    Position roomWorldPos;
    auto roomItr = _roomEntities.find(plotIndex);
    if (roomItr != _roomEntities.end())
    {
        roomEntityGuid = roomItr->second;
        if (MeshObject* roomEntity = GetMeshObject(roomEntityGuid))
            roomWorldPos = roomEntity->GetPosition();
    }

    float worldX = decor.PosX;
    float worldY = decor.PosY;
    float worldZ = decor.PosZ;
    LoadGrid(worldX, worldY);

    QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

    // Convert world position to room-local position if we have a room entity
    float localX = worldX;
    float localY = worldY;
    float localZ = worldZ;
    if (!roomEntityGuid.IsEmpty())
    {
        localX = worldX - roomWorldPos.GetPositionX();
        localY = worldY - roomWorldPos.GetPositionY();
        localZ = worldZ - roomWorldPos.GetPositionZ();
    }

    Position localPos(localX, localY, localZ);
    Position worldPos(worldX, worldY, worldZ);

    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, 1.0f,
        fileDataID, /*isWMO*/ false,
        roomEntityGuid, /*attachFlags*/ roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3),
        &worldPos);

    if (!mesh)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to create decor MeshObject fileDataID={} for decor {}",
            fileDataID, decor.Guid.ToString());
        return nullptr;
    }

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid);

    if (!AddToMap(mesh))
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnDecorItem: Failed to add decor MeshObject to map for decor {}", decor.Guid.ToString());
        delete mesh;
        return nullptr;
    }

    _decorGameObjects[plotIndex].push_back(mesh->GetGUID());
    _decorGuidToGoGuid[decor.Guid] = mesh->GetGUID();
    _decorGuidToPlotIndex[decor.Guid] = plotIndex;

    TC_LOG_INFO("housing", "HousingMap::SpawnDecorItem: Spawned decor MeshObject fileDataID={} meshGuid={} decorGuid={} "
        "at world({:.1f},{:.1f},{:.1f}) local({:.1f},{:.1f},{:.1f}) room={} plot={}",
        fileDataID, mesh->GetGUID().ToString(), decor.Guid.ToString(),
        worldX, worldY, worldZ, localX, localY, localZ,
        roomEntityGuid.ToString(), plotIndex);
    return mesh;
}

void HousingMap::DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    ObjectGuid objGuid = itr->second;
    if (MeshObject* mesh = GetMeshObject(objGuid))
        mesh->AddObjectToRemoveList();

    // Remove from tracking
    auto& plotDecor = _decorGameObjects[plotIndex];
    plotDecor.erase(std::remove(plotDecor.begin(), plotDecor.end(), objGuid), plotDecor.end());
    _decorGuidToGoGuid.erase(itr);
    _decorGuidToPlotIndex.erase(decorGuid);

    TC_LOG_DEBUG("housing", "HousingMap::DespawnDecorItem: Despawned decor MeshObject for decorGuid={} plot={}", decorGuid.ToString(), plotIndex);
}

void HousingMap::DespawnAllDecorForPlot(uint8 plotIndex)
{
    auto itr = _decorGameObjects.find(plotIndex);
    if (itr == _decorGameObjects.end())
        return;

    for (ObjectGuid const& objGuid : itr->second)
    {
        if (MeshObject* mesh = GetMeshObject(objGuid))
            mesh->AddObjectToRemoveList();
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

    TC_LOG_DEBUG("housing", "HousingMap::DespawnAllDecorForPlot: Despawned all decor MeshObjects for plot {}", plotIndex);
}

void HousingMap::SpawnAllDecorForPlot(uint8 plotIndex, Housing const* housing)
{
    if (!housing)
        return;

    if (_decorSpawnedPlots.count(plotIndex))
        return; // Already spawned

    ObjectGuid houseGuid = housing->GetHouseGuid();
    uint32 spawnCount = 0;
    uint32 exteriorCount = 0;
    uint32 failCount = 0;
    for (auto const& [decorGuid, decor] : housing->GetPlacedDecorMap())
    {
        // Skip interior decor — those are spawned by HouseInteriorMap::SpawnInteriorDecor
        if (!decor.RoomGuid.IsEmpty())
            continue;

        ++exteriorCount;
        MeshObject* mesh = SpawnDecorItem(plotIndex, decor, houseGuid);
        if (mesh)
            ++spawnCount;
        else
            ++failCount;
    }

    _decorSpawnedPlots.insert(plotIndex);

    TC_LOG_ERROR("housing", "HousingMap::SpawnAllDecorForPlot: Spawned {}/{} exterior decor for plot {} "
        "(failed={}, neighborhood='{}')",
        spawnCount, exteriorCount, plotIndex, failCount,
        _neighborhood ? _neighborhood->GetName() : "?");
}

void HousingMap::UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& /*rot*/)
{
    auto itr = _decorGuidToGoGuid.find(decorGuid);
    if (itr == _decorGuidToGoGuid.end())
        return;

    // All decor is MeshObject now
    if (MeshObject* mesh = GetMeshObject(itr->second))
    {
        mesh->Relocate(pos);
        TC_LOG_DEBUG("housing", "HousingMap::UpdateDecorPosition: Moved decor MeshObject {} to ({:.1f}, {:.1f}, {:.1f}) for plot {}",
            decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), plotIndex);
    }
}
