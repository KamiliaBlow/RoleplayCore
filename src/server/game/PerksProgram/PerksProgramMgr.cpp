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

#include "PerksProgramMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include "GameTime.h"
#include "CollectionMgr.h"
#include "DB2Stores.h"
#include <unordered_map>

namespace
{
    std::unordered_map<ObjectGuid, PerksProgram::PlayerPerksData> _playerPerksData;
    uint32 const REFUND_WINDOW_SECONDS = 3600;
    uint32 const TRADING_POST_CURRENCY_ID = 2032;
}

PerksProgram::PerksProgramMgr* PerksProgram::PerksProgramMgr::instance()
{
    static PerksProgramMgr instance;
    return &instance;
}

void PerksProgram::PerksProgramMgr::LoadVendorItems()
{
    uint32 oldMSTime = getMSTime();

    if (QueryResult result = WorldDatabase.Query("SELECT ID, CategoryID, Name, Description, IconTexture, Price, OriginalPrice, "
        "AvailableUntil, DoesNotExpire, Disabled, ItemID, MountID, SpeciesID, TransmogSetID, ItemModifiedAppearanceID, "
        "TransmogIllusionID, ToyID, WarbandSceneID, Quality, InvType, MountTypeName FROM perks_vendor_item"))
    {
        do
        {
            Field* fields = result->Fetch();
            PerksVendorItemTemplate item;

            item.ID = fields[0].GetInt32();
            item.CategoryID = fields[1].GetInt32();
            item.Name = fields[2].GetString();
            item.Description = fields[3].GetString();
            item.IconTexture = fields[4].GetString();
            item.Price = fields[5].GetInt32();
            item.OriginalPrice = fields[6].GetInt32();
            item.AvailableUntil = fields[7].GetUInt32();
            item.DoesNotExpire = fields[8].GetBool();
            item.Disabled = fields[9].GetBool();
            item.ItemID = fields[10].GetInt32();
            item.MountID = fields[11].GetInt32();
            item.SpeciesID = fields[12].GetInt32();
            item.TransmogSetID = fields[13].GetInt32();
            item.ItemModifiedAppearanceID = fields[14].GetInt32();
            item.TransmogIllusionID = fields[15].GetInt32();
            item.ToyID = fields[16].GetInt32();
            item.WarbandSceneID = fields[17].GetInt32();
            item.Quality = fields[18].GetUInt8();
            item.InvType = fields[19].GetString();
            item.MountTypeName = fields[20].GetString();

            _vendorItemTemplates[item.ID] = item;
        } while (result->NextRow());
    }

    if (QueryResult subItemsResult = WorldDatabase.Query("SELECT VendorItemID, Name, ItemID, ItemModifiedAppearanceID, Quality, InvType FROM perks_vendor_sub_item"))
    {
        do
        {
            Field* fields = subItemsResult->Fetch();
            int32 vendorItemID = fields[0].GetInt32();

            auto it = _vendorItemTemplates.find(vendorItemID);
            if (it != _vendorItemTemplates.end())
            {
                WorldPackets::PerksProgram::PerksVendorSubItem subItem;
                subItem.Name = fields[1].GetString();
                subItem.ItemID = fields[2].GetInt32();
                subItem.ItemModifiedAppearanceID = fields[3].GetInt32();
                subItem.Quality = fields[4].GetUInt8();
                subItem.InvType = fields[5].GetString();
                it->second.SubItems.push_back(subItem);
            }
        } while (subItemsResult->NextRow());
    }

    if (QueryResult uiGroupResult = WorldDatabase.Query("SELECT VendorItemID, ID, Name, Priority FROM perks_vendor_item_ui_group"))
    {
        do
        {
            Field* fields = uiGroupResult->Fetch();
            int32 vendorItemID = fields[0].GetInt32();

            auto it = _vendorItemTemplates.find(vendorItemID);
            if (it != _vendorItemTemplates.end())
            {
                WorldPackets::PerksProgram::PerksVendorItemUIGroup uiGroup;
                uiGroup.ID = fields[1].GetInt32();
                uiGroup.Name = fields[2].GetString();
                uiGroup.Priority = fields[3].GetInt32();
                it->second.UIGroupInfo = uiGroup;
            }
        } while (uiGroupResult->NextRow());
    }

    if (QueryResult displayInfoResult = WorldDatabase.Query("SELECT VendorItemID, OverrideModelSceneID, CreatureDisplayInfoID, "
        "MainHandItemModifiedAppearanceID, OffHandItemModifiedAppearanceID FROM perks_vendor_item_display_info"))
    {
        do
        {
            Field* fields = displayInfoResult->Fetch();
            int32 vendorItemID = fields[0].GetInt32();

            auto it = _vendorItemTemplates.find(vendorItemID);
            if (it != _vendorItemTemplates.end())
            {
                if (!fields[1].IsNull())
                    it->second.OverrideModelSceneID = fields[1].GetInt32();
                if (!fields[2].IsNull())
                    it->second.CreatureDisplayInfoID = fields[2].GetInt32();
                if (!fields[3].IsNull())
                    it->second.MainHandItemModifiedAppearanceID = fields[3].GetInt32();
                if (!fields[4].IsNull())
                    it->second.OffHandItemModifiedAppearanceID = fields[4].GetInt32();
            }
        } while (displayInfoResult->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} perks vendor items in {} ms", _vendorItemTemplates.size(), GetMSTimeDiffToNow(oldMSTime));
}

void PerksProgram::PerksProgramMgr::LoadVendorCategories()
{
    uint32 oldMSTime = getMSTime();

    if (QueryResult result = WorldDatabase.Query("SELECT ID, DisplayName, DefaultUIModelSceneID FROM perks_vendor_category"))
    {
        do
        {
            Field* fields = result->Fetch();
            PerksVendorCategoryTemplate category;

            category.ID = fields[0].GetInt32();
            category.DisplayName = fields[1].GetString();
            category.DefaultUIModelSceneID = fields[2].GetInt32();

            _vendorCategoryTemplates[category.ID] = category;
        } while (result->NextRow());
    }

    TC_LOG_INFO("server.loading", ">> Loaded {} perks vendor categories in {} ms", _vendorCategoryTemplates.size(), GetMSTimeDiffToNow(oldMSTime));
}

void PerksProgram::PerksProgramMgr::LoadPlayerData(Player* player)
{
    if (!player)
        return;

    ObjectGuid guid = player->GetGUID();
    PlayerPerksData& data = _playerPerksData[guid];
    data.CurrencyAmount = 0;
    data.PurchasedItems.clear();
    data.PurchaseTimes.clear();
    data.FrozenVendorItemID = 0;

    if (QueryResult result = CharacterDatabase.Query("SELECT CurrencyAmount, FrozenVendorItemID FROM character_perks_program WHERE guid = {}", player->GetGUID().GetCounter()))
    {
        data.CurrencyAmount = (*result)[0].GetInt32();
        data.FrozenVendorItemID = (*result)[1].GetInt32();
    }

    if (QueryResult purchasesResult = CharacterDatabase.Query("SELECT VendorItemID, PurchaseTime FROM character_perks_program_purchases WHERE guid = {}", player->GetGUID().GetCounter()))
    {
        do
        {
            int32 vendorItemID = (*purchasesResult)[0].GetInt32();
            int64 purchaseTime = (*purchasesResult)[1].GetInt64();

            data.PurchasedItems.insert(vendorItemID);
            data.PurchaseTimes[vendorItemID] = purchaseTime;
        } while (purchasesResult->NextRow());
    }
}

void PerksProgram::PerksProgramMgr::SavePlayerData(Player* player)
{
    if (!player)
        return;

    ObjectGuid guid = player->GetGUID();
    auto it = _playerPerksData.find(guid);
    if (it == _playerPerksData.end())
        return;

    PlayerPerksData const& data = it->second;

    CharacterDatabase.Execute("DELETE FROM character_perks_program WHERE guid = {}", guid.GetCounter());
    CharacterDatabase.Execute("DELETE FROM character_perks_program_purchases WHERE guid = {}", guid.GetCounter());

    CharacterDatabase.Execute("INSERT INTO character_perks_program (guid, CurrencyAmount, FrozenVendorItemID) VALUES ({}, {}, {})",
        guid.GetCounter(), data.CurrencyAmount, data.FrozenVendorItemID);

    for (int32 vendorItemID : data.PurchasedItems)
    {
        auto timeIt = data.PurchaseTimes.find(vendorItemID);
        int64 purchaseTime = timeIt != data.PurchaseTimes.end() ? timeIt->second : 0;
        CharacterDatabase.Execute("INSERT INTO character_perks_program_purchases (guid, VendorItemID, PurchaseTime) VALUES ({}, {}, {})",
            guid.GetCounter(), vendorItemID, purchaseTime);
    }
}

PerksVendorItemTemplate const* PerksProgram::PerksProgramMgr::GetVendorItemTemplate(int32 vendorItemID) const
{
    auto it = _vendorItemTemplates.find(vendorItemID);
    if (it != _vendorItemTemplates.end())
        return &it->second;
    return nullptr;
}

PerksVendorCategoryTemplate const* PerksProgram::PerksProgramMgr::GetVendorCategoryTemplate(int32 categoryID) const
{
    auto it = _vendorCategoryTemplates.find(categoryID);
    if (it != _vendorCategoryTemplates.end())
        return &it->second;
    return nullptr;
}

std::vector<int32> PerksProgram::PerksProgramMgr::GetAvailableVendorItemIDs() const
{
    std::vector<int32> ids;
    ids.reserve(_vendorItemTemplates.size());

    time_t now = GameTime::GetGameTime();

    for (auto const& pair : _vendorItemTemplates)
    {
        if (!pair.second.Disabled)
        {
            if (pair.second.DoesNotExpire || pair.second.AvailableUntil == 0 || pair.second.AvailableUntil > static_cast<uint32>(now))
            {
                ids.push_back(pair.first);
            }
        }
    }

    return ids;
}

std::vector<int32> PerksProgram::PerksProgramMgr::GetAvailableCategoryIDs() const
{
    std::vector<int32> ids;
    ids.reserve(_vendorCategoryTemplates.size());

    for (auto const& pair : _vendorCategoryTemplates)
        ids.push_back(pair.first);

    return ids;
}

WorldPackets::PerksProgram::PerksVendorItemInfo PerksProgram::PerksProgramMgr::BuildVendorItemInfo(PerksVendorItemTemplate const* templ, Player const* player) const
{
    WorldPackets::PerksProgram::PerksVendorItemInfo info;

    if (!templ)
        return info;

    info.PerksVendorItemID = templ->ID;
    info.PerksVendorCategoryID = templ->CategoryID;
    info.Name = templ->Name;
    info.Description = templ->Description;
    info.IconTexture = templ->IconTexture;
    info.Price = templ->Price;
    info.ItemID = templ->ItemID;
    info.MountID = templ->MountID;
    info.SpeciesID = templ->SpeciesID;
    info.TransmogSetID = templ->TransmogSetID;
    info.ItemModifiedAppearanceID = templ->ItemModifiedAppearanceID;
    info.InvType = templ->InvType;
    info.Quality = templ->Quality;
    info.MountTypeName = templ->MountTypeName;
    info.DoesNotExpire = templ->DoesNotExpire;

    if (templ->OriginalPrice > 0 && templ->OriginalPrice != templ->Price)
    {
        info.OriginalPrice = templ->OriginalPrice;
        info.ShowSaleBanner = true;
    }

    if (!templ->DoesNotExpire && templ->AvailableUntil > 0)
    {
        time_t now = GameTime::GetGameTime();
        if (templ->AvailableUntil > static_cast<uint32>(now))
            info.TimeRemaining = Seconds(templ->AvailableUntil - static_cast<uint32>(now));
    }
    else
    {
        info.TimeRemaining = Seconds(0);
        info.DoesNotExpire = true;
    }

    info.SubItems = templ->SubItems;
    info.SubItemsLoaded = !templ->SubItems.empty();
    info.UIGroupInfo = templ->UIGroupInfo;

    if (player)
    {
        auto it = _playerPerksData.find(player->GetGUID());
        if (it != _playerPerksData.end())
        {
            PlayerPerksData const& data = it->second;
            info.Purchased = data.PurchasedItems.count(templ->ID) > 0;

            if (info.Purchased)
            {
                auto timeIt = data.PurchaseTimes.find(templ->ID);
                if (timeIt != data.PurchaseTimes.end())
                {
                    time_t now = GameTime::GetGameTime();
                    info.Refundable = (static_cast<uint64>(now) - static_cast<uint64>(timeIt->second)) < REFUND_WINDOW_SECONDS;
                }
            }
        }
    }

    return info;
}

WorldPackets::PerksProgram::PerksVendorCategory PerksProgram::PerksProgramMgr::BuildVendorCategoryInfo(PerksVendorCategoryTemplate const* templ) const
{
    WorldPackets::PerksProgram::PerksVendorCategory info;

    if (templ)
    {
        info.ID = templ->ID;
        info.DisplayName = templ->DisplayName;
        info.DefaultUIModelSceneID = templ->DefaultUIModelSceneID;
    }

    return info;
}

int32 PerksProgram::PerksProgramMgr::GetCurrencyAmount(Player const* player) const
{
    if (!player)
        return 0;

    auto it = _playerPerksData.find(player->GetGUID());
    if (it != _playerPerksData.end())
        return it->second.CurrencyAmount;

    return 0;
}

void PerksProgram::PerksProgramMgr::ModifyCurrency(Player* player, int32 amount, bool sendUpdate)
{
    if (!player)
        return;

    int32 oldAmount = GetCurrencyAmount(player);
    int32 newAmount = std::max(0, oldAmount + amount);

    PlayerPerksData& data = _playerPerksData[player->GetGUID()];
    data.CurrencyAmount = newAmount;

    if (sendUpdate)
        SendCurrencyUpdate(player, oldAmount, newAmount);
}

bool PerksProgram::PerksProgramMgr::CanPurchaseItem(Player const* player, int32 vendorItemID) const
{
    PerksVendorItemTemplate const* templ = GetVendorItemTemplate(vendorItemID);
    if (!templ || templ->Disabled)
        return false;

    if (!templ->DoesNotExpire && templ->AvailableUntil > 0)
    {
        time_t now = GameTime::GetGameTime();
        if (templ->AvailableUntil <= static_cast<uint32>(now))
            return false;
    }

    if (GetCurrencyAmount(player) < templ->Price)
        return false;

    auto it = _playerPerksData.find(player->GetGUID());
    if (it != _playerPerksData.end() && it->second.PurchasedItems.count(vendorItemID) > 0)
        return false;

    return true;
}

bool PerksProgram::PerksProgramMgr::PurchaseItem(Player* player, int32 vendorItemID)
{
    if (!CanPurchaseItem(player, vendorItemID))
        return false;

    PerksVendorItemTemplate const* templ = GetVendorItemTemplate(vendorItemID);
    if (!templ)
        return false;

    PlayerPerksData& data = _playerPerksData[player->GetGUID()];
    data.CurrencyAmount -= templ->Price;
    data.PurchasedItems.insert(vendorItemID);
    data.PurchaseTimes[vendorItemID] = static_cast<int64>(GameTime::GetGameTime());

    if (templ->ItemID > 0)
        player->StoreNewItemInBestSlots(templ->ItemID, 1, ItemContext::NONE);

    if (templ->MountID > 0)
    {
        MountEntry const* mount = sDB2Manager.GetMountById(templ->MountID);
        if (mount)
            player->GetSession()->GetCollectionMgr()->AddMount(mount->SourceSpellID, MOUNT_STATUS_NONE, false, true);
    }

    if (templ->ToyID > 0)
        player->GetSession()->GetCollectionMgr()->AddToy(templ->ToyID, false, false);

    if (templ->ItemModifiedAppearanceID > 0)
        player->GetSession()->GetCollectionMgr()->AddItemAppearance(templ->ItemModifiedAppearanceID);

    if (templ->TransmogSetID > 0)
        player->GetSession()->GetCollectionMgr()->AddTransmogSet(templ->TransmogSetID);

    if (templ->TransmogIllusionID > 0)
        player->GetSession()->GetCollectionMgr()->AddTransmogIllusion(templ->TransmogIllusionID);

    if (templ->WarbandSceneID > 0)
        player->GetSession()->GetCollectionMgr()->AddWarbandScene(templ->WarbandSceneID);

    WorldPackets::PerksProgram::PerksProgramPurchaseSuccess success;
    success.VendorItemID = vendorItemID;
    success.NewCurrencyAmount = data.CurrencyAmount;
    success.ItemInfo = BuildVendorItemInfo(templ, player);
    player->SendDirectMessage(success.Write());

    return true;
}

bool PerksProgram::PerksProgramMgr::CanRefundItem(Player const* player, int32 vendorItemID) const
{
    auto it = _playerPerksData.find(player->GetGUID());
    if (it == _playerPerksData.end())
        return false;

    PlayerPerksData const& data = it->second;

    if (data.PurchasedItems.count(vendorItemID) == 0)
        return false;

    auto timeIt = data.PurchaseTimes.find(vendorItemID);
    if (timeIt == data.PurchaseTimes.end())
        return false;

    time_t now = GameTime::GetGameTime();
    if ((static_cast<uint64>(now) - static_cast<uint64>(timeIt->second)) >= REFUND_WINDOW_SECONDS)
        return false;

    return true;
}

bool PerksProgram::PerksProgramMgr::RefundItem(Player* player, int32 vendorItemID)
{
    if (!CanRefundItem(player, vendorItemID))
        return false;

    PerksVendorItemTemplate const* templ = GetVendorItemTemplate(vendorItemID);
    if (!templ)
        return false;

    PlayerPerksData& data = _playerPerksData[player->GetGUID()];

    data.CurrencyAmount += templ->Price;
    data.PurchasedItems.erase(vendorItemID);
    data.PurchaseTimes.erase(vendorItemID);

    WorldPackets::PerksProgram::PerksProgramRefundSuccess refund;
    refund.VendorItemID = vendorItemID;
    refund.NewCurrencyAmount = data.CurrencyAmount;
    refund.ItemInfo = BuildVendorItemInfo(templ, player);
    player->SendDirectMessage(refund.Write());

    return true;
}

void PerksProgram::PerksProgramMgr::SetFrozenItem(Player* player, int32 vendorItemID)
{
    if (!player)
        return;

    PlayerPerksData& data = _playerPerksData[player->GetGUID()];
    data.FrozenVendorItemID = vendorItemID;
}

int32 PerksProgram::PerksProgramMgr::GetFrozenItem(Player const* player) const
{
    if (!player)
        return 0;

    auto it = _playerPerksData.find(player->GetGUID());
    if (it != _playerPerksData.end())
        return it->second.FrozenVendorItemID;

    return 0;
}

void PerksProgram::PerksProgramMgr::SendOpenPerksProgram(Player* player)
{
    if (!player)
        return;

    WorldPackets::PerksProgram::PerksProgramOpen packet;
    packet.CurrencyAmount = GetCurrencyAmount(player);
    packet.VendorItemIDs = GetAvailableVendorItemIDs();

    for (int32 categoryID : GetAvailableCategoryIDs())
    {
        if (PerksVendorCategoryTemplate const* category = GetVendorCategoryTemplate(categoryID))
            packet.Categories.push_back(BuildVendorCategoryInfo(category));
    }

    for (int32 itemID : packet.VendorItemIDs)
    {
        if (PerksVendorItemTemplate const* item = GetVendorItemTemplate(itemID))
            packet.Items.push_back(BuildVendorItemInfo(item, player));
    }

    player->SendDirectMessage(packet.Write());
}

void PerksProgram::PerksProgramMgr::SendClosePerksProgram(Player* player)
{
    if (!player)
        return;

    WorldPackets::PerksProgram::PerksProgramClose packet;
    player->SendDirectMessage(packet.Write());
}

void PerksProgram::PerksProgramMgr::SendCurrencyUpdate(Player* player, int32 oldValue, int32 newValue)
{
    if (!player)
        return;

    WorldPackets::PerksProgram::PerksProgramCurrencyRefresh packet;
    packet.OldValue = oldValue;
    packet.NewValue = newValue;
    player->SendDirectMessage(packet.Write());
}

void PerksProgram::PerksProgramMgr::SendPurchaseResult(Player* player, int32 vendorItemID, int32 result, Optional<std::string> errorDesc)
{
    if (!player)
        return;

    WorldPackets::PerksProgram::PerksProgramResult packet;
    packet.VendorItemID = vendorItemID;
    packet.Result = result;
    packet.ErrorDescription = errorDesc;
    player->SendDirectMessage(packet.Write());
}

void PerksProgram::PerksProgramMgr::SendRefundResult(Player* player, int32 vendorItemID, int32 result)
{
    SendPurchaseResult(player, vendorItemID, result);
}

void PerksProgram::PerksProgramMgr::SendActivityUpdate(Player* player)
{
    if (!player)
        return;

    WorldPackets::PerksProgram::PerksProgramActivityUpdate packet;
    player->SendDirectMessage(packet.Write());
}

void PerksProgram::PerksProgramMgr::AddMonthlyCurrency(Player* player, int32 amount)
{
    if (!player || amount <= 0)
        return;

    ModifyCurrency(player, amount, true);
}
