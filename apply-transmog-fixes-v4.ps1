#Requires -Version 5.1
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

# Read raw bytes and normalize to LF for matching, then restore original endings on write
$playerRaw = [System.IO.File]::ReadAllText($playerCppPath)
$hasCRLF = $playerRaw.Contains("`r`n")
$playerContent = $playerRaw.Replace("`r`n", "`n")

# ============================================================================
Write-Header "Preflight check"
if (-not $playerContent.Contains("fillOutfitData")) {
    Write-Err "Codex changes not found!"; Pop-Location; exit 1
}
Write-OK "Codex cherry-picks detected"
Write-Host "  Line endings: $(if($hasCRLF){'CRLF'}else{'LF'})" -ForegroundColor Gray

# ============================================================================
Write-Header "Fix 1: SetType 0 -> 1 (Outfit)"

$old1 = 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(0));'
$new1 = 'SetUpdateFieldValue(outfitInfoSetter.ModifyValue(&UF::TransmogOutfitDataInfo::SetType), uint8(1));  // TransmogOutfitSetType::Outfit'

if ($playerContent.Contains($old1)) {
    $playerContent = $playerContent.Replace($old1, $new1)
    Write-OK "SetType uint8(0) -> uint8(1)"
} elseif ($playerContent.Contains("uint8(1));  // TransmogOutfitSetType")) {
    Write-Warn "Already applied"
} else { Write-Err "Search text not found!" }

# ============================================================================
Write-Header "Fix 2: Slot loop remapping"

# Build search/replace with explicit LF line endings
$LF = "`n"

$old2 = "        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)${LF}" +
"        {${LF}" +
"            if (equipmentSet->IgnoreMask & (1 << slot))${LF}" +
"                continue;${LF}" +
"${LF}" +
"            if (!equipmentSet->Appearances[slot] && slot != EQUIPMENT_SLOT_MAINHAND && slot != EQUIPMENT_SLOT_OFFHAND)${LF}" +
"                continue;${LF}" +
"${LF}" +
"            auto slotSetter = AddDynamicUpdateFieldValue(outfitSetter.ModifyValue(&UF::TransmogOutfitData::Slots));${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Slot).SetValue(int8(slot));${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SlotOption).SetValue(uint8(0));${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::ItemModifiedAppearanceID).SetValue(equipmentSet->Appearances[slot] > 0 ? uint32(equipmentSet->Appearances[slot]) : 0);${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::AppearanceDisplayType).SetValue(uint8(0));${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Flags).SetValue(uint32(0));${LF}" +
"${LF}" +
"            uint32 enchant = 0;${LF}" +
"            if (slot == EQUIPMENT_SLOT_MAINHAND)${LF}" +
"                enchant = equipmentSet->Enchants[0] > 0 ? uint32(equipmentSet->Enchants[0]) : 0;${LF}" +
"            else if (slot == EQUIPMENT_SLOT_OFFHAND)${LF}" +
"                enchant = equipmentSet->Enchants[1] > 0 ? uint32(equipmentSet->Enchants[1]) : 0;${LF}" +
"${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SpellItemEnchantmentID).SetValue(enchant);${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::IllusionDisplayType).SetValue(uint8(0));${LF}" +
"        }"

