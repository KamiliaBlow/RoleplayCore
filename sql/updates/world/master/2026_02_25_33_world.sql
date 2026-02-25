-- SmartAI Fix 4C/4E/4I/4J: Invalid spell, waypoint, target, and text references
-- Source: SmartScriptMgr.cpp IsEventValid() and IsSpellValid() / IsTextValid()

-- ============================================================================
-- 4C: Delete smart_scripts entries that reference non-existent spells
-- The runtime validation uses sSpellMgr->GetSpellInfo() against DBC/DB2 data.
-- There is no SQL-accessible spell table in the 12.x world DB, so we enumerate
-- the specific spell IDs reported as non-existent in the server error log.
--
-- Action types that reference spells via action_param1:
--   SMART_ACTION_CAST = 11
--   SMART_ACTION_SELF_CAST = 85
--   SMART_ACTION_CROSS_CAST = 86
--   SMART_ACTION_INVOKER_CAST = 134
--
-- Event types that reference spells via event_param1:
--   SMART_EVENT_SPELLHIT = 8
--   SMART_EVENT_SPELLHIT_TARGET = 22
--   SMART_EVENT_ON_SPELL_CAST = 83
--   SMART_EVENT_ON_SPELL_FAILED = 84
--   SMART_EVENT_ON_SPELL_START = 85
--   SMART_EVENT_ON_AURA_APPLIED = 89
--   SMART_EVENT_ON_AURA_REMOVED = 90
-- ============================================================================

-- 283 unique spell IDs confirmed non-existent via server runtime validation
SET @invalid_spells_note = 'Extracted from SmartAIMgr non-existent Spell entry errors';

-- Delete action-side references (cast actions using non-existent spells)
DELETE FROM smart_scripts
WHERE action_type IN (11, 85, 86, 134)
  AND action_param1 IN (
    72, 324, 332, 421, 547, 588, 643, 689, 782, 865, 913, 930, 939, 945, 959,
    1000, 1006, 1026, 1032, 1058, 1094, 1245, 1430, 1499, 1672, 1777, 1785,
    2000, 2052, 2054, 2055, 2090, 2589, 2590, 2591, 2607, 2608, 2610, 2767, 2941,
    3000, 3627, 4000, 4133, 4148, 4500, 5000, 5187, 5242, 5588,
    6063, 6064, 6066, 6074, 6075, 6077, 6078, 6141, 6143, 6192, 6205, 6223, 6363, 6364,
    7386, 7405, 7816, 8005, 8050, 8053, 8056, 8058, 8075, 8076, 8129, 8134, 8218,
    8380, 8422, 8439, 8499, 8721, 8809, 9081,
    10310, 10395, 10591, 10901, 10951, 11306, 11567, 11667, 11990,
    12251, 13748, 14032, 14034, 14145, 14204, 14887,
    15230, 15242, 15245, 15550, 15785, 16799, 16828, 16830, 16831,
    17145, 17255, 17260, 18368, 19503, 19615,
    20000, 20164, 20787, 20812, 22128, 22695,
    24604, 24882, 24905, 24907, 25420, 25602, 26364, 26369, 28858,
    30223, 30914, 31293, 31803, 32130, 32329, 32931,
    33839, 34120, 34151, 34267, 34920, 34930, 34931, 34942,
    35696, 35942, 36054, 37361, 37668, 37718, 37956, 38064, 39413,
    44583, 46045, 49016, 50271, 50518, 51209,
    54840, 54879, 54955, 56247, 58017, 58027, 58079, 59307, 60200, 61610,
    70205, 70206, 70207, 70208, 70209, 70211, 70212, 70213, 70273, 70275, 70387,
    72124, 73119, 75418, 75419,
    88462, 89867, 90933, 91161, 91286, 91287, 91678, 91717, 91720, 91734, 91735,
    91859, 91863, 92066, 92834, 93041, 93130,
    100787, 108373, 109151, 109212, 114074, 114157, 114168,
    163472, 178165, 179388, 191203, 205351, 264265,
    327258, 368980, 368982, 369072, 369107,
    373194, 374152, 377029, 377411, 377490, 378428,
    381592, 381659, 383872, 385239, 385277, 386188, 387398,
    388402, 388960, 389249, 389267, 389360, 389376, 389430, 389488, 389522, 389532,
    389948, 389949, 390483, 390492, 390932,
    391480, 391791, 391799, 391821, 391840, 391842, 391849, 391866, 391870,
    391898, 391903, 391910, 391939, 391959, 391998,
    392080, 392105, 392111, 392239, 392257, 392264, 392272, 392336, 392672,
    394680, 394787, 394827, 395760, 395939,
    396349, 396430, 396790, 396836, 405871
  );

