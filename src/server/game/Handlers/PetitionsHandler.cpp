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
#include "Common.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Item.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PetitionMgr.h"
#include "PetitionPackets.h"
#include "Player.h"
#include "World.h"
#include <sstream>

#define CHARTER_DISPLAY_ID 16161
#define GUILD_CHARTER_ITEM_ID 5863

void WorldSession::HandlePetitionBuy(WorldPackets::Petition::PetitionBuy& packet)
{
    TC_LOG_DEBUG("network", "Petitioner {} tried sell petition: title {}", packet.Unit.ToString(), packet.Title);

    // prevent cheating
    Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(packet.Unit, UNIT_NPC_FLAG_PETITIONER, UNIT_NPC_FLAG_2_NONE);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandlePetitionBuyOpcode - {} not found or you can't interact with him.", packet.Unit.ToString());
        return;
    }

    // remove fake death
    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    uint32 charterItemID = GUILD_CHARTER_ITEM_ID;
    uint32 cost = sWorld->getIntConfig(CONFIG_CHARTER_COST_GUILD);

    // do not let if already in guild.
    if (_player->GetGuildId())
        return;

    if (sGuildMgr->GetGuildByName(packet.Title))
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NAME_EXISTS_S, packet.Title);
        return;
    }

    if (sObjectMgr->IsReservedName(packet.Title) || !ObjectMgr::IsValidCharterName(packet.Title))
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NAME_INVALID, packet.Title);
        return;
    }

    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(charterItemID);
    if (!pProto)
    {
        _player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, charterItemID, 0);
        return;
    }

    if (!_player->HasEnoughMoney(uint64(cost)))
    {
        _player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, creature, charterItemID, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, charterItemID, pProto->GetBuyCount());
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, nullptr, nullptr, charterItemID);
        return;
    }

    _player->ModifyMoney(-int32(cost));
    Item* charter = _player->StoreNewItem(dest, charterItemID, true);
    if (!charter)
        return;

    charter->SetPetitionId(charter->GetGUID().GetCounter());
    charter->SetState(ITEM_CHANGED, _player);
    _player->SendNewItem(charter, 1, true, false);

    // a petition is invalid, if both the owner and the type matches
    // we checked above, if this player is in an arenateam, so this must be
    // datacorruption
    if (Petition const* petition = sPetitionMgr->GetPetitionByOwner(_player->GetGUID()))
    {
        // clear from petition store
        sPetitionMgr->RemovePetition(petition->PetitionGuid);
        TC_LOG_DEBUG("network", "Invalid petition {}", petition->PetitionGuid.ToString());
    }

    // fill petition store
    sPetitionMgr->AddPetition(charter->GetGUID(), _player->GetGUID(), packet.Title, false);
}

void WorldSession::HandlePetitionShowSignatures(WorldPackets::Petition::PetitionShowSignatures& packet)
{
    Petition const* petition = sPetitionMgr->GetPetition(packet.Item);
    if (!petition)
    {
        TC_LOG_DEBUG("entities.player.items", "Petition {} is not found for player {} {}", packet.Item.ToString(), GetPlayer()->GetGUID().ToString(), GetPlayer()->GetName());
        return;
    }

    // if has guild => error, return;
    if (_player->GetGuildId())
        return;

    TC_LOG_DEBUG("network", "CMSG_PETITION_SHOW_SIGNATURES petition {}", packet.Item.ToString());

    SendPetitionSigns(petition, _player);
}

void WorldSession::SendPetitionSigns(Petition const* petition, Player* sendTo)
{
    WorldPackets::Petition::ServerPetitionShowSignatures signaturesPacket;
    signaturesPacket.Item = petition->PetitionGuid;
    signaturesPacket.Owner = petition->OwnerGuid;
    signaturesPacket.OwnerAccountID = ObjectGuid::Create<HighGuid::WowAccount>(sCharacterCache->GetCharacterAccountIdByGuid(petition->OwnerGuid));
    signaturesPacket.PetitionID = petition->PetitionGuid.GetCounter();

    for (Signature const& signature : petition->Signatures)
    {
        WorldPackets::Petition::ServerPetitionShowSignatures::PetitionSignature signaturePkt;
        signaturePkt.Signer = signature.second;
        signaturePkt.Choice = 0;
        signaturesPacket.Signatures.push_back(signaturePkt);
    }

    sendTo->SendDirectMessage(signaturesPacket.Write());
}

