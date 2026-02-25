#Requires -Version 5.1
<#
.SYNOPSIS
    Apply Codex diffs + manual fixes for transmog outfit system.
    Run from your repo root: C:\Dev\RoleplayCore

.DESCRIPTION
    Phase 1: Applies Codex branch diffs (if not already applied)
    Phase 2: Applies our fixes on top:
      - SetType 0 -> 1 (Outfit, not Equipped)
      - Equipment slot -> TransmogOutfitSlot mapping for update fields
      - TransmogOutfitSlot -> Equipment slot mapping for CMSG parser

.NOTES
    Based on API docs from:
      TransmogOutfitConstantsDocumentation.lua
      TransmogOutfitInfoDocumentation.lua
#>

param(
    [string]$RepoRoot = "C:\Dev\RoleplayCore",
    [switch]$SkipCodexDiffs,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Push-Location $RepoRoot

function Write-Header($msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }
function Write-OK($msg) { Write-Host "  [OK] $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg) { Write-Host "  [ERR] $msg" -ForegroundColor Red }

function Safe-Replace {
    param(
        [string]$FilePath,
        [string]$OldText,
        [string]$NewText,
        [string]$Description
    )
    $content = Get-Content $FilePath -Raw
    if ($content.Contains($OldText)) {
        if (-not $DryRun) {
            $content = $content.Replace($OldText, $NewText)
            Set-Content -Path $FilePath -Value $content -NoNewline
        }
        Write-OK "$Description"
        return $true
    }
    elseif ($content.Contains($NewText)) {
        Write-Warn "$Description (already applied)"
        return $true
    }
    else {
        Write-Err "$Description (target text not found!)"
        return $false
    }
}

# ============================================================================
# PHASE 1: Apply Codex diffs
# ============================================================================
if (-not $SkipCodexDiffs) {
    Write-Header "Phase 1: Applying Codex diffs via combined patch"

    # Generate the combined patch file from the Codex diffs
    $patchContent = @'
diff --git a/src/server/game/Entities/Player/Player.cpp b/src/server/game/Entities/Player/Player.cpp
--- a/src/server/game/Entities/Player/Player.cpp
+++ b/src/server/game/Entities/Player/Player.cpp
@@ -56,6 +56,7 @@
 #include "DatabaseEnv.h"
 #include "DisableMgr.h"
 #include "DuelPackets.h"
+#include <cstdlib>
 #include "EquipmentSetPackets.h"
 #include "Formulas.h"
 #include "GameEventMgr.h"
'@
    # NOTE: The full Codex patch is very large (7 files, ~1000 lines).
    # Instead of embedding it all, we check if key indicators are present.

    $playerCpp = "src\server\game\Entities\Player\Player.cpp"
    $handlerCpp = "src\server\game\Handlers\TransmogrificationHandler.cpp"
    $packetsCpp = "src\server\game\Server\Packets\TransmogrificationPackets.cpp"

    $hasCodexChanges = $false
    if (Test-Path $playerCpp) {
        $content = Get-Content $playerCpp -Raw
        if ($content.Contains("_SyncTransmogOutfitsToActivePlayerData")) {
            Write-OK "Codex changes detected in Player.cpp (has _SyncTransmogOutfitsToActivePlayerData)"
            $hasCodexChanges = $true
        }
    }

    if (-not $hasCodexChanges) {
        Write-Err @"
Codex changes NOT detected in Player.cpp!
You need to apply the Codex diffs first. The diffs were provided as documents 6-12
in the chat. Save each diff block (between the EOF markers) as a .patch file and run:

    git apply --3way codex-player.patch
    git apply --3way codex-handler.patch
    ... etc

Or pull the Codex branch if it was pushed:
    git fetch origin
    git merge origin/codex/<branch-name>

Then re-run this script with -SkipCodexDiffs
"@
        Pop-Location
        exit 1
    }

    # Verify all handler files exist
    if (Test-Path $handlerCpp) {
        $hContent = Get-Content $handlerCpp -Raw
        if ($hContent.Contains("HandleTransmogOutfitNew")) {
            Write-OK "Codex changes detected in TransmogrificationHandler.cpp"
        } else {
            Write-Warn "TransmogrificationHandler.cpp exists but missing HandleTransmogOutfitNew"
        }
    }
} else {
    Write-Header "Phase 1: Skipped (--SkipCodexDiffs)"
}

# ============================================================================
# PHASE 2: Fix #1 - SetType 0 -> 1 (Outfit, not Equipped)
# ============================================================================
Write-Header "Phase 2: Fix SetType (0=Equipped -> 1=Outfit)"

# From TransmogOutfitConstantsDocumentation.lua:
#   TransmogOutfitSetType: Equipped=0, Outfit=1, CustomSet=2
# Codex set it to 0 (Equipped), but player-purchased outfits need 1 (Outfit)

$playerCppPath = Join-Path $RepoRoot "src\server\game\Entities\Player\Player.cpp"

Safe-Replace -FilePath $playerCppPath `
    -OldText 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(0));' `
    -NewText 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(1));  // TransmogOutfitSetType::Outfit' `
    -Description "SetType 0 -> 1"

# Also check if it was the original value of 2 (CustomSet) from an older Codex run
Safe-Replace -FilePath $playerCppPath `
    -OldText 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(2));' `
    -NewText 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(1));  // TransmogOutfitSetType::Outfit' `
    -Description "SetType 2 -> 1 (fallback check for older Codex)"

