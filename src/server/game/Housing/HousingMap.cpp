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
#include "Housing.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectGuid.h"
#include "World.h"

HousingMap::HousingMap(uint32 id, time_t expiry, uint32 instanceId, Difficulty spawnMode, uint32 neighborhoodId)
    : Map(id, expiry, instanceId, spawnMode), _neighborhoodId(neighborhoodId), _neighborhood(nullptr)
{
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

Housing* HousingMap::GetHousingForPlayer(ObjectGuid playerGuid) const
{
    auto itr = _playerHousings.find(playerGuid);
    if (itr != _playerHousings.end())
        return itr->second;

    return nullptr;
}

void HousingMap::LoadNeighborhoodData()
{
    ObjectGuid neighborhoodGuid = ObjectGuid::Create<HighGuid::Uniq>(static_cast<uint64>(_neighborhoodId));
    _neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);

    if (!_neighborhood)
        TC_LOG_ERROR("maps", "HousingMap::LoadNeighborhoodData: Failed to load neighborhood {} for map {} instanceId {}",
            _neighborhoodId, GetId(), GetInstanceId());
    else
        TC_LOG_DEBUG("maps", "HousingMap::LoadNeighborhoodData: Loaded neighborhood '{}' (id: {}) for map {} instanceId {}",
            _neighborhood->GetName(), _neighborhoodId, GetId(), GetInstanceId());
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
