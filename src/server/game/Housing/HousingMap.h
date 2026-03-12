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

#include "Housing.h"
#include "Map.h"
#include <unordered_set>

class AreaTrigger;
class Housing;
class MeshObject;
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
    AreaTrigger* GetPlotAreaTrigger(uint8 plotIndex);
    GameObject* GetPlotGameObject(uint8 plotIndex);
    void SetPlotOwnershipState(uint8 plotIndex, bool owned);
    HousingPlotOwnerType GetPlotOwnerTypeForPlayer(Player const* player, uint8 plotIndex) const;
    void SendPerPlayerPlotWorldStates(Player* player);
    Neighborhood* GetNeighborhood() const { return _neighborhood; }
    uint32 GetNeighborhoodId() const { return _neighborhoodId; }

    void LoadNeighborhoodData();
    void SpawnPlotGameObjects();
    void LockPlotGrids();

    // Player housing instance tracking
    void AddPlayerHousing(ObjectGuid playerGuid, Housing* housing);
    void RemovePlayerHousing(ObjectGuid playerGuid);

    // House structure GO management
    // Sniff-verified defaults: ExteriorComponentID=141 (Stucco Base), HouseExteriorWmoDataID=9 (Human theme)
    GameObject* SpawnHouseForPlot(uint8 plotIndex, Position const* customPos = nullptr,
        int32 exteriorComponentID = 141, int32 houseExteriorWmoDataID = 9);
    void DespawnHouseForPlot(uint8 plotIndex);
    GameObject* GetHouseGameObject(uint8 plotIndex);
    int8 GetPlotIndexForHouseGO(ObjectGuid goGuid) const;
    uint32 GetHouseGameObjectCount() const { return static_cast<uint32>(_houseGameObjects.size()); }

    // MeshObject management (housing fixture rendering)
    // pos: local-space position for child pieces (or world position for root pieces)
    // worldPos: if non-null, used for server-side grid placement (child pieces must be in parent's grid cell)
    MeshObject* SpawnHouseMeshObject(uint8 plotIndex, int32 fileDataID, bool isWMO,
        Position const& pos, QuaternionData const& rot, float scale,
        ObjectGuid houseGuid, int32 exteriorComponentID, int32 houseExteriorWmoDataID,
        uint8 exteriorComponentType = 9, uint8 houseSize = 2, int32 exteriorComponentHookID = -1,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 0,
        Position const* worldPos = nullptr);
    void SpawnFullHouseMeshObjects(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid,
        int32 exteriorComponentID, int32 houseExteriorWmoDataID,
        int32 factionRestriction = NEIGHBORHOOD_FACTION_ALLIANCE);
    void SpawnHordeHouseMeshObjects(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid,
        int32 exteriorComponentID, int32 houseExteriorWmoDataID);
    uint32 SpawnExtCompTree(uint8 plotIndex, uint32 extCompID,
        Position const& pos, QuaternionData const& rot,
        ObjectGuid houseGuid, int32 houseExteriorWmoDataID,
        ObjectGuid parentGuid, Position const* worldPos, int32 depth = 0);
    void DespawnAllMeshObjectsForPlot(uint8 plotIndex);

    // Room entity management (provides Geobox for client OutsidePlotBounds check)
    void SpawnRoomForPlot(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid);
    void DespawnRoomForPlot(uint8 plotIndex);

    // Decor management (all decor is MeshObject — sniff-verified, never GO)
    MeshObject* SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid);
    void DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid);
    void DespawnAllDecorForPlot(uint8 plotIndex);
    void SpawnAllDecorForPlot(uint8 plotIndex, Housing const* housing);
    void UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot);

    // Track which plot a player is currently visiting (set by at_housing_plot)
    void SetPlayerCurrentPlot(ObjectGuid playerGuid, uint8 plotIndex) { _playerCurrentPlot[playerGuid] = plotIndex; }
    void ClearPlayerCurrentPlot(ObjectGuid playerGuid) { _playerCurrentPlot.erase(playerGuid); }
    int8 GetPlayerCurrentPlot(ObjectGuid playerGuid) const
    {
        auto itr = _playerCurrentPlot.find(playerGuid);
        return itr != _playerCurrentPlot.end() ? static_cast<int8>(itr->second) : -1;
    }

    // Accessor for diagnostic logging (decor GUID → MeshObject GUID map)
    std::unordered_map<ObjectGuid, ObjectGuid> const& GetDecorGuidMap() const { return _decorGuidToGoGuid; }

    // Accessor for fixture MeshObjects (plotIndex → vector of MeshObject GUIDs)
    std::unordered_map<uint8, std::vector<ObjectGuid>> const& GetPlotMeshObjects() const { return _meshObjects; }

    // Manual spell packet helpers — called from AddPlayerToMap and at_housing_plot AT script.
    // These spells don't exist in DB2, so CastSpell() silently fails; manual packets are required.
    void SendPostTutorialAuras(Player* player);
    void SendPlotEnterSpellPackets(Player* player, uint8 plotIndex);
    void SendPlotLeaveAuraRemoval(Player* player);

private:
    uint32 _neighborhoodId;
    Neighborhood* _neighborhood;
    std::unordered_map<ObjectGuid, Housing*> _playerHousings;
    std::unordered_map<uint8, ObjectGuid> _plotAreaTriggers;
    std::unordered_map<uint8, ObjectGuid> _plotGameObjects;

    // House structure GO tracking (plotIndex -> house GO GUID)
    std::unordered_map<uint8, ObjectGuid> _houseGameObjects;

    // MeshObject tracking (plotIndex -> vector of MeshObject GUIDs)
    std::unordered_map<uint8, std::vector<ObjectGuid>> _meshObjects;

    // Room entity tracking (plotIndex -> room/component MeshObject GUIDs)
    std::unordered_map<uint8, ObjectGuid> _roomEntities;        // room "entity" MeshObject
    std::unordered_map<uint8, ObjectGuid> _roomComponentMeshes; // room component MeshObject (has Geobox)

    // Decor GO tracking
    std::unordered_map<uint8, std::vector<ObjectGuid>> _decorGameObjects;         // plotIndex -> decor GO GUIDs
    std::unordered_map<ObjectGuid, ObjectGuid> _decorGuidToGoGuid;                // decor GUID -> GO GUID
    std::unordered_map<ObjectGuid, uint8> _decorGuidToPlotIndex;                  // decor GUID -> plotIndex
    std::unordered_set<uint8> _decorSpawnedPlots;                                 // plots whose decor has been spawned
    std::unordered_map<ObjectGuid, uint8> _playerCurrentPlot;                    // player GUID -> current visited plot index
};

#endif // HousingMap_h__
