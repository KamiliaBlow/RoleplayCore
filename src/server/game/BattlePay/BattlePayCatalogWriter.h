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

#ifndef TRINITYCORE_BATTLE_PAY_CATALOG_WRITER_H
#define TRINITYCORE_BATTLE_PAY_CATALOG_WRITER_H

#include "Define.h"
#include "Optional.h"
#include <string>
#include <vector>

/*
 * Field-exact (de)serializer for the 12.0.7.68275 SMSG_BATTLE_PAY_GET_PRODUCT_LIST_RESPONSE body.
 *
 * WHY THIS EXISTS / WHAT REPLACED WHAT
 * ------------------------------------
 * The first version of this writer modelled a product record with fixed constants (HDR_SIZE 89,
 * NAME1_LEN_BIT 428, NAME2_LEN_BIT 451, FRAME_AFTER_BLOCKA 18, MAX_SIMPLE 9, PROD_START 28) and could
 * only rebuild the first 9 records. Those constants were not merely too small, they were reading the
 * WRONG FIELDS:
 *
 *   old "ProductID u32 @ record+81"  is really  DisplayInfo.fileDataID   (icon art FileDataID)
 *   old "Flags     u32 @ record+85"  is really  DisplayInfo.modelSceneID
 *   old NAME1_LEN_BIT 428            is the low 8 bits of DisplayInfo.name1's 10-bit length
 *   old NAME2_LEN_BIT 451            is the low 8 bits of DisplayInfo.name3's 13-bit length
 *   old "name3 = repeated title"     is DisplayCard[0].title
 *   old 7-u32 header, header[6]=248  is a 6-u32 header + Product[0].productID
 *
 * So any server-side map keyed on the old "ProductID" is keyed on a FileDataID, and the client will
 * purchase by an id the server does not know. The real ids are Product.productID (248, 253, 80, 81,
 * 51, 52, 54, 56, 61, ...), which is what C_StoreSecure.PurchaseProduct sends back.
 *
 * THE ACTUAL MESSAGE
 * ------------------
 * 6 x u32 header (result, currencyID, then FOUR counts), then four arrays back to back:
 * Product[nProducts], Deliverable[nDeliverables], ProductGroup[nGroups], ShopEntry[nShopEntries].
 * For the shipped 68275 blob that is 94/116/21/97 and ZERO bytes are left over - there is no opaque
 * tail. Records are JAM reflection records: a byte block of fixed scalars + vector counts, then a bit
 * block (bools = 1 bit, strings = a bit-packed length, optional sub-structs = 1 presence bit) with a
 * FlushBits pad to the next byte, then the payload (vector elements, string bytes, nested structs).
 *
 * Reference model and evidence: c:/dumps/BATTLEPAY_CATALOG_RECORD_FORMAT_68275.md and the prover
 * c:/dumps/scratch_catalog_v2.py (byte-exact round trip over 10 captures / 3 client builds). Bit widths
 * are derived from the client's own reflection descriptors (sizeArr at tag-0x40), not guessed; the
 * DisplayInfo bit header was confirmed against the client parser at RVA 0x6988A0, the Deliverable bit
 * block against ReadDeliverable at RVA 0x699460.
 *
 * Fields marked HYPOTHESIS below are never exercised by any capture. They are carried through verbatim
 * (read, kept, written back unchanged) and MUST NOT be interpreted or "cleaned up".
 */

// One BattlepayDisplayCard: the small art card shown under a product.
struct BattlePayDisplayCard
{
    uint32 CreatureDisplayInfoID = 0;
    uint32 ModelSceneID = 0;
    uint32 TransmogSetID = 0;
    std::string Title;
    uint8 PadBits = 0;                          // 6 FlushBits pad bits after the 10-bit title length; 0 in every capture
};

