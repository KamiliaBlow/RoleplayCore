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
#include "AchievementPackets.h"
#include "Common.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GuildPackets.h"
#include "GuildRenameMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"

void WorldSession::HandleGuildQueryOpcode(WorldPackets::Guild::QueryGuildInfo& query)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_QUERY [{}]: Guild: {} Target: {}",
        GetPlayerInfo(), query.GuildGuid.ToString(), query.PlayerGuid.ToString());

    if (Guild* guild = sGuildMgr->GetGuildByGuid(query.GuildGuid))
    {
        guild->HandleQuery(this, query.GuildGuid);
        return;
    }

    WorldPackets::Guild::QueryGuildInfoResponse response;
    response.GuildGuid = query.GuildGuid;
    SendPacket(response.Write());

    TC_LOG_DEBUG("guild", "SMSG_GUILD_QUERY_RESPONSE [{}]", GetPlayerInfo());
}

void WorldSession::HandleGuildInviteByName(WorldPackets::Guild::GuildInviteByName& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_INVITE [{}]: Invited: {}", GetPlayerInfo(), packet.Name);
    if (normalizePlayerName(packet.Name))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleInviteMember(this, packet.Name);
}

void WorldSession::HandleGuildOfficerRemoveMember(WorldPackets::Guild::GuildOfficerRemoveMember& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_REMOVE [{}]: Target: {}", GetPlayerInfo(), packet.Removee.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRemoveMember(this, packet.Removee);
}

void WorldSession::HandleGuildAcceptInvite(WorldPackets::Guild::AcceptGuildInvite& /*invite*/)
{
    if (!GetPlayer()->GetGuildId())
        if (Guild* guild = sGuildMgr->GetGuildById(GetPlayer()->GetGuildIdInvited()))
            guild->HandleAcceptMember(this);
}

void WorldSession::HandleGuildDeclineInvitation(WorldPackets::Guild::GuildDeclineInvitation& /*decline*/)
{
    if (GetPlayer()->GetGuildId())
        return;

    GetPlayer()->SetGuildIdInvited(UI64LIT(0));
}

void WorldSession::HandleGuildGetRoster(WorldPackets::Guild::GuildGetRoster& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRoster(this);
    else
        Guild::SendCommandResult(this, GUILD_COMMAND_GET_ROSTER, ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

void WorldSession::HandleGuildPromoteMember(WorldPackets::Guild::GuildPromoteMember& promote)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_PROMOTE [{}]: Target: {}", GetPlayerInfo(), promote.Promotee.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateMemberRank(this, promote.Promotee, false);
}

void WorldSession::HandleGuildDemoteMember(WorldPackets::Guild::GuildDemoteMember& demote)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_DEMOTE [{}]: Target: {}", GetPlayerInfo(), demote.Demotee.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleUpdateMemberRank(this, demote.Demotee, true);
}

void WorldSession::HandleGuildAssignRank(WorldPackets::Guild::GuildAssignMemberRank& packet)
{
    ObjectGuid setterGuid = GetPlayer()->GetGUID();

    TC_LOG_DEBUG("guild", "CMSG_GUILD_ASSIGN_MEMBER_RANK [{}]: Target: {} Rank: {}, Issuer: {}",
        GetPlayerInfo(), packet.Member.ToString(), packet.RankOrder, setterGuid.ToString());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMemberRank(this, packet.Member, setterGuid, GuildRankOrder(packet.RankOrder));
}

