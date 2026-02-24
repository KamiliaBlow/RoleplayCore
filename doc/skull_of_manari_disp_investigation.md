# Skull of the Man'ari `.disp offhand` investigation notes

## Located `.disp` implementation in this codebase

The command is custom and is implemented under:

- `src/server/scripts/Custom/RolePlayFunction/Display/custom_display_commands.cpp`
- `src/server/scripts/Custom/RolePlayFunction/Display/custom_display_handler.cpp`
- loaded by `src/server/scripts/Custom/custom_script_loader.cpp`

The top-level command registered here is `display` with subcommands including `offhand`.

## Important findings from the rendering/transmog pipeline

### 1) `artifactAppearanceId` loading/apply path is present

Artifact appearance is read from `item_instance_artifact` and applied in item load/create logic:

- `Item::LoadAdditionalDataFromDB(...)` -> `Item::LoadArtifactData(...)`
- `Item::LoadArtifactData(...)` sets:
  - `ITEM_MODIFIER_ARTIFACT_APPEARANCE_ID`
  - `ITEM_MODIFIER_ARTIFACT_TIER`
  - `SetAppearanceModId(artifactAppearance->ItemAppearanceModifierID)`

Artifact appearance is also applied by packet handler:

- `WorldSession::HandleArtifactSetAppearance(...)` in `ArtifactHandler.cpp`
  - validates forge/object and set ownership
  - sets `SetAppearanceModId(...)`
  - sets `ITEM_MODIFIER_ARTIFACT_APPEARANCE_ID`
  - calls `player->SetVisibleItemSlot(...)` to force visual refresh if equipped

### 2) Visual refresh path is present

Visible equipment fields are pushed via:

- `Player::SetVisibleItemSlot(...)`
  - `VisibleItem::ItemAppearanceModID`
  - `VisibleItem::ItemModifiedAppearanceID`
  - `VisibleItem::SecondaryItemModifiedAppearanceID`

The item-side values come from:

- `Item::GetVisibleAppearanceModId(...)`
- `Item::GetVisibleModifiedAppearanceId(...)`
- `Item::GetDisplayId(...)`

### 3) Critical data-width mismatch for ArtifactAppearance modifier IDs

In this source tree:

- `ArtifactAppearanceEntry::ItemAppearanceModifierID` is declared as **`uint8`** in `DB2Structure.h`.
- `ArtifactAppearanceLoadInfo` declares this column as **`FT_BYTE`** in `DB2LoadInfo.h`.

That is incompatible with your observed ArtifactAppearance modifier values in the `52489-52512` range.

If those values are correct for your DB2, they will be truncated when loaded and the wrong modifier will be applied.

### 4) Critical keying limitation in `DB2Manager::GetItemModifiedAppearance`

Item modified appearances are indexed as:

- `_itemModifiedAppearancesByItem[itemId | (appearanceModId << 24)]`

and looked up using the same expression. This effectively assumes:

- `itemId <= 24 bits`
- `appearanceModId <= 8 bits`

Therefore appearance modifier IDs above `255` cannot be represented correctly in this map key scheme and will collide/truncate.

This is consistent with artifacts that require high modifier IDs failing to resolve correctly.

### 5) `.display`/`.disp` handler itself also constrains mod selection

The custom display handler:

- accepts `bonus` as `uint8` in `DisplayHandler::Display(...)`
- uses a static `m_valid_appearance_mod_list` capped to small IDs (0..216 style map)
- does not route through artifact appearance IDs from `ArtifactAppearance.db2`

So `.disp offhand 128943 ...` can only target that curated small modifier set, not high artifact-specific modifier namespaces.

## Most likely root cause (for this repository)

Skull variant changes depending on ArtifactAppearance modifier IDs above 255 will fail because multiple layers in this fork still treat modifier IDs as byte-sized and index them into a packed 24+8 key. That breaks both artifact-driven appearance and `.disp`-driven lookups for this class of item.

## Recommended code fixes to test

1. Update `ArtifactAppearanceEntry::ItemAppearanceModifierID` to a wider type matching current client DB2 (likely `int32/uint32`) and adjust `DB2LoadInfo` field type from `FT_BYTE` to the correct width.
2. Replace `_itemModifiedAppearancesByItem` key packing (`itemId | (appearanceModId << 24)`) with a 64-bit composite key or nested map keyed by full-width `appearanceModId`.
3. Update custom `.display` script:
   - do not narrow bonus/modifier arguments to `uint8`
   - allow direct targeting of actual `ItemAppearanceModifierID` values
   - add artifact-aware path or call existing artifact packet-like flow for artifact weapons.
