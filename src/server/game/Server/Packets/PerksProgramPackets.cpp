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

#include "PerksProgramPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::PerksProgram
{
ByteBuffer& operator<<(ByteBuffer& data, PerksVendorCategory const& category)
{
    data << int32(category.ID);
    data << int32(category.DefaultUIModelSceneID);
    data.WriteBits(category.DisplayName.size(), 9);
    data.FlushBits();
    data.WriteString(category.DisplayName);
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksVendorSubItem const& subItem)
{
    data << int32(subItem.ItemID);
    data << int32(subItem.ItemModifiedAppearanceID);
    data << uint8(subItem.Quality);
    data.WriteBits(subItem.Name.size(), 10);
    data.WriteBits(subItem.InvType.size(), 5);
    data.FlushBits();
    data.WriteString(subItem.Name);
    data.WriteString(subItem.InvType);
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksVendorItemUIGroup const& uiGroup)
{
    data << int32(uiGroup.ID);
    data << int32(uiGroup.Priority);
    data.WriteBits(uiGroup.Name.size(), 9);
    data.FlushBits();
    data.WriteString(uiGroup.Name);
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksVendorItemInfo const& itemInfo)
{
    data << int32(itemInfo.PerksVendorItemID);
    data << int32(itemInfo.PerksVendorCategoryID);
    data << int32(itemInfo.Price);
    data << itemInfo.TimeRemaining;
    data << int32(itemInfo.ItemID);
    data << int32(itemInfo.MountID);
    data << int32(itemInfo.SpeciesID);
    data << int32(itemInfo.TransmogSetID);
    data << int32(itemInfo.ItemModifiedAppearanceID);
    data << uint8(itemInfo.Quality);

    data.WriteBits(itemInfo.Name.size(), 10);
    data.WriteBits(itemInfo.Description.size(), 12);
    data.WriteBits(itemInfo.IconTexture.size(), 10);
    data.WriteBits(itemInfo.MountTypeName.size(), 10);
    data.WriteBits(itemInfo.InvType.size(), 5);
    data.WriteBits(itemInfo.SubItems.size(), 4);

    data << Bits<1>(itemInfo.OriginalPrice.has_value());
    data << Bits<1>(itemInfo.ShowSaleBanner);
    data << Bits<1>(itemInfo.DoesNotExpire);
    data << Bits<1>(itemInfo.Purchased);
    data << Bits<1>(itemInfo.Refundable);
    data << Bits<1>(itemInfo.IsPurchasePending);
    data << Bits<1>(itemInfo.SubItemsLoaded);
    data << Bits<1>(itemInfo.UIGroupInfo.has_value());
    data.FlushBits();

    data.WriteString(itemInfo.Name);
    data.WriteString(itemInfo.Description);
    data.WriteString(itemInfo.IconTexture);
    data.WriteString(itemInfo.MountTypeName);
    data.WriteString(itemInfo.InvType);

    if (itemInfo.OriginalPrice)
        data << int32(*itemInfo.OriginalPrice);

    for (PerksVendorSubItem const& subItem : itemInfo.SubItems)
        data << subItem;

    if (itemInfo.UIGroupInfo)
        data << *itemInfo.UIGroupInfo;

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksProgramItemDisplayInfo const& displayInfo)
{
    data << Bits<1>(displayInfo.OverrideModelSceneID.has_value());
    data << Bits<1>(displayInfo.CreatureDisplayInfoID.has_value());
    data << Bits<1>(displayInfo.MainHandItemModifiedAppearanceID.has_value());
    data << Bits<1>(displayInfo.OffHandItemModifiedAppearanceID.has_value());
    data.FlushBits();

    if (displayInfo.OverrideModelSceneID)
        data << int32(*displayInfo.OverrideModelSceneID);
    if (displayInfo.CreatureDisplayInfoID)
        data << int32(*displayInfo.CreatureDisplayInfoID);
    if (displayInfo.MainHandItemModifiedAppearanceID)
        data << int32(*displayInfo.MainHandItemModifiedAppearanceID);
    if (displayInfo.OffHandItemModifiedAppearanceID)
        data << int32(*displayInfo.OffHandItemModifiedAppearanceID);

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksProgramPendingChestReward const& reward)
{
    data << int32(reward.RewardTypeID);
    data << int32(reward.RewardAmount);
    data << int32(reward.ActivityMonthID);
    data << int32(reward.ThresholdOrderIndex);

    data << Bits<1>(reward.PerksVendorItemID.has_value());
    data << Bits<1>(reward.MonthRewarded.has_value());
    data.FlushBits();

    if (reward.PerksVendorItemID)
        data << int32(*reward.PerksVendorItemID);
    if (reward.MonthRewarded)
    {
        data.WriteBits(reward.MonthRewarded->size(), 8);
        data.FlushBits();
        data.WriteString(*reward.MonthRewarded);
    }

    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksProgramActivityThreshold const& threshold)
{
    data << int32(threshold.ThresholdOrderIndex);
    data << int32(threshold.CurrencyAmount);
    data << threshold.WhenComplete;
    data << Bits<1>(threshold.IsComplete);
    data.FlushBits();
    return data;
}

ByteBuffer& operator<<(ByteBuffer& data, PerksProgramActivityInfo const& activityInfo)
{
    data << int32(activityInfo.ActivityMonthID);
    data << activityInfo.MonthStartTime;
    data << activityInfo.MonthEndTime;
    data << int32(activityInfo.CurrencyEarned);
    data << int32(activityInfo.MaxCurrency);
    data << Size<uint32>(activityInfo.Thresholds);

    for (PerksProgramActivityThreshold const& threshold : activityInfo.Thresholds)
        data << threshold;

    return data;
}

WorldPacket const* PerksProgramOpen::Write()
{
    _worldPacket << int32(CurrencyAmount);
    _worldPacket << Size<uint32>(VendorItemIDs);
    _worldPacket << Size<uint32>(Categories);
    _worldPacket << Size<uint32>(Items);

    for (int32 vendorItemID : VendorItemIDs)
        _worldPacket << int32(vendorItemID);

    for (PerksVendorCategory const& category : Categories)
        _worldPacket << category;

    for (PerksVendorItemInfo const& item : Items)
        _worldPacket << item;

    return &_worldPacket;
}

WorldPacket const* PerksProgramClose::Write()
{
    return &_worldPacket;
}

void PerksProgramStatusRequest::Read()
{
}

void PerksProgramItemsRefreshed::Read()
{
}

void PerksProgramRequestPurchase::Read()
{
    _worldPacket >> VendorItemID;
}

void PerksProgramRequestCartCheckout::Read()
{
    uint32 itemCount = _worldPacket.read<uint32>();
    VendorItemIDs.resize(itemCount);
    for (uint32 i = 0; i < itemCount; ++i)
        _worldPacket >> VendorItemIDs[i];
}

void PerksProgramRequestRefund::Read()
{
    _worldPacket >> VendorItemID;
}

void PerksProgramSetFrozenVendorItem::Read()
{
    _worldPacket >> VendorItemID;
}

void PerksProgramRequestPendingRewards::Read()
{
}

void PerksProgramGetRecentPurchases::Read()
{
}

WorldPacket const* PerksProgramResult::Write()
{
    _worldPacket << int32(VendorItemID);
    _worldPacket << int32(Result);

    _worldPacket.WriteBits(ErrorDescription.has_value() ? ErrorDescription->size() : 0, 12);
    _worldPacket.WriteBits(ErrorDescription.has_value(), 1);
    _worldPacket.FlushBits();

    if (ErrorDescription)
        _worldPacket.WriteString(*ErrorDescription);

    return &_worldPacket;
}

WorldPacket const* PerksProgramPurchaseSuccess::Write()
{
    _worldPacket << int32(VendorItemID);
    _worldPacket << int32(NewCurrencyAmount);
    _worldPacket << ItemInfo;

    return &_worldPacket;
}

WorldPacket const* PerksProgramRefundSuccess::Write()
{
    _worldPacket << int32(VendorItemID);
    _worldPacket << int32(NewCurrencyAmount);
    _worldPacket << ItemInfo;

    return &_worldPacket;
}

WorldPacket const* PerksProgramCurrencyRefresh::Write()
{
    _worldPacket << int32(OldValue);
    _worldPacket << int32(NewValue);

    return &_worldPacket;
}

WorldPacket const* PerksProgramActivityUpdate::Write()
{
    _worldPacket << ActivityInfo;

    return &_worldPacket;
}

WorldPacket const* PerksProgramActivityComplete::Write()
{
    _worldPacket << int32(ActivityMonthID);
    _worldPacket << int32(ThresholdOrderIndex);

    return &_worldPacket;
}

WorldPacket const* PerksProgramPendingRewards::Write()
{
    _worldPacket << Size<uint32>(Rewards);

    for (PerksProgramPendingChestReward const& reward : Rewards)
        _worldPacket << reward;

    return &_worldPacket;
}
}