void WorldSession::HandleGuildLeave(WorldPackets::Guild::GuildLeave& /*leave*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleLeaveMember(this);
}

void WorldSession::HandleGuildDelete(WorldPackets::Guild::GuildDelete& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleDelete(this);
}

void WorldSession::HandleGuildUpdateMotdText(WorldPackets::Guild::GuildUpdateMotdText& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_UPDATE_MOTD_TEXT [{}]: MOTD: {}", GetPlayerInfo(), std::string_view(packet.MotdText));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMOTD(this, packet.MotdText);
}

void WorldSession::HandleGuildSetMemberNote(WorldPackets::Guild::GuildSetMemberNote& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_NOTE [{}]: Target: {}, Note: {}, Public: {}",
        GetPlayerInfo(), packet.NoteeGUID.ToString(), std::string_view(packet.Note), packet.IsPublic);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetMemberNote(this, packet.Note, packet.NoteeGUID, packet.IsPublic);
}

void WorldSession::HandleGuildGetRanks(WorldPackets::Guild::GuildGetRanks& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_GET_RANKS [{}]: Guild: {}",
        GetPlayerInfo(), packet.GuildGUID.ToString());

    if (Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID))
        if (guild->IsMember(_player->GetGUID()))
            guild->SendGuildRankInfo(this);
}

void WorldSession::HandleGuildAddRank(WorldPackets::Guild::GuildAddRank& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_ADD_RANK [{}]: Rank: {}", GetPlayerInfo(), std::string_view(packet.Name));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleAddNewRank(this, packet.Name);
}

void WorldSession::HandleGuildDeleteRank(WorldPackets::Guild::GuildDeleteRank& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_DELETE_RANK [{}]: Rank: {}", GetPlayerInfo(), packet.RankOrder);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleRemoveRank(this, GuildRankOrder(packet.RankOrder));
}

void WorldSession::HandleGuildShiftRank(WorldPackets::Guild::GuildShiftRank& shiftRank)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_SHIFT_RANK [{}]: RankOrder: {}, ShiftUp: {}", GetPlayerInfo(), shiftRank.RankOrder, shiftRank.ShiftUp ? "true" : "false");

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleShiftRank(this, GuildRankOrder(shiftRank.RankOrder), shiftRank.ShiftUp);
}

void WorldSession::HandleGuildUpdateInfoText(WorldPackets::Guild::GuildUpdateInfoText& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_UPDATE_INFO_TEXT [{}]: {}", GetPlayerInfo(), std::string_view(packet.InfoText));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetInfo(this, packet.InfoText);
}

void WorldSession::HandleSaveGuildEmblem(WorldPackets::Guild::SaveGuildEmblem& packet)
{
    EmblemInfo emblemInfo;
    emblemInfo.ReadPacket(packet);

    TC_LOG_DEBUG("guild", "CMSG_SAVE_GUILD_EMBLEM [{}]: Guid: [{}] Style: {}, Color: {}, BorderStyle: {}, BorderColor: {}, BackgroundColor: {}"
        , GetPlayerInfo(), packet.Vendor.ToString(), emblemInfo.GetStyle()
        , emblemInfo.GetColor(), emblemInfo.GetBorderStyle()
        , emblemInfo.GetBorderColor(), emblemInfo.GetBackgroundColor());

    if (GetPlayer()->GetNPCIfCanInteractWith(packet.Vendor, UNIT_NPC_FLAG_TABARDDESIGNER, UNIT_NPC_FLAG_2_NONE))
    {
        // Remove fake death
        if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
            GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

        if (!emblemInfo.ValidateEmblemColors())
        {
            Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_INVALID_TABARD_COLORS);
            return;
        }

        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleSetEmblem(this, emblemInfo);
        else
            Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_NOGUILD); // "You are not part of a guild!";
    }
    else
        Guild::SendSaveEmblemResult(this, ERR_GUILDEMBLEM_INVALIDVENDOR); // "That's not an emblem vendor!"
}

void WorldSession::HandleGuildEventLogQuery(WorldPackets::Guild::GuildEventLogQuery& /*packet*/)
{
    TC_LOG_DEBUG("guild", "MSG_GUILD_EVENT_LOG_QUERY [{}]", GetPlayerInfo());

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendEventLog(this);
}

void WorldSession::HandleGuildBankMoneyWithdrawn(WorldPackets::Guild::GuildBankRemainingWithdrawMoneyQuery& /*packet*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendMoneyInfo(this);
}

void WorldSession::HandleGuildPermissionsQuery(WorldPackets::Guild::GuildPermissionsQuery& /* packet */)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendPermissions(this);
}

// Called when clicking on Guild bank gameobject
void WorldSession::HandleGuildBankActivate(WorldPackets::Guild::GuildBankActivate& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_ACTIVATE [{}]: [{}] AllSlots: {}"
        , GetPlayerInfo(), packet.Banker.ToString(), packet.FullUpdate);

#ifndef DISABLE_DRESSNPCS_CORESOUNDS
    if (packet.Banker.IsAnyTypeCreature())
        if (Creature* creature = _player->GetMap()->GetCreature(packet.Banker))
            creature->SendMirrorSound(_player, 0);
