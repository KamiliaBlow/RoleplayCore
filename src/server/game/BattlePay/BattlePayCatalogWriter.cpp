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

#include "BattlePayCatalogWriter.h"
#include "Log.h"
#include <algorithm>
#include <utility>

/*
 * Straight port of the prover c:/dumps/scratch_catalog_v2.py. Every record is rebuilt from decoded
 * FIELD VALUES - the only things carried as raw bits are the FlushBits padding bits (which are asserted
 * to be zero in the self-check) and the fields flagged HYPOTHESIS in the header. See the header comment
 * for what the old fixed-offset model was actually reading.
 */

namespace
{
    constexpr size_t CATALOG_HEADER_SIZE = 24;      // 6 x u32: result, currencyID, and FOUR array counts

    // Declared string capacities from the client's reflection descriptors (sizeArr @ tag-0x40);
    // bitWidth = bit_length(capacity - 1). Not guessed - read out of the client's own metadata.
    constexpr uint32 BITS_NAME1        = 10;        // char[513]
    constexpr uint32 BITS_NAME2        = 10;        // char[513]
    constexpr uint32 BITS_NAME3        = 13;        // char[4097]
    constexpr uint32 BITS_TOOLTIP      = 13;        // char[4097]
    constexpr uint32 BITS_INSTRUCTIONS = 13;        // char[4097]
    constexpr uint32 BITS_DISCLAIMER   = 13;        // char[4097]
    constexpr uint32 BITS_NYDUS_LINK   = 12;        // char[4001]
    constexpr uint32 BITS_CARD_TITLE   = 10;        // char[513]
    constexpr uint32 BITS_GROUP_NAME   = 8;         // char[256]
    constexpr uint32 BITS_GROUP_DESC   = 24;        // heap string, declared max 16777215
    constexpr uint32 BITS_DELIV_NAME   = 8;         // char[256]

    // MSB-first bit reader sharing its cursor with the byte reads, with FlushBits semantics. Every read
    // is bounds checked: on overrun the reader latches a failure and yields zeroes, so a malformed blob
    // unwinds instead of running away (a zero count simply stops the enclosing loop).
    class CatalogReader
    {
    public:
        CatalogReader(std::vector<uint8> const& buffer, size_t pos) : _buf(buffer), _pos(pos) { }

        bool Ok() const { return _ok; }
        size_t Pos() const { return _pos; }
        size_t Remaining() const { return _pos < _buf.size() ? _buf.size() - _pos : 0; }

        uint32 Bits(uint32 count)
        {
            uint32 value = 0;
            for (uint32 i = 0; i < count; ++i)
            {
                if (_pos >= _buf.size())
                {
                    _ok = false;
                    return 0;
                }
                value = (value << 1) | ((_buf[_pos] >> (7 - _bit)) & 1);
                if (++_bit == 8)
                {
                    _bit = 0;
                    ++_pos;
                }
            }
            return value;
        }

        // FlushBits: discard the rest of the current byte. Payload always resumes byte aligned.
        void FlushBits()
        {
            if (_bit)
            {
                _bit = 0;
                ++_pos;
            }
        }

        uint8 U8()
        {
            if (!Need(1))
                return 0;
            return _buf[_pos++];
        }

        uint32 U32()
        {
            if (!Need(4))
                return 0;
            uint32 const v = uint32(_buf[_pos]) | (uint32(_buf[_pos + 1]) << 8)
                | (uint32(_buf[_pos + 2]) << 16) | (uint32(_buf[_pos + 3]) << 24);
            _pos += 4;
            return v;
        }

        int32 I32() { return int32(U32()); }

        uint64 U64()
        {
            if (!Need(8))
                return 0;
            uint64 v = 0;
            for (size_t i = 0; i < 8; ++i)
                v |= uint64(_buf[_pos + i]) << (8 * i);
            _pos += 8;
            return v;
        }

        std::string Str(uint32 length)
        {
            if (!length)
                return std::string();
            if (!Need(length))
                return std::string();
            std::string s(reinterpret_cast<char const*>(&_buf[_pos]), length);
            _pos += length;
            return s;
        }

