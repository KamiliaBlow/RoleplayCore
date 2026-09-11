-- Repair warband group member rows misassigned by the global-id era:
-- a member row must belong to the account that owns its character, and its
-- (accountId, realmId, groupId) must reference that account's own group.
-- Run AFTER 2026_09_11_00 (needs the composite schema). Order matters.
-- Multi-realm installs: run both statements once per character DB
-- (characters, characters_rep, ...). The guid-only join is realm-agnostic;
-- a wrongly dropped membership is cosmetic (the player re-adds the character
-- to a group in the UI), so no per-realm precision is required here.

-- 1) re-point to the character's real account, but only if that account has the group
UPDATE `auth`.`account_warband_group_members` m
  JOIN `characters`.`characters` c ON c.`guid` = m.`characterGuid`
  SET m.`accountId` = c.`account`
  WHERE m.`accountId` <> c.`account`
    AND EXISTS (SELECT 1 FROM `auth`.`account_warband_groups` g
                WHERE g.`accountId` = c.`account` AND g.`realmId` = m.`realmId` AND g.`id` = m.`groupId`);

-- 2) memberships that cannot be re-pointed (the account has no such group) are stale, drop them
DELETE m FROM `auth`.`account_warband_group_members` m
  JOIN `characters`.`characters` c ON c.`guid` = m.`characterGuid`
  WHERE m.`accountId` <> c.`account`;

-- verify: must return 0 rows
SELECT m.characterGuid, m.groupId, m.accountId AS assignedAccount, c.account AS actualAccount
FROM `auth`.`account_warband_group_members` m
JOIN `characters`.`characters` c ON c.`guid` = m.`characterGuid`
WHERE c.account <> m.accountId;