#endif

    GameObject const* const go = GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK);
    if (!go)
        return;

    Guild* const guild = GetPlayer()->GetGuild();
    if (!guild)
    {
        Guild::SendCommandResult(this, GUILD_COMMAND_VIEW_TAB, ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    GetPlayer()->PlayerTalkClass->GetInteractionData().StartInteraction(packet.Banker, PlayerInteractionType::GuildBanker);

    guild->SendBankList(this, 0, packet.FullUpdate);
}

// Called when opening guild bank tab only (first one)
void WorldSession::HandleGuildBankQueryTab(WorldPackets::Guild::GuildBankQueryTab& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_QUERY_TAB [{}]: {}, TabId: {}, ShowTabs: {}"
        , GetPlayerInfo(), packet.Banker.ToString(), packet.Tab, packet.FullUpdate);

    if (GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->SendBankList(this, packet.Tab, packet.FullUpdate);
}

void WorldSession::HandleGuildBankDepositMoney(WorldPackets::Guild::GuildBankDepositMoney& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_DEPOSIT_MONEY [{}]: [{}], money: {}",
        GetPlayerInfo(), packet.Banker.ToString(), packet.Money);

    if (GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (packet.Money && GetPlayer()->HasEnoughMoney(packet.Money))
            if (Guild* guild = GetPlayer()->GetGuild())
                guild->HandleMemberDepositMoney(this, packet.Money);
}

void WorldSession::HandleGuildBankWithdrawMoney(WorldPackets::Guild::GuildBankWithdrawMoney& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_WITHDRAW_MONEY [{}]: [{}], money: {}",
        GetPlayerInfo(), packet.Banker.ToString(), packet.Money);

    if (packet.Money && GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleMemberWithdrawMoney(this, packet.Money);
}

void WorldSession::HandleAutoGuildBankItem(WorldPackets::Guild::AutoGuildBankItem& depositGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(depositGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(depositGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), depositGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, depositGuildBankItem.BankTab, depositGuildBankItem.BankSlot,
            depositGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), depositGuildBankItem.ContainerItemSlot, 0);
}

void WorldSession::HandleStoreGuildBankItem(WorldPackets::Guild::StoreGuildBankItem& storeGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(storeGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(storeGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), storeGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), true, storeGuildBankItem.BankTab, storeGuildBankItem.BankSlot,
            storeGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), storeGuildBankItem.ContainerItemSlot, 0);
}

void WorldSession::HandleSwapItemWithGuildBankItem(WorldPackets::Guild::SwapItemWithGuildBankItem& swapItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(swapItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(swapItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), swapItemWithGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, swapItemWithGuildBankItem.BankTab, swapItemWithGuildBankItem.BankSlot,
            swapItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), swapItemWithGuildBankItem.ContainerItemSlot, 0);
}

void WorldSession::HandleSwapGuildBankItemWithGuildBankItem(WorldPackets::Guild::SwapGuildBankItemWithGuildBankItem& swapGuildBankItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(swapGuildBankItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), swapGuildBankItemWithGuildBankItem.BankTab[0], swapGuildBankItemWithGuildBankItem.BankSlot[0],
        swapGuildBankItemWithGuildBankItem.BankTab[1], swapGuildBankItemWithGuildBankItem.BankSlot[1], 0);
}

void WorldSession::HandleMoveGuildBankItem(WorldPackets::Guild::MoveGuildBankItem& moveGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(moveGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), moveGuildBankItem.BankTab, moveGuildBankItem.BankSlot, moveGuildBankItem.BankTab1, moveGuildBankItem.BankSlot1, 0);
}

void WorldSession::HandleMergeItemWithGuildBankItem(WorldPackets::Guild::MergeItemWithGuildBankItem& mergeItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(mergeItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(mergeItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeItemWithGuildBankItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, mergeItemWithGuildBankItem.BankTab, mergeItemWithGuildBankItem.BankSlot,
            mergeItemWithGuildBankItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeItemWithGuildBankItem.ContainerItemSlot, mergeItemWithGuildBankItem.StackCount);
}

void WorldSession::HandleSplitItemToGuildBank(WorldPackets::Guild::SplitItemToGuildBank& splitItemToGuildBank)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(splitItemToGuildBank.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(splitItemToGuildBank.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitItemToGuildBank.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), false, splitItemToGuildBank.BankTab, splitItemToGuildBank.BankSlot,
            splitItemToGuildBank.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitItemToGuildBank.ContainerItemSlot, splitItemToGuildBank.StackCount);
}

