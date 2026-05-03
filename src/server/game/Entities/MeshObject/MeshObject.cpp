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

#include "MeshObject.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "PhasingHandler.h"
#include "UpdateData.h"

MeshObject::MeshObject() : WorldObject(false), MapObject()
{
    m_objectTypeId = TYPEID_MESH_OBJECT;

    // Retail MeshObjects: HasPositionFragment=true (from Object), Stationary=false,
    // HasMeshObject=true. The MeshObject block writes AttachParentGUID + local pos/rot.
    // Stationary MUST be false — it writes extra position data that shifts parsing.
    m_updateFlag.Stationary = false;
    m_updateFlag.MeshObject = true;

    m_entityFragments.Add(WowCS::EntityFragment::Tag_MeshObject, false);
}

MeshObject::~MeshObject() = default;

void MeshObject::AddToWorld()
{
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().Insert<MeshObject>(this);
        WorldObject::AddToWorld();
    }
}

void MeshObject::RemoveFromWorld()
{
    if (IsInWorld())
    {
        WorldObject::RemoveFromWorld();
        GetMap()->GetObjectsStore().Remove<MeshObject>(this);
    }
}

void MeshObject::Update(uint32 diff)
{
    WorldObject::Update(diff);
}

MeshObject* MeshObject::CreateMeshObject(Map* map, Position const& pos,
    QuaternionData const& rotation, float scale,
    int32 fileDataID, bool isWMO,
    ObjectGuid attachParent /*= ObjectGuid::Empty*/, uint8 attachFlags /*= 0*/,
    Position const* worldPos /*= nullptr*/)
{
    MeshObject* mesh = new MeshObject();
    if (!mesh->Create(map, pos, rotation, scale, fileDataID, isWMO, attachParent, attachFlags, worldPos))
    {
        delete mesh;
        return nullptr;
    }

    return mesh;
}

bool MeshObject::Create(Map* map, Position const& pos, QuaternionData const& rotation,
    float scale, int32 fileDataID, bool isWMO,
    ObjectGuid attachParent, uint8 attachFlags,
    Position const* worldPos)
{
    SetMap(map);

    // For child pieces (attached to a parent), pos contains LOCAL-SPACE coordinates.
    // Use worldPos (the parent's position) for server-side grid placement so the MeshObject
    // is in the correct grid cell and visible to players near the house.
    // The local-space offset is stored in FMirroredPositionData_C for client rendering.
    if (worldPos)
        Relocate(*worldPos);
    else
        Relocate(pos);

    if (!IsPositionValid())
    {
        TC_LOG_ERROR("entities.meshobject", "MeshObject not created. Invalid coordinates (X: {} Y: {})",
            GetPositionX(), GetPositionY());
        return false;
    }

    // Phase shift: always visible regardless of player's phase state.
    // Must use PHASE_USE_FLAGS_ALWAYS_VISIBLE (matching door GOs, cornerstones, ATs, decor GOs)
    // otherwise cosmetic phase additions on plot exit hide meshes including neighbor houses.
    PhasingHandler::InitDbPhaseShift(GetPhaseShift(), PHASE_USE_FLAGS_ALWAYS_VISIBLE, 0, 0);

    _Create(ObjectGuid::Create<HighGuid::MeshObject>(GetMapId(), 0,
        GetMap()->GenerateLowGuid<HighGuid::MeshObject>()));

    SetObjectScale(1.0f);

    // Retail sniff: MeshObject decor entities have ObjectData.EntryID set to FileDataID.
    // e.g., EntryID=7011541 matching FileDataID=7011541 on the same entity.
    // The client may use EntryID for internal entity lookups in the placed decor hash map.
    SetEntry(fileDataID);

    // Set mesh object update fields
    auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::FileDataID), fileDataID);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsWMO), isWMO);
    SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), false);

    // Register FMeshObjectData_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FMeshObjectData_C, false,
        WowCS::GetRawFragmentData(m_meshObjectData));

    // Store movement block data (used by BaseEntity::BuildCreateUpdateBlockMovement)
    _attachParentGUID = attachParent;
    _positionLocalSpace = pos;

    TC_LOG_ERROR("housing", "MeshObject::Create: guid={} fileDataID={} COB: HasEntityPos={} Stationary={} MeshObj={} attachParent={}",
        GetGUID().ToString(), fileDataID,
        bool(m_updateFlag.HasEntityPosition), bool(m_updateFlag.Stationary), bool(m_updateFlag.MeshObject),
        attachParent.ToString());
    _rotationLocalSpace = rotation;
    _scaleLocalSpace = scale;
    _attachmentFlags = attachFlags;

    // Set mirrored position data (FMirroredPositionData_C fragment)
    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rotation);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachFlags);

    // Register FMirroredPositionData_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false,
        WowCS::GetRawFragmentData(m_mirroredPositionData));

    SetZoneScript();
    UpdatePositionData();

    // NOTE: AddToMap is NOT called here. The caller must call map->AddToMap(mesh) after
    // setting up all entity fragments (e.g. InitHousingFixtureData). The create packet
    // is sent during AddToMap, so all fragments must be registered before that point.

    TC_LOG_DEBUG("housing", "MeshObject::Create: guid={} fileDataID={} isWMO={} at ({:.1f}, {:.1f}, {:.1f}) on map {} (not yet added to map)",
        GetGUID().ToString(), fileDataID, isWMO,
        pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), GetMapId());

    return true;
}

