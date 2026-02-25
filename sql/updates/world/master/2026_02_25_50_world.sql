-- ============================================================================
-- Workflow 6H: Game Event Creature/Gameobject Cleanup
-- Removes game_event_creature and game_event_gameobject entries that reference
-- GUIDs no longer present in the creature/gameobject tables.
-- Also cleans up pool_creature entries referencing non-existent creature spawns.
-- ============================================================================

-- Delete game_event_creature entries where the creature GUID no longer exists
DELETE FROM game_event_creature WHERE guid NOT IN (SELECT guid FROM creature);

-- Delete game_event_gameobject entries where the gameobject GUID no longer exists
DELETE FROM game_event_gameobject WHERE guid NOT IN (SELECT guid FROM gameobject);

-- Clean up pool_members entries referencing non-existent creature spawns (type=0)
DELETE FROM pool_members WHERE type = 0 AND spawnId NOT IN (SELECT guid FROM creature);

-- Clean up pool_members entries referencing non-existent gameobject spawns (type=1)
DELETE FROM pool_members WHERE type = 1 AND spawnId NOT IN (SELECT guid FROM gameobject);
