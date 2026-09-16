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

#ifndef TRINITYCORE_BATTLE_PAY_PACKETS_H
#define TRINITYCORE_BATTLE_PAY_PACKETS_H

#include "Packet.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include <string>
#include <utility>
#include <vector>

namespace WorldPackets
{
    namespace BattlePay
    {
        // Client requests the shop catalog. Body carries a locale/region selector we do not need.
        class GetProductList final : public ClientPacket
        {
        public:
            explicit GetProductList(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_GET_PRODUCT_LIST, std::move(packet)) { }

            void Read() override { }
        };

        // Client requests the account purchase/distribution list.
        class GetPurchaseList final : public ClientPacket
        {
        public:
            explicit GetPurchaseList(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_GET_PURCHASE_LIST, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_CATALOG_SHOP_LICENSE_GAME_DATA_REQUEST. Sent by the client mid-checkout carrying its own
        // license / game data. The body is variable (876/140/52/36/24 bytes across the 12.1.0.69382
        // capture). We keep the whole body so the handler can log its size for future response modeling;
        // the paired response wire (SMSG 0x4202C1) is not yet recovered - see
        // WorldSession::HandleCatalogShopLicenseGameDataRequest.
        class CatalogShopLicenseGameDataRequest final : public ClientPacket
        {
        public:
            explicit CatalogShopLicenseGameDataRequest(WorldPacket&& packet)
                : ClientPacket(CMSG_CATALOG_SHOP_LICENSE_GAME_DATA_REQUEST, std::move(packet)) { }

            void Read() override;

            std::vector<uint8> Data;    // raw request body, retained for diagnostics only
        };

        // The 12.0.7 catalog is a nested reflection bitstream that cannot be re-serialized field-by-field
        // offline (see docs). For P0 we replay a byte-exact, client-validated catalog blob captured from a
        // real 68275 session, so the shop opens and displays real products. RawData is the message BODY
        // (opcode dword already stripped); the ServerPacket base prepends the opcode header.
        class ProductListResponse final : public ServerPacket
        {
        public:
            explicit ProductListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_PRODUCT_LIST_RESPONSE) { }

            WorldPacket const* Write() override;

            std::vector<uint8> const* RawData = nullptr;
        };

        // What a product actually hands over. Layout recovered from the client's
        // own parser (sub_7FF729139460 @ image base 0x7FF728AA0000) and validated byte-exact against the
        // 68275 capture, where the embedded record decodes with zero remainder to catalog deliverable
        // 1161 { type = 1 CharacterBoost, boostID = 11, flags = 1620 } - the very values our decoded
        // catalog carries for that deliverable, which is what closes the loop on this struct.
        //
        // `Type` is the catalog's own deliverable vocabulary: 1 CharacterBoost, 2 BattlePet, 3 Mount,
        // 4 WowToken, 5 NameChange, 6 FactionChange, 8 RaceChange, 11 CharacterTransfer, 13 TransmogSet,
        // 14 Item/Toy, 18 GameUpgrade, 26 TransmogEnsemble. Captures also show 9, 12, 19, 20, 27 and 33
        // in use by retail; 12 is the Trading Post tender grant (Quantity = tender, Name = "500 Tender"),
        // which our catalog has no way to express and which the Perks Program owns.
        //
        // NAMING: this struct is the client's `JamBattlePayProduct`, not `JamBattlePayDeliverable`. The
        // 13 scalars below match that type's reflection descriptor (RVA 0x36A1FD8) member-for-member;
        // `JamBattlePayDeliverable` is a different 7-field type. The C++ name here is kept as-is because
        // it is what the shop code already calls it - only the comment was wrong.
        struct DistributionDeliverable
        {
            uint32 DeliverableID = 0;
            uint32 Type = 0;
            uint32 ItemID = 0;
            uint32 Quantity = 0;
            uint32 MountSpellID = 0;
            uint32 BattlePetCreatureID = 0;
            uint32 BoostID = 0;
            uint32 Flags = 0;
            uint32 TransItemModifiedAppearanceID = 0;
            uint32 TransmogSetID = 0;
            uint32 CharTitleID = 0;
            uint32 SpellItemEnchantmentID = 0;
            uint32 WarbandSceneID = 0;
            std::string Name;
            bool AlreadyOwns = false;
            // Choices and DisplayInfo are never emitted: DisplayInfo is a ~21 KB bit-packed struct that
            // has NOT been decoded (it is absent from both capture samples, has_displayInfo = 0), so we
            // always write the "no choices, no display info" form the captures show.
        };

        // JamBattlePayDistributionObject - one entitlement. Layout recovered from the client's parser
        // (sub_7FF729139990) and validated byte-exact: the captured 101-byte body decodes to
        // { distributionID = 0x1E828000009EECAC, status = 1, deliverableID = 1161,
        //   licenseGameAccountGUID = packed mask 0x800F, targetPlayer = empty, realms = 0,
        //   purchaseID = 0, manualReview = 0, flags = 0x80 (hasDeliverable) } + the 55-byte deliverable,
        // consuming all 101 bytes with nothing left over.
        struct DistributionObject
        {
            uint64 DistributionID = 0;
            uint32 Status = 0;              // 1 = available; the only value ever observed on the wire
            uint32 DeliverableID = 0;
            ObjectGuid LicenseGameAccountGUID;
            ObjectGuid TargetPlayer;        // empty while unassigned
            uint32 TargetNativeRealm = 0;
            uint32 TargetVirtualRealm = 0;
            uint64 PurchaseID = 0;
            uint32 ManualReview = 0;
            bool Revoked = false;
            Optional<DistributionDeliverable> Deliverable;
        };

        // Sent unsolicited at session start (character select) and again at login. There is no CMSG that
        // requests it - confirmed against the client's opcode table: no CMSG_BATTLE_PAY_GET_DISTRIBUTION_LIST
        // exists in this build. The client's StoreFrame_IsLoading gate keeps the Shop on "Loading, please
        // wait" until C_StoreSecure.HasDistributionList() flips, which this response does.
        //
        // Header PROVEN: the captured body starts `00 00 00 00 00 20`, and `uint32 Result` followed by
        // WriteBits(1, 11) + FlushBits() reproduces those six bytes exactly. The client reads the count
        // back as `(b0 << 3) | (b1 >> 5)`, which is precisely TC's 11-bit MSB-first encoding. An empty
        // list is the 6-byte form, also observed live.
        //
        // RawData replays the captured blob instead, and is what we send while entitlements are off.
        class GetDistributionListResponse final : public ServerPacket
        {
        public:
            explicit GetDistributionListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_DISTRIBUTION_LIST_RESPONSE) { }