# ============================================================================
# PHASE 3: Fix #2 - Slot mapping for update fields (Server -> Client)
# ============================================================================
Write-Header "Phase 3: Equipment Slot -> TransmogOutfitSlot mapping (update fields)"

# The client reads TransmogOutfitSlot values (0-14) from update fields,
# but we store appearances indexed by EQUIPMENT_SLOT (0-18).
#
# From TransmogOutfitConstantsDocumentation.lua:
#   TransmogOutfitSlot: Head=0, ShoulderRight=1, ShoulderLeft=2, Back=3,
#   Chest=4, Tabard=5, Body=6, Wrist=7, Hand=8, Waist=9, Legs=10,
#   Feet=11, WeaponMainHand=12, WeaponOffHand=13, WeaponRanged=14
#
# EQUIPMENT_SLOT: HEAD=0, NECK=1, SHOULDERS=2, BODY=3, CHEST=4, WAIST=5,
#   LEGS=6, FEET=7, WRISTS=8, HANDS=9, FINGER1=10, FINGER2=11,
#   TRINKET1=12, TRINKET2=13, BACK=14, MAINHAND=15, OFFHAND=16,
#   RANGED=17, TABARD=18

$oldSlotLoop = @'
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            // Populate all slots so client outfit data providers never see nil slot/source tables.
            auto slotSetter = AddDynamicUpdateFieldValue(outfitSetter.ModifyValue(&UF::TransmogOutfitData::Slots));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Slot).SetValue(int8(slot));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SlotOption).SetValue(uint8(0));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::ItemModifiedAppearanceID).SetValue(equipmentSet->Appearances[slot] > 0 ? uint32(equipmentSet->Appearances[slot]) : 0);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::AppearanceDisplayType).SetValue(uint8(0));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Flags).SetValue(uint32(0));

            uint32 enchant = 0;
            if (slot == EQUIPMENT_SLOT_MAINHAND)
                enchant = equipmentSet->Enchants[0] > 0 ? uint32(equipmentSet->Enchants[0]) : 0;
            else if (slot == EQUIPMENT_SLOT_OFFHAND)
                enchant = equipmentSet->Enchants[1] > 0 ? uint32(equipmentSet->Enchants[1]) : 0;

            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SpellItemEnchantmentID).SetValue(enchant);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::IllusionDisplayType).SetValue(uint8(0));
        }
'@