        // Guards a vector count read off the wire before anything is reserved for it.
        bool CountFits(uint32 count, size_t minElementBytes)
        {
            if (uint64(count) * minElementBytes > uint64(Remaining()))
            {
                _ok = false;
                return false;
            }
            return true;
        }

    private:
        bool Need(size_t bytes)
        {
            if (_bit || _pos + bytes > _buf.size())
            {
                // _bit != 0 would mean a byte read while bits are pending, i.e. a missing FlushBits.
                _ok = false;
                return false;
            }
            return true;
        }

        std::vector<uint8> const& _buf;
        size_t _pos = 0;
        uint32 _bit = 0;
        bool _ok = true;
    };

    // Mirror image of CatalogReader.
    class CatalogWriter
    {
    public:
        void Bits(uint32 count, uint32 value)
        {
            for (int32 i = int32(count) - 1; i >= 0; --i)
            {
                _acc = uint8((_acc << 1) | ((value >> i) & 1));
                if (++_nbit == 8)
                {
                    _out.push_back(_acc);
                    _acc = 0;
                    _nbit = 0;
                }
            }
        }

        void FlushBits()
        {
            if (_nbit)
            {
                _out.push_back(uint8(_acc << (8 - _nbit)));
                _acc = 0;
                _nbit = 0;
            }
        }

        // The byte writers flush first so a forgotten explicit FlushBits can never emit a byte into the
        // middle of a bit block. It is only a safety net: the reader deliberately FAILS on a byte read
        // while bits are pending, so a genuinely missing flush still shows up as a self-check failure.
        void U8(uint8 v) { FlushBits(); _out.push_back(v); }

        void U32(uint32 v)
        {
            FlushBits();
            for (size_t i = 0; i < 4; ++i)
                _out.push_back(uint8((v >> (8 * i)) & 0xFF));
        }

        void I32(int32 v) { U32(uint32(v)); }

        void U64(uint64 v)
        {
            FlushBits();
            for (size_t i = 0; i < 8; ++i)
                _out.push_back(uint8((v >> (8 * i)) & 0xFF));
        }

        void Str(std::string const& s)
        {
            FlushBits();
            _out.insert(_out.end(), s.begin(), s.end());
        }

        std::vector<uint8>& Data() { return _out; }

    private:
        std::vector<uint8> _out;
        uint8 _acc = 0;
        uint32 _nbit = 0;
    };

    // Strings are length-prefixed in the bit block; a value longer than its declared capacity cannot be
    // expressed on the wire, so clamp instead of emitting a truncated length that would desync the client.
    uint32 BitLength(std::string const& s, uint32 bits)
    {
        uint64 const cap = (uint64(1) << bits) - 1;
        return uint32(std::min<uint64>(s.size(), cap));
    }

    void ClampString(std::string& s, uint32 bits)
    {
        uint64 const cap = (uint64(1) << bits) - 1;
        if (s.size() > cap)
            s.resize(size_t(cap));
    }

    // The string as it will actually go on the wire, i.e. matching the length BitLength() emitted.
    std::string Clamped(std::string const& s, uint32 bits)
    {
        std::string out = s;
        ClampString(out, bits);
        return out;
    }

    // ------------------------------------------------------------------ DisplayInfo

