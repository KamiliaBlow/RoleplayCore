-- TransmogSpy: Transmog Debug Logger
-- Monitors and logs all transmog API calls, events, and state changes

local RED = "|cffff4444"
local GREEN = "|cff44ff44"
local YELLOW = "|cffffff44"
local CYAN = "|cff44ffff"
local WHITE = "|cffffffff"
local RESET = "|r"

-- SavedVariables
TransmogSpyDB = TransmogSpyDB or {}

-- Server-side log relay — shows up in Debug.log alongside TransmogBridge entries
local LOG_PREFIX = "TSPY_LOG"
C_ChatInfo.RegisterAddonMessagePrefix(LOG_PREFIX)
local function ServerLog(msg)
    -- Strip WoW color codes for clean server logs
    local clean = msg:gsub("|c%x%x%x%x%x%x%x%x", ""):gsub("|r", "")
    if #clean > 255 then clean = clean:sub(1, 255) end
    pcall(C_ChatInfo.SendAddonMessage, LOG_PREFIX, clean, "WHISPER", UnitName("player"))
end

local eventLog = {}
local autoMode = false
local autoTicker = nil
local lastPendingState = false
local preApplySnapshot = nil
local preApplyTimestamp = nil
local buttonsHooked = false
local quietMode = false           -- (#5) suppress chat output, log to SV only

-- Slot definitions
local SLOT_NAMES = {
    [0]  = "HEAD",
    [1]  = "SHOULDER",
    [3]  = "BACK",
    [4]  = "CHEST",
    [5]  = "TABARD",
    [6]  = "SHIRT",
    [7]  = "WRIST",
    [8]  = "HANDS",
    [9]  = "WAIST",
    [10] = "LEGS",
    [11] = "FEET",
    [12] = "MAINHAND",
    [13] = "OFFHAND",
}

-- Map slot index to inventory slot name for TransmogUtil
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

local SLOT_IDS = {}
for id in pairs(SLOT_NAMES) do
    SLOT_IDS[#SLOT_IDS + 1] = id
end
table.sort(SLOT_IDS)

-------------------------------------------------------------------------------
-- (#6) API availability cache — checked once at load, not per-call
-------------------------------------------------------------------------------

local HAS = {
    OutfitSlotInfo    = C_TransmogOutfitInfo and C_TransmogOutfitInfo.GetViewedOutfitSlotInfo ~= nil,
    SlotVisualInfo    = C_Transmog and C_Transmog.GetSlotVisualInfo ~= nil
                        and TransmogUtil and TransmogUtil.GetTransmogLocation ~= nil
                        and Enum and Enum.TransmogType ~= nil,
    SlotInfo          = C_Transmog and C_Transmog.GetSlotInfo ~= nil
                        and TransmogUtil and TransmogUtil.GetTransmogLocation ~= nil
                        and Enum and Enum.TransmogType ~= nil,
    HasPending        = C_TransmogOutfitInfo and C_TransmogOutfitInfo.HasPendingOutfitTransmogs ~= nil,
    PendingCost       = C_TransmogOutfitInfo and C_TransmogOutfitInfo.GetPendingTransmogCost ~= nil,
    SetPending        = C_TransmogOutfitInfo and C_TransmogOutfitInfo.SetPendingTransmog ~= nil,
    ClearAllPending   = C_TransmogOutfitInfo and C_TransmogOutfitInfo.ClearAllPending ~= nil,
    ClearPending      = C_TransmogOutfitInfo and C_TransmogOutfitInfo.ClearPending ~= nil,
    CommitApply       = C_TransmogOutfitInfo and C_TransmogOutfitInfo.CommitAndApplyAllPending ~= nil,
    GetOutfitsInfo    = C_TransmogOutfitInfo and C_TransmogOutfitInfo.GetOutfitsInfo ~= nil,
    CollectionOutfits = C_TransmogCollection and C_TransmogCollection.GetOutfits ~= nil,
}

-------------------------------------------------------------------------------
-- Utility
-------------------------------------------------------------------------------

local function Timestamp()
    return date("%H:%M:%S")
end

local function Log(msg)
    local line = format("[TransmogSpy %s] %s", Timestamp(), msg)
    if not quietMode then
        print(line)
    end
    eventLog[#eventLog + 1] = line
    if #eventLog > 500 then
        table.remove(eventLog, 1)
    end
    -- Persist to SavedVariables
    TransmogSpyDB.log = TransmogSpyDB.log or {}
    TransmogSpyDB.log[#TransmogSpyDB.log + 1] = line
    if #TransmogSpyDB.log > 2000 then
        table.remove(TransmogSpyDB.log, 1)
    end
    -- Relay to server Debug.log
    ServerLog(msg)
end

local function SlotLabel(slot)
    return format("%d(%s)", slot, SLOT_NAMES[slot] or "?")
end

local function BoolStr(v)
    if v == nil then return "nil" end
    return tostring(v)
end

-- (#3) pcall wrapper that logs errors (each unique label logged once per session)
local loggedErrors = {}

local function TryCall(label, func, ...)
    if not func then return false, nil end
    local ok, result = pcall(func, ...)
    if not ok then
        if not loggedErrors[label] then
            loggedErrors[label] = true
            Log(format("%s%s failed: %s%s", RED, label, tostring(result), RESET))
        end
        return false, nil
    end
    return true, result
end

-------------------------------------------------------------------------------
-- Slot State Capture
-------------------------------------------------------------------------------

local function CaptureSlotState(slot)
    local state = { slot = slot }

    -- C_TransmogOutfitInfo.GetViewedOutfitSlotInfo (new API)
    if HAS.OutfitSlotInfo then
        local ok, info = TryCall("GetViewedOutfitSlotInfo",
            C_TransmogOutfitInfo.GetViewedOutfitSlotInfo, slot, 0, 0)
        if ok and info then
            state.transmogID = info.transmogID
            state.hasPending = info.hasPending
            state.isPendingCollected = info.isPendingCollected
            state.canTransmogrify = info.canTransmogrify
            state.hasUndo = info.hasUndo
            state.isHideVisual = info.isHideVisual
            state.texture = info.texture
        end
        -- Secondary shoulder (option=1)
        if slot == 1 then
            local ok2, info2 = TryCall("GetViewedOutfitSlotInfo(shoulder2)",
                C_TransmogOutfitInfo.GetViewedOutfitSlotInfo, slot, 0, 1)
            if ok2 and info2 then
                state.shoulder2_transmogID = info2.transmogID
                state.shoulder2_hasPending = info2.hasPending
                state.shoulder2_isPendingCollected = info2.isPendingCollected
            end
        end
    end

    -- C_Transmog.GetSlotVisualInfo (old API)
    if HAS.SlotVisualInfo then
        local invName = INV_SLOT_NAMES[slot]
        if invName then
            local ok3, loc = TryCall("GetTransmogLocation(visual)",
                TransmogUtil.GetTransmogLocation, invName, Enum.TransmogType.Appearance, false)
            if ok3 and loc then
                local ok4, visInfo = TryCall("GetSlotVisualInfo",
                    C_Transmog.GetSlotVisualInfo, loc)
                if ok4 and visInfo then
                    state.visual_pendingSourceID = visInfo.pendingSourceID
                    state.visual_appliedSourceID = visInfo.appliedSourceID
                    state.visual_selectedSourceID = visInfo.selectedSourceID
                    state.visual_hasPending = visInfo.hasPending
                    state.visual_hasUndo = visInfo.hasUndo
                end
            end
        end
    end

    -- C_Transmog.GetSlotInfo (old API)
    if HAS.SlotInfo then
        local invName = INV_SLOT_NAMES[slot]
        if invName then
            local ok5, loc = TryCall("GetTransmogLocation(slotinfo)",
                TransmogUtil.GetTransmogLocation, invName, Enum.TransmogType.Appearance, false)
            if ok5 and loc then
                local ok6, slotInfo = TryCall("GetSlotInfo",
                    C_Transmog.GetSlotInfo, loc)
                if ok6 and slotInfo then
                    state.slotinfo_isTransmogrified = slotInfo.isTransmogrified
                    state.slotinfo_hasPending = slotInfo.hasPending
                    state.slotinfo_hasUndo = slotInfo.hasUndo
                    state.slotinfo_canTransmogrify = slotInfo.canTransmogrify
                    state.slotinfo_cannotTransmogrifyReason = slotInfo.cannotTransmogrifyReason
                end
            end
        end
    end

    return state
end

local function CaptureAllSlots()
    local snapshot = {}
    for _, slot in ipairs(SLOT_IDS) do
        snapshot[slot] = CaptureSlotState(slot)
    end

    -- Global pending state
    if HAS.HasPending then
        local ok, val = TryCall("HasPendingOutfitTransmogs",
            C_TransmogOutfitInfo.HasPendingOutfitTransmogs)
        if ok then snapshot._hasPending = val end
    end
    if HAS.PendingCost then
        local ok, val = TryCall("GetPendingTransmogCost",
            C_TransmogOutfitInfo.GetPendingTransmogCost)
        if ok then snapshot._cost = val end
    end

    return snapshot
end

local function PrintSlotState(prefix, state, color)
    color = color or WHITE
    local parts = { format("%s%s slot=%s:", color, prefix, SlotLabel(state.slot)) }

    if state.transmogID ~= nil then
        parts[#parts + 1] = format("transmogID=%s", tostring(state.transmogID))
    end
    if state.hasPending ~= nil then
        parts[#parts + 1] = format("hasPending=%s", BoolStr(state.hasPending))
    end
    if state.isPendingCollected ~= nil then
        parts[#parts + 1] = format("isPendingCollected=%s", BoolStr(state.isPendingCollected))
    end
    if state.canTransmogrify ~= nil then
        parts[#parts + 1] = format("canTransmogrify=%s", BoolStr(state.canTransmogrify))
    end
    if state.hasUndo ~= nil then
        parts[#parts + 1] = format("hasUndo=%s", BoolStr(state.hasUndo))
    end
    if state.isHideVisual ~= nil then
        parts[#parts + 1] = format("isHideVisual=%s", BoolStr(state.isHideVisual))
    end

    -- Secondary shoulder
    if state.shoulder2_transmogID ~= nil then
        parts[#parts + 1] = format("| shoulder2: transmogID=%s hasPending=%s",
            tostring(state.shoulder2_transmogID), BoolStr(state.shoulder2_hasPending))
    end

    Log(table.concat(parts, " ") .. RESET)
end

local function CompareAndPrintSnapshots(pre, post)
    Log(CYAN .. "=== POST-APPLY COMPARISON ===" .. RESET)
    for _, slot in ipairs(SLOT_IDS) do
        local preSlot = pre[slot]
        local postSlot = post[slot]
        if preSlot and postSlot then
            local changes = {}
            if preSlot.transmogID ~= postSlot.transmogID then
                changes[#changes + 1] = format("transmogID: %s->%s",
                    tostring(preSlot.transmogID), tostring(postSlot.transmogID))
            end
            if preSlot.hasPending ~= postSlot.hasPending then
                changes[#changes + 1] = format("hasPending: %s->%s",
                    BoolStr(preSlot.hasPending), BoolStr(postSlot.hasPending))
            end

            if #changes > 0 then
                local hadPending = preSlot.hasPending
                local lostTransmog = hadPending and (postSlot.transmogID == 0 or postSlot.transmogID == nil)
                local color = lostTransmog and RED or YELLOW
                local suffix = lostTransmog and (RED .. " << LOST!" .. RESET) or ""
                Log(format("%sPOST-APPLY slot=%s: %s%s%s",
                    color, SlotLabel(slot), table.concat(changes, ", "), suffix, RESET))
            else
                Log(format("%sPOST-APPLY slot=%s: unchanged (transmogID=%s)%s",
                    GREEN, SlotLabel(slot), tostring(postSlot.transmogID), RESET))
            end
        end
    end
end

-- (#4) Deferred comparison logic — shared by multiple event triggers
local function TryDeferredComparison(triggerEvent)
    if not preApplySnapshot then return end

    -- Expire stale snapshots (>10 seconds old = probably unrelated event)
    local age = preApplyTimestamp and (GetTime() - preApplyTimestamp) or 0
    if age > 10 then
        Log(format("%sDiscarding stale pre-apply snapshot (%.1fs old)%s", RED, age, RESET))
        preApplySnapshot = nil
        preApplyTimestamp = nil
        return
    end

    local snapshot = CaptureAllSlots()

    Log(format("%s=== DEFERRED POST-APPLY COMPARISON (trigger: %s, %.1fs after apply) ===%s",
        CYAN, triggerEvent, age, RESET))
    CompareAndPrintSnapshots(preApplySnapshot, snapshot)

    -- Persist
    TransmogSpyDB.lastApply = TransmogSpyDB.lastApply or {}
    TransmogSpyDB.lastApply.deferred_post = snapshot
    TransmogSpyDB.lastApply.deferred_timestamp = Timestamp()
    TransmogSpyDB.lastApply.deferred_trigger = triggerEvent
    preApplySnapshot = nil
    preApplyTimestamp = nil
end

-------------------------------------------------------------------------------
-- Event Logger
-------------------------------------------------------------------------------

local eventFrame = CreateFrame("Frame")

local TRANSMOG_EVENTS = {
    "TRANSMOGRIFY_UPDATE",
    "TRANSMOGRIFY_SUCCESS",
    "TRANSMOG_COLLECTION_UPDATED",
    "TRANSMOG_COLLECTION_SOURCE_ADDED",
    "TRANSMOG_COLLECTION_SOURCE_REMOVED",
    "TRANSMOG_COLLECTION_CAMERA_UPDATE",
    "TRANSMOG_SEARCH_UPDATED",
    "TRANSMOG_SETS_UPDATE_FAVORITE",
    "TRANSMOG_SOURCE_COLLECTABILITY_UPDATE",
    "TRANSMOG_OUTFIT_UPDATE",
    "VIEWED_TRANSMOG_OUTFIT_CHANGED",
    "VIEWED_TRANSMOG_OUTFIT_SLOTS_CHANGED",
    "VIEWED_TRANSMOG_OUTFIT_SITUATIONS_CHANGED",
    "TRANSMOG_PENDING_CLEARED",
}

local registeredEvents = {}

for _, ev in ipairs(TRANSMOG_EVENTS) do
    local ok = pcall(eventFrame.RegisterEvent, eventFrame, ev)
    if ok then
        registeredEvents[#registeredEvents + 1] = ev
    end
end

-- (#4) Events that trigger deferred post-apply comparison
local DEFERRED_TRIGGER_EVENTS = {
    ["TRANSMOGRIFY_SUCCESS"] = true,
    ["TRANSMOGRIFY_UPDATE"] = true,
    ["TRANSMOG_PENDING_CLEARED"] = true,  -- fallback if SUCCESS/UPDATE don't fire
}

-- Events that trigger a full slot auto-dump
local AUTO_DUMP_EVENTS = {
    ["TRANSMOGRIFY_SUCCESS"] = true,
    ["TRANSMOGRIFY_UPDATE"] = true,
}

eventFrame:SetScript("OnEvent", function(self, event, ...)
    local args = {}
    for i = 1, select("#", ...) do
        args[#args + 1] = tostring(select(i, ...))
    end
    local argStr = #args > 0 and table.concat(args, ", ") or "none"
    Log(format("%s%s%s: args=[%s]", CYAN, event, RESET, argStr))

    -- Auto-dump full state on key events
    if AUTO_DUMP_EVENTS[event] then
        Log(format("%sAuto-dumping slots after %s:%s", YELLOW, event, RESET))
        local snapshot = CaptureAllSlots()
        for _, slot in ipairs(SLOT_IDS) do
            PrintSlotState("  ", snapshot[slot], WHITE)
        end
    end

    -- (#4) Try deferred comparison on any trigger event
    if DEFERRED_TRIGGER_EVENTS[event] then
        TryDeferredComparison(event)
    end
end)

-------------------------------------------------------------------------------
-- API Hooks
-------------------------------------------------------------------------------

if HAS.SetPending then
    hooksecurefunc(C_TransmogOutfitInfo, "SetPendingTransmog", function(slot, tmogType, option, transmogID, displayType)
        Log(format("%sSetPendingTransmog%s: slot=%s type=%s option=%s transmogID=%s displayType=%s",
            YELLOW, RESET,
            SlotLabel(slot or -1),
            tostring(tmogType),
            tostring(option),
            tostring(transmogID),
            tostring(displayType)))
    end)
    Log(GREEN .. "Hooked: C_TransmogOutfitInfo.SetPendingTransmog" .. RESET)
end

if HAS.ClearAllPending then
    hooksecurefunc(C_TransmogOutfitInfo, "ClearAllPending", function()
        Log(YELLOW .. "ClearAllPending()" .. RESET)
    end)
    Log(GREEN .. "Hooked: C_TransmogOutfitInfo.ClearAllPending" .. RESET)
end

if HAS.ClearPending then
    hooksecurefunc(C_TransmogOutfitInfo, "ClearPending", function(slot, tmogType, option)
        Log(format("%sClearPending%s: slot=%s type=%s option=%s",
            YELLOW, RESET, SlotLabel(slot or -1), tostring(tmogType), tostring(option)))
    end)
    Log(GREEN .. "Hooked: C_TransmogOutfitInfo.ClearPending" .. RESET)
end

-- CommitAndApplyAllPending — posthook only (pre-apply captured via button PreClick)
if HAS.CommitApply then
    hooksecurefunc(C_TransmogOutfitInfo, "CommitAndApplyAllPending", function(useDiscount)
        Log(format("%sCommitAndApplyAllPending%s: useDiscount=%s (post-hook, pending already cleared)",
            YELLOW, RESET, BoolStr(useDiscount)))

        local postCallSnapshot = CaptureAllSlots()
        Log(format("  HasPendingOutfitTransmogs=%s (should be false now)",
            BoolStr(postCallSnapshot._hasPending)))
        Log(YELLOW .. "  (deferred comparison waiting for server response event)" .. RESET)
    end)
    Log(GREEN .. "Hooked: C_TransmogOutfitInfo.CommitAndApplyAllPending (post-hook)" .. RESET)
end

-- Capture pre-apply snapshot — called from button PreClick BEFORE the C function
local function CapturePreApplySnapshot(source)
    Log(format("%s====== PRE-APPLY SNAPSHOT (via %s) ======%s", RED, source, RESET))

    preApplySnapshot = CaptureAllSlots()
    preApplyTimestamp = GetTime()

    if preApplySnapshot._hasPending ~= nil then
        Log(format("  HasPendingOutfitTransmogs=%s", BoolStr(preApplySnapshot._hasPending)))
    end
    if preApplySnapshot._cost ~= nil then
        Log(format("  PendingTransmogCost=%s", tostring(preApplySnapshot._cost)))
    end
    for _, slot in ipairs(SLOT_IDS) do
        PrintSlotState("  PRE-APPLY", preApplySnapshot[slot],
            preApplySnapshot[slot].hasPending and YELLOW or WHITE)
    end

    -- Persist pre-apply
    TransmogSpyDB.lastApply = {
        timestamp = Timestamp(),
        pre = preApplySnapshot,
    }

    Log(YELLOW .. "  (post-apply comparison deferred until server responds)" .. RESET)
end

-- Hook Apply button PreClick + OnClick
local function HookApplyButton()
    if buttonsHooked then return end

    local buttonNames = {
        "WardrobeOutfitFrame.SaveOutfitButton",
        "WardrobeTransmogFrame.ApplyButton",
        "DressUpFrame.TransmogApplyButton",
    }
    local hooked = 0
    for _, path in ipairs(buttonNames) do
        local ok, btn = pcall(function()
            local obj = _G
            for part in path:gmatch("[^%.]+") do
                obj = obj[part]
                if not obj then return nil end
            end
            return obj
        end)
        if ok and btn and btn.HookScript then
            btn:HookScript("PreClick", function()
                CapturePreApplySnapshot(path)
            end)
            btn:HookScript("OnClick", function()
                Log(format("%sApply button clicked: %s%s", YELLOW, path, RESET))
            end)
            Log(format("%sHooked button PreClick+OnClick: %s%s", GREEN, path, RESET))
            hooked = hooked + 1
        end
    end
    if hooked > 0 then
        buttonsHooked = true
    end
end

-- Delay button hook until transmog UI loads (demand-loaded addon)
local buttonHookFrame = CreateFrame("Frame")
buttonHookFrame:RegisterEvent("ADDON_LOADED")
buttonHookFrame:SetScript("OnEvent", function(self, event, addon)
    if addon == "Blizzard_Transmog" or addon == "Blizzard_Collections"
        or addon == "Blizzard_EncounterJournal" then
        C_Timer.After(0.5, HookApplyButton)
    end
end)
-- Fallback: try immediately in case Blizzard_Transmog is already loaded
C_Timer.After(1.0, HookApplyButton)

-- Hook other C_TransmogOutfitInfo functions
local otherHooks = {
    "SetViewedOutfit",
    "SaveViewedOutfit",
    "DeleteOutfit",
    "RenameOutfit",
    "SetOutfitToFavorite",
    "UndoPending",
}

for _, funcName in ipairs(otherHooks) do
    if C_TransmogOutfitInfo and C_TransmogOutfitInfo[funcName] then
        hooksecurefunc(C_TransmogOutfitInfo, funcName, function(...)
            local args = {}
            for i = 1, select("#", ...) do
                args[#args + 1] = tostring(select(i, ...))
            end
            Log(format("%s%s%s(%s)", YELLOW, funcName, RESET, table.concat(args, ", ")))
        end)
        Log(format("%sHooked: C_TransmogOutfitInfo.%s%s", GREEN, funcName, RESET))
    end
end

-- Hook C_Transmog functions
local cTransmogHooks = {
    "SetPending",
    "ClearPending",
    "ClearAllPending",
    "ApplyAllPending",
}

for _, funcName in ipairs(cTransmogHooks) do
    if C_Transmog and C_Transmog[funcName] then
        hooksecurefunc(C_Transmog, funcName, function(...)
            local args = {}
            for i = 1, select("#", ...) do
                args[#args + 1] = tostring(select(i, ...))
            end
            Log(format("%sC_Transmog.%s%s(%s)", YELLOW, funcName, RESET, table.concat(args, ", ")))
        end)
        Log(format("%sHooked: C_Transmog.%s%s", GREEN, funcName, RESET))
    end
end

-------------------------------------------------------------------------------
-- Auto-monitoring (polling while transmog UI open)
-------------------------------------------------------------------------------

local function StartAutoMonitor()
    if autoTicker then return end
    autoMode = true
    Log(GREEN .. "Auto-monitoring ENABLED (2 sec interval)" .. RESET)

    autoTicker = C_Timer.NewTicker(2.0, function()
        -- Check if transmog UI is open
        local uiOpen = false
        if WardrobeFrame and WardrobeFrame:IsShown() then
            uiOpen = true
        elseif WardrobeOutfitFrame and WardrobeOutfitFrame:IsShown() then
            uiOpen = true
        end

        if not uiOpen then return end

        -- Check pending state transition
        local hasPending = false
        if HAS.HasPending then
            local ok, val = TryCall("HasPendingOutfitTransmogs(auto)",
                C_TransmogOutfitInfo.HasPendingOutfitTransmogs)
            if ok then hasPending = val end
        end

        if hasPending ~= lastPendingState then
            local transition = format("pending: %s -> %s",
                BoolStr(lastPendingState), BoolStr(hasPending))
            Log(format("%sAUTO: Pending state changed: %s%s", YELLOW, transition, RESET))
            lastPendingState = hasPending

            local snapshot = CaptureAllSlots()
            for _, slot in ipairs(SLOT_IDS) do
                PrintSlotState("  AUTO", snapshot[slot],
                    snapshot[slot].hasPending and YELLOW or WHITE)
            end
        end
    end)
end

local function StopAutoMonitor()
    autoMode = false
    if autoTicker then
        autoTicker:Cancel()
        autoTicker = nil
    end
    Log(RED .. "Auto-monitoring DISABLED" .. RESET)
end

-------------------------------------------------------------------------------
-- Slash Commands
-------------------------------------------------------------------------------

local function CmdDump()
    Log(CYAN .. "=== FULL SLOT DUMP ===" .. RESET)
    local snapshot = CaptureAllSlots()
    if snapshot._hasPending ~= nil then
        Log(format("  HasPendingOutfitTransmogs = %s", BoolStr(snapshot._hasPending)))
    end
    if snapshot._cost ~= nil then
        Log(format("  PendingTransmogCost = %s", tostring(snapshot._cost)))
    end
    for _, slot in ipairs(SLOT_IDS) do
        PrintSlotState("  DUMP", snapshot[slot], WHITE)
    end
end

local function CmdPending()
    Log(CYAN .. "=== PENDING CHANGES ===" .. RESET)
    local snapshot = CaptureAllSlots()
    if snapshot._hasPending ~= nil then
        Log(format("  HasPendingOutfitTransmogs = %s%s%s",
            snapshot._hasPending and GREEN or RED,
            BoolStr(snapshot._hasPending), RESET))
    end
    if snapshot._cost ~= nil then
        Log(format("  PendingTransmogCost = %s", tostring(snapshot._cost)))
    end
    local pendingCount = 0
    for _, slot in ipairs(SLOT_IDS) do
        local s = snapshot[slot]
        if s.hasPending then
            pendingCount = pendingCount + 1
            Log(format("  %sPENDING slot=%s: transmogID=%s isPendingCollected=%s%s",
                YELLOW, SlotLabel(slot), tostring(s.transmogID),
                BoolStr(s.isPendingCollected), RESET))
        end
    end
    if pendingCount == 0 then
        Log("  No pending changes.")
    else
        Log(format("  %s%d slot(s) with pending changes%s", YELLOW, pendingCount, RESET))
    end
end

local function CmdOutfits()
    Log(CYAN .. "=== OUTFITS ===" .. RESET)
    if HAS.GetOutfitsInfo then
        local ok, outfits = TryCall("GetOutfitsInfo",
            C_TransmogOutfitInfo.GetOutfitsInfo)
        if ok and outfits then
            for i, outfit in ipairs(outfits) do
                local parts = { format("  Outfit %d:", i) }
                if type(outfit) == "table" then
                    for k, v in pairs(outfit) do
                        parts[#parts + 1] = format("%s=%s", tostring(k), tostring(v))
                    end
                else
                    parts[#parts + 1] = tostring(outfit)
                end
                Log(table.concat(parts, " "))
            end
        else
            Log("  GetOutfitsInfo returned nil or failed")
        end
    else
        Log("  C_TransmogOutfitInfo.GetOutfitsInfo not available")
    end

    if HAS.CollectionOutfits then
        local ok, outfits = TryCall("C_TransmogCollection.GetOutfits",
            C_TransmogCollection.GetOutfits)
        if ok and outfits then
            Log("  (via C_TransmogCollection.GetOutfits):")
            for i, outfit in ipairs(outfits) do
                Log(format("    %d: %s", i, tostring(outfit)))
            end
        end
    end
end

local function CmdVisual()
    Log(CYAN .. "=== C_Transmog.GetSlotVisualInfo ===" .. RESET)
    if not HAS.SlotVisualInfo then
        Log(format("  %sAPI not available (C_Transmog.GetSlotVisualInfo or TransmogUtil missing)%s", RED, RESET))
        return
    end
    for _, slot in ipairs(SLOT_IDS) do
        local s = CaptureSlotState(slot)
        local parts = { format("  slot=%s:", SlotLabel(slot)) }
        if s.visual_pendingSourceID ~= nil then
            parts[#parts + 1] = format("pendingSrc=%s", tostring(s.visual_pendingSourceID))
            parts[#parts + 1] = format("appliedSrc=%s", tostring(s.visual_appliedSourceID))
            parts[#parts + 1] = format("selectedSrc=%s", tostring(s.visual_selectedSourceID))
            parts[#parts + 1] = format("hasPending=%s", BoolStr(s.visual_hasPending))
            parts[#parts + 1] = format("hasUndo=%s", BoolStr(s.visual_hasUndo))
        else
            parts[#parts + 1] = "returned nil for this slot"
        end
        Log(table.concat(parts, " "))
    end
end

local function CmdSlotInfo()
    Log(CYAN .. "=== C_Transmog.GetSlotInfo ===" .. RESET)
    if not HAS.SlotInfo then
        Log(format("  %sAPI not available (C_Transmog.GetSlotInfo or TransmogUtil missing)%s", RED, RESET))
        return
    end
    for _, slot in ipairs(SLOT_IDS) do
        local s = CaptureSlotState(slot)
        local parts = { format("  slot=%s:", SlotLabel(slot)) }
        if s.slotinfo_isTransmogrified ~= nil then
            parts[#parts + 1] = format("isTransmogrified=%s", BoolStr(s.slotinfo_isTransmogrified))
            parts[#parts + 1] = format("hasPending=%s", BoolStr(s.slotinfo_hasPending))
            parts[#parts + 1] = format("hasUndo=%s", BoolStr(s.slotinfo_hasUndo))
            parts[#parts + 1] = format("canTransmogrify=%s", BoolStr(s.slotinfo_canTransmogrify))
            if s.slotinfo_cannotTransmogrifyReason then
                parts[#parts + 1] = format("reason=%s", tostring(s.slotinfo_cannotTransmogrifyReason))
            end
        else
            parts[#parts + 1] = "returned nil for this slot"
        end
        Log(table.concat(parts, " "))
    end
end

local function CmdEvents()
    local total = #eventLog
    local start = math.max(1, total - 49)
    local count = total > 0 and (total - start + 1) or 0
    -- Print directly (not via Log) to avoid polluting the event log
    print(format("%s=== LAST %d EVENTS (of %d total) ===%s", CYAN, count, total, RESET))
    for i = start, total do
        print(format("  %s", eventLog[i]))
    end
end

local function CmdClear()
    eventLog = {}
    loggedErrors = {}
    TransmogSpyDB.log = {}
    TransmogSpyDB.lastApply = nil
    preApplySnapshot = nil
    preApplyTimestamp = nil
    Log(GREEN .. "Log cleared." .. RESET)
end

local function CmdAuto()
    if autoMode then
        StopAutoMonitor()
    else
        StartAutoMonitor()
    end
end

-- (#1) Manual snapshot capture
local function CmdSnapshot()
    CapturePreApplySnapshot("manual /tspy snapshot")
end

-- (#2) Pretty-print last apply from SavedVariables
local function CmdLast()
    local last = TransmogSpyDB.lastApply
    if not last then
        Log("  No saved apply data. Use the transmog UI to apply changes first.")
        return
    end

    Log(CYAN .. "=== LAST APPLY ===" .. RESET)
    Log(format("  Timestamp: %s", last.timestamp or "?"))

    if last.pre then
        Log(YELLOW .. "  --- PRE-APPLY ---" .. RESET)
        if last.pre._hasPending ~= nil then
            Log(format("    HasPendingOutfitTransmogs = %s", BoolStr(last.pre._hasPending)))
        end
        if last.pre._cost ~= nil then
            Log(format("    PendingTransmogCost = %s", tostring(last.pre._cost)))
        end
        for _, slot in ipairs(SLOT_IDS) do
            local s = last.pre[slot]
            if s then
                PrintSlotState("    PRE", s, s.hasPending and YELLOW or WHITE)
            end
        end
    else
        Log(RED .. "  No pre-apply snapshot saved" .. RESET)
    end

    if last.deferred_post then
        Log(format("%s  --- POST-APPLY (trigger: %s, at %s) ---%s",
            YELLOW, last.deferred_trigger or "?", last.deferred_timestamp or "?", RESET))
        for _, slot in ipairs(SLOT_IDS) do
            local s = last.deferred_post[slot]
            if s then
                PrintSlotState("    POST", s, WHITE)
            end
        end

        -- Re-run comparison
        if last.pre then
            CompareAndPrintSnapshots(last.pre, last.deferred_post)
        end
    else
        Log(RED .. "  No post-apply snapshot saved (server may not have responded)" .. RESET)
    end
end

-- (#5) Toggle quiet mode
local function CmdQuiet()
    quietMode = not quietMode
    -- Always print this one regardless of quiet mode
    print(format("[TransmogSpy %s] %sQuiet mode %s%s (logging to SavedVariables %s)",
        Timestamp(),
        quietMode and GREEN or YELLOW,
        quietMode and "ENABLED" or "DISABLED",
        RESET,
        quietMode and "only" or "+ chat"))
end

local function CmdAPIs()
    Log(CYAN .. "=== AVAILABLE TRANSMOG APIs ===" .. RESET)

    local apis = {
        { "C_TransmogOutfitInfo", C_TransmogOutfitInfo },
        { "C_Transmog", C_Transmog },
        { "C_TransmogCollection", C_TransmogCollection },
        { "C_TransmogSets", C_TransmogSets },
        { "TransmogUtil", TransmogUtil },
    }

    for _, entry in ipairs(apis) do
        local name, tbl = entry[1], entry[2]
        if tbl then
            local funcs = {}
            for k, v in pairs(tbl) do
                if type(v) == "function" then
                    funcs[#funcs + 1] = k
                end
            end
            table.sort(funcs)
            Log(format("  %s%s%s: %d functions", GREEN, name, RESET, #funcs))
            for _, fn in ipairs(funcs) do
                Log(format("    .%s", fn))
            end
        else
            Log(format("  %s%s%s: NOT AVAILABLE", RED, name, RESET))
        end
    end

    -- Show cached availability
    Log(CYAN .. "  --- Cached availability (HAS) ---" .. RESET)
    for k, v in pairs(HAS) do
        Log(format("    %s%s%s = %s",
            v and GREEN or RED, k, RESET, BoolStr(v)))
    end
end

local function CmdHelp()
    print(CYAN .. "TransmogSpy Commands:" .. RESET)
    print("  /tspy dump      - Dump all slot states")
    print("  /tspy pending   - Show pending changes per slot")
    print("  /tspy snapshot  - Manually capture pre-apply snapshot now")
    print("  /tspy last      - Show last apply comparison (survives /reload)")
    print("  /tspy outfits   - List all outfit info")
    print("  /tspy visual    - Dump C_Transmog.GetSlotVisualInfo per slot")
    print("  /tspy slotinfo  - Dump C_Transmog.GetSlotInfo per slot")
    print("  /tspy events    - Show last 50 logged events")
    print("  /tspy clear     - Clear log and saved data")
    print("  /tspy auto      - Toggle auto-monitoring (2 sec while UI open)")
    print("  /tspy quiet     - Toggle quiet mode (SavedVariables only)")
    print("  /tspy apis      - List all available transmog API functions")
    print("  /tspy help      - This help")
end

SLASH_TRANSMOGSPY1 = "/tspy"
SLASH_TRANSMOGSPY2 = "/transmogspy"

SlashCmdList["TRANSMOGSPY"] = function(msg)
    msg = (msg or ""):lower():trim()

    if msg == "dump" then
        CmdDump()
    elseif msg == "pending" then
        CmdPending()
    elseif msg == "snapshot" or msg == "snap" then
        CmdSnapshot()
    elseif msg == "last" then
        CmdLast()
    elseif msg == "outfits" then
        CmdOutfits()
    elseif msg == "visual" then
        CmdVisual()
    elseif msg == "slotinfo" then
        CmdSlotInfo()
    elseif msg == "events" then
        CmdEvents()
    elseif msg == "clear" then
        CmdClear()
    elseif msg == "auto" then
        CmdAuto()
    elseif msg == "quiet" then
        CmdQuiet()
    elseif msg == "apis" then
        CmdAPIs()
    elseif msg == "help" or msg == "" then
        CmdHelp()
    else
        print(format("%sUnknown command: %s%s", RED, msg, RESET))
        CmdHelp()
    end
end

-------------------------------------------------------------------------------
-- Init
-------------------------------------------------------------------------------

Log(GREEN .. "TransmogSpy loaded." .. RESET)
Log(format("  Registered %d/%d events", #registeredEvents, #TRANSMOG_EVENTS))
Log("  Type /tspy help for commands.")

for _, ev in ipairs(registeredEvents) do
    Log(format("  %s+ %s%s", GREEN, ev, RESET))
end
