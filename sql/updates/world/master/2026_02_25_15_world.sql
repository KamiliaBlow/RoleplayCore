-- Category 2E: Quest-required on references, negligible drop chance, and MinCount/MaxCount swaps
-- Three sub-categories of loot table warnings cleaned up in this file.

-- ============================================================================
-- 2E-1: QuestRequired on Reference items (3,554 rows: 3,534 creature + 20 gameobject)
-- Error: "quest required will be ignored" — QuestRequired flag on Reference-type (ItemType=1)
-- loot entries is meaningless; the server ignores it. Clear the flag.
-- ============================================================================

-- creature_loot_template: clear QuestRequired on all reference entries (3,534 rows)
UPDATE creature_loot_template SET QuestRequired = 0 WHERE ItemType = 1 AND QuestRequired != 0;

-- gameobject_loot_template: clear QuestRequired on all reference entries (20 rows)
UPDATE gameobject_loot_template SET QuestRequired = 0 WHERE ItemType = 1 AND QuestRequired != 0;

-- ============================================================================
-- 2E-2: Negligible drop chance (1,783 rows in creature_loot_template)
-- Error: "low chance (X) - skipped" — items with Chance > 0 but < 0.0001 are effectively
-- zero probability. The server already skips them at runtime. Remove them.
-- ============================================================================

DELETE FROM creature_loot_template WHERE Chance > 0 AND Chance < 0.0001;

-- ============================================================================
-- 2E-3: MaxCount < MinCount (34 rows in gameobject_loot_template)
-- Error: "MaxCount < MinCount - skipped" — the count range is inverted.
-- Fix: use LEAST/GREATEST to safely swap without left-to-right assignment issues.
-- ============================================================================

UPDATE gameobject_loot_template
SET MinCount = LEAST(MinCount, MaxCount),
    MaxCount = GREATEST(MinCount, MaxCount)
WHERE (Entry, Item) IN (
        (68447,141852), (68451,140176), (68467,140176), (68467,141852), (68467,141854),
        (68468,138786), (68471,140176), (68475,141852), (68475,141854), (230428,111349),
        (234457,112891), (237357,111349), (249341,140226), (251052,121407), (252159,140249),
        (252668,140176), (252668,141854), (252671,140176), (252674,140176), (252676,138786),
        (252680,141852), (252683,140176), (252683,141852), (252683,141854), (252684,138786),
        (252685,138786), (252686,141852), (252686,141854), (268897,160107), (272689,147809),
        (297874,45534), (333214,182771), (338681,178602), (355940,176958)
    )
    AND MaxCount < MinCount;
