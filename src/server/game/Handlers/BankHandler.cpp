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
#include "BankPackets.h"
#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GossipDef.h"
#include "Item.h"
#include "Language.h"
#include "Log.h"
#include "NPCPackets.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "World.h"

void WorldSession::HandleAutoBankItemOpcode(WorldPackets::Bank::AutoBankItem& packet)
{
    TC_LOG_DEBUG("network", "STORAGE: receive bag = {}, slot = {}", packet.Bag, packet.Slot);

    if (!CanUseBank())
    {
        TC_LOG_ERROR("network", "WORLD: HandleAutoBankItemOpcode - Unit ({}) not found or you can't interact with him.", _player->PlayerTalkClass->GetInteractionData().SourceGuid);
        return;
    }

    Item* item = _player->GetItemByPos(packet.Bag, packet.Slot);
    if (!item)
        return;

    ItemPosCountVec dest;
    InventoryResult msg = EQUIP_ERR_OK;

    if (packet.BankType == BankType::Account)
        msg = _player->CanAccountBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
    else if (packet.BankType == BankType::Character)
        msg = _player->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
    else
        return;

    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, item, nullptr);
        return;
    }

    if (dest.size() == 1 && dest[0].pos == item->GetPos())
    {
        _player->SendEquipError(EQUIP_ERR_CANT_SWAP, item, nullptr);
        return;
    }

    _player->RemoveItem(packet.Bag, packet.Slot, true);
    _player->ItemRemovedQuestCheck(item->GetEntry(), item->GetCount());
    _player->BankItem(dest, item, true);
}

void WorldSession::HandleBankerActivateOpcode(WorldPackets::Bank::BankerActivate const& bankerActivate)
{
#ifndef DISABLE_DRESSNPCS_CORESOUNDS
    if (bankerActivate.Banker.IsAnyTypeCreature())
        if (Creature* creature = _player->GetMap()->GetCreature(bankerActivate.Banker))
            creature->SendMirrorSound(_player, 0);
#endif
	
    if (bankerActivate.InteractionType != PlayerInteractionType::Banker
        && bankerActivate.InteractionType != PlayerInteractionType::CharacterBanker
        && bankerActivate.InteractionType != PlayerInteractionType::AccountBanker)
        return;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(bankerActivate.Banker, UNIT_NPC_FLAG_ACCOUNT_BANKER | UNIT_NPC_FLAG_BANKER, UNIT_NPC_FLAG_2_NONE);
    if (!unit)
    {
        TC_LOG_ERROR("network", "WORLD: HandleBankerActivateOpcode - {} not found or you can not interact with him.", bankerActivate.Banker);
        return;
    }

    switch (bankerActivate.InteractionType)
    {
        case PlayerInteractionType::Banker:
            if (!unit->HasNpcFlag(UNIT_NPC_FLAG_ACCOUNT_BANKER) || !unit->HasNpcFlag(UNIT_NPC_FLAG_BANKER))
                return;
            break;
        case PlayerInteractionType::CharacterBanker:
            if (!unit->HasNpcFlag(UNIT_NPC_FLAG_BANKER))
                return;
            break;
        case PlayerInteractionType::AccountBanker:
            if (!unit->HasNpcFlag(UNIT_NPC_FLAG_ACCOUNT_BANKER))
                return;
            break;
        default:
            break;
    }

    // remove fake death
    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    // set currentBankerGUID for other bank action

    SendShowBank(bankerActivate.Banker, bankerActivate.InteractionType);
}

void WorldSession::HandleAutoStoreBankItemOpcode(WorldPackets::Bank::AutoStoreBankItem& packet)
{
    TC_LOG_DEBUG("network", "STORAGE: receive bag = {}, slot = {}", packet.Bag, packet.Slot);

    if (!CanUseBank())
    {
        TC_LOG_ERROR("network", "WORLD: HandleAutoStoreBankItemOpcode - Unit ({}) not found or you can't interact with him.", _player->PlayerTalkClass->GetInteractionData().SourceGuid);
        return;
    }

    Item* item = _player->GetItemByPos(packet.Bag, packet.Slot);
    if (!item)
        return;

    if (_player->IsBankPos(packet.Bag, packet.Slot))                    // moving from character bank to inventory
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, item, nullptr);
            return;
        }

        _player->RemoveItem(packet.Bag, packet.Slot, true);
        if (Item const* storedItem = _player->StoreItem(dest, item, true))
            _player->ItemAddedQuestCheck(storedItem->GetEntry(), storedItem->GetCount());
    }
    else if (_player->IsAccountBankPos(packet.Bag, packet.Slot))        // moving from account bank to inventory
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, item, nullptr);
            return;
        }

        _player->RemoveItem(packet.Bag, packet.Slot, true);
        if (Item const* storedItem = _player->StoreItem(dest, item, true))
            _player->ItemAddedQuestCheck(storedItem->GetEntry(), storedItem->GetCount());
    }
    else                                                                // moving from inventory to bank
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanBankItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, item, nullptr);
            return;
        }

        _player->RemoveItem(packet.Bag, packet.Slot, true);
        _player->BankItem(dest, item, true);
    }
}

