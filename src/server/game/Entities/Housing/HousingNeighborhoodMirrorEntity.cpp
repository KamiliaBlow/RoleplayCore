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

#include "HousingNeighborhoodMirrorEntity.h"
#include "Map.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"
#include "WorldSession.h"

HousingNeighborhoodMirrorEntity::HousingNeighborhoodMirrorEntity(WorldSession* session, ObjectGuid guid) : _session(session)
{
    _Create(guid);

    // Retail serialises Housing/4 (NeighborhoodMirror) CREATE blocks with
    // objectType byte = 18 (TYPEID_HOUSING_ENTITY). Sniff-verified at
    // dump_12.0.1.66838_2026-04-15_09-35-59 packet idx 9984. Without this,
    // BaseEntity's default leaves m_objectTypeId = NUM_CLIENT_OBJECT_TYPES
    // (invalid sentinel), which the client's CREATE dispatcher may silently
    // drop from its entity registry.
    m_objectTypeId = TYPEID_HOUSING_ENTITY;

    m_entityFragments.Add(WowCS::EntityFragment::FNeighborhoodMirrorData_C, false, WowCS::GetRawFragmentData(m_neighborhoodMirrorData));
}

void HousingNeighborhoodMirrorEntity::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData);
    BaseEntity::ClearUpdateMask(remove);
}

std::string HousingNeighborhoodMirrorEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingNeighborhoodMirror";
}

void HousingNeighborhoodMirrorEntity::BuildUpdate(UpdateDataMapType& data_map)
{
    BuildUpdateChangesMask();

    if (Player* owner = _session->GetPlayer())
        BuildFieldsUpdate(owner, data_map);

    ClearUpdateMask(false);
}

std::string HousingNeighborhoodMirrorEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingNeighborhoodMirrorEntity", BaseEntity::GetDebugInfo());
}

UF::UpdateFieldFlag HousingNeighborhoodMirrorEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

bool HousingNeighborhoodMirrorEntity::AddToObjectUpdate()
{
    if (Player* owner = _session->GetPlayer(); owner && owner->IsInWorld())
    {
        owner->GetMap()->AddUpdateObject(this);
        return true;
    }

    return false;
}

void HousingNeighborhoodMirrorEntity::RemoveFromObjectUpdate()
{
    if (Player* owner = _session->GetPlayer(); owner && owner->IsInWorld())
        owner->GetMap()->RemoveUpdateObject(this);
}

void HousingNeighborhoodMirrorEntity::SendUpdateToPlayer(Player* player)
{
    BuildUpdateChangesMask();
    BaseEntity::SendUpdateToPlayer(player);
    ClearUpdateMask(true);
}

void HousingNeighborhoodMirrorEntity::SendCreateToPlayer(Player* player)
{
    BuildUpdateChangesMask();
    UpdateData upd(player->GetMapId());
    WorldPacket packet;
    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.BuildPacket(&packet);
    player->SendDirectMessage(&packet);
    ClearUpdateMask(true);
}

void HousingNeighborhoodMirrorEntity::SetName(std::string const& name)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Name), name);
}

void HousingNeighborhoodMirrorEntity::SetOwnerGUID(ObjectGuid ownerGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::OwnerGUID), ownerGuid);
}

void HousingNeighborhoodMirrorEntity::AddHouse(ObjectGuid houseGuid, ObjectGuid ownerGuid)
{
    UF::PlayerHouseInfo& houseInfo = AddDynamicUpdateFieldValue(m_values.ModifyValue(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Houses));
    houseInfo.HouseGUID = houseGuid;
    houseInfo.OwnerGUID = ownerGuid;
}

void HousingNeighborhoodMirrorEntity::ClearHouses()
{
    ClearDynamicUpdateFieldValues(m_values.ModifyValue(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Houses));
}

void HousingNeighborhoodMirrorEntity::AddManager(ObjectGuid bnetAccountGuid, ObjectGuid playerGuid)
{
    UF::HousingOwner& manager = AddDynamicUpdateFieldValue(m_values.ModifyValue(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Managers));
    manager.BnetAccountGUID = bnetAccountGuid;
    manager.PlayerGUID = playerGuid;
}

void HousingNeighborhoodMirrorEntity::ClearManagers()
{
    ClearDynamicUpdateFieldValues(m_values.ModifyValue(&HousingNeighborhoodMirrorEntity::m_neighborhoodMirrorData).ModifyValue(&UF::NeighborhoodMirrorData::Managers));
}
