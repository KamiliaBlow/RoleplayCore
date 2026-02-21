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
#include "AreaTrigger.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "Housing.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectGridLoader.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "RealmList.h"
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
    // Sniff analysis: Value 0 = unoccupied/for-sale, Value 1 = occupied/has-house
    uint32 wsSetCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->WorldState != 0)
        {
            Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
            bool isOccupied = plotInfo && !plotInfo->OwnerGuid.IsEmpty();

            int32 wsValue = isOccupied ? 1 : 0;
            sWorldStateMgr->SetValue(plot->WorldState, wsValue, false, this);
            ++wsSetCount;
        }
    }

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Spawned {} GOs, {} ATs, set {} WorldStates for {} plots in neighborhood '{}' (noEntry={})",
        goCount, uint32(_plotAreaTriggers.size()), wsSetCount, uint32(plots.size()), _neighborhood->GetName(), noEntryCount);

    // Spawn house structure GOs for owned plots
    uint32 houseCount = 0;
    for (NeighborhoodPlotData const* plot : plots)
    {
        uint8 plotIdx = static_cast<uint8>(plot->PlotIndex);
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(plotIdx);
        if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
            continue;

        // Try to get persisted position from the player's Housing object (if online)
        Housing* housing = GetHousingForPlayer(plotInfo->OwnerGuid);
        if (housing && housing->HasCustomPosition())
        {
            Position customPos = housing->GetHousePosition();
            SpawnHouseForPlot(plotIdx, &customPos);
        }
        else
        {
            SpawnHouseForPlot(plotIdx); // DB2 default position
        }
        ++houseCount;

        // Spawn decor GOs if the player's Housing data is loaded
        if (housing)
            SpawnAllDecorForPlot(plotIdx, housing);
    }

    if (houseCount > 0)
        TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Spawned {} house GOs for owned plots in neighborhood '{}'",
            houseCount, _neighborhood->GetName());
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

    // Update the per-plot WorldState (0 = unoccupied, 1 = occupied)
    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
    for (NeighborhoodPlotData const* plotData : plots)
    {
        if (plotData->PlotIndex == static_cast<int32>(plotIndex))
        {
            if (plotData->WorldState != 0)
                sWorldStateMgr->SetValue(plotData->WorldState, owned ? 1 : 0, false, this);
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
    Housing* housing = player->GetHousingForNeighborhood(_neighborhood->GetGuid());
    if (!housing)
    {
        // Fallback: check if any of the player's housings has a plot in this neighborhood
        for (Housing const* h : player->GetAllHousings())
        {
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
        AddPlayerHousing(player->GetGUID(), housing);

        // Update PlayerMirrorHouse.MapID so the client knows this house is on the current map.
        // Without this, MapID stays at 0 (set during login) and the client rejects
        // edit mode with HOUSING_RESULT_ACTION_LOCKED_BY_COMBAT (error 1 = first non-success code).
        player->UpdateHousingMapId(housing->GetHouseGuid(), static_cast<int32>(GetId()));

        // Spawn house GO if not already present (handles offline → online transition)
        uint8 plotIdx = housing->GetPlotIndex();
        if (_houseGameObjects.find(plotIdx) == _houseGameObjects.end())
        {
            if (housing->HasCustomPosition())
            {
                Position customPos = housing->GetHousePosition();
                SpawnHouseForPlot(plotIdx, &customPos);
            }
            else
                SpawnHouseForPlot(plotIdx);
        }

        // Spawn decor GOs if not already spawned for this plot
        SpawnAllDecorForPlot(plotIdx, housing);
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

GameObject* HousingMap::SpawnHouseForPlot(uint8 plotIndex, Position const* customPos /*= nullptr*/)
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

    // PlotGameObjectID is the plot marker (For Sale sign), NOT the house structure.
    // Use the default house structure GO (574432) for now.
    // TODO: Look up house GO entry from HouseExteriorWmo based on house type when data is available.
    uint32 goEntry = 574432; // Housing - Generic - Ground WMO (type 43, displayId 113521)

    Position pos(x, y, z, facing);
    GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, GO_STATE_ACTIVE);
    if (!go)
    {
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Failed to create house GO entry {} at ({}, {}, {}) for plot {}",
            goEntry, x, y, z, plotIndex);
        return nullptr;
    }

    // Retail sniff: house GOs have Flags=32 (GO_FLAG_NODESPAWN) | GO_FLAG_MAP_OBJECT
    go->SetFlag(GO_FLAG_NODESPAWN | GO_FLAG_MAP_OBJECT);

    if (!AddToMap(go))
    {
        delete go;
        TC_LOG_ERROR("housing", "HousingMap::SpawnHouseForPlot: Failed to add house GO to map for plot {}", plotIndex);
        return nullptr;
    }

    _houseGameObjects[plotIndex] = go->GetGUID();

    TC_LOG_INFO("housing", "HousingMap::SpawnHouseForPlot: Spawned house GO entry={} guid={} at ({:.1f}, {:.1f}, {:.1f}) for plot {} in neighborhood '{}'",
        goEntry, go->GetGUID().ToString(), x, y, z, plotIndex, _neighborhood->GetName());

    return go;
}

void HousingMap::DespawnHouseForPlot(uint8 plotIndex)
{
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