    void ReadDisplayInfo(CatalogReader& r, BattlePayDisplayInfo& d)
    {
        // 90-bit MSB-first header, then FlushBits -> 12 bytes (90 + 6 pad). Confirmed against the client
        // DisplayInfo parser @ RVA 0x6988A0.
        bool const hasFileDataID = r.Bits(1) != 0;
        bool const hasModelSceneID = r.Bits(1) != 0;
        uint32 const lenName1 = r.Bits(BITS_NAME1);
        uint32 const lenName2 = r.Bits(BITS_NAME2);
        uint32 const lenName3 = r.Bits(BITS_NAME3);
        uint32 const lenTooltip = r.Bits(BITS_TOOLTIP);
        uint32 const lenInstructions = r.Bits(BITS_INSTRUCTIONS);
        bool const hasFlags = r.Bits(1) != 0;
        bool const hasOverrideTextColor = r.Bits(1) != 0;
        bool const hasOverrideTexture = r.Bits(1) != 0;
        bool const hasOverrideBackground = r.Bits(1) != 0;
        uint32 const lenDisclaimer = r.Bits(BITS_DISCLAIMER);
        uint32 const lenNydusLink = r.Bits(BITS_NYDUS_LINK);
        d.PadBits = uint8(r.Bits(6));
        r.FlushBits();

        uint32 const cardCount = r.U32();
        d.BattlepayCardType = r.I32();
        d.BannerType = r.I32();
        d.ItemQuantity = r.I32();
        if (hasFileDataID)
            d.FileDataID = r.U32();
        if (hasModelSceneID)
            d.ModelSceneID = r.U32();
        d.Name1 = r.Str(lenName1);
        d.Name2 = r.Str(lenName2);
        d.Name3 = r.Str(lenName3);
        d.Tooltip = r.Str(lenTooltip);
        d.Instructions = r.Str(lenInstructions);
        if (hasFlags)
            d.Flags = r.U32();
        if (hasOverrideTextColor)
            d.OverrideTextColor = r.U32();          // HYPOTHESIS: never present on the wire
        if (hasOverrideTexture)
            d.OverrideTexture = r.U32();
        if (hasOverrideBackground)
            d.OverrideBackground = r.U32();
        d.Disclaimer = r.Str(lenDisclaimer);
        d.NydusLink = r.Str(lenNydusLink);

        // A card is at least 2 (bit block) + 12 (three u32) bytes.
        if (!r.CountFits(cardCount, 14))
            return;
        d.DisplayCards.resize(cardCount);
        for (BattlePayDisplayCard& card : d.DisplayCards)
        {
            uint32 const titleLength = r.Bits(BITS_CARD_TITLE);
            card.PadBits = uint8(r.Bits(6));
            r.FlushBits();
            card.CreatureDisplayInfoID = r.U32();
            card.ModelSceneID = r.U32();
            card.TransmogSetID = r.U32();
            card.Title = r.Str(titleLength);
            if (!r.Ok())
                return;
        }
    }

    void WriteDisplayInfo(CatalogWriter& w, BattlePayDisplayInfo const& d)
    {
        w.Bits(1, d.FileDataID ? 1 : 0);
        w.Bits(1, d.ModelSceneID ? 1 : 0);
        w.Bits(BITS_NAME1, BitLength(d.Name1, BITS_NAME1));
        w.Bits(BITS_NAME2, BitLength(d.Name2, BITS_NAME2));
        w.Bits(BITS_NAME3, BitLength(d.Name3, BITS_NAME3));
        w.Bits(BITS_TOOLTIP, BitLength(d.Tooltip, BITS_TOOLTIP));
        w.Bits(BITS_INSTRUCTIONS, BitLength(d.Instructions, BITS_INSTRUCTIONS));
        w.Bits(1, d.Flags ? 1 : 0);
        w.Bits(1, d.OverrideTextColor ? 1 : 0);
        w.Bits(1, d.OverrideTexture ? 1 : 0);
        w.Bits(1, d.OverrideBackground ? 1 : 0);
        w.Bits(BITS_DISCLAIMER, BitLength(d.Disclaimer, BITS_DISCLAIMER));
        w.Bits(BITS_NYDUS_LINK, BitLength(d.NydusLink, BITS_NYDUS_LINK));
        w.Bits(6, d.PadBits);
        w.FlushBits();

        w.U32(uint32(d.DisplayCards.size()));
        w.I32(d.BattlepayCardType);
        w.I32(d.BannerType);
        w.I32(d.ItemQuantity);
        if (d.FileDataID)
            w.U32(*d.FileDataID);
        if (d.ModelSceneID)
            w.U32(*d.ModelSceneID);

        w.Str(Clamped(d.Name1, BITS_NAME1));
        w.Str(Clamped(d.Name2, BITS_NAME2));
        w.Str(Clamped(d.Name3, BITS_NAME3));
        w.Str(Clamped(d.Tooltip, BITS_TOOLTIP));
        w.Str(Clamped(d.Instructions, BITS_INSTRUCTIONS));

        if (d.Flags)
            w.U32(*d.Flags);
        if (d.OverrideTextColor)
            w.U32(*d.OverrideTextColor);
        if (d.OverrideTexture)
            w.U32(*d.OverrideTexture);
        if (d.OverrideBackground)
            w.U32(*d.OverrideBackground);

        w.Str(Clamped(d.Disclaimer, BITS_DISCLAIMER));
        w.Str(Clamped(d.NydusLink, BITS_NYDUS_LINK));

        for (BattlePayDisplayCard const& card : d.DisplayCards)
        {
            w.Bits(BITS_CARD_TITLE, BitLength(card.Title, BITS_CARD_TITLE));
            w.Bits(6, card.PadBits);
            w.FlushBits();
            w.U32(card.CreatureDisplayInfoID);
            w.U32(card.ModelSceneID);
            w.U32(card.TransmogSetID);
            w.Str(Clamped(card.Title, BITS_CARD_TITLE));
        }
    }

