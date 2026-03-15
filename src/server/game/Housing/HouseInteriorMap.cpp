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
#include "Account.h"
#include "HousingPlayerHouseEntity.h"
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
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellPackets.h"
#include "World.h"
#include "WorldSession.h"

HouseInteriorMap::HouseInteriorMap(uint32 id, time_t expiry, uint32 instanceId, ObjectGuid const& owner)
    : Map(id, expiry, instanceId, DIFFICULTY_NORMAL),
      _owner(owner),
      _loadingPlayer(nullptr),
      _sourceNeighborhoodMapId(0),
      _sourcePlotIndex(0),
      _roomsSpawned(false)
{
    HouseInteriorMap::InitVisibilityDistance();

    // Look up interior origin from NeighborhoodMap DB2 data for this world map
    if (NeighborhoodMapData const* nmData = sHousingMgr.GetNeighborhoodMapDataForWorldMap(id))
    {
        _originX = nmData->Origin[0];
        _originY = nmData->Origin[1];
        _originZ = nmData->Origin[2];
        TC_LOG_INFO("housing", "HouseInteriorMap::CTOR: Using DB2 origin ({:.1f}, {:.1f}, {:.1f}) from "
            "NeighborhoodMap {} for map {}",
            _originX, _originY, _originZ, nmData->ID, id);
    }
    else
    {
        TC_LOG_INFO("housing", "HouseInteriorMap::CTOR: No NeighborhoodMap DB2 entry for map {} — "
            "using default origin ({:.1f}, {:.1f}, {:.1f})",
            id, _originX, _originY, _originZ);
    }

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
        else
        {
            TC_LOG_WARN("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No RoomWmoData for "
                "roomWmoDataID={} (room entry {}), using fallback geobox (-35,-30,-1.01)->(35,30,125.01)",
                roomWmoDataID, room->RoomEntryId);
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

        float roomX = _originX + static_cast<float>(room->SlotIndex) * sHousingMgr.GetRoomGridSpacing();
        float roomY = _originY;
        float roomZ = _originZ;
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
        uint32 wallCount = 0, floorCount = 0, ceilingCount = 0, doorwayCount = 0;
        uint32 stairsCount = 0, pillarCount = 0, doorwayWallCount = 0, otherCount = 0;

        for (RoomComponentData const& comp : *components)
        {
            if (comp.ModelFileDataID <= 0)
                continue;

            // Look up RoomComponentOption for this component.
            // Interior room components use fixed themes regardless of faction:
            //   Walls: themeID=8 (neutral), Floors/Ceilings: themeID=6
            // Try faction theme first, then neutral (8), then generic (6).
            RoomComponentOptionEntry const* optEntry = sHousingMgr.FindRoomComponentOption(comp.ID, factionThemeID);
            if (!optEntry && factionThemeID != 8)
                optEntry = sHousingMgr.FindRoomComponentOption(comp.ID, 8);
            if (!optEntry && factionThemeID != 6)
                optEntry = sHousingMgr.FindRoomComponentOption(comp.ID, 6);

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
                // TextureID: try per-option DB2 first, then per-type, then hardcoded fallback
                roomComponentTextureID = sHousingMgr.GetTextureIdForComponentOption(roomComponentOptionID);
                if (roomComponentTextureID == 0)
                    roomComponentTextureID = sHousingMgr.GetTextureIdForComponentType(comp.Type);
                if (roomComponentTextureID == 0)
                {
                    // Hardcoded fallback (sniff-verified: walls=24, floors=40, ceilings=54)
                    switch (comp.Type)
                    {
                        case 1: roomComponentTextureID = 24; break;
                        case 2: roomComponentTextureID = 40; break;
                        case 3: roomComponentTextureID = 54; break;
                        default: break;
                    }
                }
            }

            // Component position/rotation: local to room entity
            Position compPos(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2], 0.0f);
            QuaternionData compRot;
            // DB2 OffsetRot is in DEGREES — convert to radians for quaternion math
            static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
            float rx = comp.OffsetRot[0] * DEG_TO_RAD;
            float ry = comp.OffsetRot[1] * DEG_TO_RAD;
            float rz = comp.OffsetRot[2] * DEG_TO_RAD;
            float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
            float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
            float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
            compRot.x = sx * cy * cz - cx * sy * sz;
            compRot.y = cx * sy * cz + sx * cy * sz;
            compRot.z = cx * cy * sz - sx * sy * cz;
            compRot.w = cx * cy * cz + sx * sy * sz;

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

            // Count by component type for diagnostic summary
            switch (comp.Type)
            {
                case 1: ++wallCount; break;
                case 2: ++floorCount; break;
                case 3: ++ceilingCount; break;
                case 4: ++doorwayCount; break;
                case 5: ++stairsCount; break;
                case 6: ++pillarCount; break;
                case 7: ++doorwayWallCount; break;
                default: ++otherCount; break;
            }

            TC_LOG_ERROR("housing", "  Component: compID={} type={} fileDataID={} optionID={} themeID={} "
                "pos=({:.2f},{:.2f},{:.2f}) rot=({:.4f},{:.4f},{:.4f}) quat=({:.4f},{:.4f},{:.4f},{:.4f})"
                " themeFallback={}",
                comp.ID, comp.Type, compFileDataID, roomComponentOptionID, houseThemeID,
                comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2],
                comp.OffsetRot[0], comp.OffsetRot[1], comp.OffsetRot[2],
                compRot.x, compRot.y, compRot.z, compRot.w,
                optEntry ? (optEntry->HouseThemeID == factionThemeID ? "faction" :
                    (optEntry->HouseThemeID == 8 ? "neutral" : "generic")) : "none");
        }

        TC_LOG_ERROR("housing", "  Room '{}' component summary: walls={} floors={} ceilings={} "
            "doorways={} stairs={} pillars={} doorwayWalls={} other={} total={}",
            roomData->Name, wallCount, floorCount, ceilingCount, doorwayCount,
            stairsCount, pillarCount, doorwayWallCount, otherCount, uint32(components->size()));

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
        mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);

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

    // Convert world → local with inverse rotation of the room entity's facing.
    float localX = worldX, localY = worldY, localZ = worldZ;
    if (!roomEntityGuid.IsEmpty())
    {
        float dx = worldX - roomWorldPos.GetPositionX();
        float dy = worldY - roomWorldPos.GetPositionY();
        float roomFacing = roomWorldPos.GetOrientation();
        float cosF = std::cos(roomFacing);
        float sinF = std::sin(roomFacing);
        localX =  cosF * dx + sinF * dy;
        localY = -sinF * dx + cosF * dy;
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
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);

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
                    float targetX = _originX + static_cast<float>(room->SlotIndex) * sHousingMgr.GetRoomGridSpacing();
                    float targetY = _originY;
                    float targetZ = _originZ;
                    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: Teleporting player to visual room "
                        "entry={} slot={} at ({:.1f},{:.1f},{:.1f})",
                        room->RoomEntryId, room->SlotIndex, targetX, targetY, targetZ);
                    player->NearTeleportTo(targetX, targetY, targetZ, player->GetOrientation());
                    break;
                }
            }

            // Send SMSG_HOUSING_GET_CURRENT_HOUSE_INFO_RESPONSE so the client knows the
            // active house context. Without this, the client has no house context and the
            // editor UI (C_HouseEditor.GetHouseEditorModeAvailability) won't show.
            // The exterior map (HousingMap::AddPlayerToMap) sends this — the interior must too.
            {
                WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfo;
                houseInfo.House.HouseGuid = housing->GetHouseGuid();
                houseInfo.House.OwnerGuid = player->GetGUID();
                houseInfo.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
                houseInfo.House.PlotId = housing->GetPlotIndex();
                houseInfo.House.AccessFlags = housing->GetSettingsFlags();
                houseInfo.House.HasMoveOutTime = false;
                houseInfo.Result = 0;
                player->SendDirectMessage(houseInfo.Write());

                TC_LOG_DEBUG("housing", "HouseInteriorMap::AddPlayerToMap: Sent CURRENT_HOUSE_INFO "
                    "HouseGuid={} OwnerGuid={} NeighborhoodGuid={} PlotId={}",
                    housing->GetHouseGuid().ToString(), player->GetGUID().ToString(),
                    housing->GetNeighborhoodGuid().ToString(), housing->GetPlotIndex());
            }

            // Send SMSG_HOUSE_INTERIOR_ENTER_HOUSE
            WorldPackets::Housing::HouseInteriorEnterHouse enterHouse;
            enterHouse.HouseGuid = housing->GetHouseGuid();
            player->SendDirectMessage(enterHouse.Write());

            // Send updated house status with Status=1 (Interior)
            WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
            statusResponse.HouseGuid = housing->GetHouseGuid();
            statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
            statusResponse.OwnerPlayerGuid = player->GetGUID();
            statusResponse.NeighborhoodGuid = housing->GetNeighborhoodGuid();
            statusResponse.Status = 1; // Interior
            statusResponse.FlagByte = 0xC0; // bit7=Decor, bit6=Room only — Fixture context managed by dedicated ENTER/EXIT response
            player->SendDirectMessage(statusResponse.Write());

            // Send post-tutorial auras so the client unlocks all editor modes.
            // These auras signal "tutorial complete" and are lost on map transfer.
            // The exterior (HousingMap) sends them via SendPostTutorialAuras —
            // the interior must do the same or the editor remains locked.
            SendPostTutorialAuras(player);

            // Populate FHousingStorage_C (decor/catalog) and budget fields, then send
            // Account entity CREATE + HousingPlayerHouseEntity so the client has the
            // full housing context. Without this, C_HouseEditor.GetHouseEditorModeAvailability()
            // fails and the edit toolbar never appears.
            // Mirrors the exterior map's deferred ENTER_PLOT logic (HousingMap.cpp ~912-958).
            {
                housing->PopulateCatalogStorageEntries();
                housing->SyncUpdateFields();

                WorldSession* session = player->GetSession();

                UpdateData storageUpdate(player->GetMapId());
                WorldPacket storagePacket;

                // Account entity as CREATE (includes FHousingStorage_C with placed decor map)
                session->GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&storageUpdate, player);
                player->m_clientGUIDs.insert(session->GetBattlenetAccount().GetGUID());

                // HousingPlayerHouseEntity (budgets, house level, etc.)
                if (player->HaveAtClient(&session->GetHousingPlayerHouseEntity()))
                    session->GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&storageUpdate, player);
                else
                {
                    session->GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&storageUpdate, player);
                    player->m_clientGUIDs.insert(session->GetHousingPlayerHouseEntity().GetGUID());
                }

                // Bundle interior decor MeshObject CREATEs so the client can correlate
                // FHousingDecor_C.DecorGUID with FHousingStorage_C entries.
                uint32 meshCreateCount = 0;
                for (auto const& [decorGuid, meshObjGuid] : _decorGuidToObjGuid)
                {
                    MeshObject* meshObj = GetMeshObject(meshObjGuid);
                    if (!meshObj || !meshObj->IsInWorld())
                        continue;

                    meshObj->BuildCreateUpdateBlockForPlayer(&storageUpdate, player);
                    player->m_clientGUIDs.insert(meshObjGuid);
                    ++meshCreateCount;
                }

                storageUpdate.BuildPacket(&storagePacket);
                player->SendDirectMessage(&storagePacket);

                session->GetBattlenetAccount().ClearUpdateMask(true);
                session->GetHousingPlayerHouseEntity().ClearUpdateMask(true);

                TC_LOG_DEBUG("housing", "HouseInteriorMap::AddPlayerToMap: Sent Account CREATE + {} decor MeshObject CREATEs + budget for player {}",
                    meshCreateCount, player->GetGUID().ToString());
            }

            // Send SMSG_INITIATIVE_SERVICE_STATUS so IsInitiativeEnabled() returns true
            // in interior. Sniff-verified: server responds with 0x80 (enabled).
            {
                WorldPackets::Housing::InitiativeServiceStatus initStatus;
                initStatus.ServiceEnabled = true;
                player->SendDirectMessage(initStatus.Write());
            }

            // Toggle WS[30906]=1 to signal the client that the player is inside a house interior.
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_INTERIOR, 1);
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

    // Toggle WS[30906]=0 to signal the client that the player left the house interior.
    player->SendUpdateWorldState(WORLDSTATE_HOUSING_INTERIOR, 0);

    TC_LOG_ERROR("housing", "HouseInteriorMap::RemovePlayerFromMap: Player {} leaving interior "
        "(owner={}, map={}, instanceId={}, _roomsSpawned={}, roomMeshEntries={}, decorEntries={}, this={})",
        player->GetGUID().ToString(), _owner.ToString(), GetId(), GetInstanceId(),
        _roomsSpawned, uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()), (void*)this);

    Map::RemovePlayerFromMap(player, remove);
}

