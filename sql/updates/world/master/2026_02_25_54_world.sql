-- ============================================================================
-- Workflow 6D: NPC Vendor Cleanup
-- Removes vendor entries with invalid items, non-existent ExtendedCost IDs,
-- and entries for non-existent creature templates.
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 6D-1: Delete vendor entries for non-existent creature templates
-- 4 creatures: 54943, 500511, 500537, 500542 (210 rows total)
-- ---------------------------------------------------------------------------
DELETE FROM npc_vendor WHERE entry IN (54943, 500511, 500537, 500542);

-- ---------------------------------------------------------------------------
-- 6D-2: DISABLED — hotfixes.item only contains hotfix overrides, NOT all game
-- items. Most items exist only in DB2 client files with no hotfixes row.
-- This query wiped nearly all vendor data when originally run.
-- ---------------------------------------------------------------------------
-- DELETE FROM npc_vendor
-- WHERE type = 1 AND item NOT IN (SELECT ID FROM hotfixes.item);

-- ---------------------------------------------------------------------------
-- 6D-3: Delete vendor entries with invalid ExtendedCost values
-- 617 rows for vendors 152194 and 223193 reference ExtendedCost IDs that
-- don't exist. These are small numeric values (10, 20, 40, etc.) that appear
-- to be quantity/price values mistakenly placed in the ExtendedCost column.
-- ---------------------------------------------------------------------------
DELETE FROM npc_vendor
WHERE entry IN (152194, 223193)
  AND ExtendedCost != 0
  AND ExtendedCost < 10000;
