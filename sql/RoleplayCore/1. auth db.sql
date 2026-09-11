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
  `realmId` int(10) unsigned NOT NULL DEFAULT '1',
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