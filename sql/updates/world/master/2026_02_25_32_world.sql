-- SmartAI Fix 4B: Remove smart_scripts with invalid quest objective IDs
-- SMART_EVENT_QUEST_OBJ_COMPLETION (event_type=48) uses event_param1 as the
-- quest objective ID. If that ID doesn't exist in quest_objectives, the entry is skipped.
-- Source: SmartScriptMgr.cpp IsEventValid() SMART_EVENT_QUEST_OBJ_COMPLETION case

DELETE FROM smart_scripts
WHERE event_type = 48  -- SMART_EVENT_QUEST_OBJ_COMPLETION
  AND event_param1 NOT IN (SELECT ID FROM quest_objectives);
