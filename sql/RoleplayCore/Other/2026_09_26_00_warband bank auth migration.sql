-- Warband bank move to the auth database - data migration.
--
-- Apply-order guide:
--   1. Stop both worldservers.
--   2. Back up the auth and both characters databases.
--   3. Apply sql/updates/auth/master/2026_09_26_00_auth.sql (creates the empty auth tables).
--   4. Run this file ONCE, edited for your realm ids and schema names (see below).
--   5. Start the new worldserver binaries. The first login on each realm pulls the bank
--      items homed elsewhere into that realm's item_instance automatically.
--
-- Edit the two schema names below to match your characters databases
-- (the same names as in CharacterSelect.ExtraRealms / your worldserver configs).
-- Replace realm id 1 / 2 in the sourceRealm literals with YOUR realm ids.
--
-- Conflict rule: if the same bank slot holds different items on both realms (both realms
-- had a bank snapshot before this change), the row from the FIRST schema wins and the
-- losing realm's row is dropped. The dropped item_instance row stays orphaned in its
-- characters database - clean it up manually if you care, it is never referenced again.
-- After the merge, DROP TABLE removes the old per-realm bank tables.
--
-- Note: installs that created the auth tables while account_bank_item still had
-- UNIQUE KEY idx_item must downgrade it to a plain index first - item guids are only
-- unique per realm, two realms can legitimately hold the same numeric guid:
--   ALTER TABLE `auth`.`account_bank_item` DROP INDEX idx_item, ADD INDEX idx_item (`item`);

-- 1. Tab settings: realm 1 wins on conflict.
REPLACE INTO `auth`.`account_bank_tab_settings` (battlenetAccountId, tabId, name, icon, description, depositFlags)
SELECT battlenetAccountId, tabId, name, icon, description, depositFlags FROM `characters`.`account_bank_tab_settings`;

INSERT IGNORE INTO `auth`.`account_bank_tab_settings` (battlenetAccountId, tabId, name, icon, description, depositFlags)
SELECT battlenetAccountId, tabId, name, icon, description, depositFlags FROM `characters_rep`.`account_bank_tab_settings`;

-- 2. Coinage: the larger balance wins (both realms saw the same shared value; the smaller
--    copy is a stale snapshot).
REPLACE INTO `auth`.`account_bank_coinage` (battlenetAccountId, coinage)
SELECT s.battlenetAccountId, GREATEST(s.coinage, IFNULL(r.coinage, 0))
FROM `characters`.`account_bank_coinage` s
LEFT JOIN `characters_rep`.`account_bank_coinage` r ON r.battlenetAccountId = s.battlenetAccountId;

INSERT IGNORE INTO `auth`.`account_bank_coinage` (battlenetAccountId, coinage)
SELECT battlenetAccountId, coinage FROM `characters_rep`.`account_bank_coinage`
WHERE battlenetAccountId NOT IN (SELECT battlenetAccountId FROM `characters`.`account_bank_coinage`);

-- 3. Items: realm 1 wins on slot conflicts, every row carries the realm id whose
--    characters database holds the item_instance row.
REPLACE INTO `auth`.`account_bank_item` (battlenetAccountId, bag, slot, item, sourceRealm)
SELECT battlenetAccountId, bag, slot, item, 1 FROM `characters`.`account_bank_item`;

INSERT IGNORE INTO `auth`.`account_bank_item` (battlenetAccountId, bag, slot, item, sourceRealm)
SELECT battlenetAccountId, bag, slot, item, 2 FROM `characters_rep`.`account_bank_item`;

-- 4. Remove the obsolete per-realm bank tables.
DROP TABLE IF EXISTS `characters`.`account_bank_item`;
DROP TABLE IF EXISTS `characters`.`account_bank_tab_settings`;
DROP TABLE IF EXISTS `characters`.`account_bank_coinage`;
DROP TABLE IF EXISTS `characters_rep`.`account_bank_item`;
DROP TABLE IF EXISTS `characters_rep`.`account_bank_tab_settings`;
DROP TABLE IF EXISTS `characters_rep`.`account_bank_coinage`;