void MeshObject::InitHousingDecorData(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 flags,
    ObjectGuid roomEntityGuid /*= ObjectGuid::Empty*/, uint8 sourceType /*= 0*/, std::string sourceValue /*= {}*/)
{
    if (m_housingDecorData.has_value())
        return;

    // Sniff-verified: FHousingDecor_C entity fragment on MeshObject decor entities.
    // TargetGameObjectGUID is EMPTY (0x0) in ALL retail sniffs.
    // AttachParentGUID points to the room entity (Housing/18 base room) the decor is placed in.
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::DecorGUID), decorGuid);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::AttachParentGUID), roomEntityGuid);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::Flags), flags);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::TargetGameObjectGUID), ObjectGuid::Empty);

    auto persistedRef = m_values.ModifyValue(&Object::m_housingDecorData, 0)
        .ModifyValue(&UF::HousingDecorData::PersistedData, 0);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::HouseGUID), houseGuid);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceType), sourceType);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceValue), std::move(sourceValue));

    m_entityFragments.Add(WowCS::EntityFragment::FHousingDecor_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingDecorData));

    // Retail sniff-verified: decor MeshObjects have exactly these fragments:
    //   [CGObject(2), FMeshObjectData_C(19), FHousingDecor_C(20),
    //    FMirroredPositionData_C(31), Tag_MeshObject(221)]
    // FHousingDecorActor_C (28) is NOT present on any retail entity.

    // Retail sniff: HasDecor movement block flag is NEVER set (always False).
    // The room entity GUID is already in the FHousingDecor_C fragment's AttachParentGUID field.
    // Do NOT set m_updateFlag.Decor here — it adds an extra movement block field
    // that the client does not expect.
    _decorRoomEntityGUID = roomEntityGuid;

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingDecorData: guid={} decorGuid={} houseGuid={} flags={} roomEntity={} (FHousingDecor_C ON)",
        GetGUID().ToString(), decorGuid.ToString(), houseGuid.ToString(), flags, roomEntityGuid.ToString());
}