            WorldPacket const* Write() override;

            std::vector<uint8> const* RawData = nullptr;

            bool BuildFromObjects = false;
            uint32 Result = 0;                              // PurchaseResult; 0 = Ok in every capture
            std::vector<DistributionObject> Distributions;
        };

        // Pushes a single changed entitlement: the body is exactly one DistributionObject with NO header
        // of any kind (the client's parser sub_7FF7290A8090 does nothing but read the object). Proven by
        // the 68275 capture, where this opcode appears 297 times with a 101-byte body byte-identical to
        // the object embedded in the 107-byte list response. Receiving it makes the client fire
        // PRODUCT_DISTRIBUTIONS_UPDATED, which refreshes the character-select token row and the Shop.
        class DistributionUpdate final : public ServerPacket
        {
        public:
            explicit DistributionUpdate() : ServerPacket(SMSG_BATTLE_PAY_DISTRIBUTION_UPDATE, 101) { }

            WorldPacket const* Write() override;

            DistributionObject Distribution;
        };

        // NOT IMPLEMENTED, deliberately: opcode 0x420224 carries exactly one of the structs above, bare
        // and with no header. Its layout is fully proven - 138 occurrences across all 13 of our 12.0.7
        // captures decode to 13 x uint32 + uint8 nameLen + 2 bit-packed bytes + name, with zero bytes
        // left over at all four observed lengths (55/62/65/66) - so WriteDeliverable would serve it
        // as-is. It is not wired because the 12.0.7 client DISCARDS IT:
        //
        //   the SMSG jump table (0x7FF729111D54 entry 548) reaches a live case at 0x7FF72910C327 that
        //   parses the body and calls the dispatch thunk 0x7FF7290A83E0, which is
        //       mov rax, [rip+0x3DFA4F9]   ; global 0x7FF72CEA28E0
        //       test rax, rax / je ret     ; always taken
        //   and that global is never written anywhere in the image. BattlePayMgr::RegisterMessageHandlers
        //   (0x7FF72AE74360) installs 21 handler slots; this is not one of them.
        //
        // So there is no client reaction to reproduce and no trigger condition to get right. Retail sends
        // it for entitlements whose product Type is 12 or 20 (rule matched exactly on both captured
        // accounts: 9-of-77 and 1-of-7) - types our catalog never emits anyway. Send the product through
        // 0x42021E or the distribution list instead; those have registered handlers.
        //
        // RE-VERIFIED against the 68275 image (2026-08-14), by a stronger method than the original
        // finding: the 12.0.7 client dispatches every message in this group through a per-opcode thunk
        // that indirects through a writable function-pointer slot, and the slot for 0x420224 is
        // rva 0x44028E0 (VA 0x7FF72CEA28E0). A linear scan of the WHOLE image for RIP-relative stores
        // (`48/4C 89 /r mod=00 rm=101` and `48 C7 05 ...`) targeting that slot finds ZERO write sites,
        // while every one of its neighbours has exactly two (a register in the BattlePay registrar at
        // rva 0x23D4360 and a null-out in the matching unregister at rva 0x23D4574). The dispatch is
        // therefore a permanent no-op. THE FINDING STILL HOLDS - do not wire this opcode.

        // ---------------------------------------------------------------------------------------------
        // Purchase delivery notifications (0x42021F - 0x420223).
        //
        // HOW THESE WERE RECOVERED (no capture exists and none ever will - retail is on 12.1.0):
        // the client's SMSG dispatcher is one giant switch, sub_7FF729103660, keyed on the raw opcode.
        // Each case runs three calls in order: a per-opcode PARSER, then a dispatch THUNK, then the
        // message destructor. The parser gives the wire layout; the thunk indirects through a global
        // slot that the BattlePay registrar (rva 0x23D4360) fills in, and THAT is what decides whether
        // the client does anything at all with the message.
        //
        // What the five delivery opcodes actually do in 12.0.7:
        //
        //   0x42021F DELIVERY_STARTED   parser rva 0x608110  slot 0x4402910 -> rva 0x1D80E0
        //                               *** rva 0x1D80E0 is the three bytes `C2 00 00` = `ret 0`. ***
        //                               The client parses the body and calls a function that returns
        //                               immediately. NOT WIRED - see the refusal note below.
        //   0x420220 DELIVERY_ENDED     parser rva 0x608190  slot 0x4402908 -> rva 0x23CD870  (real)
        //   0x420221 MOUNT_DELIVERED    parser rva 0x608270  slot 0x4402900 -> rva 0x23CD930  (real)
        //   0x420222 BATTLE_PET_DELIV.  parser rva 0x6082F0  slot 0x44028F0 -> rva 0x23CD930  (real)
        //   0x420223 COLLECTION_ITEM_D. parser rva 0x608380  slot 0x44028F8 -> rva 0x23CD930  (real)
        //
        // The three *_DELIVERED opcodes share ONE handler, rva 0x23CD930, whose entire body is
        // "fire the Lua event whose id is 0xF8EB3D280E974224" - it never touches the parsed payload.
        // DELIVERY_ENDED's handler (rva 0x23CD870) walks its element vector under a feature-flag gate
        // and then fires THE SAME event id. So all four live opcodes collapse to a single client-visible
        // effect: one "a delivery happened, refresh" event. That is exactly the signal a freshly granted
        // mount or toy needs in order to appear without a UI reload, and it is what this server was
        // missing: the grant happened silently and only PURCHASE_UPDATE went out.
        //
        // DELIVERY_STARTED is deliberately NOT implemented. Wiring it would put bytes on the wire for a
        // handler that is a bare `ret` - the same situation as SMSG_BATTLE_PAY_TENDER_GRANTED above, and
        // refused for the same reason. It is not "unsupported"; it is proven inert.
        //
        // SMSG_BATTLE_PAY_BATTLE_PET_DELIVERED is also not implemented, and for a different reason worth
        // stating: its layout IS recovered (parser rva 0x6082F0 = `uint32; PackedGuid`), but the handler
        // provably ignores both fields, and this server has no proven source for either of them. Wiring
        // it would mean inventing two field values to obtain a client effect that
        // SMSG_BATTLE_PAY_COLLECTION_ITEM_DELIVERED - which has no fields to invent - already produces
        // identically. Battle-pet grants therefore ride the collection-item notification.
        // ---------------------------------------------------------------------------------------------