// JamBattlepayDisplayInfo - the name/description/art a product card renders with. A product WITHOUT one
// renders a nil name in the purchase confirmation dialog, which is exactly the bug this writer fixes:
// MakeDisplayInfo() can synthesise a well-formed one for a product that never had it.
struct BattlePayDisplayInfo
{
    // Bit-header order (90 bits, MSB-first): hasFileDataID, hasModelSceneID, len(name1) 10,
    // len(name2) 10, len(name3) 13, len(tooltip) 13, len(instructions) 13, hasFlags,
    // hasOverrideTextColor, hasOverrideTexture, hasOverrideBackground, len(disclaimer) 13,
    // len(nydusLink) 12, then 6 pad bits.
    Optional<uint32> FileDataID;                // the icon; ABSENT on product 9 (Argi) onward - this is
                                                // the optional that shifted everything and killed the old parser
    Optional<uint32> ModelSceneID;
    int32 BattlepayCardType = 0;
    int32 BannerType = 0;                       // 0 in all 136 captured DisplayInfos
    int32 ItemQuantity = 0;
    std::string Name1;                          // display name
    std::string Name2;
    std::string Name3;                          // description
    std::string Tooltip;
    std::string Instructions;
    Optional<uint32> Flags;
    Optional<uint32> OverrideTextColor;         // HYPOTHESIS: presence bit is 0 in every capture, payload untested
    Optional<uint32> OverrideTexture;
    Optional<uint32> OverrideBackground;
    std::string Disclaimer;
    std::string NydusLink;
    std::vector<BattlePayDisplayCard> DisplayCards;
    uint8 PadBits = 0;                          // 6 FlushBits pad bits; 0 in every capture
};

// JamBattlePayDeliverableChoice. HYPOTHESIS as a whole: no capture has ever contained one (nChoices is 0
// everywhere), so the field order comes from the reflection descriptor and the bit block from the client
// reader's stack offsets. Parsed and written back symmetrically so a future capture round-trips.
struct BattlePayDeliverableChoice
{
    uint32 ID = 0;
    int32 Type = 0;
    uint32 ItemID = 0;
    uint32 Quantity = 0;
    uint32 MountSpellID = 0;
    uint32 BattlePetCreatureID = 0;
    uint8 AlreadyOwns = 0;                      // 1 bit
    uint8 HasPetResult = 0;                     // 1 bit (presence flag)
    uint8 PetResult = 0;                        // 4 bits - HYPOTHESIS, always 0
    uint8 PadBits = 0;                          // 1 bit
    Optional<BattlePayDisplayInfo> DisplayInfo;
};

// JamBattlePayDeliverable - what a product actually grants.
struct BattlePayDeliverable
{
    uint32 DeliverableID = 0;
    int32 Type = 0;
    uint32 ItemID = 0;
    uint32 Quantity = 0;
    uint32 MountSpellID = 0;
    uint32 BattlePetCreatureID = 0;
    int32 BoostID = 0;
    int32 Flags = 0;
    uint32 TransItemModifiedAppearanceID = 0;
    uint32 TransmogSetID = 0;
    uint32 CharTitleID = 0;
    uint32 SpellItemEnchantmentID = 0;
    uint32 WarbandSceneID = 0;
    // Bit block, exactly 24 bits (three ReadBits8 calls in the client), layout read off ReadDeliverable
    // @ RVA 0x699460: len(name) 8 | alreadyOwns 1 | hasPetResult 1 | nChoices 7 | hasDisplayInfo 1 |
    // petResult 4 | pad 2. A round trip cannot distinguish this from other splits because nChoices and
    // petResult are 0 in every capture - the disassembly is the authority here, not the data.
    std::string Name;
    uint8 AlreadyOwns = 0;
    uint8 HasPetResult = 0;                     // HYPOTHESIS: never set; payload shape untested
    uint8 PetResult = 0;                        // HYPOTHESIS: 4-bit value, always 0
    uint8 PadBits = 0;                          // 2 bits
    std::vector<BattlePayDeliverableChoice> Choices;    // the client reads these BEFORE the name string
    Optional<BattlePayDisplayInfo> DisplayInfo;         // HYPOTHESIS: presence bit is 0 in every capture
};