-- Delete event-side references (spellhit/spell-cast events using non-existent spells)
DELETE FROM smart_scripts
WHERE event_type IN (8, 22, 83, 84, 85, 89, 90)
  AND event_param1 IN (
    72, 324, 332, 421, 547, 588, 643, 689, 782, 865, 913, 930, 939, 945, 959,
    1000, 1006, 1026, 1032, 1058, 1094, 1245, 1430, 1499, 1672, 1777, 1785,
    2000, 2052, 2054, 2055, 2090, 2589, 2590, 2591, 2607, 2608, 2610, 2767, 2941,
    3000, 3627, 4000, 4133, 4148, 4500, 5000, 5187, 5242, 5588,
    6063, 6064, 6066, 6074, 6075, 6077, 6078, 6141, 6143, 6192, 6205, 6223, 6363, 6364,
    7386, 7405, 7816, 8005, 8050, 8053, 8056, 8058, 8075, 8076, 8129, 8134, 8218,
    8380, 8422, 8439, 8499, 8721, 8809, 9081,
    10310, 10395, 10591, 10901, 10951, 11306, 11567, 11667, 11990,
    12251, 13748, 14032, 14034, 14145, 14204, 14887,
    15230, 15242, 15245, 15550, 15785, 16799, 16828, 16830, 16831,
    17145, 17255, 17260, 18368, 19503, 19615,
    20000, 20164, 20787, 20812, 22128, 22695,
    24604, 24882, 24905, 24907, 25420, 25602, 26364, 26369, 28858,
    30223, 30914, 31293, 31803, 32130, 32329, 32931,
    33839, 34120, 34151, 34267, 34920, 34930, 34931, 34942,
    35696, 35942, 36054, 37361, 37668, 37718, 37956, 38064, 39413,
    44583, 46045, 49016, 50271, 50518, 51209,
    54840, 54879, 54955, 56247, 58017, 58027, 58079, 59307, 60200, 61610,
    70205, 70206, 70207, 70208, 70209, 70211, 70212, 70213, 70273, 70275, 70387,
    72124, 73119, 75418, 75419,
    88462, 89867, 90933, 91161, 91286, 91287, 91678, 91717, 91720, 91734, 91735,
    91859, 91863, 92066, 92834, 93041, 93130,
    100787, 108373, 109151, 109212, 114074, 114157, 114168,
    163472, 178165, 179388, 191203, 205351, 264265,
    327258, 368980, 368982, 369072, 369107,
    373194, 374152, 377029, 377411, 377490, 378428,
    381592, 381659, 383872, 385239, 385277, 386188, 387398,
    388402, 388960, 389249, 389267, 389360, 389376, 389430, 389488, 389522, 389532,
    389948, 389949, 390483, 390492, 390932,
    391480, 391791, 391799, 391821, 391840, 391842, 391849, 391866, 391870,
    391898, 391903, 391910, 391939, 391959, 391998,
    392080, 392105, 392111, 392239, 392257, 392264, 392272, 392336, 392672,
    394680, 394787, 394827, 395760, 395939,
    396349, 396430, 396790, 396836, 405871
  );

-- ============================================================================
-- 4E: Delete smart_scripts entries with non-existent WaypointPath references
-- SMART_ACTION_WP_START (53) uses action_param2 as pathID
-- ============================================================================
DELETE FROM smart_scripts
WHERE action_type = 53  -- SMART_ACTION_WP_START
  AND action_param2 != 0
  AND action_param2 NOT IN (SELECT PathId FROM waypoint_path);

-- ============================================================================
-- 4I: Fix invalid target parameters
-- Delete entries where target references non-existent creature entries/GUIDs
-- ============================================================================

-- SMART_TARGET_CREATURE_RANGE (9) and SMART_TARGET_CREATURE_DISTANCE (10)
-- use target_param1 as creature entry
DELETE FROM smart_scripts
WHERE target_type IN (9, 10)
  AND target_param1 != 0
  AND target_param1 NOT IN (SELECT entry FROM creature_template);

-- SMART_TARGET_CREATURE_GUID (11) uses target_param1 as creature GUID
-- and target_param2 as creature entry for validation
DELETE ss FROM smart_scripts ss
LEFT JOIN creature c ON ss.target_param1 = c.guid
WHERE ss.target_type = 11
  AND ss.target_param1 != 0
  AND c.guid IS NULL;

-- ============================================================================
-- 4J: Delete smart_scripts entries with non-existent creature_text references
-- SMART_ACTION_TALK (1) and SMART_ACTION_SIMPLE_TALK (84) use action_param1
-- as the text group ID. The text must exist in creature_text for the creature.
-- ============================================================================

-- For positive entryorguid (creature entry), delete if text group doesn't exist
DELETE ss FROM smart_scripts ss
WHERE ss.source_type = 0
  AND ss.entryorguid > 0
  AND ss.action_type IN (1, 84)  -- TALK, SIMPLE_TALK
  AND NOT EXISTS (
    SELECT 1 FROM creature_text ct
    WHERE ct.CreatureID = ss.entryorguid
      AND ct.GroupID = ss.action_param1
  );

-- For negative entryorguid (creature GUID), look up the creature entry then check text
DELETE ss FROM smart_scripts ss
INNER JOIN creature c ON ss.entryorguid = -c.guid
WHERE ss.source_type = 0
  AND ss.entryorguid < 0
  AND ss.action_type IN (1, 84)  -- TALK, SIMPLE_TALK
  AND NOT EXISTS (
    SELECT 1 FROM creature_text ct
    WHERE ct.CreatureID = c.id
      AND ct.GroupID = ss.action_param1
  );
