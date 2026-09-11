-- Warband groups: make group ids per-account instead of globally unique.
-- The client assigns ids for new warband groups itself, counting from the highest
-- id it received from this account (local max + 1). A global PRIMARY KEY (id)
-- therefore collides across accounts and failed the whole
-- CMSG_SETUP_WARBAND_GROUPS replace-transaction on contact.
--
-- New layout: PRIMARY KEY (accountId, realmId, id) on account_warband_groups,
-- members carry accountId/realmId and reference the composite key.

ALTER TABLE `account_warband_group_members` DROP FOREIGN KEY `fk_warband_group`;

ALTER TABLE `account_warband_groups`
  DROP PRIMARY KEY,
  ADD PRIMARY KEY (`accountId`, `realmId`, `id`);
-- note: the old KEY idx_account / idx_account_realm is now redundant (covered by the
-- composite PK) but is intentionally left alone: its name differs between installs
-- and a failing DROP KEY would abort this script midway.

-- some live DBs still have realmId as signed int here; the FK below requires the
-- referencing and referenced columns to match exactly (signedness included)
ALTER TABLE `account_warband_groups`
  MODIFY `realmId` int(10) unsigned NOT NULL DEFAULT '1';

ALTER TABLE `account_warband_group_members`
  ADD COLUMN `accountId` int(10) unsigned NOT NULL DEFAULT '0' AFTER `groupId`,
  ADD COLUMN `realmId` int(10) unsigned NOT NULL DEFAULT '1' AFTER `accountId`;

-- this backfill can misassign members where the old global-id bug let two accounts
-- share one group id; 2026_09_11_01 repairs them against the characters DB
UPDATE `account_warband_group_members` m
  JOIN `account_warband_groups` g ON m.`groupId` = g.`id`
  SET m.`accountId` = g.`accountId`, m.`realmId` = g.`realmId`;

ALTER TABLE `account_warband_group_members`
  DROP PRIMARY KEY,
  ADD PRIMARY KEY (`accountId`, `realmId`, `groupId`, `characterGuid`),
  ADD CONSTRAINT `fk_warband_group` FOREIGN KEY (`accountId`, `realmId`, `groupId`)
    REFERENCES `account_warband_groups` (`accountId`, `realmId`, `id`) ON DELETE CASCADE;
