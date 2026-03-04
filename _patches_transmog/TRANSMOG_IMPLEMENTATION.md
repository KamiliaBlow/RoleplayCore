# Transmog Outfit System — Implementation Reference

This document captures the complete knowledge needed to understand, maintain, and replicate the transmog outfit UI integration for the 12.x / Midnight client. The system was reverse-engineered from client packet captures and Lua source inspection.

## Overview

The 12.x client has a wardrobe/transmog outfit UI that lets players save, rename, update, and apply appearance outfits. This requires:

1. **Four CMSG packet parsers** — binary layout reverse-engineered from hex dumps
2. **Four SMSG response packets** — minimal ACKs (SetID + Guid)
3. **UpdateField sync** — `TransmogOutfits` map on `ActivePlayerData`
4. **Spell effect handler** — effect 347 (`SPELL_EFFECT_EQUIP_TRANSMOG_OUTFIT`) for applying outfits
5. **Hotfix hygiene** — stale TransmogSetItem/TransmogHoliday deletes cause client Lua crashes

---

## File Map

| File | Role |
|---|---|
| `src/server/game/Server/Packets/TransmogrificationPackets.h` | Packet class declarations, slot entry structs |
| `src/server/game/Server/Packets/TransmogrificationPackets.cpp` | CMSG Read() parsers, SMSG Write() methods, slot mapping |
| `src/server/game/Handlers/TransmogrificationHandler.cpp` | Handler functions, NPC validation, outfit validation |
| `src/server/game/Spells/SpellEffects.cpp` | `EffectEquipTransmogOutfit()` — spell effect 347 |
| `src/server/game/Entities/Player/Player.cpp` | `_SyncTransmogOutfitsToActivePlayerData()` — UpdateField sync |
| `src/server/game/Entities/Player/CollectionMgr.cpp` | `SendFavoriteAppearances()` — account transmog update at login |

---

## Critical Discovery: The NPC GUID

**The single most important thing to know**: All four `CMSG_TRANSMOG_OUTFIT_*` packets send the **transmogrifier NPC's GUID**, not the player's GUID. This matches `CMSG_TRANSMOGRIFY_ITEMS` behavior.

The field was initially named `PlayerGuid` and validated against `session->GetPlayer()->GetGUID()`. Since the client sends a Creature GUID, this check always failed silently (only logged at DEBUG level). All outfit operations were rejected for over a day before this was discovered.

**Correct validation**: Use `GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_TRANSMOGRIFIER)` — the same pattern as `HandleTransmogrifyItems`.

---

## TransmogOutfitSlot Mapping (Client <-> Server)

The client uses a 15-element `TransmogOutfitSlot` enum (from `TransmogOutfitConstantsDocumentation.lua`) that does NOT map 1:1 to server `EQUIPMENT_SLOT` values.

```
TransmogOutfitSlot  Name              EQUIPMENT_SLOT        Slot #
─────────────────────────────────────────────────────────────────
 0                  Head              EQUIPMENT_SLOT_HEAD      0
 1                  ShoulderRight     EQUIPMENT_SLOT_SHOULDERS 2
 2                  ShoulderLeft      (secondary shoulder)     —
 3                  Back              EQUIPMENT_SLOT_BACK     14
 4                  Chest             EQUIPMENT_SLOT_CHEST     4
 5                  Tabard            EQUIPMENT_SLOT_TABARD   18
 6                  Body (Shirt)      EQUIPMENT_SLOT_BODY      3
 7                  Wrist             EQUIPMENT_SLOT_WRISTS    8
 8                  Hand              EQUIPMENT_SLOT_HANDS     9
 9                  Waist             EQUIPMENT_SLOT_WAIST     5
10                  Legs              EQUIPMENT_SLOT_LEGS      6
11                  Feet              EQUIPMENT_SLOT_FEET      7
12                  WeaponMainHand    EQUIPMENT_SLOT_MAINHAND 15
13                  WeaponOffHand     EQUIPMENT_SLOT_OFFHAND  16
14                  WeaponRanged      EQUIPMENT_SLOT_RANGED   17
```

