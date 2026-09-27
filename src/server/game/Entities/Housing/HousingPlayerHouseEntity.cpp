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

    // Retail serialises Housing/3 (HousingPlayerHouse) CREATE blocks with
    // objectType byte = 18 (TYPEID_HOUSING_ENTITY). Sniff-verified at
    // dump_12.0.1.66838_2026-04-15_09-35-59 packet idx 9984. Without this,
    // BaseEntity's default leaves m_objectTypeId = NUM_CLIENT_OBJECT_TYPES
    // (invalid sentinel), which the client's CREATE dispatcher may silently
    // drop from its entity registry — breaking the icon picker's
    // HouseGUID → entity → BnetAccount lookup for the own plot.
    m_objectTypeId = TYPEID_HOUSING_ENTITY;

    m_entityFragments.Add(WowCS::EntityFragment::FHousingPlayerHouse_C, false, WowCS::GetRawFragmentData(m_housingPlayerHouseData));
}

void HousingPlayerHouseEntity::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&HousingPlayerHouseEntity::m_housingPlayerHouseData);
    BaseEntity::ClearUpdateMask(remove);
}

void HousingPlayerHouseEntity::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    // Diagnostic: dump the field values at CREATE time so post-deploy sniff
    // verification can be cross-checked without decoding hex by hand. Only
    // logs when this specific entity (Housing/3) emits a CREATE.
    UF::HousingPlayerHouseData const& d = *m_housingPlayerHouseData;
    TC_LOG_INFO("housing",
        "HousingPlayerHouseEntity::BuildCreate guid={} -> target={} "
        "Bnet={} Plot={} Level={} Favor={} Budgets[I={} E={} R={} F={}] EntityGUID={}",
        GetGUID().ToString(), target ? target->GetGUID().ToString() : "<null>",
        d.BnetAccount->ToString(), int32(d.PlotIndex), uint32(d.Level), uint64(d.Favor),
        uint32(d.InteriorDecorPlacementBudget), uint32(d.ExteriorDecorPlacementBudget),
        uint32(d.RoomPlacementBudget), uint32(d.ExteriorFixtureBudget),
        d.EntityGUID->ToString());

    BaseEntity::BuildCreateUpdateBlockForPlayer(data, target);
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
    ClearUpdateMask(true);
}

void HousingPlayerHouseEntity::SendCreateToPlayer(Player* player)
{
    BuildUpdateChangesMask();
    UpdateData upd(player->GetMapId());
    WorldPacket packet;
    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.BuildPacket(&packet);
    player->SendDirectMessage(&packet);
    ClearUpdateMask(true);
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
