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
#include "PerksProgramMgr.h"
#include "PerksProgramPackets.h"
#include "Player.h"
#include "Log.h"
#include "GameTime.h"

void WorldSession::HandlePerksProgramStatusRequest(WorldPackets::PerksProgram::PerksProgramStatusRequest& packet)
{
    if (!sPerksProgramMgr->GetAvailableVendorItemIDs().empty())
        sPerksProgramMgr->SendOpenPerksProgram(GetPlayer());
    else
        sPerksProgramMgr->SendClosePerksProgram(GetPlayer());
}

void WorldSession::HandlePerksProgramItemsRefreshed(WorldPackets::PerksProgram::PerksProgramItemsRefreshed& packet)
{
    sPerksProgramMgr->SendOpenPerksProgram(GetPlayer());
}

void WorldSession::HandlePerksProgramRequestPurchase(WorldPackets::PerksProgram::PerksProgramRequestPurchase& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    int32 vendorItemID = packet.VendorItemID;

    if (!sPerksProgramMgr->CanPurchaseItem(player, vendorItemID))
    {
        PerksVendorItemTemplate const* templ = sPerksProgramMgr->GetVendorItemTemplate(vendorItemID);

        if (!templ)
        {
            sPerksProgramMgr->SendPurchaseResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_ITEM_NOT_FOUND);
            return;
        }

        if (templ->Disabled)
        {
            sPerksProgramMgr->SendPurchaseResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_ITEM_DISABLED);
            return;
        }

        if (!templ->DoesNotExpire && templ->AvailableUntil > 0)
        {
            uint32 now = static_cast<uint32>(GameTime::GetGameTime());
            if (templ->AvailableUntil <= now)
            {
                sPerksProgramMgr->SendPurchaseResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_ITEM_EXPIRED);
                return;
            }
        }

        if (sPerksProgramMgr->GetCurrencyAmount(player) < templ->Price)
        {
            sPerksProgramMgr->SendPurchaseResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_NOT_ENOUGH_CURRENCY);
            return;
        }

        sPerksProgramMgr->SendPurchaseResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_ITEM_ALREADY_OWNED);
        return;
    }

    if (sPerksProgramMgr->PurchaseItem(player, vendorItemID))
    {
        TC_LOG_INFO("network", "Player {} purchased perks vendor item {}", player->GetGUID().ToString(), vendorItemID);
    }
}

void WorldSession::HandlePerksProgramRequestCartCheckout(WorldPackets::PerksProgram::PerksProgramRequestCartCheckout& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    int32 totalPrice = 0;
    std::vector<int32> validItems;

    for (int32 vendorItemID : packet.VendorItemIDs)
    {
        PerksVendorItemTemplate const* templ = sPerksProgramMgr->GetVendorItemTemplate(vendorItemID);
        if (templ && sPerksProgramMgr->CanPurchaseItem(player, vendorItemID))
        {
            totalPrice += templ->Price;
            validItems.push_back(vendorItemID);
        }
    }

    if (sPerksProgramMgr->GetCurrencyAmount(player) < totalPrice)
    {
        sPerksProgramMgr->SendPurchaseResult(player, 0, PerksProgram::PERKS_PROGRAM_ERR_NOT_ENOUGH_CURRENCY);
        return;
    }

    for (int32 vendorItemID : validItems)
    {
        sPerksProgramMgr->PurchaseItem(player, vendorItemID);
        TC_LOG_INFO("network", "Player {} purchased perks vendor item {} (cart checkout)", player->GetGUID().ToString(), vendorItemID);
    }
}

void WorldSession::HandlePerksProgramRequestRefund(WorldPackets::PerksProgram::PerksProgramRequestRefund& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    int32 vendorItemID = packet.VendorItemID;

    if (!sPerksProgramMgr->CanRefundItem(player, vendorItemID))
    {
        PerksVendorItemTemplate const* templ = sPerksProgramMgr->GetVendorItemTemplate(vendorItemID);

        if (!templ)
        {
            sPerksProgramMgr->SendRefundResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_ITEM_NOT_FOUND);
            return;
        }

        sPerksProgramMgr->SendRefundResult(player, vendorItemID, PerksProgram::PERKS_PROGRAM_ERR_ITEM_NOT_REFUNDABLE);
        return;
    }

    if (sPerksProgramMgr->RefundItem(player, vendorItemID))
    {
        TC_LOG_INFO("network", "Player {} refunded perks vendor item {}", player->GetGUID().ToString(), vendorItemID);
    }
}

void WorldSession::HandlePerksProgramSetFrozenVendorItem(WorldPackets::PerksProgram::PerksProgramSetFrozenVendorItem& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    sPerksProgramMgr->SetFrozenItem(player, packet.VendorItemID);
}

void WorldSession::HandlePerksProgramRequestPendingRewards(WorldPackets::PerksProgram::PerksProgramRequestPendingRewards& packet)
{
    WorldPackets::PerksProgram::PerksProgramPendingRewards rewardsPacket;
    GetPlayer()->SendDirectMessage(rewardsPacket.Write());
}

void WorldSession::HandlePerksProgramGetRecentPurchases(WorldPackets::PerksProgram::PerksProgramGetRecentPurchases& packet)
{
    sPerksProgramMgr->SendOpenPerksProgram(GetPlayer());
}
