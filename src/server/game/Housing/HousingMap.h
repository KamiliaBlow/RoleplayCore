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
#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class AreaTrigger;
class Housing;
class HousingMirrorEntity;
class HousingRoomEntity;
class MeshObject;
class Neighborhood;
class Player;
struct NeighborhoodPlotData;

class TC_GAME_API HousingMap : public Map
{
public:
    HousingMap(uint32 id, time_t expiry, uint32 instanceId, Difficulty spawnMode, uint32 neighborhoodId);
    ~HousingMap();

    void InitVisibilityDistance() override;
    void LoadGridObjects(NGridType* grid) override;
    bool AddPlayerToMap(Player* player, bool initPlayer = true) override;
    void RemovePlayerFromMap(Player* player, bool remove) override;

    Housing* GetHousingForPlayer(ObjectGuid playerGuid) const;
    AreaTrigger* GetPlotAreaTrigger(uint8 plotIndex);
    // Returns the plot index whose plot-bounds AT equals `atGuid`, or -1 when none.
    int8 GetPlotIndexForAreaTrigger(ObjectGuid atGuid) const;
    GameObject* GetPlotGameObject(uint8 plotIndex);
    void SetPlotOwnershipState(uint8 plotIndex, bool owned);
    AreaTrigger* SpawnPlotAreaTrigger(NeighborhoodPlotData const* plot);
    void DespawnPlotAreaTrigger(uint8 plotIndex);
    // Retail clears every world GameObject on a plot when it is bought and brings them back when freed.
    void SetPlotGroundCleared(NeighborhoodPlotData const* plot, bool cleared);
    // World position and yaw of a plot's room (its GameObjects.db2 plot object, turned half a revolution).
    bool GetPlotRoomFrame(uint8 plotIndex, Position& frame) const;
    bool IsSpawnSuppressed(SpawnObjectType type, ObjectGuid::LowType spawnId) const override;
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

    // Fixture override map: hookID → ExteriorComponentID from the player's fixture selections.
    using FixtureOverrideMap = std::unordered_map<uint32 /*hookID*/, uint32 /*extCompID*/>;

    // Root override map: componentType → componentID from the player's root fixture selections.
    using RootOverrideMap = std::unordered_map<uint8 /*componentType*/, uint32 /*compID*/>;

    // House structure GO management
    GameObject* SpawnHouseForPlot(uint8 plotIndex, Position const* customPos,
        int32 exteriorComponentID, int32 houseExteriorWmoDataID,
        FixtureOverrideMap const* fixtureOverrides = nullptr,
        RootOverrideMap const* rootOverrides = nullptr);
    void DespawnHouseForPlot(uint8 plotIndex);
    void DespawnDoorGO(uint8 plotIndex);
    GameObject* GetHouseGameObject(uint8 plotIndex);
    int8 GetPlotIndexForHouseGO(ObjectGuid goGuid) const;
    uint32 GetHouseGameObjectCount() const { return static_cast<uint32>(_houseGameObjects.size()); }

