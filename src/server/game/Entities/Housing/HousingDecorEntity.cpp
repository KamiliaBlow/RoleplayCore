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

#include "HousingDecorEntity.h"
#include "Log.h"
#include "Map.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"

HousingDecorEntity::HousingDecorEntity()
    : WorldObject(false)
{
    m_objectTypeId = TYPEID_HOUSING_ENTITY; // 18 — same as HousingRoomEntity

    m_updateFlag.HasEntityPosition = true;

    // Retail decor entities do NOT have CGObject — sniff-verified fragment list is
    // [FHousingDecor_C, FMirroredPositionData_C] only. Remove CGObject added by Object ctor.
    m_entityFragments.Remove(WowCS::EntityFragment::CGObject);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingDecor_C, false, WowCS::GetRawFragmentData(m_housingDecorData));
    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false, WowCS::GetRawFragmentData(m_mirroredPositionData));
}

bool HousingDecorEntity::Create(ObjectGuid guid, Map* map, Position const& pos)
{
    _Create(guid);
    SetMap(map);
    Relocate(pos);
    SetObjectScale(1.0f);

    return true;
}

void HousingDecorEntity::AddToWorld()
{
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().Insert<HousingDecorEntity>(this);
        WorldObject::AddToWorld();
    }
}

void HousingDecorEntity::RemoveFromWorld()
{
    if (IsInWorld())
    {
        WorldObject::RemoveFromWorld();
        GetMap()->GetObjectsStore().Remove<HousingDecorEntity>(this);
    }
}

void HousingDecorEntity::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
        return;

    uint8 updateType = UPDATETYPE_CREATE_OBJECT;
    uint8 objectType = m_objectTypeId;
    CreateObjectBits flags = m_updateFlag;

    ByteBuffer& buf = data->GetBuffer();
    buf << uint8(updateType);
    buf << GetGUID();
    buf << uint8(objectType);

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

    TC_LOG_DEBUG("housing", "HousingDecorEntity::BuildCreate guid={} objectType={} "
        "fieldBlockSize={} pos=({:.1f},{:.1f},{:.1f})",
        GetGUID().ToString(), uint32(m_objectTypeId),
        buf.wpos() - sizePos - 4,
        GetPositionX(), GetPositionY(), GetPositionZ());
}

void HousingDecorEntity::BuildValuesCreate(UF::UpdateFieldFlag /*flags*/, ByteBuffer& /*data*/, Player const* /*target*/) const
{
    // Not used — BuildCreateUpdateBlockForPlayer handles everything via entity fragments.
}

void HousingDecorEntity::BuildValuesUpdate(UF::UpdateFieldFlag /*flags*/, ByteBuffer& /*data*/, Player const* /*target*/) const
{
}

std::string HousingDecorEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingDecorEntity";
}

std::string HousingDecorEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingDecorEntity pos: ({:.1f}, {:.1f}, {:.1f})",
        Object::GetDebugInfo(),
        GetPositionX(), GetPositionY(), GetPositionZ());
}

UF::UpdateFieldFlag HousingDecorEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

void HousingDecorEntity::ClearValuesChangesMask()
{
    m_values.ClearChangesMask(&HousingDecorEntity::m_housingDecorData);
    m_values.ClearChangesMask(&HousingDecorEntity::m_mirroredPositionData);
    Object::ClearValuesChangesMask();
}

bool HousingDecorEntity::AddToObjectUpdate()
{
    GetMap()->AddUpdateObject(this);
    return true;
}

void HousingDecorEntity::RemoveFromObjectUpdate()
{
    GetMap()->RemoveUpdateObject(this);
}

void HousingDecorEntity::SetDecorGUID(ObjectGuid decorGuid)
{
    _decorGuid = decorGuid;
    SetUpdateFieldValue(m_values.ModifyValue(&HousingDecorEntity::m_housingDecorData)
        .ModifyValue(&UF::HousingDecorData::DecorGUID), decorGuid);
}

void HousingDecorEntity::SetAttachParentGUID(ObjectGuid roomEntityGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingDecorEntity::m_housingDecorData)
        .ModifyValue(&UF::HousingDecorData::AttachParentGUID), roomEntityGuid);
}

void HousingDecorEntity::SetTargetGameObjectGUID(ObjectGuid goGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingDecorEntity::m_housingDecorData)
        .ModifyValue(&UF::HousingDecorData::TargetGameObjectGUID), goGuid);
}

void HousingDecorEntity::SetFlags(uint8 flags)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingDecorEntity::m_housingDecorData)
        .ModifyValue(&UF::HousingDecorData::Flags), flags);
}

void HousingDecorEntity::SetPersistedData(ObjectGuid houseGuid, uint8 sourceType, std::string sourceValue)
{
    auto persistedRef = m_values.ModifyValue(&HousingDecorEntity::m_housingDecorData)
        .ModifyValue(&UF::HousingDecorData::PersistedData, 0);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::HouseGUID), houseGuid);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceType), sourceType);
    SetUpdateFieldValue(persistedRef.ModifyValue(&UF::DecorStoragePersistedData::SourceValue), std::move(sourceValue));
}

void HousingDecorEntity::SetMirroredPosition(Position const& pos, QuaternionData const& rot,
    float scale, ObjectGuid attachParent, uint8 attachFlags)
{
    auto posData = m_values.ModifyValue(&HousingDecorEntity::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rot);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachFlags);
}
