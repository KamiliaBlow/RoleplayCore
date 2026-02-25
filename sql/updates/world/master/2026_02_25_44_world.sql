-- ============================================================================
-- 5E + 5F + 5G: Quest chain, visual effect, and flag fixes
--
-- 5E: PrevQuestId/NextQuestId/NextQuestInChain referencing non-existent
--     quests (~344 errors)
-- 5F: Orphaned quest_visual_effect rows (~317 errors)
-- 5G: SpecialFlags exceeding max allowed value (~108 errors),
--     QuestSortID/RequiredSkillId mismatches (~32 errors),
--     QuestSortID zone does not exist (~1 error)
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 5E: PrevQuestID references non-existent quests (~223 errors)
-- PrevQuestID can be negative (meaning the referenced quest must NOT be
-- completed), so we use ABS() for the lookup.
-- Most of these (208) are placeholder value 999999.
-- ---------------------------------------------------------------------------
UPDATE quest_template_addon SET PrevQuestID = 0
WHERE PrevQuestID != 0
AND ABS(PrevQuestID) NOT IN (SELECT ID FROM quest_template);

-- ---------------------------------------------------------------------------
-- 5E: NextQuestID references non-existent quests (~60 errors)
-- NextQuestID can also be negative, so use ABS().
-- ---------------------------------------------------------------------------
UPDATE quest_template_addon SET NextQuestID = 0
WHERE NextQuestID != 0
AND ABS(NextQuestID) NOT IN (SELECT ID FROM quest_template);

-- ---------------------------------------------------------------------------
-- 5E: RewardNextQuest (NextQuestInChain) references non-existent quests (~61 errors)
-- Column is RewardNextQuest in quest_template, maps to _nextQuestInChain in C++.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET RewardNextQuest = 0
WHERE RewardNextQuest != 0
AND RewardNextQuest NOT IN (SELECT ID FROM (SELECT ID FROM quest_template) AS qt2);

-- ---------------------------------------------------------------------------
-- 5F: Orphaned quest_visual_effect rows (~317 errors)
-- Visual effects referencing quest objectives that don't exist.
-- ---------------------------------------------------------------------------
DELETE FROM quest_visual_effect
WHERE ID NOT IN (SELECT ID FROM quest_objectives);

-- ---------------------------------------------------------------------------
-- 5G: SpecialFlags exceeding max allowed value of 31 (~108 errors)
-- QUEST_SPECIAL_FLAGS_DB_ALLOWED = 0x1F = 31
-- QUEST_SPECIAL_FLAGS_SEQUENCED_OBJECTIVES (0x20 = 32) is internal only,
-- computed at load time. Strip bit 5 (and any higher bits) from DB values.
--
-- Breakdown: 95 quests with value 32, 11 with 33, 2 with 36
-- Fix: mask with 31 (0x1F) to keep only valid DB bits.
-- ---------------------------------------------------------------------------
UPDATE quest_template_addon SET SpecialFlags = SpecialFlags & 31
WHERE SpecialFlags > 31;

-- ---------------------------------------------------------------------------
-- 5G: QuestSortID / RequiredSkillId mismatches (~32 errors)
-- When QuestSortID is negative, it represents a quest sort category.
-- The server validates that RequiredSkillID matches the skill expected for
-- that sort. These are DBC-based lookups we can't fully replicate in SQL.
--
-- Known mismatches from the error log:
--   QuestSortID -24  expects skill 182 (Herbalism)
--   QuestSortID -121 expects skill 164 (Blacksmithing)
--   QuestSortID -182 expects skill 165 (Leatherworking)
--   QuestSortID -304 expects skill 185 (Cooking)
--   QuestSortID -371 expects skill 773 (Inscription/Archaeology)
--   QuestSortID -373 expects skill 755 (Jewelcrafting)
--
-- Fix: set RequiredSkillID to the correct value for each sort category.
-- ---------------------------------------------------------------------------
UPDATE quest_template_addon SET RequiredSkillID = 182
WHERE ID IN (SELECT ID FROM quest_template WHERE QuestSortID = -24)
AND RequiredSkillID != 182;

UPDATE quest_template_addon SET RequiredSkillID = 164
WHERE ID IN (SELECT ID FROM quest_template WHERE QuestSortID = -121)
AND RequiredSkillID != 164;

UPDATE quest_template_addon SET RequiredSkillID = 165
WHERE ID IN (SELECT ID FROM quest_template WHERE QuestSortID = -182)
AND RequiredSkillID != 165;

UPDATE quest_template_addon SET RequiredSkillID = 185
WHERE ID IN (SELECT ID FROM quest_template WHERE QuestSortID = -304)
AND RequiredSkillID != 185;

UPDATE quest_template_addon SET RequiredSkillID = 773
WHERE ID IN (SELECT ID FROM quest_template WHERE QuestSortID = -371)
AND RequiredSkillID != 773;

UPDATE quest_template_addon SET RequiredSkillID = 755
WHERE ID IN (SELECT ID FROM quest_template WHERE QuestSortID = -373)
AND RequiredSkillID != 755;

-- ---------------------------------------------------------------------------
-- 5G: QuestSortID zone does not exist (~1 error for quest 63576, zone 1246)
-- Zone 1246 doesn't exist in AreaTable DBC. Clear SortID so the server
-- doesn't log an error. This is a positive SortID (zone case).
-- ---------------------------------------------------------------------------
UPDATE quest_template SET QuestSortID = 0 WHERE ID = 63576 AND QuestSortID = 1246;