void MeshObject::InitHousingFixtureData(ObjectGuid houseGuid, ObjectGuid fixtureGuid,
    ObjectGuid parentFixtureGuid, int32 exteriorComponentID,
    int32 houseExteriorWmoDataID, uint8 exteriorComponentType /*= 9*/,
    uint8 houseSize /*= 2*/, int32 exteriorComponentHookID /*= -1*/, bool isRoot /*= false*/)
{
    if (m_housingFixtureData.has_value())
        return;

    // FHousingFixture_C fragment (ID 34, 96 bytes, 11 fields with HasChangesMask<11>).
    // Field order must match the client's CREATE deserializer:
    //   [0] ExteriorComponentID  (CompressedUInt32)
    //   [1] HouseExteriorWmoDataID (CompressedUInt32)
    //   [2] ExteriorComponentHookID (CompressedUInt32, defaults -1)
    //   [3] HouseGUID (PackedGUID128)
    //   [4] AttachParentGUID (PackedGUID128) — parent fixture in hierarchy
    //   [5] Guid (PackedGUID128) — unique per fixture, for client identification
    //   [6] GameObjectGUID (PackedGUID128) — always empty
    //   [7] ExteriorComponentType (uint8)
    //   [8] Field_59 (uint8)
    //   [9] Size (uint8)
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentID), exteriorComponentID);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::HouseExteriorWmoDataID), houseExteriorWmoDataID);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentHookID), exteriorComponentHookID);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::HouseGUID), houseGuid);
    // AttachParentGUID: the parent fixture's unique GUID in the hierarchy.
    // Root pieces have empty parent. Child pieces point to their parent root's fixture GUID.
    // The client uses this to build the fixture tree and resolve hook point ownership.
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::AttachParentGUID), parentFixtureGuid);
    // Guid: unique per fixture — the client uses this to identify individual fixtures.
    // Must be a Housing-type GUID (client crashes with non-Housing GUIDs here).
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::Guid), fixtureGuid);
    // GameObjectGUID: retail sniff shows door components (Type=11) have the GO entry GUID here.
    // Other fixture types (base, roof, window, etc.) have empty GUID.
    // Look up the ExteriorComponent DB2 entry for GameObjectID.
    {
        ObjectGuid goGuid = ObjectGuid::Empty;
        if (exteriorComponentID > 0)
        {
            ExteriorComponentEntry const* extComp = sExteriorComponentStore.LookupEntry(
                static_cast<uint32>(exteriorComponentID));
            if (extComp && extComp->GameObjectID > 0)
            {
                // Build a GameObject-type GUID referencing the GO entry.
                // Retail sniff: the GUID uses the same Low value as the MeshObject's Low.
                goGuid = ObjectGuid::Create<HighGuid::GameObject>(GetMap()->GetId(),
                    static_cast<uint32>(extComp->GameObjectID), GetGUID().GetCounter());
            }
        }
        if (!goGuid.IsEmpty())
        {
            SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
                .ModifyValue(&UF::HousingFixtureData::GameObjectGUID), goGuid);
        }
    }
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentType), exteriorComponentType);
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::Field_59), uint8(1)); // sniff: always 1
    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::Size), houseSize);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingFixture_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingFixtureData));

    // Cache for targeted fixture lookup and hierarchy traversal
    _exteriorComponentHookID = exteriorComponentHookID;
    _exteriorComponentID = exteriorComponentID;
    _fixtureGuid = fixtureGuid;

    // Root pieces get Tag_HouseExteriorRoot (225), child pieces get Tag_HouseExteriorPiece (224).
    // The client uses Tag_HouseExteriorRoot to identify the fixture GUID for edit mode enter/exit.
    _isExteriorRoot = isRoot;
    if (isRoot)
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorRoot, IsInWorld());
    else
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, IsInWorld());

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingFixtureData: meshGuid={} fixtureGuid={} "
        "parentFixtureGuid={} houseGuid={} extCompID={} wmoDataID={} hookID={} type={} size={} isRoot={}",
        GetGUID().ToString(), fixtureGuid.ToString(), parentFixtureGuid.ToString(),
        houseGuid.ToString(), exteriorComponentID, houseExteriorWmoDataID,
        exteriorComponentHookID, exteriorComponentType, houseSize, isRoot);
}

