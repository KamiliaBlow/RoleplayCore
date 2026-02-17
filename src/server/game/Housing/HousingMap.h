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

#ifndef HousingMap_h__
#define HousingMap_h__

#include "Map.h"

class Housing;
class Neighborhood;

class Player;

class TC_GAME_API HousingMap : public Map
{
public:
    HousingMap(uint32 id, time_t expiry, uint32 instanceId, Difficulty spawnMode, uint32 neighborhoodId);
    ~HousingMap();

    void InitVisibilityDistance() override;
    void LoadGridObjects(NGridType* grid, Cell const& cell) override;
    bool AddPlayerToMap(Player* player, bool initPlayer = true) override;
    void RemovePlayerFromMap(Player* player, bool remove) override;

    Housing* GetHousingForPlayer(ObjectGuid playerGuid) const;
    Neighborhood* GetNeighborhood() const { return _neighborhood; }
    uint32 GetNeighborhoodId() const { return _neighborhoodId; }

    void LoadNeighborhoodData();

    // Player housing instance tracking
    void AddPlayerHousing(ObjectGuid playerGuid, Housing* housing);
    void RemovePlayerHousing(ObjectGuid playerGuid);

private:
    uint32 _neighborhoodId;
    Neighborhood* _neighborhood;
    std::unordered_map<ObjectGuid, Housing*> _playerHousings;
};

#endif // HousingMap_h__