$new2 = "        // TransmogOutfitSlot -> EquipmentSlot mapping table.${LF}" +
"        // Client expects TransmogOutfitSlot values (0-14) in the Slot field,${LF}" +
"        // NOT raw EQUIPMENT_SLOT indices (0-18).${LF}" +
"        // From TransmogOutfitConstantsDocumentation.lua${LF}" +
"        struct TransmogSlotMapping { int8 transmogSlot; uint8 equipSlot; };${LF}" +
"        static constexpr TransmogSlotMapping slotMap[] = {${LF}" +
"            {  0,  0 }, // Head            -> EQUIPMENT_SLOT_HEAD${LF}" +
"            {  1,  2 }, // ShoulderRight   -> EQUIPMENT_SLOT_SHOULDERS${LF}" +
"            {  2,  2 }, // ShoulderLeft    -> EQUIPMENT_SLOT_SHOULDERS (same source)${LF}" +
"            {  3, 14 }, // Back            -> EQUIPMENT_SLOT_BACK${LF}" +
"            {  4,  4 }, // Chest           -> EQUIPMENT_SLOT_CHEST${LF}" +
"            {  5, 18 }, // Tabard          -> EQUIPMENT_SLOT_TABARD${LF}" +
"            {  6,  3 }, // Body (Shirt)    -> EQUIPMENT_SLOT_BODY${LF}" +
"            {  7,  8 }, // Wrist           -> EQUIPMENT_SLOT_WRISTS${LF}" +
"            {  8,  9 }, // Hand            -> EQUIPMENT_SLOT_HANDS${LF}" +
"            {  9,  5 }, // Waist           -> EQUIPMENT_SLOT_WAIST${LF}" +
"            { 10,  6 }, // Legs            -> EQUIPMENT_SLOT_LEGS${LF}" +
"            { 11,  7 }, // Feet            -> EQUIPMENT_SLOT_FEET${LF}" +
"            { 12, 15 }, // WeaponMainHand  -> EQUIPMENT_SLOT_MAINHAND${LF}" +
"            { 13, 16 }, // WeaponOffHand   -> EQUIPMENT_SLOT_OFFHAND${LF}" +
"            { 14, 17 }, // WeaponRanged    -> EQUIPMENT_SLOT_RANGED${LF}" +
"        };${LF}" +
"${LF}" +
"        for (auto const& mapping : slotMap)${LF}" +
"        {${LF}" +
"            uint32 imaID = (mapping.equipSlot < EQUIPMENT_SLOT_END && equipmentSet->Appearances[mapping.equipSlot] > 0)${LF}" +
"                ? uint32(equipmentSet->Appearances[mapping.equipSlot]) : 0;${LF}" +
"${LF}" +
"            auto slotSetter = AddDynamicUpdateFieldValue(outfitSetter.ModifyValue(&UF::TransmogOutfitData::Slots));${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Slot).SetValue(mapping.transmogSlot);${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SlotOption).SetValue(uint8(0));${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::ItemModifiedAppearanceID).SetValue(imaID);${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::AppearanceDisplayType).SetValue(imaID > 0 ? uint8(1) : uint8(0));  // 0=Unassigned, 1=Assigned${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::Flags).SetValue(uint32(0));${LF}" +
"${LF}" +
"            uint32 enchant = 0;${LF}" +
"            if (mapping.equipSlot == EQUIPMENT_SLOT_MAINHAND)${LF}" +
"                enchant = equipmentSet->Enchants[0] > 0 ? uint32(equipmentSet->Enchants[0]) : 0;${LF}" +
"            else if (mapping.equipSlot == EQUIPMENT_SLOT_OFFHAND)${LF}" +
"                enchant = equipmentSet->Enchants[1] > 0 ? uint32(equipmentSet->Enchants[1]) : 0;${LF}" +
"${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::SpellItemEnchantmentID).SetValue(enchant);${LF}" +
"            slotSetter.ModifyValue(&UF::TransmogOutfitSlotData::IllusionDisplayType).SetValue(uint8(0));${LF}" +
"        }"

if ($playerContent.Contains($old2)) {
    $playerContent = $playerContent.Replace($old2, $new2)
    Write-OK "Slot loop -> TransmogOutfitSlot mapping (15 entries)"
} elseif ($playerContent.Contains("TransmogSlotMapping")) {
    Write-Warn "Already applied"
} else {
    Write-Err "Slot loop text not found!"
    # Debug: try to find what's different
    $lines = $old2.Split("`n")
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if (-not $playerContent.Contains($lines[$i])) {
            Write-Host "    Line $i NOT found: |$($lines[$i])|" -ForegroundColor Gray
            break
        }
    }
}

