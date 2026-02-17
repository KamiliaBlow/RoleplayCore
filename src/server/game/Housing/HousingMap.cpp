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

    if (!_neighborhood)
        return;

    uint32 neighborhoodMapId = _neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    uint32 goCount = 0;

    for (NeighborhoodPlotData const* plot : plots)
    {
        float x = plot->CornerstonePosition[0];
        float y = plot->CornerstonePosition[1];
        float z = plot->CornerstonePosition[2];

        // Check if this plot's cornerstone position falls in the current cell
        CellCoord cellCoord = Trinity::ComputeCellCoord(x, y);
        if (cellCoord != cell.GetCellCoord())
            continue;

        // Determine which GO to spawn: owned plot → CornerstoneGameObjectID, empty → PlotGameObjectID ("For Sale Sign")
        Neighborhood::PlotInfo const* plotInfo = _neighborhood->GetPlotInfo(static_cast<uint8>(plot->PlotIndex));
        uint32 goEntry = 0;

        if (plotInfo && !plotInfo->OwnerGuid.IsEmpty())
            goEntry = static_cast<uint32>(plot->CornerstoneGameObjectID);
        else
            goEntry = static_cast<uint32>(plot->PlotGameObjectID);

        if (!goEntry)
            continue;

        // Build rotation from the stored euler angles
        float rotZ = plot->CornerstoneRotation[2];
        QuaternionData rot = QuaternionData::fromEulerAnglesZYX(rotZ, plot->CornerstoneRotation[1], plot->CornerstoneRotation[0]);

        Position pos(x, y, z, rotZ);
        GameObject* go = GameObject::CreateGameObject(goEntry, this, pos, rot, 255, GO_STATE_READY);
        if (!go)
        {
            TC_LOG_ERROR("maps", "HousingMap::LoadGridObjects: Failed to create GO entry {} at ({}, {}, {}) for plot {} in neighborhood '{}'",
                goEntry, x, y, z, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        if (!AddToMap(go))
        {
            delete go;
            TC_LOG_ERROR("maps", "HousingMap::LoadGridObjects: Failed to add GO entry {} to map for plot {} in neighborhood '{}'",
                goEntry, plot->PlotIndex, _neighborhood->GetName());
            continue;
        }

        ++goCount;
    }

    if (goCount > 0)
    {
        TC_LOG_DEBUG("maps", "HousingMap::LoadGridObjects: Spawned {} plot GOs for neighborhood '{}' in grid {} on map {}",
            goCount, _neighborhood->GetName(), grid->GetGridId(), GetId());
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
    ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Housing>(/*subType*/ 4, /*arg1*/ 0, /*arg2*/ 0, static_cast<uint64>(_neighborhoodId));
    _neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);

    if (!_neighborhood)
        TC_LOG_ERROR("maps", "HousingMap::LoadNeighborhoodData: Failed to load neighborhood {} for map {} instanceId {}",
            _neighborhoodId, GetId(), GetInstanceId());
    else
        TC_LOG_DEBUG("maps", "HousingMap::LoadNeighborhoodData: Loaded neighborhood '{}' (id: {}) for map {} instanceId {}",
            _neighborhood->GetName(), _neighborhoodId, GetId(), GetInstanceId());
}

bool HousingMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    if (!_neighborhood)
    {
        TC_LOG_ERROR("maps", "HousingMap::AddPlayerToMap: No neighborhood loaded for map {} instanceId {}",
            GetId(), GetInstanceId());
        return false;
    }

    // Auto-add player as neighborhood resident if not already a member
    if (!_neighborhood->IsMember(player->GetGUID()))
    {
        _neighborhood->AddResident(player->GetGUID());
        TC_LOG_DEBUG("maps", "HousingMap::AddPlayerToMap: Auto-added player {} as resident of neighborhood '{}'",
            player->GetGUID().ToString(), _neighborhood->GetName());
    }

    // Track player housing if they own a house in this neighborhood
    if (Housing* housing = player->GetHousing())
    {
        if (housing->GetNeighborhoodGuid() == _neighborhood->GetGuid())
            AddPlayerHousing(player->GetGUID(), housing);
    }

    return Map::AddPlayerToMap(player, initPlayer);
}

void HousingMap::RemovePlayerFromMap(Player* player, bool remove)
{
    RemovePlayerHousing(player->GetGUID());

    TC_LOG_DEBUG("maps", "HousingMap::RemovePlayerFromMap: Player {} leaving housing map {} instanceId {}",
        player->GetGUID().ToString(), GetId(), GetInstanceId());

    Map::RemovePlayerFromMap(player, remove);
}

void HousingMap::AddPlayerHousing(ObjectGuid playerGuid, Housing* housing)
{
    if (!housing)
    {
        TC_LOG_ERROR("maps", "HousingMap::AddPlayerHousing: Attempted to add null housing for player {} on map {} instanceId {}",
            playerGuid.ToString(), GetId(), GetInstanceId());
        return;
    }

    _playerHousings[playerGuid] = housing;

    TC_LOG_DEBUG("maps", "HousingMap::AddPlayerHousing: Added housing for player {} on map {} instanceId {} (total: {})",
        playerGuid.ToString(), GetId(), GetInstanceId(), static_cast<uint32>(_playerHousings.size()));
}

void HousingMap::RemovePlayerHousing(ObjectGuid playerGuid)
{
    auto itr = _playerHousings.find(playerGuid);
    if (itr != _playerHousings.end())
    {
        _playerHousings.erase(itr);

        TC_LOG_DEBUG("maps", "HousingMap::RemovePlayerHousing: Removed housing for player {} on map {} instanceId {} (remaining: {})",
            playerGuid.ToString(), GetId(), GetInstanceId(), static_cast<uint32>(_playerHousings.size()));
    }
    else
    {
        TC_LOG_DEBUG("maps", "HousingMap::RemovePlayerHousing: No housing found for player {} on map {} instanceId {}",
            playerGuid.ToString(), GetId(), GetInstanceId());
    }
}
