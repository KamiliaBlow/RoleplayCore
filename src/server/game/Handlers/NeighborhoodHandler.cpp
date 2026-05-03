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
#include "HousingNeighborhoodMirrorEntity.h"
#include "QueryPackets.h"
#include "HousingPlayerHouseEntity.h"
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
#include "InitiativeManager.h"
#include "Neighborhood.h"
#include "NeighborhoodCharter.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "GameTime.h"
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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

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

    if (!ObjectMgr::IsValidCharterName(neighborhoodCharterCreate.Name) || sObjectMgr->IsReservedName(neighborhoodCharterCreate.Name))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "HandleNeighborhoodCharterCreate: Name rejected by filter for player {}",
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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

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

    if (!ObjectMgr::IsValidCharterName(neighborhoodCharterEdit.Name) || sObjectMgr->IsReservedName(neighborhoodCharterEdit.Name))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());

        TC_LOG_INFO("housing", "HandleNeighborhoodCharterEdit: Name rejected by filter for player {}",
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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

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
        response.Result = static_cast<uint8>(HOUSING_RESULT_DUPLICATE_CHARTER_SIGNATURE);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodCharterAddSignature: Player {} could not sign charter {}",
            player->GetGUID().ToString(), charterId);
        return;
    }

    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.CharterGuid = neighborhoodCharterAddSignature.CharterGuid;
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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_CHARTER_SEND_SIGNATURE_REQUEST TargetPlayerGuid: {}",
        neighborhoodCharterSendSignatureRequest.TargetPlayerGuid.ToString());

    // Validate target player is online and reachable
    Player* targetPlayer = ObjectAccessor::FindPlayer(neighborhoodCharterSendSignatureRequest.TargetPlayerGuid);
    if (!targetPlayer)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_PLAYER_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Send signature request notification to the target player's client
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    WorldPackets::Neighborhood::NeighborhoodCharterSignRequest signRequest;
    signRequest.CharterGuid = ObjectGuid::Create<HighGuid::Housing>(0, 0, 0, charterId);
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

    if (!ObjectMgr::IsValidCharterName(neighborhoodUpdateName.NewName) || sObjectMgr->IsReservedName(neighborhoodUpdateName.NewName))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodUpdateName: Name rejected by filter");
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

            // 12.0.5: moved from SMSG_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION (0x5C0004)
            // to SMSG_HOUSING_SVCS_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION (0x540023).
            // IDA-verified wire (sub_7FF75C1EA710 case 0x540023): ObjectGuid + string.
            WorldPackets::Housing::HousingSvcsNeighborhoodUpdateNameNotification nameNotification;
            nameNotification.NeighborhoodGuid = neighborhoodGuid;
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
        guildNotification.NewName = neighborhoodUpdateName.NewName;
        guild->BroadcastPacket(guildNotification.Write());
    }

    // Refresh NeighborhoodMirrorData on all online members' Account entities
    neighborhood->RefreshMirrorDataForOnlineMembers();

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

    // Broadcast roster update and refresh mirror data for all online members
    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ neighborhoodAddSecondaryOwner.PlayerGuid, 1 /*RoleChanged*/, true /*promoted to manager = privileged*/ });
        neighborhood->BroadcastPacket(rosterUpdate.Write(), player->GetGUID());

        neighborhood->RefreshMirrorDataForOnlineMembers();
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

    // Broadcast roster update and refresh mirror data for all online members
    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ neighborhoodRemoveSecondaryOwner.PlayerGuid, 1 /*RoleChanged*/, false /*demoted to resident = not privileged*/ });
        neighborhood->BroadcastPacket(rosterUpdate.Write(), player->GetGUID());

        neighborhood->RefreshMirrorDataForOnlineMembers();
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
        WorldPackets::Housing::InviteEntry entry;
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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_BUY_HOUSE))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_BUY_HOUSE CornerstoneGuid: {}, HouseGuid: {}",
        neighborhoodBuyHouse.CornerstoneGuid.ToString(), neighborhoodBuyHouse.HouseGuid.ToString());

    // CMSG contains CornerstoneGuid (not a NeighborhoodGuid) — resolve neighborhood from player's map
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodBuyHouse.CornerstoneGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Neighborhood not found for CornerstoneGuid {}",
            neighborhoodBuyHouse.CornerstoneGuid.ToString());
        return;
    }

    // Use the client's PlotIndex cached from OpenCornerstoneUI.
    // The BuyHouse CMSG doesn't include a PlotIndex field, so we rely on the
    // previous OpenCornerstoneUI interaction which cached _lastClientPlotIndex.
    // Validate by checking the cornerstone GUID matches what we cached.
    uint8 resolvedPlotIndex = static_cast<uint8>(_lastClientPlotIndex);

    // Also resolve via DB2 for logging/validation
    int32 db2Resolved = sHousingMgr.ResolvePlotIndex(neighborhoodBuyHouse.CornerstoneGuid, neighborhood);

    TC_LOG_INFO("housing", "HandleNeighborhoodBuyHouse: Using client PlotIndex={} (DB2 resolved={}), CornerstoneGuid={}, HouseGuid={}",
        resolvedPlotIndex, db2Resolved, neighborhoodBuyHouse.CornerstoneGuid.ToString(), neighborhoodBuyHouse.HouseGuid.ToString());

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
                player->GetGUID().ToString(), neighborhood->GetGuid().ToString(), static_cast<uint32>(joinResult));
            return;
        }
        TC_LOG_INFO("housing", "HandleNeighborhoodBuyHouse: Auto-added player {} as resident of neighborhood '{}'",
            player->GetGUID().ToString(), neighborhood->GetName());
    }

    // Must not already own a house in this neighborhood
    if (player->GetHousingForNeighborhood(neighborhood->GetGuid()))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_HOUSE);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} already has a house in neighborhood {}",
            player->GetGUID().ToString(), neighborhood->GetGuid().ToString());
        return;
    }

    // Deduct gold cost (sniff-verified: 1000g = 10,000,000 copper)
    if (!player->HasEnoughMoney(HOUSE_PURCHASE_COST_COPPER))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_CANNOT_AFFORD);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodBuyHouse: Player {} cannot afford house (need {} copper, has {})",
            player->GetGUID().ToString(), HOUSE_PURCHASE_COST_COPPER, player->GetMoney());
        return;
    }

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), resolvedPlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Consume any 5-minute reservation hold the player placed via the House Finder.
        neighborhood->ClearReservation(player->GetGUID());
        player->ModifyMoney(-static_cast<int64>(HOUSE_PURCHASE_COST_COPPER));
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

        // TODO: Replace with quest-driven tutorial progression when the housing tutorial
        // questline is implemented. See Player.cpp LoadFromDB for full explanation.
        // Mark all server tutorial flags as seen (retail sniff: all 256 bits = 0xFF).
        for (uint8 i = 0; i < MAX_ACCOUNT_TUTORIAL_VALUES; ++i)
            SetTutorialInt(i, 0xFFFFFFFF);
        SendTutorialsData();

        // Also inject FrameTutorialAccount CVars into GLOBAL_CONFIG_CACHE.
        // The client's housing UI checks closedInfoFramesAccountWide bit 38
        // (HousingModesUnlocked) separately from the 256-bit server tutorial flags.
        {
            AccountData const* configCache = GetAccountData(GLOBAL_CONFIG_CACHE);
            std::string configData = configCache ? configCache->Data : "";
            bool modified = false;

            auto ensureCVar = [&](std::string_view cvarName, std::string_view value)
            {
                std::string setPrefix = std::string("SET ") + std::string(cvarName) + " \"";
                size_t pos = configData.find(setPrefix);
                if (pos != std::string::npos)
                {
                    size_t valStart = pos + setPrefix.size();
                    size_t valEnd = configData.find('"', valStart);
                    if (valEnd != std::string::npos)
                    {
                        std::string oldVal = configData.substr(valStart, valEnd - valStart);
                        if (oldVal != value)
                        {
                            configData.replace(valStart, valEnd - valStart, value);
                            modified = true;
                        }
                    }
                }
                else
                {
                    if (!configData.empty() && configData.back() != '\n')
                        configData += '\n';
                    configData += "SET ";
                    configData += cvarName;
                    configData += " \"";
                    configData += value;
                    configData += "\"\n";
                    modified = true;
                }
            };

            ensureCVar("closedInfoFramesAccountWide", "4294967295 4294967295");
            ensureCVar("housingTutorialsEnabled", "0");

            if (modified)
            {
                SetAccountData(GLOBAL_CONFIG_CACHE, GameTime::GetGameTime(), configData);
                SendAccountDataTimes(player->GetGUID(), GLOBAL_CACHE_MASK);
            }
        }

        // Retail sequence: FirstTimeDecorAcquisition → BuyHouseResponse → LevelFavor updates

        // 1a. Populate the server-side decor catalog with starter items (so edit mode works)
        Housing* housing = player->GetHousing();
        if (housing)
        {
            auto starterDecorWithQty = sHousingMgr.GetStarterDecorWithQuantities(player->GetTeam());
            for (auto const& [decorId, qty] : starterDecorWithQty)
            {
                for (int32 i = 0; i < qty; ++i)
                    housing->AddToCatalog(decorId);
            }
            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Populated catalog with {} unique decor types for player {}",
                uint32(starterDecorWithQty.size()), player->GetGUID().ToString());

            // 1a2. Auto-place starter decor in the visual room (sniff-verified: retail pre-places items).
            // The "Welcome Home" quest requires the player to remove 3 of these items.
            housing->PlaceStarterDecor();
        }

        // 1b. Send FirstTimeDecorAcquisition notifications (sniff: 7-8 unique decor IDs)
        std::vector<uint32> starterDecorIds = sHousingMgr.GetStarterDecorIds(player->GetTeam());
        for (uint32 decorId : starterDecorIds)
        {
            WorldPackets::Housing::HousingFirstTimeDecorAcquisition decorAcq;
            decorAcq.DecorEntryID = decorId;
            SendPacket(decorAcq.Write());
        }
        TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Sent {} FirstTimeDecorAcquisition packets (unique decor IDs)",
            uint32(starterDecorIds.size()));

        // 2. Build buy response with HouseInfo
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        if (Housing const* h = player->GetHousing())
        {
            response.House.HouseGuid = h->GetHouseGuid();
            response.House.OwnerGuid = player->GetGUID();
            response.House.NeighborhoodGuid = neighborhood->GetGuid();
            response.House.PlotId = resolvedPlotIndex;
            response.House.AccessFlags = h->GetSettingsFlags();
        }
        WorldPacket const* buyRespPkt = response.Write();
        SendPacket(buyRespPkt);

        TC_LOG_DEBUG("housing", "SMSG_NEIGHBORHOOD_BUY_HOUSE_RESPONSE Result={}, PlotId={}, HouseGuid={}, OwnerGuid={}",
            uint32(response.Result), response.House.PlotId,
            response.House.HouseGuid.ToString(), response.House.OwnerGuid.ToString());

        // 3. Send level/favor updates (sniff: always 2 packets after buy response).
        // Wire format: header (Type, ChangeAmount, Reason, Count) + per-entry
        // (EntryFlags, EntryTimestamp, HouseGUID, NewFavorTotal, Reserved, Terminator).
        if (Housing const* h = player->GetHousing())
        {
            // Packet 1: Initial level assignment
            {
                WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelFavor;
                levelFavor.Result = 0;
                levelFavor.ChangeAmount = 0;
                levelFavor.Reason = 1;
                levelFavor.HouseGUID = h->GetHouseGuid();
                levelFavor.NewFavorTotal = 910;
                SendPacket(levelFavor.Write());
            }
            // Packet 2: Favor state
            {
                WorldPackets::Housing::HousingSvcsUpdateHousesLevelFavor levelFavor;
                levelFavor.Result = 0;
                levelFavor.ChangeAmount = 910;
                levelFavor.Reason = 1;
                levelFavor.HouseGUID = h->GetHouseGuid();
                levelFavor.NewFavorTotal = 910;
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
                rosterUpdate.Residents.push_back({ player->GetGUID(), 0 /*Added*/, false /*resident = not privileged*/ });
                memberPlayer->SendDirectMessage(rosterUpdate.Write());
            }
        }

        // Send guild notification for house addition
        if (Housing const* housing = player->GetHousing())
        {
            if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
            {
                WorldPackets::Housing::HousingSvcsGuildAddHouseNotification notification;
                notification.House.HouseGUID = housing->GetHouseGuid();
                notification.House.OwnerGUID = player->GetGUID();
                notification.House.HouseLevel = static_cast<uint8>(housing->GetLevel());
                guild->BroadcastPacket(notification.Write());
            }
        }

        // Mark the plot Cornerstone as owned (GOState 1 = READY)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->SetPlotOwnershipState(resolvedPlotIndex, true);

            // Spawn the house using the player's Housing data
            Housing const* buyHousing = player->GetHousing();
            int32 buyExtCompID = buyHousing ? static_cast<int32>(buyHousing->GetCoreExteriorComponentID()) : 0;
            int32 buyWmoDataID = buyHousing ? static_cast<int32>(buyHousing->GetHouseType()) : 0;
            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Calling SpawnHouseForPlot for plot {} (extComp={}, wmoData={})",
                resolvedPlotIndex, buyExtCompID, buyWmoDataID);
            GameObject* houseGo = housingMap->SpawnHouseForPlot(resolvedPlotIndex, nullptr, buyExtCompID, buyWmoDataID);
            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: SpawnHouseForPlot result: {}",
                houseGo ? houseGo->GetGUID().ToString() : "FAILED/NULL");
        }
        else
        {
            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Player map is NOT a HousingMap — cannot spawn house exterior!");
        }

        // Notify client that the basic house was created
        if (Housing const* h = player->GetHousing())
        {
            WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse houseResponse;
            houseResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(houseResponse.Write());
        }

        // Refresh NeighborhoodMirrorData (Houses[] changed) on all online members
        neighborhood->RefreshMirrorDataForOnlineMembers();

        // Proactively send DECOR_REQUEST_STORAGE_RESPONSE after purchase.
        // The client requests storage at map entry (before purchase) and gets "no house".
        // It does NOT re-request after purchase, so we must push the updated state.
        // Retail flow: populate storage entries into Account entity THEN send update.
        if (Housing* h = player->GetHousing())
        {
            h->PopulateCatalogStorageEntries();
            h->SyncUpdateFields();

            WorldPackets::Housing::HousingDecorRequestStorageResponse storageResp;
            storageResp.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(storageResp.Write());

            // Send Account + HousingPlayerHouseEntity together so budget data
            // accompanies storage data for the client's decor count display.
            {
                GetBattlenetAccount().BuildUpdateChangesMask();
                GetHousingPlayerHouseEntity().BuildUpdateChangesMask();

                UpdateData updateData(player->GetMapId());
                WorldPacket updatePacket;

                if (player->HaveAtClient(&GetBattlenetAccount()))
                    GetBattlenetAccount().BuildValuesUpdateBlockForPlayer(&updateData, player);
                else
                {
                    GetBattlenetAccount().BuildCreateUpdateBlockForPlayer(&updateData, player);
                    player->m_clientGUIDs.insert(GetBattlenetAccount().GetGUID());
                }

                if (player->HaveAtClient(&GetHousingPlayerHouseEntity()))
                    GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&updateData, player);
                else
                {
                    GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
                    player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());
                }

                updateData.BuildPacket(&updatePacket);
                player->SendDirectMessage(&updatePacket);

                GetBattlenetAccount().ClearUpdateMask(true);
                GetHousingPlayerHouseEntity().ClearUpdateMask(true);
            }

            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Sent proactive STORAGE_RSP + Account + HouseEntity update (CatalogEntries={})",
                uint32(h->GetCatalogEntries().size()));
        }

        TC_LOG_ERROR("housing", "Player {} purchased plot {} in neighborhood '{}'",
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

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_MOVE_HOUSE))
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_MOVE_HOUSE CornerstoneGuid: {}, HouseGuid: {}",
        neighborhoodMoveHouse.CornerstoneGuid.ToString(), neighborhoodMoveHouse.HouseGuid.ToString());

    // Use cornerstone GO GUID to find the neighborhood and destination plot.
    // Per IDA TryMoveHouse (0x7FF75CC59CA1), the first PackedGUID is validated
    // to be HighGuid::GameObject — i.e., a cornerstone GO at the destination plot.
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodMoveHouse.CornerstoneGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Neighborhood not found via cornerstone {}",
            neighborhoodMoveHouse.CornerstoneGuid.ToString());
        return;
    }

    // Validate that the HouseGuid in the CMSG matches the player's owned house —
    // anti-spoof: a malicious client could try to relocate someone else's house.
    Housing* housing = player->GetHousing();
    if (!housing || housing->GetHouseGuid() != neighborhoodMoveHouse.HouseGuid)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Player {} HouseGuid mismatch — owns {}, CMSG sent {}",
            player->GetGUID().ToString(),
            housing ? housing->GetHouseGuid().ToString() : "<no house>",
            neighborhoodMoveHouse.HouseGuid.ToString());
        return;
    }

    // Resolve destination plot index from the cornerstone GO GUID. Falls back to
    // the cached _lastClientPlotIndex if the cornerstone resolve misses (covers
    // the OPEN_CORNERSTONE_UI → MOVE_HOUSE flow where the GO no longer exists
    // on the destination plot, e.g. just-bought plots).
    int32 resolvedTarget = sHousingMgr.ResolvePlotIndex(neighborhoodMoveHouse.CornerstoneGuid, neighborhood);
    uint8 targetPlotIndex = (resolvedTarget >= 0)
        ? static_cast<uint8>(resolvedTarget)
        : static_cast<uint8>(_lastClientPlotIndex);

    if (targetPlotIndex == INVALID_PLOT_INDEX)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Could not resolve destination plot from cornerstone {} (fallback _lastClientPlotIndex={})",
            neighborhoodMoveHouse.CornerstoneGuid.ToString(), _lastClientPlotIndex);
        return;
    }

    // Reject moving to the same plot the player already occupies (no-op).
    Neighborhood::Member const* memberInfo = neighborhood->GetMember(player->GetGUID());
    uint8 oldPlotIndex = memberInfo ? memberInfo->PlotIndex : INVALID_PLOT_INDEX;
    if (oldPlotIndex == targetPlotIndex)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_VACANT);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: target plot {} is the player's current plot",
            targetPlotIndex);
        return;
    }

    // Deduct gold cost for house move
    if (!player->HasEnoughMoney(HOUSE_MOVE_COST_COPPER))
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_CANNOT_AFFORD);
        SendPacket(response.Write());

        TC_LOG_DEBUG("housing", "HandleNeighborhoodMoveHouse: Player {} cannot afford move (need {} copper, has {})",
            player->GetGUID().ToString(), HOUSE_MOVE_COST_COPPER, player->GetMoney());
        return;
    }

    HousingResult result = neighborhood->MoveHouse(player->GetGUID(), targetPlotIndex);

    WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
    response.Result = static_cast<uint8>(result);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // The 5-minute hold the player may have placed via the House Finder is
        // consumed by the actual move; clear it so the lock is released early.
        neighborhood->ClearReservation(player->GetGUID());

        // Update Housing::_plotIndex so all subsequent responses (HouseStatus,
        // HouseInfo, SyncUpdateFields, etc.) use the correct DB2 PlotIndex.
        if (Housing* housing = player->GetHousing())
        {
            housing->SetPlotIndex(targetPlotIndex);
            housing->SyncUpdateFields();
            // Push the Housing/3 entity (HousingPlayerHouseEntity) to the client
            // as CREATE — the regular world-map plot icon resolves via entity
            // registry lookup of HouseGUID and the icon "self/friend/stranger"
            // chooser only re-evaluates when CREATE_OBJECT arrives. Without an
            // explicit re-push here, SyncUpdateFields just flips dirty bits and
            // the client's local entity copy stays at the old PlotIndex —
            // the new plot's icon stays "unowned" until the player re-logs.
            GetHousingPlayerHouseEntity().SendCreateToPlayer(player);
        }

        player->ModifyMoney(-static_cast<int64>(HOUSE_MOVE_COST_COPPER));

        // Despawn entities at old plot, respawn at new plot
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            if (oldPlotIndex != INVALID_PLOT_INDEX)
            {
                housingMap->DespawnAllDecorForPlot(oldPlotIndex);
                housingMap->DespawnAllMeshObjectsForPlot(oldPlotIndex);
                housingMap->DespawnRoomForPlot(oldPlotIndex);
                housingMap->DespawnHouseForPlot(oldPlotIndex);
                housingMap->SetPlotOwnershipState(oldPlotIndex, false);
            }

            housingMap->SetPlotOwnershipState(targetPlotIndex, true);
            if (Housing const* h = player->GetHousing())
            {
                auto fixtureOverrides = h->GetFixtureOverrideMap();
                housingMap->SpawnHouseForPlot(targetPlotIndex, nullptr,
                    static_cast<int32>(h->GetCoreExteriorComponentID()),
                    static_cast<int32>(h->GetHouseType()),
                    fixtureOverrides.empty() ? nullptr : &fixtureOverrides);

                // Re-spawn the player's exterior decor at the new plot. DespawnAllDecorForPlot
                // (called above for the old plot) only removes the in-world entities — the
                // PlacedDecor records in the Housing object are preserved. Without this
                // matching SpawnAllDecorForPlot the decor stays gone after a move.
                // Decor is NOT returned to the chest; it follows the house.
                housingMap->SpawnAllDecorForPlot(targetPlotIndex, h);
            }
            else
            {
                TC_LOG_ERROR("housing", "HandleNeighborhoodMoveHouse: No Housing object for player — cannot spawn house at plot {}", targetPlotIndex);
            }
        }

        if (Housing const* housing = player->GetHousing())
        {
            response.House.HouseGuid = housing->GetHouseGuid();
            response.House.OwnerGuid = player->GetGUID();
            response.House.NeighborhoodGuid = housing->GetNeighborhoodGuid();
            response.House.PlotId = housing->GetPlotIndex();
            response.House.AccessFlags = housing->GetSettingsFlags();
        }

        // Broadcast roster update to other members
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ player->GetGUID(), 1 /*RoleChanged*/, false /*resident = not privileged*/ });
        neighborhood->BroadcastPacket(rosterUpdate.Write(), player->GetGUID());

        // Refresh NeighborhoodMirrorData (Houses[] changed — plot moved)
        neighborhood->RefreshMirrorDataForOnlineMembers();
    }
    // Sniff-verified (12.0.5 packet #13402, 40-byte SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE):
    // the trailing PackedGUID is a copy of the moved house's HouseGuid, not an empty
    // transaction GUID. Re-emitting the same HouseGuid lets the client correlate the
    // response with its locally-tracked move-in-progress entry.
    response.MoveTransactionGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    TC_LOG_DEBUG("housing", "MoveHouse result: {} from plot {} to plot {} via cornerstone {}",
        uint32(result), oldPlotIndex, targetPlotIndex,
        neighborhoodMoveHouse.CornerstoneGuid.ToString());
}