void WorldSession::HandleQueryPetition(WorldPackets::Petition::QueryPetition& packet)
{
    TC_LOG_DEBUG("network", "Received CMSG_QUERY_PETITION Petition {} PetitionID {}", packet.ItemGUID.ToString(), packet.PetitionID);

    SendPetitionQueryOpcode(packet.ItemGUID);
}

void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid)
{
    WorldPackets::Petition::QueryPetitionResponse responsePacket;
    responsePacket.PetitionID = uint32(petitionguid.GetCounter());  // PetitionID (in Trinity always same as GUID_LOPART(petition guid))
    Petition const* petition = sPetitionMgr->GetPetition(petitionguid);
    if (!petition)
    {
        responsePacket.Allow = false;
        SendPacket(responsePacket.Write());

        TC_LOG_DEBUG("network", "CMSG_PETITION_QUERY failed for petition ({})", petitionguid.ToString());
        return;
    }

    uint32 reqSignatures = sWorld->getIntConfig(CONFIG_MIN_PETITION_SIGNS);

    WorldPackets::Petition::PetitionInfo& petitionInfo = responsePacket.Info;
    petitionInfo.PetitionID = int32(petitionguid.GetCounter());
    petitionInfo.Petitioner = petition->OwnerGuid;
    petitionInfo.MinSignatures = reqSignatures;
    petitionInfo.MaxSignatures = reqSignatures;
    petitionInfo.Title = petition->PetitionName;

    responsePacket.Allow = true;

    SendPacket(responsePacket.Write());
}

void WorldSession::HandlePetitionRenameGuild(WorldPackets::Petition::PetitionRenameGuild& packet)
{
    Item* item = _player->GetItemByGuid(packet.PetitionGuid);
    if (!item)
        return;

    Petition* petition = sPetitionMgr->GetPetition(packet.PetitionGuid);
    if (!petition)
    {
        TC_LOG_DEBUG("network", "CMSG_PETITION_QUERY failed for petition {}", packet.PetitionGuid.ToString());
        return;
    }

    if (sGuildMgr->GetGuildByName(packet.NewGuildName))
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NAME_EXISTS_S, packet.NewGuildName);
        return;
    }

    if (sObjectMgr->IsReservedName(packet.NewGuildName) || !ObjectMgr::IsValidCharterName(packet.NewGuildName))
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NAME_INVALID, packet.NewGuildName);
        return;
    }

    // update petition storage
    petition->UpdateName(packet.NewGuildName);

    WorldPackets::Petition::PetitionRenameGuildResponse renameResponse;
    renameResponse.PetitionGuid = packet.PetitionGuid;
    renameResponse.NewGuildName = packet.NewGuildName;
    SendPacket(renameResponse.Write());

    TC_LOG_DEBUG("network", "Petition {} renamed to '{}'", packet.PetitionGuid.ToString(), packet.NewGuildName);
}