    // ------------------------------------------------------------------ Product

    void ReadProduct(CatalogReader& r, BattlePayCatalogProduct& p)
    {
        p.ProductID = r.U32();                          // was misread as DisplayInfo.fileDataID @ rec+81
        p.NormalPriceFixedPoint = r.U64();
        p.CurrentPriceFixedPoint = r.U64();
        uint32 const deliverableCount = r.U32();
        p.Type = r.I32();
        p.Flags = r.I32();                              // was misread as DisplayInfo.modelSceneID @ rec+85
        p.RequiredDeliverableID = r.U32();
        uint32 const bundledCount = r.U32();
        p.Eligibility = r.I32();
        p.PmtProductID = r.U64();

        if (!r.CountFits(deliverableCount, 4))
            return;
        p.DeliverableIDs.resize(deliverableCount);
        for (uint32& id : p.DeliverableIDs)
            id = r.U32();

        if (!r.CountFits(bundledCount, 4))
            return;
        p.BundledProductIDs.resize(bundledCount);
        for (uint32& id : p.BundledProductIDs)
            id = r.U32();

        bool const hasDisplayInfo = r.Bits(1) != 0;
        p.PadBits = uint8(r.Bits(7));
        r.FlushBits();
        if (hasDisplayInfo)
        {
            p.DisplayInfo.emplace();
            ReadDisplayInfo(r, *p.DisplayInfo);
        }
    }

    void WriteProduct(CatalogWriter& w, BattlePayCatalogProduct const& p)
    {
        w.U32(p.ProductID);
        w.U64(p.NormalPriceFixedPoint);
        w.U64(p.CurrentPriceFixedPoint);
        w.U32(uint32(p.DeliverableIDs.size()));
        w.I32(p.Type);
        w.I32(p.Flags);
        w.U32(p.RequiredDeliverableID);
        w.U32(uint32(p.BundledProductIDs.size()));
        w.I32(p.Eligibility);
        w.U64(p.PmtProductID);
        for (uint32 id : p.DeliverableIDs)
            w.U32(id);
        for (uint32 id : p.BundledProductIDs)
            w.U32(id);
        w.Bits(1, p.DisplayInfo ? 1 : 0);
        w.Bits(7, p.PadBits);
        w.FlushBits();
        if (p.DisplayInfo)
            WriteDisplayInfo(w, *p.DisplayInfo);
    }

    // ------------------------------------------------------------------ Deliverable / Choice

    void ReadChoice(CatalogReader& r, BattlePayDeliverableChoice& c)
    {
        // HYPOTHESIS in full: nChoices is 0 in all 10 captures, so no choice record has ever been seen.
        c.ID = r.U32();
        c.Type = r.I32();
        c.ItemID = r.U32();
        c.Quantity = r.U32();
        c.MountSpellID = r.U32();
        c.BattlePetCreatureID = r.U32();
        c.AlreadyOwns = uint8(r.Bits(1));
        c.HasPetResult = uint8(r.Bits(1));
        bool const hasDisplayInfo = r.Bits(1) != 0;
        c.PetResult = uint8(r.Bits(4));
        c.PadBits = uint8(r.Bits(1));
        r.FlushBits();
        if (hasDisplayInfo)
        {
            c.DisplayInfo.emplace();
            ReadDisplayInfo(r, *c.DisplayInfo);
        }
    }

