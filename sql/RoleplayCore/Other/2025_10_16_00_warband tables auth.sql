-- Warband groups are account-wide: group ids are unique per battle.net account,
-- realmId records the realm the row was created from (provenance only).

CREATE TABLE `account_warband_groups` (
  `id` bigint(20) unsigned NOT NULL,
  `accountId` int(10) unsigned NOT NULL,
  `realmId` int(10) unsigned NOT NULL DEFAULT '1',
  `orderIndex` tinyint(3) unsigned NOT NULL,
  `name` varchar(257) NOT NULL,
  `warbandSceneId` int(10) unsigned NOT NULL DEFAULT '0',
  `flags` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `account_warband_group_members` (
  `accountId` int(10) unsigned NOT NULL,
  `realmId` int(10) unsigned NOT NULL DEFAULT '1',
  `groupId` bigint(20) unsigned NOT NULL,
  `characterGuid` bigint(20) unsigned NOT NULL,
  `placementId` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`, `groupId`, `characterGuid`),
  CONSTRAINT `fk_warband_group` FOREIGN KEY (`accountId`, `groupId`) REFERENCES `account_warband_groups` (`accountId`, `id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
