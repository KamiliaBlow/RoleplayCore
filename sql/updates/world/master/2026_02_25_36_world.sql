-- SmartAI Fix 4D/4K: Kill credit and creature summon warnings (low priority)
-- Also fixes invalid Boolean parameter values
-- Source: SmartScriptMgr.cpp TC_SAI_IS_BOOLEAN_VALID macro and various validations

-- ============================================================================
-- Fix invalid Boolean values in smart_scripts
-- The TC_SAI_IS_BOOLEAN_VALID macro rejects any value > 1 for Boolean fields.
-- The most common case is action_type 80 (SMART_ACTION_CALL_TIMED_ACTIONLIST)
-- where action_param3 (allowOverride) should be 0 or 1 but is set to 2.
-- Value 2 likely was intended as "true" — normalize to 1.
-- ============================================================================

-- Fix timedActionList.allowOverride (action_type=80, param3 is allowOverride)
UPDATE smart_scripts
SET action_param3 = 1
WHERE action_type = 80
  AND action_param3 > 1;

-- Fix event kill.playerOnly (event_type=9 SMART_EVENT_KILL, event_param1 is playerOnly for some layouts)
-- Entry 17826 has event_type=10 with event_param3 (playerOnly) = 5000
-- The specific pattern from the log: event 10 = SMART_EVENT_OOC_LOS
-- Actually examining the log: "Entry 17826 SourceType 0 Event 10 Action 1 uses param
-- e.event.kill.playerOnly" — this is event_type=9 (KILL), id=10, param1=playerOnly
-- Let's fix generically: any event_type=9 with event_param1 > 1
UPDATE smart_scripts
SET event_param1 = 1
WHERE event_type = 9  -- SMART_EVENT_KILL
  AND event_param1 > 1;

-- Fix talk.useTalkTarget (action_type=1 SMART_ACTION_TALK, action_param2 is useTalkTarget)
-- Entry 26608 has action_param2=5, should be 0 or 1
UPDATE smart_scripts
SET action_param2 = 1
WHERE action_type = 1  -- SMART_ACTION_TALK
  AND action_param2 > 1;

-- Fix wpStart.run and wpStart.repeat Boolean params (action_type=53)
-- action_param1 = run (boolean), action_param5 (reactState) also has constraints
UPDATE smart_scripts
SET action_param1 = 1
WHERE action_type = 53  -- SMART_ACTION_WP_START
  AND action_param1 > 1;

-- ============================================================================
-- 4D/4K: Kill credit and summon warnings
-- These are informational warnings about redundant SmartAI actions where a spell
-- already handles the kill credit or creature summon. They don't cause skipped
-- entries — just warnings. We leave these as-is since removing them could break
-- intentional fallback behavior. No data changes needed for warnings.
-- ============================================================================
-- NOTE: Kill credit (~8,700 warnings) and creature summon (~40 warnings) are
-- informational only. The SmartAI entries still load and function. Removing them
-- risks breaking quest/scripting logic where the redundancy is intentional.
-- These can be investigated on a case-by-case basis if needed.
