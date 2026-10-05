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
#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Housing.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingNeighborhoodMirrorEntity.h"
#include "HousingPackets.h"
#include "HousingPlayerHouseEntity.h"
#include "HousingRoomEntity.h"
#include "InitiativeManager.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodCharter.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryPackets.h"
#include "UpdateData.h"
#include "World.h"

namespace
{

    // Charter ids are the creator's player GUID counter.
    bool LoadNeighborhoodCharter(uint64 charterId, NeighborhoodCharter& charter)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
        stmt->setUInt64(0, charterId);
        PreparedQueryResult charterResult = CharacterDatabase.Query(stmt);
        if (!charterResult)
            return false;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
        stmt->setUInt64(0, charterId);
        return charter.LoadFromDB(charterResult, CharacterDatabase.Query(stmt));
    }

    // Signatures a charter still needs, the creator counting as one (Housing.CharterRequiredSignatures).
    bool CharterHasEnoughSignatures(NeighborhoodCharter const& charter)
    {
        uint32 const requiredSignatures = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_REQUIRED_SIGNATURES);
        return charter.HasEnoughSignatures(requiredSignatures > 0 ? requiredSignatures - 1 : 0);
    }
}

// Neighborhood Charter System

void WorldSession::HandleNeighborhoodCharterOpenConfirmationUI(WorldPackets::Neighborhood::NeighborhoodCharterOpenConfirmationUI const& /*neighborhoodCharterOpenConfirmationUI*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Reply to the steward's "found the neighborhood" gossip option; Confirm sends CMSG_NEIGHBORHOOD_CHARTER_FINALIZE.
    WorldPackets::Neighborhood::NeighborhoodCharterOpenConfirmationUIResponse response;

    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
    else if (!LoadNeighborhoodCharter(charterId, charter))
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
    else if (!CharterHasEnoughSignatures(charter))
        response.Result = static_cast<uint8>(HOUSING_RESULT_MORE_SIGNATURES_NEEDED);
    else
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        response.Field1 = charter.GetNeighborhoodMapID();
        response.Field2 = charter.GetFactionFlags();
        response.NeighborhoodName = charter.GetName();
    }

    SendPacket(response.Write());

}

void WorldSession::SendNeighborhoodCharterOpenUI()
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Same body as the create reply: everything the charter panel renders.
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult charterResult = CharacterDatabase.Query(stmt);
    if (!charterResult)
    {
        // Retail: using the charter item opens the panel from scratch — the name/map are chosen in
        // the UI, which then sends CHARTER_CREATE. No row yet means a fresh charter, not an error.
        WorldPackets::Neighborhood::NeighborhoodCharterOpenUIResponse openUI;
        openUI.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        openUI.CharterGuid = player->GetGUID();
        openUI.MapID = 0;
        openUI.SignatureCount = 1; // the creator counts
        openUI.Unknown = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_REQUIRED_SIGNATURES);
        SendPacket(openUI.Write());
        return;
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult sigResult = CharacterDatabase.Query(stmt);

    WorldPackets::Neighborhood::NeighborhoodCharterOpenUIResponse openUI;
    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!charter.LoadFromDB(charterResult, sigResult))
    {
        // A charter row exists but will not load - report the failure instead of a blank panel.
        openUI.Result = static_cast<uint8>(HOUSING_RESULT_DB_ERROR);
        SendPacket(openUI.Write());

        TC_LOG_ERROR("housing", "SendNeighborhoodCharterOpenUI: charter {} exists but failed to load for player {}",
            charterId, player->GetGUID().ToString());
        return;
    }

    openUI.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    openUI.CharterGuid = player->GetGUID();
    openUI.MapID = charter.GetNeighborhoodMapID();
    openUI.SignatureCount = charter.GetSignatureCount() + 1; // the creator counts
    openUI.Signers = charter.GetSignatures();
    openUI.Unknown = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_REQUIRED_SIGNATURES);
    openUI.NeighborhoodName = charter.GetName();
    SendPacket(openUI.Write());

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

    // Validate name
    if (neighborhoodCharterCreate.Name.empty() || neighborhoodCharterCreate.Name.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
        SendPacket(response.Write());

        return;
    }

    if (!ObjectMgr::IsValidCharterName(neighborhoodCharterCreate.Name) || sObjectMgr->IsReservedName(neighborhoodCharterCreate.Name))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());

        return;
    }

    // Create charter object
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    NeighborhoodCharter charter(charterId, player->GetGUID());
    charter.SetName(neighborhoodCharterCreate.Name);
    charter.SetNeighborhoodMapID(neighborhoodCharterCreate.NeighborhoodMapID);
    charter.SetFactionFlags(neighborhoodCharterCreate.FactionFlags);
    charter.SetIsGuild(false);

    // The creator doesn't count toward required signatures (AddSignature has a self-sign guard).

    // Persist to DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    charter.SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    // CharterGuid = creator's player GUID; Unknown = required signature count.
    WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.CharterGuid = player->GetGUID();
    response.MapID = neighborhoodCharterCreate.NeighborhoodMapID;
    response.SignatureCount = charter.GetSignatureCount() + 1; // the creator counts
    response.Signers = charter.GetSignatures();
    response.Unknown = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_REQUIRED_SIGNATURES);
    response.NeighborhoodName = neighborhoodCharterCreate.Name;
    SendPacket(response.Write());

    // Then the Neighborhood Charter item is pushed; using it re-opens the charter UI.
    if (!player->HasItemCount(ITEM_NEIGHBORHOOD_CHARTER, 1, true))
    {
        ItemPosCountVec dest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_NEIGHBORHOOD_CHARTER, 1) == EQUIP_ERR_OK)
        {
            if (Item* item = player->StoreNewItem(dest, ITEM_NEIGHBORHOOD_CHARTER, true))
                player->SendNewItem(item, 1, true, false);
        }
        else
            player->SendEquipError(EQUIP_ERR_INV_FULL, nullptr, nullptr, ITEM_NEIGHBORHOOD_CHARTER);
    }

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

    // Validate name
    if (neighborhoodCharterEdit.Name.empty() || neighborhoodCharterEdit.Name.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
        SendPacket(response.Write());

        return;
    }

    if (!ObjectMgr::IsValidCharterName(neighborhoodCharterEdit.Name) || sObjectMgr->IsReservedName(neighborhoodCharterEdit.Name))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());

        return;
    }

    // Edit updates the charter with new parameters (same charter ID, re-saved)
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    ObjectGuid charterGuid = player->GetGUID();

    // DeleteFromDB drops all signature rows; capture the co-signers first to notify them below.
    std::vector<ObjectGuid> droppedSigners;
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
        stmt->setUInt64(0, charterId);
        PreparedQueryResult oldCharterResult = CharacterDatabase.Query(stmt);
        if (oldCharterResult)
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
            stmt->setUInt64(0, charterId);
            PreparedQueryResult oldSigResult = CharacterDatabase.Query(stmt);

            NeighborhoodCharter oldCharter(charterId, ObjectGuid::Empty);
            if (oldCharter.LoadFromDB(oldCharterResult, oldSigResult))
            {
                for (ObjectGuid const& signer : oldCharter.GetSignatures())
                {
                    if (signer != player->GetGUID())
                        droppedSigners.push_back(signer);
                }
            }
        }
    }

    NeighborhoodCharter charter(charterId, player->GetGUID());
    charter.SetName(neighborhoodCharterEdit.Name);
    charter.SetNeighborhoodMapID(neighborhoodCharterEdit.NeighborhoodMapID);
    charter.SetFactionFlags(neighborhoodCharterEdit.FactionFlags);
    charter.SetIsGuild(false);

    // The creator doesn't count toward required signatures (AddSignature has a self-sign guard).

    // Re-persist
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    NeighborhoodCharter::DeleteFromDB(charterId, trans);
    charter.SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    // Notify co-signers whose signature the edit just wiped.
    for (ObjectGuid const& signer : droppedSigners)
    {
        if (Player* signerPlayer = ObjectAccessor::FindPlayer(signer))
        {
            WorldPackets::Neighborhood::NeighborhoodCharterSignatureRemovedNotification removed;
            removed.CharterGuid = charterGuid;
            signerPlayer->SendDirectMessage(removed.Write());
        }
    }

    WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.CharterGuid = charterGuid;
    response.MapID = neighborhoodCharterEdit.NeighborhoodMapID;
    response.SignatureCount = charter.GetSignatureCount() + 1; // the creator counts
    response.Signers = charter.GetSignatures();
    response.Unknown = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_REQUIRED_SIGNATURES);
    response.NeighborhoodName = neighborhoodCharterEdit.Name;
    SendPacket(response.Write());

}

void WorldSession::HandleNeighborhoodCharterFinalize(WorldPackets::Neighborhood::NeighborhoodCharterFinalize const& /*neighborhoodCharterFinalize*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // The confirmation frame listens for CREATE_NEIGHBORHOOD_RESULT from this SMSG.
    auto sendResult = [this](HousingResult result, Neighborhood const* neighborhood = nullptr)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(result);
        if (neighborhood)
        {
            response.Neighborhood.NeighborhoodGUID = neighborhood->GetGuid();
            response.Neighborhood.OwnerGUID = neighborhood->GetClientOwnerGuid();
            response.Neighborhood.Name = neighborhood->GetName();
        }
        SendPacket(response.Write());
    };

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_CHARTER_NEIGHBORHOOD))
    {
        sendResult(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        return;
    }

    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!LoadNeighborhoodCharter(charterId, charter))
    {
        sendResult(HOUSING_RESULT_GENERIC_FAILURE);
        return;
    }

    if (!CharterHasEnoughSignatures(charter))
    {
        sendResult(HOUSING_RESULT_MORE_SIGNATURES_NEEDED);
        return;
    }

    // Founding fee (Housing.CharterFoundingCost, in copper).
    uint32 const foundingCost = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_FOUNDING_COST);
    if (foundingCost && !player->HasEnoughMoney(uint64(foundingCost)))
    {
        sendResult(HOUSING_RESULT_CANNOT_AFFORD);
        return;
    }

    // FactionFlags are client-supplied and not a faction restriction; use the founder's faction.
    int32 const factionRestriction = player->GetTeam() == HORDE ? NEIGHBORHOOD_FACTION_HORDE : NEIGHBORHOOD_FACTION_ALLIANCE;
    Neighborhood* neighborhood = sNeighborhoodMgr.CreateNeighborhood(player->GetGUID(), charter.GetName(),
        charter.GetNeighborhoodMapID(), factionRestriction);
    if (!neighborhood)
    {
        sendResult(HOUSING_RESULT_DB_ERROR);
        return;
    }

    if (foundingCost)
        player->ModifyMoney(-static_cast<int64>(foundingCost));

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    NeighborhoodCharter::DeleteFromDB(charterId, trans);
    CharacterDatabase.CommitTransaction(trans);

    // The charter item has served its purpose once the neighborhood exists.
    player->DestroyItemCount(ITEM_NEIGHBORHOOD_CHARTER, 1, true);

    sendResult(HOUSING_RESULT_SUCCESS, neighborhood);

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

    // CharterGuid counter maps to charter DB ID
    uint64 charterId = neighborhoodCharterAddSignature.CharterGuid.GetCounter();

    // Only sign a charter this session was invited to sign (session-scoped, not persisted).
    if (!HasPendingCharterSignatureRequest(charterId))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    // Load charter from DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult charterResult = CharacterDatabase.Query(stmt);

    if (!charterResult)
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());

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

        return;
    }

    // Add player's signature (validates not already signed, not creator)
    if (!charter.AddSignature(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_DUPLICATE_CHARTER_SIGNATURE);
        SendPacket(response.Write());

        return;
    }

    // One invitation, one signature.
    ClearPendingCharterSignatureRequest(charterId);

    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.CharterGuid = neighborhoodCharterAddSignature.CharterGuid;
    SendPacket(response.Write());

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

    // Validate target player is online and reachable
    Player* targetPlayer = ObjectAccessor::FindPlayer(neighborhoodCharterSendSignatureRequest.TargetPlayerGuid);
    if (!targetPlayer)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_PLAYER_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());
    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!LoadNeighborhoodCharter(charterId, charter))
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    // Show the charter name/location dialog on the target's client.
    WorldPackets::Neighborhood::NeighborhoodCharterSignRequest signRequest;
    signRequest.CharterGuid = player->GetGUID();
    signRequest.MapID = charter.GetNeighborhoodMapID();
    signRequest.NeighborhoodName = charter.GetName();
    targetPlayer->SendDirectMessage(signRequest.Write());

    // Gate ADD_SIGNATURE: charter ids are enumerable creator GUID counters.
    if (WorldSession* targetSession = targetPlayer->GetSession())
        targetSession->AddPendingCharterSignatureRequest(charterId);

    // Acknowledge to the requester that the signature request was sent
    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse ackResponse;
    ackResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(ackResponse.Write());

}

// Neighborhood Management System

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

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner or manager can rename
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    // Validate name
    if (neighborhoodUpdateName.NewName.empty() || neighborhoodUpdateName.NewName.length() > HOUSING_MAX_NAME_LENGTH)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_NEIGHBORHOOD_NAME);
        SendPacket(response.Write());

        return;
    }

    if (!ObjectMgr::IsValidCharterName(neighborhoodUpdateName.NewName) || sObjectMgr->IsReservedName(neighborhoodUpdateName.NewName))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());

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

            // 12.0.5 moved the rename notification to this SMSG.
            WorldPackets::Housing::HousingSvcsNeighborhoodUpdateNameNotification nameNotification;
            nameNotification.NeighborhoodGuid = neighborhoodGuid;
            nameNotification.NewName = neighborhoodUpdateName.NewName;
            memberPlayer->SendDirectMessage(nameNotification.Write());
        }
    }

    // Invalidate realm-wide: cache holders need not be members (as InvalidatePlayer does on rename).
    WorldPackets::Housing::InvalidateNeighborhood invalidateRecord;
    invalidateRecord.NeighborhoodGuid = neighborhoodGuid;
    sWorld->SendGlobalMessage(invalidateRecord.Write());

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

}

void WorldSession::HandleNeighborhoodSetPublicFlag(WorldPackets::Neighborhood::NeighborhoodSetPublicFlag const& neighborhoodSetPublicFlag)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodSetPublicFlag.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner or manager can change visibility
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    neighborhood->SetPublic(neighborhoodSetPublicFlag.IsPublic);

    WorldPackets::Neighborhood::NeighborhoodUpdateNameResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

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

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner can add managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodAddSecondaryOwnerResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

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
        neighborhood->BroadcastMemberStatus(neighborhoodAddSecondaryOwner.PlayerGuid);

        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

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

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner can remove managers
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodRemoveSecondaryOwnerResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

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
        neighborhood->BroadcastMemberStatus(neighborhoodRemoveSecondaryOwner.PlayerGuid);

        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

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

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner or manager can invite
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

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

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner or manager can cancel invitations
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    HousingResult result = neighborhood->CancelInvitation(neighborhoodCancelInvitation.InviteeGuid);

    WorldPackets::Neighborhood::NeighborhoodCancelInvitationResponse response;
    response.Result = static_cast<uint8>(result);
    response.InviteeGuid = neighborhoodCancelInvitation.InviteeGuid;
    SendPacket(response.Write());

}

void WorldSession::HandleNeighborhoodPlayerDeclineInvite(WorldPackets::Neighborhood::NeighborhoodPlayerDeclineInvite const& neighborhoodPlayerDeclineInvite)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodPlayerDeclineInvite.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodDeclineInvitationResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    HousingResult result = neighborhood->DeclineInvitation(player->GetGUID());

    WorldPackets::Neighborhood::NeighborhoodDeclineInvitationResponse response;
    response.Result = static_cast<uint8>(result);
    response.NeighborhoodGuid = neighborhoodPlayerDeclineInvite.NeighborhoodGuid;
    SendPacket(response.Write());

}

void WorldSession::HandleNeighborhoodPlayerGetInvite(WorldPackets::Neighborhood::NeighborhoodPlayerGetInvite const& /*neighborhoodPlayerGetInvite*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Client sends empty packet - search all neighborhoods for a pending invite to this player
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

}

void WorldSession::HandleNeighborhoodGetInvites(WorldPackets::Neighborhood::NeighborhoodGetInvites const& /*neighborhoodGetInvites*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Client sends empty packet - derive neighborhood from player's housing context
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

        return;
    }

    // Only owner or manager can view all pending invites
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetInvitesResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

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
        response.Invites.push_back(std::move(entry));
    }
    SendPacket(response.Write());

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

    // CMSG contains CornerstoneGuid (not a NeighborhoodGuid) - resolve neighborhood from player's map
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodBuyHouse.CornerstoneGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // BuyHouse CMSG has no PlotIndex; use the one cached from OpenCornerstoneUI.
    uint8 resolvedPlotIndex = static_cast<uint8>(_lastClientPlotIndex);

    // Auto-join neighborhood if not already a member - buying a plot implies joining
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        // AddResident performs no faction/invite checks, so gate the auto-join here.
        int32 faction = neighborhood->GetFactionRestriction();
        if (faction != NEIGHBORHOOD_FACTION_NONE)
        {
            uint32 team = player->GetTeam();
            if ((faction == NEIGHBORHOOD_FACTION_HORDE && team != HORDE) ||
                (faction == NEIGHBORHOOD_FACTION_ALLIANCE && team != ALLIANCE))
            {
                WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
                response.Result = static_cast<uint8>(HOUSING_RESULT_INCORRECT_FACTION);
                SendPacket(response.Write());

                return;
            }
        }

        // Private neighborhoods require a pending invite unless owner/manager.
        if (!neighborhood->IsPublic()
            && !neighborhood->HasPendingInvite(player->GetGUID())
            && !neighborhood->IsManager(player->GetGUID())
            && !neighborhood->IsOwner(player->GetGUID()))
        {
            WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
            response.Result = static_cast<uint8>(HOUSING_RESULT_MISSING_PRIVATE_NEIGHBORHOOD_INVITE);
            SendPacket(response.Write());

            return;
        }

        HousingResult joinResult = neighborhood->AddResident(player->GetGUID());
        if (joinResult != HOUSING_RESULT_SUCCESS)
        {
            WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
            response.Result = static_cast<uint8>(joinResult);
            SendPacket(response.Write());

            return;
        }
    }

    // Must not already own a house in this neighborhood
    if (player->GetHousingForNeighborhood(neighborhood->GetGuid()))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_HOUSE);
        SendPacket(response.Write());

        return;
    }

    // House GUID is (NeighborhoodMapID, bnet account): a second per map would collide.
    for (Housing const* accountHousing : player->GetAllHousings())
    {
        Neighborhood const* housingNeighborhood = sNeighborhoodMgr.GetNeighborhood(accountHousing->GetNeighborhoodGuid());
        if (housingNeighborhood && housingNeighborhood->GetNeighborhoodMapID() == neighborhood->GetNeighborhoodMapID())
        {
            WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
            response.Result = static_cast<uint8>(HOUSING_RESULT_INVALID_HOUSE);
            SendPacket(response.Write());

            return;
        }
    }

    // Global per-account house cap across all neighborhoods (0 = no limit).
    if (uint32 maxHouses = sWorld->getIntConfig(CONFIG_HOUSING_MAX_HOUSES_PER_ACCOUNT))
    {
        if (player->GetAllHousings().size() >= maxHouses)
        {
            WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
            response.Result = static_cast<uint8>(HOUSING_RESULT_MORE_HOUSE_SLOTS_NEEDED);
            SendPacket(response.Write());

            return;
        }
    }

    // Price of the plot: NeighborhoodPlot.Cost from DB2.
    NeighborhoodPlotData const* boughtPlot = nullptr;
    for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID()))
    {
        if (plot->PlotIndex == resolvedPlotIndex)
        {
            boughtPlot = plot;
            break;
        }
    }
    if (!boughtPlot)
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }
    uint64 const purchaseCost = boughtPlot->Cost;

    if (!player->HasEnoughMoney(purchaseCost))
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_CANNOT_AFFORD);
        SendPacket(response.Write());

        return;
    }

    HousingResult result = neighborhood->PurchasePlot(player->GetGUID(), resolvedPlotIndex);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Consume any 5-minute reservation hold the player placed via the House Finder.
        neighborhood->ClearReservation(player->GetGUID());
        player->ModifyMoney(-static_cast<int64>(purchaseCost));
        // Use the server's canonical GUID; the client's may carry a DB2 id.
        player->CreateHousing(neighborhood->GetGuid(), resolvedPlotIndex);

        // Update the PlotInfo with the newly created HouseGuid and Battle.net account GUID
        if (Housing const* housing = player->GetHousing())
        {
            neighborhood->UpdatePlotHouseInfo(resolvedPlotIndex,
                housing->GetHouseGuid(), GetBattlenetAccountGUID());
        }

        // Grant the kill credit that satisfies quest 91863 objective 17 ("Acquire a house").
        player->KilledMonsterCredit(NPC_KILL_CREDIT_BUY_HOME);

        // TODO: Replace with quest-driven tutorial progression when the housing tutorial
        // questline is implemented. See Player.cpp LoadFromDB for full explanation.
        // Deliberately NOT marking the 256 server tutorial flags as seen here (it used to set all of them).
        // Buying a house is precisely when the housing tutorial should START, so suppressing every tutorial at
        // that moment was backwards. The client tracks its own progress via CMSG_TUTORIAL.

        player->UpdateHousingTutorialCVars();

        // Deliberately no FirstTimeDecorAcquisition packets (see 1b below).

        // 1a. Starter decor into the catalog; SourceType DEFERRED keeps the client's redeemable credit alive.
        Housing* housing = player->GetHousing();
        if (housing)
        {
            auto starterDecorWithQty = sHousingMgr.GetStarterDecorWithQuantities(player->GetTeam());
            for (auto const& [decorId, qty] : starterDecorWithQty)
            {
                for (int32 i = 0; i < qty; ++i)
                    housing->AddToCatalog(decorId, DECOR_SOURCE_DEFERRED);
            }
            // No auto-placement; the set stays in storage for the player to place.
        }

        // 1b. No FirstTimeDecorAcquisition - it would double-count the chest UI and dupe the starter set via REDEEM.
        // 2. Build buy response with HouseInfo
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        if (Housing const* h = player->GetHousing())
        {
            response.House.HouseGUID = h->GetHouseGuid();
            response.House.OwnerGUID = player->GetGUID();
            response.House.NeighborhoodGUID = neighborhood->GetGuid();
            response.House.PlotIndex = resolvedPlotIndex;
            response.House.HouseSettingFlags = h->GetSettingsFlags();
        }
        WorldPacket const* buyRespPkt = response.Write();
        SendPacket(buyRespPkt);

        // 3. Starter favor via LEVEL_FAVOR (level -1, favor = total).
        if (Housing* h = player->GetHousing())
            h->AddFavor(HOUSE_PURCHASE_STARTER_FAVOR, HOUSING_FAVOR_SOURCE_NEW_HOUSE_DECOR);

        // The buyer now has a house on a plot: the other members' rosters need it.
        neighborhood->BroadcastRoster(player->GetGUID());

        // Send guild notification for house addition
        if (Housing const* housing = player->GetHousing())
        {
            if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
            {
                WorldPackets::Housing::HousingSvcsGuildAddHouseNotification notification;
                notification.House.HouseGUID = housing->GetHouseGuid();
                notification.House.OwnerGUID = player->GetGUID();
                notification.House.PlotIndex = housing->GetPlotIndex();
                notification.House.HouseSettingFlags = housing->GetSettingsFlags();
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
            // Same fixture/root selections as AddPlayerToMap, or the new fixtures only appear after a relog.
            HousingMap::FixtureOverrideMap buyFixtureOverrides;
            HousingMap::RootOverrideMap buyRootOverrides;
            if (buyHousing)
            {
                buyFixtureOverrides = buyHousing->GetFixtureOverrideMap();
                buyRootOverrides = buyHousing->GetRootComponentOverrides();
            }
            housingMap->SpawnHouseForPlot(resolvedPlotIndex, nullptr, buyExtCompID, buyWmoDataID,
                buyFixtureOverrides.empty() ? nullptr : &buyFixtureOverrides, &buyRootOverrides);

            // MeshObjects are not delivered by grid visibility; push them by hand.
            housingMap->SendPlotMeshObjectsToPlayers(resolvedPlotIndex);

            // Plot geometry entities ride the login bundle; push them or placement validation fails until a relog.
            housingMap->SendPlotGeometryEntitiesToMap(resolvedPlotIndex);

            // Arm the editor/ownership state the plot AreaTrigger would only arm on entry.
            if (Housing* armedHousing = player->GetHousing())
            {
                player->SetCurrentHouse(armedHousing->GetHouseGuid());

                if (housingMap->GetPlayerCurrentPlot(player->GetGUID()) != resolvedPlotIndex)
                {
                    housingMap->SetPlayerCurrentPlot(player->GetGUID(), resolvedPlotIndex);
                    housingMap->SendPlotEnterSpellPackets(player, resolvedPlotIndex);
                }

                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = armedHousing->GetHouseGuid();
                statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                statusResponse.OwnerPlayerGuid = player->GetGUID();
                statusResponse.Status = 0;
                statusResponse.EditModeFlags = armedHousing->GetEditModeStatusFlags();
                player->SendDirectMessage(statusResponse.Write());

                WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                permResponse.HouseGuid = armedHousing->GetHouseGuid();
                permResponse.ResultCode = 0;
                permResponse.PermissionFlags = HOUSING_PERMISSIONS_OWNER;
                player->SendDirectMessage(permResponse.Write());

            }
        }
        else
        {
            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Player map is NOT a HousingMap - cannot spawn house exterior!");
        }

        // Notify client that the basic house was created
        if (player->GetHousing())
        {
            WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse houseResponse;
            houseResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(houseResponse.Write());
        }

        // Refresh NeighborhoodMirrorData (Houses[] changed) on all online members
        neighborhood->RefreshMirrorDataForOnlineMembers();

        // The client requests storage at map entry only; push the post-purchase state.
        if (Housing* h = player->GetHousing())
        {
            h->PopulateCatalogStorageEntries();
            h->SyncUpdateFields();

            WorldPackets::Housing::HousingDecorRequestStorageResponse storageResp;
            storageResp.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(storageResp.Write());

            // Send Account + HousingPlayerHouseEntity together so budget data accompanies storage.
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
                else if (CanSeeHousingPlayerHouseEntity())
                {
                    GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
                    player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());
                }

                updateData.BuildPacket(&updatePacket);
                player->SendDirectMessage(&updatePacket);

                GetBattlenetAccount().ClearUpdateMask(true);
                GetHousingPlayerHouseEntity().ClearUpdateMask(true);
            }

        }

        // Move-in cutscene and tutorial credit, as retail does after a purchase or a move.
        player->CastSpell(player, SPELL_HOUSING_HOUSE_ACQUIRED, true);

        // Check if neighborhoods need expansion after plot purchase
        sNeighborhoodMgr.CheckAndExpandNeighborhoods();
    }
    else
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(result);
        SendPacket(response.Write());

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

    // CornerstoneGuid is a GO GUID at the destination plot.
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodMoveHouse.CornerstoneGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Anti-spoof: reject relocating a house the player doesn't own.
    Housing* housing = player->GetHousing();
    if (!housing || housing->GetHouseGuid() != neighborhoodMoveHouse.HouseGuid)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Fall back to the cached OpenCornerstoneUI index when the destination GO no longer exists.
    int32 resolvedTarget = sHousingMgr.ResolvePlotIndex(player, neighborhoodMoveHouse.CornerstoneGuid, neighborhood);
    uint8 targetPlotIndex = (resolvedTarget >= 0)
        ? static_cast<uint8>(resolvedTarget)
        : static_cast<uint8>(_lastClientPlotIndex);

    if (targetPlotIndex == INVALID_PLOT_INDEX)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());

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

        return;
    }

    // Deduct gold cost for house move
    if (!player->HasEnoughMoney(HOUSE_MOVE_COST_COPPER))
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_CANNOT_AFFORD);
        SendPacket(response.Write());

        return;
    }

    HousingResult result = neighborhood->MoveHouse(player->GetGUID(), targetPlotIndex);

    WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
    response.Result = static_cast<uint8>(result);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Consume the House Finder reservation hold early.
        neighborhood->ClearReservation(player->GetGUID());

        // Carry the house over keeping its plot-relative spot (taken from the old plot's root entity).
        HousingMap* moveMap = dynamic_cast<HousingMap*>(player->GetMap());
        Position fromFrame, toFrame;
        bool const haveFrames = moveMap && oldPlotIndex != INVALID_PLOT_INDEX
            && moveMap->GetPlotRoomFrame(oldPlotIndex, fromFrame) && moveMap->GetPlotRoomFrame(targetPlotIndex, toFrame);
        Optional<Position> movedHousePos;
        if (haveFrames)
        {
            if (HousingRoomEntity const* oldRoot = moveMap->GetHouseRootEntity(oldPlotIndex))
            {
                Position const local = HousingWorldToRoomLocal(fromFrame, oldRoot->GetPosition());
                float const cosTo = std::cos(toFrame.GetOrientation());
                float const sinTo = std::sin(toFrame.GetOrientation());
                movedHousePos.emplace(
                    toFrame.GetPositionX() + local.GetPositionX() * cosTo - local.GetPositionY() * sinTo,
                    toFrame.GetPositionY() + local.GetPositionX() * sinTo + local.GetPositionY() * cosTo,
                    toFrame.GetPositionZ() + local.GetPositionZ(),
                    Position::NormalizeOrientation(toFrame.GetOrientation() + oldRoot->GetOrientation() - fromFrame.GetOrientation()));
            }
        }

        // Update Housing::_plotIndex so subsequent responses use the correct DB2 PlotIndex.
        if (Housing* housing = player->GetHousing())
        {
            housing->SetPlotIndex(targetPlotIndex);
            // The old plot's world coordinates mean nothing on the new one: keep the plot-relative spot instead.
            if (movedHousePos)
                housing->SetHousePosition(movedHousePos->GetPositionX(), movedHousePos->GetPositionY(),
                    movedHousePos->GetPositionZ(), movedHousePos->GetOrientation());
            else
                housing->ResetHousePosition();
            housing->SyncUpdateFields();
            // Re-push the Housing/3 entity as CREATE: its plot-icon chooser only re-evaluates on CREATE_OBJECT.
            if (CanSeeHousingPlayerHouseEntity())
                GetHousingPlayerHouseEntity().SendCreateToPlayer(player);
        }

        player->ModifyMoney(-static_cast<int64>(HOUSE_MOVE_COST_COPPER));

        // Despawn entities at old plot, respawn at new plot
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            // The yard decor moves with the house, keeping its place on the plot.
            if (Housing* movedHousing = player->GetHousing())
                if (haveFrames)
                    movedHousing->RelocateExteriorDecor(fromFrame, toFrame);

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
                auto rootOverrides = h->GetRootComponentOverrides();
                Position const housePos = h->GetHousePosition();
                housingMap->SpawnHouseForPlot(targetPlotIndex, h->HasCustomPosition() ? &housePos : nullptr,
                    static_cast<int32>(h->GetCoreExteriorComponentID()),
                    static_cast<int32>(h->GetHouseType()),
                    fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
                    rootOverrides.empty() ? nullptr : &rootOverrides);

                // Despawn above only removed world entities; re-spawn the PlacedDecor records (decor follows the house).
                housingMap->SpawnAllDecorForPlot(targetPlotIndex, h);

                // Same delivery gap as purchase: destination geometry must reach the client.
                housingMap->SendPlotGeometryEntitiesToMap(targetPlotIndex);

                // Re-arm the editor/ownership state for the new plot.
                player->SetCurrentHouse(h->GetHouseGuid());
                if (housingMap->GetPlayerCurrentPlot(player->GetGUID()) != targetPlotIndex)
                {
                    housingMap->SetPlayerCurrentPlot(player->GetGUID(), targetPlotIndex);
                    housingMap->SendPlotEnterSpellPackets(player, targetPlotIndex);
                }

                WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
                statusResponse.HouseGuid = h->GetHouseGuid();
                statusResponse.AccountGuid = player->GetSession()->GetBattlenetAccountGUID();
                statusResponse.OwnerPlayerGuid = player->GetGUID();
                statusResponse.Status = 0;
                statusResponse.EditModeFlags = h->GetEditModeStatusFlags();
                player->SendDirectMessage(statusResponse.Write());

                WorldPackets::Housing::HousingGetPlayerPermissionsResponse permResponse;
                permResponse.HouseGuid = h->GetHouseGuid();
                permResponse.ResultCode = 0;
                permResponse.PermissionFlags = HOUSING_PERMISSIONS_OWNER;
                player->SendDirectMessage(permResponse.Write());
            }
            else
            {
                TC_LOG_ERROR("housing", "HandleNeighborhoodMoveHouse: No Housing object for player - cannot spawn house at plot {}", targetPlotIndex);
            }
        }

        if (Housing const* housing = player->GetHousing())
        {
            response.House.HouseGUID = housing->GetHouseGuid();
            response.House.OwnerGUID = player->GetGUID();
            response.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
            response.House.PlotIndex = housing->GetPlotIndex();
            response.House.HouseSettingFlags = housing->GetSettingsFlags();
        }

        // The house moved to another plot: the other members' rosters need the new plot.
        neighborhood->BroadcastRoster(player->GetGUID());

        // Refresh NeighborhoodMirrorData (Houses[] changed - plot moved)
        neighborhood->RefreshMirrorDataForOnlineMembers();
    }
    // MoveTransactionGuid echoes the moved HouseGuid so the client correlates the response.
    response.MoveTransactionGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    // Move-in cutscene and tutorial credit.
    if (result == HOUSING_RESULT_SUCCESS)
        player->CastSpell(player, SPELL_HOUSING_HOUSE_ACQUIRED, true);

}

void WorldSession::HandleNeighborhoodOpenCornerstoneUI(WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUI const& neighborhoodOpenCornerstoneUI)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodOpenCornerstoneUI.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.PlotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
        SendPacket(response.Write());
        return;
    }

    // Client PlotIndex may differ from our DB2 values; cache it for the BuyHouse CMSG.
    uint32 plotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
    _lastClientPlotIndex = plotIndex;

    // Also resolve via cornerstone GO entry for cost lookup (uses our DB2 internal index)
    int32 resolved = sHousingMgr.ResolvePlotIndex(player, neighborhoodOpenCornerstoneUI.NeighborhoodGuid, neighborhood);

    // Look up cost from plot data - try both the client's PlotIndex and our DB2 PlotIndex
    uint32 neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
    std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);

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
            }
        }
    }

    if (!plotFound)
    {
        TC_LOG_ERROR("housing", "HandleNeighborhoodOpenCornerstoneUI: PlotIndex {} (DB2: {}) not found in neighborhood map {}",
            plotIndex, resolved, neighborhoodMapId);
        WorldPackets::Neighborhood::NeighborhoodOpenCornerstoneUIResponse response;
        response.PlotIndex = plotIndex;
        response.NeighborhoodName = neighborhood->GetName();
        SendPacket(response.Write());
        return;
    }

    // Pre-send the name response so JamCliNeighborhoodName exists in the DataCache.
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

    // Reserved plot: PurchaseStatus=HOUSING_RESULT_PLOT_RESERVED(73); available: 0 with the plot cost.
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

        // Another player's hold marks the plot reserved; the holder themselves still sees 0.
        ObjectGuid otherReserver = neighborhood->GetPlotReserverOther(plotIdx, player->GetGUID());
        if (!otherReserver.IsEmpty())
        {
            response.PurchaseStatus = static_cast<uint8>(HOUSING_RESULT_PLOT_RESERVED);
        }
    }

    // Embed the player's existing house here so the cornerstone Lua flips Buy into Move.
    if (!isOwned && response.PurchaseStatus == 0)
    {
        if (Housing const* myHousing = player->GetHousingForNeighborhood(neighborhood->GetGuid()))
        {
            WorldPackets::Housing::JamCliHouse existingHouse;
            existingHouse.HouseGUID = myHousing->GetHouseGuid();
            existingHouse.OwnerGUID = player->GetGUID();
            existingHouse.NeighborhoodGUID = myHousing->GetNeighborhoodGuid();
            existingHouse.PlotIndex = myHousing->GetPlotIndex();
            existingHouse.HouseSettingFlags = myHousing->GetSettingsFlags();
            existingHouse.HasOptionalField = false;
            response.ExistingHouse = std::move(existingHouse);
        }
    }
    WorldPacket const* pkt = response.Write();
    SendPacket(pkt);

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

    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Only owner can transfer ownership
    if (!neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodOfferOwnershipResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

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

}

void WorldSession::HandleNeighborhoodGetRoster(WorldPackets::Neighborhood::NeighborhoodGetRoster const& neighborhoodGetRoster)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // ResolveNeighborhood handles both Housing GUIDs and bulletin board GO GUIDs
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodGetRoster.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Must be a member to view roster
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    WorldPackets::Neighborhood::NeighborhoodGetRosterResponse response;
    neighborhood->BuildRosterResponse(response);

    // Pre-send the name response; the roster UI resolves the name via the DataCache.
    {
        WorldPackets::Housing::QueryNeighborhoodNameResponse nameResp;
        nameResp.NeighborhoodGuid = neighborhood->GetGuid();
        nameResp.Result = true;
        nameResp.NeighborhoodName = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    WorldPacket const* rosterPkt = response.Write();
    SendPacket(rosterPkt);

    // Refresh the Housing/4 mirror, but only for the neighborhood the player stands in.
    HousingMap const* housingMap = dynamic_cast<HousingMap const*>(player->GetMap());
    bool const refreshMirror = housingMap && housingMap->GetNeighborhood() == neighborhood;
    HousingNeighborhoodMirrorEntity& mirrorEntity = GetHousingNeighborhoodMirrorEntity();
    if (refreshMirror)
    {
        mirrorEntity.SetName(neighborhood->GetName());
        mirrorEntity.SetOwnerGUID(neighborhood->GetClientOwnerGuid());

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
        // Wholesale CREATE re-push; the client's map-icon refresh only fires on CREATE.
        mirrorEntity.SendCreateToPlayer(player);
    }

    // Pre-push plot-owner names so plot names format without async name queries.
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
        }
    }

}

void WorldSession::SendNeighborhoodMapRefresh()
{
    Player* player = GetPlayer();
    if (!player || !HasHousingNeighborhoodMirrorEntity())
        return;

    // Mirror entity GUID is the neighborhood GUID; empty = no neighborhood yet.
    HousingNeighborhoodMirrorEntity& mirrorEntity = GetHousingNeighborhoodMirrorEntity();
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(mirrorEntity.GetGUID());
    if (!neighborhood)
        return;

    // Keep JamCliNeighborhoodName fed; a missing entry drops the pin label prefix.
    {
        WorldPackets::Housing::QueryNeighborhoodNameResponse nameResp;
        nameResp.NeighborhoodGuid = neighborhood->GetGuid();
        nameResp.Result = true;
        nameResp.NeighborhoodName = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    // The HousingNeighborhoodState singleton is only filled by the roster response; push it on re-entry.
    if (neighborhood->GetMember(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse rosterResponse;
        neighborhood->BuildRosterResponse(rosterResponse);
        SendPacket(rosterResponse.Write());
    }

    // VALUES when the client already holds the entity, CREATE otherwise (a blind CREATE resets Houses).
    neighborhood->RebuildMirrorDataFor(player);
    if (player->HaveAtClient(&mirrorEntity))
        mirrorEntity.SendUpdateToPlayer(player);
    else
        mirrorEntity.SendCreateToPlayer(player);

    // Pre-push plot-owner names so ownership icons resolve without async queries.
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
            SendPacket(nameResponse.Write());
    }

}

void WorldSession::HandleNeighborhoodEvictPlot(WorldPackets::Neighborhood::NeighborhoodEvictPlot const& neighborhoodEvictPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodEvictPlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Client sends its internal plot ID, which may differ from our DB2 values.
    uint32 plotIndex = neighborhoodEvictPlot.PlotIndex;

    // Only owner or manager can evict
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    // Find the plot by index - O(1) direct array access
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
                // Offline: Housing::DeleteFromDB clears the housing tables and their children.
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                Housing::DeleteFromDB(evictedPlayerGuid.GetCounter(), trans);
                CharacterDatabase.CommitTransaction(trans);
            }
        }

        // Neighborhood::EvictPlayer already sent the remaining members the new roster.

        // Eviction notice triggers the clients' neighborhood-view refresh; send to remaining members and the evictee.
        if (!evictedPlayerGuid.IsEmpty())
        {
            WorldPackets::Neighborhood::NeighborhoodEvictPlayerResponse evictNotification;
            evictNotification.PlayerGuid = evictedPlayerGuid;
            WorldPacket const* evictPkt = evictNotification.Write();

            neighborhood->BroadcastPacket(evictPkt);
            if (Player* evictedPlayer = ObjectAccessor::FindPlayer(evictedPlayerGuid))
                evictedPlayer->SendDirectMessage(evictPkt);
        }

        // Refresh NeighborhoodMirrorData (Houses[] changed)
        neighborhood->RefreshMirrorDataForOnlineMembers();
    }

}

// Neighborhood Initiative System

void WorldSession::HandleNeighborhoodInitiativeServiceStatusCheck(WorldPackets::Neighborhood::NeighborhoodInitiativeServiceStatusCheck const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // This CMSG's only response.
    sInitiativeManager.SendInitiativeServiceStatus(this, true);

    // No unsolicited info push; the client asks via CMSG_GET_AVAILABLE_INITIATIVE_REQUEST.
}

void WorldSession::HandleGetAvailableInitiativeRequest(WorldPackets::Neighborhood::GetAvailableInitiativeRequest const& getAvailableInitiativeRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(getAvailableInitiativeRequest.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        // Empty response = Flags top-2-bits 0, the client's no-data path.
        WorldPackets::Housing::GetPlayerInitiativeInfoResult response;
        response.NeighborhoodGUID = getAvailableInitiativeRequest.NeighborhoodGuid;
        SendPacket(response.Write());
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();
    sInitiativeManager.SendPlayerInitiativeInfo(this, nhObjGuid, nhGuid);

}

void WorldSession::HandleGetInitiativeActivityLogRequest(WorldPackets::Neighborhood::GetInitiativeActivityLogRequest const& getInitiativeActivityLogRequest)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(getInitiativeActivityLogRequest.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        // Empty log; real failures route via SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE.
        WorldPackets::Housing::GetInitiativeActivityLogResult response;
        response.NeighborhoodGuid = getInitiativeActivityLogRequest.NeighborhoodGuid;
        SendPacket(response.Write());
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();
    sInitiativeManager.SendActivityLog(this, nhObjGuid, nhGuid);

}

void WorldSession::HandleInitiativeUpdateActiveNeighborhood(WorldPackets::Neighborhood::InitiativeUpdateActiveNeighborhood const& initiativeUpdateActiveNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(initiativeUpdateActiveNeighborhood.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        return;
    }

    ObjectGuid nhObjGuid = neighborhood->GetGuid();
    uint64 nhGuid = nhObjGuid.GetCounter();

    // Send initiative service status to confirm the service is active
    sInitiativeManager.SendInitiativeServiceStatus(this, true);

    // Send current initiative info for the active neighborhood (with real task progress)
    sInitiativeManager.SendPlayerInitiativeInfo(this, nhObjGuid, nhGuid);

}
