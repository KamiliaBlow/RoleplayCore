INSERT INTO `rbac_permissions` VALUES (1002, 'Command: .barber');
INSERT INTO `rbac_permissions` VALUES (1003, 'Command: .castgroup');
INSERT INTO `rbac_permissions` VALUES (1004, 'Command: .castscene');
INSERT INTO `rbac_permissions` VALUES (1360, 'Command: .customnpc set displayid');
INSERT INTO `rbac_permissions` VALUES (1361, 'Command: .customnpc set guild');
INSERT INTO `rbac_permissions` VALUES (1362, 'Command: .customnpc set rank');
INSERT INTO `rbac_permissions` VALUES (1363, 'Command: .customnpc set scale');
INSERT INTO `rbac_permissions` VALUES (1364, 'Command: .customnpc set tameable');
INSERT INTO `rbac_permissions` VALUES (1365, 'Command: .customnpc remove variation');
INSERT INTO `rbac_permissions` VALUES (1398, 'Command: .gobject set scale');
INSERT INTO `rbac_permissions` VALUES (1589, 'Command: .npc set scale');
INSERT INTO `rbac_permissions` VALUES (2101, 'Command: .customnpc create');
INSERT INTO `rbac_permissions` VALUES (2102, 'Command: .customnpc spawn');
INSERT INTO `rbac_permissions` VALUES (2103, 'Command: .customnpc set displayname');
INSERT INTO `rbac_permissions` VALUES (2104, 'Command: .customnpc set face');
INSERT INTO `rbac_permissions` VALUES (2105, 'Command: .customnpc set gender');
INSERT INTO `rbac_permissions` VALUES (2106, 'Command: .customnpc set race');
INSERT INTO `rbac_permissions` VALUES (2107, 'Command: .customnpc set subname');
INSERT INTO `rbac_permissions` VALUES (2108, 'Command: .customnpc equip armor');
INSERT INTO `rbac_permissions` VALUES (2109, 'Command: .customnpc equip left');
INSERT INTO `rbac_permissions` VALUES (2110, 'Command: .customnpc equip ranged');
INSERT INTO `rbac_permissions` VALUES (2111, 'Command: .customnpc equip right');
INSERT INTO `rbac_permissions` VALUES (2112, 'Command: .customnpc delete');
INSERT INTO `rbac_permissions` VALUES (2113, 'Command: .customnpc unequip armor');
INSERT INTO `rbac_permissions` VALUES (2114, 'Command: .customnpc unequip left');
INSERT INTO `rbac_permissions` VALUES (2115, 'Command: .customnpc unequip ranged');
INSERT INTO `rbac_permissions` VALUES (2116, 'Command: .customnpc unequip right');
INSERT INTO `rbac_permissions` VALUES (3004, 'Command: .gob visible');
INSERT INTO `rbac_permissions` VALUES (1005, 'Command: .typing on');
INSERT INTO `rbac_permissions` VALUES (1006, 'Command: .typing off');
INSERT INTO `rbac_permissions` VALUES (1008, 'Command: .disp');
INSERT INTO `rbac_permissions` VALUES (1009, 'Command: .disp head');
INSERT INTO `rbac_permissions` VALUES (1010, 'Command: .disp shoulders');
INSERT INTO `rbac_permissions` VALUES (1011, 'Command: .disp shirt');
INSERT INTO `rbac_permissions` VALUES (1012, 'Command: .disp chest');
INSERT INTO `rbac_permissions` VALUES (1013, 'Command: .disp waist'); 
INSERT INTO `rbac_permissions` VALUES (1014, 'Command: .disp legs'); 
INSERT INTO `rbac_permissions` VALUES (1015, 'Command: .disp feet'); 
INSERT INTO `rbac_permissions` VALUES (1016, 'Command: .disp wrists'); 
INSERT INTO `rbac_permissions` VALUES (1017, 'Command: .disp hands'); 
INSERT INTO `rbac_permissions` VALUES (1018, 'Command: .disp back'); 
INSERT INTO `rbac_permissions` VALUES (1019, 'Command: .disp tabard'); 
INSERT INTO `rbac_permissions` VALUES (1020, 'Command: .disp mainhand'); 
INSERT INTO `rbac_permissions` VALUES (1021, 'Command: .disp offhand');

INSERT INTO `rbac_linked_permissions` VALUES (199, 1002);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1003);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1004);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1360);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1361);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1362);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1363);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1364);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1365);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1398);
INSERT INTO `rbac_linked_permissions` VALUES (193, 1589);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2101);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2102);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2103);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2104);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2105);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2106);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2107);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2108);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2109);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2110);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2111);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2112);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2113);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2114);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2115);
INSERT INTO `rbac_linked_permissions` VALUES (193, 2116);
INSERT INTO `rbac_linked_permissions` VALUES (193, 3004);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1005);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1006);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1008);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1009);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1010);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1011);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1012);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1013);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1014);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1015);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1016);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1017);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1018);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1019);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1020);
INSERT INTO `rbac_linked_permissions` VALUES (199, 1021);

CREATE TABLE `account_warband_groups` (
  `id` bigint(20) unsigned NOT NULL,
  `accountId` int(10) unsigned NOT NULL,
  `orderIndex` tinyint(3) unsigned NOT NULL,
  `name` varchar(257) NOT NULL,
  `warbandSceneId` int(10) unsigned NOT NULL DEFAULT '0',
  `flags` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`, `realmId`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `account_warband_group_members` (
  `accountId` int(10) unsigned NOT NULL,
  `realmId` int(10) unsigned NOT NULL DEFAULT '1',
  `groupId` bigint(20) unsigned NOT NULL,
  `characterGuid` bigint(20) unsigned NOT NULL,
  `placementId` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`accountId`, `realmId`, `groupId`, `characterGuid`),
  CONSTRAINT `fk_warband_group` FOREIGN KEY (`accountId`, `realmId`, `groupId`) REFERENCES `account_warband_groups` (`accountId`, `realmId`, `id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_data_global` (
  `accountId` int UNSIGNED NOT NULL,
  `type` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `time` bigint NOT NULL DEFAULT 0,
  `data` longblob NULL,
  PRIMARY KEY (`accountId`, `type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_realm_transfer` (
  `accountId` int UNSIGNED NOT NULL,
  `connectKey` int UNSIGNED NOT NULL,
  `characterGuid` bigint UNSIGNED NOT NULL,
  `createTime` bigint NOT NULL,
  PRIMARY KEY (`accountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `realm_character_schemas` (
  `realmId` int UNSIGNED NOT NULL,
  `schemaName` varchar(64) NOT NULL,
  PRIMARY KEY (`realmId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_bank_tab_settings` (
  `battlenetAccountId` int unsigned NOT NULL,
  `tabId` tinyint unsigned NOT NULL,
  `name` varchar(16) NOT NULL DEFAULT '',
  `icon` varchar(64) NOT NULL DEFAULT '',
  `description` varchar(2048) NOT NULL DEFAULT '',
  `depositFlags` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`, `tabId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_bank_item` (
  `battlenetAccountId` int unsigned NOT NULL,
  `bag` tinyint unsigned NOT NULL COMMENT 'tab index (0-4)',
  `slot` tinyint unsigned NOT NULL COMMENT 'slot within tab (0-97)',
  `item` bigint unsigned NOT NULL,
  `sourceRealm` int unsigned NOT NULL COMMENT 'realm whose characters database holds the item_instance row',
  PRIMARY KEY (`battlenetAccountId`, `bag`, `slot`),
  KEY `idx_item` (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_bank_coinage` (
  `battlenetAccountId` int unsigned NOT NULL,
  `coinage` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;