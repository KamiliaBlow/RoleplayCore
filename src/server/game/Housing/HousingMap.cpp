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
#include "AreaTrigger.h"
#include "GameObject.h"
#include "GridDefines.h"
#include "Housing.h"
#include "HousingMgr.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectGridLoader.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "World.h"

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

        // Determine which GO to spawn: owned plot -> CornerstoneGameObjectID, empty -> PlotGameObjectID ("For Sale Sign")
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
        uint32 goEntry = 0;

        if (plotInfo && !plotInfo->OwnerGuid.IsEmpty())
            goEntry = static_cast<uint32>(plot->CornerstoneGameObjectID);
        else
            goEntry = static_cast<uint32>(plot->PlotGameObjectID);

        TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Plot {} at ({:.1f}, {:.1f}, {:.1f}) -> goEntry={} (Cornerstone={}, ForSale={}, owned={})",
            plot->PlotIndex, x, y, z, goEntry, plot->CornerstoneGameObjectID, plot->PlotGameObjectID,
            (plotInfo && !plotInfo->OwnerGuid.IsEmpty()) ? "yes" : "no");

        if (!goEntry)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Plot {} has goEntry=0 (CornerstoneGO={}, PlotGO={}) - skipping",
                plot->PlotIndex, plot->CornerstoneGameObjectID, plot->PlotGameObjectID);
            ++noEntryCount;
            continue;
        }

        // Build rotation from the stored euler angles
        float rotZ = plot->CornerstoneRotation[2];
        QuaternionData rot = QuaternionData::fromEulerAnglesZYX(rotZ, plot->CornerstoneRotation[1], plot->CornerstoneRotation[0]);

        Position pos(x, y, z, rotZ);
        GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, GO_STATE_READY);
        if (!go)
        {
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create GO entry {} at ({}, {}, {}) for plot {} in neighborhood '{}'",
                goEntry, x, y, z, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        if (!AddToMap(go))
        {
            delete go;
            TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to add GO entry {} to map for plot {} in neighborhood '{}'",
                goEntry, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        // Track the plot GO for later swap (purchase/eviction)
        _plotGameObjects[static_cast<uint8>(plot->PlotIndex)] = go->GetGUID();

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

                TC_LOG_DEBUG("housing", "HousingMap::SpawnPlotGameObjects: Spawned plot AT for plot {} at ({}, {}, {}) in neighborhood '{}'",
                    plotIndex, atPos.GetPositionX(), atPos.GetPositionY(), atPos.GetPositionZ(), _neighborhood->GetName());
            }
            else
            {
                TC_LOG_ERROR("housing", "HousingMap::SpawnPlotGameObjects: Failed to create plot AT (entry 37358) for plot {} in neighborhood '{}'",
                    plotIndex, _neighborhood->GetName());
            }
        }
    }

    TC_LOG_INFO("housing", "HousingMap::SpawnPlotGameObjects: Spawned {} GOs and {} ATs for {} plots in neighborhood '{}' (noEntry={})",
        goCount, uint32(_plotAreaTriggers.size()), uint32(plots.size()), _neighborhood->GetName(), noEntryCount);
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

void HousingMap::SwapPlotGameObject(uint8 plotIndex, uint32 newGoEntry)
{
    if (!_neighborhood || !newGoEntry)
        return;

    // Find the DB2 plot data for position/rotation
    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    NeighborhoodPlotData const* plotData = nullptr;
    for (NeighborhoodPlotData const* p : plots)
    {
        if (p->PlotIndex == static_cast<int32>(plotIndex))
        {
            plotData = p;
            break;
        }
    }

    if (!plotData)
    {
        TC_LOG_ERROR("housing", "HousingMap::SwapPlotGameObject: Plot {} not found in DB2 data for neighborhood '{}'",
            plotIndex, _neighborhood->GetName());
        return;
    }

    // Remove the old GO if it exists
    auto itr = _plotGameObjects.find(plotIndex);
    if (itr != _plotGameObjects.end())
    {
        if (GameObject* oldGo = GetGameObject(itr->second))
            oldGo->AddObjectToRemoveList();

        _plotGameObjects.erase(itr);
    }

    // Create the new GO at the same cornerstone position/rotation
    float x = plotData->CornerstonePosition[0];
    float y = plotData->CornerstonePosition[1];
    float z = plotData->CornerstonePosition[2];
    float rotZ = plotData->CornerstoneRotation[2];
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(rotZ, plotData->CornerstoneRotation[1], plotData->CornerstoneRotation[0]);
    Position pos(x, y, z, rotZ);

    GameObject* newGo = GameObject::CreateGameObject(newGoEntry, this, pos, rot, 255, GO_STATE_READY);
    if (!newGo)
    {
        TC_LOG_ERROR("housing", "HousingMap::SwapPlotGameObject: Failed to create GO entry {} for plot {} in neighborhood '{}'",
            newGoEntry, plotIndex, _neighborhood->GetName());
        return;
    }

    if (!AddToMap(newGo))
    {
        delete newGo;
        TC_LOG_ERROR("housing", "HousingMap::SwapPlotGameObject: Failed to add GO entry {} to map for plot {} in neighborhood '{}'",
            newGoEntry, plotIndex, _neighborhood->GetName());
        return;
    }

    _plotGameObjects[plotIndex] = newGo->GetGUID();

    TC_LOG_DEBUG("housing", "HousingMap::SwapPlotGameObject: Swapped plot {} GO to entry {} in neighborhood '{}'",
        plotIndex, newGoEntry, _neighborhood->GetName());
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
    ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 0, static_cast<uint64>(_neighborhoodId));
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

    // Auto-add player as neighborhood resident if not already a member
    if (!_neighborhood->IsMember(player->GetGUID()))
    {
        _neighborhood->AddResident(player->GetGUID());
        TC_LOG_DEBUG("housing", "HousingMap::AddPlayerToMap: Auto-added player {} as resident of neighborhood '{}'",
            player->GetGUID().ToString(), _neighborhood->GetName());
    }

    // Track player housing if they own a house in this neighborhood
    if (Housing* housing = player->GetHousingForNeighborhood(_neighborhood->GetGuid()))
        AddPlayerHousing(player->GetGUID(), housing);

    return Map::AddPlayerToMap(player, initPlayer);
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
