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
#include "AreaTrigger.h"
#include "HousingPlayerHouseEntity.h"
#include "DB2Stores.h"
#include "DBCEnums.h"
#include "GameObjectData.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingRoomEntity.h"
#include "HousingDecorEntity.h"
#include "HousingMgr.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
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
#include "RealmList.h"
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
        "(this={}, _roomsSpawned={}, InstanceType={}, Instanceable={}, IsNeighborhood={})",
        id, instanceId, owner.ToString(), (void*)this, _roomsSpawned,
        GetEntry()->InstanceType, GetEntry()->Instanceable(), GetEntry()->IsNeighborhood());
}

void HouseInteriorMap::InitVisibilityDistance()
{
    // House interiors are small single-room spaces. Use maximum visibility so
    // all room entities, decor, and furniture are CREATEd immediately on entry.
    m_VisibleDistance = MAX_VISIBILITY_DISTANCE;
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
    SpawnRoomMeshObjectsFromList(housing->GetRooms(), factionRestriction, housing->GetHouseGuid());
}

void HouseInteriorMap::SpawnRoomMeshObjectsFromList(std::vector<Housing::Room const*> const& rooms, int32 factionRestriction, ObjectGuid houseGuid)
{
    if (rooms.empty())
    {
        TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: No rooms to spawn for owner {} "
            "(new house — rooms will appear when placed via editor)",
            _owner.ToString());
        return;
    }

    int32 factionThemeID = sHousingMgr.GetFactionDefaultThemeID(factionRestriction);
    uint32 totalMeshes = 0;

    // Build position→GUID mapping for door connection resolution.
    // Key encodes yard position: (gridX+10000)*100000 + (gridY+10000)
    auto posKey = [](int32 gx, int32 gy) -> uint64 { return uint64(gx + 10000) * 100000ULL + uint64(gy + 10000); };
    std::unordered_map<uint64, ObjectGuid> posToRoomGuid;
    for (Housing::Room const* r : rooms)
        posToRoomGuid[posKey(r->GridX, r->GridY)] = r->Guid;

    // Local GUID→room map used by the parent-side-of-a-door check below. Built
    // from the passed-in rooms vector so this function no longer depends on a
    // live `Housing` object (it's also called from the offline-owner visit path).
    std::unordered_map<ObjectGuid, Housing::Room const*> roomByGuid;
    for (Housing::Room const* r : rooms)
        roomByGuid[r->Guid] = r;

    // Helper: find the room DIRECTLY adjacent across this door's wall.
    // A neighbor qualifies only if it's aligned on the perpendicular axis AND
    // is the NEAREST room in the door's direction. Otherwise walls facing a
    // general direction would claim non-adjacent rooms as neighbors, producing
    // phantom door entries (client then refuses room removal with "more than
    // one connected doors").
    auto findNeighborAtDoor = [&](Housing::Room const* srcRoom, float doorOffX, float doorOffY) -> ObjectGuid
    {
        // Alignment tolerance (yards). Rooms are on a ~15yd grid, so same-axis
        // rooms should share GridX or GridY within a small epsilon.
        constexpr int32 ALIGN_TOLERANCE = 8; // half of minimum room width
        constexpr int32 FLOOR_TOLERANCE = 0; // rooms on different floors don't share doors

        ObjectGuid bestGuid;
        int32 bestDistance = std::numeric_limits<int32>::max();

        for (Housing::Room const* other : rooms)
        {
            if (other->Guid == srcRoom->Guid)
                continue;
            // Skip rooms on different floors — stacked stairwell partners sit at
            // the same XY but different Z, and horizontal door-matching must stay
            // per-floor so upper walls aren't replaced with doorways pointing at
            // lower-floor neighbors.
            if (std::abs(other->FloorIndex - srcRoom->FloorIndex) > FLOOR_TOLERANCE)
                continue;
            int32 dx = other->GridX - srcRoom->GridX;
            int32 dy = other->GridY - srcRoom->GridY;

            // East/West wall: neighbor must be aligned on Y and in the X direction
            if (doorOffX > 0.5f && dx > 0 && std::abs(dy) <= ALIGN_TOLERANCE)
            {
                if (dx < bestDistance) { bestDistance = dx; bestGuid = other->Guid; }
            }
            else if (doorOffX < -0.5f && dx < 0 && std::abs(dy) <= ALIGN_TOLERANCE)
            {
                if (-dx < bestDistance) { bestDistance = -dx; bestGuid = other->Guid; }
            }
            // North/South wall: neighbor must be aligned on X and in the Y direction
            else if (doorOffY > 0.5f && dy > 0 && std::abs(dx) <= ALIGN_TOLERANCE)
            {
                if (dy < bestDistance) { bestDistance = dy; bestGuid = other->Guid; }
            }
            else if (doorOffY < -0.5f && dy < 0 && std::abs(dx) <= ALIGN_TOLERANCE)
            {
                if (-dy < bestDistance) { bestDistance = -dy; bestGuid = other->Guid; }
            }
        }
        return bestGuid;
    };

    TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Starting spawn for {} rooms "
        "(owner={}, factionThemeID={}, houseGuid={})",
        uint32(rooms.size()), _owner.ToString(), factionThemeID, houseGuid.ToString());

    for (Housing::Room const* room : rooms)
    {
        // Skip rooms that already have entities on the map (incremental spawn for room add).
        if (_roomMeshObjects.count(room->Guid) > 0)
            continue;

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

        // --- Calculate room world position ---
        // GridX/GridY = yard offsets. FloorIndex = floor NUMBER (0=ground, 1=floor2, …).
        // Sniff-verified: retail HousingRoomEntity.FloorIndex is a small integer
        // used by the client's floor selector UI, while world Z is computed as
        // FloorIndex × 12 yards (confirmed by positions at Z=0.1, 12.1, 24.1).
        static constexpr float FLOOR_HEIGHT_Y = 12.0f;
        float roomX = _originX + static_cast<float>(room->GridX);
        float roomY = _originY + static_cast<float>(room->GridY);
        float roomZ = _originZ + static_cast<float>(room->FloorIndex) * FLOOR_HEIGHT_Y;
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

        int32 roomFlags = roomData->IsBaseRoom() ? 1 : 0;
        int32 floorIndex = room->FloorIndex;
        ObjectGuid roomHousingGuid = room->Guid; // Housing/2 GUID for attach parent

        // --- Phase 1: Create HousingRoomEntity FIRST ---
        // Must exist on the map BEFORE component MeshObjects because components use
        // AttachParentGUID = roomHousingGuid. The client resolves this GUID during
        // entity creation — if the parent doesn't exist, it crashes (NULL+0x20).
        // Door data is added after components are created (Phase 3).
        HousingRoomEntity* housingRoom = new HousingRoomEntity();
        PhasingHandler::InitDbPhaseShift(housingRoom->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        housingRoom->SetHouseGUID(houseGuid);
        housingRoom->SetHouseRoomID(room->RoomEntryId);
        housingRoom->SetFlags(roomFlags);
        housingRoom->SetFloorIndex(floorIndex);
        housingRoom->SetMirroredPosition(roomPos, roomRot, 1.0f);

        if (!housingRoom->Create(roomHousingGuid, this, roomPos))
        {
            TC_LOG_ERROR("housing", "HouseInteriorMap: Failed to add HousingRoomEntity to map (roomEntry={})",
                room->RoomEntryId);
            delete housingRoom;
            continue;
        }
        _roomEntities.push_back(housingRoom);

        // --- Phase 2: Create all component MeshObjects ---

        std::vector<MeshObject*> componentMeshes;
        uint32 wallCount = 0, floorCount = 0, ceilingCount = 0, doorwayCount = 0;
        uint32 stairsCount = 0, pillarCount = 0, doorwayWallCount = 0, otherCount = 0;

        bool isStairwell = roomData->HasStairs();

        // Stairwells come in stacked pairs at the same XY — the stairs-bearing
        // room and its upper partner (12 yards above). HAS_STAIRS flag is set
        // on BOTH rooms in the pair (incl. "Stairwell Room (Empty)"), so we
        // classify by checking for a partner at Z±12 at the same XY.
        //   Base   = has another stairwell 12 yards ABOVE  → skip ceiling
        //            (partner above provides it at world Z=24)
        //   Partner = has another stairwell 12 yards BELOW → skip floor
        //            (stairs mesh below already fills the landing at Z=12)
        bool hasPartnerAbove = false;
        bool hasPartnerBelow = false;
        if (isStairwell)
        {
            for (Housing::Room const* other : rooms)
            {
                if (other == room) continue;
                if (other->GridX != room->GridX || other->GridY != room->GridY) continue;
                HouseRoomData const* od = sHousingMgr.GetHouseRoomData(other->RoomEntryId);
                if (!od || !od->HasStairs()) continue;
                if (other->FloorIndex == room->FloorIndex + 1) hasPartnerAbove = true;
                if (other->FloorIndex == room->FloorIndex - 1) hasPartnerBelow = true;
            }
        }
        bool isStairwellBase    = isStairwell && hasPartnerAbove;
        bool isStairwellPartner = isStairwell && hasPartnerBelow;

        if (isStairwell)
            TC_LOG_ERROR("housing", "  STAIRWELL ROOM entry={} roomWorldPos=({:.1f},{:.1f},{:.1f}) facing={:.2f} "
                "geobox=({:.1f},{:.1f},{:.1f})->({:.1f},{:.1f},{:.1f}) components={}",
                room->RoomEntryId, roomX, roomY, roomZ, roomFacing,
                geoMinX, geoMinY, geoMinZ, geoMaxX, geoMaxY, geoMaxZ,
                uint32(components->size()));

        for (RoomComponentData const& comp : *components)
        {
            // Base stairwell (has a partner above): skip its ceiling so the
            // upper floor is visible through the stairwell opening.
            if (isStairwellBase && comp.Type == HOUSING_ROOM_COMPONENT_CEILING)
                continue;

            // Partner stairwell (upper, has a stairs-bearing room below): skip
            // its floor so the stairs mesh from below visually reaches the
            // landing without z-fighting, and the gap between floors stays
            // open. Keep the ceiling (at world Z=24) and the walls.
            if (isStairwellPartner && comp.Type == HOUSING_ROOM_COMPONENT_FLOOR)
                continue;

            // Log stairwell component positions to diagnose ceiling height
            if (isStairwell)
                TC_LOG_ERROR("housing", "  STAIRWELL comp ID={} Type={} ConnType={} LocalPos=({:.1f},{:.1f},{:.1f}) "
                    "WorldZ={:.1f} MSFID={} DefaultFDID={}",
                    comp.ID, comp.Type, comp.ConnectionType,
                    comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2],
                    roomZ + comp.OffsetPos[2], comp.MeshStyleFilterID, comp.ModelFileDataID);

            // Look up RoomComponentOption for this component via MeshStyleFilterID.
            // Alliance sniff-verified: ALL components use faction theme (1=Folk → sub-theme 6).
            // Horde uses theme 2 (Rugged → sub-theme 8). Each faction uses its OWN wall models.
            // Component position/rotation: local to room entity
            Position compPos(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2], 0.0f);
            QuaternionData compRot;
            // DB2 OffsetRot is in DEGREES — convert to radians. Z is negated (sniff-verified).
            static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
            float rx = comp.OffsetRot[0] * DEG_TO_RAD;
            float ry = comp.OffsetRot[1] * DEG_TO_RAD;
            float rz = -comp.OffsetRot[2] * DEG_TO_RAD; // negated (sniff-verified)
            float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
            float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
            float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
            compRot.x = sx * cy * cz - cx * sy * sz;
            compRot.y = cx * sy * cz + sx * cy * sz;
            compRot.z = cx * cy * sz - sx * sy * cz;
            compRot.w = cx * cy * cz + sx * sy * sz;

            // Look up ALL options for this component. Alliance sniff shows:
            // - Corner walls (MSFID=46): TWO MeshObjects (SubType=0 + SubType=1)
            // - Doorway walls: TWO MeshObjects (DoorwayWall Field_20=1 + Doorway Field_20=2)
            // - Flat walls/floors/ceilings: ONE MeshObject (Cosmetic SubType=0)
            // Per-surface theme: walls / floors / ceilings can carry independent
            // theme IDs. Fall back to the legacy single ThemeId then faction.
            uint32 perSurfaceTheme = 0;
            switch (comp.Type)
            {
                case HOUSING_ROOM_COMPONENT_WALL:
                case HOUSING_ROOM_COMPONENT_DOORWAY_WALL:
                    perSurfaceTheme = room->WallThemeId;
                    break;
                case HOUSING_ROOM_COMPONENT_FLOOR:
                    perSurfaceTheme = room->FloorThemeId;
                    break;
                case HOUSING_ROOM_COMPONENT_CEILING:
                    perSurfaceTheme = room->CeilingThemeId;
                    break;
                default:
                    break;
            }
            int32 rawTheme = perSurfaceTheme ? static_cast<int32>(perSurfaceTheme)
                : (room->ThemeId != 0 ? static_cast<int32>(room->ThemeId) : factionThemeID);
            // RoomComponentOption rows only exist for base themes (1-5). The stored
            // per-surface themes are usually sub-themes (e.g. 11=Bel'ameth Folk,
            // 20=Folk Light) — resolve them to the parent base theme or the lookup
            // falls through to the faction default and the user's style is lost.
            int32 lookupTheme = sHousingMgr.GetBaseThemeID(rawTheme);
            if (lookupTheme <= 0)
                lookupTheme = rawTheme;
            std::vector<RoomComponentOptionEntry const*> allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, lookupTheme);
            if (allOptions.empty())
                allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, factionThemeID);
            if (allOptions.empty() && factionThemeID != 2)
                allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 2);
            if (allOptions.empty() && factionThemeID != 1)
                allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 1);

            // Determine door connectivity and which side spawns the door meshes.
            // Sniff-verified: only the child room (higher slotIndex) spawns DoorwayWall+Doorway.
            // The parent room (lower slotIndex) keeps its Cosmetic wall (Type=0) to fill the
            // full wall width — needed when the child room is narrower than the parent.
            bool isDoorComponent = (comp.ConnectionType != 0) &&
                (std::abs(comp.OffsetPos[0]) > 0.5f) != (std::abs(comp.OffsetPos[1]) > 0.5f);
            bool hasConnectedRoom = false;
            bool isParentSide = false; // true = keep as Cosmetic wall only (no door meshes)
            if (isDoorComponent)
            {
                ObjectGuid neighborGuid = findNeighborAtDoor(room, comp.OffsetPos[0], comp.OffsetPos[1]);
                if (!neighborGuid.IsEmpty())
                {
                    hasConnectedRoom = true;
                    auto nItr = roomByGuid.find(neighborGuid);
                    if (nItr != roomByGuid.end() && nItr->second->SlotIndex > room->SlotIndex)
                        isParentSide = true; // child room handles the DoorwayWall+Doorway
                }
            }

            // Filter options: spawn each relevant one as a separate MeshObject
            for (RoomComponentOptionEntry const* optEntry : allOptions)
            {
                // Parent side of a connection: spawn DoorwayWall (Type=1) ONLY.
                // Type=1 is a single mesh containing both the door opening AND
                // the side fillers — no Type=0 solid wall needed (that would seal
                // the door), and no Type=2 door frame (child handles that).
                if (isParentSide && optEntry->Type != 1)
                    continue;
                // Child side with connection: spawn DoorwayWall (Type=1) + Doorway (Type=2)
                if (isDoorComponent && hasConnectedRoom && !isParentSide && optEntry->Type == 0)
                    continue;
                // For non-door walls: only spawn Type=0 (Cosmetic) options
                if (!isDoorComponent && optEntry->Type != 0)
                    continue;
                // Unconnected door walls: only Cosmetic
                if (isDoorComponent && !hasConnectedRoom && optEntry->Type != 0)
                    continue;

                int32 compFileDataID = optEntry->ModelFileDataID > 0 ? optEntry->ModelFileDataID : comp.ModelFileDataID;
                if (compFileDataID <= 0)
                    continue; // No model for this option
                int32 roomComponentOptionID = static_cast<int32>(optEntry->ID);
                int32 houseThemeID = sHousingMgr.GetDefaultSubThemeID(optEntry->HouseThemeID);
                int32 field24 = static_cast<int32>(optEntry->SubType);
                uint8 field20 = static_cast<uint8>(optEntry->Type); // 0=Cosmetic, 1=DoorwayWall, 2=Doorway

                // Per-component-type texture resolution (sniff-verified: wall/floor/ceiling store independently)
                int32 roomComponentTextureID = 0;
                uint32 storedTexture = 0;
                switch (comp.Type)
                {
                    case HOUSING_ROOM_COMPONENT_WALL:
                    case HOUSING_ROOM_COMPONENT_DOORWAY_WALL:
                        storedTexture = room->WallTextureId;
                        break;
                    case HOUSING_ROOM_COMPONENT_FLOOR:
                        storedTexture = room->FloorTextureId;
                        break;
                    case HOUSING_ROOM_COMPONENT_CEILING:
                        storedTexture = room->CeilingTextureId;
                        break;
                    default:
                        break;
                }
                if (storedTexture != 0)
                    roomComponentTextureID = static_cast<int32>(storedTexture);
                else
                {
                    roomComponentTextureID = sHousingMgr.GetTextureIdForComponentOption(roomComponentOptionID);
                    if (roomComponentTextureID == 0)
                        roomComponentTextureID = sHousingMgr.GetTextureIdForComponentType(comp.Type);
                    if (roomComponentTextureID == 0)
                    {
                        switch (comp.Type)
                        {
                            case 1: roomComponentTextureID = 24; break;
                            case 2: roomComponentTextureID = 40; break;
                            case 3: roomComponentTextureID = 54; break;
                            default: break;
                        }
                    }
                }

            // Debug: log stairwell spawns so we can see what FileDataID/option is chosen
            if (isStairwell)
                TC_LOG_ERROR("housing", "  STAIRWELL SPAWN comp={} Type={} MSFID={} -> option={} optType={} optSubType={} "
                    "FDID={} theme={} (lookupTheme={} factionTheme={})",
                    comp.ID, comp.Type, comp.MeshStyleFilterID,
                    roomComponentOptionID, uint32(field20), field24, compFileDataID,
                    optEntry->HouseThemeID, lookupTheme, factionThemeID);

            MeshObject* componentMesh = MeshObject::CreateMeshObject(this, compPos, compRot, 1.0f,
                compFileDataID, /*isWMO*/ true,
                roomHousingGuid, /*attachFlags*/ 3, &roomPos);

            if (!componentMesh)
            {
                TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: "
                    "CreateMeshObject failed for component (compID={}, fileDataID={}, roomEntry={})",
                    comp.ID, compFileDataID, room->RoomEntryId);
                continue;
            }

            PhasingHandler::InitDbPhaseShift(componentMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
            componentMesh->InitHousingRoomComponentData(roomHousingGuid,
                roomComponentOptionID, static_cast<int32>(comp.ID),
                comp.Type, field24, field20,
                houseThemeID, roomComponentTextureID,
                /*roomComponentTypeParam*/ 0,
                geoMinX, geoMinY, geoMinZ,
                geoMaxX, geoMaxY, geoMaxZ);

            componentMeshes.push_back(componentMesh);

            } // end for (allOptions)

            // Count by component type for diagnostic summary
            switch (comp.Type)
            {
                case HOUSING_ROOM_COMPONENT_WALL: ++wallCount; break;
                case HOUSING_ROOM_COMPONENT_FLOOR: ++floorCount; break;
                case HOUSING_ROOM_COMPONENT_CEILING: ++ceilingCount; break;
                case HOUSING_ROOM_COMPONENT_STAIRS: ++stairsCount; break;
                case HOUSING_ROOM_COMPONENT_PILLAR: ++pillarCount; break;
                case HOUSING_ROOM_COMPONENT_DOORWAY_WALL: ++doorwayWallCount; break;
                case HOUSING_ROOM_COMPONENT_DOORWAY: ++doorwayCount; break;
                default: ++otherCount; break;
            }
        }

        TC_LOG_ERROR("housing", "  Room '{}' component summary: walls={} floors={} ceilings={} "
            "doorways={} stairs={} pillars={} doorwayWalls={} other={} total={}",
            roomData->Name, wallCount, floorCount, ceilingCount, doorwayCount,
            stairsCount, pillarCount, doorwayWallCount, otherCount, uint32(components->size()));

        // --- Phase 2: Add all component MeshObjects to map ---

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

        // --- Phase 4: Populate HousingRoomEntity with component GUIDs + door data ---
        // The entity was created in Phase 1 (before components) so it exists when
        // components reference it via AttachParentGUID. Now add the child list + doors.
        {
            for (MeshObject* comp : componentMeshes)
            {
                if (comp->IsInWorld())
                    housingRoom->AddMeshObject(comp->GetGUID());
            }

            // Add one door entry per door-capable WALL slot. A door-capable slot
            // is a WALL (not Doorway/DoorwayWall — those are visual overlays of a
            // connected wall) with ConnectionType>0 and exactly one of X/Y
            // nonzero (skip vertical ceiling/floor doors — those go through the
            // stairwell partner stack and would trip the client's connected-door
            // count). Advertise both connected and unconnected slots: the client
            // renders a fixture handle on doors with empty AttachedRoomGUID and
            // hides it on connected doors.
            uint32 doorCount = 0;
            for (RoomComponentData const& comp : *components)
            {
                if (comp.Type != HOUSING_ROOM_COMPONENT_WALL) continue;
                if (comp.ConnectionType == 0) continue;
                bool hasX = std::abs(comp.OffsetPos[0]) > 0.5f;
                bool hasY = std::abs(comp.OffsetPos[1]) > 0.5f;
                if (hasX == hasY) continue;

                ObjectGuid attachedRoomGuid = findNeighborAtDoor(room, comp.OffsetPos[0], comp.OffsetPos[1]);

                Position doorOffset(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2]);
                uint8 connType = comp.ConnectionType;
                housingRoom->AddDoor(static_cast<int32>(comp.ID), doorOffset, connType, attachedRoomGuid);
                ++doorCount;
            }

            TC_LOG_ERROR("housing", "HouseInteriorMap::SpawnRoomMeshObjects: Created HousingRoomEntity "
                "guid={} roomEntry={} slot={} meshObjects={} doors={}",
                roomHousingGuid.ToString(), room->RoomEntryId, room->SlotIndex,
                uint32(componentMeshes.size()), doorCount);
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

    // Use immediate removal (RemoveFromMap) instead of deferred (AddObjectToRemoveList).
    // Deferred removal causes client crashes when new entities are created in the same
    // update cycle — the client sees overlapping CREATE/DESTROY for the same GUIDs.
    for (auto& [roomGuid, meshGuids] : _roomMeshObjects)
    {
        for (ObjectGuid const& meshGuid : meshGuids)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
            {
                RemoveFromMap(mesh, true);
                ++despawnCount;
            }
        }
    }

    // Also despawn HousingRoomEntities — they must be removed before
    // SpawnRoomMeshObjects recreates them with the same GUIDs.
    for (HousingRoomEntity* roomEntity : _roomEntities)
    {
        if (roomEntity && roomEntity->IsInWorld())
        {
            RemoveFromMap(roomEntity, true);
            ++despawnCount;
        }
    }
    _roomEntities.clear();

    _roomMeshObjects.clear();
    _roomsSpawned = false;

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnAllRoomMeshObjects: Despawned {} objects (owner={})",
        despawnCount, _owner.ToString());
}