void WorldSession::HandleBuyBankTab(WorldPackets::Bank::BuyBankTab const& buyBankTab)
{
    if (!CanUseBank(buyBankTab.Banker))
    {
        TC_LOG_ERROR("network", "WorldSession::HandleBuyBankTab {} - Banker {} not found or can't interact with him.",
            _player->GetGUID(), buyBankTab.Banker);
        return;
    }

    if (buyBankTab.BankType != BankType::Character && buyBankTab.BankType != BankType::Account)
    {
        TC_LOG_DEBUG("network", "WorldSession::HandleBuyBankTab {} - Bank type {} is not supported.",
            _player->GetGUID(), buyBankTab.BankType);
        return;
    }

    uint32 itemId = 0;
    uint8 slot = 0;
    uint8 inventorySlot = 0;

    switch (buyBankTab.BankType)
    {
        case BankType::Character:
            itemId = ITEM_CHARACTER_BANK_TAB_BAG;
            slot = _player->GetCharacterBankTabCount();
            inventorySlot = BANK_SLOT_BAG_START + slot;
            break;
        case BankType::Account:
            itemId = ITEM_ACCOUNT_BANK_TAB_BAG;
            slot = _player->GetAccountBankTabCount();
            inventorySlot = ACCOUNT_BANK_SLOT_BAG_START + slot;
            break;
        default:
            TC_LOG_DEBUG("network", "WorldSession::HandleBuyBankTab {} - Bank type {} is not supported.",
                _player->GetGUID(), buyBankTab.BankType);
            return;
    }

    auto bankTab = std::ranges::find(sBankTabStore, std::pair(buyBankTab.BankType, int8(slot)),
        [](BankTabEntry const* bankTab) { return std::pair(BankType(bankTab->BankType), bankTab->OrderIndex); });

    if (bankTab == sBankTabStore.end())
        return;

    uint64 price = bankTab->Cost;
    if (!_player->HasEnoughMoney(price))
        return;

    uint16 inventoryPos = 0;
    InventoryResult msg = _player->CanEquipNewItem(inventorySlot, inventoryPos, itemId, false);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, nullptr, nullptr, itemId);
        return;
    }

    Item* bag = _player->EquipNewItem(inventoryPos, itemId, ItemContext::NONE, true);
    if (!bag)
        return;

    _player->SendNewItem(bag, 1, true, false, false, 0, WorldPackets::Item::ItemPushResult::DISPLAY_TYPE_HIDDEN);

    switch (buyBankTab.BankType)
    {
        case BankType::Character:
            _player->SetCharacterBankTabCount(slot + 1);
            _player->SetCharacterBankTabSettings(slot, ChatHandler(this).PGetParseString(LANG_BANK_TAB_NAME, slot + 1), "", "", BagSlotFlags::None);
            break;
        case BankType::Account:
            _player->SetAccountBankTabCount(slot + 1);
            _player->SetAccountBankTabSettings(slot, ChatHandler(this).PGetParseString(LANG_BANK_TAB_NAME, slot + 1), "", "", BagSlotFlags::None);
            break;
        default:
            break;
    }

    _player->ModifyMoney(-int64(price));

    _player->UpdateCriteria(CriteriaType::BankTabPurchased, uint64(buyBankTab.BankType));
}