void WorldSession::HandleMergeGuildBankItemWithItem(WorldPackets::Guild::MergeGuildBankItemWithItem& mergeGuildBankItemWithItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(mergeGuildBankItemWithItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(mergeGuildBankItemWithItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeGuildBankItemWithItem.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), true, mergeGuildBankItemWithItem.BankTab, mergeGuildBankItemWithItem.BankSlot,
            mergeGuildBankItemWithItem.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), mergeGuildBankItemWithItem.ContainerItemSlot, mergeGuildBankItemWithItem.StackCount);
}

void WorldSession::HandleSplitGuildBankItemToInventory(WorldPackets::Guild::SplitGuildBankItemToInventory& splitGuildBankItemToInventory)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(splitGuildBankItemToInventory.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    if (!Player::IsInventoryPos(splitGuildBankItemToInventory.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitGuildBankItemToInventory.ContainerItemSlot))
        GetPlayer()->SendEquipError(EQUIP_ERR_INTERNAL_BAG_ERROR, nullptr);
    else
        guild->SwapItemsWithInventory(GetPlayer(), true, splitGuildBankItemToInventory.BankTab, splitGuildBankItemToInventory.BankSlot,
            splitGuildBankItemToInventory.ContainerSlot.value_or(INVENTORY_SLOT_BAG_0), splitGuildBankItemToInventory.ContainerItemSlot, splitGuildBankItemToInventory.StackCount);
}

void WorldSession::HandleAutoStoreGuildBankItem(WorldPackets::Guild::AutoStoreGuildBankItem& autoStoreGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(autoStoreGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItemsWithInventory(GetPlayer(), true, autoStoreGuildBankItem.BankTab, autoStoreGuildBankItem.BankSlot,
        INVENTORY_SLOT_BAG_0, NULL_SLOT, 0);
}

void WorldSession::HandleMergeGuildBankItemWithGuildBankItem(WorldPackets::Guild::MergeGuildBankItemWithGuildBankItem& mergeGuildBankItemWithGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(mergeGuildBankItemWithGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), mergeGuildBankItemWithGuildBankItem.BankTab, mergeGuildBankItemWithGuildBankItem.BankSlot,
        mergeGuildBankItemWithGuildBankItem.BankTab1, mergeGuildBankItemWithGuildBankItem.BankSlot1, mergeGuildBankItemWithGuildBankItem.StackCount);
}

