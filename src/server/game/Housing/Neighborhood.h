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

#ifndef TRINITYCORE_NEIGHBORHOOD_H
#define TRINITYCORE_NEIGHBORHOOD_H

#include "Define.h"
#include "DatabaseEnvFwd.h"
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>

class TC_GAME_API Neighborhood
{
public:
    struct Member
    {
        ObjectGuid PlayerGuid;
        uint8 Role = 0;        // NeighborhoodMemberRole
        uint32 JoinTime = 0;
        uint8 PlotIndex = INVALID_PLOT_INDEX;
    };

    struct PlotInfo
    {
        uint8 PlotIndex = 0;
        ObjectGuid OwnerGuid;
        ObjectGuid HouseGuid;
        ObjectGuid OwnerBnetGuid;
    };

    struct PendingInvite
    {
        ObjectGuid InviteeGuid;
        ObjectGuid InviterGuid;
        uint32 InviteTime = 0;
    };

    explicit Neighborhood(ObjectGuid guid);

    // DB persistence
    bool LoadFromDB(PreparedQueryResult neighborhood, PreparedQueryResult members, PreparedQueryResult invites);
    void SaveToDB(CharacterDatabaseTransaction trans);
    static void DeleteFromDB(ObjectGuid::LowType guid, CharacterDatabaseTransaction trans);

    // Accessors
    ObjectGuid GetGuid() const { return _guid; }
    std::string const& GetName() const { return _name; }
    uint32 GetNeighborhoodMapID() const { return _neighborhoodMapID; }
    ObjectGuid GetOwnerGuid() const { return _ownerGuid; }
    int32 GetFactionRestriction() const { return _factionRestriction; }
    bool IsPublic() const { return _isPublic; }
    uint32 GetCreateTime() const { return _createTime; }

    // Management
    void SetName(std::string const& name);
    void SetPublic(bool isPublic);
    HousingResult AddManager(ObjectGuid playerGuid);
    HousingResult RemoveManager(ObjectGuid playerGuid);
    HousingResult InviteResident(ObjectGuid inviterGuid, ObjectGuid inviteeGuid);
    HousingResult CancelInvitation(ObjectGuid inviteeGuid);
    HousingResult AcceptInvitation(ObjectGuid playerGuid);
    HousingResult DeclineInvitation(ObjectGuid playerGuid);
    HousingResult EvictPlayer(ObjectGuid plotGuid);
    HousingResult TransferOwnership(ObjectGuid newOwnerGuid);

    // Plot management
    HousingResult PurchasePlot(ObjectGuid playerGuid, uint8 plotIndex);
    void UpdatePlotHouseInfo(uint8 plotIndex, ObjectGuid houseGuid, ObjectGuid ownerBnetGuid);
    HousingResult MoveHouse(ObjectGuid sourcePlotGuid, uint8 newPlotIndex);
    PlotInfo const* GetPlotInfo(uint8 plotIndex) const;
    std::vector<PlotInfo> const& GetPlots() const { return _plots; }

    // Members
    HousingResult AddResident(ObjectGuid playerGuid);
    Member const* GetMember(ObjectGuid playerGuid) const;
    std::vector<Member> const& GetMembers() const { return _members; }
    bool IsMember(ObjectGuid playerGuid) const;
    bool IsManager(ObjectGuid playerGuid) const;
    bool IsOwner(ObjectGuid playerGuid) const;
    uint32 GetMemberCount() const { return static_cast<uint32>(_members.size()); }

    // Invites
    bool HasPendingInvite(ObjectGuid playerGuid) const;
    std::vector<PendingInvite> const& GetPendingInvites() const { return _pendingInvites; }

private:
    ObjectGuid _guid;
    std::string _name;
    uint32 _neighborhoodMapID = 0;
    ObjectGuid _ownerGuid;
    int32 _factionRestriction = 0;
    bool _isPublic = false;
    uint32 _createTime = 0;

    std::vector<Member> _members;
    std::vector<PlotInfo> _plots;
    std::vector<PendingInvite> _pendingInvites;
};

#endif // TRINITYCORE_NEIGHBORHOOD_H
