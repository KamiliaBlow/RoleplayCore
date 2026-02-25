-- =====================================================================
-- Category 2A, File 2: Clear dangling non-creature loot template references
-- =====================================================================
-- Covers gameobject loot references, dangling reference_loot_template
-- pointers, and documents cases that cannot be fixed via SQL.
--
-- All statements are idempotent — safe to run multiple times.
-- =====================================================================


-- =====================================================================
-- Gameobject loot references (~4,000 errors)
-- =====================================================================
-- gameobject_template.Data1 is the loot ID for:
--   type 3  (CHEST)
--   type 25 (FISHINGHOLE)
--   type 50 (GATHERING_NODE)
-- gameobject_template.Data30 is chestPersonalLoot (type 3 only)
-- gameobject_template.Data33 is chestPushLoot (type 3 only)

-- Clear dangling gameobject primary loot references
UPDATE gameobject_template SET Data1 = 0
WHERE type IN (3, 25, 50) AND Data1 != 0
AND NOT EXISTS (SELECT 1 FROM gameobject_loot_template WHERE Entry = gameobject_template.Data1);

-- Clear dangling chest personal loot references (type 3 only)
UPDATE gameobject_template SET Data30 = 0
WHERE type = 3 AND Data30 != 0
AND NOT EXISTS (SELECT 1 FROM gameobject_loot_template WHERE Entry = gameobject_template.Data30);

-- Clear dangling chest push loot references (type 3 only)
UPDATE gameobject_template SET Data33 = 0
WHERE type = 3 AND Data33 != 0
AND NOT EXISTS (SELECT 1 FROM gameobject_loot_template WHERE Entry = gameobject_template.Data33);


-- =====================================================================
-- Item loot, Milling, Prospecting — CANNOT fix via SQL
-- =====================================================================
-- These loot template references are driven by DBC/DB2 item flags in
-- ItemSparse, which are not editable through SQL:
--
--   item_loot_template:       ~1,925 errors — ITEM_FLAG_HAS_LOOT (DBC flag)
--   milling_loot_template:       ~49 errors — ITEM_FLAG_IS_MILLABLE (DBC flag)
--   prospecting_loot_template:   ~29 errors — ITEM_FLAG_IS_PROSPECTABLE (DBC flag)
--
-- No action taken. These would require hotfix entries or DBC edits.


-- =====================================================================
-- Spell loot — CANNOT fix via SQL
-- =====================================================================
-- spell_loot_template: ~2,243 errors
-- The server checks IsLootCrafting from spell DBC attributes. Since
-- spell data comes from DBC/DB2, we cannot clear the flag via SQL.
--
-- No action taken.


-- =====================================================================
-- Disenchant loot — CANNOT fix via SQL
-- =====================================================================
-- disenchant_loot_template: ~416 errors
-- Disenchant loot IDs are assigned via ItemDisenchantLoot and ItemBonus
-- DBC tables. These are not editable through SQL.
--
-- No action taken.


-- =====================================================================
-- Reference loot template — clean up dangling references (~23,462 errors)
-- =====================================================================
-- Loot tables can reference groups in reference_loot_template via
-- ItemType = 1. When those reference entries don't exist, the server
-- logs an error for each dangling pointer. We delete the referencing
-- rows from all loot tables since they serve no purpose without the
-- target reference group (the server already skips them).

-- Delete creature_loot_template rows referencing non-existent reference groups
DELETE FROM creature_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = creature_loot_template.Item);

-- Delete gameobject_loot_template rows referencing non-existent reference groups
DELETE FROM gameobject_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = gameobject_loot_template.Item);

-- Delete skinning_loot_template rows referencing non-existent reference groups
DELETE FROM skinning_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = skinning_loot_template.Item);

-- Delete pickpocketing_loot_template rows referencing non-existent reference groups
DELETE FROM pickpocketing_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = pickpocketing_loot_template.Item);

-- Delete disenchant_loot_template rows referencing non-existent reference groups
DELETE FROM disenchant_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = disenchant_loot_template.Item);

-- Delete fishing_loot_template rows referencing non-existent reference groups
DELETE FROM fishing_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = fishing_loot_template.Item);

-- Delete item_loot_template rows referencing non-existent reference groups
DELETE FROM item_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = item_loot_template.Item);

-- Delete mail_loot_template rows referencing non-existent reference groups
DELETE FROM mail_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = mail_loot_template.Item);

-- Delete milling_loot_template rows referencing non-existent reference groups
DELETE FROM milling_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = milling_loot_template.Item);

-- Delete prospecting_loot_template rows referencing non-existent reference groups
DELETE FROM prospecting_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = prospecting_loot_template.Item);

-- Delete spell_loot_template rows referencing non-existent reference groups
DELETE FROM spell_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = spell_loot_template.Item);

-- Delete scrapping_loot_template rows referencing non-existent reference groups
DELETE FROM scrapping_loot_template
WHERE ItemType = 1
AND NOT EXISTS (SELECT 1 FROM reference_loot_template WHERE Entry = scrapping_loot_template.Item);

-- Clean self-references within reference_loot_template itself
-- (MySQL requires a derived table to SELECT from a table being DELETEd)
DELETE FROM reference_loot_template
WHERE ItemType = 1
AND NOT EXISTS (
    SELECT 1 FROM (SELECT Entry FROM reference_loot_template) AS ref
    WHERE ref.Entry = reference_loot_template.Item
);