void MeshObject::UpdateLocalScale(float scale)
{
    _scaleLocalSpace = scale;
    auto posData = m_values.ModifyValue(&MeshObject::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
}

void MeshObject::UpdateExteriorComponentID(int32 id)
{
    if (!m_housingFixtureData.has_value())
        return;

    SetUpdateFieldValue(m_values.ModifyValue(&Object::m_housingFixtureData, 0)
        .ModifyValue(&UF::HousingFixtureData::ExteriorComponentID), id);
    _exteriorComponentID = id;
}

void MeshObject::InitHousingRoomData(ObjectGuid houseGuid, int32 houseRoomID,
    int32 flags, int32 floorIndex)
{
    if (m_housingRoomData.has_value())
        return;

    // Set IsRoom=true — this MeshObject has FHousingRoom_C, so the client's
    // MeshObjectSystem can safely read FHousingRoom_C.Flags from it.
    {
        auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
        SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), true);
    }

    // Populate HousingRoomData (FHousingRoom_C fragment data).
    auto roomData = m_values.ModifyValue(&Object::m_housingRoomData, 0);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::HouseGUID), houseGuid);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::HouseRoomID), houseRoomID);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::Flags), flags);
    SetUpdateFieldValue(roomData.ModifyValue(&UF::HousingRoomData::FloorIndex), floorIndex);

    // Register FHousingRoom_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoom_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingRoomData));

    // Register Tag_HousingRoom tag fragment
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HousingRoom, IsInWorld());

    // Retail sniff: HasRoom movement block flag is NEVER set (always False).
    // The house GUID is already in the FHousingRoom_C fragment data.
    // Do NOT set m_updateFlag.Room here — it adds an extra movement block field
    // that the client does not expect.
    _roomHouseGUID = houseGuid;

    TC_LOG_DEBUG("housing", "MeshObject::InitHousingRoomData: guid={} houseGuid={} "
        "roomID={} flags={} floor={} (Room flag=ON)",
        GetGUID().ToString(), houseGuid.ToString(), houseRoomID, flags, floorIndex);
}

void MeshObject::AddRoomMeshObject(ObjectGuid meshObjectGuid)
{
    if (!m_housingRoomData.has_value())
        return;

    AddDynamicUpdateFieldValue(m_values.ModifyValue(&Object::m_housingRoomData, 0)
        .ModifyValue(&UF::HousingRoomData::MeshObjects)) = meshObjectGuid;

    TC_LOG_DEBUG("housing", "MeshObject::AddRoomMeshObject: room={} added meshObject={}",
        GetGUID().ToString(), meshObjectGuid.ToString());
}

void MeshObject::AddRoomDoor(int32 roomComponentID, Position const& offset, uint8 roomComponentType, ObjectGuid attachedRoomGuid)
{
    if (!m_housingRoomData.has_value())
        return;

    // For a fresh dynamic array entry, populate fields via the mutable reference's ModifyValue
    // which returns a setter that marks the change mask and writes the underlying value.
    auto&& doorRef = AddDynamicUpdateFieldValue(m_values.ModifyValue(&Object::m_housingRoomData, 0)
        .ModifyValue(&UF::HousingRoomData::Doors));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentID).SetValue(roomComponentID);
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentOffset).SetValue(
        TaggedPosition<Position::XYZ>(offset.GetPositionX(), offset.GetPositionY(), offset.GetPositionZ()));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentType).SetValue(roomComponentType);
    doorRef.ModifyValue(&UF::HousingDoorData::AttachedRoomGUID).SetValue(attachedRoomGuid);

    TC_LOG_DEBUG("housing", "MeshObject::AddRoomDoor: room={} componentID={} type={} offset=({:.1f},{:.1f},{:.1f}) attachedRoom={}",
        GetGUID().ToString(), roomComponentID, roomComponentType,
        offset.GetPositionX(), offset.GetPositionY(), offset.GetPositionZ(),
        attachedRoomGuid.ToString());
}

