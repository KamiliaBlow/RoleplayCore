-- =====================================================================
-- Category 2A, File 1: Clear dangling creature loot template references
-- =====================================================================
-- When creature_template_difficulty references a loot template ID that
-- doesn't exist in the corresponding *_loot_template table, the server
-- logs an error at startup. The fix is to clear the reference (set to 0)
-- so the server no longer attempts to look up the missing template.
--
-- This file covers the three creature loot columns:
--   LootID          -> creature_loot_template     (~385,000 errors)
--   SkinLootID      -> skinning_loot_template     (~10,000 errors)
--   PickPocketLootID -> pickpocketing_loot_template (~7,500 errors)
--
-- All statements are idempotent — safe to run multiple times.
-- =====================================================================

-- Clear dangling creature loot references (~385,000 errors)
-- Sets LootID to 0 where the referenced creature_loot_template entry doesn't exist
UPDATE creature_template_difficulty SET LootID = 0
WHERE LootID != 0
AND NOT EXISTS (SELECT 1 FROM creature_loot_template WHERE Entry = creature_template_difficulty.LootID);

-- Clear dangling skinning loot references (~10,000 errors)
-- Sets SkinLootID to 0 where the referenced skinning_loot_template entry doesn't exist
UPDATE creature_template_difficulty SET SkinLootID = 0
WHERE SkinLootID != 0
AND NOT EXISTS (SELECT 1 FROM skinning_loot_template WHERE Entry = creature_template_difficulty.SkinLootID);

-- Clear dangling pickpocketing loot references (~7,500 errors)
-- Sets PickPocketLootID to 0 where the referenced pickpocketing_loot_template entry doesn't exist
UPDATE creature_template_difficulty SET PickPocketLootID = 0
WHERE PickPocketLootID != 0
AND NOT EXISTS (SELECT 1 FROM pickpocketing_loot_template WHERE Entry = creature_template_difficulty.PickPocketLootID);