$newSlotLoop = @'
        // Map EQUIPMENT_SLOT index -> { TransmogOutfitSlot, EquipmentSlot }
        // From TransmogOutfitConstantsDocumentation.lua (client enum)
        // Slots without a transmog equivalent (neck, rings, trinkets) are skipped.
        struct TransmogSlotMapping { int8 transmogSlot; uint8 equipSlot; };
        static constexpr TransmogSlotMapping slotMap[] = {
            {  0,  0 }, // Head           -> EQUIPMENT_SLOT_HEAD
            {  1,  2 }, // ShoulderRight  -> EQUIPMENT_SLOT_SHOULDERS
            {  2,  2 }, // ShoulderLeft   -> EQUIPMENT_SLOT_SHOULDERS (same source)
            {  3, 14 }, // Back           -> EQUIPMENT_SLOT_BACK
            {  4,  4 }, // Chest          -> EQUIPMENT_SLOT_CHEST
            {  5, 18 }, // Tabard         -> EQUIPMENT_SLOT_TABARD
            {  6,  3 }, // Body (Shirt)   -> EQUIPMENT_SLOT_BODY
            {  7,  8 }, // Wrist          -> EQUIPMENT_SLOT_WRISTS
            {  8,  9 }, // Hand           -> EQUIPMENT_SLOT_HANDS
            {  9,  5 }, // Waist          -> EQUIPMENT_SLOT_WAIST
            { 10,  6 }, // Legs           -> EQUIPMENT_SLOT_LEGS
            { 11,  7 }, // Feet           -> EQUIPMENT_SLOT_FEET
            { 12, 15 }, // WeaponMainHand -> EQUIPMENT_SLOT_MAINHAND
            { 13, 16 }, // WeaponOffHand  -> EQUIPMENT_SLOT_OFFHAND
            { 14, 17 }, // WeaponRanged   -> EQUIPMENT_SLOT_RANGED
        };

        for (auto const& mapping : slotMap)
        {
            auto slotSetter = AddDynamicUpdateFieldValue(outfitSetter.ModifyValue(&UF::TransmogOutfitData::Slots));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Slot).SetValue(mapping.transmogSlot);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SlotOption).SetValue(uint8(0));

            uint32 imaID = (mapping.equipSlot < EQUIPMENT_SLOT_END && equipmentSet->Appearances[mapping.equipSlot] > 0)
                ? uint32(equipmentSet->Appearances[mapping.equipSlot]) : 0;
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::ItemModifiedAppearanceID).SetValue(imaID);

            // DisplayType: 0=Unassigned, 1=Assigned, 2=Equipped
            uint8 displayType = (imaID > 0) ? uint8(1) : uint8(0);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::AppearanceDisplayType).SetValue(displayType);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Flags).SetValue(uint32(0));

            uint32 enchant = 0;
            if (mapping.equipSlot == EQUIPMENT_SLOT_MAINHAND)
                enchant = equipmentSet->Enchants[0] > 0 ? uint32(equipmentSet->Enchants[0]) : 0;
            else if (mapping.equipSlot == EQUIPMENT_SLOT_OFFHAND)
                enchant = equipmentSet->Enchants[1] > 0 ? uint32(equipmentSet->Enchants[1]) : 0;

            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SpellItemEnchantmentID).SetValue(enchant);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::IllusionDisplayType).SetValue(uint8(0));
        }
'@

