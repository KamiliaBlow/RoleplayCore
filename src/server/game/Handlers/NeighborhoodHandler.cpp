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
#include "World.h"

namespace
{
    std::string HexDumpPacket(WorldPacket const* packet, size_t maxBytes = 128)
    {
        if (!packet || packet->size() == 0)
            return "(empty)";
        size_t len = std::min(packet->size(), maxBytes);
        std::string result;
        result.reserve(len * 3 + 32);
        uint8 const* raw = packet->data();
        for (size_t i = 0; i < len; ++i)
        {
            if (i > 0 && i % 32 == 0)
                result += "\n  ";
            else if (i > 0)
                result += ' ';
            result += fmt::format("{:02X}", raw[i]);
        }
        if (len < packet->size())
            result += fmt::format(" ...({} more)", packet->size() - len);
        return result;
    }

    std::string GuidHex(ObjectGuid const& guid)
    {
        return fmt::format("lo={:016X} hi={:016X}", guid.GetRawValue(0), guid.GetRawValue(1));
    }
}

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
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
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
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
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
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_DB_ERROR);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterFinalize: Failed to load charter {} from DB",
            charterId);
        return;
    }

    // Verify enough signatures
    if (!charter.HasEnoughSignatures())
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_MORE_SIGNATURES_NEEDED);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "Player {} finalized charter '{}', created neighborhood {}",
            player->GetGUID().ToString(), charter.GetName(),
            neighborhood->GetGuid().ToString());
    }
    else
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_DB_ERROR);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_DB_ERROR);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterAddSignature: Failed to load charter {} from DB",
            charterId);
        return;
    }

    // Add player's signature (validates not already signed, not creator)
    if (!charter.AddSignature(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterAddSignature: Player {} could not sign charter {}",
            player->GetGUID().ToString(), charterId);
        return;
    }

    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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
    ackResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can rename
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    // Validate name
    if (neighborhoodUpdateName.NewName.empty() || neighborhoodUpdateName.NewName.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
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
            nameNotification.NewName = neighborhoodUpdateName.NewName;
            memberPlayer->SendDirectMessage(nameNotification.Write());
        }
    }

    WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodSetPublicFlag.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodSetPublicFlag: Neighborhood {} not found",
            neighborhoodSetPublicFlag.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can change visibility
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodSetPublicFlag: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodSetPublicFlag.NeighborhoodGuid.ToString());
        return;
    }

    neighborhood->SetPublic(neighborhoodSetPublicFlag.IsPublic);

    WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodAddSecondaryOwner: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner can add managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodAddSecondaryOwner: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->AddManager(neighborhoodAddSecondaryOwner.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
    response.PlayerGuid = neighborhoodAddSecondaryOwner.PlayerGuid;
    response.Result = static_cast<uint8>(result);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodRemoveSecondaryOwner: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner can remove managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodRemoveSecondaryOwner: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->RemoveManager(neighborhoodRemoveSecondaryOwner.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
    response.PlayerGuid = neighborhoodRemoveSecondaryOwner.PlayerGuid;
    response.Result = static_cast<uint8>(result);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodInviteResident: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can invite
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodInviteResident: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->InviteResident(player->GetGUID(), neighborhoodInviteResident.PlayerGuid);

    WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
    response.Result = static_cast<uint8>(result);
    response.InviteeGuid = neighborhoodInviteResident.PlayerGuid;
    SendPacket(response.Write());

    // Notify the invitee that they received a neighborhood invite
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Player* invitee = ObjectAccessor::FindPlayer(neighborhoodInviteResident.PlayerGuid))
        {
            WorldPackets::Neighborhood::NeighborhoodInviteNotification notification;
            notification.NeighborhoodGuid = neighborhoodGuid;
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCancelInvitation: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can cancel invitations
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCancelInvitation: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->CancelInvitation(neighborhoodCancelInvitation.InviteeGuid);

    WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
    response.Result = static_cast<uint8>(result);
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

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodPlayerDeclineInvite.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodDeclineInvitationResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodPlayerDeclineInvite: Neighborhood {} not found",
            neighborhoodPlayerDeclineInvite.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->DeclineInvitation(player->GetGUID());

    WorldPackets::Neighborhood::NeighborhoodDeclineInvitationResponse response;
    response.Result = static_cast<uint8>(result);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        response.Entry.Timestamp = foundInvite->InviteTime;
        response.Entry.PlayerGuid = foundInvite->InviterGuid;
        response.Entry.HouseGuid = foundNeighborhood->GetGuid();
    }
    else
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetInvites: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can view all pending invites
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetInvites: Player {} lacks permission for neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    std::vector<Neighborhood::PendingInvite> const& invites = neighborhood->GetPendingInvites();

    WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Invites.reserve(invites.size());
    for (auto const& invite : invites)
    {
        WorldPackets::Housing::JamNeighborhoodRosterEntry entry;
        entry.Timestamp = invite.InviteTime;
        entry.PlayerGuid = invite.InviteeGuid;
        entry.HouseGuid = ObjectGuid::Empty; // invitees don't have houses yet
        response.Invites.push_back(entry);
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

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_BUY_HOUSE NeighborhoodGuid: {}, RawPlotIndex: {}, PlotGuid: {}",
        neighborhoodBuyHouse.NeighborhoodGuid.ToString(), neighborhoodBuyHouse.PlotIndex,
        neighborhoodBuyHouse.PlotGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodBuyHouse.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Neighborhood {} not found",
            neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    // Resolve the actual DB2 PlotIndex from the cornerstone GO entry.
    // The client may send a different value in the PlotIndex field (array index vs DB2 PlotIndex),
    // so we always resolve from the GO GUID which is reliable.
    uint32 cornerstoneGoEntry = neighborhoodBuyHouse.NeighborhoodGuid.GetEntry();
    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    NeighborhoodPlotData const* plotData = sHousingMgr.GetPlotByCornerstoneEntry(neighborhoodMapId, cornerstoneGoEntry);
    if (!plotData)
    {
        TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: No plot found for CornerstoneGO entry {} on map {}",
            cornerstoneGoEntry, neighborhoodMapId);
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    uint8 resolvedPlotIndex = static_cast<uint8>(plotData->PlotIndex);
    TC_LOG_INFO("housing", "HandleNeighborhoodBuyHouse: Resolved CornerstoneGO {} -> PlotIndex {} (raw was {})",
        cornerstoneGoEntry, resolvedPlotIndex, neighborhoodBuyHouse.PlotIndex);

    // Auto-join neighborhood if not already a member — buying a plot implies joining
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        HousingResult joinResult = neighborhood->AddResident(player->GetGUID());
        if (joinResult != HOUSING_RESULT_SUCCESS)
        {
            WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
            response.Result = static_cast<uint8>(joinResult);
            SendPacket(response.Write());

            TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Failed to add player {} to neighborhood {} (result {})",
                player->GetGUID().ToString(), neighborhoodBuyHouse.NeighborhoodGuid.ToString(), static_cast<uint32>(joinResult));
            return;
        }
        TC_LOG_INFO("housing", "HandleNeighborhoodBuyHouse: Auto-added player {} as resident of neighborhood '{}'",
            player->GetGUID().ToString(), neighborhood->GetName());
    }

    // Must not already own a house in this neighborhood
    if (player->GetHousingForNeighborhood(neighborhoodBuyHouse.NeighborhoodGuid))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_HOUSE);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} already has a house in neighborhood {}",
            player->GetGUID().ToString(), neighborhoodBuyHouse.NeighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), resolvedPlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Use the server's canonical neighborhood GUID, NOT the client-supplied GUID.
        // Client may send DB2 NeighborhoodID as counter while server uses internal counter.
        player->CreateHousing(neighborhood->GetGuid(), resolvedPlotIndex);

        // Update the PlotInfo with the newly created HouseGuid and Battle.net account GUID
        if (Housing const* housing = player->GetHousing())
        {
            neighborhood->UpdatePlotHouseInfo(resolvedPlotIndex,
                housing->GetHouseGuid(), GetBattlenetAccountGUID());
        }

        // Grant "Acquire a house" kill credit for quest 91863 (objective 17)
        static constexpr uint32 NPC_KILL_CREDIT_BUY_HOME = 248858;
        player->KilledMonsterCredit(NPC_KILL_CREDIT_BUY_HOME);

        // Retail sequence: FirstTimeDecorAcquisition → BuyHouseResponse → LevelFavor updates

        // 1. Send starter decor acquisition notifications (sniff: 7-8 packets before buy response)
        std::vector<uint32> starterDecorIds = sHousingMgr.GetStarterDecorIds();
        for (uint32 decorId : starterDecorIds)
        {
            WorldPackets::Housing::HousingFirstTimeDecorAcquisition decorAcq;
            decorAcq.DecorEntryID = decorId;
            SendPacket(decorAcq.Write());
        }
        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Sent {} FirstTimeDecorAcquisition packets",
            uint32(starterDecorIds.size()));

        // 2. Build buy response with JamCurrentHouseInfo matching retail sniff format
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        if (Housing const* h = player->GetHousing())
        {
            // Sniff-verified: SecondaryOwnerGuid=NeighborhoodGUID (context for edit mode), PlotGuid=PlotGUID
            response.HouseInfo.OwnerGuid = h->GetHouseGuid();
            response.HouseInfo.SecondaryOwnerGuid = neighborhood->GetGuid();
            response.HouseInfo.PlotGuid = h->GetPlotGuid();
            response.HouseInfo.Flags = resolvedPlotIndex;
            response.HouseInfo.HouseTypeId = 32; // sniff value: 0x20
            response.HouseInfo.StatusFlags = 0;
        }
        SendPacket(response.Write());

        // 3. Send level/favor updates (sniff: always 2 packets after buy response)
        if (Housing const* h = player->GetHousing())
        {
            // Packet 1: Initial level assignment (prev=-1/-1, new=level 1, next level cost=910)
            {
                WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelFavor;
                levelFavor.Type = 0;
                levelFavor.PreviousFavor = -1;
                levelFavor.PreviousLevel = -1;
                levelFavor.NewLevel = 1;
                levelFavor.Field4 = 0;
                levelFavor.HouseGuid = h->GetHouseGuid();
                levelFavor.PreviousLevelId = -1;
                levelFavor.NextLevelFavorCost = 910; // sniff value: 0x038E
                levelFavor.Flags = 0x8000;
                SendPacket(levelFavor.Write());
            }
            // Packet 2: Favor state (prev favor=910, level=1, no next level info)
            {
                WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelFavor;
                levelFavor.Type = 0;
                levelFavor.PreviousFavor = 910;
                levelFavor.PreviousLevel = 1;
                levelFavor.NewLevel = 1;
                levelFavor.Field4 = 0;
                levelFavor.HouseGuid = h->GetHouseGuid();
                levelFavor.PreviousLevelId = -1;
                levelFavor.NextLevelFavorCost = -1;
                levelFavor.Flags = 0x8000;
                SendPacket(levelFavor.Write());
            }
        }

        // Broadcast roster update to other neighborhood members
        for (auto const& member : neighborhood->GetMembers())
        {
            if (member.PlayerGuid == player->GetGUID())
                continue;
            if (Player* memberPlayer = ObjectAccessor::FindPlayer(member.PlayerGuid))
            {
                WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
                rosterUpdate.Residents.push_back({ player->GetGUID(), uint16(resolvedPlotIndex) });
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

        // Mark the plot Cornerstone as owned (GOState 1 = READY)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->SetPlotOwnershipState(resolvedPlotIndex, true);

            // Spawn the house structure GO at DB2 default position
            housingMap->SpawnHouseForPlot(resolvedPlotIndex);
        }

        // Notify client that the basic house was created
        if (Housing const* h = player->GetHousing())
        {
            WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse houseResponse;
            houseResponse.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
            houseResponse.HouseGuid = h->GetHouseGuid();
            SendPacket(houseResponse.Write());
        }

        TC_LOG_DEBUG("housing", "Player {} purchased plot {} in neighborhood '{}'",
            player->GetGUID().ToString(), resolvedPlotIndex,
            neighborhood->GetName());

        // Check if neighborhoods need expansion after plot purchase
        sNeighborhoodMgr.CheckAndExpandNeighborhoods();
    }
    else
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(result);
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

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodMoveHouse.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
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
    response.Result = static_cast<uint8>(result);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        response.HouseInfo.OwnerGuid = player->GetGUID();
        response.HouseInfo.PlotGuid = neighborhoodMoveHouse.PlotGuid;
        if (Housing const* housing = player->GetHousing())
            response.HouseInfo.HouseTypeId = 0; // default house type
    }
    response.MoveTransactionGuid = ObjectGuid::Empty; // transaction tracking GUID
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

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodOpenCornerstoneUI.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodOpenCornerstoneUI: Neighborhood {} not found",
            neighborhoodOpenCornerstoneUI.NeighborhoodGuid.ToString());
        // No result code in this packet — just send with PurchaseStatus=0, client won't show buy button
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.PlotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
        SendPacket(response.Write());
        return;
    }

    // Look up cost from plot data using the provided PlotIndex
    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    uint64 plotCost = 0;
    bool plotFound = false;

    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == static_cast<int32>(neighborhoodOpenCornerstoneUI.PlotIndex))
        {
            plotCost = plot->Cost;
            plotFound = true;
            break;
        }
    }

    if (!plotFound)
    {
        TC_LOG_ERROR("housing", "HandleNeighborhoodOpenCornerstoneUI: PlotIndex {} not found in neighborhood map {}",
            neighborhoodOpenCornerstoneUI.PlotIndex, neighborhoodMapId);
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.PlotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
        response.NeighborhoodName = neighborhood->GetName();
        SendPacket(response.Write());
        return;
    }

    // Pre-send neighborhood name response to populate the JamCliNeighborhoodName
    // DataCache. Flag +574 in the display function checks whether the TLS
    // NeighborhoodGuid is resolved in the DataCache. Sending this immediately
    // before the cornerstone response ensures the cache entry exists.
    {
        WorldPackets::Housing::QueryNeighborhoodNameResponse nameResp;
        nameResp.NeighborhoodGuid = neighborhood->GetGuid();
        nameResp.Allow = true;
        nameResp.Name = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    // Build cornerstone UI response — wire format verified against retail 12.0.1 build 65940
    WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
    response.PlotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
    // PlotOwnerGuid (→Buffer+40): Controls flag +573. Empty triggers zero-check path
    // which clears +573. Non-Housing GUIDs (Player type 2) succeed entity name lookup.
    response.PlotOwnerGuid = ObjectGuid::Empty;
    // NeighborhoodGuid (→Buffer+56): Controls flag +574 via JamCliNeighborhoodName DataCache.
    // Client looks up neighborhood name in the cache using this GUID as the key.
    // Must match the GUID in the pre-sent QueryNeighborhoodNameResponse above,
    // otherwise the cache lookup fails and the name shows as <?>.
    response.NeighborhoodGuid = neighborhood->GetGuid();
    response.Cost = plotCost;
    // PurchaseStatus (→Buffer+80): HousingResult enum value
    // Value 73 = PlotReserved error code, which shows "Plot already reserved"
    // Value 0 = Success (no error), allows the purchase UI to render normally
    response.PurchaseStatus = 0;
    response.CornerstoneGuid = ObjectGuid::Empty;
    // CanPurchase bit is 0 in ALL retail packets (both owned and unclaimed purchasable)
    // The actual "can buy" signal is PurchaseStatus==73. Setting CanPurchase=true
    // causes client to show "Plot already reserved" message.
    response.CanPurchase = false;
    response.NeighborhoodName = neighborhood->GetName();
    WorldPacket const* pkt = response.Write();
    SendPacket(pkt);

    TC_LOG_ERROR("housing", "=== SMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI_RESPONSE (0x5C000A) ===\n"
        "  PlotIndex={}, Cost={}, PurchaseStatus={}, CanPurchase={}, IsPlotOwned={}\n"
        "  PlotOwnerGuid: {} ({})\n"
        "  NeighborhoodGuid: {} ({})\n"
        "  CornerstoneGuid: {} ({})\n"
        "  NeighborhoodName='{}' (len={})\n"
        "  Packet size={} bytes, hex:\n  {}",
        response.PlotIndex, response.Cost, uint32(response.PurchaseStatus), response.CanPurchase, response.IsPlotOwned,
        response.PlotOwnerGuid.ToString(), GuidHex(response.PlotOwnerGuid),
        response.NeighborhoodGuid.ToString(), GuidHex(response.NeighborhoodGuid),
        response.CornerstoneGuid.ToString(), GuidHex(response.CornerstoneGuid),
        response.NeighborhoodName, response.NeighborhoodName.size(),
        pkt->size(), HexDumpPacket(pkt));
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodOfferOwnership: Neighborhood {} not found",
            neighborhoodGuid.ToString());
        return;
    }

    // Only owner can transfer ownership
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodOfferOwnership: Player {} is not owner of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGuid.ToString());
        return;
    }

    HousingResult result = neighborhood->TransferOwnership(neighborhoodOfferOwnership.NewOwnerGuid);

    WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
    response.Result = static_cast<uint8>(result);
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

    // ResolveNeighborhood handles both Housing GUIDs and bulletin board GO GUIDs
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodGetRoster.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Neighborhood {} not found",
            neighborhoodGetRoster.NeighborhoodGuid.ToString());
        return;
    }

    // Must be a member to view roster
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: Player {} is not a member of neighborhood {}",
            player->GetGUID().ToString(), neighborhoodGetRoster.NeighborhoodGuid.ToString());
        return;
    }

    std::vector<Neighborhood::Member> const& members = neighborhood->GetMembers();

    WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.GroupNeighborhoodGuid = neighborhood->GetGuid();
    response.GroupOwnerGuid = neighborhood->GetOwnerGuid();
    response.NeighborhoodName = neighborhood->GetName();
    response.Members.reserve(members.size());
    for (auto const& member : members)
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse::RosterMemberData data;
        data.PlayerGuid = member.PlayerGuid;
        data.PlotIndex = member.PlotIndex;
        data.JoinTime = member.JoinTime;
        data.IsOnline = ObjectAccessor::FindPlayer(member.PlayerGuid) != nullptr;
        if (member.PlotIndex != INVALID_PLOT_INDEX)
            if (Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(member.PlotIndex))
                data.HouseGuid = plotInfo->HouseGuid;
        response.Members.push_back(data);
    }

    // Pre-send neighborhood name response to populate JamCliNeighborhoodName DataCache.
    // The roster UI resolves the neighborhood name via GroupNeighborhoodGuid cache lookup.
    {
        WorldPackets::Housing::QueryNeighborhoodNameResponse nameResp;
        nameResp.NeighborhoodGuid = neighborhood->GetGuid();
        nameResp.Allow = true;
        nameResp.Name = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    WorldPacket const* rosterPkt = response.Write();
    SendPacket(rosterPkt);

    TC_LOG_ERROR("housing", "=== SMSG_NEIGHBORHOOD_GET_ROSTER_RESPONSE (0x5C0012) [handler] ===\n"
        "  Result={}, Members={}, NeighborhoodName='{}'\n"
        "  GroupNeighborhoodGuid: {} ({})\n"
        "  GroupOwnerGuid: {} ({})\n"
        "  Packet size={} bytes, hex:\n  {}",
        uint32(response.Result), response.Members.size(), response.NeighborhoodName,
        response.GroupNeighborhoodGuid.ToString(), GuidHex(response.GroupNeighborhoodGuid),
        response.GroupOwnerGuid.ToString(), GuidHex(response.GroupOwnerGuid),
        rosterPkt->size(), HexDumpPacket(rosterPkt, 256));
}

