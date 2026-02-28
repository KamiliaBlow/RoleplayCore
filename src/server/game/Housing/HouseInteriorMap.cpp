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
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameObjectData.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "Log.h"
#include "MeshObject.h"
#include "ObjectAccessor.h"
#include "ObjectGridLoader.h"
#include "ObjectMgr.h"
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

    TC_LOG_ERROR("housing", "HouseInteriorMap::CTOR: Created interior map {} instanceId {} for owner {} "
        "(this={}, _roomsSpawned={})",
        id, instanceId, owner.ToString(), (void*)this, _roomsSpawned);
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

void HouseInteriorMap::SpawnRoomMeshObjects(Housing* housing, int32 factionRestriction)
{
    if (!housing)
        return;

    std::vector<Housing::Room const*> rooms = housing->GetRooms();
    if (rooms.empty())
    {
        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No rooms to spawn for owner {} "
            "(new house — rooms will appear when placed via editor)",
            _owner.ToString());
        return;
    }

    int32 factionThemeID = sHousingMgr.GetFactionDefaultThemeID(factionRestriction);
    uint32 totalMeshes = 0;

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Starting spawn for {} rooms "
        "(owner={}, factionThemeID={}, houseGuid={})",
        uint32(rooms.size()), _owner.ToString(), factionThemeID, housing->GetHouseGuid().ToString());

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

        // --- DB2 lookups ---

        // 1. HouseRoom → RoomWmoDataID
        int32 roomWmoDataID = roomData->RoomWmoDataID;

        // 2. RoomWmoData → Geobox bounds (bounding box for OutsidePlotBounds check)
        float geoMinX = -35.0f, geoMinY = -30.0f, geoMinZ = -1.01f;
        float geoMaxX =  35.0f, geoMaxY =  30.0f, geoMaxZ = 125.01f;
        RoomWmoDataEntry const* wmoData = roomWmoDataID ? sRoomWmoDataStore.LookupEntry(roomWmoDataID) : nullptr;
        if (wmoData)
        {
            geoMinX = wmoData->BoundingBoxMinX;
            geoMinY = wmoData->BoundingBoxMinY;
            geoMinZ = wmoData->BoundingBoxMinZ;
            geoMaxX = wmoData->BoundingBoxMaxX;
            geoMaxY = wmoData->BoundingBoxMaxY;
            geoMaxZ = wmoData->BoundingBoxMaxZ;
        }

        // 3. Get ALL components for this room
        std::vector<RoomComponentData> const* components = sHousingMgr.GetRoomComponents(roomWmoDataID);
        if (!components || components->empty())
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No components for "
                "room '{}' (entry={}, roomWmoDataID={})",
                roomData->Name, room->RoomEntryId, roomWmoDataID);
            continue;
        }

        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Room '{}' entry={} slot={} "
            "roomWmoDataID={} has {} components, geobox=({:.2f},{:.2f},{:.2f})->({:.2f},{:.2f},{:.2f})",
            roomData->Name, room->RoomEntryId, room->SlotIndex,
            roomWmoDataID, uint32(components->size()),
            geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ);

        // Use the first component's FileDataID as the room entity mesh
        int32 roomEntityFileDataID = 6322976; // fallback
        for (auto const& c : *components)
        {
            if (c.ModelFileDataID > 0)
            {
                roomEntityFileDataID = c.ModelFileDataID;
                break;
            }
        }

        // --- Calculate room world position ---

        float roomX = INTERIOR_ORIGIN_X + static_cast<float>(room->SlotIndex) * HOUSING_ROOM_GRID_SPACING;
        float roomY = INTERIOR_ORIGIN_Y;
        float roomZ = INTERIOR_ORIGIN_Z;
        float roomFacing = static_cast<float>(room->Orientation) * (M_PI / 2.0f);

        Position roomPos(roomX, roomY, roomZ, roomFacing);
        QuaternionData roomRot;
        roomRot.x = 0.0f;
        roomRot.y = 0.0f;
        roomRot.z = std::sin(roomFacing / 2.0f);
        roomRot.w = std::cos(roomFacing / 2.0f);

        LoadGrid(roomX, roomY);

        // Lock the grid so it never unloads while the interior is active
        GridCoord roomGrid = Trinity::ComputeGridCoord(roomX, roomY);
        GridMarkNoUnload(roomGrid.x_coord, roomGrid.y_coord);

        // --- Phase 1: Create ALL entities BEFORE adding to map ---
        // This matches the exterior SpawnRoomForPlot pattern:
        // create room entity + all components, link them via AddRoomMeshObject,
        // THEN add to map so the room entity's create packet includes all MeshObjects.

        int32 roomFlags = roomData->IsBaseRoom() ? 1 : 0;
        int32 floorIndex = 0;

        MeshObject* roomEntity = MeshObject::CreateMeshObject(this, roomPos, roomRot, 1.0f,
            roomEntityFileDataID, /*isWMO*/ true,
            ObjectGuid::Empty, /*attachFlags*/ 3, nullptr);

        if (!roomEntity)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                "Failed to create room entity (roomEntry={}, slot={})",
                room->RoomEntryId, room->SlotIndex);
            continue;
        }

        PhasingHandler::InitDbPhaseShift(roomEntity->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        roomEntity->InitHousingRoomData(housing->GetHouseGuid(), room->RoomEntryId, roomFlags, floorIndex);

        // --- Phase 2: Create all component MeshObjects and link to room entity ---

        std::vector<MeshObject*> componentMeshes;

        for (RoomComponentData const& comp : *components)
        {
            if (comp.ModelFileDataID <= 0)
                continue;

            // Look up faction-aware RoomComponentOption for this component
            RoomComponentOptionEntry const* optEntry = sHousingMgr.FindRoomComponentOption(comp.ID, factionThemeID);

            int32 compFileDataID = comp.ModelFileDataID;
            int32 roomComponentOptionID = 0;
            int32 houseThemeID = 0;
            int32 field24 = 0;
            int32 roomComponentTextureID = 0;

            if (optEntry)
            {
                if (optEntry->ModelFileDataID > 0)
                    compFileDataID = optEntry->ModelFileDataID;
                roomComponentOptionID = static_cast<int32>(optEntry->ID);
                houseThemeID = optEntry->HouseThemeID;
                field24 = static_cast<int32>(optEntry->SubType);
            }

            // Component position/rotation: local to room entity
            Position compPos(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2], 0.0f);
            QuaternionData compRot;
            // DB2 OffsetRot is in DEGREES — convert to radians for quaternion math
            static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
            float rx = comp.OffsetRot[0] * DEG_TO_RAD;
            float ry = comp.OffsetRot[1] * DEG_TO_RAD;
            float rz = comp.OffsetRot[2] * DEG_TO_RAD;
            compRot.x = std::sin(rx / 2.0f) * std::cos(ry / 2.0f) * std::cos(rz / 2.0f);
            compRot.y = std::cos(rx / 2.0f) * std::sin(ry / 2.0f) * std::cos(rz / 2.0f);
            compRot.z = std::cos(rx / 2.0f) * std::cos(ry / 2.0f) * std::sin(rz / 2.0f);
            compRot.w = std::cos(rx / 2.0f) * std::cos(ry / 2.0f) * std::cos(rz / 2.0f);

            MeshObject* componentMesh = MeshObject::CreateMeshObject(this, compPos, compRot, 1.0f,
                compFileDataID, /*isWMO*/ true,
                roomEntity->GetGUID(), /*attachFlags*/ 3, &roomPos);

            if (!componentMesh)
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                    "CreateMeshObject failed for component (compID={}, fileDataID={}, roomEntry={})",
                    comp.ID, compFileDataID, room->RoomEntryId);
                continue;
            }

            PhasingHandler::InitDbPhaseShift(componentMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
            componentMesh->InitHousingRoomComponentData(roomEntity->GetGUID(),
                roomComponentOptionID, static_cast<int32>(comp.ID),
                comp.Type, field24,
                houseThemeID, roomComponentTextureID,
                /*roomComponentTypeParam*/ 0,
                geoMinX, geoMinY, geoMinZ,
                geoMaxX, geoMaxY, geoMaxZ);

            // Link component to room entity BEFORE either is on the map
            roomEntity->AddRoomMeshObject(componentMesh->GetGUID());
            componentMeshes.push_back(componentMesh);

            TC_LOG_ERROR("housing", "  Component: compID={} type={} fileDataID={} optionID={} themeID={} "
                "pos=({:.2f},{:.2f},{:.2f}) rot=({:.4f},{:.4f},{:.4f}) quat=({:.4f},{:.4f},{:.4f},{:.4f})",
                comp.ID, comp.Type, compFileDataID, roomComponentOptionID, houseThemeID,
                comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2],
                comp.OffsetRot[0], comp.OffsetRot[1], comp.OffsetRot[2],
                compRot.x, compRot.y, compRot.z, compRot.w);
        }

        // --- Phase 3: Add room entity to map FIRST (create packet includes all MeshObjects) ---

        if (!AddToMap(roomEntity))
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                "AddToMap failed for room entity (roomEntry={}, slot={})",
                room->RoomEntryId, room->SlotIndex);
            delete roomEntity;
            for (MeshObject* comp : componentMeshes)
                delete comp;
            continue;
        }

        _roomMeshObjects[room->Guid].push_back(roomEntity->GetGUID());
        ++totalMeshes;

        // --- Phase 4: Add all component MeshObjects to map ---

        for (MeshObject* componentMesh : componentMeshes)
        {
            if (AddToMap(componentMesh))
            {
                _roomMeshObjects[room->Guid].push_back(componentMesh->GetGUID());
                ++totalMeshes;
            }
            else
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                    "AddToMap failed for component (roomEntry={})",
                    room->RoomEntryId);
                delete componentMesh;
            }
        }

        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Room '{}' (entry={}, slot={}) "
            "spawned {} component MeshObjects (themeID={}) at ({:.1f},{:.1f},{:.1f})",
            roomData->Name, room->RoomEntryId, room->SlotIndex,
            uint32(componentMeshes.size()), factionThemeID,
            roomX, roomY, roomZ);
    }

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Spawned {} total MeshObjects for {} rooms "
        "(owner={}, map={}, instanceId={}, faction={})",
        totalMeshes, uint32(rooms.size()), _owner.ToString(), GetId(), GetInstanceId(),
        factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE ? "Alliance" : "Horde");
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