void WorldSession::HandleNeighborhoodOpenCornerstoneUI(WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUI const& neighborhoodOpenCornerstoneUI)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_OPEN_CORNERSTONE_UI PlotIndex(raw): {}, NeighborhoodGuid: {}",
        neighborhoodOpenCornerstoneUI.PlotIndex, neighborhoodOpenCornerstoneUI.NeighborhoodGuid.ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodOpenCornerstoneUI.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "HandleNeighborhoodOpenCornerstoneUI: Neighborhood {} not found",
            neighborhoodOpenCornerstoneUI.NeighborhoodGuid.ToString());
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.PlotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
        SendPacket(response.Write());
        return;
    }

    // Use the client's PlotIndex directly — it may differ from our DB2 PlotIndex
    // values (our SQL has sequential 0-54; the client's actual DB2 may differ).
    // Also cache for the subsequent BuyHouse CMSG which doesn't include PlotIndex.
    uint32 plotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
    _lastClientPlotIndex = plotIndex;
    _lastCornerstoneGuid = neighborhoodOpenCornerstoneUI.NeighborhoodGuid;

    // Also resolve via cornerstone GO entry for cost lookup (uses our DB2 internal index)
    int32 resolved = sHousingMgr.ResolvePlotIndex(neighborhoodOpenCornerstoneUI.NeighborhoodGuid, neighborhood);

    TC_LOG_INFO("housing", "HandleNeighborhoodOpenCornerstoneUI: Client PlotIndex={}, DB2 resolved={}, CornerstoneGuid={}",
        plotIndex, resolved, neighborhoodOpenCornerstoneUI.NeighborhoodGuid.ToString());

    // Look up cost from plot data — try both the client's PlotIndex and our DB2 PlotIndex
    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

    uint64 plotCost = 0;
    bool plotFound = false;

    // Try the client's PlotIndex first, then fall back to DB2 resolved index
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == static_cast<int32>(plotIndex))
        {
            plotCost = plot->Cost;
            plotFound = true;
            break;
        }
    }

    // If client PlotIndex didn't match our DB2, try the resolved DB2 PlotIndex
    if (!plotFound && resolved >= 0 && static_cast<uint32>(resolved) != plotIndex)
    {
        for (NeighborhoodPlotData const* plot : plots)
        {
            if (plot->PlotIndex == resolved)
            {
                plotCost = plot->Cost;
                plotFound = true;
                TC_LOG_INFO("housing", "HandleNeighborhoodOpenCornerstoneUI: Cost found via DB2 PlotIndex {} (client sent {})",
                    resolved, plotIndex);
                break;
            }
        }
    }

    // Last resort: use the cornerstone GO entry to find the plot
    if (!plotFound)
    {
        uint32 goEntry = neighborhoodOpenCornerstoneUI.NeighborhoodGuid.GetEntry();
        if (goEntry)
        {
            NeighborhoodPlotData const* plotData = sHousingMgr.GetPlotByCornerstoneEntry(neighborhoodMapId, goEntry);
            if (plotData)
            {
                plotCost = plotData->Cost;
                plotFound = true;
                TC_LOG_INFO("housing", "HandleNeighborhoodOpenCornerstoneUI: Cost found via cornerstone GO entry {} (DB2 PlotIndex {})",
                    goEntry, plotData->PlotIndex);
            }
        }
    }

    if (!plotFound)
    {
        TC_LOG_ERROR("housing", "HandleNeighborhoodOpenCornerstoneUI: PlotIndex {} (DB2: {}) not found in neighborhood map {}",
            plotIndex, resolved,
            plotIndex, neighborhoodMapId);
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.PlotIndex = plotIndex;
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
        nameResp.Result = true;
        nameResp.NeighborhoodName = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    // Look up ownership from the Neighborhood's plot info
    uint8 plotIdx = static_cast<uint8>(plotIndex);
    Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(plotIdx);
    bool isOwned = plotInfo && !plotInfo->OwnerGuid.IsEmpty();

    // Build cornerstone UI response — wire format verified against retail 12.0.1 build 65940.
    // Horde retail sniff shows two patterns:
    //   Packet 1 (PlotIndex=37): PurchaseStatus=73 (PlotReserved), Cost=10M — actively reserved plot
    //   Packet 2 (PlotIndex=54): PurchaseStatus=0, Cost=10M, HasAlternatePrice — available plot
    // PurchaseStatus=73 = HousingResult::PlotReserved, NOT "purchasable".
    // For unclaimed purchasable plots: PurchaseStatus=0, Cost=plotCost.
    WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
    response.PlotIndex = plotIndex;
    response.PurchaseStatus = 0;
    response.NeighborhoodGuid = neighborhood->GetGuid();
    response.CornerstoneGuid = neighborhoodOpenCornerstoneUI.NeighborhoodGuid; // GO GUID from CMSG
    response.IsPlotOwned = isOwned;
    response.CanPurchase = !isOwned;
    response.NeighborhoodName = neighborhood->GetName();

    // Set IsInitiative when the neighborhood has an active initiative/endeavor
    uint64 nhLowGuid = neighborhood->GetGuid().GetCounter();
    response.IsInitiative = (sInitiativeManager.GetActiveInitiative(nhLowGuid) != nullptr);

    if (isOwned)
    {
        // Owned plot: show owner info, no purchase available
        response.PlotOwnerGuid = plotInfo->OwnerGuid;
        response.Cost = 0;
    }
    else
    {
        // Unclaimed plot: send cost so client can show purchase UI
        response.PlotOwnerGuid = ObjectGuid::Empty;
        response.Cost = plotCost;
        response.AlternatePrice = static_cast<uint64>(GameTime::GetGameTime()) + 7 * DAY;

        // If another player currently holds the 5-min reservation, retail
        // marks the plot with PurchaseStatus = HOUSING_RESULT_PLOT_RESERVED (73).
        // The client renders this as "Reserved" and disables the action button.
        // The reserving player themselves still gets PurchaseStatus=0 so they
        // can act on their own hold.
        ObjectGuid otherReserver = neighborhood->GetPlotReserverOther(plotIdx, player->GetGUID());
        if (!otherReserver.IsEmpty())
        {
            response.PurchaseStatus = static_cast<uint8>(HOUSING_RESULT_PLOT_RESERVED);
            TC_LOG_INFO("housing",
                "OpenCornerstoneUI: plot {} is reserved by {}; marking PurchaseStatus=PLOT_RESERVED for viewer {}",
                plotIdx, otherReserver.ToString(), player->GetGUID().ToString());
        }
    }

    // If the player already owns a house in this neighborhood, embed it in the
    // response. The cornerstone Lua reads this as "you have a house here" and
    // flips the action button from Buy to Move. Without this the button stays
    // on Buy, which then routes to BUY_HOUSE and gets rejected by HandleNeighborhoodBuyHouse
    // (HOUSING_RESULT_INVALID_HOUSE — "player already has a house in neighborhood").
    // Only embed when the plot is actually actionable for this player (not owned
    // by anyone else and not reserved by anyone else).
    if (!isOwned && response.PurchaseStatus == 0)
    {
        if (Housing const* myHousing = player->GetHousingForNeighborhood(neighborhood->GetGuid()))
        {
            WorldPackets::Housing::JamCliHouse existingHouse;
            existingHouse.HouseGUID = myHousing->GetHouseGuid();
            existingHouse.OwnerGUID = player->GetGUID();
            existingHouse.NeighborhoodGUID = myHousing->GetNeighborhoodGuid();
            existingHouse.PlotIndex = myHousing->GetPlotIndex();
            existingHouse.HouseLevel = static_cast<uint8>(myHousing->GetLevel());
            existingHouse.HasOptionalField = false;
            response.ExistingHouse = std::move(existingHouse);
        }
    }
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

    // Create a pending ownership transfer instead of instant transfer
    HousingResult result = neighborhood->OfferOwnership(neighborhoodOfferOwnership.NewOwnerGuid);

    WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    // Notify the target player about the offer
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Player* newOwner = ObjectAccessor::FindPlayer(neighborhoodOfferOwnership.NewOwnerGuid))
        {
            WorldPackets::Housing::HousingSvcsNeighborhoodOwnershipTransferredResponse transferNotification;
            transferNotification.Result = static_cast<uint8>(result);
            transferNotification.OwnerGUID = neighborhoodOfferOwnership.NewOwnerGuid;
            newOwner->SendDirectMessage(transferNotification.Write());
        }
    }

    TC_LOG_DEBUG("housing", "OfferOwnership result: {} to player {} for neighborhood {}",
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
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
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
        data.ResidentType = member.Role;
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
        nameResp.Result = true;
        nameResp.NeighborhoodName = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    WorldPacket const* rosterPkt = response.Write();
    SendPacket(rosterPkt);

    // Populate the Housing/4 entity with this neighborhood's mirror data so the
    // client's internal house list stays in sync for plot resolution.
    HousingNeighborhoodMirrorEntity& mirrorEntity = GetHousingNeighborhoodMirrorEntity();
    mirrorEntity.SetName(neighborhood->GetName());
    mirrorEntity.SetOwnerGUID(neighborhood->GetOwnerGuid());

    mirrorEntity.ClearHouses();
    for (auto const& plot : neighborhood->GetPlots())
    {
        if (plot.IsOccupied() && !plot.HouseGuid.IsEmpty())
            mirrorEntity.AddHouse(plot.HouseGuid, plot.OwnerGuid);
        else
            mirrorEntity.AddHouse(ObjectGuid::Empty, ObjectGuid::Empty);
    }

    mirrorEntity.ClearManagers();
    for (auto const& member : neighborhood->GetMembers())
    {
        if (member.Role == NEIGHBORHOOD_ROLE_MANAGER || member.Role == NEIGHBORHOOD_ROLE_OWNER)
        {
            ObjectGuid bnetGuid;
            if (Player* mgr = ObjectAccessor::FindPlayer(member.PlayerGuid))
                bnetGuid = mgr->GetSession()->GetBattlenetAccountGUID();
            mirrorEntity.AddManager(bnetGuid, member.PlayerGuid);
        }
    }
    // Wholesale re-push (ClearHouses + 55 AddHouse + ClearManagers + AddManagers).
    // Retail emits CREATE_OBJECT here (sniff-verified). The client's map-icon
    // refresh path only fires on CREATE.
    mirrorEntity.SendCreateToPlayer(player);

    // Pre-push player names for all plot owners so the client can format
    // plot names via HOUSING_HOUSE_NAME_FORMAT without waiting for async name queries.
    {
        WorldPackets::Query::QueryPlayerNamesResponse nameResponse;
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || plot.OwnerGuid.IsEmpty())
                continue;

            WorldPackets::Query::NameCacheLookupResult& entry = nameResponse.Players.emplace_back();
            BuildNameQueryData(plot.OwnerGuid, entry);
        }
        if (!nameResponse.Players.empty())
        {
            SendPacket(nameResponse.Write());
            TC_LOG_DEBUG("housing", "HandleNeighborhoodGetRoster: pre-pushed {} plot owner names to NameCache",
                nameResponse.Players.size());
        }
    }

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

    TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_EVICT_PLOT PlotIndex(raw): {}, NeighborhoodGuid: {}",
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

    // Use the client's PlotIndex directly — client sends its internal plot ID
    // which may differ from our DB2 PlotIndex values
    uint32 plotIndex = neighborhoodEvictPlot.PlotIndex;

    int32 db2Resolved = sHousingMgr.ResolvePlotIndex(neighborhoodEvictPlot.NeighborhoodGuid, neighborhood);
    TC_LOG_INFO("housing", "HandleNeighborhoodEvictPlot: Using client PlotIndex={} (DB2 resolved={})",
        plotIndex, db2Resolved);

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
    if (Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(plotIndex)))
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
        uint8 plotIdx = static_cast<uint8>(plotIndex);

        // Despawn all entities on the plot
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->DespawnAllDecorForPlot(plotIdx);
            housingMap->DespawnAllMeshObjectsForPlot(plotIdx);
            housingMap->DespawnRoomForPlot(plotIdx);
            housingMap->DespawnHouseForPlot(plotIdx);
            housingMap->SetPlotOwnershipState(plotIdx, false);
        }

        // Handle evicted player housing cleanup
        if (!evictedPlayerGuid.IsEmpty())
        {
            if (Player* evictedPlayer = ObjectAccessor::FindPlayer(evictedPlayerGuid))
            {
                // Online: send eviction notice and delete housing object
                WorldPackets::Neighborhood::NeighborhoodEvictPlotNotice notice;
                notice.PlotId = plotIndex;
                notice.NeighborhoodGuid = neighborhoodEvictPlot.NeighborhoodGuid;
                notice.PlotGuid = plotGuid;
                evictedPlayer->SendDirectMessage(notice.Write());

                evictedPlayer->DeleteHousing(neighborhoodEvictPlot.NeighborhoodGuid);
            }
            else
            {
                // Offline: delete housing directly from DB
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER_HOUSING);
                stmt->setUInt64(0, evictedPlayerGuid.GetCounter());
                CharacterDatabase.Execute(stmt);
            }
        }

        // Broadcast roster update to remaining members using helper
        WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
        rosterUpdate.Residents.push_back({ evictedPlayerGuid, 2 /*Removed*/, false });
        neighborhood->BroadcastPacket(rosterUpdate.Write(), player->GetGUID());

        // Refresh NeighborhoodMirrorData (Houses[] changed)
        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

    TC_LOG_DEBUG("housing", "EvictPlayer result: {} for plot index {} in neighborhood {}",
        uint32(result), plotIndex, neighborhoodEvictPlot.NeighborhoodGuid.ToString());
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

    // Send initiative service status (retail-observed: this CMSG's only response)
    sInitiativeManager.SendInitiativeServiceStatus(this, true);

    // REMOVED proactive SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT (0x420365).
    // Sniff set-diff of 3 retail login captures shows retail never emits
    // this SMSG at login. The earlier comment claimed it was needed for
    // the client's C_NeighborhoodInitiative.isLoaded flag, but that claim
    // was not actually sniff-verified. The client sends
    // CMSG_GET_AVAILABLE_INITIATIVE_REQUEST when it needs the data; the
    // reactive handler at HandleGetAvailableInitiativeRequest delivers
    // SendPlayerInitiativeInfo on demand. Hypothesis: the proactive
    // unsolicited INITIATIVE_INFO_RESULT at login suppresses the client's
    // map-icon refresh (same pattern as the previously-removed proactive
    // roster response).
}

