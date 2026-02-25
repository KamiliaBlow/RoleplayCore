-- ============================================================================
-- Workflow 6F + 6G: Spawn Group and Pool Cleanup
-- Removes broken spawn_group_member entries and empty pool_template entries.
-- ============================================================================

-- 6F: Delete spawn_group entries where the creature spawn no longer exists
DELETE FROM spawn_group
WHERE spawnType = 0 AND spawnId NOT IN (SELECT guid FROM creature);

-- 6F: Delete spawn_group entries where the gameobject spawn no longer exists
DELETE FROM spawn_group
WHERE spawnType = 1 AND spawnId NOT IN (SELECT guid FROM gameobject);

-- 6G: Delete empty pool_template entries (pools with no members)
-- pool_members.poolSpawnId references pool_template.entry
DELETE FROM pool_template WHERE entry NOT IN (
    SELECT DISTINCT poolSpawnId FROM pool_members
);