void HouseInteriorMap::SendPostTutorialAuras(Player* player)
{
    // Sniff-verified: After quest 94455 "Home at Last" completion, three "post-tutorial" auras
    // are applied at slots 8, 9, 50. These signal "tutorial complete" to the client and unlock
    // all editor modes (expert, cleanup, layout, customize). Auras are lost on map transfer,
    // so they must be re-sent when entering both the exterior AND interior housing maps.
    // Slot 8: spell 1285428 (NoCaster, ActiveFlags=1)
    // Slot 9: spell 1285424 (NoCaster, ActiveFlags=1)
    // Slot 50: spell 1266699 (NoCaster|Scalable, ActiveFlags=1, Points=1)
    //
    // TODO: When the housing tutorial questline is implemented, these auras should only be
    // sent for players who have actually completed the tutorial quest (94455 "Home at Last").

    // Spell 1285428 at slot 8
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_1,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 8;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;  // 15
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_1;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;  // 781
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // Spell 1285424 at slot 9
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_2,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 9;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_2;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    // Spell 1266699 at slot 50 (same ID as SPELL_HOUSING_PLOT_ENTER_2, different slot + Points)
    {
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), SPELL_HOUSING_TUTORIAL_DONE_3,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        WorldPackets::Spells::AuraUpdate auraUpdate;
        auraUpdate.UpdateAll = false;
        auraUpdate.UnitGUID = player->GetGUID();

        WorldPackets::Spells::AuraInfo auraInfo;
        auraInfo.Slot = 50;
        auraInfo.AuraData.emplace();
        auraInfo.AuraData->CastID = castId;
        auraInfo.AuraData->SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        auraInfo.AuraData->Flags = AFLAG_NOCASTER | AFLAG_SCALABLE;
        auraInfo.AuraData->ActiveFlags = 1;
        auraInfo.AuraData->CastLevel = 36;
        auraInfo.AuraData->Applications = 0;
        auraInfo.AuraData->Points.push_back(1.0f);
        auraUpdate.Auras.push_back(std::move(auraInfo));
        player->SendDirectMessage(auraUpdate.Write());

        WorldPackets::Spells::SpellStart spellStart;
        spellStart.Cast.CasterGUID = player->GetGUID();
        spellStart.Cast.CasterUnit = player->GetGUID();
        spellStart.Cast.CastID = castId;
        spellStart.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        spellStart.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4;
        spellStart.Cast.CastTime = 0;
        player->SendDirectMessage(spellStart.Write());

        WorldPackets::Spells::SpellGo spellGo;
        spellGo.Cast.CasterGUID = player->GetGUID();
        spellGo.Cast.CasterUnit = player->GetGUID();
        spellGo.Cast.CastID = castId;
        spellGo.Cast.SpellID = SPELL_HOUSING_TUTORIAL_DONE_3;
        spellGo.Cast.CastFlags = CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10;
        spellGo.Cast.CastFlagsEx = 16;
        spellGo.Cast.CastFlagsEx2 = 4;
        spellGo.Cast.CastTime = getMSTime();
        spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
        spellGo.Cast.HitTargets.push_back(player->GetGUID());
        spellGo.Cast.HitStatus.emplace_back(uint8(0));
        spellGo.LogData.Initialize(player);
        player->SendDirectMessage(spellGo.Write());
    }

    TC_LOG_DEBUG("housing", "HouseInteriorMap::SendPostTutorialAuras: Sent 3 post-tutorial aura sequences "
        "(1285428@s8, 1285424@s9, 1266699@s50) for player {}",
        player->GetGUID().ToString());
}
