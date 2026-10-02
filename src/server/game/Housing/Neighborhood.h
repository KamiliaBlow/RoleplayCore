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
#include "Housing.h"
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

class WorldPacket;

namespace WorldPackets::Neighborhood
{
    class NeighborhoodGetRosterResponse;
}

class TC_GAME_API Neighborhood
{
public:
    struct Member
    {
        ObjectGuid PlayerGuid;
        ObjectGuid HouseGuid;
        uint8 Role = 0;        // NeighborhoodMemberRole
        uint32 JoinTime = 0;
        uint8 PlotIndex = INVALID_PLOT_INDEX;
        uint8 StatusFlags = 0;
    };

    struct PlotInfo
    {
        uint8 PlotIndex = INVALID_PLOT_INDEX;
        ObjectGuid PlotGuid;
        ObjectGuid OwnerGuid;
        ObjectGuid HouseGuid;
        ObjectGuid OwnerBnetGuid;

        // Mirrored from character_housing so offline owners' houses still render for other players.
        uint8 HouseLevel = 1;
        uint64 HouseFavor = 0;
        std::string HouseName;
        uint32 HouseType = 0;
        Optional<Position> HousePosition; // custom house spot (unset = plot default)
        // Key = FixturePointId (ExteriorComponentHook slot), value = FixtureOptionId (ExteriorComponent override)
        std::unordered_map<uint32, uint32> Fixtures;
        // Exterior entries (RoomGuid.IsEmpty()) spawn at preload; interior entries serve visitor interior maps.
        std::vector<Housing::PlacedDecor> Decor;
        std::vector<Housing::Room> Rooms;
        uint32 HouseSettingsFlags = 0; // CanVisitorAccess checks work with the owner offline

        bool IsOccupied() const { return PlotIndex != INVALID_PLOT_INDEX; }
    };

    struct PendingInvite
    {
        ObjectGuid InviteeGuid;
        ObjectGuid InviterGuid;
        uint32 InviteTime = 0;
    };

    struct PendingOwnershipTransfer
    {
        ObjectGuid TargetGuid;
        uint32 OfferTime = 0;
    };

    explicit Neighborhood(ObjectGuid guid);

    // DB persistence
    bool LoadFromDB(PreparedQueryResult neighborhood, PreparedQueryResult members, PreparedQueryResult invites,
        PreparedQueryResult memberFixtures = nullptr, PreparedQueryResult memberDecor = nullptr,
        PreparedQueryResult memberRooms = nullptr);
    void SaveToDB(CharacterDatabaseTransaction trans);
    static void DeleteFromDB(ObjectGuid::LowType guid, CharacterDatabaseTransaction trans);

    // Accessors
    ObjectGuid GetGuid() const { return _guid; }
    std::string const& GetName() const { return _name; }
    uint32 GetNeighborhoodMapID() const { return _neighborhoodMapID; }
    ObjectGuid GetOwnerGuid() const { return _ownerGuid; }
    /// Client derives NeighborhoodOwnerType from this GUID: empty = public, HighGuid::Guild = guild, else charter.
    ObjectGuid GetClientOwnerGuid() const { return _guildId ? ObjectGuid::Create<HighGuid::Guild>(_guildId) : _ownerGuid; }
    int32 GetFactionRestriction() const { return _factionRestriction; }
    void SetFactionRestriction(int32 faction) { _factionRestriction = faction; }
    bool IsPublic() const { return _isPublic; }
    uint32 GetCreateTime() const { return _createTime; }

    // Guild association
    uint32 GetGuildId() const { return _guildId; }
    void SetGuildId(uint32 guildId) { _guildId = guildId; }

    // Plot reservations (in-memory, time-limited holds before purchase)
    bool ReservePlot(ObjectGuid playerGuid, uint8 plotIndex);
    bool ClearReservation(ObjectGuid playerGuid);
    bool HasReservation(ObjectGuid playerGuid) const;
    uint8 GetReservedPlot(ObjectGuid playerGuid) const;
    // Returns the reserver's GUID if plotIndex is locked by another player; sweeps expired reservations.
    ObjectGuid GetPlotReserverOther(uint8 plotIndex, ObjectGuid viewerGuid);

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
    HousingResult OfferOwnership(ObjectGuid targetGuid);
    HousingResult AcceptOwnershipTransfer(ObjectGuid acceptorGuid);
    HousingResult RejectOwnershipTransfer(ObjectGuid rejectorGuid);
    bool HasPendingTransfer() const { return _pendingTransfer.has_value(); }
    Optional<PendingOwnershipTransfer> const& GetPendingTransfer() const { return _pendingTransfer; }

