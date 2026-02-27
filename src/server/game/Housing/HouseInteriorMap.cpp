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

#include "HouseInteriorMap.h"
#include "DBCEnums.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MeshObject.h"
#include "ObjectAccessor.h"
#include "ObjectGridLoader.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"

// Interior map spawn origin (from NeighborhoodMap ID=7 DB2 data)
static constexpr float INTERIOR_ORIGIN_X = -1000.0f;
static constexpr float INTERIOR_ORIGIN_Y = -1000.0f;
static constexpr float INTERIOR_ORIGIN_Z = 0.1f;

HouseInteriorMap::HouseInteriorMap(uint32 id, time_t expiry, uint32 instanceId, ObjectGuid const& owner)
    : Map(id, expiry, instanceId, DIFFICULTY_NORMAL),
      _owner(owner),
      _loadingPlayer(nullptr),
      _sourceNeighborhoodMapId(0),
      _sourcePlotIndex(0),
      _roomsSpawned(false)
{
    HouseInteriorMap::InitVisibilityDistance();

    TC_LOG_DEBUG("housing", "HouseInteriorMap: Created interior map {} instanceId {} for owner {}",
        id, instanceId, owner.ToString());
}

void HouseInteriorMap::InitVisibilityDistance()
{
    m_VisibleDistance = sWorld->getFloatConfig(CONFIG_MAX_VISIBILITY_DISTANCE_INSTANCE);
    m_VisibilityNotifyPeriod = sWorld->getIntConfig(CONFIG_VISIBILITY_NOTIFY_PERIOD_INSTANCE);
}

void HouseInteriorMap::LoadGridObjects(NGridType* grid, Cell const& cell)
{
    Map::LoadGridObjects(grid, cell);

    // Room WMO geometry is spawned when the owner enters via AddPlayerToMap.
    // No static spawns exist on the interior map template.
}

Housing* HouseInteriorMap::GetOwnerHousing()
{
    if (_loadingPlayer)
        return _loadingPlayer->GetHousing();

    if (Player* owner = ObjectAccessor::FindConnectedPlayer(_owner))
        return owner->GetHousing();

    return nullptr;
}

void HouseInteriorMap::SpawnRoomMeshObjects(Housing* housing)
{
    if (!housing)
        return;

    std::vector<Housing::Room const*> rooms = housing->GetRooms();
    if (rooms.empty())
    {
        TC_LOG_INFO("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No rooms to spawn for owner {} "
            "(new house — rooms will appear when placed via editor)",
            _owner.ToString());
        return;
    }

    uint32 totalMeshes = 0;

    for (Housing::Room const* room : rooms)
    {
        HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
        if (!roomData)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Unknown room entry {} "
                "for room {} (owner {})",
                room->RoomEntryId, room->Guid.ToString(), _owner.ToString());
            continue;
        }

        // Get all mesh components for this room type
        std::vector<RoomComponentData> const* components = sHousingMgr.GetRoomComponents(roomData->RoomWmoDataID);
        if (!components || components->empty())
        {
            TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No components for "
                "roomWmoDataID {} (room entry {} '{}', slot {})",
                roomData->RoomWmoDataID, room->RoomEntryId, roomData->Name, room->SlotIndex);
            continue;
        }

        // Calculate room world position from slot index.
        // Slot 0 (base room) = interior origin. Additional slots are offset based on
        // room bounding boxes and doorway connections. Sniff-verified: rooms are ~23yd
        // squares arranged on a 24yd grid (HOUSING_ROOM_GRID_SPACING).
        float roomX = INTERIOR_ORIGIN_X + static_cast<float>(room->SlotIndex) * HOUSING_ROOM_GRID_SPACING;
        float roomY = INTERIOR_ORIGIN_Y;
        float roomZ = INTERIOR_ORIGIN_Z;

        // Room orientation: 0-3 for 90-degree increments
        float roomFacing = static_cast<float>(room->Orientation) * (M_PI / 2.0f);

        Position roomPos(roomX, roomY, roomZ, roomFacing);
        QuaternionData roomRot;
        roomRot.x = 0.0f;
        roomRot.y = 0.0f;
        roomRot.z = std::sin(roomFacing / 2.0f);
        roomRot.w = std::cos(roomFacing / 2.0f);

        // Load the grid cell at the room position so objects can be placed there
        LoadGrid(roomX, roomY);

        // Spawn the first component as the root piece, rest as children
        MeshObject* rootMesh = nullptr;

        for (RoomComponentData const& comp : *components)
        {
            if (comp.ModelFileDataID <= 0)
                continue;

            // Component local-space position (relative to room center)
            Position compPos(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2], 0.0f);

            // Component local-space rotation (Euler angles → quaternion)
            float rx = comp.OffsetRot[0];
            float ry = comp.OffsetRot[1];
            float rz = comp.OffsetRot[2];
            QuaternionData compRot;
            compRot.x = std::sin(rx / 2.0f) * std::cos(ry / 2.0f) * std::cos(rz / 2.0f);
            compRot.y = std::cos(rx / 2.0f) * std::sin(ry / 2.0f) * std::cos(rz / 2.0f);
            compRot.z = std::cos(rx / 2.0f) * std::cos(ry / 2.0f) * std::sin(rz / 2.0f);
            compRot.w = std::cos(rx / 2.0f) * std::cos(ry / 2.0f) * std::cos(rz / 2.0f);

            MeshObject* mesh = nullptr;

            if (!rootMesh)
            {
                // First component = root piece at room world position
                mesh = MeshObject::CreateMeshObject(this, roomPos, roomRot, 1.0f,
                    comp.ModelFileDataID, /*isWMO*/ true,
                    ObjectGuid::Empty, /*attachFlags*/ 0, nullptr);

                if (mesh)
                {
                    mesh->InitHousingFixtureData(housing->GetHouseGuid(),
                        static_cast<int32>(comp.ID), roomData->RoomWmoDataID,
                        /*exteriorComponentType*/ comp.Type, /*houseSize*/ 2, /*hookID*/ -1);

                    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(),
                        PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                    if (AddToMap(mesh))
                    {
                        rootMesh = mesh;
                        _roomMeshObjects[room->Guid].push_back(mesh->GetGUID());
                        ++totalMeshes;
                    }
                    else
                    {
                        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                            "AddToMap failed for root mesh (fileDataID={}, roomEntry={}, slot={})",
                            comp.ModelFileDataID, room->RoomEntryId, room->SlotIndex);
                        delete mesh;
                        mesh = nullptr;
                    }
                }
            }
            else
            {
                // Child component = attached to root with local offset
                mesh = MeshObject::CreateMeshObject(this, compPos, compRot, 1.0f,
                    comp.ModelFileDataID, /*isWMO*/ true,
                    rootMesh->GetGUID(), /*attachFlags*/ 3, &roomPos);

                if (mesh)
                {
                    mesh->InitHousingFixtureData(housing->GetHouseGuid(),
                        static_cast<int32>(comp.ID), roomData->RoomWmoDataID,
                        /*exteriorComponentType*/ comp.Type, /*houseSize*/ 2,
                        /*hookID*/ static_cast<int32>(comp.ID));

                    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(),
                        PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                    if (AddToMap(mesh))
                    {
                        _roomMeshObjects[room->Guid].push_back(mesh->GetGUID());
                        ++totalMeshes;
                    }
                    else
                    {
                        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                            "AddToMap failed for child mesh (fileDataID={}, roomEntry={}, slot={})",
                            comp.ModelFileDataID, room->RoomEntryId, room->SlotIndex);
                        delete mesh;
                    }
                }
            }

            if (!mesh)
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                    "CreateMeshObject failed (fileDataID={}, comp={}, type={}, roomEntry={})",
                    comp.ModelFileDataID, comp.ID, comp.Type, room->RoomEntryId);
            }
        }

        TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Room '{}' (entry={}, slot={}, wmoData={}) "
            "spawned {} components",
            roomData->Name, room->RoomEntryId, room->SlotIndex, roomData->RoomWmoDataID,
            _roomMeshObjects.count(room->Guid) ? uint32(_roomMeshObjects[room->Guid].size()) : 0u);
    }

    TC_LOG_INFO("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Spawned {} total MeshObjects for {} rooms "
        "(owner={}, map={}, instanceId={})",
        totalMeshes, uint32(rooms.size()), _owner.ToString(), GetId(), GetInstanceId());
}