void MeshObject::InitHousingRoomComponentData(ObjectGuid roomGuid,
    int32 roomComponentOptionID, int32 roomComponentID,
    uint8 roomComponentType, int32 field24, uint8 field20,
    int32 houseThemeID, int32 roomComponentTextureID,
    int32 roomComponentTypeParam,
    float geoboxMinX, float geoboxMinY, float geoboxMinZ,
    float geoboxMaxX, float geoboxMaxY, float geoboxMaxZ)
{
    if (m_housingRoomComponentMeshData.has_value())
        return;

    // Sniff-verified: ALL retail room component MeshObjects have IsRoom=true.
    {
        auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
        SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::IsRoom), true);
    }

    // Set Geobox (axis-aligned bounding box) on MeshObjectData.
    {
        auto meshData = m_values.ModifyValue(&MeshObject::m_meshObjectData);
        UF::AaBox geobox;
        geobox.Low = TaggedPosition<Position::XYZ>(geoboxMinX, geoboxMinY, geoboxMinZ);
        geobox.High = TaggedPosition<Position::XYZ>(geoboxMaxX, geoboxMaxY, geoboxMaxZ);
        SetUpdateFieldValue(meshData.ModifyValue(&UF::MeshObjectData::Geobox, uint32(0)), std::move(geobox));
    }

    // Populate HousingRoomComponentMeshData (FHousingRoomComponentMesh_C fragment data)
    auto compData = m_values.ModifyValue(&Object::m_housingRoomComponentMeshData, 0);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomGUID), roomGuid);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentOptionID), roomComponentOptionID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentID), roomComponentID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::Field_20), field20);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentType), roomComponentType);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::Field_24), field24);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::HouseThemeID), houseThemeID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTextureID), roomComponentTextureID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTypeParam), roomComponentTypeParam);

    // Register FHousingRoomComponentMesh_C entity fragment
    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoomComponentMesh_C, IsInWorld(),
        WowCS::GetRawFragmentData(m_housingRoomComponentMeshData));

    TC_LOG_ERROR("housing", "MeshObject::InitHousingRoomComponentData: guid={} roomGuid={} "
        "compOptionID={} compID={} compType={} themeID={} "
        "geobox=({:.2f},{:.2f},{:.2f})→({:.2f},{:.2f},{:.2f})",
        GetGUID().ToString(), roomGuid.ToString(),
        roomComponentOptionID, roomComponentID, roomComponentType, houseThemeID,
        geoboxMinX, geoboxMinY, geoboxMinZ, geoboxMaxX, geoboxMaxY, geoboxMaxZ);
}

void MeshObject::UpdateRoomComponentVisuals(int32 roomComponentOptionID, int32 houseThemeID,
    int32 roomComponentTextureID, int32 roomComponentTypeParam /*= -1*/)
{
    if (!m_housingRoomComponentMeshData.has_value())
        return;

    auto compData = m_values.ModifyValue(&Object::m_housingRoomComponentMeshData, 0);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentOptionID), roomComponentOptionID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::HouseThemeID), houseThemeID);
    SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTextureID), roomComponentTextureID);
    if (roomComponentTypeParam >= 0)
        SetUpdateFieldValue(compData.ModifyValue(&UF::HousingRoomComponentMeshData::RoomComponentTypeParam), roomComponentTypeParam);

    TC_LOG_DEBUG("housing", "MeshObject::UpdateRoomComponentVisuals: guid={} optionID={} themeID={} textureID={} typeParam={}",
        GetGUID().ToString(), roomComponentOptionID, houseThemeID, roomComponentTextureID, roomComponentTypeParam);
}

int32 MeshObject::GetRoomComponentID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->RoomComponentID;
}

int32 MeshObject::GetRoomComponentOptionID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->RoomComponentOptionID;
}

int32 MeshObject::GetHouseThemeID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->HouseThemeID;
}

int32 MeshObject::GetRoomComponentTextureID() const
{
    if (!m_housingRoomComponentMeshData.has_value())
        return 0;
    return m_housingRoomComponentMeshData->RoomComponentTextureID;
}