        // SMSG_BATTLE_PAY_MOUNT_DELIVERED (0x420221) and SMSG_BATTLE_PAY_COLLECTION_ITEM_DELIVERED
        // (0x420223) have IDENTICAL wire shape. Their parsers (rva 0x608270 / 0x608380) make exactly one
        // call, to rva 0x33CC980, with the count argument computed as `packet[0x18] - packet[0x1C]`,
        // i.e. "however many bytes are left". That primitive (disassembled at rva 0x33CC980) zeroes the
        // out-pointer first, bounds-checks the request, and on success stores a POINTER into the packet
        // buffer and advances the read cursor - it is a zero-copy blob view, and the length is never
        // stored anywhere in the message object. The handler then never reads it.
        //
        // So these messages carry NO fields. An empty body is the complete and correct encoding, not a
        // placeholder: there is no value the client could observe, and because the primitive zeroes its
        // output before the bounds check, an empty body cannot leave the client reading uninitialised
        // memory either (that was checked specifically, since the parser does not pre-zero its own slot).
        class DeliveryNotification final : public ServerPacket
        {
        public:
            explicit DeliveryNotification(OpcodeServer opcode) : ServerPacket(opcode, 0) { }

            WorldPacket const* Write() override { return &_worldPacket; }
        };

        // One entry of SMSG_BATTLE_PAY_DELIVERY_ENDED's element vector. Shape proven from the element
        // parser at rva 0x72BD40 (memory stride 0x70) and the nested reader it calls at rva 0x72C010:
        //
        //     uint32   ProductID                          (READ_U32   -> elem+0x00)
        //     Bits<1>  hasUnlockList ; FlushBits          (READ_U8 + `shr dl,7` -> optional flag elem+0x28)
        //     Bits<7>  choiceCount   ; FlushBits          (nested, READ_U8 + `shr rdx,1`)
        //     choiceCount x { uint8; uint32 }             (nested loop, 8-byte stride)
        //     if (hasUnlockList) {
        //         uint8  kind                             (READ_U8  -> elem+0x08)
        //         uint32 count                            (READ_U32)
        //         count x uint32                          (loop -> vector at elem+0x10)
        //     }
        //
        // Note both bit fields are read as whole bytes with a shift, i.e. each is written and flushed on
        // its own - they are NOT one packed group. Nothing here is guessed: every read is a call to a
        // known primitive (0x33CC410 = uint32, 0x33CC370 = uint8, both confirmed by disassembly, the
        // latter being the byte a bit group is flushed into).
        //
        // We emit ProductID and leave both sub-lists empty, because this server genuinely has neither: a
        // "choice" only exists for products the client picks a variant of, and the unlock list is a set of
        // ids we do not produce. Empty is a legal encoding of both - the client's loops are plain
        // `for (i < count)` and its per-element call is behind a feature gate that an empty element
        // satisfies trivially.
        struct DeliveredProduct
        {
            uint32 ProductID = 0;
        };

        // SMSG_BATTLE_PAY_DELIVERY_ENDED (0x420220). Layout proven from the parser at rva 0x608190:
        //
        //     uint64 PurchaseID                  (READ_U64, primitive rva 0x33CC460 - disassembled and
        //                                         confirmed to read exactly 8 bytes and advance by 8)
        //     uint32 Count                       (READ_U32)
        //     Count x DeliveredProduct
        //
        // The client's handler (rva 0x23CD870) reads only the vector pointer and the count; PurchaseID is
        // parsed and never touched. We send the real PurchaseID anyway - it is the id of the purchase
        // whose delivery just ended, it costs nothing, and inventing was never necessary here.
        class DeliveryEnded final : public ServerPacket
        {
        public:
            explicit DeliveryEnded() : ServerPacket(SMSG_BATTLE_PAY_DELIVERY_ENDED, 8 + 4) { }

            WorldPacket const* Write() override;

            uint64 PurchaseID = 0;
            std::vector<DeliveredProduct> Products;
        };

        // One row of the account's entitlement ledger: which deliverable the account owns, and for how
        // long. Stride is exactly 25 bytes on the wire (4 + 8 + 8 + 4 + 1), which is what makes the
        // whole message decode with zero remainder.
        //
        // Every captured row carries ExpireDate = DisplayExpireDate = 0, UnitsRemaining = 0 and
        // ManualReviewStatus = 0, with a single exception in one capture where both dates are INT_MAX
        // (0x7FFFFFFF) - the "never expires" sentinel. So zero is the ordinary value for a permanent,
        // fully-available entitlement, not a "spent" marker.
        struct AccountEntitlement
        {
            uint32 DeliverableID = 0;
            int64 ExpireDate = 0;               // 8 bytes on the wire - never sized from a reserve hint
            int64 DisplayExpireDate = 0;
            uint32 UnitsRemaining = 0;
            bool ManualReviewStatus = false;
        };

        // The account's full entitlement ledger: what this account owns, plus the definition of each
        // owned deliverable. This is what lets the Shop mark a product as already purchased.
        //
        // Layout PROVEN byte-exact, with ZERO bytes left over, against every occurrence in our 12.0.7
        // captures - three distinct body sizes (617, 7247, 7343) across two different accounts holding
        // 7, 77 and 78 entitlements respectively:
        //
        //     uint32 entitlementCount
        //     uint32 deliverableCount
        //     entitlementCount x AccountEntitlement   (stride 25)
        //     deliverableCount x JamBattlePayDeliverable  (variable, same struct as the distribution
        //                                                  list embeds - see WriteDeliverable)
        //
        // The two counts were equal in every capture and the two arrays ran in the SAME order, both
        // ascending by DeliverableID: entitlement[i].DeliverableID == deliverable[i].DeliverableID for
        // all i, in all five distinct bodies. Write() enforces that pairing rather than trusting the
        // caller to keep two vectors in step.
        class SyncWowEntitlements final : public ServerPacket
        {
        public:
            explicit SyncWowEntitlements() : ServerPacket(SMSG_SYNC_WOW_ENTITLEMENTS) { }

            WorldPacket const* Write() override;