    void WriteChoice(CatalogWriter& w, BattlePayDeliverableChoice const& c)
    {
        w.U32(c.ID);
        w.I32(c.Type);
        w.U32(c.ItemID);
        w.U32(c.Quantity);
        w.U32(c.MountSpellID);
        w.U32(c.BattlePetCreatureID);
        w.Bits(1, c.AlreadyOwns ? 1 : 0);
        w.Bits(1, c.HasPetResult ? 1 : 0);
        w.Bits(1, c.DisplayInfo ? 1 : 0);
        w.Bits(4, c.PetResult);
        w.Bits(1, c.PadBits);
        w.FlushBits();
        if (c.DisplayInfo)
            WriteDisplayInfo(w, *c.DisplayInfo);
    }

    void ReadDeliverable(CatalogReader& r, BattlePayDeliverable& d)
    {
        d.DeliverableID = r.U32();
        d.Type = r.I32();
        d.ItemID = r.U32();
        d.Quantity = r.U32();
        d.MountSpellID = r.U32();
        d.BattlePetCreatureID = r.U32();
        d.BoostID = r.I32();
        d.Flags = r.I32();
        d.TransItemModifiedAppearanceID = r.U32();
        d.TransmogSetID = r.U32();
        d.CharTitleID = r.U32();
        d.SpellItemEnchantmentID = r.U32();
        d.WarbandSceneID = r.U32();

        // 24-bit block; split taken from ReadDeliverable @ RVA 0x699460, NOT from the data (nChoices and
        // petResult are zero in every capture, so a round trip cannot tell the split apart).
        uint32 const nameLength = r.Bits(BITS_DELIV_NAME);
        d.AlreadyOwns = uint8(r.Bits(1));
        d.HasPetResult = uint8(r.Bits(1));
        uint32 const choiceCount = r.Bits(7);
        bool const hasDisplayInfo = r.Bits(1) != 0;
        d.PetResult = uint8(r.Bits(4));
        d.PadBits = uint8(r.Bits(2));
        r.FlushBits();                                  // no-op: the block is exactly 24 bits

        // The client reads the choice vector BEFORE the name string.
        if (!r.CountFits(choiceCount, 25))
            return;
        d.Choices.resize(choiceCount);
        for (BattlePayDeliverableChoice& choice : d.Choices)
        {
            ReadChoice(r, choice);
            if (!r.Ok())
                return;
        }
        d.Name = r.Str(nameLength);
        if (hasDisplayInfo)
        {
            d.DisplayInfo.emplace();
            ReadDisplayInfo(r, *d.DisplayInfo);
        }
    }

    void WriteDeliverable(CatalogWriter& w, BattlePayDeliverable const& d)
    {
        w.U32(d.DeliverableID);
        w.I32(d.Type);
        w.U32(d.ItemID);
        w.U32(d.Quantity);
        w.U32(d.MountSpellID);
        w.U32(d.BattlePetCreatureID);
        w.I32(d.BoostID);
        w.I32(d.Flags);
        w.U32(d.TransItemModifiedAppearanceID);
        w.U32(d.TransmogSetID);
        w.U32(d.CharTitleID);
        w.U32(d.SpellItemEnchantmentID);
        w.U32(d.WarbandSceneID);
        w.Bits(BITS_DELIV_NAME, BitLength(d.Name, BITS_DELIV_NAME));
        w.Bits(1, d.AlreadyOwns ? 1 : 0);
        w.Bits(1, d.HasPetResult ? 1 : 0);
        // The count field is only 7 bits wide, so more than 127 choices cannot be expressed; emit the
        // ones the count covers rather than a count that disagrees with the payload.
        size_t const choiceCount = std::min<size_t>(d.Choices.size(), 127);
        w.Bits(7, uint32(choiceCount));
        w.Bits(1, d.DisplayInfo ? 1 : 0);
        w.Bits(4, d.PetResult);
        w.Bits(2, d.PadBits);
        w.FlushBits();
        for (size_t i = 0; i < choiceCount; ++i)
            WriteChoice(w, d.Choices[i]);
        w.Str(Clamped(d.Name, BITS_DELIV_NAME));
        if (d.DisplayInfo)
            WriteDisplayInfo(w, *d.DisplayInfo);
    }

    // ------------------------------------------------------------------ ProductGroup / ShopEntry