void WorldSession::HandleUpdateBankTabSettings(WorldPackets::Bank::UpdateBankTabSettings const& updateBankTabSettings)
{
    if (!CanUseBank(updateBankTabSettings.Banker))
    {
        TC_LOG_ERROR("network", "WorldSession::HandleUpdateBankTabSettings {} - Banker {} not found or can't interact with him.",
            _player->GetGUID(), updateBankTabSettings.Banker);
        return;
    }

    switch (updateBankTabSettings.BankType)
    {
        case BankType::Character:
            if (updateBankTabSettings.Tab >= _player->m_activePlayerData->CharacterBankTabSettings.size())
            {
                TC_LOG_DEBUG("network", "WorldSession::HandleUpdateBankTabSettings {} doesn't have bank tab {} in bank type {}.",
                    _player->GetGUID(), updateBankTabSettings.Tab, updateBankTabSettings.BankType);
                return;
            }
            _player->SetCharacterBankTabSettings(updateBankTabSettings.Tab, updateBankTabSettings.Settings.Name,
                updateBankTabSettings.Settings.Icon, updateBankTabSettings.Settings.Description, updateBankTabSettings.Settings.DepositFlags);
            break;
        case BankType::Account:
            if (updateBankTabSettings.Tab >= _player->m_activePlayerData->AccountBankTabSettings.size())
            {
                TC_LOG_DEBUG("network", "WorldSession::HandleUpdateBankTabSettings {} doesn't have bank tab {} in bank type {}.",
                    _player->GetGUID(), updateBankTabSettings.Tab, updateBankTabSettings.BankType);
                return;
            }
            _player->SetAccountBankTabSettings(updateBankTabSettings.Tab, updateBankTabSettings.Settings.Name,
                updateBankTabSettings.Settings.Icon, updateBankTabSettings.Settings.Description, updateBankTabSettings.Settings.DepositFlags);
            break;
        default:
            TC_LOG_DEBUG("network", "WorldSession::HandleUpdateBankTabSettings {} - Bank type {} is not supported.",
                _player->GetGUID(), updateBankTabSettings.BankType);
            break;
    }
}

// Try every tab in the bank starting with the priority pick, falling back to any tab
// without DisableAutoSort. Returns true if the item was deposited.
static bool TryAutoDepositItem(Player* player, BankType bank, Item* item)
{
    uint8 const bagStart = (bank == BankType::Account) ? static_cast<uint8>(ACCOUNT_BANK_SLOT_BAG_START) : static_cast<uint8>(BANK_SLOT_BAG_START);
    uint8 const tabCount = (bank == BankType::Account) ? player->GetAccountBankTabCount() : player->GetCharacterBankTabCount();
    if (!tabCount)
        return false;

    auto attemptStore = [&](uint8 bag) -> bool
    {
        ItemPosCountVec dest;
        InventoryResult msg = (bank == BankType::Account)
            ? player->CanAccountBankItem(bag, NULL_SLOT, dest, item, false)
            : player->CanBankItem(bag, NULL_SLOT, dest, item, false);
        if (msg != EQUIP_ERR_OK)
            return false;

        if (dest.size() == 1 && dest[0].pos == item->GetPos())
            return false;

        player->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
        player->BankItem(dest, item, true);
        return true;
    };

    // First, try the tab whose DepositFlags match the item's category.
    if (int8 preferred = player->PickAutoDepositTab(bank, item); preferred >= 0)
        if (attemptStore(bagStart + uint8(preferred)))
            return true;

    // Fall back to scanning all other tabs in order.
    for (uint8 i = 0; i < tabCount; ++i)
        if (attemptStore(bagStart + i))
            return true;

    return false;
}

void WorldSession::HandleAutoDepositCharacterBank(WorldPackets::Bank::AutoDepositCharacterBank const& autoDepositCharacterBank)
{
    if (!CanUseBank(autoDepositCharacterBank.Banker))
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAutoDepositCharacterBank - {} not found or you can't interact with him.", autoDepositCharacterBank.Banker);
        return;
    }

    if (_player->GetCharacterBankTabCount() == 0)
    {
        _player->SendEquipError(EQUIP_ERR_BANK_FULL);
        return;
    }

    bool anyDeposited = false;
    for (Item* item : _player->GetCraftingReagentItemsToDeposit())
    {
        if (!TryAutoDepositItem(_player, BankType::Character, item))
        {
            if (!anyDeposited)
                _player->SendEquipError(EQUIP_ERR_BANK_FULL, item, nullptr);
            break;
        }
        anyDeposited = true;
    }
}