            // Paired one-to-one and emitted in ascending DeliverableID order.
            std::vector<std::pair<AccountEntitlement, DistributionDeliverable>> Entitlements;
        };

        // Client asks to apply one owned entitlement to a character it picked. Layout PROVEN by
        // disassembling the client's serializer (sub_7FF729079930): it writes the opcode and then
        // exactly uint32, uint64, PackedGuid, uint32. This packet appears in none of our captures, so the
        // field NAMES below are the plausible reading rather than a client-derived fact - which is why
        // the handler additionally validates both ids against values this server issued before acting.
        class DistributionAssignToTarget final : public ClientPacket
        {
        public:
            explicit DistributionAssignToTarget(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_DISTRIBUTION_ASSIGN_TO_TARGET, std::move(packet)) { }

            void Read() override;

            uint32 ClientToken = 0;
            uint64 DistributionID = 0;
            ObjectGuid TargetCharacter;
            uint32 ProductChoice = 0;       // a ChrSpecialization id for boosts, per the external fork
        };

        // Server's answer to an assign. Structure PROVEN by disassembling the client's parser
        // (sub_7FF7290A8F70): uint32, uint32, uint64. Never observed on the wire, so the field meanings
        // are inferred - the first uint32 is taken to be the PurchaseResult the client surfaces through
        // PRODUCT_ASSIGN_TO_TARGET_FAILED, and the middle uint32 is sent as 0 because we do not know it.
        class StartDistributionAssignToTargetResponse final : public ServerPacket
        {
        public:
            explicit StartDistributionAssignToTargetResponse() : ServerPacket(SMSG_BATTLE_PAY_START_DISTRIBUTION_ASSIGN_TO_TARGET_RESPONSE, 16) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            uint32 Unknown = 0;
            uint64 DistributionID = 0;
        };

        // Client initiates an in-game purchase. Layout from the client Write method (0x5d9f90):
        // u32, u64, then a 1-bit bool. The u32 is the strong candidate for the productID (the setter is
        // Warden-obfuscated so the exact semantic is runtime-confirmed via the handler's diagnostic log).
        class StartPurchase final : public ClientPacket
        {
        public:
            explicit StartPurchase(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_START_PURCHASE, std::move(packet)) { }

            void Read() override;

            // Layout settled from live 505-byte packets (three clicks, two products):
            //   @0  u32 ClientToken - a per-session click counter (observed 1, 2, 3)
            //   @4  u32 ProductID   - the entry productID the client buys by; stable across two
            //                         attempts at the same pet (1448, 1448) and different for
            //                         another card (1061). This is entryInfo.productID, which
            //                         Blizzard_StoreUI passes to C_StoreSecure.PurchaseProduct().
            //   @8  u32 (always 0 so far)
            //   then bit-packed lengths + "win" (platform) + a ~480 char client attestation blob
            //   { "RGKY" : ..., "CPGE" : ... } which we do not need and do not parse.
            // The old reader took the FIRST scalar as the product, i.e. the click counter, so no
            // purchase ever resolved.
            uint32 ClientToken = 0;
            uint32 ProductID = 0;
            uint32 Unused = 0;
            bool Flag = false;
        };

        // Client opens the checkout. Body is 8 bytes: { u32 ClientToken, u32 ProductID } (12.1.0.69382
        // capture, Midnight ProductID 0x417070 in all 7 attempts). ClientToken is the token the SSO/token
        // handshake echoes back verbatim for an in-game-currency product (COMMERCE_AUDIT C-09); ProductID
        // selects the product and decides the payment rail - see HandleBattlePayOpenCheckout.
        class OpenCheckout final : public ClientPacket
        {
        public:
            explicit OpenCheckout(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_OPEN_CHECKOUT, std::move(packet)) { }

            void Read() override;

            uint32 ClientToken = 0;
            uint32 ProductID = 0;
        };

        // Server-driven purchase confirmation prompt (retail interposes this between StartPurchase and
        // completion; it clears the client's WaitingOnConfirmation and shows the confirm dialog, whose
        // C_StoreSecure.GetConfirmationInfo() reads productID, walletName and the current price).
        //
        // INFERRED LAYOUT - NOT byte-verified: the 68275 client read struct (sub_7FF7290A91A0, opcode
        // 0x420232) is an opaque nested reflection struct in the RE dump and this opcode never appears
        // on-wire in any of the 8 captures (retail hands purchases to web checkout). The field set below
        // is the classic JamBattlePayConfirmPurchase shape that GetConfirmationInfo consumes. Because a
        // malformed packet could disconnect a live client, sending this is gated behind the
        // Shop.PurchaseConfirmation config (default off) until a live client validates the layout; the
        // proven direct-grant path (StartPurchase -> grant/charge -> PurchaseUpdate) stays the default.
        class ConfirmPurchase final : public ServerPacket
        {
        public:
            explicit ConfirmPurchase() : ServerPacket(SMSG_BATTLE_PAY_CONFIRM_PURCHASE, 8 + 4) { }

            WorldPacket const* Write() override;

            // EXACTLY 12 bytes - recovered from the client's handler (0x23D06D0 reads a u64 at +0 and a
            // u32 at +8, stores both to globals and fires STORE_CONFIRM_PURCHASE; the message class does
            // not field-parse, it takes a raw pointer to the payload). Nothing past +12 is ever read, so
            // the price and the bit-packed string we used to append were dead weight. Full RE:
            // c:\dumps\BATTLEPAY_CONFIRM_PURCHASE_WIRE_68275.md
            uint64 PurchaseID = 0;
            // The client echoes WHATEVER sits at +8 back to us as the token. We previously wrote ProductID
            // here, so the echo never matched the token we were comparing against.
            uint32 ServerToken = 0;
        };

        // Client's answer to the confirmation prompt. Layout byte-grounded from the 68275 client read of
        // CMSG_BATTLE_PAY_CONFIRM_PURCHASE_RESPONSE (0x4000fb): a u32 then a 1-bit bool. The u32 echoes
        // our ServerToken; the bool is confirm(true)/cancel(false).
        class ConfirmPurchaseResponse final : public ClientPacket
        {
        public:
            explicit ConfirmPurchaseResponse(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_CONFIRM_PURCHASE_RESPONSE, std::move(packet)) { }

            void Read() override;