    // Plot management
    HousingResult PurchasePlot(ObjectGuid playerGuid, uint8 plotIndex);
    void UpdatePlotHouseInfo(uint8 plotIndex, ObjectGuid houseGuid, ObjectGuid ownerBnetGuid);
    void UpdatePlotSettingsFlags(ObjectGuid ownerGuid, uint32 settingsFlags);
    void UpdatePlotHousePosition(ObjectGuid ownerGuid, Optional<Position> const& housePosition);
    void UpdatePlotHouseType(ObjectGuid ownerGuid, uint32 houseType);
    HousingResult MoveHouse(ObjectGuid sourcePlotOwner, uint8 newPlotIndex);
    // House went to another character of the same account: the plot follows; a resident membership moves with it.
    bool TransferPlot(ObjectGuid oldOwnerGuid, ObjectGuid newOwnerGuid, CharacterDatabaseTransaction trans);
    void SetPlotAreaTriggerGuid(uint8 plotIndex, ObjectGuid atGuid);
    // Frees ownerGuid's plot on house delete / kiosk reset; the player stays a member. Returns true if freed.
    bool ReleasePlot(ObjectGuid ownerGuid);

    PlotInfo const* GetPlotInfo(uint8 plotIndex) const
    {
        return (plotIndex < MAX_NEIGHBORHOOD_PLOTS && _plots[plotIndex].IsOccupied())
            ? &_plots[plotIndex] : nullptr;
    }

    std::array<PlotInfo, MAX_NEIGHBORHOOD_PLOTS> const& GetPlots() const { return _plots; }
    uint32 GetOccupiedPlotCount() const;

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

    // Broadcast
    void BroadcastPacket(WorldPacket const* packet, ObjectGuid excludeGuid = ObjectGuid::Empty) const;
    // The roster as SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE carries it.
    void BuildRosterResponse(WorldPackets::Neighborhood::NeighborhoodGetRosterResponse& response) const;
    // Whole-roster refresh for all online members' bulletin boards (status updates can't add/remove members).
    void BroadcastRoster(ObjectGuid excludeGuid = ObjectGuid::Empty) const;
    // A member's resident type or online state changed (SMSG_NEIGHBORHOOD_ROSTER_RESIDENT_UPDATE).
    void BroadcastMemberStatus(ObjectGuid playerGuid, bool isOnline) const;
    void BroadcastMemberStatus(ObjectGuid playerGuid) const;

    // Rebuild NeighborhoodMirrorData on online members after any name/owner/manager/house mutation.
    void RefreshMirrorDataForOnlineMembers() const;
    // Per-player variant — used by the map-entry refresh path (SendNeighborhoodMapRefresh).
    void RefreshMirrorDataForPlayer(Player* player) const;
    // Setter-only half of RefreshMirrorDataForPlayer (no packet); caller picks the wire form.
    void RebuildMirrorDataFor(Player* player) const;

private:
    PlotInfo* GetPlotByOwner(ObjectGuid ownerGuid);

    ObjectGuid _guid;
    std::string _name;
    uint32 _neighborhoodMapID = 0;
    ObjectGuid _ownerGuid;
    int32 _factionRestriction = 0;
    bool _isPublic = false;
    uint32 _createTime = 0;
    uint32 _guildId = 0;

    std::vector<Member> _members;
    std::array<PlotInfo, MAX_NEIGHBORHOOD_PLOTS> _plots{};
    std::vector<PendingInvite> _pendingInvites;
    Optional<PendingOwnershipTransfer> _pendingTransfer;

    // In-memory plot reservations: playerGuid -> {plotIndex, reserveTime}
    struct PlotReservation
    {
        uint8 PlotIndex = 0;
        uint32 ReserveTime = 0;
    };
    std::unordered_map<ObjectGuid, PlotReservation> _plotReservations;
};

#endif // TRINITYCORE_NEIGHBORHOOD_H
