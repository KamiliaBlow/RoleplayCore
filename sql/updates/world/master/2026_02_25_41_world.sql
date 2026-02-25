-- ============================================================================
-- 5H: Quest starter/ender cleanup for non-existent creatures and gameobjects
-- Removes creature_queststarter/questender rows referencing creatures that
-- don't exist in creature_template (~114 errors), and gameobject_queststarter/
-- questender rows referencing gameobjects not in gameobject_template (~58 errors).
-- ============================================================================

-- creature_queststarter: remove entries for non-existent creatures (~102 errors)
DELETE FROM creature_queststarter WHERE id NOT IN (SELECT entry FROM creature_template);

-- creature_questender: remove entries for non-existent creatures (~12 errors)
DELETE FROM creature_questender WHERE id NOT IN (SELECT entry FROM creature_template);

-- gameobject_queststarter: remove entries for non-existent gameobjects (~24 errors)
DELETE FROM gameobject_queststarter WHERE id NOT IN (SELECT entry FROM gameobject_template);

-- gameobject_questender: remove entries for non-existent gameobjects (~34 errors)
DELETE FROM gameobject_questender WHERE id NOT IN (SELECT entry FROM gameobject_template);