void HouseInteriorMap::SpawnInteriorDecor(Housing* housing)
{
    if (!housing)
        return;

    ObjectGuid houseGuid = housing->GetHouseGuid();
    uint32 spawnCount = 0;
    uint32 exteriorSkipped = 0;
    uint32 totalDecor = uint32(housing->GetPlacedDecorMap().size());

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Starting — totalDecor={} "
        "_roomMeshObjects entries={} owner={}",
        totalDecor, uint32(_roomMeshObjects.size()), _owner.ToString());

    // Log all room mesh object entries for cross-reference
    for (auto const& [roomGuid, meshGuids] : _roomMeshObjects)
    {
        TC_LOG_ERROR("housing", "  _roomMeshObjects[{}] = {} entries (first={})",
            roomGuid.ToString(), uint32(meshGuids.size()),
            meshGuids.empty() ? "EMPTY" : meshGuids[0].ToString());
    }

    for (auto const& [decorGuid, decor] : housing->GetPlacedDecorMap())
    {
        HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
        if (!decorData)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: No HouseDecorData for entry {} (decorGuid={})",
                decor.DecorEntryId, decor.Guid.ToString());
            continue;
        }

        // Only spawn decor placed inside a room (interior). Exterior decor has empty RoomGuid.
        if (decor.RoomGuid.IsEmpty())
        {
            ++exteriorSkipped;
            continue;
        }

        // Skip decor already spawned (e.g., placed during this session via SpawnSingleInteriorDecor)
        if (_decorGuidToObjGuid.count(decor.Guid))
            continue;

        TC_LOG_ERROR("housing", "  SpawnInteriorDecor: decor entry={} roomGuid={} pos=({:.1f},{:.1f},{:.1f})",
            decor.DecorEntryId, decor.RoomGuid.ToString(), decor.PosX, decor.PosY, decor.PosZ);

        // Sniff-verified: ALL retail decor is MeshObject (never GO).
        // Determine FileDataID: prefer ModelFileDataID, fall back to GO template displayInfo.
        int32 fileDataID = decorData->ModelFileDataID;
        if (fileDataID <= 0 && decorData->GameObjectID > 0)
        {
            GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(
                static_cast<uint32>(decorData->GameObjectID));
            if (goTemplate)
            {
                GameObjectDisplayInfoEntry const* displayInfo =
                    sGameObjectDisplayInfoStore.LookupEntry(goTemplate->displayId);
                if (displayInfo && displayInfo->FileDataID > 0)
                    fileDataID = displayInfo->FileDataID;
            }
        }

        if (fileDataID <= 0)
        {
            TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnInteriorDecor: Cannot derive FileDataID for decor entry {} "
                "(GameObjectID={}, ModelFileDataID={}), skipping",
                decor.DecorEntryId, decorData->GameObjectID, decorData->ModelFileDataID);
            continue;
        }

        // Find the room entity that this decor is attached to (by RoomGuid).
        // Sniff-verified: decor attaches to room entity with attachFlags=3.
        ObjectGuid roomEntityGuid = ObjectGuid::Empty;
        Position roomWorldPos;
        auto roomItr = _roomMeshObjects.find(decor.RoomGuid);
        if (roomItr != _roomMeshObjects.end() && !roomItr->second.empty())
        {
            // First entry in the room's mesh list is the room entity itself
            roomEntityGuid = roomItr->second[0];
            if (MeshObject* roomEntity = GetMeshObject(roomEntityGuid))
                roomWorldPos = roomEntity->GetPosition();
        }

        float worldX = decor.PosX;
        float worldY = decor.PosY;
        float worldZ = decor.PosZ;
        LoadGrid(worldX, worldY);

        QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

        // Convert world position to room-local position
        float localX = worldX;
        float localY = worldY;
        float localZ = worldZ;
        if (!roomEntityGuid.IsEmpty())
        {
            localX = worldX - roomWorldPos.GetPositionX();
            localY = worldY - roomWorldPos.GetPositionY();
            localZ = worldZ - roomWorldPos.GetPositionZ();
        }

        Position localPos(localX, localY, localZ);
        Position worldPos(worldX, worldY, worldZ);

        MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, 1.0f,
            fileDataID, /*isWMO*/ false,
            roomEntityGuid, /*attachFlags*/ roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3),
            &worldPos);

        if (!mesh)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Failed to create MeshObject for decor {} (fileDataID={})",
                decor.Guid.ToString(), fileDataID);
            continue;
        }

        PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid);

        if (AddToMap(mesh))
        {
            _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
            ++spawnCount;
            TC_LOG_INFO("housing", "HouseInteriorMap::SpawnInteriorDecor: Spawned decor MeshObject fileDataID={} "
                "at world({:.1f},{:.1f},{:.1f}) local({:.1f},{:.1f},{:.1f}) room={}",
                fileDataID, worldX, worldY, worldZ, localX, localY, localZ, roomEntityGuid.ToString());
        }
        else
        {
            delete mesh;
            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: AddToMap failed for MeshObject decor {}", decor.Guid.ToString());
        }
    }

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnInteriorDecor: Spawned {} decor entities for owner {} "
        "(total={}, exteriorSkipped={})",
        spawnCount, _owner.ToString(), totalDecor, exteriorSkipped);
}

