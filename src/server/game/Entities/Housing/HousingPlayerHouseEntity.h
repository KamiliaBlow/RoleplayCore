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

#ifndef TRINITYCORE_HOUSING_PLAYER_HOUSE_ENTITY_H
#define TRINITYCORE_HOUSING_PLAYER_HOUSE_ENTITY_H

#include "BaseEntity.h"

class WorldSession;

// FHousingPlayerHouse_C carrier entity on a Housing/3 GUID (subType 3, arg2 7).
class HousingPlayerHouseEntity final : public BaseEntity
{
public:
    explicit HousingPlayerHouseEntity(WorldSession* session, ObjectGuid guid);

    void ClearUpdateMask(bool remove) override;
    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;
    void BuildUpdate(UpdateDataMapType& data_map) override;
    std::string GetDebugInfo() const override;

    void SendUpdateToPlayer(Player* player);
    // The entity stands for the logged-in character's house (Housing::SyncUpdateFields); its GUID follows that house.
    void SetGuid(ObjectGuid const& guid) { _Create(guid); }
    // force-emit CREATE_OBJECT (bypasses HaveAtClient); map-icon refresh only fires on CREATE
    void SendCreateToPlayer(Player* player);

    // HouseType/HouseSize are not part of this fragment
    void SetPlotIndex(int32 plotIndex);
    void SetLevel(uint32 level);
    void SetFavor(uint64 favor);
    void SetBudgets(uint32 interiorDecor, uint32 exteriorDecor, uint32 room, uint32 fixture);
    void SetBnetAccount(ObjectGuid bnetAccountGuid);
    void SetCosmeticOwner(ObjectGuid ownerGuid);
    void SetEntityGUID(ObjectGuid entityGuid);
    void SetObjectType(TypeID typeId) { m_objectTypeId = typeId; }

    UF::UpdateField<UF::HousingPlayerHouseData, int32(WowCS::EntityFragment::FHousingPlayerHouse_C), 0> m_housingPlayerHouseData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;
    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;

private:
    WorldSession* _session;
};

#endif // TRINITYCORE_HOUSING_PLAYER_HOUSE_ENTITY_H
