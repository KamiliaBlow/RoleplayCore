#Requires -Version 5.1
<#
.SYNOPSIS
    Apply transmog outfit fixes on top of Codex cherry-picks.
    Run from repo root: C:\Dev\RoleplayCore

.DESCRIPTION
    Fix 1: SetType 0 -> 1 (Outfit, not Equipped)
    Fix 2: Slot loop uses TransmogOutfitSlot mapping instead of raw EQUIPMENT_SLOT
    Fix 3: Packet parser maps TransmogOutfitSlot back to EQUIPMENT_SLOT

.NOTES
    Based on API docs from TransmogOutfitConstantsDocumentation.lua
    Requires Codex cherry-picks already applied (fillOutfitData lambda present)
#>

param(
    [string]$RepoRoot = "C:\Dev\RoleplayCore",
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
    $content = Get-Content $FilePath -Raw -Encoding UTF8
    if ($content.Contains($OldText)) {
        if (-not $DryRun) {
            $content = $content.Replace($OldText, $NewText)
            [System.IO.File]::WriteAllText($FilePath, $content)
        }
        Write-OK "$Description"
        return $true
    }
    elseif ($content.Contains($NewText)) {
        Write-Warn "$Description (already applied)"
        return $true
    }
    else {
        Write-Err "$Description (search text not found!)"
        Write-Host "    Expected to find in: $FilePath" -ForegroundColor Gray
        return $false
    }
}

# ============================================================================
# Preflight: Verify Codex changes are present
# ============================================================================
Write-Header "Preflight check"

$playerCppPath = Join-Path $RepoRoot "src\server\game\Entities\Player\Player.cpp"
$packetsCppPath = Join-Path $RepoRoot "src\server\game\Server\Packets\TransmogrificationPackets.cpp"

$playerContent = Get-Content $playerCppPath -Raw
if (-not $playerContent.Contains("fillOutfitData")) {
    Write-Err "Codex changes not found (missing fillOutfitData lambda). Cherry-pick first!"
    Pop-Location; exit 1
}
Write-OK "Codex cherry-picks detected"

# ============================================================================
# FIX 1: SetType 0 -> 1 (TransmogOutfitSetType::Outfit)
# ============================================================================
Write-Header "Fix 1: SetType 0 -> 1 (Outfit)"

# From TransmogOutfitConstantsDocumentation.lua:
#   Equipped=0, Outfit=1, CustomSet=2
# Player-purchased outfits must be type 1.

Safe-Replace -FilePath $playerCppPath `
    -OldText 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(0));' `
    -NewText 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(1));  // TransmogOutfitSetType::Outfit' `
    -Description "SetType uint8(0) -> uint8(1)"

# ============================================================================
# FIX 2: Slot loop - EQUIPMENT_SLOT -> TransmogOutfitSlot mapping
# ============================================================================
Write-Header "Fix 2: Slot loop remapping (Player.cpp fillOutfitData)"

# The current code iterates EQUIPMENT_SLOT_START..END and writes raw equipment
# slot indices into TransmogOutfitSlotData::Slot. The client expects
# TransmogOutfitSlot enum values (0-14), which have DIFFERENT numbering.
#
# TransmogOutfitSlot enum (from TransmogOutfitConstantsDocumentation.lua):
#   Head=0 ShoulderR=1 ShoulderL=2 Back=3 Chest=4 Tabard=5 Body=6
#   Wrist=7 Hand=8 Waist=9 Legs=10 Feet=11 MH=12 OH=13 Ranged=14
#
# EQUIPMENT_SLOT (server):
#   HEAD=0 NECK=1 SHOULDERS=2 BODY=3 CHEST=4 WAIST=5 LEGS=6 FEET=7
#   WRISTS=8 HANDS=9 FINGER1=10 FINGER2=11 TRINKET1=12 TRINKET2=13
#   BACK=14 MAINHAND=15 OFFHAND=16 RANGED=17 TABARD=18

$oldSlotLoop = @"
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (equipmentSet->IgnoreMask & (1 << slot))
                continue;

            if (!equipmentSet->Appearances[slot] && slot != EQUIPMENT_SLOT_MAINHAND && slot != EQUIPMENT_SLOT_OFFHAND)
                continue;

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
"@