            // 13 bytes, from the client's serializer (0x5D9FF0, class id via vtable 0x5DA060):
            //   u32 ServerToken @0  - echo of the SMSG's +8
            //   u64 price      @4  - dollars*10000 + cents*100, ZERO on cancel
            //   bits{1}        @12 - Confirmed, MSB-first then flush
            // The u64 used to be missing here, so ReadBit() read a bit of the price and Confirmed was
            // always false - i.e. even a working prompt would have been treated as a cancel.
            uint32 ServerToken = 0;
            uint64 ClientPriceFixedPoint = 0;
            bool Confirmed = false;
        };

        // Server ack for StartPurchase. Layout from the client read ctor (0x608ec0): u32, u32, u64.
        class StartPurchaseResponse final : public ServerPacket
        {
        public:
            explicit StartPurchaseResponse() : ServerPacket(SMSG_BATTLE_PAY_START_PURCHASE_RESPONSE, 16) { }

            WorldPacket const* Write() override;

            uint32 ResultA = 0;
            uint32 ResultB = 0;
            uint64 PurchaseID = 0;
        };

        // One JamBattlePayPurchase record. Wire order (68974 capture, TESTER_SNIFF2_LINDORMI_MINE):
        // fields below in declaration order, then a record-final u8 walletName length (sent empty, see .cpp).
        struct PurchaseRecord
        {
            uint64 PurchaseID = 0;
            int32 Status = 0;       // BattlepayPurchaseStatus: live 68974 completed purchases carry 6 (failed VAS: 12)
            int32 ResultCode = 0;   // PurchaseResult: Ok=0
            uint32 ProductID = 0;
            uint64 BasePrice = 0;
            uint64 UserPrice = 0;
            int64 TimeCreated = 0;
        };

        // Server drives purchase progress/completion.
        //
        // Body = { uint32 Count, Count x JamBattlePayPurchase }. There is NO leading Result: the client
        // read ctor at RVA 0x6090D0 performs exactly ONE ReadUInt32 and passes it straight to
        // vector_resize. An earlier comment here claimed "u32 result, then a u32-counted vector", which
        // was wrong and cost a working shop - see PurchaseUpdate::Write.
        //
        // status = 6 signals completion (live 68974 value; see PurchaseRecord) and the record echoes the
        // productID delivered; status = 9 (ConfirmationPending) with resultCode 0 is what the
        // confirmation dialog needs before the client will send CMSG_BATTLE_PAY_CONFIRM_PURCHASE_RESPONSE.
        class PurchaseUpdate final : public ServerPacket
        {
        public:
            explicit PurchaseUpdate() : ServerPacket(SMSG_BATTLE_PAY_PURCHASE_UPDATE) { }

            WorldPacket const* Write() override;

            std::vector<PurchaseRecord> Purchases;
        };

        // Reply to CMSG_BATTLE_PAY_GET_PURCHASE_LIST. Body = { uint32 Result, uint32 Count,
        // Count x PurchaseRecord }.
        //
        // NOTE the header is NOT the same as SMSG_BATTLE_PAY_PURCHASE_UPDATE, which has no Result and
        // starts straight at Count. This message's ctor (RVA 0x607DA0) does TWO ReadUInt32; the other's
        // does one. The client structs confirm it: the record vector lives at +0x28 here and at +0x20
        // there, displaced by exactly these 4 bytes. Assuming the two were identical is what broke the
        // purchase flow.
        // Proven against a live sniff: a retail account with 9 purchases produced a 413-byte body, and
        // 8 (header) + 9 * 45 (PurchaseRecord = u64+i32+i32+u32+u64+u64+i64+u8) == 413 exactly. The
        // record layout (walletName length record-final) matches the fixed PurchaseUpdate serializer.
        class GetPurchaseListResponse final : public ServerPacket
        {
        public:
            explicit GetPurchaseListResponse() : ServerPacket(SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE, 4 + 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
            std::vector<PurchaseRecord> Purchases;
        };

        // ---------------------------------------------------------------------------------------------
        // VAS (Value Added Services) - paid character services: transfer, rename, faction/race change.
        //
        // Only the two opcodes a real client actually sends are answered here. CMSG_UPDATE_VAS_PURCHASE_STATES
        // is emitted at character select in every one of the 19 captures on this machine, and
        // CMSG_VAS_GET_SERVICE_STATUS alongside it. Both have EMPTY bodies (client serializers RVA 0x5DC6D0
        // and 0x5DD670 write the opcode header and nothing else).
        //
        // The replies below are the truthful complete answers for a realm that has no VAS purchases in
        // flight, not placeholders: retail sends the very same single 0x00 byte for
        // SMSG_ENUM_VAS_PURCHASE_STATES_RESPONSE every session when a player has no pending purchase, and
        // the client's handler (RVA 0x23D0140) CLEARS and rebuilds its whole VASPurchaseState cache from
        // it. Not answering leaves that cache holding whatever it had.
        // ---------------------------------------------------------------------------------------------