void HouseInteriorMap::DespawnAllRoomMeshObjects()
{
    uint32 despawnCount = 0;

    for (auto& [roomGuid, meshGuids] : _roomMeshObjects)
    {
        for (ObjectGuid const& meshGuid : meshGuids)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
            {
                mesh->AddObjectToRemoveList();
                ++despawnCount;
            }
        }
    }

    _roomMeshObjects.clear();
    _roomsSpawned = false;

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnAllRoomMeshObjects: Despawned {} MeshObjects (owner={})",
        despawnCount, _owner.ToString());
}

bool HouseInteriorMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    if (player->GetGUID() == _owner)
        _loadingPlayer = player;

    bool result = Map::AddPlayerToMap(player, initPlayer);

    if (player->GetGUID() == _owner)
        _loadingPlayer = nullptr;

    if (result)
    {
        Housing* housing = player->GetHousing();
        if (housing)
        {
            housing->SetInInterior(true);

            // Spawn room meshes on first entry
            if (!_roomsSpawned && player->GetGUID() == _owner)
            {
                SpawnRoomMeshObjects(housing);
                _roomsSpawned = true;
            }

            // Send SMSG_HOUSE_INTERIOR_ENTER_HOUSE
            WorldPackets::Housing::HouseInteriorEnterHouse enterHouse;
            enterHouse.HouseGuid = housing->GetHouseGuid();
            player->SendDirectMessage(enterHouse.Write());

            // Send updated house status with Status=1 (Interior)
            WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
            statusResponse.HouseGuid = housing->GetHouseGuid();
            statusResponse.NeighborhoodGuid = player->GetSession()->GetBattlenetAccountGUID();
            statusResponse.OwnerPlayerGuid = player->GetGUID();
            statusResponse.Status = 1; // Interior
            player->SendDirectMessage(statusResponse.Write());
        }

        TC_LOG_INFO("housing", "HouseInteriorMap: Player {} entered house interior (owner={}, map={}, instanceId={})",
            player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId());
    }

    return result;
}

void HouseInteriorMap::RemovePlayerFromMap(Player* player, bool remove)
{
    Housing* housing = player->GetHousing();
    if (housing)
        housing->SetInInterior(false);

    TC_LOG_DEBUG("housing", "HouseInteriorMap: Player {} leaving house interior (owner={}, map={}, instanceId={})",
        player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId());

    Map::RemovePlayerFromMap(player, remove);
}