void HouseInteriorMap::DespawnRoomEntities(ObjectGuid roomGuid)
{
    uint32 despawnCount = 0;

    // Remove this room's MeshObjects
    auto itr = _roomMeshObjects.find(roomGuid);
    if (itr != _roomMeshObjects.end())
    {
        for (ObjectGuid const& meshGuid : itr->second)
        {
            if (MeshObject* mesh = GetMeshObject(meshGuid))
            {
                RemoveFromMap(mesh, true);
                ++despawnCount;
            }
        }
        _roomMeshObjects.erase(itr);
    }

    // Remove this room's HousingRoomEntity
    for (auto it = _roomEntities.begin(); it != _roomEntities.end(); ++it)
    {
        HousingRoomEntity* entity = *it;
        if (entity && entity->IsInWorld() && entity->GetGUID() == roomGuid)
        {
            RemoveFromMap(entity, true);
            _roomEntities.erase(it);
            ++despawnCount;
            break;
        }
    }

    TC_LOG_DEBUG("housing", "HouseInteriorMap::DespawnRoomEntities: Removed {} entities for room {} (owner={})",
        despawnCount, roomGuid.ToString(), _owner.ToString());
}

void HouseInteriorMap::ReplaceWallWithDoorway(ObjectGuid roomGuid, uint32 doorComponentID,
    int32 factionRestriction, Housing::Room const& room, ObjectGuid newRoomGuid)
{
    // The parent room keeps its Cosmetic wall (Type=0) to fill the full wall width.
    // The child room's DoorwayWall+Doorway renders over it to create the door opening.
    // This function is now a no-op — the parent's wall stays as-is.
    // Replace the parent's Cosmetic wall (Type=0) with DoorwayWall (Type=1) only.
    // Type=1 has the door opening + side fillers. The child room handles the Doorway (Type=2).
    auto meshItr = _roomMeshObjects.find(roomGuid);
    if (meshItr == _roomMeshObjects.end())
        return;

    // Destroy existing Cosmetic wall for this component
    ObjectGuid wallMeshGuid;
    Position wallPos;
    QuaternionData wallRot;
    for (ObjectGuid const& meshGuid : meshItr->second)
    {
        if (MeshObject* mesh = GetMeshObject(meshGuid))
        {
            if (mesh->GetRoomComponentID() == static_cast<int32>(doorComponentID))
            {
                wallMeshGuid = meshGuid;
                wallPos = mesh->GetLocalPosition();
                wallRot = mesh->GetLocalRotation();
                break;
            }
        }
    }

    if (wallMeshGuid.IsEmpty())
        return;

    MeshObject* wallMesh = GetMeshObject(wallMeshGuid);
    if (!wallMesh) return;

    wallMesh->DestroyForNearbyPlayers();
    RemoveFromMap(wallMesh, true);
    meshItr->second.erase(
        std::remove(meshItr->second.begin(), meshItr->second.end(), wallMeshGuid),
        meshItr->second.end());

    // Spawn DoorwayWall (Type=1) only — the child room handles Doorway (Type=2)
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    if (!roomData) return;
    std::vector<RoomComponentData> const* components = sHousingMgr.GetRoomComponents(roomData->RoomWmoDataID);
    if (!components) return;

    RoomComponentData const* doorComp = nullptr;
    for (auto const& c : *components)
        if (c.ID == doorComponentID) { doorComp = &c; break; }
    if (!doorComp) return;

    int32 lookupTheme = (room.ThemeId != 0) ? static_cast<int32>(room.ThemeId) : sHousingMgr.GetFactionDefaultThemeID(factionRestriction);
    auto allOptions = sHousingMgr.FindAllRoomComponentOptions(doorComp->MeshStyleFilterID, lookupTheme);

    float roomX = _originX + static_cast<float>(room.GridX);
    float roomY = _originY + static_cast<float>(room.GridY);
    Position roomPos(roomX, roomY, _originZ, 0.0f);

    for (RoomComponentOptionEntry const* optEntry : allOptions)
    {
        if (optEntry->Type != 1) // Only DoorwayWall
            continue;

        int32 compFileDataID = optEntry->ModelFileDataID > 0 ? optEntry->ModelFileDataID : doorComp->ModelFileDataID;
        int32 houseThemeID = sHousingMgr.GetDefaultSubThemeID(optEntry->HouseThemeID);

        MeshObject* doorMesh = MeshObject::CreateMeshObject(this, wallPos, wallRot, 1.0f,
            compFileDataID, true, roomGuid, 3, &roomPos);
        if (!doorMesh) continue;

        PhasingHandler::InitDbPhaseShift(doorMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
        doorMesh->InitHousingRoomComponentData(roomGuid,
            static_cast<int32>(optEntry->ID), static_cast<int32>(doorComp->ID),
            doorComp->Type, static_cast<int32>(optEntry->SubType), static_cast<uint8>(optEntry->Type),
            houseThemeID, 24, 0,
            0, 0, 0, 0, 0, 0);

        if (AddToMap(doorMesh))
            meshItr->second.push_back(doorMesh->GetGUID());
        else
            delete doorMesh;
    }

    // Update room entity's MeshObjects list
    for (HousingRoomEntity* re : _roomEntities)
    {
        if (re && re->IsInWorld() && re->GetGUID() == roomGuid)
        {
            re->ReplaceMeshObjects(meshItr->second);
            break;
        }
    }

    TC_LOG_DEBUG("housing", "ReplaceWallWithDoorway: room={} compID={} — replaced Cosmetic with DoorwayWall(Type=1)",
        roomGuid.ToString(), doorComponentID);
}

void HouseInteriorMap::UpdateRoomComponentTextures(ObjectGuid roomGuid, Housing::Room const& room,
    std::vector<uint32> const* componentIDs, int32 textureID)
{
    // Material/texture-only change: update existing MeshObjects in-place via UPDATE_OBJECT.
    // No model change needed — only the RoomComponentTextureID field changes.
    auto itr = _roomMeshObjects.find(roomGuid);
    if (itr == _roomMeshObjects.end())
        return;

    uint32 matchCount = 0;
    for (ObjectGuid const& meshGuid : itr->second)
    {
        MeshObject* mesh = GetMeshObject(meshGuid);
        if (!mesh) continue;
        int32 compID = mesh->GetRoomComponentID();
        if (compID == 0) continue;

        bool match = !componentIDs || componentIDs->empty();
        if (!match)
            for (uint32 cid : *componentIDs)
                if (static_cast<int32>(cid) == compID) { match = true; break; }
        if (!match) continue;

        mesh->UpdateRoomComponentVisuals(
            mesh->GetRoomComponentOptionID(),
            mesh->GetHouseThemeID(),
            textureID);
        ++matchCount;
    }
    TC_LOG_DEBUG("housing", "UpdateRoomComponentTextures: room={} textureID={} matched={}", roomGuid.ToString(), textureID, matchCount);
}

void HouseInteriorMap::RespawnRoomComponentsForTheme(ObjectGuid roomGuid, int32 factionRestriction,
    Housing::Room const& room, std::vector<uint32> const* componentIDs, int32 newThemeID,
    int32 overrideSubType /*= -1*/, int32 overrideRoomCompID /*= -1*/)
{
    // Theme changes require different models (FileDataIDs), so we DESTROY old meshes
    // and CREATE new ones with new GUIDs. The client shows walls disappearing/reappearing.
    auto meshItr = _roomMeshObjects.find(roomGuid);
    if (meshItr == _roomMeshObjects.end())
        return;

    int32 factionThemeID = sHousingMgr.GetFactionDefaultThemeID(factionRestriction);
    int32 effectiveThemeID = sHousingMgr.GetBaseThemeID(newThemeID);

    // Find the room entity for attachment
    ObjectGuid roomHousingGuid;
    Position roomPos;
    for (HousingRoomEntity* re : _roomEntities)
    {
        if (re && re->GetGUID() == roomGuid)
        {
            roomHousingGuid = re->GetGUID();
            roomPos = re->GetPosition();
            break;
        }
    }
    if (roomHousingGuid.IsEmpty())
    {
        TC_LOG_ERROR("housing", "RespawnRoomComponentsForTheme: room entity {} not found", roomGuid.ToString());
        return;
    }

    // Phase 1: Record existing mesh option Types before destroying.
    // This preserves door vs plain wall state — we only spawn options whose Type
    // matches what was there before (prevents doors appearing on every wall).
    struct OldMeshInfo { ObjectGuid guid; int32 compID; uint8 optType; uint8 optSubType; int32 textureID; };
    std::vector<OldMeshInfo> toDestroy;

    for (ObjectGuid const& meshGuid : meshItr->second)
    {
        MeshObject* mesh = GetMeshObject(meshGuid);
        if (!mesh) continue;
        int32 compID = mesh->GetRoomComponentID();
        if (compID == 0) continue;

        bool match = !componentIDs || componentIDs->empty();
        if (!match)
            for (uint32 cid : *componentIDs)
                if (static_cast<int32>(cid) == compID) { match = true; break; }
        if (!match) continue;

        // Record option Type+SubType+TextureID from the current mesh to preserve during respawn
        uint8 optType = 0, optSubType = 0;
        int32 optionID = mesh->GetRoomComponentOptionID();
        if (RoomComponentOptionEntry const* opt = sRoomComponentOptionStore.LookupEntry(static_cast<uint32>(optionID)))
        {
            optType = opt->Type;
            optSubType = opt->SubType;
        }
        toDestroy.push_back({ meshGuid, compID, optType, optSubType, mesh->GetRoomComponentTextureID() });
    }

    // Destroy old meshes
    for (auto const& info : toDestroy)
    {
        MeshObject* mesh = GetMeshObject(info.guid);
        if (!mesh) continue;
        mesh->DestroyForNearbyPlayers();
        RemoveFromMap(mesh, true);
        auto& guids = meshItr->second;
        guids.erase(std::remove(guids.begin(), guids.end(), info.guid), guids.end());
    }

    // Phase 2: Create new meshes for each destroyed mesh, matching by (compID, Type, SubType)
    // Build a per-compID list of (Type, SubType, TextureID) that need respawning
    struct RespawnSlot { uint8 optType; uint8 optSubType; int32 textureID; };
    std::unordered_map<int32, std::vector<RespawnSlot>> compTypeMap;
    for (auto const& info : toDestroy)
        compTypeMap[info.compID].push_back({ info.optType, info.optSubType, info.textureID });

    // Look up the room's components from DB2
    HouseRoomData const* roomData = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
    if (!roomData)
        return;
    std::vector<RoomComponentData> const* components = sHousingMgr.GetRoomComponents(roomData->RoomWmoDataID);
    if (!components)
        return;

    uint32 spawnedCount = 0;
    for (RoomComponentData const& comp : *components)
    {
        auto typeItr = compTypeMap.find(static_cast<int32>(comp.ID));
        if (typeItr == compTypeMap.end())
            continue;

        Position compPos(comp.OffsetPos[0], comp.OffsetPos[1], comp.OffsetPos[2], 0.0f);
        QuaternionData compRot;
        static constexpr float DEG_TO_RAD = static_cast<float>(M_PI / 180.0);
        float rx = comp.OffsetRot[0] * DEG_TO_RAD;
        float ry = comp.OffsetRot[1] * DEG_TO_RAD;
        float rz = -comp.OffsetRot[2] * DEG_TO_RAD;
        float cx = std::cos(rx / 2.0f), sx = std::sin(rx / 2.0f);
        float cy = std::cos(ry / 2.0f), sy = std::sin(ry / 2.0f);
        float cz = std::cos(rz / 2.0f), sz = std::sin(rz / 2.0f);
        compRot.x = sx * cy * cz - cx * sy * sz;
        compRot.y = cx * sy * cz + sx * cy * sz;
        compRot.z = cx * cy * sz - sx * sy * cz;
        compRot.w = cx * cy * cz + sx * sy * sz;

        // Find all options for new theme
        auto allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, effectiveThemeID);
        if (allOptions.empty())
            allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, factionThemeID);
        if (allOptions.empty() && factionThemeID != 2)
            allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 2);
        if (allOptions.empty() && factionThemeID != 1)
            allOptions = sHousingMgr.FindAllRoomComponentOptions(comp.MeshStyleFilterID, 1);

        // For each old mesh's (Type, SubType), find the equivalent in new theme.
        for (auto const& slot : typeItr->second)
        {
            uint8 targetSubType = (overrideSubType >= 0) ? static_cast<uint8>(overrideSubType) : slot.optSubType;

            RoomComponentOptionEntry const* bestOpt = nullptr;
            for (auto const* opt : allOptions)
            {
                // If overrideRoomCompID is set, only match options with that RoomCompID
                // (used for ceiling/door type selection: normal=0, vaulted=1, etc.)
                if (overrideRoomCompID >= 0 && opt->RoomComponentID != overrideRoomCompID)
                    continue;
                if (opt->Type == slot.optType && opt->SubType == targetSubType)
                { bestOpt = opt; break; }
            }
            // Fallback: match Type only (still respecting RoomCompID filter)
            if (!bestOpt)
                for (auto const* opt : allOptions)
                {
                    if (overrideRoomCompID >= 0 && opt->RoomComponentID != overrideRoomCompID)
                        continue;
                    if (opt->Type == slot.optType) { bestOpt = opt; break; }
                }
            // Last resort: any option matching RoomCompID
            if (!bestOpt)
                for (auto const* opt : allOptions)
                {
                    if (overrideRoomCompID >= 0 && opt->RoomComponentID != overrideRoomCompID)
                        continue;
                    bestOpt = opt; break;
                }
            if (!bestOpt)
                continue;

            int32 compFileDataID = bestOpt->ModelFileDataID > 0 ? bestOpt->ModelFileDataID : comp.ModelFileDataID;
            if (compFileDataID <= 0)
                continue;

            int32 roomComponentOptionID = static_cast<int32>(bestOpt->ID);
            int32 houseThemeID = (newThemeID != 0) ? newThemeID : sHousingMgr.GetDefaultSubThemeID(bestOpt->HouseThemeID);

            // Preserve the old mesh's texture during theme respawn (don't reset material).
            // Only fall back to defaults if the old texture was 0.
            int32 roomComponentTextureID = slot.textureID;
            if (roomComponentTextureID == 0)
            {
                roomComponentTextureID = sHousingMgr.GetTextureIdForComponentOption(roomComponentOptionID);
                if (roomComponentTextureID == 0)
                    roomComponentTextureID = sHousingMgr.GetTextureIdForComponentType(comp.Type);
            }

            MeshObject* newMesh = MeshObject::CreateMeshObject(this, compPos, compRot, 1.0f,
                compFileDataID, /*isWMO*/ true, roomHousingGuid, /*attachFlags*/ 3, &roomPos);
            if (!newMesh) continue;

            PhasingHandler::InitDbPhaseShift(newMesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
            // Set RoomComponentTypeParam to overrideRoomCompID for ceiling/door type changes
            // so the client's menu reflects the current type (normal=0, vaulted=1, etc.)
            int32 typeParam = (overrideRoomCompID >= 0) ? overrideRoomCompID : 0;

            newMesh->InitHousingRoomComponentData(roomHousingGuid,
                roomComponentOptionID, static_cast<int32>(comp.ID),
                comp.Type, static_cast<int32>(bestOpt->SubType), static_cast<uint8>(bestOpt->Type),
                houseThemeID, roomComponentTextureID, typeParam,
                0, 0, 0, 0, 0, 0);

            if (AddToMap(newMesh))
            {
                meshItr->second.push_back(newMesh->GetGUID());
                ++spawnedCount;
            }
            else
                delete newMesh;
        }
    }

    // Update the HousingRoomEntity's MeshObjects dynamic array so the client
    // references the new GUIDs instead of the stale destroyed ones (prevents crash).
    for (HousingRoomEntity* re : _roomEntities)
    {
        if (re && re->GetGUID() == roomGuid)
        {
            re->ReplaceMeshObjects(meshItr->second);
            break;
        }
    }

    TC_LOG_INFO("housing", "RespawnRoomComponentsForTheme: room={} theme={} destroyed={} spawned={}",
        roomGuid.ToString(), newThemeID, toDestroy.size(), spawnedCount);
}