**Secondary Shoulder (slot 2)**: The 12.x client supports asymmetric shoulder appearances. Transmog slot 1 = right shoulder (primary), slot 2 = left shoulder (secondary). Both reference `EQUIPMENT_SLOT_SHOULDERS` on the server, but slot 2 must be stored in `SecondaryShoulderApparanceID` on the outfit data, not in `Appearances[EQUIPMENT_SLOT_SHOULDERS]`.

Implementation uses a sentinel value `TRANSMOG_SECONDARY_SHOULDER_SLOT = EQUIPMENT_SLOT_END + 1` returned by `TransmogOutfitSlotToEquipSlot()` for transmog slot 2. Callers check for this sentinel and route to the secondary shoulder fields.

---

## CMSG Packet Binary Layouts

These were reverse-engineered from hex dumps. The parsers use heuristic approaches because no official documentation exists.

### CMSG_TRANSMOG_OUTFIT_NEW

```
[NPC PackedGuid]        variable-length packed ObjectGuid (transmogrifier NPC)
--- middle section (6+ bytes) ---
[MiddleType: u8]        observed as 0
[MiddleFlags: u8]       observed as 0
[IconFileDataID: u32]   icon file data ID
--- optional slot data (N * 16 bytes, where N = (middleLength - 6) / 16) ---
[AppearanceID: u32]     item modified appearance ID
[RawSlotField: u32]     high byte = TransmogOutfitSlot index
[Reserved1: u32]        unknown, observed as 0
[Reserved2: u32]        unknown, observed as 0
--- name trailer ---
[NameLength: u8]        length of name string
[Padding: u8]           observed as 0
[Name: bytes]           ASCII outfit name (NameLength bytes)
```

**Parser approach**: Scan backwards from end of payload for printable ASCII (0x20–0x7E) to find the name. Validate the length byte 2 positions before the name start. Everything between offset 6 and the name trailer is the "middle section" — first 6 bytes are type/flags/icon, remaining bytes may be slot data.

### CMSG_TRANSMOG_OUTFIT_UPDATE_INFO

```
[SetID: u32]            outfit ID to update
[NPC PackedGuid]
--- middle section (5 bytes) ---
[MiddleType: u8]        observed as 0
[IconFileDataID: u32]   icon file data ID (NO flags byte — only 5 bytes)
--- name trailer ---
[NameLength: u8]
[Padding: u8]
[Name: bytes]
```

Note: UPDATE_INFO has a 5-byte middle section (no flags byte), vs 6 for NEW.

### CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS

```
[SetID: u32]            outfit ID to update
[SlotCount: u32]        number of slot entries
[NPC PackedGuid]
[AlignmentGap: bytes]   variable padding between guid and slot data
--- per slot (SlotCount * 16 bytes) ---
[AppearanceID: u32]     item modified appearance ID
[RawSlotField: u32]     high byte = TransmogOutfitSlot index (>> 24)
[Reserved1: u32]
[Reserved2: u32]
```

**Parser approach**: Calculate expected slot bytes (slotCount * 16), subtract from remaining bytes to find alignment gap, skip gap bytes, then read structured slot entries.

### CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS

```
[SetID: u32]
[NPC PackedGuid]
[Count: u32]
--- per situation (Count * 16 bytes) ---
[SituationID: u32]
[SpecID: u32]
[LoadoutID: u32]
[EquipmentSetID: u32]
```

This is the cleanest parser — straightforward structured reads. Situations are logged but not persisted (no DB table exists yet).

---

## SMSG Response Packets

All four responses are minimal — just SetID + Guid:

```cpp
// All four Write() methods:
_worldPacket << uint32(SetID);
_worldPacket << uint64(Guid);
```

| Opcode | When Sent |
|---|---|
| `SMSG_TRANSMOG_OUTFIT_NEW_ENTRY_ADDED` | After creating a new outfit |
| `SMSG_TRANSMOG_OUTFIT_INFO_UPDATED` | After renaming/re-iconing an outfit |
| `SMSG_TRANSMOG_OUTFIT_SLOTS_UPDATED` | After updating slot appearances |
| `SMSG_TRANSMOG_OUTFIT_SITUATIONS_UPDATED` | After updating situation assignments |

