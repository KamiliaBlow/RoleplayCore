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

#include "HousingMirrorEntity.h"
#include "StringFormat.h"

HousingMirrorEntity::HousingMirrorEntity(Map* /*map*/, ObjectGuid guid)
{
    _Create(guid);

    m_objectTypeId = TYPEID_HOUSING_ENTITY;

    m_entityFragments.Add(WowCS::EntityFragment::FMirroredPositionData_C, false,
        WowCS::GetRawFragmentData(m_mirroredPositionData));
}

HousingMirrorEntity::~HousingMirrorEntity() = default;

void HousingMirrorEntity::InitPositionData(ObjectGuid attachParent,
    Position const& position, QuaternionData const& rotation,
    float scale, uint8 attachmentFlags, Tagging tagging)
{
    auto posData = m_values.ModifyValue(&HousingMirrorEntity::m_mirroredPositionData)
        .ModifyValue(&UF::MirroredPositionData::PositionData);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachParentGUID), attachParent);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::PositionLocalSpace),
        TaggedPosition<Position::XYZ>(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()));
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::RotationLocalSpace), rotation);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::ScaleLocalSpace), scale);
    SetUpdateFieldValue(posData.ModifyValue(&UF::MirroredMeshObjectData::AttachmentFlags), attachmentFlags);

    switch (tagging)
    {
        case Tagging::PieceAndRoot:
            m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, false);
            m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorRoot, false);
            break;
        case Tagging::Piece:
            m_entityFragments.Add(WowCS::EntityFragment::Tag_HouseExteriorPiece, false);
            break;
        case Tagging::None:
            break;
    }
}

void HousingMirrorEntity::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&HousingMirrorEntity::m_mirroredPositionData);
    BaseEntity::ClearUpdateMask(remove);
}

std::string HousingMirrorEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingMirror";
}

void HousingMirrorEntity::BuildUpdate(UpdateDataMapType& /*data_map*/)
{
    // mirror position is set once at spawn and never changes
    ClearUpdateMask(false);
}

std::string HousingMirrorEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingMirrorEntity", BaseEntity::GetDebugInfo());
}

UF::UpdateFieldFlag HousingMirrorEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

bool HousingMirrorEntity::AddToObjectUpdate()
{
    // fields are static once set; the mirror is never queued for updates
    return false;
}

void HousingMirrorEntity::RemoveFromObjectUpdate()
{
}
