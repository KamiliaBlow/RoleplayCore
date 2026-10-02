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
#include "HousingRoomEntity.h"
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

// ============================================================
// Neighborhood Charter System
// ============================================================

void WorldSession::HandleNeighborhoodCharterOpenConfirmationUI(WorldPackets::Neighborhood::NeighborhoodCharterOpenConfirmationUI const& /*neighborhoodCharterOpenConfirmationUI*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Sent after the steward's "found the neighborhood" gossip option (OptionNpc 63). The reply feeds
    // OPEN_CHARTER_CONFIRMATION_UI(neighborhoodName, locationName); its Confirm button sends
    // CMSG_NEIGHBORHOOD_CHARTER_FINALIZE.
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

    // SMSG_NEIGHBORHOOD_CHARTER_OPEN_UI_RESPONSE carries the same body as the create reply:
    // everything the charter panel renders (12.1.0.69933: sent when the charter item is used).
    uint64 charterId = static_cast<uint64>(player->GetGUID().GetCounter());

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult charterResult = CharacterDatabase.Query(stmt);
    if (!charterResult)
    {
        return;
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_NEIGHBORHOOD_CHARTER_SIGNATURES);
    stmt->setUInt64(0, charterId);
    PreparedQueryResult sigResult = CharacterDatabase.Query(stmt);

    WorldPackets::Neighborhood::NeighborhoodCharterOpenUIResponse openUI;
    NeighborhoodCharter charter(charterId, ObjectGuid::Empty);
    if (!charter.LoadFromDB(charterResult, sigResult))
    {
        // A charter row exists but will not load ? report the failure instead of a blank panel.
        openUI.Result = static_cast<uint8>(HOUSING_RESULT_DB_ERROR);
        SendPacket(openUI.Write());

        TC_LOG_ERROR("housing", "SendNeighborhoodCharterOpenUI: charter {} exists but failed to load for player {}",
            charterId, player->GetGUID().ToString());
        return;
    }

    openUI.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    openUI.CharterGuid = player->GetGUID();
    openUI.MapID = charter.GetNeighborhoodMapID();
    openUI.SignatureCount = charter.GetSignatureCount() + 1; // the creator counts (retail: 1 with no co-signers)
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

    // H-17: the creator does NOT count toward the required signatures. This used to
    // call AddSignature(player->GetGUID()) under a "Creator auto-signs" comment, but
    // AddSignature opens with a self-sign guard and returns false, so the call always
    // failed and its result was discarded - the comment described behaviour that never
    // happened. Stating the rule instead of pretending; whether the creator should
    // count is a design decision, and the code now matches whichever way it is read
    // today rather than claiming the opposite.

    // Persist to DB
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    charter.SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    // Retail 12.1.0.69933 (sniff 11-13-10): the charter GUID is the creator's player GUID and
    // `Unknown` is the required signature count (10).
    WorldPackets::Neighborhood::NeighborhoodCharterUpdateResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.CharterGuid = player->GetGUID();
    response.MapID = neighborhoodCharterCreate.NeighborhoodMapID;
    response.SignatureCount = charter.GetSignatureCount() + 1; // the creator counts (retail: 1 with no co-signers)
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

    // Capture the signatures the edit is about to discard. DeleteFromDB drops every signature
    // row and the re-save only re-adds the creator's, so every co-signer silently loses their
    // signature here. Their clients still believe they have signed this charter until told
    // otherwise ? that notification is SMSG_NEIGHBORHOOD_CHARTER_SIGNATURE_REMOVED (0x5B0005).
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

    // H-17: the creator does NOT count toward the required signatures. This used to
    // call AddSignature(player->GetGUID()) under a "Creator auto-signs" comment, but
    // AddSignature opens with a self-sign guard and returns false, so the call always
    // failed and its result was discarded - the comment described behaviour that never
    // happened. Stating the rule instead of pretending; whether the creator should
    // count is a design decision, and the code now matches whichever way it is read
    // today rather than claiming the opposite.

    // Re-persist
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    NeighborhoodCharter::DeleteFromDB(charterId, trans);
    charter.SaveToDB(trans);
    CharacterDatabase.CommitTransaction(trans);

    // Tell every co-signer whose signature the edit just wiped, so their charter panel drops
    // the stale "signed" state instead of holding it until relog.
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
    response.SignatureCount = charter.GetSignatureCount() + 1; // the creator counts (retail: 1 with no co-signers)
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

    // The confirmation frame listens for CREATE_NEIGHBORHOOD_RESULT(result, neighborhoodName), which comes
    // from SMSG_HOUSING_SVCS_CREATE_CHARTER_NEIGHBORHOOD_RESPONSE; a failure shows HousingResultToErrorText.
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

    // Founding fee (Housing.CharterFoundingCost, in copper). Retail charges nothing
    // beyond the plot price, so the default keeps charter founding free.
    uint32 const foundingCost = sWorld->getIntConfig(CONFIG_HOUSING_CHARTER_FOUNDING_COST);
    if (foundingCost && !player->HasEnoughMoney(uint64(foundingCost)))
    {
        sendResult(HOUSING_RESULT_CANNOT_AFFORD);
        return;
    }

    // The charter's FactionFlags come from the client (1 for an Alliance charter in the 11-13-10 sniff) and are
    // not a NeighborhoodFactionRestriction (1 = Horde), so restrict by the founder's faction like guild neighborhoods.
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

    // H-25: only sign a charter this session was invited to sign. Session-scoped, so a
    // relog means the requester has to ask again - a signature request is an
    // in-the-moment offer, and nothing about it is persisted.
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

    // H-25: one invitation, one signature. Without consuming it, a signer whose
    // signature is later dropped by a charter edit could re-sign unasked.
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

    // Send signature request notification to the target player's client; the dialog shows the
    // charter's name and location (OPEN_NEIGHBORHOOD_CHARTER_SIGNATURE_REQUEST).
    WorldPackets::Neighborhood::NeighborhoodCharterSignRequest signRequest;
    signRequest.CharterGuid = player->GetGUID();
    signRequest.MapID = charter.GetNeighborhoodMapID();
    signRequest.NeighborhoodName = charter.GetName();
    targetPlayer->SendDirectMessage(signRequest.Write());

    // H-25: record that this player was actually asked. ADD_SIGNATURE takes the charter
    // id straight from the client and charter ids are creator GUID counters, so without
    // this the invite step is decorative and any charter can be signed by anyone who
    // enumerates ids.
    if (WorldSession* targetSession = targetPlayer->GetSession())
        targetSession->AddPendingCharterSignatureRequest(charterId);

    // Acknowledge to the requester that the signature request was sent
    WorldPackets::Neighborhood::NeighborhoodCharterAddSignatureResponse ackResponse;
    ackResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(ackResponse.Write());

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

            // 12.0.5: moved from SMSG_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION (0x5C0004)
            // to SMSG_HOUSING_SVCS_NEIGHBORHOOD_UPDATE_NAME_NOTIFICATION (0x540023).
            // IDA-verified wire (sub_7FF75C1EA710 case 0x540023): ObjectGuid + string.
            WorldPackets::Housing::HousingSvcsNeighborhoodUpdateNameNotification nameNotification;
            nameNotification.NeighborhoodGuid = neighborhoodGuid;
            nameNotification.NewName = neighborhoodUpdateName.NewName;
            memberPlayer->SendDirectMessage(nameNotification.Write());
        }
    }

    // SMSG_INVALIDATE_NEIGHBORHOOD (0x5F0008) is the neighborhood twin of SMSG_INVALIDATE_PLAYER
    // (0x5F0007): the 12.0.7 dispatcher handles both in the same switch with the same shape ?
    // read one PackedGUID, then call a registered nullary C++ callback (no Lua event). It is a
    // pure "drop your cached record for this GUID" signal, so it must reach cache holders who are
    // NOT members (house-finder browsers, visitors), not just the members handled above.
    // Sent realm-wide exactly as CharacterCache::UpdateCharacterData sends InvalidatePlayer on a
    // character rename; the client's follow-up CMSG_QUERY_NEIGHBORHOOD_INFO is already answered
    // by HandleQueryNeighborhoodInfo. Renames are rare and explicitly operator-driven, so this
    // does not put the realm-wide send on a hot path.
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

    // Client sends empty packet ? search all neighborhoods for a pending invite to this player
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

    // Client sends empty packet ? derive neighborhood from player's housing context
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
        response.Invites.push_back(entry);
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

    // CMSG contains CornerstoneGuid (not a NeighborhoodGuid) ? resolve neighborhood from player's map
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodBuyHouse.CornerstoneGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodBuyHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Use the client's PlotIndex cached from OpenCornerstoneUI.
    // The BuyHouse CMSG doesn't include a PlotIndex field, so we rely on the
    // previous OpenCornerstoneUI interaction which cached _lastClientPlotIndex.
    // Validate by checking the cornerstone GUID matches what we cached.
    uint8 resolvedPlotIndex = static_cast<uint8>(_lastClientPlotIndex);

    // Auto-join neighborhood if not already a member ? buying a plot implies joining
    if (!neighborhood->IsMember(player->GetGUID()))
    {
        // M9/A2: enforce faction restriction + private-neighborhood invite gating
        // BEFORE auto-join. AddResident itself performs no such checks (unlike
        // InviteResident), so a wrong-faction or uninvited player could otherwise
        // join a private/faction-locked neighborhood simply by buying a plot.
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

        // Private (non-public) neighborhoods require a matching pending invite,
        // unless the player is already an owner/manager of that neighborhood.
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

    // One house per NeighborhoodMap per account: the house GUID is (NeighborhoodMapID, Battle.net account),
    // so a second one would collide (retail: one Alliance and one Horde house per account).
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

    // m2/A5: enforce a configurable global house cap across ALL neighborhoods
    // (retail allows 2 per account ? one Alliance hub, one Horde hub). 0 = no
    // limit. Prevents plot hoarding across the realm.
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

    // Price of the plot: NeighborhoodPlot.Cost of the DB2 (10,000,000 copper on every 12.1.0.69933 plot).
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
        // Use the server's canonical neighborhood GUID, NOT the client-supplied GUID.
        // Client may send DB2 NeighborhoodID as counter while server uses internal counter.
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

        // Retail purchase sequence: (FirstTimeDecorAcquisition xN) ? BuyHouseResponse ? LevelFavor.
        // We deliberately skip the FirstTime packets — see 1b below for why.

        // 1a. Populate the server-side decor catalog with starter items (so edit mode works).
        // SourceType MUST be DEFERRED: the client counts its HouseDecor.StartingQuantity as
        // "remaining redeemable" (Blizzard_HousingCatalogUtil.GetEntryNumStored) and only a
        // storage entry flagged like a materialized auto-award retires that credit. Retail
        // storage rows for starter decor carry SourceType 3 (dump 69933 2026-10-01, decor
        // 15454 [73]); rows with SourceType 0 left the credit alive and showed phantom
        // chest entries that could never be placed (REDEEM_DEFERRED answered 27 forever).
        Housing* housing = player->GetHousing();
        if (housing)
        {
            auto starterDecorWithQty = sHousingMgr.GetStarterDecorWithQuantities(player->GetTeam());
            for (auto const& [decorId, qty] : starterDecorWithQty)
            {
                for (int32 i = 0; i < qty; ++i)
                    housing->AddToCatalog(decorId, DECOR_SOURCE_DEFERRED);
            }
            // NO auto-placement of the starter set. The old PlaceStarterDecor put items at
            // 12.0.1 sniffed room-local coordinates whose item identities no longer match
            // 69933 (1435 was a painting then, is a hearth now — it floated at painting
            // height; 726 sank into the floor) and retail 12.1.0.69933 pre-places NOTHING
            // from the grant (14 redeemable credits, zero placed entries, empty house).
            // The set stays in storage; the player places what they want via the editor.
        }

        // 1b. NO FirstTimeDecorAcquisition here. In the retail 12.1.0.69933 purchase capture the
        // starter set arrives as 14 of those packets (1700x4, 2549x4, 81x2, 10952x2, 8906x2) and
        // NOTHING else — because the retail client credits each packet as a lazily-instantiated
        // "redeemable" (Blizzard_HousingCatalogUtil.GetEntryNumStored = totalNumStored +
        // remainingRedeemable), later materialized by CMSG_HOUSING_DECOR_REDEEM_DEFERRED_DECOR.
        // Our model is instance-based instead: the copies already exist as FHousingStorage_C
        // entries built by 1a. Sending the notifications on top double-counts every unit in the
        // chest UI (15454 showed x2 against one DB row), and the phantom "redeemable" then makes
        // the client REDEEM it — which our handler grants as a brand-new catalog copy: a x2 dupe
        // of the whole starter set. Pick ONE model; this server picked instances.
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

        // 3. Starter favor. Retail 12.1.0.69933 sends one LEVEL_FAVOR packet: level -1 (unchanged), favor = total.
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
            // Same fixture/root selections HousingMap::AddPlayerToMap spawns with - without them the
            // fixtures Housing::Create just gave the house (porch etc.) only appeared after a relog.
            HousingMap::FixtureOverrideMap buyFixtureOverrides;
            HousingMap::RootOverrideMap buyRootOverrides;
            if (buyHousing)
            {
                buyFixtureOverrides = buyHousing->GetFixtureOverrideMap();
                buyRootOverrides = buyHousing->GetRootComponentOverrides();
            }
            housingMap->SpawnHouseForPlot(resolvedPlotIndex, nullptr, buyExtCompID, buyWmoDataID,
                buyFixtureOverrides.empty() ? nullptr : &buyFixtureOverrides, &buyRootOverrides);

            // The house is made of MeshObjects, which ordinary grid visibility does NOT deliver -
            // every other site in this system transmits them by hand. SpawnHouseForPlot sends
            // nothing, so a house bought while the buyer is standing on the plot existed only on
            // the server: the cornerstone flipped to owned (a GameObject, sent normally) and no
            // house appeared until the player re-entered the map and AddPlayerToMap pushed them.
            housingMap->SendPlotMeshObjectsToPlayers(resolvedPlotIndex);

            // The plot geometry entities (room identity, Geobox mesh, exterior root, Group B mirrors)
            // ride the login bundle only ? without them the client's placement validation rejected
            // every house move on the new plot ("cannot place outside the plot") until a relog.
            housingMap->SendPlotGeometryEntitiesToMap(resolvedPlotIndex);

            // Arm the editor/ownership state the plot AreaTrigger would only arm on walking into
            // its circle (the buyer stands at the plot edge after the purchase).
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
            TC_LOG_ERROR("housing", "HandleNeighborhoodBuyHouse: Player map is NOT a HousingMap ? cannot spawn house exterior!");
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

    // Use cornerstone GO GUID to find the neighborhood and destination plot.
    // Per IDA TryMoveHouse (0x7FF75CC59CA1), the first PackedGUID is validated
    // to be HighGuid::GameObject ? i.e., a cornerstone GO at the destination plot.
    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodMoveHouse.CornerstoneGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Validate that the HouseGuid in the CMSG matches the player's owned house ?
    // anti-spoof: a malicious client could try to relocate someone else's house.
    Housing* housing = player->GetHousing();
    if (!housing || housing->GetHouseGuid() != neighborhoodMoveHouse.HouseGuid)
    {
        WorldPackets::Neighborhood::NeighborhoodMoveHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // Resolve destination plot index from the cornerstone GO GUID. Falls back to
    // the cached _lastClientPlotIndex if the cornerstone resolve misses (covers
    // the OPEN_CORNERSTONE_UI ? MOVE_HOUSE flow where the GO no longer exists
    // on the destination plot, e.g. just-bought plots).
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
        // The 5-minute hold the player may have placed via the House Finder is
        // consumed by the actual move; clear it so the lock is released early.
        neighborhood->ClearReservation(player->GetGUID());

        // Retail (12.1.0.69933 sniff) carries the house over keeping its place on the plot: after the move the root
        // Entity has the same plot-relative transform as before. Taken from the root on the old plot (it knows where
        // the house actually stands, moved or not); the DB2 default spot of some plots lies outside the plot.
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

        // Update Housing::_plotIndex so all subsequent responses (HouseStatus,
        // HouseInfo, SyncUpdateFields, etc.) use the correct DB2 PlotIndex.
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
            // Push the Housing/3 entity (HousingPlayerHouseEntity) to the client
            // as CREATE ? the regular world-map plot icon resolves via entity
            // registry lookup of HouseGUID and the icon "self/friend/stranger"
            // chooser only re-evaluates when CREATE_OBJECT arrives. Without an
            // explicit re-push here, SyncUpdateFields just flips dirty bits and
            // the client's local entity copy stays at the old PlotIndex ?
            // the new plot's icon stays "unowned" until the player re-logs.
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

                // Re-spawn the player's exterior decor at the new plot. DespawnAllDecorForPlot
                // (called above for the old plot) only removes the in-world entities ? the
                // PlacedDecor records in the Housing object are preserved. Without this
                // matching SpawnAllDecorForPlot the decor stays gone after a move.
                // Decor is NOT returned to the chest; it follows the house.
                housingMap->SpawnAllDecorForPlot(targetPlotIndex, h);

                // Same relog-only delivery gap as a plot purchase: the destination plot's
                // geometry entities must reach the client or its placement validation keeps
                // checking against the old plot ("cannot place outside the plot").
                housingMap->SendPlotGeometryEntitiesToMap(targetPlotIndex);

                // Re-arm the editor/ownership state for the new plot (the AreaTrigger only
                // arms it on walking into its circle).
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
                TC_LOG_ERROR("housing", "HandleNeighborhoodMoveHouse: No Housing object for player ? cannot spawn house at plot {}", targetPlotIndex);
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

        // Refresh NeighborhoodMirrorData (Houses[] changed ? plot moved)
        neighborhood->RefreshMirrorDataForOnlineMembers();
    }
    // Sniff-verified (12.0.5 packet #13402, 40-byte SMSG_NEIGHBORHOOD_MOVE_HOUSE_RESPONSE):
    // the trailing PackedGUID is a copy of the moved house's HouseGuid, not an empty
    // transaction GUID. Re-emitting the same HouseGuid lets the client correlate the
    // response with its locally-tracked move-in-progress entry.
    response.MoveTransactionGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    // Move-in cutscene and tutorial credit (retail casts this right after the response).
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

    // Use the client's PlotIndex directly ? it may differ from our DB2 PlotIndex
    // values (our SQL has sequential 0-54; the client's actual DB2 may differ).
    // Also cache for the subsequent BuyHouse CMSG which doesn't include PlotIndex.
    uint32 plotIndex = neighborhoodOpenCornerstoneUI.PlotIndex;
    _lastClientPlotIndex = plotIndex;

    // Also resolve via cornerstone GO entry for cost lookup (uses our DB2 internal index)
    int32 resolved = sHousingMgr.ResolvePlotIndex(player, neighborhoodOpenCornerstoneUI.NeighborhoodGuid, neighborhood);

    // Look up cost from plot data ? try both the client's PlotIndex and our DB2 PlotIndex
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

    // Build cornerstone UI response ? wire format verified against retail 12.0.1 build 65940.
    // Horde retail sniff shows two patterns:
    //   Packet 1 (PlotIndex=37): PurchaseStatus=73 (PlotReserved), Cost=10M ? actively reserved plot
    //   Packet 2 (PlotIndex=54): PurchaseStatus=0, Cost=10M, HasAlternatePrice ? available plot
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
        }
    }

    // If the player already owns a house in this neighborhood, embed it in the
    // response. The cornerstone Lua reads this as "you have a house here" and
    // flips the action button from Buy to Move. Without this the button stays
    // on Buy, which then routes to BUY_HOUSE and gets rejected by HandleNeighborhoodBuyHouse
    // (HOUSING_RESULT_INVALID_HOUSE ? "player already has a house in neighborhood").
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
    // client's internal house list stays in sync for plot resolution. The mirror is the neighborhood the
    // player stands in (the map pins come from it), so a roster of another one leaves it alone.
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
        // Wholesale re-push (ClearHouses + 55 AddHouse + ClearManagers + AddManagers).
        // Retail emits CREATE_OBJECT here (sniff-verified). The client's map-icon
        // refresh path only fires on CREATE.
        mirrorEntity.SendCreateToPlayer(player);
    }

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
        }
    }

}

void WorldSession::SendNeighborhoodMapRefresh()
{
    Player* player = GetPlayer();
    if (!player || !HasHousingNeighborhoodMirrorEntity())
        return;

    // The mirror entity's GUID is the neighborhood GUID (assigned at login by
    // Player::LoadFromDB); empty means the session has no neighborhood yet.
    HousingNeighborhoodMirrorEntity& mirrorEntity = GetHousingNeighborhoodMirrorEntity();
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(mirrorEntity.GetGUID());
    if (!neighborhood)
        return;

    // Keep the JamCliNeighborhoodName DataCache fed ? the map pin label resolves the
    // neighborhood name from it, and a missing entry drops the name prefix entirely.
    {
        WorldPackets::Housing::QueryNeighborhoodNameResponse nameResp;
        nameResp.NeighborhoodGuid = neighborhood->GetGuid();
        nameResp.Result = true;
        nameResp.NeighborhoodName = neighborhood->GetName();
        SendPacket(nameResp.Write());
    }

    // Re-send the roster. The HousingNeighborhoodState singleton (members, plotIDs, the
    // ownership classification and the neighborhood name the pin labels format from) is
    // ONLY filled by the roster response. After a relog the client requests it itself, but
    // on a mid-session map re-entry it does not re-ask while its singleton starts empty ?
    // the pins then render bare plot numbers ("  49") with no ownership detected.
    if (neighborhood->GetMember(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodGetRosterResponse rosterResponse;
        neighborhood->BuildRosterResponse(rosterResponse);
        SendPacket(rosterResponse.Write());
    }

    // Re-push the Housing/4 mirror with freshly rebuilt fields. The client retains session
    // entities across map switches, so a blind CREATE here is a duplicate for a held GUID ?
    // the documented client behavior for that is resetting/ignoring the dynamic Houses array,
    // which is exactly the lost-pins state. VALUES when held, CREATE when not (the same gate
    // BuildHousingAccountEntitiesUpdate uses for the Account/Housing/3 pair).
    neighborhood->RebuildMirrorDataFor(player);
    if (player->HaveAtClient(&mirrorEntity))
        mirrorEntity.SendUpdateToPlayer(player);
    else
        mirrorEntity.SendCreateToPlayer(player);

    // Pre-push plot-owner names so ownership icons resolve without async name queries.
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

    // Use the client's PlotIndex directly ? client sends its internal plot ID
    // which may differ from our DB2 PlotIndex values
    uint32 plotIndex = neighborhoodEvictPlot.PlotIndex;

    // Only owner or manager can evict
    if (!neighborhood->IsOwner(player->GetGUID()) && !neighborhood->IsManager(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodEvictPlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        SendPacket(response.Write());

        return;
    }

    // Find the plot by index ? O(1) direct array access
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
                // Offline: delete housing directly from DB.
                //
                // H-12: this used to run CHAR_DEL_CHARACTER_HOUSING alone, clearing one
                // of the five tables and orphaning character_housing_decor, _rooms,
                // _fixtures and _catalog. Those are selected by ownerGuid, not by house,
                // so the rows were picked up again by the player's NEXT house - decor at
                // the old coordinates, against a room layout that no longer existed.
                // Housing::DeleteFromDB is the same clearing the online branch performs
                // via DeleteHousing().
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                Housing::DeleteFromDB(evictedPlayerGuid.GetCounter(), trans);
                CharacterDatabase.CommitTransaction(trans);
            }
        }

        // Neighborhood::EvictPlayer already sent the remaining members the new roster.

        // SMSG_NEIGHBORHOOD_EVICT_PLAYER (0x5C0000). The 12.0.7 client handler (case 6029312)
        // does not decode any field ? it consumes the remaining bytes as a blob and then fires
        // three neighborhood-view refreshes (codes 2, 3, 1). It is a "the roster you are showing
        // is stale, rebuild it" notification, which is exactly the state after an eviction, so it
        // goes to everyone whose view just changed: the remaining members and the evicted player.
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

// ============================================================
// Neighborhood Initiative System
// ============================================================

void WorldSession::HandleNeighborhoodInitiativeServiceStatusCheck(WorldPackets::Neighborhood::NeighborhoodInitiativeServiceStatusCheck const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

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
        // ? Flags top-2-bits == 0 makes the client skip the data block entirely,
        // which is the no-data path. Real failures route via SMSG_HOUSING_SVCS_NOTIFY_PERMISSIONS_FAILURE.
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

// ============================================================
// Phase 7 ? Charter Handlers
// ============================================================

// ? fake CMSGs 0x370002 + 0x370005, no client senders in build 67186 (STUB-OK only).

// ============================================================
// Phase 7 ? Neighborhood Handlers
// ============================================================
