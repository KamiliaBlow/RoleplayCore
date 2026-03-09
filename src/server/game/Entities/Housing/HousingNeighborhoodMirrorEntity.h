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

#ifndef TRINITYCORE_HOUSING_NEIGHBORHOOD_MIRROR_ENTITY_H
#define TRINITYCORE_HOUSING_NEIGHBORHOOD_MIRROR_ENTITY_H

#include "BaseEntity.h"

class WorldSession;

// Separate entity carrying FNeighborhoodMirrorData_C fragment.
// Retail sends this on a Housing/4 GUID (subType=4), NOT on the BNetAccount entity.
class HousingNeighborhoodMirrorEntity final : public BaseEntity
{
public:
    explicit HousingNeighborhoodMirrorEntity(WorldSession* session, ObjectGuid guid);

    void ClearUpdateMask(bool remove) override;
    std::string GetNameForLocaleIdx(LocaleConstant locale) const override;
    void BuildUpdate(UpdateDataMapType& data_map) override;
    std::string GetDebugInfo() const override;

    void SendUpdateToPlayer(Player* player);

    // Neighborhood mirror setters
    void SetName(std::string const& name);
    void SetOwnerGUID(ObjectGuid ownerGuid);
    void AddHouse(ObjectGuid houseGuid, ObjectGuid ownerGuid);
    void ClearHouses();
    void AddManager(ObjectGuid bnetAccountGuid, ObjectGuid playerGuid);
    void ClearManagers();

    UF::UpdateField<UF::NeighborhoodMirrorData, int32(WowCS::EntityFragment::FNeighborhoodMirrorData_C), 0> m_neighborhoodMirrorData;

protected:
    UF::UpdateFieldFlag GetUpdateFieldFlagsFor(Player const* target) const override;
    bool AddToObjectUpdate() override;
    void RemoveFromObjectUpdate() override;

private:
    WorldSession* _session;
};

#endif // TRINITYCORE_HOUSING_NEIGHBORHOOD_MIRROR_ENTITY_H