void WorldSession::HandleSignPetition(WorldPackets::Petition::SignPetition& packet)
{
    Petition* petition = sPetitionMgr->GetPetition(packet.PetitionGUID);
    if (!petition)
    {
        TC_LOG_ERROR("network", "Petition {} is not found for player {} {}", packet.PetitionGUID.ToString(), GetPlayer()->GetGUID().ToString(), GetPlayer()->GetName());
        return;
    }

    ObjectGuid ownerGuid = petition->OwnerGuid;
    uint64 signs = petition->Signatures.size();

    if (ownerGuid == _player->GetGUID())
        return;

    // not let enemies sign guild charter
    if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) && GetPlayer()->GetTeam() != sCharacterCache->GetCharacterTeamByGuid(ownerGuid))
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (_player->GetGuildId())
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_INVITE_PLAYER, ERR_ALREADY_IN_GUILD_S, _player->GetName());
        return;
    }

    if (_player->GetGuildIdInvited())
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_INVITE_PLAYER, ERR_ALREADY_INVITED_TO_GUILD_S, _player->GetName());
        return;
    }

    if (++signs > 10)                                          // client signs maximum
        return;

    // Client doesn't allow to sign petition two times by one character, but not check sign by another character from same account
    // not allow sign another player from already sign player account
    WorldPackets::Petition::PetitionSignResults signResult;
    signResult.Player = _player->GetGUID();
    signResult.Item = packet.PetitionGUID;

    bool isSigned = petition->IsPetitionSignedByAccount(GetAccountId());
    if (isSigned)
    {
        signResult.Error = int32(PETITION_SIGN_ALREADY_SIGNED);

        // close at signer side
        SendPacket(signResult.Write());

        // update for owner if online
        if (Player* owner = ObjectAccessor::FindConnectedPlayer(ownerGuid))
            owner->SendDirectMessage(signResult.GetRawPacket());
        return;
    }

    // fill petition store
    petition->AddSignature(GetAccountId(), _player->GetGUID(), false);

    TC_LOG_DEBUG("network", "PETITION SIGN: {} by player: {} ({} Account: {})", packet.PetitionGUID.ToString(), _player->GetName(), _player->GetGUID().ToString(), GetAccountId());

    signResult.Error = int32(PETITION_SIGN_OK);

    SendPacket(signResult.Write());

    // update signs count on charter
    if (Item* item = _player->GetItemByGuid(packet.PetitionGUID))
    {
        item->SetPetitionNumSignatures(signs);
        item->SetState(ITEM_CHANGED, _player);
    }

    // update for owner if online
    if (Player* owner = ObjectAccessor::FindConnectedPlayer(ownerGuid))
        owner->SendDirectMessage(signResult.GetRawPacket());
}

void WorldSession::HandleDeclinePetition(WorldPackets::Petition::DeclinePetition& packet)
{
    TC_LOG_DEBUG("network", "Petition {} declined by {}", packet.PetitionGUID.ToString(), _player->GetGUID().ToString());

    // Disabled because packet isn't handled by the client in any way
    /*
    Petition const* petition = sPetitionMgr->GetPetition(packet.PetitionGUID);
    if (!petition)
        return;

    // petition owner online
    if (Player* owner = ObjectAccessor::FindConnectedPlayer(petition->OwnerGuid))
    {
        WorldPackets::Petition::PetitionDeclined packet;
        packet.Decliner = _player->GetGUID();
        owner->SendDirectMessage(packet.Write());
    }
    */
}

void WorldSession::HandleOfferPetition(WorldPackets::Petition::OfferPetition& packet)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(packet.TargetPlayer);
    if (!player)
        return;

    Petition const* petition = sPetitionMgr->GetPetition(packet.ItemGUID);
    if (!petition)
        return;

    TC_LOG_DEBUG("network", "OFFER PETITION: {}, to {}", packet.ItemGUID.ToString(), packet.TargetPlayer.ToString());

    if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) && GetPlayer()->GetTeam() != player->GetTeam())
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_INVITE_PLAYER, ERR_ALREADY_IN_GUILD_S, _player->GetName());
        return;
    }

    if (player->GetGuildIdInvited())
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_INVITE_PLAYER, ERR_ALREADY_INVITED_TO_GUILD_S, _player->GetName());
        return;
    }

    SendPetitionSigns(petition, player);
}

