-- TransmogBridge: Sends all outfit slot appearances and illusions to the server via addon message.
-- The 12.x client's CommitAndApplyAllPending() C++ serializer omits HEAD, MH, OH,
-- and sends stale data for all other slots. This addon uses a hybrid approach:
-- snapshot via GetViewedOutfitSlotInfo (captures outfit-loaded armor slots)
-- merged with SetPendingTransmog hooks (captures weapons, illusions, secondary shoulder,
-- tabard, shirt — slots where the snapshot is unreliable). Hook data wins on conflict.

local ADDON_PREFIX = "TMOG_BRIDGE"
local LOG_PREFIX  = "TMOG_LOG"
local pendingOverrides = {}
local pendingIllusions = {} -- slot -> SpellItemEnchantmentID (weapon enchant visuals)

-- Inventory slot names for TransmogUtil.GetTransmogLocation (Layer 3 fallback)
local INV_SLOT_NAMES = {
    [0]  = "HEADSLOT",
    [1]  = "SHOULDERSLOT",
    [3]  = "BACKSLOT",
    [4]  = "CHESTSLOT",
    [5]  = "TABARDSLOT",
    [6]  = "SHIRTSLOT",
    [7]  = "WRISTSLOT",
    [8]  = "HANDSSLOT",
    [9]  = "WAISTSLOT",
    [10] = "LEGSSLOT",
    [11] = "FEETSLOT",
    [12] = "MAINHANDSLOT",
    [13] = "SECONDARYHANDSLOT",
}

-- Slots where the client serializer always returns nil (broken, not hidden)
local ALWAYS_NIL_SLOTS = { [0]=true, [2]=true, [12]=true, [13]=true }

-- Check API availability once at load
local HAS_SLOT_VISUAL_INFO = C_Transmog and C_Transmog.GetSlotVisualInfo ~= nil
    and TransmogUtil and TransmogUtil.GetTransmogLocation ~= nil
    and Enum and Enum.TransmogType ~= nil

C_ChatInfo.RegisterAddonMessagePrefix(ADDON_PREFIX)
C_ChatInfo.RegisterAddonMessagePrefix(LOG_PREFIX)

-- Send log entries to the server via addon message → shows up in Debug.log
local function Log(msg)
    local entry = date("%H:%M:%S") .. " " .. msg
    -- Truncate to 255 byte addon message limit
    if #entry > 255 then entry = entry:sub(1, 255) end
    C_ChatInfo.SendAddonMessage(LOG_PREFIX, entry, "WHISPER", UnitName("player"))
end

-- Capture every SetPendingTransmog call.
-- Client slot indices (confirmed via TransmogSpy):
--   0=HEAD, 1=SHOULDER, 2=SECONDARY_SHOULDER, 3=BACK, 4=CHEST,
--   5=TABARD, 6=SHIRT, 7=WRIST, 8=HANDS, 9=WAIST, 10=LEGS,
--   11=FEET, 12=MAINHAND, 13=OFFHAND
-- tmogType: 0=appearance, 1=illusion (weapon enchant visual)
-- transmogID: IMAID (type 0) or SpellItemEnchantmentID (type 1)
hooksecurefunc(C_TransmogOutfitInfo, "SetPendingTransmog", function(slot, tmogType, option, transmogID, displayType)
    if tmogType == 1 then
        -- Illusion (weapon enchant visual): slot 12=MH, 13=OH
        pendingIllusions[slot] = transmogID or 0
        Log(string.format("SetPending illusion slot=%d enchantID=%d", slot, transmogID or 0))
        return
    end
    if tmogType ~= 0 then
        Log(string.format("SetPending SKIP type=%d slot=%d id=%d", tmogType, slot, transmogID or 0))
        return
    end
    pendingOverrides[slot] = pendingOverrides[slot] or {}
    pendingOverrides[slot].transmogID = transmogID
    pendingOverrides[slot].option = option
    Log(string.format("SetPending slot=%d IMAID=%d opt=%d", slot, transmogID or 0, option or 0))
end)