# ============================================================================
# Write Player.cpp
# ============================================================================
if (-not $DryRun) {
    if ($hasCRLF) { $playerContent = $playerContent.Replace("`n", "`r`n") }
    [System.IO.File]::WriteAllText($playerCppPath, $playerContent)
    Write-OK "Player.cpp written"
}

# ============================================================================
Write-Header "Fix 3: CMSG slot reverse mapping helper"

$packetsRaw = [System.IO.File]::ReadAllText($packetsCppPath)
$packetsContent = $packetsRaw.Replace("`r`n", "`n")

if ($packetsContent.Contains("TransmogOutfitUpdateSlots::Read")) {
    Write-OK "TransmogOutfitUpdateSlots::Read() found"

    if (-not $packetsContent.Contains("TransmogOutfitSlotToEquipSlot")) {
        $helper = "// TransmogOutfitSlot (client enum 0-14) -> EQUIPMENT_SLOT (server 0-18) mapping.${LF}" +
"// From TransmogOutfitConstantsDocumentation.lua${LF}" +
"static uint8 TransmogOutfitSlotToEquipSlot(uint8 transmogSlot)${LF}" +
"{${LF}" +
"    static constexpr uint8 map[] = {${LF}" +
"         0, // Head(0)            -> EQUIPMENT_SLOT_HEAD(0)${LF}" +
"         2, // ShoulderRight(1)   -> EQUIPMENT_SLOT_SHOULDERS(2)${LF}" +
"         2, // ShoulderLeft(2)    -> EQUIPMENT_SLOT_SHOULDERS(2)${LF}" +
"        14, // Back(3)            -> EQUIPMENT_SLOT_BACK(14)${LF}" +
"         4, // Chest(4)           -> EQUIPMENT_SLOT_CHEST(4)${LF}" +
"        18, // Tabard(5)          -> EQUIPMENT_SLOT_TABARD(18)${LF}" +
"         3, // Body(6)            -> EQUIPMENT_SLOT_BODY(3)${LF}" +
"         8, // Wrist(7)           -> EQUIPMENT_SLOT_WRISTS(8)${LF}" +
"         9, // Hand(8)            -> EQUIPMENT_SLOT_HANDS(9)${LF}" +
"         5, // Waist(9)           -> EQUIPMENT_SLOT_WAIST(5)${LF}" +
"         6, // Legs(10)           -> EQUIPMENT_SLOT_LEGS(6)${LF}" +
"         7, // Feet(11)           -> EQUIPMENT_SLOT_FEET(7)${LF}" +
"        15, // WeaponMainHand(12) -> EQUIPMENT_SLOT_MAINHAND(15)${LF}" +
"        16, // WeaponOffHand(13)  -> EQUIPMENT_SLOT_OFFHAND(16)${LF}" +
"        17, // WeaponRanged(14)   -> EQUIPMENT_SLOT_RANGED(17)${LF}" +
"    };${LF}" +
"    if (transmogSlot < std::size(map))${LF}" +
"        return map[transmogSlot];${LF}" +
"    return transmogSlot;${LF}" +
"}${LF}" +
"${LF}"

        $packetsContent = $packetsContent.Replace(
            "void TransmogOutfitUpdateSlots::Read()",
            $helper + "void TransmogOutfitUpdateSlots::Read()")

        if (-not $DryRun) {
            $packetsHasCRLF = $packetsRaw.Contains("`r`n")
            if ($packetsHasCRLF) { $packetsContent = $packetsContent.Replace("`n", "`r`n") }
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
Write-Header "Summary"
if ($DryRun) { Write-Warn "DRY RUN - no files modified" }
else {
    Write-Host "  All fixes written. Build with:" -ForegroundColor Green
    Write-Host '  cmake --build C:\Dev\RoleplayCore\out\build\x64-Debug --target worldserver' -ForegroundColor White
}
Pop-Location