// JamBattlePayProduct.
struct BattlePayCatalogProduct
{
    uint32 ProductID = 0;                       // the id the client purchases by (NOT the old rec+81 FileDataID)
    uint64 NormalPriceFixedPoint = 0;           // fixed point, /100000
    uint64 CurrentPriceFixedPoint = 0;
    int32 Type = 0;
    int32 Flags = 0;                            // NOT BattlepayDisplayFlags and NOT the old rec+85
                                                // modelSceneID. A separate, unreflected enum: bit 15 is
                                                // IsDynamicBundle and bits 1/3 drive `buyableHere`.
                                                // Display flags live on DisplayInfo.Flags instead.
    uint32 RequiredDeliverableID = 0;
    int32 Eligibility = 0;
    uint64 PmtProductID = 0;
    std::vector<uint32> DeliverableIDs;         // count is written from the vector, never stored twice
    std::vector<uint32> BundledProductIDs;
    Optional<BattlePayDisplayInfo> DisplayInfo; // set this to give a product a card it never had
    uint8 PadBits = 0;                          // 7 FlushBits pad bits after hasDisplayInfo; 0 in every capture
};

// JamBattlePayProductGroup - a storefront category.
struct BattlePayProductGroup
{
    uint32 GroupID = 0;
    int32 IconFileDataID = 0;
    uint8 DisplayType = 0;
    int32 Ordering = 0;
    int32 Flags = 0;
    uint32 ParentGroupID = 0;
    std::string Name;                           // 8-bit length
    std::string DisabledDescription;            // 24-bit length (declared max 16777215 in the descriptor;
                                                // no capture exceeds 61 chars, so only the descriptor proves it)
};

// JamBattlePayShopEntry - one tile in a group. ShopEntry.productID must agree with Product.productID or
// the client buys an id the server cannot resolve.
struct BattlePayShopEntry
{
    uint32 EntryID = 0;
    uint32 GroupID = 0;
    uint32 ProductID = 0;
    int32 Ordering = 0;
    int32 Flags = 0;
    uint8 BannerType = 0;
    Optional<BattlePayDisplayInfo> DisplayInfo;
    uint8 PadBits = 0;                          // 7 bits, 0 in every capture
};

// The whole decoded message. Serialize(Parse(x)) == x holds over all four arrays.
struct BattlePayCatalog
{
    uint32 Result = 0;
    uint32 CurrencyID = 0;
    std::vector<BattlePayCatalogProduct> Products;
    std::vector<BattlePayDeliverable> Deliverables;
    std::vector<BattlePayProductGroup> Groups;
    std::vector<BattlePayShopEntry> Entries;
    // Always empty for every capture we have (the message ends exactly at the last shop entry). Kept so
    // that a future build which appends something is echoed instead of silently truncated.
    std::vector<uint8> Remainder;

    // Byte offset one past the last product record in the blob Parse() consumed. Only useful for
    // diagnostics ("did my edit perturb the tail?").
    size_t AfterProducts = 0;
};

class TC_GAME_API BattlePayCatalogWriter
{
public:
    // Decodes a catalog body into fields. Returns false (and fills `error`, if given) when the body is
    // not a well-formed 68275 catalog - a short read, a length that runs past the end, or trailing bytes
    // that are not a whole record. Never throws, never reads out of bounds.
    static bool Parse(std::vector<uint8> const& body, BattlePayCatalog& out, std::string* error = nullptr);

    // Rebuilds a catalog body from fields. The array counts in the header are always taken from the
    // vectors, so adding/removing records is safe.
    static std::vector<uint8> Serialize(BattlePayCatalog const& catalog);

    // Serialize(Parse(blob)) == blob. Logs the record counts. The reskin path must not be trusted unless
    // this passes on the exact blob being reskinned.
    static bool SelfCheck(std::vector<uint8> const& templateBlob);

    // Builds a complete DisplayInfo from scratch for a product that has none (54 of the 94 shipped
    // records lack one, which is why their purchase confirmation shows a nil name). `donor`, when given,
    // only supplies art styling (card type, banner, model scene, override art and the display-card
    // scaffolding); all text and the icon come from the arguments.
    static BattlePayDisplayInfo MakeDisplayInfo(std::string name, std::string description,
        Optional<uint32> fileDataID = {}, BattlePayDisplayInfo const* donor = nullptr);
};

#endif // TRINITYCORE_BATTLE_PAY_CATALOG_WRITER_H
