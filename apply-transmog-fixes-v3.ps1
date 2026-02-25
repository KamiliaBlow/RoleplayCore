#Requires -Version 5.1
<#
.SYNOPSIS
    Apply transmog outfit fixes on top of Codex cherry-picks.
    Run from repo root: C:\Dev\RoleplayCore
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

$playerCppPath = Join-Path $RepoRoot "src\server\game\Entities\Player\Player.cpp"
$packetsCppPath = Join-Path $RepoRoot "src\server\game\Server\Packets\TransmogrificationPackets.cpp"

# ============================================================================
# Preflight
# ============================================================================
Write-Header "Preflight check"
$playerContent = [System.IO.File]::ReadAllText($playerCppPath)
if (-not $playerContent.Contains("fillOutfitData")) {
    Write-Err "Codex changes not found. Cherry-pick first!"; Pop-Location; exit 1
}
Write-OK "Codex cherry-picks detected"

# ============================================================================
# FIX 1: SetType 0 -> 1
# ============================================================================
Write-Header "Fix 1: SetType 0 -> 1 (Outfit)"

$old1 = 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(0));'
$new1 = 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(1));  // TransmogOutfitSetType::Outfit'

if ($playerContent.Contains($old1)) {
    if (-not $DryRun) { $playerContent = $playerContent.Replace($old1, $new1) }
    Write-OK "SetType uint8(0) -> uint8(1)"
} elseif ($playerContent.Contains($new1)) {
    Write-Warn "Already applied"
} else {
    Write-Err "Search text not found!"
}

# ============================================================================
# FIX 2: Slot loop remapping
# ============================================================================
Write-Header "Fix 2: Slot loop remapping"

# Exact text from the file (8-space base indent, no blank lines between continues)
$old2 = '        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
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
        }'

$new2 = '        // TransmogOutfitSlot -> EquipmentSlot mapping table.
        // Client expects TransmogOutfitSlot values (0-14) in the Slot field,
        // NOT raw EQUIPMENT_SLOT indices (0-18).
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
        }'

if ($playerContent.Contains($old2)) {
    if (-not $DryRun) { $playerContent = $playerContent.Replace($old2, $new2) }
    Write-OK "Slot loop -> TransmogOutfitSlot mapping (15 entries)"
} elseif ($playerContent.Contains("TransmogSlotMapping")) {
    Write-Warn "Already applied"
} else {
    Write-Err "Slot loop search text not found! Checking for partial match..."
    # Debug: check if first line matches
    $firstLine = '        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)'
    if ($playerContent.Contains($firstLine)) {
        Write-Host "    First line matches. Issue is likely in body whitespace." -ForegroundColor Gray
        Write-Host "    Try running: `$content[17980..18002] | ForEach-Object { `$_.Length }" -ForegroundColor Gray
    } else {
        Write-Host "    First line does NOT match either." -ForegroundColor Gray
    }
}

# ============================================================================
# Write Player.cpp if modified
# ============================================================================
if (-not $DryRun) {
    [System.IO.File]::WriteAllText($playerCppPath, $playerContent)
}

# ============================================================================
# FIX 3: Reverse mapping helper in packets
# ============================================================================
Write-Header "Fix 3: CMSG slot reverse mapping helper"

$packetsContent = [System.IO.File]::ReadAllText($packetsCppPath)

if ($packetsContent.Contains("TransmogOutfitUpdateSlots::Read")) {
    Write-OK "TransmogOutfitUpdateSlots::Read() found"

    $helperFunction = @'
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

'@

    if (-not $packetsContent.Contains("TransmogOutfitSlotToEquipSlot")) {
        if (-not $DryRun) {
            $packetsContent = $packetsContent.Replace(
                "void TransmogOutfitUpdateSlots::Read()",
                $helperFunction + "void TransmogOutfitUpdateSlots::Read()")
            [System.IO.File]::WriteAllText($packetsCppPath, $packetsContent)
        }
        Write-OK "Added TransmogOutfitSlotToEquipSlot() helper"
    } else {
        Write-Warn "Already present"
    }
} else {
    Write-Warn "TransmogOutfitUpdateSlots::Read() not found"
}

# ============================================================================
# Summary
# ============================================================================
Write-Header "Summary"
if ($DryRun) { Write-Warn "DRY RUN - no files modified" }
else {
    Write-Host "  All fixes written. Build with:" -ForegroundColor Green
    Write-Host '  cmake --build C:\Dev\RoleplayCore\out\build\x64-Debug --target worldserver' -ForegroundColor White
}

Pop-Location
