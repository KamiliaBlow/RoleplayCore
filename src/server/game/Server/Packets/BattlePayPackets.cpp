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

#include "BattlePayPackets.h"
#include "StringFormat.h"
#include "Log.h"
#include "PacketOperators.h"

namespace WorldPackets::BattlePay
{
WorldPacket const* ProductListResponse::Write()
{
    if (RawData && !RawData->empty())
        _worldPacket.append(RawData->data(), RawData->size());

    return &_worldPacket;
}

namespace
{
// Writes a JamBattlePayDeliverable exactly as the client's parser reads it.
//
// The tail is a 16-bit MSB-first group, which is what produces the two trailing bytes the client
// decomposes as B2/B3: alreadyOwns(1) + hasPetResult(1) + choicesCount(7) + hasDisplayInfo(1) +
// petResult(6) = 16 bits. With everything zero this writes 00 00, matching the capture.
void WriteDeliverable(ByteBuffer& buffer, DistributionDeliverable const& deliverable)
{
    buffer << deliverable.DeliverableID;
    buffer << deliverable.Type;
    buffer << deliverable.ItemID;
    buffer << deliverable.Quantity;
    buffer << deliverable.MountSpellID;
    buffer << deliverable.BattlePetCreatureID;
    buffer << deliverable.BoostID;
    buffer << deliverable.Flags;
    buffer << deliverable.TransItemModifiedAppearanceID;
    buffer << deliverable.TransmogSetID;
    buffer << deliverable.CharTitleID;
    buffer << deliverable.SpellItemEnchantmentID;
    buffer << deliverable.WarbandSceneID;

    buffer << uint8(deliverable.Name.size());       // plain byte, read before the bit group
    buffer.WriteBit(deliverable.AlreadyOwns);
    buffer.WriteBit(false);                         // hasPetResult - we never grant a battle pet this way
    buffer.WriteBits(0u, 7);                        // choicesCount - no choice products
    buffer.WriteBit(false);                         // hasDisplayInfo - the struct is not decoded (see .h)
    buffer.WriteBits(0u, 6);                        // petResult
    buffer.FlushBits();

    if (!deliverable.Name.empty())
        buffer.append(deliverable.Name.data(), deliverable.Name.size());
}

// Writes a JamBattlePayDistributionObject. ObjectGuid streams as a PackedGuid (uint16 mask + the
// non-zero bytes), which is exactly what the client's ReadPackedGuid consumes here.
void WriteDistributionObject(ByteBuffer& buffer, DistributionObject const& distribution)
{
    buffer << distribution.DistributionID;
    buffer << distribution.Status;
    buffer << distribution.DeliverableID;
    buffer << distribution.LicenseGameAccountGUID;
    buffer << distribution.TargetPlayer;
    buffer << distribution.TargetNativeRealm;
    buffer << distribution.TargetVirtualRealm;
    buffer << distribution.PurchaseID;
    buffer << distribution.ManualReview;             // precedes the flag byte on the wire

    buffer.WriteBit(distribution.Deliverable.has_value());
    buffer.WriteBit(distribution.Revoked);
    buffer.FlushBits();

    if (distribution.Deliverable)
        WriteDeliverable(buffer, *distribution.Deliverable);
}
}

WorldPacket const* GetDistributionListResponse::Write()
{
    if (!BuildFromObjects)
    {
        if (RawData && !RawData->empty())
            _worldPacket.append(RawData->data(), RawData->size());

        return &_worldPacket;
    }

    // Header proven byte-exact against the capture - see the class comment.
    _worldPacket << Result;
    _worldPacket.WriteBits(uint32(Distributions.size()), 11);
    _worldPacket.FlushBits();

    for (DistributionObject const& distribution : Distributions)
        WriteDistributionObject(_worldPacket, distribution);

    return &_worldPacket;
}

WorldPacket const* DistributionUpdate::Write()
{
    WriteDistributionObject(_worldPacket, Distribution);

    return &_worldPacket;
}

WorldPacket const* SyncWowEntitlements::Write()
{
    // Both counts up front, then the two parallel arrays - see the class comment for the byte proof.
    // They are written from one vector of pairs so the two arrays cannot fall out of step.
    _worldPacket << uint32(Entitlements.size());
    _worldPacket << uint32(Entitlements.size());

    for (auto const& pair : Entitlements)
    {
        AccountEntitlement const& entitlement = pair.first;
        _worldPacket << entitlement.DeliverableID;
        _worldPacket << entitlement.ExpireDate;
        _worldPacket << entitlement.DisplayExpireDate;
        _worldPacket << entitlement.UnitsRemaining;

        // The client bit-unpacks this trailing byte, so it is written as a bit group. We only ever
        // emit false, which is 0x00 under either reading - every captured row had it clear, so the
        // encoding of a set flag is untested and deliberately unused.
        _worldPacket.WriteBit(entitlement.ManualReviewStatus);
        _worldPacket.FlushBits();
    }

    for (auto const& pair : Entitlements)
        WriteDeliverable(_worldPacket, pair.second);

    return &_worldPacket;
}

void DistributionAssignToTarget::Read()
{
    _worldPacket >> ClientToken;
    _worldPacket >> DistributionID;
    _worldPacket >> TargetCharacter;
    _worldPacket >> ProductChoice;
}

WorldPacket const* StartDistributionAssignToTargetResponse::Write()
{
    _worldPacket << Result;
    _worldPacket << Unknown;
    _worldPacket << DistributionID;

    return &_worldPacket;
}

void StartPurchase::Read()
{
    _worldPacket >> ClientToken;
    _worldPacket >> ProductID;
    _worldPacket >> Unused;
    Flag = _worldPacket.ReadBit();

    // Remainder is the platform string and the client's attestation blob; nothing here needs them,
    // and consuming the buffer keeps the "Unprocessed tail data" warning from firing every purchase.
    _worldPacket.rfinish();
}



void OpenCheckout::Read()
{
    _worldPacket >> ClientToken;
    _worldPacket >> ProductID;
}

void CatalogShopLicenseGameDataRequest::Read()
{
    // The response wire is not yet modeled, so this body is retained for diagnostics only (its size
    // identifies which of the captured request variants it is). Consume the whole body so the packet
    // is not reported as under-read.
    size_t const len = _worldPacket.size();
    Data.resize(len);
    if (len)
        _worldPacket.read(Data.data(), len);
}

WorldPacket const* StartPurchaseResponse::Write()
{
    _worldPacket << ResultA;
    _worldPacket << ResultB;
    _worldPacket << PurchaseID;

    return &_worldPacket;
}

// INFERRED layout - see the ConfirmPurchase comment in the header. Gated off by default.
WorldPacket const* ConfirmPurchase::Write()
{
    _worldPacket << PurchaseID;     // +0
    _worldPacket << ServerToken;    // +8 - echoed back verbatim by the client

    return &_worldPacket;
}



void ConfirmPurchaseResponse::Read()
{
    _worldPacket >> ServerToken;
    _worldPacket >> ClientPriceFixedPoint;
    Confirmed = _worldPacket.ReadBit();
}



// Record order proven against the live 68974 purchase list (TESTER_SNIFF2_LINDORMI_MINE, 458 B =
// 8 + 10x45): { u64 PurchaseID, i32 Status, i32 ResultCode, u32 ProductID, u64 BasePrice,
// u64 UserPrice, i64 TimeCreated, u8 walletNameLen }. walletName sits at the END of the record -
// in all 10 live records the unix purchase time aligns at record offset 36 and byte 44 is the
// empty-wallet 0; with the u8 after ProductID the time would start at 37, one byte late.
// Shares the JamBattlePayPurchase record layout with PurchaseUpdate::Write (walletName length
// record-final - see the comment there). Answered honestly-empty today (no purchase ledger yet).
WorldPacket const* GetPurchaseListResponse::Write()
{
    _worldPacket << Result;
    _worldPacket << uint32(Purchases.size());
    for (PurchaseRecord const& p : Purchases)
    {
        _worldPacket << p.PurchaseID;
        _worldPacket << p.Status;
        _worldPacket << p.ResultCode;
        _worldPacket << p.ProductID;
        _worldPacket << p.BasePrice;
        _worldPacket << p.UserPrice;
        _worldPacket << p.TimeCreated;
        _worldPacket << uint8(0);       // walletName: empty (8-bit length primitive, value 0), record-final
    }

    return &_worldPacket;
}

WorldPacket const* PurchaseUpdate::Write()
{
    // NO leading Result here. SMSG_BATTLE_PAY_PURCHASE_UPDATE (0x420231) begins straight with the record
    // count: its ctor (client RVA 0x6090D0) performs exactly ONE ReadUInt32 and feeds it directly to
    // vector_resize, then parses that many records.
    //
    // Its sibling SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE (0x42021B, ctor 0x607DA0) DOES lead with a
    // Result and performs TWO ReadUInt32. The two messages share the record type but not the header, and
    // the client structs prove it: the record vector sits at +0x20 in this message and at +0x28 in that
    // one - displaced by exactly the 4 bytes of Result.
    //
    // Writing Result here made the client read our always-zero Result AS THE COUNT, so it parsed zero
    // records and returned immediately (merge handler 0x23CD340, cmp/je on count == 0) with no error
    // anywhere. That silently broke the entire purchase confirmation handshake - see the commit message.
    _worldPacket << uint32(Purchases.size());
    for (PurchaseRecord const& p : Purchases)
    {
        _worldPacket << p.PurchaseID;
        _worldPacket << p.Status;
        _worldPacket << p.ResultCode;
        _worldPacket << p.ProductID;
        _worldPacket << p.BasePrice;
        _worldPacket << p.UserPrice;
        _worldPacket << p.TimeCreated;
        _worldPacket << uint8(0);       // walletName: empty (8-bit length primitive, value 0), record-final
    }

    return &_worldPacket;
}

// Mirrors the client's element parser at rva 0x72BD40 exactly, including the detail that the two bit
// fields are separately flushed bytes rather than one packed group (the parser reads each with its own
// READ_U8 and then shifts: `shr dl,7` for the optional flag, `shr rdx,1` for the 7-bit choice count).
// Writing them as one group would shift every following byte and desynchronise the whole vector.
WorldPacket const* DeliveryEnded::Write()
{
    _worldPacket << PurchaseID;
    _worldPacket << uint32(Products.size());
    for (DeliveredProduct const& product : Products)
    {
        _worldPacket << product.ProductID;

        _worldPacket.WriteBit(false);       // hasUnlockList - no unlock-id list to send (see the header)
        _worldPacket.FlushBits();

        _worldPacket << Bits<7>(0);         // choiceCount - our products have no client-chosen variants
        _worldPacket.FlushBits();
    }

    return &_worldPacket;
}

WorldPacket const* EnumVasPurchaseStatesResponse::Write()
{
    // Six-bit count, then flush. With no purchases this is the single 0x00 byte retail sends.
    _worldPacket << Bits<6>(0);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

WorldPacket const* VasGetServiceStatusResponse::Write()
{
    _worldPacket << Bits<4>(ServiceStatus);
    _worldPacket << Bits<4>(Unknown);
    _worldPacket.FlushBits();

    return &_worldPacket;
}

void CharacterUpgradeStart::Read()
{
    _worldPacket >> CharacterGUID;
    _worldPacket >> SpecializationID;
}

WorldPacket const* CharacterUpgradeStarted::Write()
{
    _worldPacket << CharacterGUID;

    return &_worldPacket;
}

WorldPacket const* CharacterUpgradeComplete::Write()
{
    _worldPacket << CharacterGUID;

    return &_worldPacket;
}

WorldPacket const* CharacterUpgradeAborted::Write()
{
    _worldPacket << CharacterGUID;

    return &_worldPacket;
}

void CharacterUpgradeManualUnrevokeRequest::Read()
{
    _worldPacket >> CharacterGUID;
}

void GetVasAccountCharacterList::Read()
{
    _worldPacket >> Field1;
    _worldPacket >> Field2;
}

void GetVasTransferTargetRealmList::Read()
{
    _worldPacket >> Field1;
    _worldPacket >> Field2;
}

void VasGetQueueMinutes::Read()
{
    // The response echoes a uint64 correlation handle; the request carries it. Read it as a uint64 when the
    // body is wide enough, else zero-extend a uint32 - robust against either request width without throwing.
    if (_worldPacket.size() - _worldPacket.rpos() >= sizeof(uint64))
        _worldPacket >> Handle;
    else
    {
        uint32 low = 0;
        _worldPacket >> low;
        Handle = low;
    }
}

void VasCheckTransferOk::Read()
{
    _worldPacket >> Field1;
}

void BattlePayStartVasPurchase::Read()
{
    // Full wire (serializer sub_7FF72907C390): 4x uint32 + 4x packed guid, then a 5-byte bit block of five
    // string-length prefixes (6,7,7,6,12 bits) followed by the single IsValidationOnly bool bit, then the
    // string bodies. Read through the bool; the string bodies are not needed, so rfinish afterwards.
    _worldPacket >> SequenceId;
    _worldPacket >> ServiceType;
    _worldPacket >> Guid1;
    _worldPacket >> Context;
    _worldPacket >> TargetRealmAddress;
    _worldPacket >> Guid2;
    _worldPacket >> Guid3;
    _worldPacket >> Guid4;

    _worldPacket.ReadBits(6);    // len(string1)
    _worldPacket.ReadBits(7);    // len(string2)
    _worldPacket.ReadBits(7);    // len(string3)
    _worldPacket.ReadBits(6);    // len(string4)
    _worldPacket.ReadBits(12);   // len(string5)
    IsValidationOnly = _worldPacket.ReadBit();

    _worldPacket.rfinish();      // string bodies not needed
}

void BattlePayDistributionAssignVas::Read()
{
    _worldPacket >> Token;
    _worldPacket.rfinish();   // the remaining fields are not modelled (see header); consume them
}

WorldPacket const* VasGetQueueMinutesResponse::Write()
{
    _worldPacket << uint64(Handle);
    _worldPacket << uint32(QueueMinutes);

    return &_worldPacket;
}

WorldPacket const* CharacterUpgradeManualUnrevokeResult::Write()
{
    _worldPacket << uint32(Result);

    return &_worldPacket;
}

WorldPacket const* BattlePayDistributionAssignVasResponse::Write()
{
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << uint32(Result);

    return &_worldPacket;
}

WorldPacket const* GetVasAccountCharacterListResult::Write()
{
    // Outer header: 4 uint32 (both dump versions agree), then a uint32-counted vector of characters. The
    // per-entry string pair shares one bit-packed length block exactly as the client reads it: the 6-bit
    // name length then the 9-bit realm length (WriteBits 6 then 9, FlushBits), then the two raw bodies -
    // this reproduces the verified nameLen = A>>2, realmLen = ((A&3)<<7)|(B>>1).
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << uint32(Field3);
    _worldPacket << uint32(Characters.size());

    for (VasAccountCharacterInfo const& c : Characters)
    {
        _worldPacket << c.CharacterGUID;
        _worldPacket << c.AccountGUID;
        _worldPacket << uint32(c.VirtualRealmAddress);
        _worldPacket << uint8(c.Flags1);
        _worldPacket << uint8(c.Flags2);
        _worldPacket << uint8(c.Flags3);
        _worldPacket << uint8(c.Flags4);
        _worldPacket << uint64(c.HousingData);
        _worldPacket << uint32(c.Field9);
        _worldPacket.WriteBits(c.CharacterName.length(), 6);
        _worldPacket.WriteBits(c.RealmName.length(), 9);
        _worldPacket.FlushBits();
        _worldPacket.append(c.CharacterName.data(), c.CharacterName.length());
        _worldPacket.append(c.RealmName.data(), c.RealmName.length());
    }

    return &_worldPacket;
}

WorldPacket const* VasCheckTransferOkResponse::Write()
{
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << CharacterGUID;
    _worldPacket << uint32(Accounts.size());

    for (VasTransferWowAccount const& a : Accounts)
    {
        _worldPacket << a.AccountGUID;
        _worldPacket.WriteBits(a.AccountName.length(), 11);
        _worldPacket.FlushBits();
        _worldPacket.append(a.AccountName.data(), a.AccountName.length());
    }

    return &_worldPacket;
}

WorldPacket const* GetVasTransferTargetRealmListResult::Write()
{
    // Outer = 3 uint32 header + a flat uint32 count + the realm vector (VERIFIED, same shape as the account
    // list). Header fields have no proven offline meaning and stay 0; the 6 uint32 per realm entry are an
    // unlabeled reflected type populated best-effort - live-test-pending.
    _worldPacket << uint32(Field1);
    _worldPacket << uint32(Field2);
    _worldPacket << uint32(Field3);
    _worldPacket << uint32(Realms.size());

    for (VasTargetRealmInfo const& r : Realms)
    {
        _worldPacket << uint32(r.WowRealmAddress);
        _worldPacket << uint32(r.RealmId);
        _worldPacket << uint32(r.Flags);
        _worldPacket << uint32(r.PopulationState);
        _worldPacket << uint32(r.CategoryId);
        _worldPacket << uint32(r.ConfigId);
        _worldPacket.WriteBits(r.RealmName.length(), 9);
        _worldPacket.FlushBits();
        _worldPacket.append(r.RealmName.data(), r.RealmName.length());
    }

    return &_worldPacket;
}
}
