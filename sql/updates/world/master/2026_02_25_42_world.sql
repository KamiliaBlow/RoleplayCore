-- ============================================================================
-- 5B: Quest objectives fixes for non-existent entities (~1,266 errors)
-- Clears ObjectID on quest_objectives that reference creatures, gameobjects,
-- items, areatriggers, spells, currencies, or criteria trees that do not
-- exist in their respective tables/stores.
--
-- QuestObjectiveType enum (from QuestDef.h):
--   0 = QUEST_OBJECTIVE_MONSTER       -> creature_template.entry
--   1 = QUEST_OBJECTIVE_ITEM          -> item_template.entry
--   2 = QUEST_OBJECTIVE_GAMEOBJECT    -> gameobject_template.entry
--   3 = QUEST_OBJECTIVE_TALKTO        -> creature_template.entry
--  10 = QUEST_OBJECTIVE_AREATRIGGER   -> areatrigger store (DBC, can't fix via SQL)
--
-- Note: Types 4 (currency), 5 (learnspell), 14 (criteria_tree) are validated
-- against DBC stores which we cannot query in SQL. Those ~22 errors are left
-- for the server to silently handle (it already zeroes them at load time).
-- ============================================================================

-- Type 0 (QUEST_OBJECTIVE_MONSTER): clear objectives referencing non-existent creatures
-- Includes 353 entries with ObjectID=0 (already empty) which are harmless no-ops
UPDATE quest_objectives SET ObjectID = 0
WHERE Type = 0
AND ObjectID != 0
AND ObjectID NOT IN (SELECT entry FROM world.creature_template);

-- Type 3 (QUEST_OBJECTIVE_TALKTO): also references creature_template
UPDATE quest_objectives SET ObjectID = 0
WHERE Type = 3
AND ObjectID != 0
AND ObjectID NOT IN (SELECT entry FROM world.creature_template);

-- Type 1 (QUEST_OBJECTIVE_ITEM): clear objectives referencing non-existent items
UPDATE quest_objectives SET ObjectID = 0
WHERE Type = 1
AND ObjectID != 0
AND ObjectID NOT IN (SELECT ID FROM hotfixes.item);

-- Type 2 (QUEST_OBJECTIVE_GAMEOBJECT): clear objectives referencing non-existent gameobjects
UPDATE quest_objectives SET ObjectID = 0
WHERE Type = 2
AND ObjectID != 0
AND ObjectID NOT IN (SELECT entry FROM world.gameobject_template);