void WorldSession::HandleAutoDepositAccountBank(WorldPackets::Bank::AutoDepositAccountBank const& autoDepositAccountBank)
{
    if (!CanUseBank(autoDepositAccountBank.Banker))
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAutoDepositAccountBank - {} not found or you can't interact with him.", autoDepositAccountBank.Banker);
        return;
    }

    if (_player->GetAccountBankTabCount() == 0)
    {
        _player->SendEquipError(EQUIP_ERR_BANK_FULL);
        return;
    }

    bool anyDeposited = false;
    for (Item* item : _player->GetItemsForBankAutoDeposit(BankType::Account, autoDepositAccountBank.IncludeReagents))
    {
        if (!TryAutoDepositItem(_player, BankType::Account, item))
        {
            if (!anyDeposited)
                _player->SendEquipError(EQUIP_ERR_BANK_FULL, item, nullptr);
            break;
        }
        anyDeposited = true;
    }
}

void WorldSession::HandleAccountBankDepositMoney(WorldPackets::Bank::AccountBankDepositMoney const& accountBankDepositMoney)
{
    if (!CanUseBank(accountBankDepositMoney.Banker))
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAccountBankDepositMoney - {} not found or you can't interact with him.", accountBankDepositMoney.Banker.ToString());
        return;
    }

    if (!accountBankDepositMoney.Money)
        return;

    if (!_player->HasEnoughMoney(accountBankDepositMoney.Money))
        return;

    if (_player->GetAccountBankCoinage() > MAX_MONEY_AMOUNT - accountBankDepositMoney.Money)
        return;

    _player->ModifyMoney(-int64(accountBankDepositMoney.Money));
    _player->ModifyAccountBankCoinage(int64(accountBankDepositMoney.Money));
}

void WorldSession::HandleAccountBankWithdrawMoney(WorldPackets::Bank::AccountBankWithdrawMoney const& accountBankWithdrawMoney)
{
    if (!CanUseBank(accountBankWithdrawMoney.Banker))
    {
        TC_LOG_DEBUG("network", "WORLD: HandleAccountBankWithdrawMoney - {} not found or you can't interact with him.", accountBankWithdrawMoney.Banker.ToString());
        return;
    }

    if (!accountBankWithdrawMoney.Money)
        return;

    if (_player->GetAccountBankCoinage() < accountBankWithdrawMoney.Money)
        return;

    if (_player->GetMoney() > MAX_MONEY_AMOUNT - accountBankWithdrawMoney.Money)
    {
        _player->SendEquipError(EQUIP_ERR_TOO_MUCH_GOLD, nullptr, nullptr);
        return;
    }

    _player->ModifyAccountBankCoinage(-int64(accountBankWithdrawMoney.Money));
    _player->ModifyMoney(int64(accountBankWithdrawMoney.Money));
}

void WorldSession::SendShowBank(ObjectGuid guid, PlayerInteractionType interactionType)
{
    _player->PlayerTalkClass->GetInteractionData().StartInteraction(guid, interactionType);

    WorldPackets::NPC::NPCInteractionOpenResult npcInteraction;
    npcInteraction.Npc = guid;
    npcInteraction.InteractionType = interactionType;
    npcInteraction.Success = true;
    SendPacket(npcInteraction.Write());
}

