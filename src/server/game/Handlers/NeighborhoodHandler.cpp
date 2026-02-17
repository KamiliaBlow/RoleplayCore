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
#include "Account.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
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

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_OPEN_CONFIRMATION_UI received for player {}",
        player->GetGUID().ToString());

    // Client requests to open the charter creation confirmation UI
    // The client handles UI display; server acknowledges readiness
    WorldPackets::Neighborhood::NeighborhoodCharterOpenConfirmationUIResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Sent NeighborhoodCharterOpenConfirmationUIResponse (SUCCESS) to player {}",
        player->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodCharterCreate(WorldPackets::Neighborhood::NeighborhoodCharterCreate const& neighborhoodCharterCreate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_CREATE NeighborhoodMapID: {}, FactionFlags: {}, Name: {}",
        neighborhoodCharterCreate.NeighborhoodMapID, neighborhoodCharterCreate.FactionFlags,
        neighborhoodCharterCreate.Name);

    // Validate name
    if (neighborhoodCharterCreate.Name.empty() || neighborhoodCharterCreate.Name.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_INVALID_CHARTER_NAME);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "HandleNeighborhoodCharterCreate: Invalid name length for player {}",
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

    WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Player {} created neighborhood charter '{}' (ID: {}, MapID: {})",
        player->GetGUID().ToString(), neighborhoodCharterCreate.Name, charterId,
        neighborhoodCharterCreate.NeighborhoodMapID);
}

void WorldSession::HandleNeighborhoodCharterEdit(WorldPackets::Neighborhood::NeighborhoodCharterEdit const& neighborhoodCharterEdit)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_EDIT NeighborhoodMapID: {}, FactionFlags: {}, Name: {}",
        neighborhoodCharterEdit.NeighborhoodMapID, neighborhoodCharterEdit.FactionFlags,
        neighborhoodCharterEdit.Name);

    // Validate name
    if (neighborhoodCharterEdit.Name.empty() || neighborhoodCharterEdit.Name.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_INVALID_CHARTER_NAME);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "HandleNeighborhoodCharterEdit: Invalid name length for player {}",
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

    WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Player {} edited neighborhood charter '{}' (ID: {}, MapID: {})",
        player->GetGUID().ToString(), neighborhoodCharterEdit.Name, charterId,
        neighborhoodCharterEdit.NeighborhoodMapID);
}