        // CMSG_UPDATE_VAS_PURCHASE_STATES (0x400123) - empty body.
        class UpdateVasPurchaseStates final : public ClientPacket
        {
        public:
            explicit UpdateVasPurchaseStates(WorldPacket&& packet) : ClientPacket(CMSG_UPDATE_VAS_PURCHASE_STATES, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_ENUM_VAS_PURCHASE_STATES_RESPONSE (0x42029B):
        //   Bits<6> Count; FlushBits; Count x VASPurchaseState
        //
        // Count is SIX BITS, not a uint32 - confirmed by the client ctor at RVA 0x60EB40 and by 19 live
        // captures, every one of which is a single 0x00 byte. There is no ClientToken gate on this message
        // (unlike 0x420297 / 0x420298 / 0x4202C3, which silently drop unless the token is echoed).
        //
        // The per-entry VASPurchaseState struct is recovered but deliberately not modelled yet: its State
        // field indexes the LE_VAS_PURCHASE_STATE_* enum whose numeric values are NOT recoverable offline
        // (they are Lua globals, not an AddEnumConstant registrar). Emitting an entry would mean guessing a
        // state value that drives the client's purchase UI, so entries wait for the phase that can prove
        // them. An empty list needs none of that and is exactly what retail sends.
        class EnumVasPurchaseStatesResponse final : public ServerPacket
        {
        public:
            explicit EnumVasPurchaseStatesResponse() : ServerPacket(SMSG_ENUM_VAS_PURCHASE_STATES_RESPONSE, 1) { }

            WorldPacket const* Write() override;
        };

        // CMSG_VAS_GET_SERVICE_STATUS (0x400137) - empty body.
        class VasGetServiceStatus final : public ClientPacket
        {
        public:
            explicit VasGetServiceStatus(WorldPacket&& packet) : ClientPacket(CMSG_VAS_GET_SERVICE_STATUS, std::move(packet)) { }

            void Read() override { }
        };

        // SMSG_VAS_GET_SERVICE_STATUS_RESPONSE (0x4202C0) - exactly 1 byte:
        //   Bits<4> ServiceStatus (high nibble); Bits<4> Unknown (low nibble); FlushBits
        //
        // Client ctor RVA 0x6115E0, handler 0x23D0410 stores both nibbles and fires a Lua event. 0x00 is a
        // legal, complete body. We send 0 because this realm offers no VAS services - that is the accurate
        // status, not a stand-in. The meaning of the low nibble is genuinely unknown, so it stays 0 rather
        // than being given an invented value.
        class VasGetServiceStatusResponse final : public ServerPacket
        {
        public:
            explicit VasGetServiceStatusResponse() : ServerPacket(SMSG_VAS_GET_SERVICE_STATUS_RESPONSE, 1) { }

            WorldPacket const* Write() override;

            uint8 ServiceStatus = 0;
            uint8 Unknown = 0;
        };

        // ---- Character boost (VAS service type 1) ---------------------------------------------------
        //
        // These four live here rather than in CharacterPackets because the whole flow is the Shop's: a
        // boost is one of this account's owned entitlements being spent, and the handler hangs off
        // BattlePayMgr. Only the opcode NAMES belong to the character group.

        // CMSG_CHARACTER_UPGRADE_START (0x4000F4). The client picks the character and the specialization
        // it wants to come out of the boost as. There is no DistributionID in the body, so the server
        // chooses which of the account's owned boost entitlements to spend.
        //
        // Layout PROVEN from the client's own serializer (0x5D99F0, a straight-line writer, no branches):
        // after the opcode word it writes a PackedGuid and then a uint32 - entry 0x4000f4 of
        // c:/dumps/tools/cmsg_sweep/cmsg_layouts_68275.json, `"wire": "pguid u32"`, confidence HIGH.
        class CharacterUpgradeStart final : public ClientPacket
        {
        public:
            explicit CharacterUpgradeStart(WorldPacket&& packet) : ClientPacket(CMSG_CHARACTER_UPGRADE_START, std::move(packet)) { }

            void Read() override;

            ObjectGuid CharacterGUID;
            uint32 SpecializationID = 0;    ///< ChrSpecialization.db2 id the boosted character comes out as
        };

        // The three server answers. Each is a single ObjectGuid - proven from the client's own parsers
        // (0x420267 -> sub_7FF7290ABAB0, 0x420268 -> sub_7FF7290ABB10, 0x420269 -> sub_7FF7290ABB70),
        // every one of which reads exactly one guid and nothing else.
        //
        // STARTED goes out once the boost has been paid for and is about to be written; COMPLETE only
        // after it is durably committed; ABORTED whenever it is not going to happen, so the client's
        // boost UI is never left waiting on a request the server quietly dropped.
        class CharacterUpgradeStarted final : public ServerPacket
        {
        public:
            explicit CharacterUpgradeStarted() : ServerPacket(SMSG_CHARACTER_UPGRADE_STARTED, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid CharacterGUID;
        };

        class CharacterUpgradeComplete final : public ServerPacket
        {
        public:
            explicit CharacterUpgradeComplete() : ServerPacket(SMSG_CHARACTER_UPGRADE_COMPLETE, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid CharacterGUID;
        };

        class CharacterUpgradeAborted final : public ServerPacket
        {
        public:
            explicit CharacterUpgradeAborted() : ServerPacket(SMSG_CHARACTER_UPGRADE_ABORTED, 18) { }

            WorldPacket const* Write() override;

            ObjectGuid CharacterGUID;
        };

        // ---- VAS transfer/service request layer (client -> server) -------------------------------------
        //
        // CMSG read layouts recovered from the client deserializers (c:/dumps/all_cmsg_layouts_68275.json,
        // opcode NAMES build-stable). The two-uint32 request bodies carry a request context whose exact
        // send-side field names are not offline-proven; they are read verbatim and named by position so the
        // server can echo/validate them without inventing semantics.

        // CMSG_CHARACTER_CHECK_UPGRADE (0x4000F5) - empty body; a poll for boost eligibility of this account.
        class CharacterCheckUpgrade final : public ClientPacket
        {
        public:
            explicit CharacterCheckUpgrade(WorldPacket&& packet) : ClientPacket(CMSG_CHARACTER_CHECK_UPGRADE, std::move(packet)) { }

            void Read() override { }
        };

        // CMSG_CHARACTER_UPGRADE_MANUAL_UNREVOKE_REQUEST (0x4000F3) - one character guid: undo a revoked boost.
        class CharacterUpgradeManualUnrevokeRequest final : public ClientPacket
        {
        public:
            explicit CharacterUpgradeManualUnrevokeRequest(WorldPacket&& packet) : ClientPacket(CMSG_CHARACTER_UPGRADE_MANUAL_UNREVOKE_REQUEST, std::move(packet)) { }

            void Read() override;

            ObjectGuid CharacterGUID;
        };

        // CMSG_GET_VAS_ACCOUNT_CHARACTER_LIST (0x400120) - two uint32 request context.
        class GetVasAccountCharacterList final : public ClientPacket
        {
        public:
            explicit GetVasAccountCharacterList(WorldPacket&& packet) : ClientPacket(CMSG_GET_VAS_ACCOUNT_CHARACTER_LIST, std::move(packet)) { }

            void Read() override;

            uint32 Field1 = 0;
            uint32 Field2 = 0;
        };

        // CMSG_GET_VAS_TRANSFER_TARGET_REALM_LIST (0x400121) - two uint32 request context.
        class GetVasTransferTargetRealmList final : public ClientPacket
        {
        public:
            explicit GetVasTransferTargetRealmList(WorldPacket&& packet) : ClientPacket(CMSG_GET_VAS_TRANSFER_TARGET_REALM_LIST, std::move(packet)) { }

            void Read() override;

            uint32 Field1 = 0;
            uint32 Field2 = 0;
        };

        // CMSG_VAS_GET_QUEUE_MINUTES (0x400138) - one uint32 (the VAS service/product context to estimate).
        class VasGetQueueMinutes final : public ClientPacket
        {
        public:
            explicit VasGetQueueMinutes(WorldPacket&& packet) : ClientPacket(CMSG_VAS_GET_QUEUE_MINUTES, std::move(packet)) { }

            void Read() override;

            uint64 Handle = 0;   // the correlation token the response echoes (client matches it in its pending table)
        };

        // SMSG_VAS_GET_QUEUE_MINUTES_RESPONSE (0x4202C1) - flat 12 bytes, VERIFIED from the client parser
        // sub_7FF72AE70450: { uint64 Handle; uint32 QueueMinutes }. No sub-frame, no bits.
        class VasGetQueueMinutesResponse final : public ServerPacket
        {
        public:
            explicit VasGetQueueMinutesResponse() : ServerPacket(SMSG_VAS_GET_QUEUE_MINUTES_RESPONSE, 12) { }

            WorldPacket const* Write() override;

            uint64 Handle = 0;
            uint32 QueueMinutes = 0;
        };

        // SMSG_CHARACTER_UPGRADE_MANUAL_UNREVOKE_RESULT (0x42026B) - flat 4 bytes, VERIFIED from the client
        // parser sub_7FF72A663530: a single uint32 Result. No character guid on the wire (resolved client-side).
        class CharacterUpgradeManualUnrevokeResult final : public ServerPacket
        {
        public:
            explicit CharacterUpgradeManualUnrevokeResult() : ServerPacket(SMSG_CHARACTER_UPGRADE_MANUAL_UNREVOKE_RESULT, 4) { }

            WorldPacket const* Write() override;

            uint32 Result = 0;
        };

        // CMSG_VAS_CHECK_TRANSFER_OK (0x400139) - one uint32 (the VAS service/product context to validate).
        class VasCheckTransferOk final : public ClientPacket
        {
        public:
            explicit VasCheckTransferOk(WorldPacket&& packet) : ClientPacket(CMSG_VAS_CHECK_TRANSFER_OK, std::move(packet)) { }

            void Read() override;

            uint32 Field1 = 0;
        };

        // CMSG_BATTLE_PAY_ACK_FAILED_RESPONSE - the client acknowledges a failed purchase result. One uint32
        // (the server token being acked). Fire-and-forget: no response.
        class BattlePayAckFailedResponse final : public ClientPacket
        {
        public:
            explicit BattlePayAckFailedResponse(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_ACK_FAILED_RESPONSE, std::move(packet)) { }

            void Read() override { _worldPacket >> ServerToken; }

            uint32 ServerToken = 0;
        };

        // CMSG_BATTLE_PAY_REQUEST_PRICE_INFO - one uint32 (the product/context to price).
        class BattlePayRequestPriceInfo final : public ClientPacket
        {
        public:
            explicit BattlePayRequestPriceInfo(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_REQUEST_PRICE_INFO, std::move(packet)) { }

            void Read() override { _worldPacket >> ProductID; }

            uint32 ProductID = 0;
        };

        // CMSG_BATTLE_PAY_CANCEL_OPEN_CHECKOUT - the client abandons an open checkout. Small body that the
        // server does not need to act on (the checkout it opened carries no server-side reservation to release).
        class BattlePayCancelOpenCheckout final : public ClientPacket
        {
        public:
            explicit BattlePayCancelOpenCheckout(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_CANCEL_OPEN_CHECKOUT, std::move(packet)) { }

            void Read() override { _worldPacket.rfinish(); }
        };

        // CMSG_BATTLE_PAY_START_VAS_PURCHASE (0x400122) - begins a paid VAS purchase. The leading fields are
        // resolved from the client builder sub_7FF72AE6E2F0: a request sequence id, the VAS service type, a
        // guid (the selected character/product), a context uint32, then the TARGET REALM's wowRealmAddress
        // (copied from the picked JamCliVASTargetRealm[+0]). The tail (name/blob strings, more guids) is not
        // needed to identify the transfer and is consumed. The per-guid role is not offline-provable, but the
        // first guid is the selected character in the transfer flow; downstream validation (VasTransferMgr)
        // rejects a wrong guid safely.
        class BattlePayStartVasPurchase final : public ClientPacket
        {
        public:
            explicit BattlePayStartVasPurchase(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_START_VAS_PURCHASE, std::move(packet)) { }

            void Read() override;

            // Wire (client serializer sub_7FF72907C390, fully RE'd): 4x uint32 + 4x packed guid header, then a
            // 5-byte bit block of five string-length prefixes (6,7,7,6,12 bits) and a single trailing bool bit
            // = IsValidationOnly, then the string bodies (not needed here). The four guids carry the character
            // plus account/bnet/target guids; the character is the one that is a Player guid.
            uint32 SequenceId = 0;
            uint32 ServiceType = 0;
            uint32 Context = 0;
            uint32 TargetRealmAddress = 0;     // wowRealmAddress of the picked target realm (JamCliVASTargetRealm[+0])
            ObjectGuid Guid1;
            ObjectGuid Guid2;
            ObjectGuid Guid3;
            ObjectGuid Guid4;
            bool IsValidationOnly = false;     // flow calls validate-pass first, then commit-pass

            ObjectGuid GetCharacterGuid() const  // the character is the sole Player-type guid among the four
            {
                for (ObjectGuid const& g : { Guid1, Guid2, Guid3, Guid4 })
                    if (g.IsPlayer())
                        return g;
                return ObjectGuid::Empty;
            }
        };

        // CMSG_BATTLE_PAY_DISTRIBUTION_ASSIGN_VAS (0x400167) - a large nested struct (client token, several
        // guids, product choice and flag bytes). Only the leading uint32 (a client/distribution token used to
        // correlate the answer) is modelled; the rest is consumed to keep the stream aligned. The exact struct
        // is live-capture-pending and is not needed to refuse an assign for which no entitlement exists.
        class BattlePayDistributionAssignVas final : public ClientPacket
        {
        public:
            explicit BattlePayDistributionAssignVas(WorldPacket&& packet) : ClientPacket(CMSG_BATTLE_PAY_DISTRIBUTION_ASSIGN_VAS, std::move(packet)) { }

            void Read() override;

            uint32 Token = 0;
        };

        // SMSG_BATTLE_PAY_DISTRIBUTION_ASSIGN_VAS_RESPONSE (0x420316) - three uint32 (client parser
        // sub_7FF7290B7D50 reads exactly {uint32, uint32, uint32}, no strings, no vector).
        class BattlePayDistributionAssignVasResponse final : public ServerPacket
        {
        public:
            explicit BattlePayDistributionAssignVasResponse() : ServerPacket(SMSG_BATTLE_PAY_DISTRIBUTION_ASSIGN_VAS_RESPONSE, 12) { }

            WorldPacket const* Write() override;

            uint32 Field1 = 0;
            uint32 Field2 = 0;
            uint32 Result = 0;
        };

        // ---- VAS transfer list responses (server -> client) --------------------------------------------
        //
        // Element layouts are VERIFIED from the client deserializers (cfunc_cache decompile). Outer headers
        // and a few per-entry field SEMANTICS are not offline-provable (JamCliVASTargetRealm is a reflected
        // type with no descriptor); those are populated with clearly-labeled best-effort values and are
        // live-test-pending. String lengths are bit-packed exactly as the client reads them.

        // One transferable character (SMSG_GET_VAS_ACCOUNT_CHARACTER_LIST_RESULT vector element, verified).
        struct VasAccountCharacterInfo
        {
            ObjectGuid CharacterGUID;                  // the character
            ObjectGuid AccountGUID;                    // 2nd guid: the owning bnet/wow-account guid (inferred)
            uint32 VirtualRealmAddress = 0;            // field 3 (inferred: the character's home realm address)
            uint8 Flags1 = 0;                          // 4 per-entry u8s (transfer eligibility flags - unlabeled)
            uint8 Flags2 = 0;
            uint8 Flags3 = 0;
            uint8 Flags4 = 0;
            uint64 HousingData = 0;                    // opaque 8-byte housing/plot key (verified width, opaque)
            uint32 Field9 = 0;                         // trailing uint32 (unlabeled)
            std::string CharacterName;                 // 6-bit length prefix
            std::string RealmName;                     // 9-bit length prefix
        };

        // SMSG_GET_VAS_ACCOUNT_CHARACTER_LIST_RESULT (0x420297). Outer = 3 uint32 header + a FLAT uint32 count +
        // the character vector - VERIFIED (deserializer sub_7FF7290AE4B0; the "4th uint32" earlier dumps showed
        // is the list count, not a header field). Header field meanings are unlabeled offline; Field1 echoes the
        // request token so the client can correlate, the other two are 0 until proven.
        class GetVasAccountCharacterListResult final : public ServerPacket
        {
        public:
            explicit GetVasAccountCharacterListResult() : ServerPacket(SMSG_GET_VAS_ACCOUNT_CHARACTER_LIST_RESULT, 32) { }

            WorldPacket const* Write() override;

            uint32 Field1 = 0;
            uint32 Field2 = 0;
            uint32 Field3 = 0;
            std::vector<VasAccountCharacterInfo> Characters;
        };

        // One transfer-target realm (SMSG_GET_VAS_TRANSFER_TARGET_REALM_LIST_RESULT element JamCliVASTargetRealm,
        // verified widths). The 6 uint32s are an unlabeled reflected type; the mapping below is inferred so the
        // client has a selectable, named realm - live-test-pending on which field it transfers TO.
        struct VasTargetRealmInfo
        {
            // Field order mirrors the bnet JSON::RealmList::RealmEntry the client already consumes at login;
            // wowRealmAddress (realm.Id.GetAddress()) is the value the client sends back to target the transfer.
            uint32 WowRealmAddress = 0;                // field[0]: realm.Id.GetAddress() - the transfer target
            uint32 RealmId = 0;                        // field[1]: cfgRealmsID
            uint32 Flags = 0;                          // field[2]
            uint32 PopulationState = 0;                // field[3]
            uint32 CategoryId = 0;                     // field[4]: timezone/category
            uint32 ConfigId = 0;                       // field[5]
            std::string RealmName;                     // 9-bit length prefix
        };

        // One WoW account a character may transfer to (SMSG_VAS_CHECK_TRANSFER_OK_RESPONSE element
        // JamVASTransferWowAccount, verified: an account guid + an 11-bit-length-prefixed account name).
        struct VasTransferWowAccount
        {
            ObjectGuid AccountGUID;
            std::string AccountName;                    // 11-bit length prefix
        };

        // SMSG_VAS_CHECK_TRANSFER_OK_RESPONSE (0x4202C3). Outer (deserializer sub_7FF7290B1A10) =
        // { uint32, uint32, ObjectGuid, uint32-count, vector<VasTransferWowAccount> }. The two leading uint32
        // are unlabeled offline; Field1 echoes the request context. An empty account list is the honest,
        // byte-correct answer when the realm has no VAS transfer network.
        class VasCheckTransferOkResponse final : public ServerPacket
        {
        public:
            explicit VasCheckTransferOkResponse() : ServerPacket(SMSG_VAS_CHECK_TRANSFER_OK_RESPONSE, 28) { }

            WorldPacket const* Write() override;

            uint32 Field1 = 0;
            uint32 Field2 = 0;
            ObjectGuid CharacterGUID;
            std::vector<VasTransferWowAccount> Accounts;
        };

        // SMSG_GET_VAS_TRANSFER_TARGET_REALM_LIST_RESULT (0x420298). Outer = 3 uint32 header + a FLAT uint32
        // count + the realm vector - VERIFIED from the deserializer sub_7FF7290AE5B0 (same shape as the account
        // list; the earlier "10x uint32" / "u8+bitfield" dumps were miscounts of the element string prefix).
        // Header field meanings are unlabeled offline and stay 0 until proven.
        class GetVasTransferTargetRealmListResult final : public ServerPacket
        {
        public:
            explicit GetVasTransferTargetRealmListResult() : ServerPacket(SMSG_GET_VAS_TRANSFER_TARGET_REALM_LIST_RESULT, 16) { }

            WorldPacket const* Write() override;

            uint32 Field1 = 0;
            uint32 Field2 = 0;
            uint32 Field3 = 0;
            std::vector<VasTargetRealmInfo> Realms;
        };
    }
}

#endif // TRINITYCORE_BATTLE_PAY_PACKETS_H