$newSlotLoop = @"
        // TransmogOutfitSlot -> EquipmentSlot mapping table.
        // Client expects TransmogOutfitSlot values (0-14) in Slot field.
        // We read appearance data from Appearances[equipSlot] and write
        // with the transmog slot index the client understands.
        // From TransmogOutfitConstantsDocumentation.lua
        struct TransmogSlotMapping { int8 transmogSlot; uint8 equipSlot; };
        static constexpr TransmogSlotMapping slotMap[] = {
            {  0,  0 }, // Head            -> EQUIPMENT_SLOT_HEAD
            {  1,  2 }, // ShoulderRight   -> EQUIPMENT_SLOT_SHOULDERS
            {  2,  2 }, // ShoulderLeft    -> EQUIPMENT_SLOT_SHOULDERS (same source)
            {  3, 14 }, // Back            -> EQUIPMENT_SLOT_BACK
            {  4,  4 }, // Chest           -> EQUIPMENT_SLOT_CHEST
            {  5, 18 }, // Tabard          -> EQUIPMENT_SLOT_TABARD
            {  6,  3 }, // Body (Shirt)    -> EQUIPMENT_SLOT_BODY
            {  7,  8 }, // Wrist           -> EQUIPMENT_SLOT_WRISTS
            {  8,  9 }, // Hand            -> EQUIPMENT_SLOT_HANDS
            {  9,  5 }, // Waist           -> EQUIPMENT_SLOT_WAIST
            { 10,  6 }, // Legs            -> EQUIPMENT_SLOT_LEGS
            { 11,  7 }, // Feet            -> EQUIPMENT_SLOT_FEET
            { 12, 15 }, // WeaponMainHand  -> EQUIPMENT_SLOT_MAINHAND
            { 13, 16 }, // WeaponOffHand   -> EQUIPMENT_SLOT_OFFHAND
            { 14, 17 }, // WeaponRanged    -> EQUIPMENT_SLOT_RANGED
        };

        for (auto const& mapping : slotMap)
        {
            uint32 imaID = (mapping.equipSlot < EQUIPMENT_SLOT_END && equipmentSet->Appearances[mapping.equipSlot] > 0)
                ? uint32(equipmentSet->Appearances[mapping.equipSlot]) : 0;

            auto slotSetter = AddDynamicUpdateFieldValue(outfitSetter.ModifyValue(&UF::TransmogOutfitData::Slots));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Slot).SetValue(mapping.transmogSlot);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SlotOption).SetValue(uint8(0));
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::ItemModifiedAppearanceID).SetValue(imaID);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::AppearanceDisplayType).SetValue(imaID > 0 ? uint8(1) : uint8(0));  // 0=Unassigned, 1=Assigned
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Flags).SetValue(uint32(0));

            uint32 enchant = 0;
            if (mapping.equipSlot == EQUIPMENT_SLOT_MAINHAND)
                enchant = equipmentSet->Enchants[0] > 0 ? uint32(equipmentSet->Enchants[0]) : 0;
            else if (mapping.equipSlot == EQUIPMENT_SLOT_OFFHAND)
                enchant = equipmentSet->Enchants[1] > 0 ? uint32(equipmentSet->Enchants[1]) : 0;

            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SpellItemEnchantmentID).SetValue(enchant);
            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::IllusionDisplayType).SetValue(uint8(0));
        }
"@