void WorldSession::HandleSplitGuildBankItem(WorldPackets::Guild::SplitGuildBankItem& splitGuildBankItem)
{
    if (!GetPlayer()->GetGameObjectIfCanInteractWith(splitGuildBankItem.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    guild->SwapItems(GetPlayer(), splitGuildBankItem.BankTab, splitGuildBankItem.BankSlot,
        splitGuildBankItem.BankTab1, splitGuildBankItem.BankSlot1, splitGuildBankItem.StackCount);
}

void WorldSession::HandleGuildBankBuyTab(WorldPackets::Guild::GuildBankBuyTab& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_BUY_TAB [{}]: [{}[, TabId: {}", GetPlayerInfo(), packet.Banker.ToString(), packet.BankTab);

    if (!packet.Banker || GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleBuyBankTab(this, packet.BankTab);
}

void WorldSession::HandleGuildBankUpdateTab(WorldPackets::Guild::GuildBankUpdateTab& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_UPDATE_TAB [{}]: [{}], TabId: {}, Name: {}, Icon: {}"
        , GetPlayerInfo(), packet.Banker.ToString(), packet.BankTab, std::string_view(packet.Name), std::string_view(packet.Icon));

    if (!packet.Name.empty() && !packet.Icon.empty())
        if (GetPlayer()->GetGameObjectIfCanInteractWith(packet.Banker, GAMEOBJECT_TYPE_GUILD_BANK))
            if (Guild* guild = GetPlayer()->GetGuild())
                guild->HandleSetBankTabInfo(this, packet.BankTab, packet.Name, packet.Icon);
}

void WorldSession::HandleGuildBankLogQuery(WorldPackets::Guild::GuildBankLogQuery& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_LOG_QUERY [{}]: TabId: {}", GetPlayerInfo(), packet.Tab);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendBankLog(this, packet.Tab);
}

void WorldSession::HandleGuildBankTextQuery(WorldPackets::Guild::GuildBankTextQuery& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_BANK_QUERY_TEXT [{}]: TabId: {}", GetPlayerInfo(), packet.Tab);

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SendBankTabText(this, packet.Tab);
}

void WorldSession::HandleGuildBankSetTabText(WorldPackets::Guild::GuildBankSetTabText& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_SET_GUILD_BANK_TEXT [{}]: TabId: {}, Text: {}", GetPlayerInfo(), packet.Tab, std::string_view(packet.TabText));

    if (Guild* guild = GetPlayer()->GetGuild())
        guild->SetBankTabText(packet.Tab, packet.TabText);
}

void WorldSession::HandleGuildSetRankPermissions(WorldPackets::Guild::GuildSetRankPermissions& packet)
{
    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    std::array<GuildBankRightsAndSlots, GUILD_BANK_MAX_TABS> rightsAndSlots;
    for (uint8 tabId = 0; tabId < GUILD_BANK_MAX_TABS; ++tabId)
        rightsAndSlots[tabId] = GuildBankRightsAndSlots(tabId, uint8(packet.TabFlags[tabId]), uint32(packet.TabWithdrawItemLimit[tabId]));

    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_RANK_PERMISSIONS [{}]: Rank: {} ({})", GetPlayerInfo(), std::string_view(packet.RankName), packet.RankOrder);

    guild->HandleSetRankInfo(this, GuildRankId(packet.RankID), packet.RankName, packet.Flags, packet.WithdrawGoldLimit, rightsAndSlots);
}

void WorldSession::HandleGuildRequestPartyState(WorldPackets::Guild::RequestGuildPartyState& packet)
{
    if (Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID))
        guild->HandleGuildPartyRequest(this);
}

void WorldSession::HandleGuildChallengeUpdateRequest(WorldPackets::Guild::GuildChallengeUpdateRequest& /*packet*/)
{
    if (Guild* guild = _player->GetGuild())
        guild->HandleGuildRequestChallengeUpdate(this);
}

void WorldSession::HandleDeclineGuildInvites(WorldPackets::Guild::DeclineGuildInvites& packet)
{
    if (packet.Allow)
        GetPlayer()->SetPlayerFlag(PLAYER_FLAGS_AUTO_DECLINE_GUILD);
    else
        GetPlayer()->RemovePlayerFlag(PLAYER_FLAGS_AUTO_DECLINE_GUILD);
}

void WorldSession::HandleRequestGuildRewardsList(WorldPackets::Guild::RequestGuildRewardsList& /*packet*/)
{
    if (sGuildMgr->GetGuildById(_player->GetGuildId()))
    {
        std::vector<GuildReward> const& rewards = sGuildMgr->GetGuildRewards();

        WorldPackets::Guild::GuildRewardList rewardList;
        rewardList.Version = GameTime::GetSystemTime();
        rewardList.RewardItems.reserve(rewards.size());

        for (uint32 i = 0; i < rewards.size(); i++)
        {
            WorldPackets::Guild::GuildRewardItem rewardItem;
            rewardItem.ItemID = rewards[i].ItemID;
            rewardItem.RaceMask = rewards[i].RaceMask;
            rewardItem.MinGuildLevel = 0;
            rewardItem.MinGuildRep = rewards[i].MinGuildRep;
            rewardItem.AchievementsRequired = rewards[i].AchievementsRequired;
            rewardItem.Cost = rewards[i].Cost;
            rewardList.RewardItems.push_back(rewardItem);
        }

        SendPacket(rewardList.Write());
    }
}

void WorldSession::HandleGuildQueryNews(WorldPackets::Guild::GuildQueryNews& newsQuery)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        if (guild->GetGUID() == newsQuery.GuildGUID)
            guild->SendNewsUpdate(this);
}

void WorldSession::HandleGuildNewsUpdateSticky(WorldPackets::Guild::GuildNewsUpdateSticky& packet)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleNewsSetSticky(this, packet.NewsID, packet.Sticky);
}

void WorldSession::HandleGuildReplaceGuildMaster(WorldPackets::Guild::GuildReplaceGuildMaster& /*replaceGuildMaster*/)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetNewGuildMaster(this, "", true);
}

