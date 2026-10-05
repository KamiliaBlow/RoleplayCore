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

#include "HousingRoomEntity.h"
#include "Map.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"

HousingRoomEntity::HousingRoomEntity(bool exteriorRoot /*= false*/)
    : WorldObject(false)
{
    m_objectTypeId = TYPEID_HOUSING_ENTITY;

    m_updateFlag.HasEntityPosition = true;
    m_updateFlag.Stationary = !exteriorRoot;

    // remove the CGObject fragment added by the Object constructor
    m_entityFragments.Remove(WowCS::EntityFragment::CGObject);

    if (exteriorRoot)
    {
        // base and roof meshes hang off this root at local 0
        m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false, WowCS::GetRawFragmentData(m_mirroredPositionData));
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, false);
        m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorRoot, false);
        return;
    }

    m_entityFragments.Add(WowCS::EntityFragment::FHousingRoom_C, false, WowCS::GetRawFragmentData(m_housingRoomData));
    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false, WowCS::GetRawFragmentData(m_mirroredPositionData));
    m_entityFragments.Add(WowCS::EntityFragment::Tag_HousingRoom, false);
}

bool HousingRoomEntity::Create(ObjectGuid guid, Map* map, Position const& pos)
{
    _Create(guid);
    SetMap(map);
    Relocate(pos);
    SetObjectScale(1.0f);

    if (!GetMap()->AddToMap(this))
        return false;

    // client plot-bounds checks walk the room registry; keep this streamed to every player
    setActive(true);
    SetFarVisible(true);

    return true;
}

void HousingRoomEntity::AddToWorld()
{
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().Insert<HousingRoomEntity>(this);
        WorldObject::AddToWorld();
    }
}

void HousingRoomEntity::RemoveFromWorld()
{
    if (IsInWorld())
    {
        WorldObject::RemoveFromWorld();
        GetMap()->GetObjectsStore().Remove<HousingRoomEntity>(this);
    }
}

void HousingRoomEntity::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
        return;

    CreateObjectBits flags = m_updateFlag;

    ByteBuffer& buf = data->GetBuffer();
    buf << uint8(UPDATETYPE_CREATE_OBJECT);
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

        WowCS::EntityFragmentInfo->SerializeCreate[static_cast<std::size_t>(fragmentId)](
            m_entityFragments.Updateable.Data[i], fieldFlags, buf, target, this);
    }

    buf.put<uint32>(sizePos, buf.wpos() - sizePos - 4);
    data->AddUpdateBlock();
}

void HousingRoomEntity::BuildValuesCreate(UF::UpdateFieldFlag /*flags*/, ByteBuffer& /*data*/, Player const* /*target*/) const
{
    // values are serialized via entity fragments
}

void HousingRoomEntity::BuildValuesUpdate(UF::UpdateFieldFlag /*flags*/, ByteBuffer& /*data*/, Player const* /*target*/) const
{
    // handled by the entity fragment change mask system
}

std::string HousingRoomEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingRoom";
}

std::string HousingRoomEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingRoomEntity pos: ({:.1f}, {:.1f}, {:.1f})",
        Object::GetDebugInfo(),
        GetPositionX(), GetPositionY(), GetPositionZ());
}

UF::UpdateFieldFlag HousingRoomEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

void HousingRoomEntity::ClearValuesChangesMask()
{
    m_values.ClearChangesMask(&HousingRoomEntity::m_housingRoomData);
    m_values.ClearChangesMask(&HousingRoomEntity::m_mirroredPositionData);
    Object::ClearValuesChangesMask();
}

bool HousingRoomEntity::AddToObjectUpdate()
{
    GetMap()->AddUpdateObject(this);
    return true;
}

void HousingRoomEntity::RemoveFromObjectUpdate()
{
    GetMap()->RemoveUpdateObject(this);
}

void HousingRoomEntity::SetHouseGUID(ObjectGuid houseGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::HouseGUID), houseGuid);
}

void HousingRoomEntity::SetHouseRoomID(int32 roomId)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::HouseRoomID), roomId);
}

void HousingRoomEntity::SetFlags(int32 flags)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData).ModifyValue(&UF::HousingRoomData::Flags), flags);
}

void HousingRoomEntity::SetFloorIndex(int32 floorIndex)
{
    // server-side only; not a client wire field anymore
    _floorIndex = floorIndex;
}

void HousingRoomEntity::AddMeshObject(ObjectGuid meshObjectGuid)
{
    AddDynamicUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
        .ModifyValue(&UF::HousingRoomData::MeshObjects)) = meshObjectGuid;
}

void HousingRoomEntity::ReplaceMeshObjects(std::vector<ObjectGuid> const& newGuids)
{
    // refresh the mesh list after theme respawn (stale GUIDs crash the client)
    ClearDynamicUpdateFieldValues(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
        .ModifyValue(&UF::HousingRoomData::MeshObjects));

    for (ObjectGuid const& guid : newGuids)
        AddDynamicUpdateFieldValue(m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
            .ModifyValue(&UF::HousingRoomData::MeshObjects)) = guid;
}

void HousingRoomEntity::AddDoor(int32 roomComponentID, Position const& offset, uint8 connectionType, ObjectGuid attachedRoomGuid)
{
    auto doorRef = AddDynamicUpdateFieldValue(
        m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
            .ModifyValue(&UF::HousingRoomData::Doors));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentID).SetValue(roomComponentID);
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentOffset).SetValue(
        TaggedPosition<Position::XYZ>(offset.GetPositionX(), offset.GetPositionY(), offset.GetPositionZ()));
    doorRef.ModifyValue(&UF::HousingDoorData::RoomComponentType).SetValue(connectionType);
    doorRef.ModifyValue(&UF::HousingDoorData::AttachedRoomGUID).SetValue(attachedRoomGuid);
}

bool HousingRoomEntity::UpdateDoorConnection(int32 roomComponentID, ObjectGuid attachedRoomGuid)
{
    UF::HousingRoomData const& roomData = *m_housingRoomData;
    for (uint32 i = 0; i < roomData.Doors.size(); ++i)
    {
        if (roomData.Doors[i].RoomComponentID == roomComponentID)
        {
            SetUpdateFieldValue(
                m_values.ModifyValue(&HousingRoomEntity::m_housingRoomData)
                    .ModifyValue(&UF::HousingRoomData::Doors, i)
                    .ModifyValue(&UF::HousingDoorData::AttachedRoomGUID),
                attachedRoomGuid);
            return true;
        }
    }
    return false;
}

void HousingRoomEntity::SetMirroredPosition(Position const& pos, QuaternionData const& rot,
    float scale, ObjectGuid attachParent, uint8 attachFlags)
{
    auto posData = m_values.ModifyValue(&HousingRoomEntity::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rot);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachFlags);
}
