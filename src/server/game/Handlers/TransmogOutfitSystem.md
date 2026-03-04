# Transmog Outfit System — Source Reference

Quick reference for developers working on the transmog outfit code in this directory.

## Architecture

```
Client (12.x Midnight)
  |
  |-- CMSG_TRANSMOG_OUTFIT_NEW          --> HandleTransmogOutfitNew()
  |-- CMSG_TRANSMOG_OUTFIT_UPDATE_INFO  --> HandleTransmogOutfitUpdateInfo()
  |-- CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS --> HandleTransmogOutfitUpdateSlots()
  |-- CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS --> HandleTransmogOutfitUpdateSituations()
  |
  |-- Spell 1247613 (effect 347)        --> Spell::EffectEquipTransmogOutfit()
  |
  |<-- SMSG_TRANSMOG_OUTFIT_NEW_ENTRY_ADDED
  |<-- SMSG_TRANSMOG_OUTFIT_INFO_UPDATED
  |<-- SMSG_TRANSMOG_OUTFIT_SLOTS_UPDATED
  |<-- SMSG_TRANSMOG_OUTFIT_SITUATIONS_UPDATED
  |<-- SMSG_ACCOUNT_TRANSMOG_UPDATE (login)
  |
  |<-- UpdateFields: ActivePlayerData::TransmogOutfits (map)
```

## Key Files

- **Packet parsing**: `../Server/Packets/TransmogrificationPackets.cpp` — CMSG Read(), SMSG Write(), `TransmogOutfitSlotToEquipSlot()`
- **Handlers**: `TransmogrificationHandler.cpp` (this directory) — all four Handle* functions, validation, NPC checks
- **Spell effect**: `../Spells/SpellEffects.cpp` → `EffectEquipTransmogOutfit()`
- **UpdateField sync**: `../Entities/Player/Player.cpp` → `_SyncTransmogOutfitsToActivePlayerData()`
- **Login sync**: `../Entities/Player/CollectionMgr.cpp` → `SendFavoriteAppearances()`

## Things That Will Break If You Change Them

1. **SetType must be 1** — `UF::TransmogOutfitDataInfo::SetType` = 1 (Outfit). Values 0 or 2 cause the client to ignore the outfit.

2. **The GUID field is the NPC** — All CMSG packets send the transmogrifier Creature GUID, not the player GUID. Validate with `GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_TRANSMOGRIFIER)`.

3. **Slot mapping is non-trivial** — Client TransmogOutfitSlot 0-14 does NOT map 1:1 to EQUIPMENT_SLOT 0-18. See `TransmogOutfitSlotToEquipSlot()` in TransmogrificationPackets.cpp.

4. **Secondary shoulder** — Transmog slot 2 is the left/secondary shoulder. It maps to `TRANSMOG_SECONDARY_SHOULDER_SLOT` (sentinel = `EQUIPMENT_SLOT_END + 1`), NOT to `EQUIPMENT_SLOT_SHOULDERS`. Store in `SecondaryShoulderApparanceID`, not `Appearances[]`.

5. **Hotfix hygiene** — Stale `hotfix_data` entries with Status=2 for TransmogSetItem/TransmogHoliday crash the client UI. If you add transmog hotfixes, be careful about record removals.

## Handler Flow

```
CMSG received
  -> Packet::Read() parses binary payload (heuristic for NEW/INFO, structured for SLOTS/SITUATIONS)
  -> Check ParseSuccess (if false, log error + trace, return)
  -> ValidateTransmogOutfitNpc() — NPC interaction check
  -> Look up / create EquipmentSetData
  -> ValidateTransmogOutfitSet() — appearance + enchant collection checks
  -> Player::SetEquipmentSet() — persists to DB
  -> Send SMSG response (SetID + Guid)
  -> _SyncTransmogOutfitsToActivePlayerData() — called internally by SetEquipmentSet
```

## Diagnostic Logging

All transmog logging uses filter string `"network.opcode.transmog"` (packet handlers) or `"spells.effect"` (spell handler). Enable in worldserver.conf:

```
Appender.Console.Type = Console
Logger.network.opcode.transmog = 1,Console
Logger.spells.effect = 1,Console
```

The parsers capture raw hex payload previews (first 128 bytes) and diagnostic read traces on every packet, logged at DEBUG level.