void WorldSession::HandleTurnInPetition(WorldPackets::Petition::TurnInPetition& packet)
{
    // Check if player really has the required petition charter
    Item* item = _player->GetItemByGuid(packet.Item);
    if (!item)
        return;

    TC_LOG_DEBUG("network", "Petition {} turned in by {}", packet.Item.ToString(), _player->GetGUID().ToString());

    Petition const* petition = sPetitionMgr->GetPetition(packet.Item);
    if (!petition)
    {
        TC_LOG_ERROR("entities.player.cheat", "Player {} {} tried to turn in petition ({}) that is not present in the database", _player->GetName(), _player->GetGUID().ToString(), packet.Item.ToString());
        return;
    }

    std::string const name = petition->PetitionName; // we need a copy, Guild::AddMember invalidates petition

    // Only the petition owner can turn in the petition
    if (_player->GetGUID() != petition->OwnerGuid)
        return;

    // Check if player is already in a guild
    if (_player->GetGuildId())
    {
        WorldPackets::Petition::TurnInPetitionResult resultPacket;
        resultPacket.Result = int32(PETITION_TURN_ALREADY_IN_GUILD);
        SendPacket(resultPacket.Write());
        return;
    }

    // Check if guild name is already taken
    if (sGuildMgr->GetGuildByName(name))
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_NAME_EXISTS_S, name);
        return;
    }

    SignaturesVector const signatures = petition->Signatures; // we need a copy, Guild::AddMember invalidates petition
    uint32 requiredSignatures = sWorld->getIntConfig(CONFIG_MIN_PETITION_SIGNS);

    // Notify player if signatures are missing
    if (signatures.size() < requiredSignatures)
    {
        WorldPackets::Petition::TurnInPetitionResult resultPacket;
        resultPacket.Result = int32(PETITION_TURN_NEED_MORE_SIGNATURES);
        SendPacket(resultPacket.Write());
        return;
    }

    // Proceed with guild creation

    // Delete charter item
    _player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    // Create guild
    Guild* guild = new Guild;

    if (!guild->Create(_player, name))
    {
        delete guild;
        return;
    }

    // Register guild and add guild master
    sGuildMgr->AddGuild(guild);

    Guild::SendCommandResult(this, GUILD_COMMAND_CREATE_GUILD, ERR_GUILD_COMMAND_SUCCESS, name);

    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Add members from signatures
        for (Signature const& signature : signatures)
            guild->AddMember(trans, signature.second);

        CharacterDatabase.CommitTransaction(trans);
    }

    sPetitionMgr->RemovePetition(packet.Item);

    // created
    TC_LOG_DEBUG("network", "Player {} ({}) turning in petition {}", _player->GetName(), _player->GetGUID().ToString(), packet.Item.ToString());

    WorldPackets::Petition::TurnInPetitionResult resultPacket;
    resultPacket.Result = int32(PETITION_TURN_OK);
    SendPacket(resultPacket.Write());
}

void WorldSession::HandlePetitionShowList(WorldPackets::Petition::PetitionShowList& packet)
{
#ifndef DISABLE_DRESSNPCS_CORESOUNDS
    if (packet.PetitionUnit.IsAnyTypeCreature())
        if (Creature* creature = _player->GetMap()->GetCreature(packet.PetitionUnit))
            creature->SendMirrorSound(_player, 0);
#endif
    SendPetitionShowList(packet.PetitionUnit);
}

void WorldSession::SendPetitionShowList(ObjectGuid guid)
{
    Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER, UNIT_NPC_FLAG_2_NONE);
    if (!creature)
    {
        TC_LOG_DEBUG("network", "WORLD: HandlePetitionShowListOpcode - {} not found or you can't interact with him.", guid.ToString());
        return;
    }

    GetPlayer()->PlayerTalkClass->GetInteractionData().StartInteraction(guid, PlayerInteractionType::PetitionVendor);

    WorldPackets::Petition::ServerPetitionShowList packet;
    packet.Unit = guid;
    packet.Price = uint32(sWorld->getIntConfig(CONFIG_CHARTER_COST_GUILD));
    SendPacket(packet.Write());
}