void WorldSession::HandleGuildSetGuildMaster(WorldPackets::Guild::GuildSetGuildMaster& packet)
{
    TC_LOG_DEBUG("guild", "CMSG_GUILD_SET_GUILD_MASTER [{}]: Target: {}", GetPlayerInfo(), packet.NewMasterName);

    if (normalizePlayerName(packet.NewMasterName))
        if (Guild* guild = GetPlayer()->GetGuild())
            guild->HandleSetNewGuildMaster(this, packet.NewMasterName, false);
}

void WorldSession::HandleGuildSetAchievementTracking(WorldPackets::Guild::GuildSetAchievementTracking& packet)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleSetAchievementTracking(this, packet.AchievementIDs.data(), packet.AchievementIDs.data() + packet.AchievementIDs.size());
}

void WorldSession::HandleGuildGetAchievementMembers(WorldPackets::Achievement::GuildGetAchievementMembers& getAchievementMembers)
{
    if (Guild* guild = GetPlayer()->GetGuild())
        guild->HandleGetAchievementMembers(this, uint32(getAchievementMembers.AchievementID));
}

void WorldSession::HandleGuildRequestRenameStatus(WorldPackets::Guild::GuildRequestRenameStatus& /*packet*/)
{
    sGuildRenameMgr->HandleStatusRequest(this);
}

void WorldSession::HandleGuildRequestRenameNameCheck(WorldPackets::Guild::GuildRequestRenameNameCheck& packet)
{
    sGuildRenameMgr->HandleNameCheck(this, packet.DesiredName);
}

void WorldSession::HandleGuildRequestRename(WorldPackets::Guild::GuildRequestRename& packet)
{
    sGuildRenameMgr->HandleRenameRequest(this, packet.DesiredName);
}

void WorldSession::HandleGuildRequestRenameRefund(WorldPackets::Guild::GuildRequestRenameRefund& /*packet*/)
{
    sGuildRenameMgr->HandleRefundRequest(this);
}

void WorldSession::HandleGuildChangeNameRequest(WorldPackets::Guild::GuildChangeNameRequest& packet)
{
    Guild* guild = GetPlayer()->GetGuild();
    if (!guild)
        return;

    // Only the guild master may rename the guild.
    if (guild->GetLeaderGUID() != GetPlayer()->GetGUID())
        return;

    // Reject a name already taken by another guild. Guild::SetName performs the remaining validation (non-empty,
    // <= 24 chars, not reserved, valid charter name, actually changed) and, on success, persists the new name and
    // broadcasts SMSG_GUILD_NAME_CHANGED to every member so their client reflects it.
    if (sGuildMgr->GetGuildByName(packet.NewName))
        return;

    guild->SetName(packet.NewName);
}

// =====================================================================================================
// Guild recipe sharing
//
// Wire and bit-index rule are documented on the packet classes in GuildPackets.h; the full RE is in
// c:\dumps\GUILD_RECIPE_BITINDEX_RE_68275.md. In short: bit N of the 600-byte blob is the
// SkillLineAbility row with SkillLine == <the SkillLineID sent alongside> and UniqueBit == N, and with
// HeaderCount emitted as 0 the bits start at byte offset 2.
//
// Recipe knowledge has to cover OFFLINE members too - a guild recipe browser listing only the recipes of
// whoever happens to be online would be misleading rather than merely incomplete. So the authoritative
// source is character_spell, read asynchronously, with the online members' in-memory spell lists overlaid
// on top (they may know spells not yet flushed to the database).
// =====================================================================================================

namespace
{
    // sSkillLineAbilityStore is only indexed by skill line, so build the spell -> abilities direction once.
    std::unordered_map<uint32, std::vector<SkillLineAbilityEntry const*>> const& GetAbilitiesBySpell()
    {
        static std::unordered_map<uint32, std::vector<SkillLineAbilityEntry const*>> const index = []
        {
            std::unordered_map<uint32, std::vector<SkillLineAbilityEntry const*>> map;
            for (SkillLineAbilityEntry const* ability : sSkillLineAbilityStore)
            {
                // UniqueBit 0 means the row has no bit in the mask at all; the client's own loop skips
                // those rows too, so they can never be represented.
                if (!ability->UniqueBit)
                    continue;

                map[uint32(ability->Spell)].push_back(ability);
            }
            return map;
        }();

        return index;
    }

