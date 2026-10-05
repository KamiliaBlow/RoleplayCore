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

#include "Housing.h"
#include "Map.h"
#include "ObjectGuid.h"
#include <array>
#include <unordered_map>
#include <vector>

class HousingRoomEntity;
class MeshObject;
class Player;
struct RoomComponentData;
struct RoomComponentOptionEntry;

/// Map instance for a player's house interior (MapID 2783), one per player/account; similar to GarrisonMap.
class TC_GAME_API HouseInteriorMap : public Map
{
public:
    HouseInteriorMap(uint32 id, time_t expiry, uint32 instanceId, ObjectGuid const& owner);

    void LoadGridObjects(NGridType* grid) override;
    void InitVisibilityDistance() override;
    bool AddPlayerToMap(Player* player, bool initPlayer = true) override;
    void RemovePlayerFromMap(Player* player, bool remove) override;

    ObjectGuid GetOwnerGuid() const { return _owner; }
    // The house moved to another character of the account (house settings owner change)
    void SetOwnerGuid(ObjectGuid owner) { _owner = owner; }
    float GetOriginX() const { return _originX; }
    float GetOriginY() const { return _originY; }
    float GetOriginZ() const { return _originZ; }

    /// Get the Housing data for the owner (needed for room/decor state).
    Housing* GetOwnerHousing();
    Housing* GetOwnerHousing() const;
    // The house belongs to the player's account (the owner or another character of the same account).
    bool IsHouseOwnerAccount(Player const* player) const;

    /// The neighborhood map ID the owner came from (for exit teleport).
    uint32 GetSourceNeighborhoodMapId() const { return _sourceNeighborhoodMapId; }
    void SetSourceNeighborhoodMapId(uint32 mapId) { _sourceNeighborhoodMapId = mapId; }

    /// The plot index the owner's house is on (for exit teleport position).
    uint8 GetSourcePlotIndex() const { return _sourcePlotIndex; }
    void SetSourcePlotIndex(uint8 plotIndex) { _sourcePlotIndex = plotIndex; }

    /// Spawn all room meshes for the owner's house layout (factionRestriction: NEIGHBORHOOD_FACTION_*).
    void SpawnRoomMeshObjects(Housing* housing, int32 factionRestriction);

    /// Overload taking raw rooms — for visits to an offline owner's house (data from Neighborhood::PlotInfo.Rooms);
    /// houseGuid becomes the parent of every spawned HousingRoomEntity.
    void SpawnRoomMeshObjectsFromList(std::vector<Housing::Room const*> const& rooms, int32 factionRestriction, ObjectGuid houseGuid);

    /// Despawn all room meshes (e.g., when the interior is rebuilt).
    void DespawnAllRoomMeshObjects();

    /// Update room component textures in-place (material/wallpaper change; no model change).
    void UpdateRoomComponentTextures(ObjectGuid roomGuid, Housing::Room const& room,
        std::vector<uint32> const* componentIDs, int32 textureID);

    /// Rebuild some component slots of one room from its stored look (DESTROY + CREATE of the slot's pieces).
    void RebuildRoomComponents(std::vector<Housing::Room const*> const& rooms, Housing::Room const& room,
        int32 factionRestriction, std::vector<uint32> const& componentIds);

    /// Despawn a single room's entities (MeshObjects + HousingRoomEntity).
    void DespawnRoomEntities(ObjectGuid roomGuid);

    /// After a layout edit: rebuild the door slots whose look changed and re-point every room's door list.
    void RefreshRoomDoors(std::vector<Housing::Room const*> const& rooms, int32 factionRestriction);

    /// Move/turn a spawned room entity to its stored placement (its meshes and decor follow on the client).
    void UpdateRoomPlacement(Housing::Room const& room);

    /// Re-point a decor's attachment to its current parent (parent decor object, or the room once the
    /// stack link is gone); call before UpdateDecorPosition so the local transform uses the fresh anchor.
    void UpdateDecorAttachment(Housing::PlacedDecor const& decor);

    /// Despawn and re-spawn one room's entity, meshes and decor. The client bakes each mesh's world
    /// transform from the room entity at mesh-create time and never re-reads it, so a turned or moved
    /// room must arrive whole again — and the fresh entity create is what rebuilds the editor's layout
    /// entry and door pins. Decor objects bake the same way and must re-arrive with the room.
    /// Destroys go out as immediate direct packets (the door-slot churn ordering) so they cannot
    /// overlap the creates issued in the same call.
    void RespawnRoomVisuals(Housing::Room const& room, int32 factionRestriction, ObjectGuid houseGuid,
        Housing* housing);

    /// True when the position lies inside one of the rooms (its RoomWmoData box, on its floor).
    bool IsInsideAnyRoom(Position const& pos, std::vector<Housing::Room const*> const& rooms) const;

    /// Where the house puts people: the entry hall at the interior origin (SMSG_NEW_WORLD on entry).
    /// Where entering players appear: the placed starter door decor's position when it exists,
    /// else the entry hall origin.
    Position GetEntryPosition() const;

    /// Spawn all placed decor for the owner's house on the interior map.
    void SpawnInteriorDecor(Housing* housing);

    /// Overload for visits to offline owners (raw decor from Neighborhood::PlotInfo.Decor).
    void SpawnInteriorDecorFromList(std::vector<Housing::PlacedDecor> const& decor, ObjectGuid houseGuid);

    /// Spawn a single placed decor item immediately (called from PLACE handler).
    void SpawnSingleInteriorDecor(Housing::PlacedDecor const& decor, ObjectGuid houseGuid);

