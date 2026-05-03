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

        // Mirrored from character_housing for tooltip display of OTHER players' houses
        // on the neighborhood map (hover info). Set by Neighborhood::LoadFromDB; refreshed
        // on ownership / level / favor changes.
        uint8 HouseLevel = 1;
        uint64 HouseFavor = 0;
        std::string HouseName;

        // Mirrored from character_housing so the neighborhood map can spawn the
        // correct WMO geometry for EVERY occupied plot at preload. The data is
        // always in the DB regardless of whether the owner is currently online —
        // PlotInfo carries enough of it to build the exterior visual for every
        // plot at map init without needing a live Housing object.
        uint32 HouseType = 0;

        // Mirrored from character_housing_fixtures. Key = FixturePointId (DB2
        // ExteriorComponentHook slot), value = FixtureOptionId (DB2
        // ExteriorComponent override). Drives the correct roof/doors/windows
        // on every plot's exterior, not just the logged-in owner's.
        std::unordered_map<uint32, uint32> Fixtures;

        // Mirrored from character_housing_decor. Every placed decor item
        // (exterior AND interior) for this plot's owner. HousingMap uses the
        // exterior entries (RoomGuid.IsEmpty()) at preload so visitors see
        // neighbours' placed decor even when the owner is offline. Interior
        // entries are reused when a visitor opens the owner's interior map.
        std::vector<Housing::PlacedDecor> Decor;

        // Mirrored from character_housing_rooms. Interior-room layout for the
        // plot owner. Used by HouseInteriorMap to spawn the owner's actual
        // rooms when a visitor enters their house — independent of whether
        // the owner is currently online.
        std::vector<Housing::Room> Rooms;

        // Mirrored from character_housing.settingsFlags so visitor permission
        // checks (CanVisitorAccess) work when the plot owner is offline.
        // Refreshed when an online owner mutates their Housing settings.
        uint32 HouseSettingsFlags = 0;

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
    // Returns the reserver's GUID if `plotIndex` is currently locked by another
    // player, or ObjectGuid::Empty when the plot is free / locked by `viewerGuid`.
    // Sweeps expired reservations as a side-effect (same 5-minute window as ReservePlot).
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
    HousingResult MoveHouse(ObjectGuid sourcePlotOwner, uint8 newPlotIndex);
    void SetPlotAreaTriggerGuid(uint8 plotIndex, ObjectGuid atGuid);

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

    // Rebuild NeighborhoodMirrorData on every online member's Account entity.
    // Call after any mutation to name, owner, managers, or houses.
    void RefreshMirrorDataForOnlineMembers() const;

private:
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