    void ReadGroup(CatalogReader& r, BattlePayProductGroup& g)
    {
        g.GroupID = r.U32();
        g.IconFileDataID = r.I32();
        g.DisplayType = r.U8();
        g.Ordering = r.I32();
        g.Flags = r.I32();
        g.ParentGroupID = r.U32();
        uint32 const nameLength = r.Bits(BITS_GROUP_NAME);
        uint32 const descLength = r.Bits(BITS_GROUP_DESC);
        r.FlushBits();                                  // no-op: the block is exactly 32 bits
        g.Name = r.Str(nameLength);
        g.DisabledDescription = r.Str(descLength);
    }

    void WriteGroup(CatalogWriter& w, BattlePayProductGroup const& g)
    {
        w.U32(g.GroupID);
        w.I32(g.IconFileDataID);
        w.U8(g.DisplayType);
        w.I32(g.Ordering);
        w.I32(g.Flags);
        w.U32(g.ParentGroupID);
        w.Bits(BITS_GROUP_NAME, BitLength(g.Name, BITS_GROUP_NAME));
        w.Bits(BITS_GROUP_DESC, BitLength(g.DisabledDescription, BITS_GROUP_DESC));
        w.FlushBits();
        w.Str(Clamped(g.Name, BITS_GROUP_NAME));
        w.Str(Clamped(g.DisabledDescription, BITS_GROUP_DESC));
    }

    void ReadEntry(CatalogReader& r, BattlePayShopEntry& e)
    {
        e.EntryID = r.U32();
        e.GroupID = r.U32();
        e.ProductID = r.U32();
        e.Ordering = r.I32();
        e.Flags = r.I32();
        e.BannerType = r.U8();
        bool const hasDisplayInfo = r.Bits(1) != 0;
        e.PadBits = uint8(r.Bits(7));
        r.FlushBits();
        if (hasDisplayInfo)
        {
            e.DisplayInfo.emplace();
            ReadDisplayInfo(r, *e.DisplayInfo);
        }
    }

    void WriteEntry(CatalogWriter& w, BattlePayShopEntry const& e)
    {
        w.U32(e.EntryID);
        w.U32(e.GroupID);
        w.U32(e.ProductID);
        w.I32(e.Ordering);
        w.I32(e.Flags);
        w.U8(e.BannerType);
        w.Bits(1, e.DisplayInfo ? 1 : 0);
        w.Bits(7, e.PadBits);
        w.FlushBits();
        if (e.DisplayInfo)
            WriteDisplayInfo(w, *e.DisplayInfo);
    }

    bool Fail(std::string* error, char const* message)
    {
        if (error)
            *error = message;
        return false;
    }
}

bool BattlePayCatalogWriter::Parse(std::vector<uint8> const& body, BattlePayCatalog& out, std::string* error /*= nullptr*/)
{
    out = BattlePayCatalog();

    if (body.size() < CATALOG_HEADER_SIZE)
        return Fail(error, "body shorter than the 24-byte header");

    CatalogReader r(body, 0);
    out.Result = r.U32();
    out.CurrencyID = r.U32();
    uint32 const productCount = r.U32();
    uint32 const deliverableCount = r.U32();
    uint32 const groupCount = r.U32();
    uint32 const entryCount = r.U32();

    // Cheapest possible records: product 53, deliverable 55, group 25, entry 22 bytes.
    if (!r.CountFits(productCount, 53) || !r.CountFits(deliverableCount, 55)
        || !r.CountFits(groupCount, 25) || !r.CountFits(entryCount, 22))
        return Fail(error, "header record counts cannot fit in the body");

    out.Products.resize(productCount);
    for (BattlePayCatalogProduct& product : out.Products)
    {
        ReadProduct(r, product);
        if (!r.Ok())
            return Fail(error, "truncated product record");
    }
    out.AfterProducts = r.Pos();

    out.Deliverables.resize(deliverableCount);
    for (BattlePayDeliverable& deliverable : out.Deliverables)
    {
        ReadDeliverable(r, deliverable);
        if (!r.Ok())
            return Fail(error, "truncated deliverable record");
    }

    out.Groups.resize(groupCount);
    for (BattlePayProductGroup& group : out.Groups)
    {
        ReadGroup(r, group);
        if (!r.Ok())
            return Fail(error, "truncated product group record");
    }

    out.Entries.resize(entryCount);
    for (BattlePayShopEntry& entry : out.Entries)
    {
        ReadEntry(r, entry);
        if (!r.Ok())
            return Fail(error, "truncated shop entry record");
    }

    // Every capture ends exactly here (0 bytes). Anything else is echoed rather than dropped.
    out.Remainder.assign(body.begin() + r.Pos(), body.end());
    return true;
}

