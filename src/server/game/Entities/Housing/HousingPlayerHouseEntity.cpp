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

#include "HousingPlayerHouseEntity.h"
#include "Map.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"
#include "WorldSession.h"

HousingPlayerHouseEntity::HousingPlayerHouseEntity(WorldSession* session, ObjectGuid guid) : _session(session)
{
    _Create(guid);

    m_entityFragments.Add(WowCS::EntityFragment::FHousingPlayerHouse_C, false, WowCS::GetRawFragmentData(m_housingPlayerHouseData));
}

void HousingPlayerHouseEntity::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&HousingPlayerHouseEntity::m_housingPlayerHouseData);
    BaseEntity::ClearUpdateMask(remove);
}

std::string HousingPlayerHouseEntity::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return "HousingPlayerHouse";
}

void HousingPlayerHouseEntity::BuildUpdate(UpdateDataMapType& data_map)
{
    BuildUpdateChangesMask();

    if (Player* owner = _session->GetPlayer())
        BuildFieldsUpdate(owner, data_map);

    ClearUpdateMask(false);
}

std::string HousingPlayerHouseEntity::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nType: HousingPlayerHouseEntity", BaseEntity::GetDebugInfo());
}

UF::UpdateFieldFlag HousingPlayerHouseEntity::GetUpdateFieldFlagsFor(Player const* /*target*/) const
{
    return UF::UpdateFieldFlag::None;
}

bool HousingPlayerHouseEntity::AddToObjectUpdate()
{
    if (Player* owner = _session->GetPlayer(); owner && owner->IsInWorld())
    {
        owner->GetMap()->AddUpdateObject(this);
        return true;
    }

    return false;
}

void HousingPlayerHouseEntity::RemoveFromObjectUpdate()
{
    if (Player* owner = _session->GetPlayer(); owner && owner->IsInWorld())
        owner->GetMap()->RemoveUpdateObject(this);
}

void HousingPlayerHouseEntity::SendUpdateToPlayer(Player* player)
{
    BuildUpdateChangesMask();
    BaseEntity::SendUpdateToPlayer(player);
    ClearUpdateMask(false);
}

void HousingPlayerHouseEntity::SetHouseType(uint32 houseType)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::HouseType), houseType);
}

void HousingPlayerHouseEntity::SetHouseSize(uint32 houseSize)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::HouseSize), houseSize);
}

void HousingPlayerHouseEntity::SetPlotIndex(int32 plotIndex)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::PlotIndex), plotIndex);
}

void HousingPlayerHouseEntity::SetLevel(uint32 level)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::Level), level);
}

void HousingPlayerHouseEntity::SetFavor(uint64 favor)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::Favor), favor);
}

void HousingPlayerHouseEntity::SetBudgets(uint32 interiorDecor, uint32 exteriorDecor, uint32 room, uint32 fixture)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::InteriorDecorPlacementBudget), interiorDecor);
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::ExteriorDecorPlacementBudget), exteriorDecor);
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::RoomPlacementBudget), room);
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::ExteriorFixtureBudget), fixture);
}

void HousingPlayerHouseEntity::SetBnetAccount(ObjectGuid bnetAccountGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::BnetAccount), bnetAccountGuid);
}

void HousingPlayerHouseEntity::SetEntityGUID(ObjectGuid entityGuid)
{
    SetUpdateFieldValue(m_values.ModifyValue(&HousingPlayerHouseEntity::m_housingPlayerHouseData).ModifyValue(&UF::HousingPlayerHouseData::EntityGUID), entityGuid);
}