    /// Update position/rotation of a single interior decor item.
    void UpdateDecorPosition(ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale = 1.0f);
    void UpdateDecorDyes(ObjectGuid decorGuid, std::array<uint32, MAX_HOUSING_DYE_SLOTS> const& dyeSlots);
    void UpdateDecorPet(ObjectGuid decorGuid, ObjectGuid battlePetGuid, uint32 creatureId, std::string const& petName, uint8 petBehavior);
    // Restores FHousingDecor_C.PetInfo from the owner's battle pet journal and spawns the companion creature.
    void ApplyDecorPetBinding(WorldObject* obj, ObjectGuid decorGuid, ObjectGuid battlePetGuid,
        uint32 creatureId, std::string const& petName, uint8 petBehavior);

    /// Despawn a single decor item by its Housing decor GUID.
    void DespawnDecorItem(ObjectGuid decorGuid);

    /// Get the interior decor GUID → MeshObject GUID map (for edit mode CREATEs).
    std::unordered_map<ObjectGuid, ObjectGuid> const& GetDecorGuidMap() const { return _decorGuidToObjGuid; }

    /// Get the room GUID → room MeshObject GUID vectors (for entity set synchronization).
    std::unordered_map<ObjectGuid, std::vector<ObjectGuid>> const& GetRoomMeshObjects() const { return _roomMeshObjects; }

    /// Get HousingRoomEntity instances for inclusion in initial UPDATE_OBJECT
    std::vector<HousingRoomEntity*> const& GetRoomEntities() const { return _roomEntities; }

    // Puts QUEST_HOUSING_TUTORIAL_COMPLETE in the log and credits the house-entered kill credit (AUTO_ACCEPT|AUTO_COMPLETE, no quest-giver NPC).
    void GrantHousingTutorialProgress(Player* player);

private:
    void RestoreDecorPetBinding(WorldObject* obj, ObjectGuid decorGuid, ObjectGuid petGuid, uint8 petBehavior);

    /// A door slot that meets another room.
    struct DoorwayState
    {
        ObjectGuid AttachedRoom;
        bool Owner = false;     ///< this side builds the doorway (Housing::OwnsDoorway)
        uint8 Variant = 0;      ///< RoomComponentOption.RoomComponentID of the doorway pieces
    };

    Position GetRoomWorldPosition(Housing::Room const& room) const;
    HousingRoomEntity* FindRoomEntity(ObjectGuid roomGuid) const;
    /// The HouseTheme a slot is built with: its own, else the room's per-surface/legacy theme, else the faction's.
    static int32 GetComponentThemeID(Housing::Room const& room, RoomComponentData const& comp, int32 factionThemeID);
    /// FHousingRoomComponentMesh HouseThemeID of a piece: the slot's sub-theme when it belongs to the option's theme.
    static int32 GetComponentHouseThemeID(Housing::Room const& room, RoomComponentData const& comp, RoomComponentOptionEntry const* option);
    /// Stairwell halves leave out the shaft between them (lower: ceiling, upper: floor and stairs).
    static bool IsComponentHidden(std::vector<Housing::Room const*> const& rooms, Housing::Room const& room, RoomComponentData const& comp);
    void ReplaceSlotMeshes(Housing::Room const& room, RoomComponentData const& comp,
        std::vector<RoomComponentOptionEntry const*> const& wanted, Position const& roomPos, std::vector<ObjectGuid>& roomMeshes);
    static std::unordered_map<uint32 /*componentId*/, DoorwayState> GetDoorwayStates(std::vector<Housing::Room const*> const& rooms, Housing::Room const& room);
    /// The RoomComponentOption rows one component slot is built from.
    static std::vector<RoomComponentOptionEntry const*> SelectComponentOptions(Housing::Room const& room, RoomComponentData const& comp,
        int32 factionThemeID, DoorwayState const* doorway);
    MeshObject* CreateRoomComponentMesh(Housing::Room const& room, RoomComponentData const& comp,
        RoomComponentOptionEntry const* option, Position const& roomPos);

    ObjectGuid _owner;
    Player* _loadingPlayer; ///< @workaround Player not in ObjectAccessor during login
    uint32 _sourceNeighborhoodMapId;
    uint8 _sourcePlotIndex;
    bool _roomsSpawned = false;

    // Interior map origin from NeighborhoodMap DB2 (fallback: -1000, -1000, 0.1)
    float _originX = -1000.0f;
    float _originY = -1000.0f;
    float _originZ = 0.1f;

    /// GUIDs of all spawned room MeshObjects, indexed by room GUID
    std::unordered_map<ObjectGuid /*roomGuid*/, std::vector<ObjectGuid>> _roomMeshObjects;

    /// Decor GUID → visual object GUID (for despawning individual decor items)
    std::unordered_map<ObjectGuid, ObjectGuid> _decorGuidToObjGuid;
    std::unordered_map<ObjectGuid, ObjectGuid> _decorAttachParentObj;         // decorGuid -> parent decor's client object (snapped stacks)

    /// Decor GUID → companion battle pet creature GUID (CAN_ATTACH_PET decor)
    std::unordered_map<ObjectGuid, ObjectGuid> _decorGuidToPetSummon;

    /// Map-owned interior exit door GO (persists across owner leave/re-entry)

    /// HousingRoomEntity instances (objectType=18, Housing/2 GUIDs) for the layout editor
    std::vector<HousingRoomEntity*> _roomEntities;
};

#endif // HouseInteriorMap_h__
