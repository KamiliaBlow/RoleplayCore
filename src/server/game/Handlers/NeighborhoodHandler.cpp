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

#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingPackets.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodCharter.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"

// ============================================================
// Neighborhood Charter System
// ============================================================

void WorldSession::HandleNeighborhoodCharterOpenConfirmationUI(WorldPackets::Neighborhood::NeighborhoodCharterOpenConfirmationUI const& /*neighborhoodCharterOpenConfirmationUI*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_OPEN_CHARTER_CONFIRMATION received for player {}",
        player->GetGUID().ToString());

    // Client requests to open the charter creation confirmation UI
    // The client handles UI display; server acknowledges readiness
}

void WorldSession::HandleNeighborhoodCharterCreate(WorldPackets::Neighborhood::NeighborhoodCharterCreate const& neighborhoodCharterCreate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_CREATE_NEIGHBORHOOD_CHARTER NeighborhoodMapID: {}, FactionFlags: {}, Name: {}",
        neighborhoodCharterCreate.NeighborhoodMapID, neighborhoodCharterCreate.FactionFlags,
        neighborhoodCharterCreate.Name);

    // Validate name
    if (neighborhoodCharterCreate.Name.empty() || neighborhoodCharterCreate.Name.length() > HOUSING_MAX_NAME_LENGTH)
    {
        TC_LOG_DEBUG("network", "HandleNeighborhoodCharterCreate: Invalid name length for player {}",
            player->GetGUID().ToString());
        return;
    }

    // Create charter object
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    NeighborhoodCharter charter(charterId, player->GetGUID());
    charter.SetName(neighborhoodCharterCreate.Name);
    charter.SetNeighborhoodMapID(neighborhoodCharterCreate.NeighborhoodMapID);
    charter.SetFactionFlags(neighborhoodCharterCreate.FactionFlags);
    charter.SetIsGuild(false);

    // Creator auto-signs
    charter.AddSignature(player->GetGUID());

    // Persist to DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    charter.SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Player {} created neighborhood charter '{}' (ID: {}, MapID: {})",
        player->GetGUID().ToString(), neighborhoodCharterCreate.Name, charterId,
        neighborhoodCharterCreate.NeighborhoodMapID);
}

void WorldSession::HandleNeighborhoodCharterEdit(WorldPackets::Neighborhood::NeighborhoodCharterEdit const& neighborhoodCharterEdit)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_EDIT_NEIGHBORHOOD_CHARTER NeighborhoodMapID: {}, FactionFlags: {}, Name: {}",
        neighborhoodCharterEdit.NeighborhoodMapID, neighborhoodCharterEdit.FactionFlags,
        neighborhoodCharterEdit.Name);

    // Validate name
    if (neighborhoodCharterEdit.Name.empty() || neighborhoodCharterEdit.Name.length() > HOUSING_MAX_NAME_LENGTH)
    {
        TC_LOG_DEBUG("network", "HandleNeighborhoodCharterEdit: Invalid name length for player {}",
            player->GetGUID().ToString());
        return;
    }

    // Edit updates the charter with new parameters (same charter ID, re-saved)
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    NeighborhoodCharter charter(charterId, player->GetGUID());
    charter.SetName(neighborhoodCharterEdit.Name);
    charter.SetNeighborhoodMapID(neighborhoodCharterEdit.NeighborhoodMapID);
    charter.SetFactionFlags(neighborhoodCharterEdit.FactionFlags);
    charter.SetIsGuild(false);

    // Creator auto-signs
    charter.AddSignature(player->GetGUID());

    // Re-persist
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    NeighborhoodCharter::DeleteFromDB(charterId, trans);
    charter.SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    TC_LOG_DEBUG("housing", "Player {} edited neighborhood charter '{}' (ID: {}, MapID: {})",
        player->GetGUID().ToString(), neighborhoodCharterEdit.Name, charterId,
        neighborhoodCharterEdit.NeighborhoodMapID);
}

void WorldSession::HandleNeighborhoodCharterFinalize(WorldPackets::Neighborhood::NeighborhoodCharterFinalize const& /*neighborhoodCharterFinalize*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_CONFIRMATION_RESPONSE received for player {}",
        player->GetGUID().ToString());

    // Finalize charter: create the neighborhood from charter data
    // For now, we create a default neighborhood since the charter state
    // is stored in DB. In a complete system this would load the charter
    // back and verify signatures before creating.
    Neighborhood* neighborhood = sNeighborhoodMgr.CreateNeighborhood(
        player->GetGUID(),
        "New Neighborhood",
        0,  // Will be loaded from charter
        0   // No faction restriction by default
    );

    if (neighborhood)
    {
        TC_LOG_DEBUG("housing", "Player {} finalized charter, created neighborhood {}",
            player->GetGUID().ToString(), neighborhood->GetGuid().ToString());

        // Clean up charter from DB
        uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        NeighborhoodCharter::DeleteFromDB(charterId, trans);
        CharacterDatabase.CommitTransaction(trans);
    }
    else
    {
        TC_LOG_DEBUG("housing", "Player {} failed to finalize charter - neighborhood creation failed",
            player->GetGUID().ToString());
    }
}

void WorldSession::HandleNeighborhoodCharterAddSignature(WorldPackets::Neighborhood::NeighborhoodCharterAddSignature const& neighborhoodCharterAddSignature)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_SIGN CharterGuid: {}",
        neighborhoodCharterAddSignature.CharterGuid.ToString());

    // Player signs a charter presented to them
    // The CharterGuid maps to the charter creator's player GUID counter
    // In a full implementation, we'd load the charter from DB, add signature, re-save
    TC_LOG_DEBUG("housing", "Player {} signed charter {}",
        player->GetGUID().ToString(), neighborhoodCharterAddSignature.CharterGuid.ToString());
}

void WorldSession::HandleNeighborhoodCharterSendSignatureRequest(WorldPackets::Neighborhood::NeighborhoodCharterSendSignatureRequest const& neighborhoodCharterSendSignatureRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_REQUEST_SIGNATURE TargetPlayerGuid: {}",
        neighborhoodCharterSendSignatureRequest.TargetPlayerGuid.ToString());

    // Validate target player is online and reachable
    Player* targetPlayer = ObjectAccessor::FindPlayer(neighborhoodCharterSendSignatureRequest.TargetPlayerGuid);
    if (!targetPlayer)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterSendSignatureRequest: Target player {} not found",
            neighborhoodCharterSendSignatureRequest.TargetPlayerGuid.ToString());
        return;
    }

    TC_LOG_DEBUG("housing", "Player {} requested signature from {} for charter",
        player->GetGUID().ToString(), targetPlayer->GetGUID().ToString());

    // In a complete implementation, this would send a signature request
    // notification to the target player's client
}

// ============================================================
// Neighborhood Management System
// ============================================================

void WorldSession::HandleNeighborhoodUpdateName(WorldPackets::Neighborhood::NeighborhoodUpdateName const& neighborhoodUpdateName)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_SET_NAME NeighborhoodGuid: {}, NewName: {}",
        neighborhoodUpdateName.NeighborhoodGuid.ToString(), neighborhoodUpdateName.NewName);

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodUpdateName.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Neighborhood {} not found",
            neighborhoodUpdateName.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can rename
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodUpdateName.NeighborhoodGuid.ToString());
        return;
    }

    // Validate name
    if (neighborhoodUpdateName.NewName.empty() || neighborhoodUpdateName.NewName.length() > HOUSING_MAX_NAME_LENGTH)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Invalid name length");
        return;
    }

    neighborhood->SetName(neighborhoodUpdateName.NewName);

    // Notify clients to invalidate cached name
    WorldPackets::Housing::InvalidateNeighborhoodName invalidate;
    invalidate.NeighborhoodGuid = neighborhoodUpdateName.NeighborhoodGuid;
    player->SendDirectMessage(invalidate.Write());

    TC_LOG_DEBUG("housing", "Neighborhood {} renamed to '{}' by player {}",
        neighborhoodUpdateName.NeighborhoodGuid.ToString(), neighborhoodUpdateName.NewName,
        player->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodSetPublicFlag(WorldPackets::Neighborhood::NeighborhoodSetPublicFlag const& neighborhoodSetPublicFlag)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_SET_PUBLIC NeighborhoodGuid: {}, IsPublic: {}",
        neighborhoodSetPublicFlag.NeighborhoodGuid.ToString(), neighborhoodSetPublicFlag.IsPublic);

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodSetPublicFlag.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodSetPublicFlag: Neighborhood {} not found",
            neighborhoodSetPublicFlag.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can change visibility
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodSetPublicFlag: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodSetPublicFlag.NeighborhoodGuid.ToString());
        return;
    }

    neighborhood->SetPublic(neighborhoodSetPublicFlag.IsPublic);

    TC_LOG_DEBUG("housing", "Neighborhood {} set to {} by player {}",
        neighborhoodSetPublicFlag.NeighborhoodGuid.ToString(),
        neighborhoodSetPublicFlag.IsPublic ? "public" : "private",
        player->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodAddSecondaryOwner(WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwner const& neighborhoodAddSecondaryOwner)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_ADD_MANAGER NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodAddSecondaryOwner.NeighborhoodGuid.ToString(), neighborhoodAddSecondaryOwner.PlayerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodAddSecondaryOwner.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodAddSecondaryOwner: Neighborhood {} not found",
            neighborhoodAddSecondaryOwner.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner can add managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodAddSecondaryOwner: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodAddSecondaryOwner.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->AddManager(neighborhoodAddSecondaryOwner.PlayerGuid);
    TC_LOG_DEBUG("housing", "AddManager result: {} for player {} in neighborhood {}",
        uint32(result), neighborhoodAddSecondaryOwner.PlayerGuid.ToString(),
        neighborhoodAddSecondaryOwner.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodRemoveSecondaryOwner(WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwner const& neighborhoodRemoveSecondaryOwner)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_REMOVE_MANAGER NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodRemoveSecondaryOwner.NeighborhoodGuid.ToString(), neighborhoodRemoveSecondaryOwner.PlayerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodRemoveSecondaryOwner.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodRemoveSecondaryOwner: Neighborhood {} not found",
            neighborhoodRemoveSecondaryOwner.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner can remove managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodRemoveSecondaryOwner: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodRemoveSecondaryOwner.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->RemoveManager(neighborhoodRemoveSecondaryOwner.PlayerGuid);
    TC_LOG_DEBUG("housing", "RemoveManager result: {} for player {} in neighborhood {}",
        uint32(result), neighborhoodRemoveSecondaryOwner.PlayerGuid.ToString(),
        neighborhoodRemoveSecondaryOwner.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodInviteResident(WorldPackets::Neighborhood::NeighborhoodInviteResident const& neighborhoodInviteResident)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_INVITE_RESIDENT NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodInviteResident.NeighborhoodGuid.ToString(), neighborhoodInviteResident.PlayerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodInviteResident.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodInviteResident: Neighborhood {} not found",
            neighborhoodInviteResident.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can invite
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodInviteResident: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodInviteResident.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->InviteResident(player->GetGUID(), neighborhoodInviteResident.PlayerGuid);
    TC_LOG_DEBUG("housing", "InviteResident result: {} for player {} in neighborhood {}",
        uint32(result), neighborhoodInviteResident.PlayerGuid.ToString(),
        neighborhoodInviteResident.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodCancelInvitation(WorldPackets::Neighborhood::NeighborhoodCancelInvitation const& neighborhoodCancelInvitation)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CANCEL_RESIDENT_INVITATION NeighborhoodGuid: {}, InviteeGuid: {}",
        neighborhoodCancelInvitation.NeighborhoodGuid.ToString(), neighborhoodCancelInvitation.InviteeGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodCancelInvitation.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodCancelInvitation: Neighborhood {} not found",
            neighborhoodCancelInvitation.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can cancel invitations
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodCancelInvitation: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodCancelInvitation.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->CancelInvitation(neighborhoodCancelInvitation.InviteeGuid);
    TC_LOG_DEBUG("housing", "CancelInvitation result: {} for invitee {} in neighborhood {}",
        uint32(result), neighborhoodCancelInvitation.InviteeGuid.ToString(),
        neighborhoodCancelInvitation.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodPlayerDeclineInvite(WorldPackets::Neighborhood::NeighborhoodPlayerDeclineInvite const& neighborhoodPlayerDeclineInvite)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_DECLINE_NEIGHBORHOOD_INVITATION NeighborhoodGuid: {}",
        neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodPlayerDeclineInvite.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodPlayerDeclineInvite: Neighborhood {} not found",
            neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->DeclineInvitation(player->GetGUID());
    TC_LOG_DEBUG("housing", "DeclineInvitation result: {} for player {} in neighborhood {}",
        uint32(result), player->GetGUID().ToString(),
        neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodPlayerGetInvite(WorldPackets::Neighborhood::NeighborhoodPlayerGetInvite const& neighborhoodPlayerGetInvite)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_GET_HOUSING_INVITATION NeighborhoodGuid: {}",
        neighborhoodPlayerGetInvite.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodPlayerGetInvite.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodPlayerGetInvite: Neighborhood {} not found",
            neighborhoodPlayerGetInvite.NeighborhoodGuid.ToString());
        return;
    }

    // Check if this player has a pending invite to this neighborhood
    bool hasInvite = neighborhood->HasPendingInvite(player->GetGUID());
    TC_LOG_DEBUG("housing", "Player {} {} a pending invite to neighborhood {}",
        player->GetGUID().ToString(), hasInvite ? "has" : "does not have",
        neighborhoodPlayerGetInvite.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodGetInvites(WorldPackets::Neighborhood::NeighborhoodGetInvites const& neighborhoodGetInvites)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_GET_INVITES NeighborhoodGuid: {}",
        neighborhoodGetInvites.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGetInvites.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetInvites: Neighborhood {} not found",
            neighborhoodGetInvites.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can view all pending invites
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetInvites: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGetInvites.NeighborhoodGuid.ToString());
        return;
    }

    std::vector<Neighborhood::PendingInvite> const& invites = neighborhood->GetPendingInvites();
    TC_LOG_DEBUG("housing", "Neighborhood {} has {} pending invites",
        neighborhoodGetInvites.NeighborhoodGuid.ToString(), uint32(invites.size()));
}

void WorldSession::HandleNeighborhoodBuyHouse(WorldPackets::Neighborhood::NeighborhoodBuyHouse const& neighborhoodBuyHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_PURCHASE_PLOT NeighborhoodGuid: {}, PlotIndex: {}",
        neighborhoodBuyHouse.NeighborhoodGuid.ToString(), neighborhoodBuyHouse.PlotIndex);

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodBuyHouse.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Neighborhood {} not found",
            neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    // Must be a member of the neighborhood
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} is not a member of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    // Must not already own a house
    if (player->GetHousing())
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} already has a house",
            player->GetGUID().ToString());
        return;
    }

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), neighborhoodBuyHouse.PlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        player->CreateHousing(neighborhoodBuyHouse.NeighborhoodGuid, neighborhoodBuyHouse.PlotIndex);
        TC_LOG_DEBUG("housing", "Player {} purchased plot {} in neighborhood {}",
            player->GetGUID().ToString(), neighborhoodBuyHouse.PlotIndex,
            neighborhoodBuyHouse.NeighborhoodGuid.ToString());
    }
    else
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: PurchasePlot result: {} for player {}",
            uint32(result), player->GetGUID().ToString());
    }
}

void WorldSession::HandleNeighborhoodMoveHouse(WorldPackets::Neighborhood::NeighborhoodMoveHouse const& neighborhoodMoveHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_MOVE_HOUSE SourcePlotGuid: {}, NewPlotIndex: {}",
        neighborhoodMoveHouse.SourcePlotGuid.ToString(), neighborhoodMoveHouse.NewPlotIndex);

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Neighborhood not found for player {}",
            player->GetGUID().ToString());
        return;
    }

    HousingResult result = neighborhood->MoveHouse(neighborhoodMoveHouse.SourcePlotGuid, neighborhoodMoveHouse.NewPlotIndex);
    TC_LOG_DEBUG("housing", "MoveHouse result: {} for plot {} to index {}",
        uint32(result), neighborhoodMoveHouse.SourcePlotGuid.ToString(),
        neighborhoodMoveHouse.NewPlotIndex);
}

void WorldSession::HandleNeighborhoodOpenCornerstoneUI(WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUI const& neighborhoodOpenCornerstoneUI)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_INTERACT_WITH_CORNERSTONE CornerstoneGuid: {}",
        neighborhoodOpenCornerstoneUI.CornerstoneGuid.ToString());

    // Cornerstone interaction opens the neighborhood UI
    // The client handles the UI display based on the cornerstone's neighborhood data
    TC_LOG_DEBUG("housing", "Player {} interacted with cornerstone {}",
        player->GetGUID().ToString(), neighborhoodOpenCornerstoneUI.CornerstoneGuid.ToString());
}

void WorldSession::HandleNeighborhoodOfferOwnership(WorldPackets::Neighborhood::NeighborhoodOfferOwnership const& neighborhoodOfferOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_OFFER_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}, NewOwnerGuid: {}",
        neighborhoodOfferOwnership.NeighborhoodGuid.ToString(), neighborhoodOfferOwnership.NewOwnerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodOfferOwnership.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodOfferOwnership: Neighborhood {} not found",
            neighborhoodOfferOwnership.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner can transfer ownership
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodOfferOwnership: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodOfferOwnership.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->TransferOwnership(neighborhoodOfferOwnership.NewOwnerGuid);
    TC_LOG_DEBUG("housing", "TransferOwnership result: {} to player {} for neighborhood {}",
        uint32(result), neighborhoodOfferOwnership.NewOwnerGuid.ToString(),
        neighborhoodOfferOwnership.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodGetRoster(WorldPackets::Neighborhood::NeighborhoodGetRoster const& neighborhoodGetRoster)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_REQUEST_NEIGHBORHOOD_ROSTER NeighborhoodGuid: {}",
        neighborhoodGetRoster.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGetRoster.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Neighborhood {} not found",
            neighborhoodGetRoster.NeighborhoodGuid.ToString());
        return;
    }

    // Must be a member to view roster
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Player {} is not a member of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGetRoster.NeighborhoodGuid.ToString());
        return;
    }

    std::vector<Neighborhood::Member> const& members = neighborhood->GetMembers();
    TC_LOG_DEBUG("housing", "Neighborhood {} roster: {} members",
        neighborhoodGetRoster.NeighborhoodGuid.ToString(), uint32(members.size()));
}

void WorldSession::HandleNeighborhoodEvictPlot(WorldPackets::Neighborhood::NeighborhoodEvictPlot const& neighborhoodEvictPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_EVICT_PLAYER NeighborhoodGuid: {}, PlotGuid: {}",
        neighborhoodEvictPlot.NeighborhoodGuid.ToString(), neighborhoodEvictPlot.PlotGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodEvictPlot.NeighborhoodGuid);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodEvictPlot: Neighborhood {} not found",
            neighborhoodEvictPlot.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can evict
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodEvictPlot: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodEvictPlot.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->EvictPlayer(neighborhoodEvictPlot.PlotGuid);
    TC_LOG_DEBUG("housing", "EvictPlayer result: {} for plot {} in neighborhood {}",
        uint32(result), neighborhoodEvictPlot.PlotGuid.ToString(),
        neighborhoodEvictPlot.NeighborhoodGuid.ToString());
}