    // House-exterior root Entity attached to the plot room; base and roof meshes hang off it (FHousingPlayerHouse_C.EntityGUID).
    HousingRoomEntity* GetHouseRootEntity(uint8 plotIndex) const;
    ObjectGuid GetHouseMirrorGuid(uint8 plotIndex) const;
    // Deterministic mirror-GUID derivation usable before the mirror exists; pieceIndex 0 is the Type-9 root mirror.
    ObjectGuid MakeHouseMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex = 0) const;

    // "Group B" per-piece mirrors paired with each visible exterior fixture MeshObject (Type 9/10/11/12); used by the client's spatial index.
    HousingMirrorEntity* GetHouseMeshMirror(uint8 plotIndex) const;
    ObjectGuid GetHouseMeshMirrorGuid(uint8 plotIndex) const;
    ObjectGuid MakeHouseMeshMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex = 0) const;
    // Full list of per-piece Group B mirrors for this plot; empty if no house spawned.
    std::vector<HousingMirrorEntity*> GetHouseMeshMirrors(uint8 plotIndex) const;

    // Lightweight Housing/2 identity room entity — the authoritative room the Group A mirrors attach to.
    HousingRoomEntity* GetRoomIdentityEntity(uint8 plotIndex) const;
    ObjectGuid GetRoomIdentityGuid(uint8 plotIndex) const;

    // MeshObject management (housing fixture rendering)
    // pos: local-space position for child pieces (or world position for roots); worldPos: server-side grid placement
    MeshObject* SpawnHouseMeshObject(uint8 plotIndex, int32 fileDataID, bool isWMO,
        Position const& pos, QuaternionData const& rot, float scale,
        ObjectGuid houseGuid, int32 exteriorComponentID, int32 houseExteriorWmoDataID,
        uint8 exteriorComponentType = 9, uint8 houseSize = 2, int32 exteriorComponentHookID = -1,
        ObjectGuid attachParent = ObjectGuid::Empty, uint8 attachFlags = 0,
        Position const* worldPos = nullptr);
    void SpawnFullHouseMeshObjects(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid,
        int32 exteriorComponentID, int32 houseExteriorWmoDataID,
        int32 factionRestriction = NEIGHBORHOOD_FACTION_ALLIANCE,
        FixtureOverrideMap const* fixtureOverrides = nullptr,
        RootOverrideMap const* rootOverrides = nullptr);
    void SpawnHordeHouseMeshObjects(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid,
        int32 exteriorComponentID, int32 houseExteriorWmoDataID);
    uint32 SpawnExtCompTree(uint8 plotIndex, uint32 extCompID,
        Position const& pos, QuaternionData const& rot,
        ObjectGuid houseGuid, int32 houseExteriorWmoDataID,
        ObjectGuid parentGuid, Position const* worldPos, int32 depth = 0,
        FixtureOverrideMap const* fixtureOverrides = nullptr,
        int32 hookIDOverride = -1);
    void DespawnAllMeshObjectsForPlot(uint8 plotIndex);

    // Targeted fixture mesh operations (no full house rebuild)
    // Finds and removes the MeshObject at the given hookID, sending DESTROY to nearby players.
    MeshObject* FindMeshObjectByHookID(uint8 plotIndex, int32 hookID);
    void DespawnSingleMeshObject(uint8 plotIndex, ObjectGuid meshGuid);
    // Spawn a single fixture at a hook; parentHint is the client-named parent mesh (variant re-keys invalidate the DB2 lookup).
    MeshObject* SpawnFixtureAtHook(uint8 plotIndex, uint32 hookID, uint32 componentID,
        ObjectGuid houseGuid, int32 houseExteriorWmoDataID, Player* target,
        ObjectGuid parentHint = ObjectGuid::Empty);

    // Room entity management (provides Geobox for client OutsidePlotBounds check)
    void SpawnRoomForPlot(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid);
    void DespawnRoomForPlot(uint8 plotIndex);
    void SpawnOrMoveHouseRootEntity(uint8 plotIndex, Position const& housePos, ObjectGuid rootGuid);
    void DespawnHouseRootEntity(uint8 plotIndex);

    // Decor management. Functional decor (HouseDecorData.GameObjectID > 0) spawns as an interactive GameObject; visual-only decor as a MeshObject.
    bool SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid);
    void DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid);
    void DespawnAllDecorForPlot(uint8 plotIndex);
    void SpawnAllDecorForPlot(uint8 plotIndex, Housing const* housing);
    void UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale = 1.0f);
    void UpdateDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots);
    void UpdateDecorPet(ObjectGuid decorGuid, ObjectGuid battlePetGuid, uint32 creatureId, std::string const& petName, uint8 petBehavior);

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

    // Transmit a plot's house MeshObjects to everyone on this map; grid visibility does not deliver them.
    void SendPlotMeshObjectsToPlayers(uint8 plotIndex);

    // Transmit a plot's geometry entities (room identity, Geobox mesh, root Entity, Group B mirrors) to one player.
    void SendPlotGeometryEntitiesToPlayer(uint8 plotIndex, Player* player);
    void SendPlotGeometryEntitiesToMap(uint8 plotIndex);

    // Manual spell packet helpers — called from AddPlayerToMap and at_housing_plot AT script.
    // These spells don't exist in DB2, so CastSpell() silently fails; manual packets are required.
    void SendPlotEnterSpellPackets(Player* player, uint8 plotIndex);
    void SendPlotLeaveAuraRemoval(Player* player);

    // Retail neighborhood-map-entry aura burst: four housing-specific AURA_UPDATE+SPELL_START+SPELL_GO triples.
    void SendNeighborhoodMapEntryAuras(Player* player);

private:
    uint32 _neighborhoodId;
    Neighborhood* _neighborhood;
    std::unordered_map<ObjectGuid, Housing*> _playerHousings;
    std::unordered_map<uint8, ObjectGuid> _plotAreaTriggers;
    std::unordered_map<uint8, ObjectGuid> _plotGameObjects;

    // House structure GO tracking (plotIndex -> house GO GUID)
    std::unordered_map<uint8, ObjectGuid> _houseGameObjects;

    // House-exterior root Entity per plot (see GetHouseRootEntity). Kept across house rebuilds like the plot room.
    std::unordered_map<uint8, ObjectGuid> _houseRootEntityGuids;

    // World GameObject spawns inside each plot's bounds (cached) and the ones kept out while their plot is owned.
    std::unordered_map<uint8, std::vector<ObjectGuid::LowType>> _plotGroundSpawns;
    std::unordered_set<ObjectGuid::LowType> _suppressedPlotSpawns;

    // "Group B" per-piece Entity mirrors, co-spawned with the house and despawned together.
    std::unordered_map<uint8, std::vector<std::unique_ptr<HousingMirrorEntity>>> _houseMeshMirrorEntities;

    // Housing/2 identity room GUID per plot; the entity is owned by the map's object store.
    std::unordered_map<uint8, ObjectGuid> _roomIdentityGuids;

    // MeshObject tracking (plotIndex -> vector of MeshObject GUIDs)
    std::unordered_map<uint8, std::vector<ObjectGuid>> _meshObjects;

    // Room entity tracking (plotIndex -> room/component MeshObject GUIDs)
    std::unordered_map<uint8, ObjectGuid> _roomEntities;        // room "entity" MeshObject
    std::unordered_map<uint8, ObjectGuid> _roomComponentMeshes; // room component MeshObject (has Geobox)

    // Decor GO tracking
    std::unordered_map<uint8, std::vector<ObjectGuid>> _decorGameObjects;         // plotIndex -> decor GO GUIDs
    std::unordered_map<ObjectGuid, ObjectGuid> _decorGuidToGoGuid;                // decor GUID -> GO GUID
    std::unordered_map<ObjectGuid, ObjectGuid> _decorGuidToPetSummon;             // decor GUID -> companion battle pet creature
    std::unordered_map<ObjectGuid, uint8> _decorGuidToPlotIndex;                  // decor GUID -> plotIndex
    std::unordered_set<uint8> _decorSpawnedPlots;                                 // plots whose decor has been spawned
    std::unordered_map<ObjectGuid, uint8> _playerCurrentPlot;                    // player GUID -> current visited plot index
};

#endif // HousingMap_h__