void MeshObject::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
        return;

    // Write CREATE block manually with UpdateType=1 (UPDATETYPE_CREATE_OBJECT).
    // Retail MeshObjects always use CreateObject1. BaseEntity uses m_isNewObject
    // which produces UpdateType=2 for newly spawned entities, but the client may
    // not handle type 2 for entity-fragment types (objectType 14/18).
    uint8 updateType = UPDATETYPE_CREATE_OBJECT; // Always 1, like HousingRoomEntity
    CreateObjectBits flags = m_updateFlag;

    ByteBuffer& buf = data->GetBuffer();
    std::size_t startPos = buf.wpos();
    buf << uint8(updateType);
    buf << GetGUID();
    buf << uint8(m_objectTypeId);

    BuildMovementUpdate(buf, flags, target);

    UF::UpdateFieldFlag fieldFlags = GetUpdateFieldFlagsFor(target);
    std::size_t sizePos = buf.wpos();
    buf << uint32(0);
    buf << uint8(fieldFlags);
    BuildEntityFragments(buf, m_entityFragments.GetIds());

    for (std::size_t i = 0; i < m_entityFragments.UpdateableCount; ++i)
    {
        WowCS::EntityFragment fragmentId = m_entityFragments.Updateable.Ids[i];
        if (WowCS::IsIndirectFragment(fragmentId))
            buf << uint8(1);

        WowCS::EntityFragmentInfo->SerializeCreate[static_cast<std::size_t>(m_entityFragments.Updateable.Ids[i])](
            m_entityFragments.Updateable.Data[i], fieldFlags, buf, target, this);
    }

    buf.put<uint32>(sizePos, buf.wpos() - sizePos - 4);
    data->AddUpdateBlock();
    std::size_t endPos = buf.wpos();

    // Hex dump the FULL CREATE block for the first few interior MeshObjects
    if (m_housingRoomComponentMeshData.has_value() && (endPos - startPos) > 0)
    {
        ByteBuffer const& buf = data->GetBuffer();
        std::string hex;
        std::size_t dumpLen = std::min(endPos - startPos, std::size_t(200));
        for (std::size_t i = startPos; i < startPos + dumpLen; ++i)
            hex += Trinity::StringFormat("{:02x} ", buf[i]);
        TC_LOG_ERROR("housing", "MeshObject::BuildCreate guid={} compID={} FULL_HEX[{} bytes]: {}",
            GetGUID().ToString(), int32(m_housingRoomComponentMeshData->RoomComponentID),
            endPos - startPos, hex);
    }
}

void MeshObject::BuildValuesCreate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // Only ObjectData belongs to the CGObject fragment for MeshObjects.
    // m_meshObjectData is serialized by FMeshObjectData_C fragment's own SerializeCreate handler.
    // m_mirroredPositionData is serialized by FMirroredPositionData_C fragment.
    // m_housingFixtureData is serialized by FHousingFixture_C fragment.
    m_objectData->WriteCreate(flags, data, target, this);
}

void MeshObject::BuildValuesUpdate(UF::UpdateFieldFlag flags, ByteBuffer& data, Player const* target) const
{
    // GetChangedObjectTypeMask() only contains CGObject-owned fields (TYPEID_OBJECT).
    // Entity fragment data is handled by BuildValuesUpdateBlockForPlayer using SerializeUpdate.
    data << uint32(m_values.GetChangedObjectTypeMask());

    if (m_values.HasChanged(TYPEID_OBJECT))
        m_objectData->WriteUpdate(flags, data, target, this);
}

void MeshObject::ClearValuesChangesMask()
{
    m_values.ClearChangesMask(&MeshObject::m_meshObjectData);
    m_values.ClearChangesMask(&MeshObject::m_mirroredPositionData);
    m_values.ClearChangesMask(&Object::m_housingRoomComponentMeshData);
    m_values.ClearChangesMask(&Object::m_housingDecorData);
    m_values.ClearChangesMask(&Object::m_housingFixtureData);
    WorldObject::ClearValuesChangesMask();
}