    // A recipe browser means professions. Without this filter the guild-wide response would carry a
    // 600-byte blob for EVERY skill line any member has an ability in - class skills, weapon skills,
    // languages and so on - because SkillLineAbility covers all of them. That is not wrong data, but it
    // bloats one packet by tens of kilobytes for something the recipe UI never shows.
    bool IsRecipeSkillLine(uint32 skillLineId)
    {
        SkillLineEntry const* skillLine = sSkillLineStore.LookupEntry(skillLineId);
        if (!skillLine)
            return false;

        return skillLine->CategoryID == SKILL_CATEGORY_PROFESSION
            || skillLine->CategoryID == SKILL_CATEGORY_SECONDARY;
    }

    void SetRecipeBit(WorldPackets::Guild::GuildRecipeBlob& blob, uint16 uniqueBit)
    {
        // HeaderCount is emitted as 0 (bytes 0-1 stay zero), so the bit array starts at offset 2.
        std::size_t const byteOffset = 2u + (uniqueBit / 8u);
        if (byteOffset >= blob.size())
            return;

        blob[byteOffset] |= uint8(1u << (uniqueBit % 8u));
    }

    struct GuildMemberSet
    {
        std::vector<ObjectGuid> Guids;
        std::string LowGuidList;
    };

    GuildMemberSet CollectGuildMembers(Guild const* guild)
    {
        GuildMemberSet set;
        for (auto const& [guid, member] : guild->GetMembers())
        {
            set.Guids.push_back(guid);
            if (!set.LowGuidList.empty())
                set.LowGuidList += ',';
            set.LowGuidList += std::to_string(guid.GetCounter());
        }
        return set;
    }

    std::unordered_set<uint32> CollectKnownSpells(Player const* player)
    {
        std::unordered_set<uint32> known;
        for (auto const& spellPair : player->GetSpellMap())
            if (player->HasSpell(spellPair.first))
                known.insert(spellPair.first);

        return known;
    }
}

void WorldSession::HandleGuildQueryRecipes(WorldPackets::Guild::GuildQueryRecipes& packet)
{
    Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID);
    if (!guild || !guild->IsMember(_player->GetGUID()))
        return;

    GuildMemberSet members = CollectGuildMembers(guild);
    if (members.LowGuidList.empty())
        return;

    std::string const spellQuery = Trinity::StringFormat("SELECT spell FROM character_spell WHERE guid IN ({})", members.LowGuidList);

    GetQueryProcessor().AddCallback(
        CharacterDatabase.AsyncQuery(spellQuery.c_str())
        .WithCallback([this, memberGuids = std::move(members.Guids)](QueryResult result)
    {
        auto const& abilitiesBySpell = GetAbilitiesBySpell();
        std::unordered_map<uint32, WorldPackets::Guild::GuildRecipeBlob> blobsBySkillLine;

        auto addSpell = [&](uint32 spellId)
        {
            auto itr = abilitiesBySpell.find(spellId);
            if (itr == abilitiesBySpell.end())
                return;

            for (SkillLineAbilityEntry const* ability : itr->second)
                if (IsRecipeSkillLine(ability->SkillLine))
                    SetRecipeBit(blobsBySkillLine[ability->SkillLine], ability->UniqueBit);
        };

        if (result)
        {
            do
            {
                addSpell((*result)[0].GetUInt32());
            } while (result->NextRow());
        }

        // Overlay the online members: their in-memory spell list is newer than character_spell.
        for (ObjectGuid const& guid : memberGuids)
            if (Player const* member = ObjectAccessor::FindConnectedPlayer(guid))
                for (uint32 spellId : CollectKnownSpells(member))
                    addSpell(spellId);

        WorldPackets::Guild::GuildKnownRecipes response;
        response.Data.reserve(blobsBySkillLine.size());
        for (auto const& blobPair : blobsBySkillLine)
        {
            WorldPackets::Guild::GuildKnownRecipes::SkillLineRecipes entry;
            entry.SkillLineID = blobPair.first;
            entry.Blob = blobPair.second;
            response.Data.push_back(entry);
        }

        SendPacket(response.Write());
    }));
}

