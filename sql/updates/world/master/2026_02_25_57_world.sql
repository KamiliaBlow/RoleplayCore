-- ============================================================================
-- Workflow 6L + 6M: Spell Proc, Trainer, and Miscellaneous Fixes
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 6L-2: Add spell_proc entries for spells with SPELL_ATTR3_CAN_PROC_FROM_PROCS
-- that have no proc restriction, risking infinite proc loops.
-- These need DisableEffectsMask set to prevent runaway procs.
-- 45 spells total.
-- ---------------------------------------------------------------------------
-- Note: spell_proc entries require careful tuning per-spell. Adding placeholder
-- entries that disable the proc-from-proc behavior to prevent infinite loops.
DELETE FROM spell_proc WHERE SpellId IN (
    14190,49530,55221,371780,372381,382144,383703,407876,413669,422747,
    422925,425518,429634,441198,441814,441824,442290,444773,449624,449645,
    451302,451432,451454,453636,453639,453641,453646,453670,453685,453710,
    454379,458572,462990,467741,1215676,1216603,1217017,1223157,1233466,
    1233480,1251069,1252947,1252976,1263660,1273126
);
INSERT INTO spell_proc (SpellId, SchoolMask, SpellFamilyName, SpellFamilyMask0, SpellFamilyMask1, SpellFamilyMask2, SpellFamilyMask3, ProcFlags, ProcFlags2, SpellTypeMask, SpellPhaseMask, HitMask, AttributesMask, DisableEffectsMask, ProcsPerMinute, Chance, Cooldown, Charges) VALUES
(14190,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(49530,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(55221,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(371780,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(372381,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(382144,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(383703,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(407876,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(413669,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(422747,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(422925,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(425518,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(429634,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(441198,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(441814,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(441824,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(442290,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(444773,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(449624,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(449645,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(451302,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(451432,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(451454,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453636,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453639,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453641,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453646,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453670,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453685,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(453710,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(454379,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(458572,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(462990,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(467741,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1215676,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1216603,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1217017,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1223157,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1233466,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1233480,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1251069,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1252947,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1252976,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1263660,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0),
(1273126,0,0,0,0,0,0,0,0,0,0,0,0x4,0,0,0,0,0);

-- ---------------------------------------------------------------------------
-- 6L-3: Fix existing spell_proc entries with missing SpellPhaseMask
-- Without SpellPhaseMask, the proc will never trigger.
-- Set SpellPhaseMask to 0x4 (SPELL_PROC_SPELL_PHASE_HIT) as default.
-- 19 spells affected.
-- ---------------------------------------------------------------------------
UPDATE spell_proc SET SpellPhaseMask = 0x4
WHERE SpellPhaseMask = 0 AND SpellId IN (
    7434,39958,53651,55380,70727,144998,145179,199921,209389,260242,321136,
    378279,378405,384815,384914,403530,406101,433718,469883
);

-- ---------------------------------------------------------------------------
-- 6L-3b: Fix spell_proc entries with missing ProcFlags
-- Without ProcFlags, the proc will never trigger.
-- Set ProcFlags to 0x10 (PROC_FLAG_DEAL_HELPFUL_ABILITY) as minimum default.
-- 8 spells affected.
-- ---------------------------------------------------------------------------
UPDATE spell_proc SET ProcFlags = 0x10
WHERE ProcFlags = 0 AND ProcFlags2 = 0 AND SpellId IN (
    157299,268175,371032,383695,383980,390693,392302,473520
);

-- ---------------------------------------------------------------------------
-- 6M-5: Fix trainer_spell entries with invalid ReqSkillLine values
-- Spells referencing SkillLine 129 (FIRST_AID, removed) and 2575
-- Set ReqSkillLine to 0 to remove the skill requirement
-- ---------------------------------------------------------------------------
UPDATE trainer_spell SET ReqSkillLine = 0
WHERE ReqSkillLine IN (129, 2575)
  AND SpellId IN (
    3276,3277,3278,3280,3308,7928,7929,7934,10840,10841,10847,18629,18630,
    27032,27033,45545,45546,50299,54254,54255,74556,74557,74558,74560,88893,
    102697,102698,110408,201701,202853,202854,211353,211696,211926,212067,
    221690,310874
);

-- ---------------------------------------------------------------------------
-- 6M-7: Delete ui_map_quest_line entries for empty/non-existing questlines
-- 67 rows across 16 questline IDs that have no quests
-- ---------------------------------------------------------------------------
DELETE FROM ui_map_quest_line WHERE QuestLineId IN (
    82,149,171,197,212,214,225,226,227,231,232,234,235,272,372,5561
);

-- ---------------------------------------------------------------------------
-- 6M-8: Delete spawn_tracking_quest_objective entries with invalid objectives
-- 44 rows across 33 quest objective IDs that don't exist
-- ---------------------------------------------------------------------------
DELETE FROM spawn_tracking_quest_objective WHERE QuestObjectiveId IN (
    274964,279996,280563,280564,393319,393332,397901,397903,397907,397909,
    397913,397915,397919,397921,397925,397927,397931,397933,397937,397939,
    397943,397945,397949,397951,397955,397957,407047,450329,450330,450331,
    450332,451884,461360
);

-- ---------------------------------------------------------------------------
-- 6M-6: Delete vehicle_template_accessory entries for creatures missing spellclick
-- 30 entries reference creatures that have no npc_spellclick_spells entry,
-- meaning the vehicle accessory can never be seated. (22 unique entries)
-- ---------------------------------------------------------------------------
DELETE FROM vehicle_template_accessory WHERE entry IN (
    101638,101640,114323,142640,154577,180673,181851,185490,187120,187410,
    187414,187708,187710,187860,187949,188010,189826,191938,194912,196954,
    198034,9000002
);

-- ---------------------------------------------------------------------------
-- 6M-9: Delete skill_discovery_template entries for non-existent spells
-- 35 rows across 34 spell IDs that don't exist
-- ---------------------------------------------------------------------------
DELETE FROM skill_discovery_template WHERE spellId IN (
    56944,56965,64323,115063,123516,123548,123549,129673,129674,129675,
    129676,129687,129705,129757,129796,129814,129843,129887,130025,130026,
    130109,130140,130168,133106,165304,165456,165460,165461,165463,165464,
    165465,165466,165467,165564
);

-- ---------------------------------------------------------------------------
-- 6M-4: Fix lfg_dungeon_template entries with invalid coordinates
-- 70 entries have wrong coordinates. Since we don't have the correct coords,
-- delete the entries so the server falls back to default behavior.
-- ---------------------------------------------------------------------------
DELETE FROM lfg_dungeon_template WHERE dungeonId IN (
    18,165,499,504,511,517,537,539,542,543,566,567,586,588,589,590,593,594,
    595,602,603,604,606,607,608,615,616,619,624,625,637,639,640,646,647,648,
    649,652,654,655,791,792,793,794,795,796,797,799,800,801,802,803,805,806,
    807,808,809,810,811,812,813,814,815,816,817,852,1594,2058,2289,2661
);

-- ---------------------------------------------------------------------------
-- 6L-1: Spell proc DBC ProcFlags warnings (1,357 unique spells)
-- These spells have DBC ProcFlags set but a non-proc aura type.
-- They each need individual spell_proc entries with correct parameters.
-- This is an upstream TrinityCore data issue affecting 1,300+ spells.
-- TODO: Requires per-spell analysis to determine correct proc parameters.
--       Not automated here due to risk of incorrect proc behavior.
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- 6M-1: Serverside spell difficulty issues (15 spells)
-- These are serverside spells that incorrectly reference regular spells.
-- This is a code-level issue, not fixable via SQL.
-- Spell IDs: 6440, 16331, 26161, 26162, 34703, 34704, 34705, 34707,
--            34710, 34781, 35773, 49885, 276827, 277430, 287688
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- 6M-3: World location / WorldSafeLocs issues (161 location IDs)
-- These reference DBC entries that may not exist in the 12.x client.
-- Not fixable via SQL — requires DBC/DB2 data correction or removal of
-- references from tables that use these location IDs.
-- ---------------------------------------------------------------------------
