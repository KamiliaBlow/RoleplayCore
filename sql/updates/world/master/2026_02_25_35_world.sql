-- SmartAI Fix 4G: Fix missing repeat flags
-- Events HAS_AURA(23), TARGET_BUFFED(24), VICTIM_CASTING(13), FRIENDLY_IS_CC(15)
-- require either a repeat interval (repeatMin/repeatMax > 0) or the NOT_REPEATABLE flag (0x01).
-- When both repeat values are 0 and the flag is missing, the server auto-adds the flag at
-- runtime and logs an error. This fix applies the flag in the database to silence the errors.
-- Source: SmartScriptMgr.cpp LoadSmartAIFromDB() lines 337-361
--
-- For events 23 (HAS_AURA) and 24 (TARGET_BUFFED):
--   event_param3 = repeatMin, event_param4 = repeatMax
-- For event 13 (VICTIM_CASTING):
--   event_param1 = repeatMin, event_param2 = repeatMax (min/max fields)
-- For event 15 (FRIENDLY_IS_CC):
--   event_param2 = repeatMin, event_param3 = repeatMax
--
-- SMART_EVENT_FLAG_NOT_REPEATABLE = 0x01 (1)

-- HAS_AURA (23) and TARGET_BUFFED (24): repeatMin=event_param3, repeatMax=event_param4
UPDATE smart_scripts
SET event_flags = event_flags | 0x01
WHERE event_type IN (23, 24)
  AND event_param3 = 0
  AND event_param4 = 0
  AND (event_flags & 0x01) = 0
  AND source_type != 9;  -- exclude SMART_SCRIPT_TYPE_TIMED_ACTIONLIST

-- VICTIM_CASTING (13): repeatMin=event_param1, repeatMax=event_param2
UPDATE smart_scripts
SET event_flags = event_flags | 0x01
WHERE event_type = 13
  AND event_param1 = 0
  AND event_param2 = 0
  AND (event_flags & 0x01) = 0
  AND source_type != 9;  -- exclude SMART_SCRIPT_TYPE_TIMED_ACTIONLIST

-- FRIENDLY_IS_CC (15): repeatMin=event_param2, repeatMax=event_param3
UPDATE smart_scripts
SET event_flags = event_flags | 0x01
WHERE event_type = 15
  AND event_param2 = 0
  AND event_param3 = 0
  AND (event_flags & 0x01) = 0
  AND source_type != 9;  -- exclude SMART_SCRIPT_TYPE_TIMED_ACTIONLIST