-- Clear on window close (also handles cancel — ClearAllPending doesn't exist in 12.x)
local f = CreateFrame("Frame")
f:RegisterEvent("TRANSMOGRIFY_CLOSE")
f:SetScript("OnEvent", function(self, event)
    if event == "TRANSMOGRIFY_CLOSE" then
        local count = 0
        for _ in pairs(pendingOverrides) do count = count + 1 end
        if count > 0 then
            Log(string.format("TRANSMOGRIFY_CLOSE — cleared %d pending overrides", count))
        end
        wipe(pendingOverrides)
        wipe(pendingIllusions)
    end
end)

-- Send overrides on apply (post-hook: fires after CommitAndApplyAllPending queues the CMSG)
hooksecurefunc(C_TransmogOutfitInfo, "CommitAndApplyAllPending", function(useDiscount)
    -- Hybrid merge: snapshot all slots via GetViewedOutfitSlotInfo (base layer),
    -- then overlay SetPendingTransmog accumulations on top (wins on conflict).
    -- GetViewedOutfitSlotInfo is unreliable for weapons (12, 13), secondary
    -- shoulder (2), tabard (5), and shirt (6), but SetPendingTransmog captures
    -- those correctly when the user clicks slots manually. Outfit set loading
    -- bypasses SetPendingTransmog but populates the viewed state for armor slots.
    local merged = {} -- slot -> {transmogID, option}

    -- Pre-snapshot: capture current applied appearances BEFORE the merge.
    -- GetSlotVisualInfo.appliedSourceID = what's on the equipped item right now.
    -- After merge, if a slot's IMAID matches the pre-snapshot, it's stale data
    -- from Layer 1 (echoing current appearance, not from the applied outfit set).
    local preSnapshot = {}
    if HAS_SLOT_VISUAL_INFO then
        for slot = 0, 13 do
            if slot ~= 2 then
                local invName = INV_SLOT_NAMES[slot]
                if invName then
                    local ok, loc = pcall(TransmogUtil.GetTransmogLocation, invName, Enum.TransmogType.Appearance, false)
                    if ok and loc then
                        local ok2, visInfo = pcall(C_Transmog.GetSlotVisualInfo, loc)
                        if ok2 and visInfo and visInfo.appliedSourceID and visInfo.appliedSourceID > 0 then
                            preSnapshot[slot] = visInfo.appliedSourceID
                        end
                    end
                end
            end
        end
    end

    -- Layer 1: snapshot from GetViewedOutfitSlotInfo (base)
    for slot = 0, 13 do
        if slot ~= 2 then -- secondary shoulder queried separately below
            local info = C_TransmogOutfitInfo.GetViewedOutfitSlotInfo(slot, 0, 0)
            if info and info.transmogID and info.transmogID > 0 then
                merged[slot] = { transmogID = info.transmogID, option = 0 }
            end
        end
    end

    -- Secondary shoulder: slot 1 with option=1
    local info2 = C_TransmogOutfitInfo.GetViewedOutfitSlotInfo(1, 0, 1)
    if info2 and info2.transmogID and info2.transmogID > 0 then
        merged[2] = { transmogID = info2.transmogID, option = 0 }
    end

    -- Layer 2: SetPendingTransmog accumulations override snapshot (wins on conflict)
    -- transmogID=0 from SetPendingTransmog means user explicitly cleared this slot
    local layer2Slots = {} -- slots explicitly set by user (exempt from stale detection)
    for slot, data in pairs(pendingOverrides) do
        local tmogID = data.transmogID or 0
        layer2Slots[slot] = true
        if tmogID > 0 then
            merged[slot] = { transmogID = tmogID, option = data.option or 0 }
        else
            -- Explicit clear: user removed transmog from this slot via UI
            merged[slot] = { transmogID = 0, option = 0 }
            Log(string.format("Layer2 explicit clear slot=%d", slot))
        end
    end

    -- Layer 3: C_Transmog.GetSlotVisualInfo fallback for slots still missing.
    -- GetViewedOutfitSlotInfo returns nil for tabard (5), shirt (6), weapons (12, 13),
    -- and secondary shoulder during outfit loading. If SetPendingTransmog didn't fire
    -- either (outfit load, not manual click), try the old per-slot transmog API.
    if HAS_SLOT_VISUAL_INFO then
        for slot = 0, 13 do
            if not merged[slot] and slot ~= 2 then -- skip secondary shoulder (no inv slot)
                local invName = INV_SLOT_NAMES[slot]
                if invName then
                    local ok, loc = pcall(TransmogUtil.GetTransmogLocation, invName, Enum.TransmogType.Appearance, false)
                    if ok and loc then
                        local ok2, visInfo = pcall(C_Transmog.GetSlotVisualInfo, loc)
                        if ok2 and visInfo then
                            local id = visInfo.pendingSourceID
                            if (not id or id == 0) then id = visInfo.appliedSourceID end
                            if id and id > 0 then
                                merged[slot] = { transmogID = id, option = 0 }
                                Log(string.format("Layer3 fallback slot=%d IMAID=%d (visual)", slot, id))
                            end
                        end
                    end
                end
            end
        end
    end

    -- Illusion overlay: apply SetPendingTransmog illusion data to weapon slots (12=MH, 13=OH)
    for slot, illusionID in pairs(pendingIllusions) do
        if not merged[slot] then
            -- Get current weapon appearance (illusions require a transmogged weapon)
            local invName = INV_SLOT_NAMES[slot]
            if invName and HAS_SLOT_VISUAL_INFO then
                local ok, loc = pcall(TransmogUtil.GetTransmogLocation, invName, Enum.TransmogType.Appearance, false)
                if ok and loc then
                    local ok2, visInfo = pcall(C_Transmog.GetSlotVisualInfo, loc)
                    if ok2 and visInfo then
                        local id = visInfo.appliedSourceID
                        if id and id > 0 then
                            merged[slot] = { transmogID = id, option = 0 }
                            Log(string.format("Illusion: looked up current appearance slot=%d IMAID=%d", slot, id))
                        end
                    end
                end
            end
        end
        if merged[slot] then
            merged[slot].illusionID = illusionID
            Log(string.format("Illusion overlay slot=%d enchantID=%d", slot, illusionID))
        end
    end

    -- Detect hidden appearances: slots nil across all 3 layers.
    -- HEAD (0), SECONDARY_SHOULDER (2), MH (12), OH (13) are ALWAYS nil
    -- due to client serializer bugs — defer these to server baseline.
    -- EXCEPTION: if Layer 2 (SetPendingTransmog) captured data for an always-nil slot,
    -- the user explicitly clicked it — send it regardless of always-nil status.
    -- (Layer 2 processing above should have set merged[slot] already, but this is a safety
    -- net for the case where that didn't happen. Also handles pendingOverrides with transmogID=0
    -- which Layer 2 sets into merged but we double-check here.)
    -- All other slots being nil means a hidden appearance — send explicit clear (slot.0.0).
    local missing = {}
    local nilCount = 0
    local clearCount = 0
    local layer2OverrideCount = 0
    for slot = 0, 13 do
        if not merged[slot] then
            nilCount = nilCount + 1
            local isAlwaysNil = ALWAYS_NIL_SLOTS[slot]
            Log(string.format("nil-detect: slot=%d isAlwaysNil=%s layer2=%s", slot, tostring(isAlwaysNil), tostring(pendingOverrides[slot] ~= nil)))
            if isAlwaysNil then
                -- Check if Layer 2 has explicit data for this always-nil slot.
                -- If so, user manually clicked it — override the always-nil deferral.
                if pendingOverrides[slot] then
                    local tmogID = pendingOverrides[slot].transmogID or 0
                    merged[slot] = { transmogID = tmogID, option = pendingOverrides[slot].option or 0 }
                    layer2OverrideCount = layer2OverrideCount + 1
                    Log(string.format("always-nil OVERRIDE: slot=%d Layer2 has IMAID=%d, sending instead of deferring", slot, tmogID))
                else
                    missing[#missing + 1] = slot
                end
            else
                -- Nil across all layers = hidden appearance, send explicit clear
                merged[slot] = { transmogID = 0, option = 0 }
                clearCount = clearCount + 1
                Log(string.format("Hidden detect: slot=%d nil in all layers, sending clear", slot))
            end
        end
    end
    Log(string.format("nil-detection found %d nil slots, generated %d clears, %d always-nil overrides", nilCount, clearCount, layer2OverrideCount))
    if #missing > 0 then
        Log(string.format("Deferred to server baseline: slots %s (always-nil client slots)",
            table.concat(missing, ",")))
    end

    -- Stale data detection: Layer 1 returns currently-worn appearances for ALL slots,
    -- including slots the applied set doesn't define. Compare merged IMAIDs against
    -- the pre-snapshot to detect stale echoes. Layer 2 slots are exempt (user choices).
    local staleCount = 0
    for slot, data in pairs(merged) do
        if data.transmogID > 0 and not layer2Slots[slot] and preSnapshot[slot] then
            if data.transmogID == preSnapshot[slot] then
                Log(string.format("stale-detect: slot=%d merged=%d snapshot=%d -> clear", slot, data.transmogID, preSnapshot[slot]))
                merged[slot] = { transmogID = 0, option = 0 }
                staleCount = staleCount + 1
            end
        end
    end
    if staleCount > 0 then
        Log(string.format("stale-detect: cleared %d stale slots", staleCount))
    end

    -- Encode: "slot.transmogID.option[.illusionID];..."
    -- 4th field only included when illusion data was explicitly set
    local parts = {}
    for slot, data in pairs(merged) do
        if data.illusionID ~= nil then
            parts[#parts + 1] = string.format("%d.%d.%d.%d", slot, data.transmogID, data.option, data.illusionID)
        else
            parts[#parts + 1] = string.format("%d.%d.%d", slot, data.transmogID, data.option)
        end
    end

    if #parts == 0 then
        Log("CommitAndApplyAllPending — hybrid merge produced 0 overrides")
        wipe(pendingOverrides)
        wipe(pendingIllusions)
        return
    end

    local payload = table.concat(parts, ";")

    -- Addon message payload limit is 255 bytes.
    -- Worst case: 12 3-field + 2 4-field (illusions) = ~192 bytes. Multi-part handles overflow.
    if #payload <= 255 then
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, payload, "WHISPER", UnitName("player"))
    else
        -- Split at nearest ; boundary (253 = 255 limit minus 2-byte "1>" prefix)
        local mid = payload:sub(1, 253):match(".*;") or payload:sub(1, 253)
        local part1 = mid
        local part2 = payload:sub(#mid + 1)
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, "1>" .. part1, "WHISPER", UnitName("player"))
        C_ChatInfo.SendAddonMessage(ADDON_PREFIX, "2>" .. part2, "WHISPER", UnitName("player"))
    end

    Log(string.format("Sent %d overrides (%d bytes): %s", #parts, #payload, payload))

    wipe(pendingOverrides)
    wipe(pendingIllusions)
end)
