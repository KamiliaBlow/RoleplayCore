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

#include "Account.h"
#include "Map.h"
#include "HousingDefines.h"
#include "Player.h"
#include "StringFormat.h"
#include "UpdateData.h"
#include "WorldSession.h"
#include <algorithm>

namespace Battlenet
{
Account::Account(WorldSession* session, ObjectGuid guid, std::string&& name) : m_session(session), m_name(std::move(name))
{
    _Create(guid);

    // Only FHousingStorage_C belongs on the BNetAccount entity.
    // FHousingPlayerHouse_C ? Housing/3 entity (HousingPlayerHouseEntity)
    // FNeighborhoodMirrorData_C ? Housing/4 entity (HousingNeighborhoodMirrorEntity)
    m_entityFragments.Add(WowCS::EntityFragment::FHousingStorage_C, false, WowCS::GetRawFragmentData(m_housingStorageData));

    // Default value
    SetUpdateFieldValue(m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::DecorMaxOwnedCount), 5000);
}

void Account::ClearUpdateMask(bool remove)
{
    m_values.ClearChangesMask(&Account::m_housingStorageData);
    BaseEntity::ClearUpdateMask(remove);
}


std::string Account::GetNameForLocaleIdx(LocaleConstant /*locale*/) const
{
    return m_name;
}

void Account::BuildUpdate(UpdateDataMapType& data_map)
{
    BuildUpdateChangesMask();

    if (Player* owner = m_session->GetPlayer())
        BuildFieldsUpdate(owner, data_map);

    ClearUpdateMask(false);
}

std::string Account::GetDebugInfo() const
{
    return Trinity::StringFormat("{}\nName: {}", BaseEntity::GetDebugInfo(), m_name);
}

UF::UpdateFieldFlag Account::GetUpdateFieldFlagsFor(Player const* target) const
{
    if (*target->m_playerData->BnetAccount == GetGUID())
        return UF::UpdateFieldFlag::Owner;

    return UF::UpdateFieldFlag::None;
}

bool Account::AddToObjectUpdate()
{
    if (Player* owner = m_session->GetPlayer(); owner && owner->IsInWorld())
    {
        owner->GetMap()->AddUpdateObject(this);
        return true;
    }

    return false;
}

void Account::RemoveFromObjectUpdate()
{
    if (Player* owner = m_session->GetPlayer(); owner && owner->IsInWorld())
        owner->GetMap()->RemoveUpdateObject(this);
}

void Account::SendUpdateToPlayer(Player* player)
{
    // BaseEntity::SendUpdateToPlayer is const and skips BuildUpdateChangesMask(),
    // so ContentsChangedMask is 0 and the VALUES_UPDATE contains no fragment data.
    // We must compute the mask before serializing so that pending fragment changes
    // (e.g., FHousingStorage_C populated by PopulateCatalogStorageEntries) are included.
    BuildUpdateChangesMask();
    BaseEntity::SendUpdateToPlayer(player);
    ClearUpdateMask(true);
}

void Account::SetHousingDecorStorageEntry(ObjectGuid decorGuid, ObjectGuid houseGuid, uint8 sourceType, std::string sourceValue, std::optional<uint8> placementStatus)
{
    auto setter = m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::Decor);
    // Remove before re-inserting so identical values still mark the entry changed: the client
    // ingests storage entries only through updates after its housing state initializes (the
    // login CREATE is processed too early), so the editor-entry storage refresh must reach it
    // even when nothing actually changed.
    RemoveMapUpdateFieldValue(setter, decorGuid);

    // Retail 12.1.0.69933: 1 = placed inside the house, 2 = placed on the plot. The client sums
    // the interior placement budget from status 1 records and the exterior budget from status 2.
    uint8 const status = placementStatus.value_or(houseGuid.IsEmpty()
        ? uint8(HOUSING_DECOR_IN_STORAGE)
        : uint8(HOUSING_DECOR_PLACED_HOUSE));

    auto ref = m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::Decor, decorGuid);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::HouseGUID), houseGuid);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::PlacementStatus), status);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::SourceType), sourceType);
    SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::SourceValue), std::move(sourceValue));
}

void Account::SetHousingDecorDyeSlots(ObjectGuid decorGuid, std::array<uint32, 3> const& dyeSlots)
{
    auto ref = m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::Decor, decorGuid);
    if (std::ranges::any_of(dyeSlots, [](uint32 dye) { return dye != 0; }))
        SetUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::DyeSlots, 0)
            .ModifyValue(&UF::DecorDyeSlots::DyeColorID), { int32(dyeSlots[0]), int32(dyeSlots[1]), int32(dyeSlots[2]) });
    else
        RemoveOptionalUpdateFieldValue(ref.ModifyValue(&UF::DecorStoragePersistedData::DyeSlots));
}

void Account::RemoveHousingDecorStorageEntry(ObjectGuid decorGuid)
{
    auto setter = m_values.ModifyValue(&Account::m_housingStorageData).ModifyValue(&UF::HousingStorageData::Decor);
    RemoveMapUpdateFieldValue(setter, decorGuid);
}

}