---

## UpdateField Sync (`_SyncTransmogOutfitsToActivePlayerData`)

Located in `Player.cpp`. Called when:
- Equipment sets are loaded at login
- A set is created/updated/deleted
- The character loads into the world

The function:
1. Clears all existing `TransmogOutfits` from the update field map
2. Iterates all equipment sets where `Type == TRANSMOG`
3. For each outfit, creates a `UF::TransmogOutfitData` with:
   - `SetType = 1` (Outfit) — **must be 1, not 0 or 2**
   - `Name` and `Icon` from the saved set
   - 15 slot entries using the TransmogOutfitSlot mapping table
   - `AppearanceDisplayType = 1` for assigned slots, `0` for empty
   - Enchant illusions on mainhand/offhand slots
4. Sets `TransmogOutfitMetadata` with the first outfit ID
5. Sets `ViewedOutfit` to the first outfit ID

---

## Spell Effect 347: EffectEquipTransmogOutfit

When a player clicks "Apply" in the wardrobe UI, the client casts spell 1247613 with `SPELL_EFFECT_EQUIP_TRANSMOG_OUTFIT`. The `MiscValue` contains the outfit's SetID.

The handler in `SpellEffects.cpp`:
1. Looks up the outfit via `GetTransmogOutfitBySetID(effectInfo->MiscValue)`
2. For each equipment slot not in the ignore mask:
   - Sets `ITEM_MODIFIER_TRANSMOG_APPEARANCE_ALL_SPECS` (clears per-spec overrides)
   - For shoulders: also sets `ITEM_MODIFIER_TRANSMOG_SECONDARY_APPEARANCE_ALL_SPECS`
   - For mainhand/offhand: also sets `ITEM_MODIFIER_ENCHANT_ILLUSION_ALL_SPECS`
3. Calls `SetVisibleItemSlot()`, `SetNotRefundable()`, `ClearSoulboundTradeable()`, marks `ITEM_CHANGED`

No appearance validation at equip time — assumed valid from when the outfit was saved.

---

## Validation

`ValidateTransmogOutfitSet()` in `TransmogrificationHandler.cpp`:
- Checks `SetID < MAX_EQUIPMENT_SET_INDEX`
- Forces `Type = TRANSMOG`
- For each slot: validates appearance exists in `ItemModifiedAppearance.db2` and is collected
- Validates enchant illusions (ItemVisual exists, AllowTransmog flag, class restriction, condition)
- Clears pieces (no item links in transmog outfits)
- Normalizes ignore mask

---

## Hotfix Crash: TransmogSetItem / TransmogHoliday

`hotfix_data` entries with `Status = 2` (RecordRemoved) for TransmogSetItem records caused client crashes. 107 entries instructed the client to delete DB2 records, orphaning TransmogSet parent entries. When the client called `C_TransmogOutfitInfo.GetSourceIDsForSlot()`, it returned nil, crashing `Blizzard_Transmog.lua:2488` approximately 40 times per session.

**Fix**: Delete the stale hotfix_data rows for TransmogSetItem and TransmogHoliday tables.

---

## Known Gaps

1. **Situation persistence**: `HandleTransmogOutfitUpdateSituations` ACKs but discards data. No DB table, no auto-switch logic.
2. **SMSG responses are minimal**: Retail may include additional data beyond SetID + Guid.
3. **Outfit deletion**: Assumed to route through `CMSG_DELETE_EQUIPMENT_SET` since transmog outfits are stored as `EquipmentSetInfo` with `Type=TRANSMOG`. Not explicitly verified.
4. **No cost enforcement**: Outfit save/update operations are free. `HandleTransmogrifyItems` charges gold per item, but outfit-level operations don't.
5. **Heuristic parsers**: The backward-ASCII-scan approach for NEW/UPDATE_INFO breaks on non-ASCII (Unicode) outfit names or if the binary layout changes in a client patch.
6. **Secondary shoulder in NEW parser**: Extra middle bytes are heuristically parsed as slot data when length is a multiple of 16. This is speculative but validated downstream.
