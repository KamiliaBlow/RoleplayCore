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

#ifndef HouseInteriorMap_h__
#define HouseInteriorMap_h__

#include "Map.h"
#include "ObjectGuid.h"
#include <vector>

class Housing;
class Player;

/// Map instance for a player's house interior (MAP_HOUSE_INTERIOR = 7, MapID 2783).
/// Each player/account gets their own instance of this map. The interior is a
/// WMO-based space with modular rooms that the player can customize.
/// Similar pattern to GarrisonMap but for housing interiors.
class TC_GAME_API HouseInteriorMap : public Map
{
public:
    HouseInteriorMap(uint32 id, time_t expiry, uint32 instanceId, ObjectGuid const& owner);

    void LoadGridObjects(NGridType* grid, Cell const& cell) override;
    void InitVisibilityDistance() override;
    bool AddPlayerToMap(Player* player, bool initPlayer = true) override;
    void RemovePlayerFromMap(Player* player, bool remove) override;

    ObjectGuid GetOwnerGuid() const { return _owner; }

    /// Get the Housing data for the owner (needed for room/decor state).
    Housing* GetOwnerHousing();

    /// The neighborhood map ID the owner came from (for exit teleport).
    uint32 GetSourceNeighborhoodMapId() const { return _sourceNeighborhoodMapId; }
    void SetSourceNeighborhoodMapId(uint32 mapId) { _sourceNeighborhoodMapId = mapId; }

    /// The plot index the owner's house is on (for exit teleport position).
    uint8 GetSourcePlotIndex() const { return _sourcePlotIndex; }
    void SetSourcePlotIndex(uint8 plotIndex) { _sourcePlotIndex = plotIndex; }

    /// Spawn all room meshes for the owner's house layout.
    /// Called once when the interior map is first populated.
    void SpawnRoomMeshObjects(Housing* housing);

    /// Despawn all room meshes (e.g., when the interior is rebuilt).
    void DespawnAllRoomMeshObjects();

private:
    ObjectGuid _owner;
    Player* _loadingPlayer; ///< @workaround Player not in ObjectAccessor during login
    uint32 _sourceNeighborhoodMapId;
    uint8 _sourcePlotIndex;
    bool _roomsSpawned = false;

    /// GUIDs of all spawned room MeshObjects, indexed by room GUID
    std::unordered_map<ObjectGuid /*roomGuid*/, std::vector<ObjectGuid>> _roomMeshObjects;
};

#endif // HouseInteriorMap_h__
