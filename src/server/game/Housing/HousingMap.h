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
#include <memory>
#include <unordered_set>

class AreaTrigger;
class Housing;
class HousingMirrorEntity;
class HousingRoomEntity;
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
    // Returns the plot index whose plot-bounds AT equals `atGuid`, or -1 when none.
    int8 GetPlotIndexForAreaTrigger(ObjectGuid atGuid) const;
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

    // Fixture override map: hookID → ExteriorComponentID from player's fixture selections.
    // When provided, SpawnExtCompTree uses these instead of the DB2 default component at each hook.
    using FixtureOverrideMap = std::unordered_map<uint32 /*hookID*/, uint32 /*extCompID*/>;

    // Root override map: componentType → componentID from player's root fixture selections
    // (e.g., player chose a specific roof variant). When provided, SpawnFullHouseMeshObjects
    // uses these instead of the DB2 default root for that type.
    using RootOverrideMap = std::unordered_map<uint8 /*componentType*/, uint32 /*compID*/>;

    // House structure GO management
    GameObject* SpawnHouseForPlot(uint8 plotIndex, Position const* customPos,
        int32 exteriorComponentID, int32 houseExteriorWmoDataID,
        FixtureOverrideMap const* fixtureOverrides = nullptr,
        RootOverrideMap const* rootOverrides = nullptr);
    void DespawnHouseForPlot(uint8 plotIndex);
    void RespawnDoorGOAtHook(uint8 plotIndex, uint32 hookID, uint32 doorComponentID, Housing const* housing, Player* player = nullptr);
    void DespawnDoorGO(uint8 plotIndex);
    GameObject* GetHouseGameObject(uint8 plotIndex);
    int8 GetPlotIndexForHouseGO(ObjectGuid goGuid) const;
    uint32 GetHouseGameObjectCount() const { return static_cast<uint32>(_houseGameObjects.size()); }

    // House-exterior root mirror (HighGuid::Entity, objectType=18). Carries the
    // plot's world position in a single FMirroredPositionData_C fragment, used
    // by the client's world-map icon picker as the target of
    // FHousingPlayerHouse_C.EntityGUID (struct offset +56). Spawned 1:1 with
    // the exterior root MeshObject at SpawnHouseForPlot time. See
    // docs/HOUSING_ENTITY_MIRROR.md (pending) / memory/housing_entity_mirror_architecture.md.
    HousingMirrorEntity* GetHouseMirror(uint8 plotIndex) const;
    ObjectGuid GetHouseMirrorGuid(uint8 plotIndex) const;
    // Full list of per-piece Group A mirrors for this plot (one per visible
    // exterior fixture MeshObject, Type 9/10/11/12). Returns empty if no
    // house spawned. Index 0 is the Type-9 root (PieceAndRoot tags); the
    // others (Piece tag only) come from Roof/Door/Window pieces in spawn
    // order. The root mirror's GUID is the canonical "house mirror GUID"
    // referenced by FHousingPlayerHouse_C.EntityGUID.
    std::vector<HousingMirrorEntity*> GetHouseMirrors(uint8 plotIndex) const;
    // Deterministic mirror-GUID derivation that does not require the mirror to
    // exist yet — used by proxy emission for neighbour plots whose plot index
    // and bnet owner are known from NeighborhoodMirror data. pieceIndex defaults
    // to 0 (the Type-9 root mirror) which is the canonical GUID referenced by
    // FHousingPlayerHouse_C.EntityGUID; pieceIndex 1+ identify the non-root
    // Group A mirrors (Roof/Door/Window).
    ObjectGuid MakeHouseMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex = 0) const;

    // "Group B" per-piece mesh-level mirrors. Retail pairs EACH visible
    // exterior fixture MeshObject (Base/Roof/Door/Window — ExteriorComponent
    // Type 9/10/11/12) with one untagged FMirroredPositionData_C Entity
    // mirror whose AttachParent is the piece's MeshObject. Sniff idx 9984:
    // 4 Group A (44-byte values) + 4 Group B (52-byte values, AttachParent=
    // MeshObject serialises longer). Without these per-piece anchors the
    // client's spatial index is missing finer-grained hooks (door-hover
    // detection, expert-mode placement preview off non-root meshes).
    HousingMirrorEntity* GetHouseMeshMirror(uint8 plotIndex) const;
    ObjectGuid GetHouseMeshMirrorGuid(uint8 plotIndex) const;
    ObjectGuid MakeHouseMeshMirrorGuid(uint8 plotIndex, uint32 bnetAccountId, uint8 pieceIndex = 0) const;
    // Full list of per-piece Group B mirrors for this plot (one per fixture
    // MeshObject of Type 9/10/11/12). Returns empty if no house spawned.
    std::vector<HousingMirrorEntity*> GetHouseMeshMirrors(uint8 plotIndex) const;

    // Lightweight Housing/2 identity room entity (HighGuid::Housing subType=2,
    // objectType=18) — the authoritative room. Retail-verified architecture:
    // one Housing/2 identity per plot + one component MeshObject attached to
    // it (carries the Geobox). The unified model replaces the old dual-
    // MeshObject pattern. Group A Entity mirrors attach here.
    HousingRoomEntity* GetRoomIdentityEntity(uint8 plotIndex) const;
    ObjectGuid GetRoomIdentityGuid(uint8 plotIndex) const;

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
    // Finds and removes the MeshObject at the given hookID for a plot, sends DESTROY to nearby players.
    MeshObject* FindMeshObjectByHookID(uint8 plotIndex, int32 hookID);
    void DespawnSingleMeshObject(uint8 plotIndex, ObjectGuid meshGuid);
    // Spawn a single fixture component at a hook and send CREATE to a specific player.
    MeshObject* SpawnFixtureAtHook(uint8 plotIndex, uint32 hookID, uint32 componentID,
        ObjectGuid houseGuid, int32 houseExteriorWmoDataID, Player* target);

    // Room entity management (provides Geobox for client OutsidePlotBounds check)
    void SpawnRoomForPlot(uint8 plotIndex, Position const& housePos,
        QuaternionData const& houseRot, ObjectGuid houseGuid);
    void DespawnRoomForPlot(uint8 plotIndex);

    // Decor management. Functional decor (HouseDecorData.GameObjectID > 0) spawns
    // as an interactive GameObject with FHousingDecor_C + FMirroredPositionData_C
    // fragments — retains sit/open/use behavior. Visual-only decor spawns as a
    // MeshObject. Returns true on success.
    bool SpawnDecorItem(uint8 plotIndex, Housing::PlacedDecor const& decor, ObjectGuid houseGuid);
    void DespawnDecorItem(uint8 plotIndex, ObjectGuid decorGuid);
    void DespawnAllDecorForPlot(uint8 plotIndex);
    void SpawnAllDecorForPlot(uint8 plotIndex, Housing const* housing);
    void UpdateDecorPosition(uint8 plotIndex, ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale = 1.0f);

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

    // Blizzlike neighborhood-map-entry aura burst. Sniff-decoded from
    // dump_12.0.1.66838_2026-04-15_09-35-59.pkt at idx 9985-10000 (and
    // cross-checked against the 2026-04-10 capture). Emits the four
    // housing-specific AURA_UPDATE+SPELL_START+SPELL_GO triples that retail
    // sends immediately after the big UPDATE_OBJECT batch: Housing Fixup
    // (1272741 slot 20), Player Action React (1263578 slot 22), Endeavor
    // Cover (1276064 slot 53), In Your Neighborhood (1227147 slot 121 with
    // SpellXSpellVisualID 503683). Each aura has ActiveFlags and Flags
    // sniff-verified per slot. Non-housing pre-existing character auras
    // (e.g. class talents) are intentionally excluded — core TC aura
    // resync handles those.
    void SendNeighborhoodMapEntryAuras(Player* player);

