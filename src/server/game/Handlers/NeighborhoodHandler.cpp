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
#include "HousingPackets.h"
#include "Log.h"
#include "Player.h"

// ============================================================
// Neighborhood Charter System
// ============================================================

void WorldSession::HandleNeighborhoodCharterOpenConfirmationUI(WorldPackets::Neighborhood::NeighborhoodCharterOpenConfirmationUI const& /*neighborhoodCharterOpenConfirmationUI*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_OPEN_CHARTER_CONFIRMATION received");
}

void WorldSession::HandleNeighborhoodCharterCreate(WorldPackets::Neighborhood::NeighborhoodCharterCreate const& neighborhoodCharterCreate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_CREATE_NEIGHBORHOOD_CHARTER NeighborhoodMapID: {}, FactionFlags: {}, Name: {}",
        neighborhoodCharterCreate.NeighborhoodMapID, neighborhoodCharterCreate.FactionFlags,
        neighborhoodCharterCreate.Name);
}

void WorldSession::HandleNeighborhoodCharterEdit(WorldPackets::Neighborhood::NeighborhoodCharterEdit const& neighborhoodCharterEdit)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_EDIT_NEIGHBORHOOD_CHARTER NeighborhoodMapID: {}, FactionFlags: {}, Name: {}",
        neighborhoodCharterEdit.NeighborhoodMapID, neighborhoodCharterEdit.FactionFlags,
        neighborhoodCharterEdit.Name);
}

void WorldSession::HandleNeighborhoodCharterFinalize(WorldPackets::Neighborhood::NeighborhoodCharterFinalize const& /*neighborhoodCharterFinalize*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_CONFIRMATION_RESPONSE received");
}

void WorldSession::HandleNeighborhoodCharterAddSignature(WorldPackets::Neighborhood::NeighborhoodCharterAddSignature const& neighborhoodCharterAddSignature)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_SIGN CharterGuid: {}",
        neighborhoodCharterAddSignature.CharterGuid.ToString());
}

void WorldSession::HandleNeighborhoodCharterSendSignatureRequest(WorldPackets::Neighborhood::NeighborhoodCharterSendSignatureRequest const& neighborhoodCharterSendSignatureRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CHARTER_REQUEST_SIGNATURE TargetPlayerGuid: {}",
        neighborhoodCharterSendSignatureRequest.TargetPlayerGuid.ToString());
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
}

void WorldSession::HandleNeighborhoodSetPublicFlag(WorldPackets::Neighborhood::NeighborhoodSetPublicFlag const& neighborhoodSetPublicFlag)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_SET_PUBLIC NeighborhoodGuid: {}, IsPublic: {}",
        neighborhoodSetPublicFlag.NeighborhoodGuid.ToString(), neighborhoodSetPublicFlag.IsPublic);
}

void WorldSession::HandleNeighborhoodAddSecondaryOwner(WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwner const& neighborhoodAddSecondaryOwner)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_ADD_MANAGER NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodAddSecondaryOwner.NeighborhoodGuid.ToString(), neighborhoodAddSecondaryOwner.PlayerGuid.ToString());
}

void WorldSession::HandleNeighborhoodRemoveSecondaryOwner(WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwner const& neighborhoodRemoveSecondaryOwner)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_REMOVE_MANAGER NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodRemoveSecondaryOwner.NeighborhoodGuid.ToString(), neighborhoodRemoveSecondaryOwner.PlayerGuid.ToString());
}

void WorldSession::HandleNeighborhoodInviteResident(WorldPackets::Neighborhood::NeighborhoodInviteResident const& neighborhoodInviteResident)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_INVITE_RESIDENT NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodInviteResident.NeighborhoodGuid.ToString(), neighborhoodInviteResident.PlayerGuid.ToString());
}

void WorldSession::HandleNeighborhoodCancelInvitation(WorldPackets::Neighborhood::NeighborhoodCancelInvitation const& neighborhoodCancelInvitation)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_CANCEL_RESIDENT_INVITATION NeighborhoodGuid: {}, InviteeGuid: {}",
        neighborhoodCancelInvitation.NeighborhoodGuid.ToString(), neighborhoodCancelInvitation.InviteeGuid.ToString());
}

void WorldSession::HandleNeighborhoodPlayerDeclineInvite(WorldPackets::Neighborhood::NeighborhoodPlayerDeclineInvite const& neighborhoodPlayerDeclineInvite)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_DECLINE_NEIGHBORHOOD_INVITATION NeighborhoodGuid: {}",
        neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodPlayerGetInvite(WorldPackets::Neighborhood::NeighborhoodPlayerGetInvite const& neighborhoodPlayerGetInvite)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_GET_HOUSING_INVITATION NeighborhoodGuid: {}",
        neighborhoodPlayerGetInvite.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodGetInvites(WorldPackets::Neighborhood::NeighborhoodGetInvites const& neighborhoodGetInvites)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_GET_INVITES NeighborhoodGuid: {}",
        neighborhoodGetInvites.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodBuyHouse(WorldPackets::Neighborhood::NeighborhoodBuyHouse const& neighborhoodBuyHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_PURCHASE_PLOT NeighborhoodGuid: {}, PlotIndex: {}",
        neighborhoodBuyHouse.NeighborhoodGuid.ToString(), neighborhoodBuyHouse.PlotIndex);
}

void WorldSession::HandleNeighborhoodMoveHouse(WorldPackets::Neighborhood::NeighborhoodMoveHouse const& neighborhoodMoveHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_MOVE_HOUSE SourcePlotGuid: {}, NewPlotIndex: {}",
        neighborhoodMoveHouse.SourcePlotGuid.ToString(), neighborhoodMoveHouse.NewPlotIndex);
}

void WorldSession::HandleNeighborhoodOpenCornerstoneUI(WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUI const& neighborhoodOpenCornerstoneUI)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_INTERACT_WITH_CORNERSTONE CornerstoneGuid: {}",
        neighborhoodOpenCornerstoneUI.CornerstoneGuid.ToString());
}

void WorldSession::HandleNeighborhoodOfferOwnership(WorldPackets::Neighborhood::NeighborhoodOfferOwnership const& neighborhoodOfferOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_OFFER_NEIGHBORHOOD_OWNERSHIP NeighborhoodGuid: {}, NewOwnerGuid: {}",
        neighborhoodOfferOwnership.NeighborhoodGuid.ToString(), neighborhoodOfferOwnership.NewOwnerGuid.ToString());
}

void WorldSession::HandleNeighborhoodGetRoster(WorldPackets::Neighborhood::NeighborhoodGetRoster const& neighborhoodGetRoster)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_REQUEST_NEIGHBORHOOD_ROSTER NeighborhoodGuid: {}",
        neighborhoodGetRoster.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodEvictPlot(WorldPackets::Neighborhood::NeighborhoodEvictPlot const& neighborhoodEvictPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("network", "CMSG_NEIGHBORHOOD_EVICT_PLAYER NeighborhoodGuid: {}, PlotGuid: {}",
        neighborhoodEvictPlot.NeighborhoodGuid.ToString(), neighborhoodEvictPlot.PlotGuid.ToString());
}