void HouseInteriorMap::SpawnInteriorDecor(Housing* housing)
{
    if (!housing)
        return;
    std::vector<Housing::PlacedDecor> tmp;
    tmp.reserve(housing->GetPlacedDecorMap().size());
    for (auto const& [_, decor] : housing->GetPlacedDecorMap())
        tmp.push_back(decor);
    SpawnInteriorDecorFromList(tmp, housing->GetHouseGuid());
}

void HouseInteriorMap::SpawnInteriorDecorFromList(std::vector<Housing::PlacedDecor> const& placedDecor, ObjectGuid houseGuid)
{
    uint32 spawnCount = 0;
    uint32 exteriorSkipped = 0;
    uint32 totalDecor = uint32(placedDecor.size());

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

    for (Housing::PlacedDecor const& decor : placedDecor)
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

        ObjectGuid roomEntityGuid = decor.RoomGuid;
        Position roomWorldPos;

        Housing* ownerHousing = GetOwnerHousing();
        if (ownerHousing && !roomEntityGuid.IsEmpty())
        {
            for (Housing::Room const* rm : ownerHousing->GetRooms())
            {
                if (rm->Guid == roomEntityGuid)
                {
                    static constexpr float FLOOR_HEIGHT = 12.0f;
                    roomWorldPos = Position(_originX + static_cast<float>(rm->GridX),
                                            _originY + static_cast<float>(rm->GridY),
                                            _originZ + static_cast<float>(rm->FloorIndex) * FLOOR_HEIGHT, 0.0f);
                    break;
                }
            }
        }

        float worldX = decor.PosX;
        float worldY = decor.PosY;
        float worldZ = decor.PosZ;
        LoadGrid(worldX, worldY);

        QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

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
        float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
        uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

        // Functional decor branch (real GameObject — chair, chest, mailbox, etc.)
        bool spawnedAsGo = false;
        if (decorData->GameObjectID > 0)
        {
            uint32 goEntry = static_cast<uint32>(decorData->GameObjectID);
            if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(goEntry))
            {
                float orientation = 2.0f * std::atan2(rot.z, rot.w);
                Position goWorldPos(worldX, worldY, worldZ, orientation);

                GameObject* go = GameObject::CreateGameObject(goEntry, this, goWorldPos, rot,
                    255, GO_STATE_READY, 0);
                if (go)
                {
                    PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
                    go->SetObjectScale(decorScale);
                    // Template default flags — keep CHAIR/CHEST/MAILBOX interactive behavior.
                    go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                        roomEntityGuid, decor.SourceType, decor.SourceValue);
                    go->InitHousingDecorMirroredPosition(localPos, rot, decorScale, roomEntityGuid, attachFlags);

                    if (AddToMap(go))
                    {
                        _decorGuidToObjGuid[decor.Guid] = go->GetGUID();
                        ++spawnCount;
                        spawnedAsGo = true;
                        TC_LOG_INFO("housing", "HouseInteriorMap::SpawnInteriorDecor: Spawned functional-decor "
                            "GameObject entry={} goEntry={} goType={} goGuid={} at world({:.1f},{:.1f},{:.1f}) room={}",
                            decor.DecorEntryId, goEntry, uint32(goTemplate->type), go->GetGUID().ToString(),
                            worldX, worldY, worldZ, roomEntityGuid.ToString());
                    }
                    else
                    {
                        delete go;
                    }
                }
            }
        }

        if (spawnedAsGo)
            continue;

        // Visual-only branch (MeshObject)
        int32 fileDataID = decorData->ModelFileDataID;
        if (fileDataID <= 0 && decorData->GameObjectID > 0)
        {
            if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(
                    static_cast<uint32>(decorData->GameObjectID)))
            {
                if (GameObjectDisplayInfoEntry const* displayInfo =
                        sGameObjectDisplayInfoStore.LookupEntry(goTemplate->displayId))
                {
                    if (displayInfo->FileDataID > 0)
                        fileDataID = displayInfo->FileDataID;
                }
            }
        }

        if (fileDataID <= 0)
        {
            TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnInteriorDecor: Cannot derive FileDataID for decor entry {} "
                "(GameObjectID={}, ModelFileDataID={}), skipping",
                decor.DecorEntryId, decorData->GameObjectID, decorData->ModelFileDataID);
            continue;
        }

        MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, decorScale,
            fileDataID, /*isWMO*/ false, roomEntityGuid, attachFlags, &worldPos);

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
    // If RoomGuid is empty, the decor was placed without room association.
    // This can happen when placed via the interior editor before room entities existed.
    // Skip truly exterior decor, but allow interior-placed decor through.
    if (decor.RoomGuid.IsEmpty())
    {
        TC_LOG_DEBUG("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Decor {} has empty RoomGuid, skipping",
            decor.Guid.ToString());
        return;
    }

    // Already spawned?
    if (_decorGuidToObjGuid.count(decor.Guid))
        return;

    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decor.DecorEntryId);
    if (!decorData)
        return;

    // Decor attaches to the HousingRoomEntity (Housing/2 GUID), not a MeshObject.
    ObjectGuid roomEntityGuid = decor.RoomGuid;
    Position roomWorldPos;

    // If decor has no RoomGuid (placed before room entity system), auto-assign
    // to the first non-base room. Without a valid parent, decor is not selectable.
    Housing* ownerHousing = GetOwnerHousing();
    if (roomEntityGuid.IsEmpty() && ownerHousing)
    {
        for (Housing::Room const* rm : ownerHousing->GetRooms())
        {
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(rm->RoomEntryId);
            if (rd && !rd->IsBaseRoom())
            {
                roomEntityGuid = rm->Guid;
                break;
            }
        }
        if (roomEntityGuid.IsEmpty())
        {
            for (Housing::Room const* rm : ownerHousing->GetRooms())
            {
                roomEntityGuid = rm->Guid;
                break;
            }
        }
    }

    // Find the room's world position from its grid coordinates
    if (ownerHousing)
    {
        for (Housing::Room const* rm : ownerHousing->GetRooms())
        {
            if (rm->Guid == roomEntityGuid)
            {
                static constexpr float FLOOR_HEIGHT = 7.0f;
                roomWorldPos = Position(_originX + static_cast<float>(rm->GridX),
                                        _originY + static_cast<float>(rm->GridY),
                                        _originZ + static_cast<float>(rm->FloorIndex) * FLOOR_HEIGHT, 0.0f);
                break;
            }
        }
    }

    float worldX = decor.PosX, worldY = decor.PosY, worldZ = decor.PosZ;
    LoadGrid(worldX, worldY);

    QuaternionData rot(decor.RotationX, decor.RotationY, decor.RotationZ, decor.RotationW);

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
    float decorScale = decor.Scale > 0.01f ? decor.Scale : 1.0f;
    uint8 attachFlags = roomEntityGuid.IsEmpty() ? uint8(0) : uint8(3);

    // Functional decor (chair, chest, mailbox, etc.): spawn real GameObject so it stays interactive.
    if (decorData->GameObjectID > 0)
    {
        uint32 goEntry = static_cast<uint32>(decorData->GameObjectID);
        if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(goEntry))
        {
            float orientation = 2.0f * std::atan2(rot.z, rot.w);
            Position goWorldPos(worldX, worldY, worldZ, orientation);

            GameObject* go = GameObject::CreateGameObject(goEntry, this, goWorldPos, rot,
                255, GO_STATE_READY, 0);
            if (go)
            {
                PhasingHandler::InitDbPhaseShift(go->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
                go->SetObjectScale(decorScale);
                go->ReplaceAllFlags(GameObjectFlags(0x40000));
                go->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0,
                    roomEntityGuid, decor.SourceType, decor.SourceValue);
                go->InitHousingDecorMirroredPosition(localPos, rot, decorScale, roomEntityGuid, attachFlags);

                if (AddToMap(go))
                {
                    _decorGuidToObjGuid[decor.Guid] = go->GetGUID();
                    TC_LOG_INFO("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Spawned functional-decor "
                        "GameObject entry={} goEntry={} goType={} goGuid={} at world({:.1f},{:.1f},{:.1f}) room={}",
                        decor.DecorEntryId, goEntry, uint32(goTemplate->type), go->GetGUID().ToString(),
                        worldX, worldY, worldZ, roomEntityGuid.ToString());
                    return;
                }
                delete go;
            }
        }
    }

    // Visual-only (MeshObject) path.
    int32 fileDataID = decorData->ModelFileDataID;
    if (fileDataID <= 0 && decorData->GameObjectID > 0)
    {
        if (GameObjectTemplate const* goTemplate = sObjectMgr->GetGameObjectTemplate(
                static_cast<uint32>(decorData->GameObjectID)))
        {
            if (GameObjectDisplayInfoEntry const* displayInfo =
                    sGameObjectDisplayInfoStore.LookupEntry(goTemplate->displayId))
            {
                if (displayInfo->FileDataID > 0)
                    fileDataID = displayInfo->FileDataID;
            }
        }
    }

    if (fileDataID <= 0)
        return;

    MeshObject* mesh = MeshObject::CreateMeshObject(this, localPos, rot, decorScale,
        fileDataID, /*isWMO*/ false, roomEntityGuid, attachFlags, &worldPos);

    if (!mesh)
        return;

    PhasingHandler::InitDbPhaseShift(mesh->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
    mesh->InitHousingDecorData(decor.Guid, houseGuid, decor.Locked ? 1 : 0, roomEntityGuid, decor.SourceType, decor.SourceValue);

    if (AddToMap(mesh))
    {
        _decorGuidToObjGuid[decor.Guid] = mesh->GetGUID();
        TC_LOG_INFO("housing", "HouseInteriorMap::SpawnSingleInteriorDecor: Spawned decor MeshObject fileDataID={} "
            "at world({:.1f},{:.1f},{:.1f}) room={}",
            fileDataID, worldX, worldY, worldZ, roomEntityGuid.ToString());
    }
    else
    {
        delete mesh;
    }
}

