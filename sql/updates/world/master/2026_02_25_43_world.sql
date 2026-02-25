-- ============================================================================
-- 5C + 5D: Quest item and reward fixes
--
-- 5C: StartItem/ProvidedItemCount (~1,585), ItemDrop (~189+113),
--     SourceItemId (~127) errors
-- 5D: RewardSpell (~880), RewardItem (~36+21) errors
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 5C: ProvidedItemCount = 0 when StartItem is set (~1,585 errors)
-- The server auto-corrects this at load time but logs an error.
-- Fix: set ProvidedItemCount = 1 where StartItem is nonzero and exists.
-- ---------------------------------------------------------------------------
UPDATE quest_template_addon SET ProvidedItemCount = 1
WHERE ProvidedItemCount = 0
AND ID IN (SELECT ID FROM quest_template WHERE StartItem != 0);

-- ---------------------------------------------------------------------------
-- 5C: SourceItemId (StartItem) references a non-existent item (~127 errors)
-- The server clears StartItem at load time. Fix the DB to match.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET StartItem = 0
WHERE StartItem != 0
AND StartItem NOT IN (SELECT ID FROM hotfixes.item);

-- Also clear ProvidedItemCount where StartItem is 0 but count is nonzero
-- (covers quests whose StartItem we just zeroed, plus any pre-existing mismatches)
UPDATE quest_template_addon SET ProvidedItemCount = 0
WHERE ProvidedItemCount > 0
AND ID IN (SELECT ID FROM quest_template WHERE StartItem = 0);

-- ---------------------------------------------------------------------------
-- 5C: ItemDrop references non-existent items (~189 errors across ItemDrop1-4)
-- Clear the ItemDrop and its quantity when the item doesn't exist.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET ItemDrop1 = 0, ItemDropQuantity1 = 0
WHERE ItemDrop1 != 0 AND ItemDrop1 NOT IN (SELECT ID FROM hotfixes.item);

UPDATE quest_template SET ItemDrop2 = 0, ItemDropQuantity2 = 0
WHERE ItemDrop2 != 0 AND ItemDrop2 NOT IN (SELECT ID FROM hotfixes.item);

UPDATE quest_template SET ItemDrop3 = 0, ItemDropQuantity3 = 0
WHERE ItemDrop3 != 0 AND ItemDrop3 NOT IN (SELECT ID FROM hotfixes.item);

UPDATE quest_template SET ItemDrop4 = 0, ItemDropQuantity4 = 0
WHERE ItemDrop4 != 0 AND ItemDrop4 NOT IN (SELECT ID FROM hotfixes.item);

-- ---------------------------------------------------------------------------
-- 5C: ItemDrop = 0 but ItemDropQuantity > 0 (~113 errors)
-- Clear the orphaned quantity.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET ItemDropQuantity1 = 0 WHERE ItemDrop1 = 0 AND ItemDropQuantity1 > 0;
UPDATE quest_template SET ItemDropQuantity2 = 0 WHERE ItemDrop2 = 0 AND ItemDropQuantity2 > 0;
UPDATE quest_template SET ItemDropQuantity3 = 0 WHERE ItemDrop3 = 0 AND ItemDropQuantity3 > 0;
UPDATE quest_template SET ItemDropQuantity4 = 0 WHERE ItemDrop4 = 0 AND ItemDropQuantity4 > 0;

-- ---------------------------------------------------------------------------
-- 5D: RewardSpell references non-existent spell (~880 errors)
-- Spell existence is checked against SpellMgr (DBC data), not a SQL table.
-- We use spell_dbc as a proxy; spells not in any DB table are truly missing.
-- Note: The server already clears these at load time. We zero them in the DB.
-- ---------------------------------------------------------------------------
-- TrinityCore stores spell data in DBC/DB2, not in a queryable SQL table for
-- all spells. However, spells referenced by RewardSpell that don't exist in
-- the client DBC simply cannot be validated via SQL alone.
-- We can still fix the subset of rewards pointing to spells that also aren't
-- in any server-side spell table. The server will handle the rest at load.
--
-- If a spell_dbc or serverside_spell table exists, uncomment and use:
-- UPDATE quest_template SET RewardSpell = 0
-- WHERE RewardSpell != 0
-- AND RewardSpell NOT IN (SELECT ID FROM spell_dbc)
-- AND RewardSpell NOT IN (SELECT ID FROM serverside_spell);
--
-- Since spell validation requires DBC data we cannot query, these ~880 errors
-- are left for the server's existing load-time correction. No SQL fix needed.

-- ---------------------------------------------------------------------------
-- 5D: RewardItemId references non-existent items (~36 errors)
-- Clear the reward item and its count when the item doesn't exist.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET RewardItem1 = 0, RewardAmount1 = 0
WHERE RewardItem1 != 0 AND RewardItem1 NOT IN (SELECT ID FROM hotfixes.item);

UPDATE quest_template SET RewardItem2 = 0, RewardAmount2 = 0
WHERE RewardItem2 != 0 AND RewardItem2 NOT IN (SELECT ID FROM hotfixes.item);

UPDATE quest_template SET RewardItem3 = 0, RewardAmount3 = 0
WHERE RewardItem3 != 0 AND RewardItem3 NOT IN (SELECT ID FROM hotfixes.item);

UPDATE quest_template SET RewardItem4 = 0, RewardAmount4 = 0
WHERE RewardItem4 != 0 AND RewardItem4 NOT IN (SELECT ID FROM hotfixes.item);

-- ---------------------------------------------------------------------------
-- 5D: RewardItemId = 0 but RewardAmount > 0 (orphaned counts) (~some of the 21 errors)
-- Clear orphaned reward amounts.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET RewardAmount1 = 0 WHERE RewardItem1 = 0 AND RewardAmount1 > 0;
UPDATE quest_template SET RewardAmount2 = 0 WHERE RewardItem2 = 0 AND RewardAmount2 > 0;
UPDATE quest_template SET RewardAmount3 = 0 WHERE RewardItem3 = 0 AND RewardAmount3 > 0;
UPDATE quest_template SET RewardAmount4 = 0 WHERE RewardItem4 = 0 AND RewardAmount4 > 0;

-- ---------------------------------------------------------------------------
-- 5D: RewardItemCount = 0 but RewardItemId is set (~remaining of the 21 errors)
-- Set count to 1 so the reward is actually given.
-- ---------------------------------------------------------------------------
UPDATE quest_template SET RewardAmount1 = 1 WHERE RewardItem1 != 0 AND RewardAmount1 = 0;
UPDATE quest_template SET RewardAmount2 = 1 WHERE RewardItem2 != 0 AND RewardAmount2 = 0;
UPDATE quest_template SET RewardAmount3 = 1 WHERE RewardItem3 != 0 AND RewardAmount3 = 0;
UPDATE quest_template SET RewardAmount4 = 1 WHERE RewardItem4 != 0 AND RewardAmount4 = 0;