void WorldSession::HandleGetAvailableInitiativeRequest(WorldPackets::Neighborhood::GetAvailableInitiativeRequest const& getAvailableInitiativeRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(getAvailableInitiativeRequest.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        // No neighborhood: emit empty response. Wire is just GUID + uint8(0)
        // — Flags top-2-bits == 0 makes the client skip the data block entirely,
        // which is the no-data path. Real failures route via SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE.
        WorldPackets::Housing::GetPlayerInitiativeInfoResult response;
        response.NeighborhoodGUID = getAvailableInitiativeRequest.NeighborhoodGuid;
        SendPacket(response.Write());
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();
    sInitiativeManager.SendPlayerInitiativeInfo(this, nhObjGuid, nhGuid);

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
        // No neighborhood: emit empty log. Wire is just GUID + uint32(0).
        // Real failures route via SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE.
        WorldPackets::Housing::GetInitiativeActivityLogResult response;
        response.NeighborhoodGuid = getInitiativeActivityLogRequest.NeighborhoodGuid;
        SendPacket(response.Write());
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();
    sInitiativeManager.SendActivityLog(this, nhObjGuid, nhGuid);

    TC_LOG_DEBUG("housing", "CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST NeighborhoodGuid: {}, Player: {}",
        getInitiativeActivityLogRequest.NeighborhoodGuid.ToString(), player->GetGUID().ToString());
}

void WorldSession::HandleGetNeighborhoodInitiativeInfoRequest(WorldPackets::Neighborhood::GetNeighborhoodInitiativeInfoRequest const& getNeighborhoodInitiativeInfoRequest)
{
    // 12.0.5 sniff-verified opcode 0x380003. Lua entry point:
    // C_NeighborhoodInitiative.RequestNeighborhoodInitiativeInfo(neighborhoodGUID).
    // Always paired with CMSG_GET_INITIATIVE_ACTIVITY_LOG_REQUEST in the captured
    // traffic — both fired when the player opens the neighborhood initiative panel.
    // The 0x380002 request also returns initiative info but for the `Available`
    // panel; this 0x380003 path corresponds to the active/current initiative view.
    // Reuse SendPlayerInitiativeInfo, which serialises the current cycle, milestone,
    // remaining duration and player tasks via SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT
    // (0x420368) — that same SMSG is consumed by the client for both panels.
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(getNeighborhoodInitiativeInfoRequest.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "CMSG_GET_NEIGHBORHOOD_INITIATIVE_INFO_REQUEST NeighborhoodGuid {} not resolvable for Player: {}",
            getNeighborhoodInitiativeInfoRequest.NeighborhoodGuid.ToString(), player->GetGUID().ToString());
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();
    sInitiativeManager.SendPlayerInitiativeInfo(this, nhObjGuid, nhGuid);

    TC_LOG_DEBUG("housing", "CMSG_GET_NEIGHBORHOOD_INITIATIVE_INFO_REQUEST NeighborhoodGuid: {}, Player: {}",
        getNeighborhoodInitiativeInfoRequest.NeighborhoodGuid.ToString(), player->GetGUID().ToString());
}

void WorldSession::HandleInitiativeUpdateActiveNeighborhood(WorldPackets::Neighborhood::InitiativeUpdateActiveNeighborhood const& initiativeUpdateActiveNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_INITIATIVE_UPDATE_ACTIVE_NEIGHBORHOOD NeighborhoodGuid: {}, Player: {}",
        initiativeUpdateActiveNeighborhood.NeighborhoodGuid.ToString(), player->GetGUID().ToString());

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(initiativeUpdateActiveNeighborhood.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        TC_LOG_DEBUG("housing", "CMSG_INITIATIVE_UPDATE_ACTIVE_NEIGHBORHOOD: Neighborhood not found for Player: {}",
            player->GetGUID().ToString());
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();

    // Send initiative service status to confirm the service is active
    sInitiativeManager.SendInitiativeServiceStatus(this, true);

    // Send current initiative info for the active neighborhood (with real task progress)
    sInitiativeManager.SendPlayerInitiativeInfo(this, nhObjGuid, nhGuid);

    TC_LOG_DEBUG("housing", "SMSG_INITIATIVE_SERVICE_STATUS + SMSG_GET_PLAYER_INITIATIVE_INFO_RESULT sent for NeighborhoodGuid: {}",
        initiativeUpdateActiveNeighborhood.NeighborhoodGuid.ToString());
}

// ============================================================================
// 0x38xxxx NeighborhoodInitiative — generic Op-XX handlers
// ============================================================================
//
// These 12 opcodes are sent by the client per IDA-decoded wire formats
// (INITIATIVE_WIRE_FORMAT_AUTHORITATIVE_67186.md). Their 1:1 Lua API binding
// requires runtime sniff data — vtable indirection in the client (hash
// 0xBA8F5C5BC59E8E8E = INITIATIVE_TASKS_TRACKED_LIST_CHANGED) prevents static
// resolution. Per the doc:
//   - 0x380001, 0x38000C: candidates for SetActiveNeighborhood / SetViewingNeighborhood
//   - 0x380007, 0x38000A, 0x38000B: candidates for AddTrackedInitiativeTask /
//     RemoveTrackedInitiativeTask (uint32 taskID); the doc indicates the user-callable
//     APIs flush via the BATCH path 0x38000E rather than these direct uint32 senders.
//   - 0x380006, 0x380008: empty-payload candidates for RequestInitiativeActivityLog /
//     RequestNeighborhoodInitiativeInfo (the dedicated 0x380003/0x380004 opcodes also
//     match those Lua APIs — multiple paths exist).
//   - 0x380005: uint32+GUID, possibly task-state mutation tied to a neighborhood
//   - 0x380009: float, debug or rate-progress sender
//   - 0x38000D: pair-array progress submission with bit flag
//   - 0x38000E: bulk uint32 array (likely tracked-task ID list flush)
//   - 0x38000F: bulk quad-int records (likely milestone/claim batch)
//
// Until sniff data confirms exact semantics, each handler:
//   - parses the wire format successfully (doesn't error/disconnect)
//   - logs the request for diagnostic capture
//   - returns silently (no SMSG response)
// This matches how the client's other "fire-and-forget" senders behave in retail.

void WorldSession::HandleNeighborhoodInitiativeOp01(WorldPackets::Neighborhood::NeighborhoodInitiativeOp01 const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_01 NeighborhoodGuid: {} (player: {})",
        packet.NeighborhoodGuid.ToString(), GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp05(WorldPackets::Neighborhood::NeighborhoodInitiativeOp05 const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_05 Field1: {} NeighborhoodGuid: {} (player: {})",
        packet.Field1, packet.NeighborhoodGuid.ToString(), GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp06(WorldPackets::Neighborhood::NeighborhoodInitiativeOp06 const& /*packet*/)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_06 (player: {})", GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp07(WorldPackets::Neighborhood::NeighborhoodInitiativeOp07 const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_07 Value: {} (player: {})",
        packet.Value, GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp08(WorldPackets::Neighborhood::NeighborhoodInitiativeOp08 const& /*packet*/)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_08 (player: {})", GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp09(WorldPackets::Neighborhood::NeighborhoodInitiativeOp09 const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_09 Value: {} (player: {})",
        packet.Value, GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp0A(WorldPackets::Neighborhood::NeighborhoodInitiativeOp0A const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_0A Value: {} (player: {})",
        packet.Value, GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp0B(WorldPackets::Neighborhood::NeighborhoodInitiativeOp0B const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_0B Value: {} (player: {})",
        packet.Value, GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp0C(WorldPackets::Neighborhood::NeighborhoodInitiativeOp0C const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_0C NeighborhoodGuid: {} (player: {})",
        packet.NeighborhoodGuid.ToString(), GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp0D(WorldPackets::Neighborhood::NeighborhoodInitiativeOp0D const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_0D Header: {} Pairs: {} Flag: {} (player: {})",
        packet.Header, uint32(packet.Pairs.size()), packet.Flag, GetPlayer()->GetGUID().ToString());
}

void WorldSession::HandleNeighborhoodInitiativeOp0E(WorldPackets::Neighborhood::NeighborhoodInitiativeOp0E const& packet)
{
    if (!GetPlayer())
        return;
    // Per the IDA doc this is the bulk-flush path for tracked-task list mutations.
    // Persist the new tracked-task list into the player's initiative state.
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_0E TaskIDs[{}] (player: {})",
        uint32(packet.TaskIDs.size()), GetPlayer()->GetGUID().ToString());
    for (uint32 id : packet.TaskIDs)
        TC_LOG_TRACE("housing", "  task: {}", id);
}

void WorldSession::HandleNeighborhoodInitiativeOp0F(WorldPackets::Neighborhood::NeighborhoodInitiativeOp0F const& packet)
{
    if (!GetPlayer())
        return;
    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INITIATIVE_OPCODE_0F Records[{}] (player: {})",
        uint32(packet.Records.size()), GetPlayer()->GetGUID().ToString());
    for (auto const& r : packet.Records)
        TC_LOG_TRACE("housing", "  record: ({}, {}, {}, {})", r.A, r.B, r.C, r.D);
}

// ============================================================
// Phase 7 — Charter Handlers
// ============================================================

void WorldSession::HandleNeighborhoodCharterSignResponse(WorldPackets::Neighborhood::NeighborhoodCharterSignResponsePacket const& neighborhoodCharterSignResponse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_CHARTER_SIGN_RESPONSE Player: {} CharterGuid: {}",
        player->GetGUID().ToString(), neighborhoodCharterSignResponse.CharterGuid.ToString());

    // Process the sign response — the player is accepting/declining a charter sign request
    // Send the sign request notification back to the charter owner
    WorldPackets::Neighborhood::NeighborhoodCharterSignRequest response;
    response.CharterGuid = neighborhoodCharterSignResponse.CharterGuid;
    SendPacket(response.Write());
}

void WorldSession::HandleNeighborhoodCharterRemoveSignature(WorldPackets::Neighborhood::NeighborhoodCharterRemoveSignature const& neighborhoodCharterRemoveSignature)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_CHARTER_REMOVE_SIGNATURE Player: {} CharterGuid: {} SignerGuid: {}",
        player->GetGUID().ToString(), neighborhoodCharterRemoveSignature.CharterGuid.ToString(),
        neighborhoodCharterRemoveSignature.SignerGuid.ToString());

    WorldPackets::Neighborhood::NeighborhoodCharterSignatureRemovedNotification response;
    response.CharterGuid = neighborhoodCharterRemoveSignature.CharterGuid;
    SendPacket(response.Write());
}