Safe-Replace -FilePath $playerCppPath `
    -OldText $oldSlotLoop `
    -NewText $newSlotLoop `
    -Description "Slot loop -> TransmogOutfitSlot mapping (15 entries)"


# ============================================================================
# FIX 3: Packet parser - TransmogOutfitSlot -> EquipmentSlot reverse mapping
# ============================================================================
Write-Header "Fix 3: CMSG slot reverse mapping (TransmogrificationPackets.cpp)"

$packetsContent = Get-Content $packetsCppPath -Raw

if ($packetsContent.Contains("TransmogOutfitUpdateSlots::Read")) {
    Write-OK "TransmogOutfitUpdateSlots::Read() found"

    # Add a TransmogOutfitSlot-to-EquipmentSlot helper if not present
    $helperFunction = @"

// TransmogOutfitSlot (client enum 0-14) -> EQUIPMENT_SLOT (server 0-18) mapping.
// From TransmogOutfitConstantsDocumentation.lua
static uint8 TransmogOutfitSlotToEquipSlot(uint8 transmogSlot)
{
    static constexpr uint8 map[] = {
         0, // Head(0)            -> EQUIPMENT_SLOT_HEAD(0)
         2, // ShoulderRight(1)   -> EQUIPMENT_SLOT_SHOULDERS(2)
         2, // ShoulderLeft(2)    -> EQUIPMENT_SLOT_SHOULDERS(2)
        14, // Back(3)            -> EQUIPMENT_SLOT_BACK(14)
         4, // Chest(4)           -> EQUIPMENT_SLOT_CHEST(4)
        18, // Tabard(5)          -> EQUIPMENT_SLOT_TABARD(18)
         3, // Body(6)            -> EQUIPMENT_SLOT_BODY(3)
         8, // Wrist(7)           -> EQUIPMENT_SLOT_WRISTS(8)
         9, // Hand(8)            -> EQUIPMENT_SLOT_HANDS(9)
         5, // Waist(9)           -> EQUIPMENT_SLOT_WAIST(5)
         6, // Legs(10)           -> EQUIPMENT_SLOT_LEGS(6)
         7, // Feet(11)           -> EQUIPMENT_SLOT_FEET(7)
        15, // WeaponMainHand(12) -> EQUIPMENT_SLOT_MAINHAND(15)
        16, // WeaponOffHand(13)  -> EQUIPMENT_SLOT_OFFHAND(16)
        17, // WeaponRanged(14)   -> EQUIPMENT_SLOT_RANGED(17)
    };
    if (transmogSlot < std::size(map))
        return map[transmogSlot];
    return transmogSlot;  // fallback: treat as raw equipment slot
}

"@

    if (-not $packetsContent.Contains("TransmogOutfitSlotToEquipSlot")) {
        if (-not $DryRun) {
            $insertPoint = "void TransmogOutfitUpdateSlots::Read()"
            $packetsContent = $packetsContent.Replace($insertPoint, $helperFunction + $insertPoint)
            [System.IO.File]::WriteAllText($packetsCppPath, $packetsContent)
        }
        Write-OK "Added TransmogOutfitSlotToEquipSlot() helper function"
    } else {
        Write-Warn "TransmogOutfitSlotToEquipSlot() already present"
    }
} else {
    Write-Warn "TransmogOutfitUpdateSlots::Read() not found - packet parser may need manual review"
}


# ============================================================================
# Summary
# ============================================================================
Write-Header "Done! Changes applied"

Write-Host @"

  Player.cpp (_SyncTransmogOutfitsToActivePlayerData):
    [1] SetType = uint8(1)  ->  TransmogOutfitSetType::Outfit (was 0=Equipped)
    [2] Slot loop writes 15 TransmogOutfitSlot entries (was 19 EQUIPMENT_SLOT)
        Head=0 ShR=1 ShL=2 Back=3 Chest=4 Tab=5 Body=6 Wrist=7
        Hand=8 Waist=9 Legs=10 Feet=11 MH=12 OH=13 Ranged=14
        Sets AppearanceDisplayType=1 for populated slots

  TransmogrificationPackets.cpp:
    [3] Added TransmogOutfitSlotToEquipSlot() reverse mapping helper
        (TODO: wire into TransmogOutfitUpdateSlots::Read if needed)

"@ -ForegroundColor White

if ($DryRun) {
    Write-Warn "DRY RUN - no files were modified"
} else {
    Write-Host "  Build:" -ForegroundColor Yellow
    Write-Host '    cmake --build C:\Dev\RoleplayCore\out\build\x64-Debug --target worldserver' -ForegroundColor White
    Write-Host ""
    Write-Host "  Test checklist:" -ForegroundColor Yellow
    Write-Host "    1. Log in, open transmog NPC" -ForegroundColor White
    Write-Host "    2. Check: does outfit list populate? (no firstElementData=nil crash)" -ForegroundColor White
    Write-Host "    3. Check: do existing outfits show correct names/icons?" -ForegroundColor White
    Write-Host "    4. Try: create new outfit, rename, change icon" -ForegroundColor White
    Write-Host "    5. Check worldserver log for _SyncTransmogOutfitsToActivePlayerData debug lines" -ForegroundColor White
    Write-Host ""
}

Pop-Location