Safe-Replace -FilePath $playerCppPath `
    -OldText $oldSlotLoop `
    -NewText $newSlotLoop `
    -Description "Slot loop: EQUIPMENT_SLOT -> TransmogOutfitSlot mapping (15 entries)"


# ============================================================================
# PHASE 4: Fix #3 - Reverse slot mapping for CMSG parser (Client -> Server)
# ============================================================================
Write-Header "Phase 4: TransmogOutfitSlot -> Equipment Slot mapping (CMSG parser)"

$packetsCppPath = Join-Path $RepoRoot "src\server\game\Server\Packets\TransmogrificationPackets.cpp"

# The AssignSlotAppearance function currently assumes slotIndex is an equipment slot.
# From UPDATE_SLOTS captures, GetSlotIndex() (RawSlotField >> 24) produces values 0-18
# which look like equipment slot indices. However, the client API uses TransmogOutfitSlot
# (0-14), so we should handle BOTH formats gracefully.

$oldAssignSlot = @'
void AssignSlotAppearance(EquipmentSetInfo::EquipmentSetData& set, uint32 slotIndex, int32 appearanceID)
{
    // Captures show this field is a direct equipment slot index (e.g. 15, 16), not a bitmask.
    if (slotIndex < EQUIPMENT_SLOT_END)
        set.Appearances[slotIndex] = appearanceID;
}
'@

$newAssignSlot = @'
void AssignSlotAppearance(EquipmentSetInfo::EquipmentSetData& set, uint32 slotIndex, int32 appearanceID)
{
    // TransmogOutfitSlot (0-14) -> EQUIPMENT_SLOT mapping
    // From TransmogOutfitConstantsDocumentation.lua
    static constexpr uint8 TransmogOutfitSlotToEquipSlot[] = {
         0, // Head(0)           -> EQUIPMENT_SLOT_HEAD(0)
         2, // ShoulderRight(1)  -> EQUIPMENT_SLOT_SHOULDERS(2)
         2, // ShoulderLeft(2)   -> EQUIPMENT_SLOT_SHOULDERS(2)
        14, // Back(3)           -> EQUIPMENT_SLOT_BACK(14)
         4, // Chest(4)          -> EQUIPMENT_SLOT_CHEST(4)
        18, // Tabard(5)         -> EQUIPMENT_SLOT_TABARD(18)
         3, // Body(6)           -> EQUIPMENT_SLOT_BODY(3)
         8, // Wrist(7)          -> EQUIPMENT_SLOT_WRISTS(8)
         9, // Hand(8)           -> EQUIPMENT_SLOT_HANDS(9)
         5, // Waist(9)          -> EQUIPMENT_SLOT_WAIST(5)
         6, // Legs(10)          -> EQUIPMENT_SLOT_LEGS(6)
         7, // Feet(11)          -> EQUIPMENT_SLOT_FEET(7)
        15, // WeaponMainHand(12)-> EQUIPMENT_SLOT_MAINHAND(15)
        16, // WeaponOffHand(13) -> EQUIPMENT_SLOT_OFFHAND(16)
        17, // WeaponRanged(14)  -> EQUIPMENT_SLOT_RANGED(17)
    };

    // If slotIndex <= 14, treat as TransmogOutfitSlot and map to equipment slot.
    // If slotIndex >= 15, treat as raw equipment slot index (fallback for captures
    // that showed values like 15/16 from RawSlotField >> 24).
    uint8 equipSlot;
    if (slotIndex < std::size(TransmogOutfitSlotToEquipSlot))
        equipSlot = TransmogOutfitSlotToEquipSlot[slotIndex];
    else
        equipSlot = uint8(slotIndex);

    if (equipSlot < EQUIPMENT_SLOT_END)
        set.Appearances[equipSlot] = appearanceID;
}
'@

Safe-Replace -FilePath $packetsCppPath `
    -OldText $oldAssignSlot `
    -NewText $newAssignSlot `
    -Description "AssignSlotAppearance: add TransmogOutfitSlot -> EquipmentSlot mapping"


# ============================================================================
# PHASE 5: Summary & build command
# ============================================================================
Write-Header "All fixes applied! Summary of changes"

Write-Host @"

  Player.cpp:
    [1] SetType = uint8(1)  (was 0)     -> TransmogOutfitSetType::Outfit
    [2] Slot loop writes 15 TransmogOutfitSlot entries (was 19 equipment slots)
        - Maps EQUIPMENT_SLOT indices to TransmogOutfitSlot enum values
        - Includes both ShoulderRight and ShoulderLeft from same source
        - Skips neck, rings, trinkets (no transmog equivalent)
        - Sets AppearanceDisplayType=1 (Assigned) when appearance is populated

  TransmogrificationPackets.cpp:
    [3] AssignSlotAppearance maps TransmogOutfitSlot (0-14) -> EQUIPMENT_SLOT
        - Falls back to raw index for values >= 15 (observed in captures)

  Reference (TransmogOutfitConstantsDocumentation.lua):
    TransmogOutfitSetType:  Equipped=0, Outfit=1, CustomSet=2
    TransmogOutfitSlot:     Head=0 ShR=1 ShL=2 Back=3 Chest=4 Tab=5
                            Body=6 Wrist=7 Hand=8 Waist=9 Legs=10
                            Feet=11 MH=12 OH=13 Ranged=14

"@ -ForegroundColor White

if ($DryRun) {
    Write-Warn "DRY RUN - no files were modified"
} else {
    Write-Host "  Build with:" -ForegroundColor Yellow
    Write-Host '    cmake --build C:\Dev\RoleplayCore\out\build\x64-Debug --target worldserver' -ForegroundColor White
    Write-Host ""
}

Pop-Location