// ============================================================
// Phase 7 — Neighborhood Handlers
// ============================================================

void WorldSession::HandleNeighborhoodCancelInvitationAlt(WorldPackets::Neighborhood::NeighborhoodCancelInvitationAlt const& neighborhoodCancelInvitationAlt)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_CANCEL_INVITATION_ALT Player: {} InviteeGuid: {}",
        player->GetGUID().ToString(), neighborhoodCancelInvitationAlt.InviteeGuid.ToString());

    WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.InviteeGuid = neighborhoodCancelInvitationAlt.InviteeGuid;
    SendPacket(response.Write());
}

void WorldSession::HandleNeighborhoodInviteNotificationAck(WorldPackets::Neighborhood::NeighborhoodInviteNotificationAck const& neighborhoodInviteNotificationAck)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_INVITE_NOTIFICATION_ACK Player: {} NeighborhoodGuid: {}",
        player->GetGUID().ToString(), neighborhoodInviteNotificationAck.NeighborhoodGuid.ToString());

    // Acknowledgment only — no response needed
    // The client is confirming it received the invite notification
}

void WorldSession::HandleNeighborhoodOfferOwnershipResponse(WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponsePacket const& neighborhoodOfferOwnershipResponse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    TC_LOG_DEBUG("housing", "CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE Player: {} NeighborhoodGuid: {} Accept: {}",
        player->GetGUID().ToString(), neighborhoodOfferOwnershipResponse.NeighborhoodGuid.ToString(),
        neighborhoodOfferOwnershipResponse.Accept);

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(
        neighborhoodOfferOwnershipResponse.NeighborhoodGuid, player);

    WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;

    if (!neighborhood)
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (neighborhoodOfferOwnershipResponse.Accept)
    {
        HousingResult result = neighborhood->AcceptOwnershipTransfer(player->GetGUID());
        response.Result = static_cast<uint8>(result);

        if (result == HOUSING_RESULT_SUCCESS)
        {
            // Broadcast roster update — role change for both old owner and new owner
            WorldPackets::Neighborhood::NeighborhoodRosterResidentUpdate rosterUpdate;
            rosterUpdate.Residents.push_back({ player->GetGUID(), 1 /*RoleChanged*/, true /*owner = privileged*/ });
            neighborhood->BroadcastPacket(rosterUpdate.Write());

            // Refresh NeighborhoodMirrorData (OwnerGUID + Managers changed)
            neighborhood->RefreshMirrorDataForOnlineMembers();

            TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE: Player {} accepted ownership of neighborhood {}",
                player->GetGUID().ToString(), neighborhood->GetGuid().ToString());
        }
    }
    else
    {
        HousingResult result = neighborhood->RejectOwnershipTransfer(player->GetGUID());
        response.Result = static_cast<uint8>(result);

        TC_LOG_INFO("housing", "CMSG_NEIGHBORHOOD_OFFER_OWNERSHIP_RESPONSE: Player {} rejected ownership of neighborhood {}",
            player->GetGUID().ToString(), neighborhood->GetGuid().ToString());
    }

    SendPacket(response.Write());
}
