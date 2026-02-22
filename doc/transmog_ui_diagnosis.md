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

## What to do next (troubleshooting sequence)

Follow this order to avoid chasing SQL issues while protocol handling is still missing.

### Phase 1: Confirm current behavior with logging

1. Enable verbose network logging for opcodes and transmog handler debug.
2. Reproduce exactly:
   - Login
   - Open transmog UI
   - Attempt: create outfit, rename outfit, change slot appearance, apply set
3. Confirm log shows incoming `CMSG_TRANSMOG_OUTFIT_*` opcodes but no handler-side processing (because they map to `Handle_NULL`).

Expected: requests arrive, but no outfit state transition occurs server-side.

### Phase 2: Implement missing packet/handler flow

1. Add packet definitions for outfit CMSG payloads in `TransmogrificationPackets.h/.cpp`.
2. Add corresponding `WorldSession` handlers.
3. Replace opcode routing in `Opcodes.cpp` from `Handle_NULL` to real handlers.
4. Persist updates to `character_transmog_outfits` and in-memory player equipment set/transmog outfit structures.
5. Emit `SMSG_TRANSMOG_OUTFIT_*` updates after successful server-side mutation.

Expected: UI moves from read-only behavior to editable/applicable outfit behavior.

### Phase 3: Validate account appearance feed (slot list population)

After handler implementation, if slot lists are still empty:

1. Re-check account appearance bitmask load path on login (`LoadAccountItemAppearances`).
2. Verify blocks are actually applied to player with transmog flags (transmog blocks and flags).
3. Validate DB2 startup integrity for item appearance and transmog set sources.
4. Re-test with a fresh account/character to rule out stale account cache edge cases.

Expected: slot lists populate when account appearance state and DB2 data are both healthy.

## Suggested implementation checklist

- [ ] Packet structs for outfit new/update info/update situations/update slots.
- [ ] `WorldSession::HandleTransmogOutfit*` handlers added and registered.
- [ ] Mutation logic for set name/icon/ignore mask/slot appearances/enchants.
- [ ] DB write path for `character_transmog_outfits` updates.
- [ ] `SMSG_TRANSMOG_OUTFIT_*` responses emitted on success.
- [ ] Negative-path logging (invalid set ID, invalid slot, unknown appearance, etc.).

## Quick verification checklist

- Confirm opcode routing is no longer `STATUS_UNHANDLED` for the four outfit CMSGs.
- Confirm handler logs fire when clicking outfit actions in UI.
- Confirm DB `character_transmog_outfits` rows mutate after rename/icon/slot/situation changes.
- Confirm client receives corresponding `SMSG_TRANSMOG_OUTFIT_*` updates.
- Confirm slot appearance lists show data for known collected appearances.