private:
    uint32 _neighborhoodId;
    Neighborhood* _neighborhood;
    std::unordered_map<ObjectGuid, Housing*> _playerHousings;
    std::unordered_map<uint8, ObjectGuid> _plotAreaTriggers;
    std::unordered_map<uint8, ObjectGuid> _plotGameObjects;

    // House structure GO tracking (plotIndex -> house GO GUID)
    std::unordered_map<uint8, ObjectGuid> _houseGameObjects;

    // Group A house-exterior Entity mirrors (HighGuid::Entity, objectType=18)
    // — one per visible exterior fixture MeshObject (Type 9/10/11/12). The
    // Type-9 (Base) entry at index 0 carries both Tag_HouseExteriorPiece +
    // Tag_HouseExteriorRoot and is the GUID referenced by every external
    // proxy (FHousingPlayerHouse_C.EntityGUID). The other three carry
    // Tag_HouseExteriorPiece only. Co-spawned with the house and despawned
    // together. Vector since retail emits 4 of these per plot (sniff idx
    // 9984 in dump_12.0.1.66838_2026-04-15_09-35-59).
    std::unordered_map<uint8, std::vector<std::unique_ptr<HousingMirrorEntity>>> _houseMirrorEntities;

    // "Group B" per-piece mesh-level Entity mirrors, one per visible exterior
    // fixture MeshObject (Type 9/10/11/12). Co-spawned with the house and
    // despawned together. Vector since retail emits 4 of these per plot.
    std::unordered_map<uint8, std::vector<std::unique_ptr<HousingMirrorEntity>>> _houseMeshMirrorEntities;

    // Lightweight Housing/2 identity room GUID per plot. The entity itself is
    // a WorldObject owned by the map's object store; we only track its GUID.
    std::unordered_map<uint8, ObjectGuid> _roomIdentityGuids;

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