void HouseInteriorMap::SpawnSingleInteriorDecor(Housing::PlacedDecor const& decor, ObjectGuid houseGuid)
{
    if (decor.RoomGuid.IsEmpty())
        return; // exterior decor, not for interior map

    // Already spawned?
    if (_decorGuidToObjGuid.count(decor.Guid))
        return;

    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
        return;

    int32 fileDataID = decorData->ModelFileDataID;
    if (fileDataID <= 0 && decorData->GameObjectID > 0)
    {
        GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(
            static_cast<uint32>(decorData->GameObjectID));
        if (goTemplate)
        {
            GameObjectDisplayInfoEntry const* displayInfo =
                sGameObjectDisplayInfoStore.LookupEntry(goTemplate->displayId);
            if (displayInfo && displayInfo->FileDataID > 0)
                fileDataID = displayInfo->FileDataID;
        }
    }

    if (fileDataID <= 0)
        return;

    // Find the room entity for this decor
    ObjectGuid roomEntityGuid = ObjectGuid::Empty;
    Position roomWorldPos;
    auto roomItr = _roomMeshObjects.find(decor.RoomGuid);
    if (roomItr != _roomMeshObjects.end() && !roomItr->second.empty())
    {
        roomEntityGuid = roomItr->second[0];
        if (MeshObject* roomEntity = GetMeshObject(roomEntityGuid))
            roomWorldPos = roomEntity->GetPosition();
    }

    float worldX = decor.PosX, worldY = decor.PosY, worldZ = decor.PosZ;
    LoadGrid(worldX, worldY);

    QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

    float localX = worldX, localY = worldY, localZ = worldZ;
    if (!roomEntityGuid.IsEmpty())
    {
        localX = worldX - roomWorldPos.GetPositionX();
        localY = worldY - roomWorldPos.GetPositionY();
        localZ = worldZ - roomWorldPos.GetPositionZ();
    }

    Position localPos(localX, localY, localZ);
    Position worldPos(worldX, worldY, worldZ);

    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, 1.0f,
        fileDataID, /*isWMO*/ false,
        roomEntityGuid, /*attachFlags*/ roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3),
        &worldPos);

    if (!mesh)
        return;

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid);

    if (AddToMap(mesh))
    {
        _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
        TC_LOG_INFO("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Spawned decor fileDataID={} "
            "at world({:.1f},{:.1f},{:.1f}) room={}",
            fileDataID, worldX, worldY, worldZ, roomEntityGuid.ToString());
    }
    else
    {
        delete mesh;
    }
}