void WorldSession::HandleGuildQueryMemberRecipes(WorldPackets::Guild::GuildQueryMemberRecipes& packet)
{
    Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID);
    if (!guild || !guild->IsMember(_player->GetGUID()))
        return;

    // Only answer for someone who is actually in this guild - never leak another guild's members.
    if (!guild->IsMember(packet.MemberGUID))
        return;

    if (!sDB2Manager.GetSkillLineAbilitiesBySkill(packet.SkillLineID))
        return;

    ObjectGuid const memberGuid = packet.MemberGUID;
    uint32 const skillLineId = packet.SkillLineID;

    auto buildResponse = [this, memberGuid, skillLineId](std::unordered_set<uint32> const& knownSpells, uint32 skillRank, uint32 skillStep)
    {
        WorldPackets::Guild::GuildMemberRecipes response;
        response.MemberGUID = memberGuid;
        response.SkillLineID = skillLineId;
        response.SkillRank = skillRank;
        response.SkillStep = skillStep;

        if (std::vector<SkillLineAbilityEntry const*> const* lineAbilities = sDB2Manager.GetSkillLineAbilitiesBySkill(skillLineId))
            for (SkillLineAbilityEntry const* ability : *lineAbilities)
                if (ability->UniqueBit && knownSpells.count(uint32(ability->Spell)))
                    SetRecipeBit(response.Blob, ability->UniqueBit);

        SendPacket(response.Write());
    };

    if (Player const* member = ObjectAccessor::FindConnectedPlayer(memberGuid))
    {
        buildResponse(CollectKnownSpells(member), member->GetSkillValue(skillLineId), member->GetSkillStep(skillLineId));
        return;
    }

    // Offline member: read their spells from the database. Rank/step stay 0 rather than being invented -
    // character_skills would have to be joined for those, and the two fields are an unproven hypothesis
    // anyway (see GuildMemberRecipes in GuildPackets.h).
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
    stmt->setUInt64(0, memberGuid.GetCounter());

    GetQueryProcessor().AddCallback(CharacterDatabase.AsyncQuery(stmt)
        .WithPreparedCallback([buildResponse](PreparedQueryResult result)
    {
        std::unordered_set<uint32> known;
        if (result)
        {
            do
            {
                known.insert((*result)[0].GetUInt32());
            } while (result->NextRow());
        }

        buildResponse(known, 0, 0);
    }));
}

void WorldSession::HandleGuildQueryMembersForRecipe(WorldPackets::Guild::GuildQueryMembersForRecipe& packet)
{
    Guild* guild = sGuildMgr->GetGuildByGuid(packet.GuildGUID);
    if (!guild || !guild->IsMember(_player->GetGUID()))
        return;

    GuildMemberSet members = CollectGuildMembers(guild);
    if (members.LowGuidList.empty())
        return;

    uint32 const skillLineId = packet.SkillLineID;
    uint32 const recipeSpellId = packet.RecipeSpellID;

    std::string const memberQuery = Trinity::StringFormat("SELECT guid FROM character_spell WHERE spell = {} AND guid IN ({})",
        recipeSpellId, members.LowGuidList);

    GetQueryProcessor().AddCallback(
        CharacterDatabase.AsyncQuery(memberQuery.c_str())
        .WithCallback([this, skillLineId, recipeSpellId, memberGuids = std::move(members.Guids)](QueryResult result)
    {
        std::unordered_set<ObjectGuid> found;
        if (result)
        {
            do
            {
                found.insert(ObjectGuid::Create<HighGuid::Player>((*result)[0].GetUInt64()));
            } while (result->NextRow());
        }

        // Same overlay as the guild-wide query: an online member may have learned the recipe since their
        // last save.
        for (ObjectGuid const& guid : memberGuids)
            if (Player const* member = ObjectAccessor::FindConnectedPlayer(guid))
                if (member->HasSpell(recipeSpellId))
                    found.insert(guid);

        WorldPackets::Guild::GuildMembersWithRecipe response;
        response.SkillLineID = skillLineId;
        response.RecipeSpellID = recipeSpellId;
        response.Members.assign(found.begin(), found.end());

        SendPacket(response.Write());
    }));
}
