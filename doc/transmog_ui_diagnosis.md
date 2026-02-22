# Transmog UI Diagnosis (RoleplayCore)

This note documents the current server-side transmog/wardrobe state in this branch and why the UI can show completed sets but still fail to edit/apply outfits.

## What is implemented

- `CMSG_TRANSMOGRIFY_ITEMS` is implemented and routed to `WorldSession::HandleTransmogrifyItems`.
- Account appearance ownership loading is implemented through `CollectionMgr::LoadAccountItemAppearances(...)`, which reads bitmasks from login DB and applies them to player transmog flags.
- Character transmog outfit rows are loaded from `characters.character_transmog_outfits` by `Player::_LoadTransmogOutfits(...)` during login.

## What is missing (critical)

The outfit-editing opcodes are currently explicitly marked unhandled:

- `CMSG_TRANSMOG_OUTFIT_NEW` -> `Handle_NULL`
- `CMSG_TRANSMOG_OUTFIT_UPDATE_INFO` -> `Handle_NULL`
- `CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS` -> `Handle_NULL`
- `CMSG_TRANSMOG_OUTFIT_UPDATE_SLOTS` -> `Handle_NULL`

This means the client can open wardrobe UI and display passive data, but any action requiring these CMSGs (creating/selecting/updating/applying outfit state) is ignored by the server.

## Why SQL seeding alone did not fix it

Even when `character_transmog_outfits` exists and account appearance masks are populated:

- Outfit CMSGs are still dropped (unhandled), so client-side editing/apply flows cannot complete.
- The only packet type in `TransmogrificationPackets.*` here is `TransmogrifyItems` (plus account update packet), with no packet classes for outfit-new/update flows.

## DB2/data-dir angle

A bad DB2 load can absolutely break transmog collections, but in this case:

- Set completion data is visible in UI (suggesting core DB2 set data is present enough to render set metadata).
- The stronger blocker in this branch is missing outfit opcode handling.

You should still validate DB2 startup logs if slot lists remain empty after opcode fixes.

## Suggested next implementation steps

1. Add packet definitions for the outfit CMSG payloads in `TransmogrificationPackets.h/.cpp`.
2. Implement world session handlers for the four outfit CMSGs.
3. Persist/update player transmog outfit state (the player already loads transmog outfits from DB).
4. Emit the expected outfit update SMSGs (`SMSG_TRANSMOG_OUTFIT_*`) after successful edits.
5. Re-test with opcode logging while:
   - logging in
   - opening transmog UI
   - selecting and applying an outfit

## Quick verification checklist

- Confirm opcode routing is no longer `STATUS_UNHANDLED` for the four outfit CMSGs.
- Confirm handler logs fire when clicking outfit actions in UI.
- Confirm DB `character_transmog_outfits` rows mutate after rename/icon/slot/situation changes.
- Confirm client receives corresponding `SMSG_TRANSMOG_OUTFIT_*` updates.