void HouseInteriorMap::UpdateDecorPosition(ObjectGuid decorGuid, Position const& pos, QuaternionData const& rot, float scale /*= 1.0f*/)
{
    auto itr = _decorGuidToObjGuid.find(decorGuid);
    if (itr == _decorGuidToObjGuid.end())
        return;

    ObjectGuid objGuid = itr->second;
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
        {
            go->Relocate(pos);
            go->SetLocalRotation(rot.x, rot.y, rot.z, rot.w);
            if (std::abs(go->GetObjectScale() - scale) > 0.001f)
                go->SetObjectScale(scale);
            TC_LOG_DEBUG("housing", "HouseInteriorMap::UpdateDecorPosition: Moved decor GameObject {} to ({:.1f},{:.1f},{:.1f}) scale={:.2f}",
                decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), scale);
        }
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
    {
        mesh->Relocate(pos);
        if (std::abs(mesh->GetLocalScale() - scale) > 0.001f)
            mesh->UpdateLocalScale(scale);
        TC_LOG_DEBUG("housing", "HouseInteriorMap::UpdateDecorPosition: Moved decor MeshObject {} to ({:.1f},{:.1f},{:.1f}) scale={:.2f}",
            decorGuid.ToString(), pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), scale);
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
    if (objGuid.IsGameObject())
    {
        if (GameObject* go = GetGameObject(objGuid))
            go->AddObjectToRemoveList();
    }
    else if (MeshObject* mesh = GetMeshObject(objGuid))
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

    // === PRE-SPAWN: Populate ALL housing entities BEFORE Map::AddPlayerToMap ===
    // Retail sends ONE ~210KB UPDATE_OBJECT with all entity data (rooms, decor,
    // account storage, budgets) on interior map transfer. If we spawn after
    // AddPlayerToMap, the initial UPDATE_OBJECT has empty housing data and the
    // client never gets proper housing context for the editor UI.
    //
    // Visiting an offline-owner house: `preloadHousing` is null and the player
    // isn't the map's owner. Spawn rooms/decor from PlotInfo (mirrored from
    // the DB) so visitors see the owner's actual layout without needing the
    // owner online.
    Housing* preloadHousing = player->GetHousing();
    bool visitingOfflineOwner = !_roomsSpawned && player->GetGUID() != _owner;
    if (visitingOfflineOwner)
    {
        for (Neighborhood* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(_owner))
        {
            Neighborhood::PlotInfo const* ownerPlot = nullptr;
            for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
            {
                if (plot.OwnerGuid == _owner && plot.IsOccupied())
                {
                    ownerPlot = &plot;
                    break;
                }
            }
            if (!ownerPlot)
                continue;

            int32 faction = nbh->GetFactionRestriction();
            std::vector<Housing::Room const*> roomPtrs;
            roomPtrs.reserve(ownerPlot->Rooms.size());
            for (Housing::Room const& room : ownerPlot->Rooms)
                roomPtrs.push_back(&room);
            SpawnRoomMeshObjectsFromList(roomPtrs, faction, ownerPlot->HouseGuid);
            SpawnInteriorDecorFromList(ownerPlot->Decor, ownerPlot->HouseGuid);
            _roomsSpawned = true;

            // Place the visitor at the first visual (non-base) room so they
            // don't spawn inside geometry.
            for (Housing::Room const& room : ownerPlot->Rooms)
            {
                HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(room.RoomEntryId);
                if (rd && !rd->IsBaseRoom())
                {
                    float targetX = _originX + static_cast<float>(room.GridX);
                    player->Relocate(targetX, _originY, _originZ, player->GetOrientation());
                    break;
                }
            }

            TC_LOG_INFO("housing", "HouseInteriorMap::AddPlayerToMap: VISITOR pre-spawn for offline owner {} — {} rooms, {} decor",
                _owner.ToString(), uint32(ownerPlot->Rooms.size()), uint32(ownerPlot->Decor.size()));
            break;
        }
    }

    if (preloadHousing && player->GetGUID() == _owner)
    {
        // Clear exterior fixture edit mode that persists across map transfer
        if (preloadHousing->GetEditorMode() != HOUSING_EDITOR_MODE_NONE)
        {
            preloadHousing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
            player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
            player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
            player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
        }

        preloadHousing->SetInInterior(true);

        // Spawn rooms + decor onto the map (before player enters)
        if (!_roomsSpawned)
        {
            int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
            SpawnRoomMeshObjects(preloadHousing, faction);
            SpawnInteriorDecor(preloadHousing);
            _roomsSpawned = true;
        }

        // Populate Account entity with FHousingStorage_C + budget data
        // so the initial UPDATE_OBJECT includes full housing context
        preloadHousing->PopulateCatalogStorageEntries();
        preloadHousing->SyncUpdateFields();

        // Relocate player to the visual room BEFORE map add so they
        // spawn at the correct position in the initial UPDATE_OBJECT
        for (Housing::Room const* room : preloadHousing->GetRooms())
        {
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
            if (rd && !rd->IsBaseRoom())
            {
                float targetX = _originX + static_cast<float>(room->GridX);
                player->Relocate(targetX, _originY, _originZ, player->GetOrientation());
                break;
            }
        }

        // The interior plot AreaTrigger is created in the DEFERRED callback,
        // NOT here. If we AddToMap now, the visibility system includes it in
        // the initial UPDATE_OBJECT, and the client fires HOUSE_PLOT_ENTERED
        // before Status+Permissions arrive. Retail sends the AT in a separate
        // UPDATE_OBJECT (#11752) AFTER the main entity data.

        TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: PRE-SPAWNED rooms+decor+storage "
            "(%u rooms, %u decor) before Map::AddPlayerToMap (AT deferred)",
            uint32(_roomMeshObjects.size()), uint32(_decorGuidToObjGuid.size()));
    }

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
            if (player->GetGUID() == _owner)
            {
                // Always force a fresh spawn on login — old entities from a previous binary/session
                // may have stale fragment formats (e.g., root MeshObjects with FHousingRoom_C that
                // no longer exist in the current code). DespawnAll is safe here because the player
                // hasn't received any entities yet (no DESTROY goes to the client).
                if (_roomsSpawned)
                    DespawnAllRoomMeshObjects();

                TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: === SPAWNING ROOMS ===");

                for (Housing::Room const* room : housing->GetRooms())
                {
                    TC_LOG_ERROR("housing", "  Room: guid={} entryId={} slot={} grid=({},{}) orientation={} mirrored={}",
                        room->Guid.ToString(), room->RoomEntryId, room->SlotIndex,
                        room->GridX, room->GridY, room->Orientation, room->Mirrored);
                }

                int32 faction = (player->GetTeamId() == TEAM_ALLIANCE)
                    ? NEIGHBORHOOD_FACTION_ALLIANCE : NEIGHBORHOOD_FACTION_HORDE;
                SpawnRoomMeshObjects(housing, faction);
                _roomsSpawned = true;
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
                    float targetX = _originX + static_cast<float>(room->GridX);
                    float targetY = _originY;
                    float targetZ = _originZ;
                    TC_LOG_ERROR("housing", "HouseInteriorMap::AddPlayerToMap: Teleporting player to visual room "
                        "entry={} slot={} at ({:.1f},{:.1f},{:.1f})",
                        room->RoomEntryId, room->SlotIndex, targetX, targetY, targetZ);
                    player->NearTeleportTo(targetX, targetY, targetZ, player->GetOrientation());
                    break;
                }
            }

            // Toggle WS[30906]=1 to signal the client that the player is inside a house interior.
            // Sent synchronously so the client knows it's an interior before deferred packets.
            player->SendUpdateWorldState(WORLDSTATE_HOUSING_INTERIOR, 1);

            // Defer ALL housing context packets by 500ms. The client needs time to
            // process the initial UPDATE_OBJECT (entities, room MeshObjects) before
            // housing response packets can be processed. This mirrors the exterior
            // map's deferred ENTER_PLOT pattern (HousingMap.cpp).
            // Without the delay, the housing system TLS may not be ready and the
            // client silently drops the Status/Permissions packets.
            {
                ObjectGuid playerGuid = player->GetGUID();
                ObjectGuid houseGuid = housing->GetHouseGuid();
                ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
                ObjectGuid accountGuid = player->GetSession()->GetBattlenetAccountGUID();
                uint8 plotIndex = housing->GetPlotIndex();
                uint32 settingsFlags = housing->GetSettingsFlags();

                player->m_Events.AddEventAtOffset([this, playerGuid, houseGuid, neighborhoodGuid, accountGuid, plotIndex, settingsFlags]()
                {
                    Player* p = ObjectAccessor::FindPlayer(playerGuid);
                    if (!p || !p->IsInWorld())
                        return;

                    Housing* housing = p->GetHousing();
                    if (!housing)
                        return;

                    // ENTER_PLOT is sent AFTER the AT CREATE in step 8 below.
                    // The sequence is: AT CREATE → ENTER_PLOT → re-send Status+Perms.
                    // ENTER_PLOT fires HOUSE_PLOT_ENTERED (FrameScript event 1073) which
                    // loads Blizzard_HousingControls. The handler resets editor state, so
                    // Status+Perms are re-sent afterward to re-establish context.

                    // Steps 1-3 (HouseInfo, Status, Permissions) moved to AFTER
                    // ENTER_PLOT below. ENTER_PLOT resets editor state, so sending
                    // Status+Permissions before it is wasteful. The exterior AT handler
                    // (at_housing_plot.cpp) also sends Status+Permissions AFTER ENTER_PLOT.

                    // 1) PostTutorialAuras (slots 8, 9, 50)
                    SendPostTutorialAuras(p);

                    // 5) Account CREATE + HousingPlayerHouseEntity + decor
                    {
                        housing->PopulateCatalogStorageEntries();
                        housing->SyncUpdateFields();

                        WorldSession* session = p->GetSession();
                        UpdateData storageUpdate(p->GetMapId());
                        WorldPacket storagePacket;

                        session->GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&storageUpdate, p);
                        p->m_clientGUIDs.insert(session->GetBattlenetAccount().GetGUID());

                        if (p->HaveAtClient(&session->GetHousingPlayerHouseEntity()))
                            session->GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&storageUpdate, p);
                        else
                        {
                            session->GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&storageUpdate, p);
                            p->m_clientGUIDs.insert(session->GetHousingPlayerHouseEntity().GetGUID());
                        }

                        // Decor and HousingRoomEntity CREATEs are sent by the map visibility
                        // system (AddToMap in SpawnRoomMeshObjects/SpawnInteriorDecor).
                        // Do NOT send manual CREATEs here — double-sending corrupts the
                        // client's entity state and makes decor unselectable after relog.

                        storageUpdate.BuildPacket(&storagePacket);
                        p->SendDirectMessage(&storagePacket);

                        session->GetBattlenetAccount().ClearUpdateMask(true);
                        session->GetHousingPlayerHouseEntity().ClearUpdateMask(true);

                        TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Sent Account+budget for {}",
                            playerGuid.ToString());
                    }

                    // Map-level Housing/3 entity (objectType=18) is now included in
                    // the initial UPDATE_OBJECT via Player::BuildCreateUpdateBlockForPlayer.
                    // It must be in the initial batch for the client's type-18 render init.

                    // 7) InitiativeServiceStatus
                    {
                        WorldPackets::Housing::InitiativeServiceStatus initStatus;
                        initStatus.ServiceEnabled = true;
                        p->SendDirectMessage(initStatus.Write());
                    }

                    // 7) Create AND send the interior plot AreaTrigger LAST.
                    // The AT must be created here (not in pre-spawn) because if it's
                    // on the map during AddPlayerToMap, the visibility system includes
                    // it in the initial UPDATE_OBJECT — before Status+Permissions.
                    // Retail sends the AT in a separate UPDATE_OBJECT (#11752) AFTER
                    // the main entity data. The client fires HOUSE_PLOT_ENTERED on AT
                    // receipt and immediately checks IsHouseEditorStatusAvailable(),
                    // which requires permissions to already be set.
                    if (_interiorPlotAT.IsEmpty())
                    {
                        float atX = _originX;
                        float atY = _originY;
                        float atZ = _originZ;
                        LoadGrid(atX, atY);

                        Position atPos(atX, atY, atZ, 0.0f);
                        AreaTrigger* plotAt = AreaTrigger::CreateStaticAreaTrigger(
                            { .Id = 37358, .IsCustom = false }, this, atPos, -1, /*addToMap*/ false);

                        if (plotAt)
                        {
                            PhasingHandler::InitDbPhaseShift(plotAt->GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);
                            // 12.0.5: FHousingPlotAreaTrigger_C fragment removed. Plot ownership
                            // propagates via PlayerHouseInfoComponentData.CurrentHouse; AT only
                            // carries its own visual fields.
                            plotAt->InitHousingPlotVisuals();

                            if (AddToMap(plotAt))
                            {
                                _interiorPlotAT = plotAt->GetGUID();

                                // Send CREATE directly to the player (visibility system
                                // won't send it since the player is already on the map)
                                UpdateData atUpdate(p->GetMapId());
                                plotAt->BuildCreateUpdateBlockForPlayer(&atUpdate, p);
                                p->m_clientGUIDs.insert(plotAt->GetGUID());

                                WorldPacket atPacket;
                                atUpdate.BuildPacket(&atPacket);
                                p->SendDirectMessage(&atPacket);

                                TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Created+sent interior plot AT "
                                    "guid={} at ({:.1f},{:.1f},{:.1f}) plotIndex={} for {}",
                                    plotAt->GetGUID().ToString(), atX, atY, atZ,
                                    housing->GetPlotIndex(), playerGuid.ToString());

                                // 8) Plot enter spell packets — same as exterior AT overlap.
                                // Sniff-verified: exterior sends 3 spell+aura sequences
                                // (1239847@slot50, 469226@slot56, 1266699@slot9) before
                                // ENTER_PLOT. These trigger editor availability on the client.
                                {
                                    plotAt->SetAreaTriggerFlag(AreaTriggerFieldFlags::HasPlayers);

                                    // Spell 1239847 at slot 50 (plot enter tracking)
                                    {
                                        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
                                            SPELL_CAST_SOURCE_NORMAL, p->GetMapId(), SPELL_HOUSING_PLOT_ENTER,
                                            GenerateLowGuid<HighGuid::Cast>());
                                        WorldPackets::Spells::AuraUpdate au;
                                        au.UpdateAll = false;
                                        au.UnitGUID = p->GetGUID();
                                        WorldPackets::Spells::AuraInfo ai;
                                        ai.Slot = 55; // sniff-verified: retail uses slot 55
                                        ai.AuraData.emplace();
                                        ai.AuraData->CastID = castId;
                                        ai.AuraData->SpellID = SPELL_HOUSING_PLOT_ENTER;
                                        ai.AuraData->Flags = AFLAG_NOCASTER;
                                        ai.AuraData->ActiveFlags = 1; // sniff-verified: retail uses 1
                                        ai.AuraData->CastLevel = 36;
                                        au.Auras.push_back(std::move(ai));
                                        p->SendDirectMessage(au.Write());
                                    }

                                    // Spell 469226 at slot 56 (plot context)
                                    {
                                        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
                                            SPELL_CAST_SOURCE_NORMAL, p->GetMapId(), SPELL_HOUSING_PLOT_PRESENCE,
                                            GenerateLowGuid<HighGuid::Cast>());
                                        WorldPackets::Spells::AuraUpdate au;
                                        au.UpdateAll = false;
                                        au.UnitGUID = p->GetGUID();
                                        WorldPackets::Spells::AuraInfo ai;
                                        ai.Slot = 56;
                                        ai.AuraData.emplace();
                                        ai.AuraData->CastID = castId;
                                        ai.AuraData->SpellID = SPELL_HOUSING_PLOT_PRESENCE;
                                        ai.AuraData->Flags = AFLAG_NOCASTER;
                                        ai.AuraData->ActiveFlags = 1;
                                        ai.AuraData->CastLevel = 36;
                                        au.Auras.push_back(std::move(ai));
                                        p->SendDirectMessage(au.Write());
                                    }

                                    TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Sent plot enter spells (slots 50+56) for {}",
                                        playerGuid.ToString());
                                }

                                // 12.0.5: SMSG_NEIGHBORHOOD_PLAYER_ENTER_PLOT is gone. The
                                // HOUSE_PLOT_ENTERED client event is now triggered by the
                                // UPDATE_OBJECT carrying PlayerHouseInfoComponent.CurrentHouse.
                                // Set CurrentHouse to the house GUID for the interior plot.
                                if (Housing const* ownerHousingForCurrent = GetOwnerHousing())
                                    p->SetCurrentHouse(ownerHousingForCurrent->GetHouseGuid());

                                TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Set CurrentHouse for {} (Status+Perms reactive via CMSG handlers)",
                                    playerGuid.ToString());
                            }
                            else
                            {
                                TC_LOG_ERROR("housing", "HouseInteriorMap deferred: AddToMap failed for interior plot AT");
                                delete plotAt;
                            }
                        }
                    }

                    // 10) Spawn the interior exit door — blizzlike decor entity + GO hierarchy.
                    // Retail sniff: A HousingDecorEntity (Object Type 18, Housing/56 GUID) with
                    // FHousingDecor_C fragment carries TargetGameObjectGUID pointing to the door GO.
                    // The GO (type GOOBER) attaches to the decor entity via TransportGUID with
                    // PositionLocalSpace=(0,0,0) and AttachmentFlags=7.
                    // Alliance entry=575017 (displayId=113554), Horde entry=587318.
                    {
                        TC_LOG_INFO("housing", "InteriorDoor: Starting blizzlike door spawn for player {} (map owner={})",
                            playerGuid.ToString(), _owner.ToString());

                        // The interior map is instanced per-HOUSE, not per-visitor.
                        // Guests can enter the owner's house — p->GetHousing() is the VISITOR's house,
                        // not the one being visited. Always resolve the OWNER's housing for door context.
                        Housing* ownerHousing = nullptr;
                        if (Player* ownerPlayer = ObjectAccessor::FindPlayer(_owner))
                            ownerHousing = ownerPlayer->GetHousing();

                        if (!ownerHousing)
                        {
                            // Owner may be offline when a guest enters — fall back to a minimal
                            // SummonGameObject so the guest isn't locked in. Blizzlike decor entity
                            // hierarchy requires owner housing context (entry hall GUID, houseGuid).
                            TC_LOG_WARN("housing", "InteriorDoor: owner housing unavailable (owner offline?) — "
                                "fallback to SummonGameObject for player {}", playerGuid.ToString());
                            float fbX = _originX - 2.52f;
                            float fbY = _originY;
                            float fbZ = _originZ + 0.02f;
                            if (GameObject* doorGo = p->SummonGameObject(575017,
                                Position(fbX, fbY, fbZ, 0.0f), QuaternionData(0, 0, 0, 1), 0s))
                            {
                                doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                TC_LOG_INFO("housing", "InteriorDoor: Fallback door guid={}",
                                    doorGo->GetGUID().ToString());
                            }
                            return;
                        }

                        uint32 doorGoEntry = 575017; // Alliance default
                        Neighborhood* nbh = sNeighborhoodMgr.GetNeighborhood(ownerHousing->GetNeighborhoodGuid());
                        int32 faction = nbh ? nbh->GetFactionRestriction() : NEIGHBORHOOD_FACTION_ALLIANCE;
                        if (faction == NEIGHBORHOOD_FACTION_HORDE)
                            doorGoEntry = 587318;

                        TC_LOG_INFO("housing", "InteriorDoor: faction={} doorGoEntry={}",
                            faction == NEIGHBORHOOD_FACTION_HORDE ? "Horde" : "Alliance", doorGoEntry);

                        // Sniff-verified position: (-2.521, 0.006, 0.020) relative to entry hall room entity
                        float doorLocalX = -2.52f;
                        float doorLocalY = 0.006f;
                        float doorLocalZ = 0.02f;
                        float doorWorldX = _originX + doorLocalX;
                        float doorWorldY = _originY + doorLocalY;
                        float doorWorldZ = _originZ + doorLocalZ;

                        TC_LOG_INFO("housing", "InteriorDoor: origin=({:.2f},{:.2f},{:.2f}) doorWorld=({:.2f},{:.2f},{:.2f})",
                            _originX, _originY, _originZ, doorWorldX, doorWorldY, doorWorldZ);

                        // Find the entry hall room entity GUID (slot 0) from the OWNER's housing
                        ObjectGuid entryHallGuid = ObjectGuid::Empty;
                        for (auto const* rm : ownerHousing->GetRooms())
                        {
                            TC_LOG_DEBUG("housing", "InteriorDoor: examining room slot={} entryId={} guid={}",
                                rm->SlotIndex, rm->RoomEntryId, rm->Guid.ToString());
                            if (rm->SlotIndex == 0)
                            {
                                entryHallGuid = rm->Guid;
                                break;
                            }
                        }

                        if (entryHallGuid.IsEmpty())
                        {
                            TC_LOG_ERROR("housing", "InteriorDoor: entry hall room (slot 0) NOT FOUND — "
                                "falling back to SummonGameObject");
                            // Fallback to the old simple approach so door still works
                            if (GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                                Position(doorWorldX, doorWorldY, doorWorldZ, 0.0f),
                                QuaternionData(0, 0, 0, 1), 0s))
                            {
                                doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                TC_LOG_INFO("housing", "InteriorDoor: Fallback SummonGameObject succeeded guid={}",
                                    doorGo->GetGUID().ToString());
                            }
                            return;
                        }

                        // The OWNER's house GUID — not the visiting player's
                        ObjectGuid interiorHouseGuid = ownerHousing->GetHouseGuid();
                        TC_LOG_INFO("housing", "InteriorDoor: entryHallGuid={} houseGuid={}",
                            entryHallGuid.ToString(), interiorHouseGuid.ToString());

                        // Create the decor entity (Object Type 18, Housing/56 subType=1)
                        ObjectGuid decorGuid = ObjectGuidFactory::CreateHousing(1, 0,
                            doorGoEntry, GetInstanceId() + 900000);

                        TC_LOG_INFO("housing", "InteriorDoor: generated decorGuid={}", decorGuid.ToString());

                        // GO via SummonGameObject has PrivateObjectOwner=player, so it is
                        // despawned when the player leaves the map. The decor entity persists
                        // on the (per-player) interior map across leave+reenter. Split the two:
                        //   - if decor entity exists, skip re-creating it (duplicate-GUID insert
                        //     would hit MapStoredObjectsUnorderedMap's assertion)
                        //   - always (re)summon the interactive GO so the door button is there
                        //     on re-entry too
                        Position doorWorldPos(doorWorldX, doorWorldY, doorWorldZ, 0.0f);
                        bool decorAlreadyPresent = GetObjectsStore().Find<HousingDecorEntity>(decorGuid) != nullptr;

                        if (!decorAlreadyPresent)
                        {
                            HousingDecorEntity* decorEntity = new HousingDecorEntity();

                            if (!decorEntity->Create(decorGuid, this, doorWorldPos))
                            {
                                TC_LOG_ERROR("housing", "InteriorDoor: decorEntity Create FAILED — falling back to SummonGameObject");
                                delete decorEntity;
                                if (GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                                    doorWorldPos, QuaternionData(0, 0, 0, 1), 0s))
                                {
                                    doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                    TC_LOG_INFO("housing", "InteriorDoor: Fallback SummonGameObject succeeded guid={}",
                                        doorGo->GetGUID().ToString());
                                }
                                return;
                            }

                            TC_LOG_INFO("housing", "InteriorDoor: decorEntity created OK guid={}", decorGuid.ToString());

                            decorEntity->SetDecorGUID(decorGuid);
                            decorEntity->SetAttachParentGUID(entryHallGuid);
                            decorEntity->SetFlags(0);
                            decorEntity->SetPersistedData(interiorHouseGuid);

                            ObjectGuid goGuid = ObjectGuid::Create<HighGuid::GameObject>(
                                GetId(), doorGoEntry, GetInstanceId() + 900000);
                            decorEntity->SetTargetGameObjectGUID(goGuid);

                            Position localPos(doorLocalX, doorLocalY, doorLocalZ);
                            decorEntity->SetMirroredPosition(localPos, QuaternionData(0, 0, 0, 1),
                                1.0f, entryHallGuid, 3);

                            PhasingHandler::InitDbPhaseShift(decorEntity->GetPhaseShift(),
                                PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

                            if (!AddToMap(decorEntity))
                            {
                                TC_LOG_ERROR("housing", "InteriorDoor: decorEntity AddToMap FAILED — falling back to SummonGameObject");
                                delete decorEntity;
                                if (GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                                    doorWorldPos, QuaternionData(0, 0, 0, 1), 0s))
                                {
                                    doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));
                                    TC_LOG_INFO("housing", "InteriorDoor: Fallback SummonGameObject succeeded guid={}",
                                        doorGo->GetGUID().ToString());
                                }
                                return;
                            }

                            TC_LOG_INFO("housing", "InteriorDoor: decorEntity AddToMap OK");
                        }
                        else
                        {
                            TC_LOG_INFO("housing", "InteriorDoor: decor entity already present on map — re-summoning GO only");
                        }

                        // Spawn the interactive GO via SummonGameObject — this path is proven
                        // to trigger visibility updates to the existing player. Manual
                        // CreateGameObject+AddToMap misses SetSpawnedByDefault(false) +
                        // SetRespawnTime(0) and resulted in the GO being invisible client-side.
                        // This always runs (even on re-entry with existing decor) because the
                        // GO is PrivateObjectOwner-tied to the player and despawns on leave.
                        GameObject* doorGo = p->SummonGameObject(doorGoEntry,
                            doorWorldPos, QuaternionData(0, 0, 0, 1), 0s);
                        if (!doorGo)
                        {
                            TC_LOG_ERROR("housing", "InteriorDoor: SummonGameObject FAILED for entry={}",
                                doorGoEntry);
                            return;
                        }

                        doorGo->ReplaceAllFlags(GameObjectFlags(0x40000));

                        TC_LOG_INFO("housing", "InteriorDoor: SUCCESS — decorReused={} decorEntity={} doorGO={} entry={} "
                            "at ({:.2f},{:.2f},{:.2f}) entryHall={} house={}",
                            decorAlreadyPresent, decorGuid.ToString(), doorGo->GetGUID().ToString(),
                            doorGoEntry, doorWorldX, doorWorldY, doorWorldZ,
                            entryHallGuid.ToString(), interiorHouseGuid.ToString());
                    }

                    TC_LOG_ERROR("housing", "HouseInteriorMap deferred: Complete — "
                        "HouseInfo+Status+Perms+Auras+Account+Initiative+PlotAT+ENTER_PLOT+Door for {}",
                        playerGuid.ToString());
                }, Milliseconds(500));
            }
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