// The warband bank slot map is account-global (auth database) while item rows are realm-local.
// Only one session per battle.net account can exist, so the bank is exclusively owned by the
// online session: before it is loaded, items homed to sibling realms are pulled into this
// realm's database and the slot map is re-pointed here.
void WorldSession::MigrateAccountBankItems()
{
    uint32 const battlenetAccountId = GetBattlenetAccountId();
    if (!battlenetAccountId)
        return;

    // without sibling schemas every banked item is homed here, nothing can need migrating
    if (GetCrossRealmSchemas().empty())
        return;

    uint32 const currentRealmId = sRealmList->GetCurrentRealmId().Realm;

    LoginDatabasePreparedStatement* sourcesStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_BANK_ITEM_SOURCES);
    sourcesStmt->setUInt32(0, battlenetAccountId);
    PreparedQueryResult sources = LoginDatabase.Query(sourcesStmt);
    if (!sources)
        return;

    std::vector<std::pair<uint64, CrossRealmSchema const*>> pendingItems; // item guid -> source realm
    do
    {
        Field* fields = sources->Fetch();
        uint32 const sourceRealm = fields[3].GetUInt32();
        if (sourceRealm == currentRealmId)
            continue;

        auto schemaItr = std::find_if(GetCrossRealmSchemas().begin(), GetCrossRealmSchemas().end(),
            [sourceRealm](CrossRealmSchema const& crossRealm) { return crossRealm.HomeRealmId == sourceRealm; });
        if (schemaItr == GetCrossRealmSchemas().end())
        {
            TC_LOG_ERROR("entities.player", "Warband bank item {} of battle.net account {} is homed to realm {} which has no configured characters schema, skipping",
                fields[2].GetUInt64(), battlenetAccountId, sourceRealm);
            continue;
        }

        pendingItems.emplace_back(fields[2].GetUInt64(), &*schemaItr);
    } while (sources->NextRow());

    if (pendingItems.empty())
        return;

    std::string const authSchema = LoginDatabase.GetConnectionInfo()->database;
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto const& [oldGuid, sourceRealm] : pendingItems)
    {
        uint64 const newGuid = sObjectMgr->GetGenerator<HighGuid::Item>().Generate();
        std::string const& sourceSchema = sourceRealm->Schema;

        trans->PAppend("INSERT INTO item_instance (guid, itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomBonusListId, durability, playedTime, createTime, text, battlePetSpeciesId, battlePetBreedData, battlePetLevel, battlePetDisplayId, context, bonusListIDs) "
            "SELECT {}, itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomBonusListId, durability, playedTime, createTime, text, battlePetSpeciesId, battlePetBreedData, battlePetLevel, battlePetDisplayId, context, bonusListIDs FROM {}.item_instance WHERE guid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_gems (itemGuid, gemItemId1, gemBonuses1, gemContext1, gemScalingLevel1, gemItemId2, gemBonuses2, gemContext2, gemScalingLevel2, gemItemId3, gemBonuses3, gemContext3, gemScalingLevel3) "
            "SELECT {}, gemItemId1, gemBonuses1, gemContext1, gemScalingLevel1, gemItemId2, gemBonuses2, gemContext2, gemScalingLevel2, gemItemId3, gemBonuses3, gemContext3, gemScalingLevel3 FROM {}.item_instance_gems WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_transmog (itemGuid, itemModifiedAppearanceAllSpecs, itemModifiedAppearanceSpec1, itemModifiedAppearanceSpec2, itemModifiedAppearanceSpec3, itemModifiedAppearanceSpec4, itemModifiedAppearanceSpec5, spellItemEnchantmentAllSpecs, spellItemEnchantmentSpec1, spellItemEnchantmentSpec2, spellItemEnchantmentSpec3, spellItemEnchantmentSpec4, spellItemEnchantmentSpec5, secondaryItemModifiedAppearanceAllSpecs, secondaryItemModifiedAppearanceSpec1, secondaryItemModifiedAppearanceSpec2, secondaryItemModifiedAppearanceSpec3, secondaryItemModifiedAppearanceSpec4, secondaryItemModifiedAppearanceSpec5) "
            "SELECT {}, itemModifiedAppearanceAllSpecs, itemModifiedAppearanceSpec1, itemModifiedAppearanceSpec2, itemModifiedAppearanceSpec3, itemModifiedAppearanceSpec4, itemModifiedAppearanceSpec5, spellItemEnchantmentAllSpecs, spellItemEnchantmentSpec1, spellItemEnchantmentSpec2, spellItemEnchantmentSpec3, spellItemEnchantmentSpec4, spellItemEnchantmentSpec5, secondaryItemModifiedAppearanceAllSpecs, secondaryItemModifiedAppearanceSpec1, secondaryItemModifiedAppearanceSpec2, secondaryItemModifiedAppearanceSpec3, secondaryItemModifiedAppearanceSpec4, secondaryItemModifiedAppearanceSpec5 FROM {}.item_instance_transmog WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_modifiers (itemGuid, fixedScalingLevel, artifactKnowledgeLevel, craftingModifiedStat1, craftingModifiedStat2) "
            "SELECT {}, fixedScalingLevel, artifactKnowledgeLevel, craftingModifiedStat1, craftingModifiedStat2 FROM {}.item_instance_modifiers WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_artifact (itemGuid, xp, artifactAppearanceId, artifactTierId) "
            "SELECT {}, xp, artifactAppearanceId, artifactTierId FROM {}.item_instance_artifact WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_artifact_powers (itemGuid, artifactPowerId, purchasedRank) "
            "SELECT {}, artifactPowerId, purchasedRank FROM {}.item_instance_artifact_powers WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_azerite (itemGuid, xp, level, knowledgeLevel, selectedAzeriteEssences1specId, selectedAzeriteEssences1azeriteEssenceId1, selectedAzeriteEssences1azeriteEssenceId2, selectedAzeriteEssences1azeriteEssenceId3, selectedAzeriteEssences1azeriteEssenceId4, selectedAzeriteEssences2specId, selectedAzeriteEssences2azeriteEssenceId1, selectedAzeriteEssences2azeriteEssenceId2, selectedAzeriteEssences2azeriteEssenceId3, selectedAzeriteEssences2azeriteEssenceId4, selectedAzeriteEssences3specId, selectedAzeriteEssences3azeriteEssenceId1, selectedAzeriteEssences3azeriteEssenceId2, selectedAzeriteEssences3azeriteEssenceId3, selectedAzeriteEssences3azeriteEssenceId4, selectedAzeriteEssences4specId, selectedAzeriteEssences4azeriteEssenceId1, selectedAzeriteEssences4azeriteEssenceId2, selectedAzeriteEssences4azeriteEssenceId3, selectedAzeriteEssences4azeriteEssenceId4) "
            "SELECT {}, xp, level, knowledgeLevel, selectedAzeriteEssences1specId, selectedAzeriteEssences1azeriteEssenceId1, selectedAzeriteEssences1azeriteEssenceId2, selectedAzeriteEssences1azeriteEssenceId3, selectedAzeriteEssences1azeriteEssenceId4, selectedAzeriteEssences2specId, selectedAzeriteEssences2azeriteEssenceId1, selectedAzeriteEssences2azeriteEssenceId2, selectedAzeriteEssences2azeriteEssenceId3, selectedAzeriteEssences2azeriteEssenceId4, selectedAzeriteEssences3specId, selectedAzeriteEssences3azeriteEssenceId1, selectedAzeriteEssences3azeriteEssenceId2, selectedAzeriteEssences3azeriteEssenceId3, selectedAzeriteEssences3azeriteEssenceId4, selectedAzeriteEssences4specId, selectedAzeriteEssences4azeriteEssenceId1, selectedAzeriteEssences4azeriteEssenceId2, selectedAzeriteEssences4azeriteEssenceId3, selectedAzeriteEssences4azeriteEssenceId4 FROM {}.item_instance_azerite WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_azerite_empowered (itemGuid, azeritePowerId1, azeritePowerId2, azeritePowerId3, azeritePowerId4, azeritePowerId5) "
            "SELECT {}, azeritePowerId1, azeritePowerId2, azeritePowerId3, azeritePowerId4, azeritePowerId5 FROM {}.item_instance_azerite_empowered WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_azerite_milestone_power (itemGuid, azeriteItemMilestonePowerId) "
            "SELECT {}, azeriteItemMilestonePowerId FROM {}.item_instance_azerite_milestone_power WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);
        trans->PAppend("INSERT INTO item_instance_azerite_unlocked_essence (itemGuid, azeriteEssenceId, rank) "
            "SELECT {}, azeriteEssenceId, rank FROM {}.item_instance_azerite_unlocked_essence WHERE itemGuid = {}",
            newGuid, sourceSchema, oldGuid);

        trans->PAppend("DELETE FROM {}.item_instance WHERE guid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_gems WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_transmog WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_modifiers WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_artifact WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_artifact_powers WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_azerite WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_azerite_empowered WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_azerite_milestone_power WHERE itemGuid = {}", sourceSchema, oldGuid);
        trans->PAppend("DELETE FROM {}.item_instance_azerite_unlocked_essence WHERE itemGuid = {}", sourceSchema, oldGuid);

        // pointer update rides the same transaction so a crash can never orphan the auth row;
        // scoped by sourceRealm because sibling realms issue the same numeric item guids
        trans->PAppend("UPDATE {}.account_bank_item SET item = {}, sourceRealm = {} WHERE battlenetAccountId = {} AND item = {} AND sourceRealm = {}",
            authSchema, newGuid, currentRealmId, battlenetAccountId, oldGuid, sourceRealm->HomeRealmId);
    }

    CharacterDatabase.CommitTransaction(trans);
    TC_LOG_INFO("entities.player", "Migrated {} warband bank item(s) of battle.net account {} into this realm's database", pendingItems.size(), battlenetAccountId);
}