void HouseInteriorMap::UpdateDecorPosition(ObjectGuid decorGuid, Position const& pos, QuaternionData const& /*rot*/)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
        return;

    if (MeshObject* mesh = GetMeshObject(itr->second))
    {
        mesh->Relocate(pos);
        TC_LOG_DEBUG("housing", "HouseInteriorMap::UpdateDecorPosition: Moved decor {} to ({:.1f},{:.1f},{:.1f})",
            decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ());
    }
}

void HouseInteriorMap::DespawnDecorItem(ObjectGuid decorGuid)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
    {
        TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnDecorItem: No tracked visual for decorGuid={}", decorGuid.ToString());
        return;
    }

    ObjectGuid objGuid = itr->second;
    if (MeshObject* mesh = GetMeshObject(objGuid))
        mesh->AddObjectToRemoveList();

    _decorGuidToObjGuid.erase(itr);

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnDecorItem: Despawned visual for decorGuid={}", decorGuid.ToString());
}

bool HouseInteriorMap::AddPlayerToMap(Player* player, bool initPlayer /*= true*/)
{
    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: ENTER player={} owner={} isOwner={} "
        "_roomsSpawned={} map={} instanceId={} this={}",
        player->GetGUID().ToString(), _owner.ToString(),
        player->GetGUID() == _owner, _roomsSpawned,
        GetId(), GetInstanceId(), (void*)this);

    if (player->GetGUID() == _owner)
        _loadingPlayer = player;

    bool result = Map::AddPlayerToMap(player, initPlayer);

    if (player->GetGUID() == _owner)
        _loadingPlayer = nullptr;

    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: Map::AddPlayerToMap returned {} for player={}",
        result, player->GetGUID().ToString());

    if (result)
    {
        Housing* housing = player->GetHousing();
        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: housing={} for player={}",
            housing ? "VALID" : "NULL", player->GetGUID().ToString());

        if (housing)
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: houseGuid={} rooms={} decor={} "
                "houseGuid.IsEmpty={}",
                housing->GetHouseGuid().ToString(),
                uint32(housing->GetRooms().size()),
                uint32(housing->GetPlacedDecorMap().size()),
                housing->GetHouseGuid().IsEmpty());

            housing->SetInInterior(true);

            // Spawn room meshes on first entry
            if (!_roomsSpawned && player->GetGUID() == _owner)
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: === SPAWNING ROOMS ===");

                // Log each room for debugging
                for (Housing::Room const* room : housing->GetRooms())
                {
                    TC_LOG_ERROR("housing", "  Room: guid={} entryId={} slot={} orientation={} mirrored={}",
                        room->Guid.ToString(), room->RoomEntryId, room->SlotIndex,
                        room->Orientation, room->Mirrored);
                }

                int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                    ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
                SpawnRoomMeshObjects(housing, faction);
                _roomsSpawned = true;
            }
            else
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: Rooms already spawned "
                    "(_roomsSpawned={}, isOwner={})",
                    _roomsSpawned, player->GetGUID() == _owner);
            }

            // Always spawn interior decor (handles both first entry and re-entry).
            // SpawnInteriorDecor skips already-spawned decor via _decorGuidToObjGuid check.
            SpawnInteriorDecor(housing);

            TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: === SPAWN COMPLETE === "
                "roomMeshObjects entries={} decorGuidToObj entries={}",
                uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()));

            // Teleport player to the center of the first visual room.
            // Player spawns at INTERIOR_ORIGIN (slot 0) but the visual room may be at a different slot.
            for (Housing::Room const* room : housing->GetRooms())
            {
                HouseRoomData const* roomData2 = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
                if (roomData2 && !roomData2->IsBaseRoom())
                {
                    float targetX = INTERIOR_ORIGIN_X + static_cast<float>(room->SlotIndex) * HOUSING_ROOM_GRID_SPACING;
                    float targetY = INTERIOR_ORIGIN_Y;
                    float targetZ = INTERIOR_ORIGIN_Z;
                    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: Teleporting player to visual room "
                        "entry={} slot={} at ({:.1f},{:.1f},{:.1f})",
                        room->RoomEntryId, room->SlotIndex, targetX, targetY, targetZ);
                    player->NearTeleportTo(targetX, targetY, targetZ, player->GetOrientation());
                    break;
                }
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
        else
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: NO HOUSING for player {} — "
                "cannot spawn rooms/decor", player->GetGUID().ToString());
        }

        TC_LOG_ERROR("housing", "HouseInteriorMap: Player {} entered house interior (owner={}, map={}, instanceId={})",
            player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId());
    }
    else
    {
        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: FAILED for player={}",
            player->GetGUID().ToString());
    }

    return result;
}

void HouseInteriorMap::RemovePlayerFromMap(Player* player, bool remove)
{
    Housing* housing = player->GetHousing();
    if (housing)
        housing->SetInInterior(false);

    TC_LOG_ERROR("housing", "HouseInteriorMap::RemovePlayerFromMap: Player {} leaving interior "
        "(owner={}, map={}, instanceId={}, _roomsSpawned={}, roomMeshEntries={}, decorEntries={}, this={})",
        player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId(),
        _roomsSpawned, uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()), (void*)this);

    Map::RemovePlayerFromMap(player, remove);
}
