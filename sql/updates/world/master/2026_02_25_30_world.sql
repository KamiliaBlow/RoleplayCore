-- SmartAI Fix 4H: Remove smart_scripts data for creatures not using SmartAI and non-existent GUIDs
-- These creatures have smart_scripts entries but their creature_template.AIName is not 'SmartAI',
-- or the creature GUID referenced does not exist in the creature table.
-- Source: SmartScriptMgr.cpp LoadSmartAIFromDB() validation

-- ============================================================================
-- 4H-1: Delete smart_scripts for creature entries (source_type=0, positive entryorguid)
-- whose AIName != 'SmartAI' in creature_template
-- ============================================================================
DELETE ss FROM smart_scripts ss
LEFT JOIN creature_template ct ON ss.entryorguid = ct.entry
WHERE ss.source_type = 0
  AND ss.entryorguid > 0
  AND (ct.entry IS NULL OR ct.AIName != 'SmartAI');

-- ============================================================================
-- 4H-2: Delete smart_scripts for creature GUIDs (source_type=0, negative entryorguid)
-- where the GUID does not exist in the creature table
-- ============================================================================
DELETE ss FROM smart_scripts ss
LEFT JOIN creature c ON ss.entryorguid = -c.guid
WHERE ss.source_type = 0
  AND ss.entryorguid < 0
  AND c.guid IS NULL;

-- ============================================================================
-- 4H-3: Delete smart_scripts for creature GUIDs (source_type=0, negative entryorguid)
-- where the creature exists but its template AIName != 'SmartAI'
-- ============================================================================
DELETE ss FROM smart_scripts ss
INNER JOIN creature c ON ss.entryorguid = -c.guid
INNER JOIN creature_template ct ON c.id = ct.entry
WHERE ss.source_type = 0
  AND ss.entryorguid < 0
  AND ct.AIName != 'SmartAI';
