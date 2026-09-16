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

#include "TokenPackets.h"
#include "PacketOperators.h"

namespace WorldPackets::Token
{
void CommerceTokenGetLog::Read()
{
    _worldPacket >> ClientToken;
}

WorldPacket const* CommerceTokenGetLogResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;
    _worldPacket << Size<uint32>(AuctionableTokens);
    for (AuctionableTokenInfo const& auctionableTokenAuctionable : AuctionableTokens)
    {
        _worldPacket << auctionableTokenAuctionable.Id;
        _worldPacket << auctionableTokenAuctionable.LastUpdate;
        _worldPacket << auctionableTokenAuctionable.Status;
        _worldPacket << auctionableTokenAuctionable.Price;
        _worldPacket << auctionableTokenAuctionable.DurationLeft;
    }

    return &_worldPacket;
}

void CommerceTokenGetCount::Read()
{
    _worldPacket >> ClientToken;
}

WorldPacket const* CommerceTokenGetCountResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;
    _worldPacket << Size<uint32>(AuctionableTokenIDs);
    _worldPacket << Size<uint32>(ConsumableTokenIDs);
    for (uint64 tokenId : AuctionableTokenIDs)
        _worldPacket << tokenId;

    for (uint64 tokenId : ConsumableTokenIDs)
        _worldPacket << tokenId;

    return &_worldPacket;
}

WorldPacket const* CommerceTokenUpdate::Write()
{
    _worldPacket << Size<uint32>(AuctionableTokenIDs);
    _worldPacket << Size<uint32>(ConsumableTokenIDs);
    for (uint64 tokenId : AuctionableTokenIDs)
        _worldPacket << tokenId;

    for (uint64 tokenId : ConsumableTokenIDs)
        _worldPacket << tokenId;

    return &_worldPacket;
}

void ConsumableTokenCanVeteranBuy::Read()
{
    _worldPacket >> ClientToken;
}

WorldPacket const* ConsumableTokenCanVeteranBuyResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;
    _worldPacket << RemainingGoldAmount;

    return &_worldPacket;
}

void CanRedeemTokenForBalance::Read()
{
    _worldPacket >> Bits<1>(Refresh);
}

WorldPacket const* GenerateSsoTokenResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;
    _worldPacket << Issued;
    _worldPacket << Expires;
    _worldPacket << SizedString::BitsSize<7>(Token);
    _worldPacket.FlushBits();
    _worldPacket << SizedString::Data(Token);

    return &_worldPacket;
}

void CommerceTokenGetMarketPrice::Read()
{
    _worldPacket >> ClientToken;
}

WorldPacket const* CommerceTokenGetMarketPriceResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;
    _worldPacket << Price;
    _worldPacket << ExpectedSecondsUntilSold;

    return &_worldPacket;
}

void AuctionableTokenSell::Read()
{
    _worldPacket >> TokenID;
    _worldPacket >> Price;
    _worldPacket >> ClientToken;
}

void AuctionableTokenSellAtMarketPrice::Read()
{
    _worldPacket >> Guid;
    _worldPacket >> ClientToken;
    _worldPacket >> Unk;
    _worldPacket >> UnkPrice;
    _worldPacket >> Bits<1>(Confirmed);
}

WorldPacket const* AuctionableTokenSellAtMarketPriceResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;

    return &_worldPacket;
}

void ConsumableTokenBuy::Read()
{
    _worldPacket >> ClientToken;
    _worldPacket >> Guid;
    _worldPacket >> Unk;
}

void ConsumableTokenBuyAtMarketPrice::Read()
{
    _worldPacket >> Unk;
    _worldPacket >> ClientToken;
    _worldPacket >> Unk2;
    _worldPacket >> Bits<1>(Confirmed);
}

WorldPacket const* ConsumableTokenBuyAtMarketPriceResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;

    return &_worldPacket;
}

void ConsumableTokenRedeem::Read()
{
    _worldPacket >> RedeemType;
    _worldPacket >> TokenID;
    _worldPacket >> ClientToken;
}

void ConsumableTokenRedeemConfirmation::Read()
{
    _worldPacket >> RedeemType;
    _worldPacket >> TokenID;
    _worldPacket >> Guid;
    _worldPacket >> ClientToken;
    _worldPacket >> Bits<1>(Confirmed);
}

WorldPacket const* ConsumableTokenRedeemConfirmRequired::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << ChoiceType;
    _worldPacket << Result;
    _worldPacket << Amount;
    _worldPacket << Amount2;
    _worldPacket << Seconds;
    _worldPacket << Flags;

    return &_worldPacket;
}

WorldPacket const* ConsumableTokenRedeemResponse::Write()
{
    _worldPacket << ClientToken;
    _worldPacket << Result;
    _worldPacket << ChoiceType;

    return &_worldPacket;
}
}
