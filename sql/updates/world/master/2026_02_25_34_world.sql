-- SmartAI Fix 4F: Remove entries with deprecated/unused event types and action types
-- Source: SmartScriptMgr.cpp IsEventValid() "Unused event_type" and "Unused action_type" cases
--         Also handles "has invalid action type" (action_type >= SMART_ACTION_END = 160)
--         and "deprecated event flags" (difficulty flags 0x02|0x04|0x08|0x10 = 0x1E)

-- ============================================================================
-- 4F-1: Delete entries with unused/deprecated event types
-- These event types are marked UNUSED, DO NOT REUSE in SmartScriptMgr.h:
--   12 = SMART_EVENT_TARGET_HEALTH_PCT
--   14 = SMART_EVENT_FRIENDLY_HEALTH
--   18 = SMART_EVENT_TARGET_MANA_PCT
--   30 = SMART_EVENT_CHARMED_TARGET
--   39 = SMART_EVENT_WAYPOINT_START
--   66 = SMART_EVENT_EVENT_PHASE_CHANGE
--   67 = SMART_EVENT_IS_BEHIND_TARGET
-- ============================================================================
DELETE FROM smart_scripts
WHERE event_type IN (12, 14, 18, 30, 39, 66, 67);

-- ============================================================================
-- 4F-2: Delete entries with unused/deprecated action types
-- These action types are marked UNUSED, DO NOT REUSE in SmartScriptMgr.h:
--   15  = SMART_ACTION_CALL_AREAEXPLOREDOREVENTHAPPENS
--   18  = SMART_ACTION_SET_UNIT_FLAG
--   19  = SMART_ACTION_REMOVE_UNIT_FLAG
--   26  = SMART_ACTION_CALL_GROUPEVENTHAPPENS
--   58  = SMART_ACTION_INSTALL_AI_TEMPLATE
--   61  = SMART_ACTION_SET_SWIM
--   75  = SMART_ACTION_ADD_AURA
--   76  = SMART_ACTION_OVERRIDE_SCRIPT_BASE_OBJECT
--   77  = SMART_ACTION_RESET_SCRIPT_BASE_OBJECT
--   93  = SMART_ACTION_SEND_GO_CUSTOM_ANIM
--   94  = SMART_ACTION_SET_DYNAMIC_FLAG
--   95  = SMART_ACTION_ADD_DYNAMIC_FLAG
--   96  = SMART_ACTION_REMOVE_DYNAMIC_FLAG
--   104 = SMART_ACTION_SET_GO_FLAG
--   105 = SMART_ACTION_ADD_GO_FLAG
--   106 = SMART_ACTION_REMOVE_GO_FLAG
--   119 = SMART_ACTION_SET_CAN_FLY
--   120 = SMART_ACTION_REMOVE_AURAS_BY_TYPE
--   121 = SMART_ACTION_SET_SIGHT_DIST
--   122 = SMART_ACTION_FLEE
--   126 = SMART_ACTION_REMOVE_ALL_GAMEOBJECTS
-- ============================================================================
DELETE FROM smart_scripts
WHERE action_type IN (15, 18, 19, 26, 58, 61, 75, 76, 77, 93, 94, 95, 96,
                      104, 105, 106, 119, 120, 121, 122, 126);

-- ============================================================================
-- 4F-3: Delete entries with invalid action types (>= SMART_ACTION_END = 160)
-- These are action_type values that exceed the known enum range entirely
-- ============================================================================
DELETE FROM smart_scripts
WHERE action_type >= 160;

-- ============================================================================
-- 4F-4: Delete entries with invalid event types (>= SMART_EVENT_END = 91)
-- ============================================================================
DELETE FROM smart_scripts
WHERE event_type >= 91;

-- ============================================================================
-- 4F-5: Fix deprecated event flags (difficulty flags)
-- Deprecated flags: 0x02 | 0x04 | 0x08 | 0x10 = 0x1E (30 decimal)
-- Clear only the deprecated bits, preserving all other valid flags
-- ============================================================================
UPDATE smart_scripts
SET event_flags = event_flags & ~0x1E
WHERE (event_flags & 0x1E) != 0;