void WorldSession::HandleNeighborhoodCharterFinalize(WorldPackets::Neighborhood::NeighborhoodCharterFinalize const& /*neighborhoodCharterFinalize*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_FINALIZE received for player {}",
        player->GetGUID().ToString());

    // Load charter from DB using player's GUID counter as charter ID
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult charterResult = CharacterDatabase.Query(stmt);

    if (!charterResult)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_ALLOWED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterFinalize: No charter found for player {}",
            player->GetGUID().ToString());
        return;
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult sigResult = CharacterDatabase.Query(stmt);

    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!charter.LoadFromDB(charterResult, sigResult))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_DB_ERROR);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterFinalize: Failed to load charter {} from DB",
            charterId);
        return;
    }

    // Verify enough signatures
    if (!charter.HasEnoughSignatures())
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_ENOUGH_SIGNATURES);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterFinalize: Charter {} has only {}/{} signatures",
            charterId, charter.GetSignatureCount(), MIN_CHARTER_SIGNATURES);
        return;
    }

    // Create neighborhood from charter data
    Neighborhood* neighborhood = sNeighborhoodMgr.CreateNeighborhood(
        player->GetGUID(),
        charter.GetName(),
        charter.GetNeighborhoodMapID(),
        charter.GetFactionFlags()
    );

    if (neighborhood)
    {
        // Clean up charter from DB
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        NeighborhoodCharter::DeleteFromDB(charterId, trans);
        CharacterDatabase.CommitTransaction(trans);

        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "Player {} finalized charter '{}', created neighborhood {}",
            player->GetGUID().ToString(), charter.GetName(),
            neighborhood->GetGuid().ToString());
    }
    else
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_DB_ERROR);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "Player {} failed to finalize charter '{}' - neighborhood creation failed",
            player->GetGUID().ToString(), charter.GetName());
    }
}

void WorldSession::HandleNeighborhoodCharterAddSignature(WorldPackets::Neighborhood::NeighborhoodCharterAddSignature const& neighborhoodCharterAddSignature)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_ADD_SIGNATURECharterGuid: {}",
        neighborhoodCharterAddSignature.CharterGuid.ToString());

    // CharterGuid counter maps to charter DB ID
    uint64 charterId = neighborhoodCharterAddSignature.CharterGuid.GetCounter();

    // Load charter from DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult charterResult = CharacterDatabase.Query(stmt);

    if (!charterResult)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_ALLOWED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterAddSignature: Charter {} not found in DB",
            charterId);
        return;
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult sigResult = CharacterDatabase.Query(stmt);

    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!charter.LoadFromDB(charterResult, sigResult))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_DB_ERROR);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterAddSignature: Failed to load charter {} from DB",
            charterId);
        return;
    }

    // Add player's signature (validates not already signed, not creator)
    if (!charter.AddSignature(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_ALLOWED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterAddSignature: Player {} could not sign charter {}",
            player->GetGUID().ToString(), charterId);
        return;
    }

    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.CharterGuid = neighborhoodCharterAddSignature.CharterGuid;
    response.SignerGuid = player->GetGUID();
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Player {} signed charter {} ({}/{} signatures)",
        player->GetGUID().ToString(), charterId,
        charter.GetSignatureCount(), MIN_CHARTER_SIGNATURES);
}

void WorldSession::HandleNeighborhoodCharterSendSignatureRequest(WorldPackets::Neighborhood::NeighborhoodCharterSendSignatureRequest const& neighborhoodCharterSendSignatureRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_SEND_SIGNATURE_REQUEST TargetPlayerGuid: {}",
        neighborhoodCharterSendSignatureRequest.TargetPlayerGuid.ToString());

    // Validate target player is online and reachable
    Player* targetPlayer = ObjectAccessor::FindPlayer(neighborhoodCharterSendSignatureRequest.TargetPlayerGuid);
    if (!targetPlayer)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.Result = static_cast<uint16>(HOUSING_RESULT_PLAYER_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Send signature request notification to the target player's client
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    WorldPackets::Neighborhood::NeighborhoodCharterSignRequest signRequest;
    signRequest.CharterGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, charterId);
    signRequest.RequesterGuid = player->GetGUID();
    targetPlayer->SendDirectMessage(signRequest.Write());

    // Acknowledge to the requester that the signature request was sent
    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse ackResponse;
    ackResponse.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(ackResponse.Write());

    TC_LOG_DEBUG("housing", "Player {} requested signature from {} for charter {}",
        player->GetGUID().ToString(), targetPlayer->GetGUID().ToString(), charterId);
}

// ============================================================
// Neighborhood Management System
// ============================================================

void WorldSession::HandleNeighborhoodUpdateName(WorldPackets::Neighborhood::NeighborhoodUpdateName const& neighborhoodUpdateName)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_UPDATE_NAME NeighborhoodGuid: {}, NewName: {}",
        neighborhoodGuid.ToString(), neighborhoodUpdateName.NewName);

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can rename
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    // Validate name
    if (neighborhoodUpdateName.NewName.empty() || neighborhoodUpdateName.NewName.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Invalid name length");
        return;
    }

    neighborhood->SetName(neighborhoodUpdateName.NewName);

    // Broadcast name invalidation and update notification to ALL neighborhood members
    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            WorldPackets::Housing::InvalidateNeighborhoodName invalidate;
            invalidate.NeighborhoodGuid = neighborhoodGuid;
            memberPlayer->SendDirectMessage(invalidate.Write());

            WorldPackets::Neighborhood::NeighborhoodUpdateNameNotification nameNotification;
            nameNotification.NeighborhoodGuid = neighborhoodGuid;
            nameNotification.NewName = neighborhoodUpdateName.NewName;
            memberPlayer->SendDirectMessage(nameNotification.Write());
        }
    }

    WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.NeighborhoodGuid = neighborhoodGuid;
    SendPacket(response.Write());

    // Send guild rename notification if player is in a guild
    if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
    {
        WorldPackets::Housing::HousingSvcsGuildRenameNeighborhoodNotification guildNotification;
        guildNotification.NeighborhoodGuid = neighborhoodGuid;
        guildNotification.NewName = neighborhoodUpdateName.NewName;
        guild->BroadcastPacket(guildNotification.Write());
    }

    TC_LOG_DEBUG("housing", "Neighborhood {} renamed to '{}' by player {}",
        neighborhoodGuid.ToString(), neighborhoodUpdateName.NewName,
        player->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodSetPublicFlag(WorldPackets::Neighborhood::NeighborhoodSetPublicFlag const& neighborhoodSetPublicFlag)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_SET_PUBLIC_FLAGNeighborhoodGuid: {}, IsPublic: {}",
        neighborhoodSetPublicFlag.NeighborhoodGuid.ToString(), neighborhoodSetPublicFlag.IsPublic);

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodSetPublicFlag.NeighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodSetPublicFlag: Neighborhood {} not found",
            neighborhoodSetPublicFlag.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can change visibility
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodSetPublicFlag: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodSetPublicFlag.NeighborhoodGuid.ToString());
        return;
    }

    neighborhood->SetPublic(neighborhoodSetPublicFlag.IsPublic);

    WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

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

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_ADD_SECONDARY_OWNER NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodGuid.ToString(), neighborhoodAddSecondaryOwner.PlayerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodAddSecondaryOwner: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner can add managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodAddSecondaryOwner: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->AddManager(neighborhoodAddSecondaryOwner.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodGuid;
    response.PlayerGuid = neighborhoodAddSecondaryOwner.PlayerGuid;
    SendPacket(response.Write());

    // Broadcast roster update and refresh manager mirror data for all online members
    if (result == HOUSING_RESULT_SUCCESS)
    {
        for (auto const& member : neighborhood->GetMembers())
        {
            if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            {
                if (member.PlayerGuid != player->GetGUID())
                {
                    WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
                    rosterUpdate.Residents.push_back({ neighborhoodAddSecondaryOwner.PlayerGuid, uint16(member.PlotIndex) });
                    memberPlayer->SendDirectMessage(rosterUpdate.Write());
                }

                // Refresh manager mirror data on each online member's Account entity
                Battlenet::Account& memberAccount = memberPlayer->GetSession()->GetBattlenetAccount();
                memberAccount.ClearNeighborhoodMirrorManagers();
                for (auto const& m : neighborhood->GetMembers())
                {
                    if (m.Role == NEIGHBORHOOD_ROLE_MANAGER || m.Role == NEIGHBORHOOD_ROLE_OWNER)
                    {
                        ObjectGuid bnetGuid;
                        if (Player* mgr = ObjectAccessor::FindPlayer(m.PlayerGuid))
                            bnetGuid = mgr->GetSession()->GetBattlenetAccountGUID();
                        memberAccount.AddNeighborhoodMirrorManager(bnetGuid, m.PlayerGuid);
                    }
                }
            }
        }
    }

    TC_LOG_DEBUG("housing", "AddManager result: {} for player {} in neighborhood {}",
        uint32(result), neighborhoodAddSecondaryOwner.PlayerGuid.ToString(),
        neighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodRemoveSecondaryOwner(WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwner const& neighborhoodRemoveSecondaryOwner)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_REMOVE_SECONDARY_OWNER NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodGuid.ToString(), neighborhoodRemoveSecondaryOwner.PlayerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodRemoveSecondaryOwner: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner can remove managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodRemoveSecondaryOwner: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->RemoveManager(neighborhoodRemoveSecondaryOwner.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodGuid;
    response.PlayerGuid = neighborhoodRemoveSecondaryOwner.PlayerGuid;
    SendPacket(response.Write());

    // Broadcast roster update and refresh manager mirror data for all online members
    if (result == HOUSING_RESULT_SUCCESS)
    {
        for (auto const& member : neighborhood->GetMembers())
        {
            if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            {
                if (member.PlayerGuid != player->GetGUID())
                {
                    WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
                    rosterUpdate.Residents.push_back({ neighborhoodRemoveSecondaryOwner.PlayerGuid, uint16(member.PlotIndex) });
                    memberPlayer->SendDirectMessage(rosterUpdate.Write());
                }

                // Refresh manager mirror data on each online member's Account entity
                Battlenet::Account& memberAccount = memberPlayer->GetSession()->GetBattlenetAccount();
                memberAccount.ClearNeighborhoodMirrorManagers();
                for (auto const& m : neighborhood->GetMembers())
                {
                    if (m.Role == NEIGHBORHOOD_ROLE_MANAGER || m.Role == NEIGHBORHOOD_ROLE_OWNER)
                    {
                        ObjectGuid bnetGuid;
                        if (Player* mgr = ObjectAccessor::FindPlayer(m.PlayerGuid))
                            bnetGuid = mgr->GetSession()->GetBattlenetAccountGUID();
                        memberAccount.AddNeighborhoodMirrorManager(bnetGuid, m.PlayerGuid);
                    }
                }
            }
        }
    }

    TC_LOG_DEBUG("housing", "RemoveManager result: {} for player {} in neighborhood {}",
        uint32(result), neighborhoodRemoveSecondaryOwner.PlayerGuid.ToString(),
        neighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodInviteResident(WorldPackets::Neighborhood::NeighborhoodInviteResident const& neighborhoodInviteResident)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_INVITE_RESIDENT NeighborhoodGuid: {}, PlayerGuid: {}",
        neighborhoodGuid.ToString(), neighborhoodInviteResident.PlayerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodInviteResident: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can invite
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodInviteResident: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->InviteResident(player->GetGUID(), neighborhoodInviteResident.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodGuid;
    response.InviteeGuid = neighborhoodInviteResident.PlayerGuid;
    SendPacket(response.Write());

    // Notify the invitee that they received a neighborhood invite
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Player* invitee = ObjectAccessor::FindPlayer(neighborhoodInviteResident.PlayerGuid))
        {
            WorldPackets::Neighborhood::NeighborhoodInviteNotification notification;
            notification.NeighborhoodGuid = neighborhoodGuid;
            notification.InviterGuid = player->GetGUID();
            notification.NeighborhoodName = neighborhood->GetName();
            invitee->SendDirectMessage(notification.Write());
        }
    }

    TC_LOG_DEBUG("housing", "InviteResident result: {} for player {} in neighborhood {}",
        uint32(result), neighborhoodInviteResident.PlayerGuid.ToString(),
        neighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodCancelInvitation(WorldPackets::Neighborhood::NeighborhoodCancelInvitation const& neighborhoodCancelInvitation)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CANCEL_INVITATION NeighborhoodGuid: {}, InviteeGuid: {}",
        neighborhoodGuid.ToString(), neighborhoodCancelInvitation.InviteeGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCancelInvitation: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can cancel invitations
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCancelInvitation: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->CancelInvitation(neighborhoodCancelInvitation.InviteeGuid);

    WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodGuid;
    response.InviteeGuid = neighborhoodCancelInvitation.InviteeGuid;
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CancelInvitation result: {} for invitee {} in neighborhood {}",
        uint32(result), neighborhoodCancelInvitation.InviteeGuid.ToString(),
        neighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodPlayerDeclineInvite(WorldPackets::Neighborhood::NeighborhoodPlayerDeclineInvite const& neighborhoodPlayerDeclineInvite)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_PLAYER_DECLINE_INVITE NeighborhoodGuid: {}",
        neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodPlayerDeclineInvite.NeighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodDeclineInvitationResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodPlayerDeclineInvite: Neighborhood {} not found",
            neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->DeclineInvitation(player->GetGUID());

    WorldPackets::Neighborhood::NeighborhoodDeclineInvitationResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodPlayerDeclineInvite.NeighborhoodGuid;
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "DeclineInvitation result: {} for player {} in neighborhood {}",
        uint32(result), player->GetGUID().ToString(),
        neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodPlayerGetInvite(WorldPackets::Neighborhood::NeighborhoodPlayerGetInvite const& /*neighborhoodPlayerGetInvite*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_PLAYER_GET_INVITE for player {}",
        player->GetGUID().ToString());

    // Client sends empty packet — search all neighborhoods for a pending invite to this player
    Neighborhood* foundNeighborhood = sNeighborhoodMgr.FindNeighborhoodWithPendingInvite(player->GetGUID());
    Neighborhood::PendingInvite const* foundInvite = nullptr;

    if (foundNeighborhood)
    {
        for (auto const& invite : foundNeighborhood->GetPendingInvites())
        {
            if (invite.InviteeGuid == player->GetGUID())
            {
                foundInvite = &invite;
                break;
            }
        }
    }

    WorldPackets::Neighborhood::NeighborhoodPlayerGetInviteResponse response;
    if (foundNeighborhood && foundInvite)
    {
        response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
        response.NeighborhoodGuid = foundNeighborhood->GetGuid();
        response.InviterGuid = foundInvite->InviterGuid;
        response.InviteTime = foundInvite->InviteTime;
        response.NeighborhoodName = foundNeighborhood->GetName();
    }
    else
    {
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_ALLOWED);
    }
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Player {} {} a pending invite",
        player->GetGUID().ToString(), foundNeighborhood ? "has" : "does not have");
}

void WorldSession::HandleNeighborhoodGetInvites(WorldPackets::Neighborhood::NeighborhoodGetInvites const& /*neighborhoodGetInvites*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_GET_INVITES for player {}",
        player->GetGUID().ToString());

    // Client sends empty packet — derive neighborhood from player's housing context
    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetInvites: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can view all pending invites
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetInvites: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    std::vector<Neighborhood::PendingInvite> const& invites = neighborhood->GetPendingInvites();

    WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Invites.reserve(invites.size());
    for (auto const& invite : invites)
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse::InviteData data;
        data.InviteeGuid = invite.InviteeGuid;
        data.InviterGuid = invite.InviterGuid;
        data.InviteTime = invite.InviteTime;
        response.Invites.push_back(data);
    }
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Neighborhood {} has {} pending invites sent",
        neighborhoodGuid.ToString(), uint32(invites.size()));
}

void WorldSession::HandleNeighborhoodBuyHouse(WorldPackets::Neighborhood::NeighborhoodBuyHouse const& neighborhoodBuyHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_BUY_HOUSE NeighborhoodGuid: {}, PlotIndex: {}",
        neighborhoodBuyHouse.NeighborhoodGuid.ToString(), neighborhoodBuyHouse.PlotIndex);

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodBuyHouse.NeighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Neighborhood {} not found",
            neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    // Must be a member of the neighborhood
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_IN_NEIGHBORHOOD);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} is not a member of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    // Must not already own a house in this neighborhood
    if (player->GetHousingForNeighborhood(neighborhoodBuyHouse.NeighborhoodGuid))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_ALREADY_EXISTS);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} already has a house in neighborhood {}",
            player->GetGUID().ToString(), neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), neighborhoodBuyHouse.PlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        player->CreateHousing(neighborhoodBuyHouse.NeighborhoodGuid, neighborhoodBuyHouse.PlotIndex);

        // Update the PlotInfo with the newly created HouseGuid and Battle.net account GUID
        if (Housing const* housing = player->GetHousing())
        {
            neighborhood->UpdatePlotHouseInfo(neighborhoodBuyHouse.PlotIndex,
                housing->GetHouseGuid(), GetBattlenetAccountGUID());
        }

        // Grant "Acquire a house" kill credit for quest 91863 (objective 17)
        static constexpr uint32 NPC_KILL_CREDIT_BUY_HOME = 248858;
        player->KilledMonsterCredit(NPC_KILL_CREDIT_BUY_HOME);

        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
        response.NeighborhoodGuid = neighborhoodBuyHouse.NeighborhoodGuid;
        response.PlotIndex = neighborhoodBuyHouse.PlotIndex;
        if (Housing const* h = player->GetHousing())
            response.HouseGuid = h->GetHouseGuid();
        SendPacket(response.Write());

        // Broadcast roster update to other neighborhood members
        for (auto const& member : neighborhood->GetMembers())
        {
            if (member.PlayerGuid == player->GetGUID())
                continue;
            if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            {
                WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
                rosterUpdate.Residents.push_back({ player->GetGUID(), uint16(neighborhoodBuyHouse.PlotIndex) });
                memberPlayer->SendDirectMessage(rosterUpdate.Write());
            }
        }

        // Send guild notification for house addition
        if (Housing const* housing = player->GetHousing())
        {
            if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
            {
                WorldPackets::Housing::HousingSvcsGuildAddHouseNotification notification;
                notification.HouseGuid = housing->GetHouseGuid();
                guild->BroadcastPacket(notification.Write());
            }
        }

        // Swap the for-sale-sign GO to a cornerstone GO on the housing map
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
            std::vector<NeighborhoodPlotData const*> plotDataList = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
            for (NeighborhoodPlotData const* plotData : plotDataList)
            {
                if (plotData->PlotIndex == static_cast<int32>(neighborhoodBuyHouse.PlotIndex))
                {
                    if (uint32 cornerstoneEntry = static_cast<uint32>(plotData->CornerstoneGameObjectID))
                        housingMap->SwapPlotGameObject(neighborhoodBuyHouse.PlotIndex, cornerstoneEntry);
                    break;
                }
            }
        }

        TC_LOG_DEBUG("housing", "Player {} purchased plot {} in neighborhood {}",
            player->GetGUID().ToString(), neighborhoodBuyHouse.PlotIndex,
            neighborhoodBuyHouse.NeighborhoodGuid.ToString());

        // Check if neighborhoods need expansion after plot purchase
        sNeighborhoodMgr.CheckAndExpandNeighborhoods();
    }
    else
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint32>(result);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: PurchasePlot result: {} for player {}",
            uint32(result), player->GetGUID().ToString());
    }
}

void WorldSession::HandleNeighborhoodMoveHouse(WorldPackets::Neighborhood::NeighborhoodMoveHouse const& neighborhoodMoveHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_MOVE_HOUSE NeighborhoodGuid: {}, PlotGuid: {}",
        neighborhoodMoveHouse.NeighborhoodGuid.ToString(), neighborhoodMoveHouse.PlotGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodMoveHouse.NeighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Neighborhood {} not found",
            neighborhoodMoveHouse.NeighborhoodGuid.ToString());
        return;
    }

    // Resolve the target plot index from the PlotGuid
    uint8 targetPlotIndex = INVALID_PLOT_INDEX;
    for (auto const& plot : neighborhood->GetPlots())
    {
        if (!plot.IsOccupied())
            continue;
        if (plot.PlotGuid == neighborhoodMoveHouse.PlotGuid)
        {
            targetPlotIndex = plot.PlotIndex;
            break;
        }
    }

    HousingResult result = neighborhood->MoveHouse(player->GetGUID(), targetPlotIndex);

    WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodMoveHouse.NeighborhoodGuid;
    response.NewPlotIndex = targetPlotIndex;
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "MoveHouse result: {} to plot {} in neighborhood {}",
        uint32(result), neighborhoodMoveHouse.PlotGuid.ToString(),
        neighborhoodMoveHouse.NeighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodOpenCornerstoneUI(WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUI const& neighborhoodOpenCornerstoneUI)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI PlotIndex: {}, NeighborhoodGuid: {}",
        neighborhoodOpenCornerstoneUI.PlotIndex, neighborhoodOpenCornerstoneUI.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodOpenCornerstoneUI.NeighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodOpenCornerstoneUI: Neighborhood {} not found",
            neighborhoodOpenCornerstoneUI.NeighborhoodGuid.ToString());
        return;
    }

    // Look up cost from plot data using the provided PlotIndex
    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    uint64 plotCost = 0;
    bool plotFound = false;

    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == neighborhoodOpenCornerstoneUI.PlotIndex)
        {
            plotCost = plot->Cost;
            plotFound = true;
            break;
        }
    }

    if (!plotFound)
    {
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_RPC_ERROR);
        SendPacket(response.Write());

        TC_LOG_ERROR("housing", "HandleNeighborhoodOpenCornerstoneUI: PlotIndex {} not found in neighborhood map {}",
            neighborhoodOpenCornerstoneUI.PlotIndex, neighborhoodMapId);
        return;
    }

    WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.NeighborhoodGuid = neighborhoodOpenCornerstoneUI.NeighborhoodGuid;
    response.PlotIndex = static_cast<uint8>(neighborhoodOpenCornerstoneUI.PlotIndex);
    response.Cost = plotCost;
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "HandleNeighborhoodOpenCornerstoneUI: Player {} opened cornerstone UI for plot {} (cost: {}) in neighborhood '{}'",
        player->GetGUID().ToString(), neighborhoodOpenCornerstoneUI.PlotIndex, plotCost, neighborhood->GetName());
}

void WorldSession::HandleNeighborhoodOfferOwnership(WorldPackets::Neighborhood::NeighborhoodOfferOwnership const& neighborhoodOfferOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP NeighborhoodGuid: {}, NewOwnerGuid: {}",
        neighborhoodGuid.ToString(), neighborhoodOfferOwnership.NewOwnerGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodOfferOwnership: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner can transfer ownership
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodOfferOwnership: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->TransferOwnership(neighborhoodOfferOwnership.NewOwnerGuid);

    WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodGuid;
    response.NewOwnerGuid = neighborhoodOfferOwnership.NewOwnerGuid;
    SendPacket(response.Write());

    // Notify the new owner and broadcast to all members
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Player* newOwner = ObjectAccessor::FindPlayer(neighborhoodOfferOwnership.NewOwnerGuid))
        {
            WorldPackets::Housing::HousingSvcsNeighborhoodOwnershipTransferredResponse transferNotification;
            transferNotification.NeighborhoodGuid = neighborhoodGuid;
            transferNotification.NewOwnerGuid = neighborhoodOfferOwnership.NewOwnerGuid;
            newOwner->SendDirectMessage(transferNotification.Write());
        }

        // Broadcast roster update to all members (role changes)
        for (auto const& member : neighborhood->GetMembers())
        {
            if (member.PlayerGuid == player->GetGUID())
                continue;
            if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            {
                WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
                rosterUpdate.Residents.push_back({ neighborhoodOfferOwnership.NewOwnerGuid, uint16(member.PlotIndex) });
                memberPlayer->SendDirectMessage(rosterUpdate.Write());
            }
        }
    }

    TC_LOG_DEBUG("housing", "TransferOwnership result: {} to player {} for neighborhood {}",
        uint32(result), neighborhoodOfferOwnership.NewOwnerGuid.ToString(),
        neighborhoodGuid.ToString());
}

void WorldSession::HandleNeighborhoodGetRoster(WorldPackets::Neighborhood::NeighborhoodGetRoster const& neighborhoodGetRoster)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_GET_ROSTER NeighborhoodGuid: {}",
        neighborhoodGetRoster.NeighborhoodGuid.ToString());

    // Try the GUID from the packet first; fall back to the player's current housing map neighborhood
    ObjectGuid neighborhoodGuid = neighborhoodGetRoster.NeighborhoodGuid;
    if (neighborhoodGuid.IsEmpty())
    {
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            if (Neighborhood* mapNeighborhood = housingMap->GetNeighborhood())
                neighborhoodGuid = mapNeighborhood->GetGuid();

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Client sent empty NeighborhoodGuid, resolved to {} from housing map",
            neighborhoodGuid.ToString());
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Neighborhood {} not found",
            neighborhoodGetRoster.NeighborhoodGuid.ToString());
        return;
    }

    // Must be a member to view roster
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NOT_IN_NEIGHBORHOOD);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Player {} is not a member of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGetRoster.NeighborhoodGuid.ToString());
        return;
    }

    std::vector<Neighborhood::Member> const& members = neighborhood->GetMembers();

    WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    response.Members.reserve(members.size());
    for (auto const& member : members)
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse::RosterMemberData data;
        data.PlayerGuid = member.PlayerGuid;
        data.Role = member.Role;
        data.PlotIndex = member.PlotIndex;
        data.JoinTime = member.JoinTime;
        if (member.PlotIndex != INVALID_PLOT_INDEX)
            if (Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(member.PlotIndex))
                data.HouseGuid = plotInfo->HouseGuid;
        response.Members.push_back(data);
    }
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "Neighborhood {} roster: {} members sent",
        neighborhoodGetRoster.NeighborhoodGuid.ToString(), uint32(members.size()));
}

void WorldSession::HandleNeighborhoodEvictPlot(WorldPackets::Neighborhood::NeighborhoodEvictPlot const& neighborhoodEvictPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_EVICT_PLOT PlotIndex: {}, NeighborhoodGuid: {}",
        neighborhoodEvictPlot.PlotIndex, neighborhoodEvictPlot.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodEvictPlot.NeighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodEvictPlot: Neighborhood {} not found",
            neighborhoodEvictPlot.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can evict
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodEvictPlot: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodEvictPlot.NeighborhoodGuid.ToString());
        return;
    }

    // Find the plot by index — O(1) direct array access
    ObjectGuid evictedPlayerGuid;
    ObjectGuid plotGuid;
    if (Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(neighborhoodEvictPlot.PlotIndex))
    {
        evictedPlayerGuid = plotInfo->OwnerGuid;
        plotGuid = plotInfo->PlotGuid;
    }

    HousingResult result = neighborhood->EvictPlayer(evictedPlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
    response.Result = static_cast<uint32>(result);
    response.NeighborhoodGuid = neighborhoodEvictPlot.NeighborhoodGuid;
    response.PlotGuid = plotGuid;
    SendPacket(response.Write());

    // Send eviction notice to the evicted player and broadcast roster update
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Swap the cornerstone GO back to a for-sale-sign
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
            std::vector<NeighborhoodPlotData const*> plotDataList = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
            for (NeighborhoodPlotData const* plotData : plotDataList)
            {
                if (plotData->PlotIndex == static_cast<int32>(neighborhoodEvictPlot.PlotIndex))
                {
                    if (uint32 forSaleEntry = static_cast<uint32>(plotData->PlotGameObjectID))
                        housingMap->SwapPlotGameObject(neighborhoodEvictPlot.PlotIndex, forSaleEntry);
                    break;
                }
            }
        }

        if (!evictedPlayerGuid.IsEmpty())
        {
            if (Player* evictedPlayer = ObjectAccessor::FindPlayer(evictedPlayerGuid))
            {
                WorldPackets::Neighborhood::NeighborhoodEvictPlotNotice notice;
                notice.NeighborhoodGuid = neighborhoodEvictPlot.NeighborhoodGuid;
                notice.PlotGuid = plotGuid;
                evictedPlayer->SendDirectMessage(notice.Write());
            }
        }

        // Broadcast roster update to remaining members
        for (auto const& member : neighborhood->GetMembers())
        {
            if (member.PlayerGuid == player->GetGUID())
                continue;
            if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            {
                WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
                rosterUpdate.Residents.push_back({ evictedPlayerGuid, uint16(0) }); // evicted player no longer has a plot
                memberPlayer->SendDirectMessage(rosterUpdate.Write());
            }
        }
    }

    TC_LOG_DEBUG("housing", "EvictPlayer result: {} for plot index {} in neighborhood {}",
        uint32(result), neighborhoodEvictPlot.PlotIndex,
        neighborhoodEvictPlot.NeighborhoodGuid.ToString());
}

// ============================================================
// Neighborhood Initiative System
// ============================================================

void WorldSession::HandleNeighborhoodInitiativeServiceStatusCheck(WorldPackets::Neighborhood::NeighborhoodInitiativeServiceStatusCheck const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_SERVICE_STATUS_CHECK for player {}",
        player->GetGUID().ToString());

    // Respond with initiative service status (service is available)
    WorldPackets::Housing::InitiativeServiceStatus response;
    response.ServiceEnabled = true;
    SendPacket(response.Write());
}

void WorldSession::HandleGetAvailableInitiativeRequest(WorldPackets::Neighborhood::GetAvailableInitiativeRequest const& getAvailableInitiativeRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_GET_AVAILABLE_INITIATIVE_REQUEST NeighborhoodGuid: {}",
        getAvailableInitiativeRequest.NeighborhoodGuid.ToString());

    // Respond with empty initiative list for now
    WorldPackets::Housing::GetPlayerInitiativeInfoResult response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());
}

void WorldSession::HandleGetInitiativeActivityLogRequest(WorldPackets::Neighborhood::GetInitiativeActivityLogRequest const& getInitiativeActivityLogRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST NeighborhoodGuid: {}",
        getInitiativeActivityLogRequest.NeighborhoodGuid.ToString());

    // Respond with empty activity log
    WorldPackets::Housing::GetInitiativeActivityLogResult response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());
}