void WorldSession::HandleNeighborhoodEvictPlot(WorldPackets::Neighborhood::NeighborhoodEvictPlot const& neighborhoodEvictPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_EVICT_PLOT PlotIndex: {}, NeighborhoodGuid: {}",
        neighborhoodEvictPlot.PlotIndex, neighborhoodEvictPlot.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodEvictPlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodEvictPlot: Neighborhood {} not found",
            neighborhoodEvictPlot.NeighborhoodGuid.ToString());
        return;
    }

    // Only owner or manager can evict
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
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
    response.Result = static_cast<uint8>(result);
    response.NeighborhoodGuid = neighborhoodEvictPlot.NeighborhoodGuid;
    SendPacket(response.Write());

    // Send eviction notice to the evicted player and broadcast roster update
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Mark the plot Cornerstone as unowned/for-sale (GOState 0 = ACTIVE)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->SetPlotOwnershipState(neighborhoodEvictPlot.PlotIndex, false);
            housingMap->DespawnHouseForPlot(neighborhoodEvictPlot.PlotIndex);
            housingMap->DespawnAllDecorForPlot(neighborhoodEvictPlot.PlotIndex);
        }

        if (!evictedPlayerGuid.IsEmpty())
        {
            if (Player* evictedPlayer = ObjectAccessor::FindPlayer(evictedPlayerGuid))
            {
                WorldPackets::Neighborhood::NeighborhoodEvictPlotNotice notice;
                notice.PlotId = neighborhoodEvictPlot.PlotIndex;
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

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(getAvailableInitiativeRequest.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::GetPlayerInitiativeInfoResult response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Initiative info is driven by DB2 data (NeighborhoodInitiative.db2) which the client
    // already has. This response confirms the server acknowledges the initiative query.
    WorldPackets::Housing::GetPlayerInitiativeInfoResult response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_GET_AVAILABLE_INITIATIVE_REQUEST NeighborhoodGuid: {}, Player: {}",
        getAvailableInitiativeRequest.NeighborhoodGuid.ToString(), player->GetGUID().ToString());
}

void WorldSession::HandleGetInitiativeActivityLogRequest(WorldPackets::Neighborhood::GetInitiativeActivityLogRequest const& getInitiativeActivityLogRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(getInitiativeActivityLogRequest.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::GetInitiativeActivityLogResult response;
        response.Result = static_cast<uint32>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Activity log entries are tracked per-neighborhood. The response confirms the query
    // and the client uses cached initiative data from DB2 to display the log.
    WorldPackets::Housing::GetInitiativeActivityLogResult response;
    response.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST NeighborhoodGuid: {}, Player: {}",
        getInitiativeActivityLogRequest.NeighborhoodGuid.ToString(), player->GetGUID().ToString());
}