std::vector<uint8> BattlePayCatalogWriter::Serialize(BattlePayCatalog const& catalog)
{
    CatalogWriter w;
    w.U32(catalog.Result);
    w.U32(catalog.CurrencyID);
    w.U32(uint32(catalog.Products.size()));
    w.U32(uint32(catalog.Deliverables.size()));
    w.U32(uint32(catalog.Groups.size()));
    w.U32(uint32(catalog.Entries.size()));

    for (BattlePayCatalogProduct const& product : catalog.Products)
        WriteProduct(w, product);
    for (BattlePayDeliverable const& deliverable : catalog.Deliverables)
        WriteDeliverable(w, deliverable);
    for (BattlePayProductGroup const& group : catalog.Groups)
        WriteGroup(w, group);
    for (BattlePayShopEntry const& entry : catalog.Entries)
        WriteEntry(w, entry);

    w.FlushBits();
    std::vector<uint8> out = std::move(w.Data());
    out.insert(out.end(), catalog.Remainder.begin(), catalog.Remainder.end());
    return out;
}

bool BattlePayCatalogWriter::SelfCheck(std::vector<uint8> const& templateBlob)
{
    BattlePayCatalog catalog;
    std::string error;
    if (!Parse(templateBlob, catalog, &error))
    {
        TC_LOG_ERROR("server.loading", "BattlePayCatalogWriter: self-check FAILED - template did not parse ({}).", error);
        return false;
    }

    std::vector<uint8> const rebuilt = Serialize(catalog);
    if (rebuilt == templateBlob)
    {
        TC_LOG_INFO("server.loading", "BattlePayCatalogWriter: self-check PASS - byte-exact round trip of "
            "{} products / {} deliverables / {} groups / {} shop entries ({} bytes, {} verbatim).",
            catalog.Products.size(), catalog.Deliverables.size(), catalog.Groups.size(),
            catalog.Entries.size(), templateBlob.size(), catalog.Remainder.size());
        return true;
    }

    size_t firstDiff = 0;
    size_t const cmpLen = std::min(rebuilt.size(), templateBlob.size());
    while (firstDiff < cmpLen && rebuilt[firstDiff] == templateBlob[firstDiff])
        ++firstDiff;
    TC_LOG_ERROR("server.loading", "BattlePayCatalogWriter: self-check FAILED - sizes {}/{}, first diff @ byte {}.",
        rebuilt.size(), templateBlob.size(), firstDiff);
    return false;
}

BattlePayDisplayInfo BattlePayCatalogWriter::MakeDisplayInfo(std::string name, std::string description,
    Optional<uint32> fileDataID /*= {}*/, BattlePayDisplayInfo const* donor /*= nullptr*/)
{
    BattlePayDisplayInfo info;
    info.Name1 = std::move(name);
    info.Name3 = std::move(description);
    info.FileDataID = fileDataID;

    if (donor)
    {
        // Art styling only - never text. A donor is optional; a from-scratch DisplayInfo (all art fields
        // zero, no cards) parses fine, it just renders as a plain card.
        info.BattlepayCardType = donor->BattlepayCardType;
        info.BannerType = donor->BannerType;
        info.ItemQuantity = donor->ItemQuantity;
        info.ModelSceneID = donor->ModelSceneID;
        info.Flags = donor->Flags;
        info.OverrideTextColor = donor->OverrideTextColor;
        info.OverrideTexture = donor->OverrideTexture;
        info.OverrideBackground = donor->OverrideBackground;
        if (!info.FileDataID)
            info.FileDataID = donor->FileDataID;
        info.DisplayCards = donor->DisplayCards;
        for (BattlePayDisplayCard& card : info.DisplayCards)
            card.Title = info.Name1;
    }

    ClampString(info.Name1, BITS_NAME1);
    ClampString(info.Name3, BITS_NAME3);
    return info;
}
