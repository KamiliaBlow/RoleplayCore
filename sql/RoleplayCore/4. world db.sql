-- ----------------------------
-- Table structure for creature_template_outfits
-- ----------------------------
DROP TABLE IF EXISTS `creature_template_outfits`;
CREATE TABLE `creature_template_outfits`  (
  `entry` int UNSIGNED NOT NULL,
  `npcsoundsid` INT(10) UNSIGNED NOT NULL DEFAULT '0' COMMENT 'entry from NPCSounds.dbc/db2',
  `race` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `class` tinyint UNSIGNED NOT NULL DEFAULT 1,
  `gender` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 for male, 1 for female',
  `spellvisualkitid` int NOT NULL DEFAULT 0,
  `customizations` text CHARACTER SET utf8 COLLATE utf8_general_ci NULL,
  `head` bigint NOT NULL DEFAULT 0,
  `head_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `shoulders` bigint NOT NULL DEFAULT 0,
  `shoulders_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `body` bigint NOT NULL DEFAULT 0,
  `body_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `chest` bigint NOT NULL DEFAULT 0,
  `chest_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `waist` bigint NOT NULL DEFAULT 0,
  `waist_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `legs` bigint NOT NULL DEFAULT 0,
  `legs_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `feet` bigint NOT NULL DEFAULT 0,
  `feet_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `wrists` bigint NOT NULL DEFAULT 0,
  `wrists_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `hands` bigint NOT NULL DEFAULT 0,
  `hands_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `back` bigint NOT NULL DEFAULT 0,
  `back_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `tabard` bigint NOT NULL DEFAULT 0,
  `tabard_appearance` int UNSIGNED NOT NULL DEFAULT 0,
  `guildid` bigint UNSIGNED NOT NULL DEFAULT 0,
  `description` text CHARACTER SET utf8 COLLATE utf8_general_ci NULL,
  PRIMARY KEY (`entry`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci COMMENT = 'Use positive values for item entries and negative to use item displayid for head, shoulders etc.' ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Table structure for creature_template_outfits_customizations
-- ----------------------------
DROP TABLE IF EXISTS `creature_template_outfits_customizations`;
CREATE TABLE `creature_template_outfits_customizations`  (
  `chrCustomizationOptionID` int UNSIGNED NOT NULL,
  `chrCustomizationChoiceID` int UNSIGNED NOT NULL DEFAULT 0,
  `outfitID` bigint NOT NULL
) ENGINE = InnoDB CHARACTER SET = utf8 COLLATE = utf8_general_ci ROW_FORMAT = DYNAMIC;

-- ----------------------------
-- Records of creature_template_outfits_customizations
-- ----------------------------

-- ----------------------------
-- Table structure for scrapping_loot_template
-- ----------------------------
DROP TABLE IF EXISTS `scrapping_loot_template`;
CREATE TABLE `scrapping_loot_template`  (
  `Entry` mediumint UNSIGNED NOT NULL DEFAULT 0,
  `Item` mediumint NOT NULL DEFAULT 0,
  `Reference` mediumint UNSIGNED NOT NULL DEFAULT 0,
  `Chance` float NOT NULL DEFAULT 100,
  `QuestRequired` tinyint(1) NOT NULL DEFAULT 0,
  `LootMode` smallint UNSIGNED NOT NULL DEFAULT 1,
  `GroupId` tinyint UNSIGNED NOT NULL DEFAULT 0,
  `MinCount` int UNSIGNED NOT NULL DEFAULT 1,
  `MaxCount` int UNSIGNED NOT NULL DEFAULT 1,
  `Comment` varchar(255) CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci NULL DEFAULT NULL,
  PRIMARY KEY (`Entry`, `Item`) USING BTREE
) ENGINE = MyISAM AUTO_INCREMENT = 1 CHARACTER SET = utf8mb3 COLLATE = utf8mb3_general_ci COMMENT = 'Loot System' ROW_FORMAT = FIXED;

-- ----------------------------
-- Records of scrapping_loot_template
-- ----------------------------
DELETE FROM `scrapping_loot_template`;
INSERT INTO `scrapping_loot_template` (`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`) VALUES
	(1, 160266, 0, 100, 0, 1, 0, 1, 1, NULL),
	(2, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(2, 154165, 0, 100, 0, 1, 0, 1, 8, NULL),
	(2, 11291, 0, 100, 0, 1, 0, 1, 3, NULL),
	(3, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(3, 154165, 0, 100, 0, 1, 0, 2, 8, NULL),
	(3, 11291, 0, 100, 0, 1, 0, 1, 4, NULL),
	(4, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(4, 154165, 0, 100, 0, 1, 0, 4, 8, NULL),
	(4, 11291, 0, 100, 0, 1, 0, 3, 6, NULL),
	(5, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(5, 152576, 0, 100, 0, 1, 0, 1, 3, NULL),
	(5, 159959, 0, 100, 0, 1, 0, 1, 5, NULL),
	(6, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(6, 152576, 0, 100, 0, 1, 0, 1, 3, NULL),
	(6, 152577, 0, 20, 0, 1, 0, 5, 8, NULL),
	(6, 158378, 0, 5, 0, 1, 0, 2, 6, NULL),
	(6, 159959, 0, 100, 0, 1, 0, 1, 8, NULL),
	(7, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(7, 152576, 0, 100, 0, 1, 0, 1, 3, NULL),
	(7, 152577, 0, 5, 0, 1, 0, 4, 6, NULL),
	(7, 158378, 0, 5, 0, 1, 0, 2, 4, NULL),
	(7, 159959, 0, 100, 0, 1, 0, 1, 8, NULL),
	(8, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(8, 152577, 0, 100, 0, 1, 0, 8, 21, NULL),
	(8, 158378, 0, 100, 0, 1, 0, 3, 8, NULL),
	(8, 159959, 0, 100, 0, 1, 0, 5, 10, NULL),
	(9, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(9, 152577, 0, 100, 0, 1, 0, 4, 10, NULL),
	(9, 158378, 0, 100, 0, 1, 0, 2, 6, NULL),
	(9, 159959, 0, 100, 0, 1, 0, 3, 9, NULL),
	(10, 162460, 0, 100, 0, 1, 0, 15, 15, NULL),
	(11, 162461, 0, 100, 0, 1, 0, 25, 25, NULL),
	(12, 162461, 0, 100, 0, 1, 0, 250, 250, NULL),
	(13, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(13, 152541, 0, 100, 0, 1, 0, 1, 3, NULL),
	(13, 154164, 0, 100, 0, 1, 0, 1, 2, NULL),
	(14, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(14, 152541, 0, 100, 0, 1, 0, 2, 5, NULL),
	(14, 152542, 0, 5, 0, 1, 0, 2, 5, NULL),
	(14, 154164, 0, 100, 0, 1, 0, 1, 3, NULL),
	(14, 154165, 0, 20, 0, 1, 0, 3, 9, NULL),
	(15, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(15, 152541, 0, 100, 0, 1, 0, 2, 5, NULL),
	(15, 152542, 0, 5, 0, 1, 0, 1, 4, NULL),
	(15, 154164, 0, 100, 0, 1, 0, 1, 3, NULL),
	(15, 154165, 0, 5, 0, 1, 0, 2, 7, NULL),
	(16, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(16, 152542, 0, 100, 0, 1, 0, 4, 9, NULL),
	(16, 154165, 0, 100, 0, 1, 0, 7, 12, NULL),
	(16, 154722, 0, 100, 0, 1, 0, 3, 21, NULL),
	(17, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(17, 152542, 0, 100, 0, 1, 0, 1, 4, NULL),
	(17, 154165, 0, 100, 0, 1, 0, 5, 8, NULL),
	(17, 154722, 0, 100, 0, 1, 0, 5, 12, NULL),
	(18, 162460, 0, 100, 0, 1, 0, 15, 15, NULL),
	(19, 162461, 0, 100, 0, 1, 0, 25, 25, NULL),
	(20, 162461, 0, 100, 0, 1, 0, 250, 250, NULL),
	(21, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(21, 153050, 0, 100, 0, 1, 0, 1, 3, NULL),
	(21, 154164, 0, 100, 0, 1, 0, 1, 2, NULL),
	(22, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(22, 153051, 0, 10, 0, 1, 0, 8, 17, NULL),
	(22, 154165, 0, 10, 0, 1, 0, 5, 8, NULL),
	(22, 153050, 0, 100, 0, 1, 0, 2, 5, NULL),
	(22, 154164, 0, 100, 0, 1, 0, 2, 3, NULL),
	(23, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(23, 153051, 0, 10, 0, 1, 0, 4, 9, NULL),
	(23, 154165, 0, 10, 0, 1, 0, 2, 8, NULL),
	(23, 153050, 0, 100, 0, 1, 0, 2, 5, NULL),
	(23, 154164, 0, 100, 0, 1, 0, 2, 3, NULL),
	(24, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(24, 152542, 0, 100, 0, 1, 0, 4, 8, NULL),
	(24, 154165, 0, 100, 0, 1, 0, 6, 16, NULL),
	(24, 153051, 0, 100, 0, 1, 0, 8, 18, NULL),
	(24, 160059, 0, 100, 0, 1, 0, 6, 10, NULL),
	(25, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(25, 152542, 0, 100, 0, 1, 0, 1, 2, NULL),
	(25, 154165, 0, 100, 0, 1, 0, 6, 8, NULL),
	(25, 153051, 0, 100, 0, 1, 0, 10, 14, NULL),
	(25, 160059, 0, 100, 0, 1, 0, 3, 7, NULL),
	(26, 162460, 0, 100, 0, 1, 0, 15, 15, NULL),
	(27, 162461, 0, 100, 0, 1, 0, 25, 25, NULL),
	(28, 162461, 0, 100, 0, 1, 0, 250, 250, NULL),
	(29, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(29, 152512, 0, 100, 0, 1, 0, 1, 2, NULL),
	(29, 160298, 0, 100, 0, 1, 0, 1, 1, NULL),
	(30, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(30, 152513, 0, 10, 0, 1, 0, 2, 7, NULL),
	(30, 152579, 0, 10, 0, 1, 0, 2, 9, NULL),
	(30, 152512, 0, 100, 0, 1, 0, 1, 4, NULL),
	(30, 160298, 0, 100, 0, 1, 0, 1, 1, NULL),
	(31, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(31, 152513, 0, 10, 0, 1, 0, 1, 5, NULL),
	(31, 152579, 0, 10, 0, 1, 0, 2, 5, NULL),
	(31, 152512, 0, 100, 0, 1, 0, 1, 3, NULL),
	(31, 160298, 0, 100, 0, 1, 0, 1, 1, NULL),
	(32, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(32, 152542, 0, 100, 0, 1, 0, 4, 8, NULL),
	(32, 154165, 0, 100, 0, 1, 0, 6, 16, NULL),
	(32, 153051, 0, 100, 0, 1, 0, 8, 18, NULL),
	(32, 160059, 0, 100, 0, 1, 0, 6, 10, NULL),
	(33, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(33, 152542, 0, 100, 0, 1, 0, 1, 2, NULL),
	(33, 154165, 0, 100, 0, 1, 0, 6, 8, NULL),
	(33, 153051, 0, 100, 0, 1, 0, 10, 14, NULL),
	(33, 160059, 0, 100, 0, 1, 0, 3, 7, NULL),
	(34, 162460, 0, 100, 0, 1, 0, 15, 15, NULL),
	(35, 162461, 0, 100, 0, 1, 0, 25, 25, NULL),
	(36, 162461, 0, 100, 0, 1, 0, 250, 250, NULL),
	(37, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(37, 152576, 0, 100, 0, 1, 0, 1, 3, NULL),
	(37, 159959, 0, 100, 0, 1, 0, 1, 5, NULL),
	(38, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(38, 152576, 0, 100, 0, 1, 0, 1, 3, NULL),
	(38, 152577, 0, 5, 0, 1, 0, 4, 6, NULL),
	(38, 158378, 0, 5, 0, 1, 0, 2, 4, NULL),
	(38, 159959, 0, 100, 0, 1, 0, 1, 8, NULL),
	(39, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(39, 152577, 0, 100, 0, 1, 0, 4, 10, NULL),
	(39, 158378, 0, 100, 0, 1, 0, 2, 6, NULL),
	(39, 159959, 0, 100, 0, 1, 0, 3, 9, NULL),
	(40, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(40, 153700, 0, 5, 0, 1, 0, 1, 2, NULL),
	(40, 153701, 0, 5, 0, 1, 0, 1, 2, NULL),
	(40, 153702, 0, 5, 0, 1, 0, 1, 2, NULL),
	(40, 153703, 0, 5, 0, 1, 0, 1, 2, NULL),
	(40, 153704, 0, 5, 0, 1, 0, 1, 2, NULL),
	(40, 153705, 0, 5, 0, 1, 0, 1, 2, NULL),
	(40, 152512, 0, 100, 0, 1, 0, 1, 3, NULL),
	(41, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(41, 154120, 0, 5, 0, 1, 0, 1, 2, NULL),
	(41, 154121, 0, 5, 0, 1, 0, 1, 2, NULL),
	(41, 154122, 0, 5, 0, 1, 0, 1, 2, NULL),
	(41, 154123, 0, 5, 0, 1, 0, 1, 2, NULL),
	(41, 154124, 0, 5, 0, 1, 0, 1, 2, NULL),
	(41, 154125, 0, 5, 0, 1, 0, 1, 2, NULL),
	(41, 152579, 0, 100, 0, 1, 0, 1, 5, NULL),
	(42, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(42, 154120, 0, 10, 0, 1, 0, 1, 2, NULL),
	(42, 154121, 0, 10, 0, 1, 0, 1, 2, NULL),
	(42, 154122, 0, 10, 0, 1, 0, 1, 2, NULL),
	(42, 154123, 0, 10, 0, 1, 0, 1, 2, NULL),
	(42, 154124, 0, 10, 0, 1, 0, 1, 2, NULL),
	(42, 154125, 0, 10, 0, 1, 0, 1, 2, NULL),
	(42, 152513, 0, 100, 0, 1, 0, 1, 5, NULL),
	(43, 162460, 0, 100, 0, 1, 0, 15, 15, NULL),
	(44, 162461, 0, 100, 0, 1, 0, 25, 25, NULL),
	(45, 162461, 0, 100, 0, 1, 0, 250, 250, NULL),
	(46, 152668, 0, 100, 0, 1, 0, 1, 1, NULL),
	(47, 152510, 0, 100, 0, 1, 0, 1, 3, NULL),
	(47, 152507, 0, 100, 0, 1, 0, 1, 12, NULL),
	(47, 152505, 0, 100, 0, 1, 0, 1, 12, NULL),
	(47, 152511, 0, 100, 0, 1, 0, 1, 12, NULL),
	(47, 152509, 0, 100, 0, 1, 0, 1, 8, NULL),
	(47, 152506, 0, 100, 0, 1, 0, 1, 8, NULL),
	(47, 152508, 0, 100, 0, 1, 0, 1, 12, NULL),
	(48, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(48, 158188, 0, 50, 0, 1, 0, 2, 8, NULL),
	(48, 158187, 0, 100, 0, 1, 0, 5, 10, NULL),
	(49, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(49, 158188, 0, 50, 0, 1, 0, 5, 11, NULL),
	(49, 158187, 0, 100, 0, 1, 0, 5, 10, NULL),
	(50, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(50, 158189, 0, 25, 0, 1, 0, 1, 4, NULL),
	(50, 158188, 0, 50, 0, 1, 0, 5, 13, NULL),
	(50, 158187, 0, 100, 0, 1, 0, 5, 10, NULL),
	(51, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(51, 11291, 0, 50, 0, 1, 0, 1, 1, NULL),
	(51, 152875, 0, 100, 0, 1, 0, 1, 3, NULL),
	(52, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(52, 11291, 0, 50, 0, 1, 0, 1, 1, NULL),
	(52, 152875, 0, 100, 0, 1, 0, 1, 3, NULL),
	(53, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(53, 11291, 0, 50, 0, 1, 0, 1, 1, NULL),
	(53, 152876, 0, 100, 0, 1, 0, 1, 1, NULL),
	(54, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(54, 153050, 0, 80, 0, 1, 0, 1, 4, NULL),
	(54, 154164, 0, 80, 0, 1, 0, 1, 2, NULL),
	(54, 152512, 0, 100, 0, 1, 0, 1, 3, NULL),
	(55, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(55, 154165, 0, 80, 0, 1, 0, 5, 10, NULL),
	(55, 153051, 0, 80, 0, 1, 0, 5, 10, NULL),
	(55, 160059, 0, 100, 0, 1, 0, 4, 10, NULL),
	(56, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(56, 154165, 0, 80, 0, 1, 0, 9, 12, NULL),
	(56, 153051, 0, 80, 0, 1, 0, 7, 17, NULL),
	(56, 160059, 0, 100, 0, 1, 0, 4, 15, NULL),
	(57, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(57, 153700, 0, 5, 0, 1, 0, 1, 2, NULL),
	(57, 153701, 0, 5, 0, 1, 0, 1, 2, NULL),
	(57, 153702, 0, 5, 0, 1, 0, 1, 2, NULL),
	(57, 153703, 0, 5, 0, 1, 0, 1, 2, NULL),
	(57, 153704, 0, 5, 0, 1, 0, 1, 2, NULL),
	(57, 153705, 0, 5, 0, 1, 0, 1, 2, NULL),
	(57, 152512, 0, 100, 0, 1, 0, 1, 3, NULL),
	(58, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(58, 154120, 0, 5, 0, 1, 0, 1, 2, NULL),
	(58, 154121, 0, 5, 0, 1, 0, 1, 2, NULL),
	(58, 154122, 0, 5, 0, 1, 0, 1, 2, NULL),
	(58, 154123, 0, 5, 0, 1, 0, 1, 2, NULL),
	(58, 154124, 0, 5, 0, 1, 0, 1, 2, NULL),
	(58, 154125, 0, 5, 0, 1, 0, 1, 2, NULL),
	(58, 152579, 0, 100, 0, 1, 0, 1, 5, NULL),
	(59, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(59, 154120, 0, 10, 0, 1, 0, 1, 2, NULL),
	(59, 154121, 0, 10, 0, 1, 0, 1, 2, NULL),
	(59, 154122, 0, 10, 0, 1, 0, 1, 2, NULL),
	(59, 154123, 0, 10, 0, 1, 0, 1, 2, NULL),
	(59, 154124, 0, 10, 0, 1, 0, 1, 2, NULL),
	(59, 154125, 0, 10, 0, 1, 0, 1, 2, NULL),
	(59, 152579, 0, 100, 0, 1, 0, 1, 5, NULL),
	(60, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(60, 152512, 0, 100, 0, 1, 0, 1, 2, NULL),
	(60, 160298, 0, 100, 0, 1, 0, 1, 1, NULL),
	(61, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(61, 152513, 0, 10, 0, 1, 0, 1, 5, NULL),
	(61, 152579, 0, 10, 0, 1, 0, 2, 5, NULL),
	(61, 152512, 0, 100, 0, 1, 0, 1, 3, NULL),
	(61, 160298, 0, 100, 0, 1, 0, 1, 1, NULL),
	(62, 152668, 0, 16, 0, 1, 0, 1, 1, NULL),
	(62, 152542, 0, 100, 0, 1, 0, 1, 2, NULL),
	(62, 154165, 0, 100, 0, 1, 0, 6, 8, NULL),
	(62, 153051, 0, 100, 0, 1, 0, 10, 14, NULL),
	(62, 160059, 0, 100, 0, 1, 0, 3, 7, NULL),
	(63, 152512, 0, 100, 0, 1, 0, 1, 3, ''),
	(63, 152513, 0, 10, 0, 1, 0, 1, 5, ''),
	(63, 152579, 0, 10, 0, 1, 0, 2, 5, ''),
	(63, 152668, 0, 16, 0, 1, 0, 1, 1, ''),
	(63, 160298, 0, 100, 0, 1, 0, 1, 1, '');
	
ALTER TABLE `scrapping_loot_template` ADD `ItemType` tinyint NOT NULL DEFAULT 0 AFTER `Entry`;

REPLACE INTO `creature_model_info` VALUES (105540, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (105328, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (105213, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (112642, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (113958, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (114211, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (106003, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (107058, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (107056, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (105268, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (115279, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (115281, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (116539, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (116687, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (126177, 0, 0, 0, 0);
REPLACE INTO `creature_model_info` VALUES (113609, 0, 0, 0, 0);

-- ----------------------------
-- Warlock spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (5782, 'spell_warl_fear');
REPLACE INTO `spell_script_names` VALUES (204730, 'spell_warl_fear_buff');
REPLACE INTO `spell_script_names` VALUES (146739, 'spell_warl_corruption_effect');
REPLACE INTO `spell_script_names` VALUES (234153, 'spell_warlock_drain_life');
REPLACE INTO `spell_script_names` VALUES (205179, 'aura_warl_phantomatic_singularity');
REPLACE INTO `spell_script_names` VALUES (48181, 'aura_warl_haunt');
REPLACE INTO `spell_script_names` VALUES (205180, 'spell_warlock_summon_darkglare');

REPLACE INTO `creature_template` VALUES (103673, 0, 0, 'Darkglare', '', '', NULL, '', 0, 0, 954, 0, 1, 0.857143, 1, 0, 0, 1477, 2000, 1, 1, 8, 32768, 2048, 0, 19, 0, 3, 0, '', 0, 1, 0, 121, 0, 0, 1, 0, 0, 'npc_pet_warlock_darkglare', NULL, 53040);

REPLACE INTO `spell_script_names` VALUES (5697, 'spell_warlock_unending_breath');
REPLACE INTO `spell_script_names` VALUES (111771, 'spell_warl_demonic_gateway');

REPLACE INTO `creature_template` VALUES (59262, 0, 0, 'Demonic Gateway', '', NULL, '', 'interact', 0, 0, 35, 16777216, 1, 1.14286, 1, 0, 0, 2000, 2000, 1, 1, 1, 512, 67108896, 17825793, 0, 0, 11, 0, '', 0, 1, 0, 0, 0, 0, 1, 0, 0, 'npc_warl_demonic_gateway', NULL, 53040);
REPLACE INTO `creature_template` VALUES (59271, 0, 0, 'Demonic Gateway', '', NULL, '', 'interact', 0, 0, 35, 16777216, 1, 1.14286, 1, 0, 0, 2000, 2000, 1, 1, 1, 512, 67108896, 17825793, 0, 0, 11, 0, '', 0, 1, 0, 0, 0, 0, 1, 0, 0, 'npc_warl_demonic_gateway', NULL, 53040);

REPLACE INTO `npc_spellclick_spells` VALUES (59262, 113902, 0, 0);
REPLACE INTO `npc_spellclick_spells` VALUES (59271, 113902, 0, 0);

REPLACE INTO `spell_script_names` VALUES (105174, 'spell_warl_hand_of_guldan');
REPLACE INTO `spell_script_names` VALUES (86040, 'spell_warl_hand_of_guldan_damage');
REPLACE INTO `spell_script_names` VALUES (104316, 'spell_warlock_call_dreadstalkers');

REPLACE INTO `creature_template` VALUES (98035, 0, 0, 'Dreadstalker', '', '', NULL, '', 0, 0, 90, 0, 1, 1.28571, 1, 6, 0, 1444, 2000, 1, 1, 4, 0, 0, 0, 0, 0, 3, 0, '', 0, 1, 0, 133, 0, 0, 1, 0, 0, 'npc_warlock_dreadstalker', NULL, 53040);

REPLACE INTO `spell_script_names` VALUES (205231, 'spell_warl_eye_laser');
REPLACE INTO `spell_script_names` VALUES (264178, 'spell_warlock_demonbolt_new');
REPLACE INTO `spell_script_names` VALUES (205145, 'spell_warl_demonic_calling');
REPLACE INTO `spell_script_names` VALUES (157695, 'spell_warl_demonbolt');
REPLACE INTO `spell_script_names` VALUES (196277, 'spell_warl_implosion');
REPLACE INTO `spell_script_names` VALUES (603, 'spell_warlock_doom');
REPLACE INTO `spell_script_names` VALUES (6353, 'spell_warlock_soul_fire');
REPLACE INTO `spell_script_names` VALUES (215941, 'spell_warl_soul_conduit');
REPLACE INTO `spell_script_names` VALUES (232670, 'spell_warr_shadowbolt_affliction');
REPLACE INTO `spell_script_names` VALUES (104318, 'spell_warlock_fel_firebolt_wild_imp');

REPLACE INTO `creature_template` VALUES (55659, 0, 0, 'Wild Imps', '', '', NULL, '', 0, 0, 90, 0, 1, 1.14286, 1, 6, 0, 0, 2000, 1, 1, 4, 0, 0, 0, 0, 0, 3, 0, '', 0, 1, 0, 121, 0, 0, 1, 0, 0, 'npc_pet_warlock_wild_imp', NULL, 64743);

REPLACE INTO `spell_script_names` VALUES (386344, 'spell_warlock_inquisitors_gaze');
REPLACE INTO `spell_script_names` VALUES (29722, 'spell_warl_incinerate');
REPLACE INTO `spell_script_names` VALUES (980, 'spell_warlock_agony');
REPLACE INTO `spell_script_names` VALUES (3110, 'spell_warlock_imp_firebolt');
REPLACE INTO `spell_script_names` VALUES (17962, 'spell_warl_conflagrate');
REPLACE INTO `spell_script_names` VALUES (17962, 'spell_warl_conflagrate_aura');
REPLACE INTO `spell_script_names` VALUES (152108, 'spell_warl_cataclysm');

-- ----------------------------
-- Warrior spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (23920, 'spell_warr_spell_reflect');
REPLACE INTO `spell_script_names` VALUES (228920, 'spell_warr_ravager');
REPLACE INTO `spell_script_names` VALUES (152277, 'spell_warr_ravager');

REPLACE INTO `creature_template` VALUES (76168, 0, 0, 'Ravager', '', '', NULL, '', 0, 0, 35, 0, 2, 0.714286, 1, 0, 0, 2000, 2000, 1, 1, 1, 33587200, 67667968, 16809984, 0, 0, 10, 0, '', 0, 1, 0, 94, 0, 0, 1, 0, 0, 'npc_warr_ravager', NULL, 53040);

REPLACE INTO `spell_script_names` VALUES (156287, 'spell_warr_ravager_damage');
REPLACE INTO `spell_script_names` VALUES (190456, 'spell_warr_ignore_pain');
REPLACE INTO `spell_script_names` VALUES (12975,	'spell_warr_last_stand');
REPLACE INTO `spell_script_names` VALUES (64380, 'spell_warr_shattering_throw');
REPLACE INTO `spell_script_names` VALUES (65941, 'spell_warr_shattering_throw');
REPLACE INTO `spell_script_names` VALUES (469, 'spell_warr_shout');
REPLACE INTO `spell_script_names` VALUES (6673, 'spell_warr_shout');
REPLACE INTO `spell_script_names` VALUES (1680, 'spell_warr_wirlwind_dmg');
REPLACE INTO `spell_script_names` VALUES (385952, 'spell_warr_shiel_charge');
REPLACE INTO `spell_script_names` VALUES (190411, 'spell_warr_whirlwind');
REPLACE INTO `spell_script_names` VALUES (76856, 'spell_warr_unshackled_fury');
REPLACE INTO `spell_script_names` VALUES (184361, 'spell_warr_enrage');
REPLACE INTO `spell_script_names` VALUES (184362, 'spell_warr_enrage_aura');
REPLACE INTO `spell_script_names` VALUES (215571, 'spell_warr_frothing_berserker');
REPLACE INTO `spell_script_names` VALUES (214871, 'spell_warr_odyns_fury');
REPLACE INTO `spell_script_names` VALUES (7384, 'spell_warr_overpower');
REPLACE INTO `spell_script_names` VALUES (6343, 'spell_warr_thunder_clap');
REPLACE INTO `spell_script_names` VALUES (845, 'spell_warr_cleave_dmg');

-- ----------------------------
-- Mage spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (205025, 'spell_mage_presence_of_mind');

REPLACE INTO `areatrigger_scripts` VALUES (1612, 'at_mage_arcane_orb');
REPLACE INTO `areatrigger_scripts` VALUES (8661,'at_mage_frozen_orb');

REPLACE INTO `spell_script_names` VALUES (148022, 'spell_mage_icicle_damage');
REPLACE INTO `spell_script_names` VALUES (76613, 'spell_mastery_icicles_proc');
REPLACE INTO `spell_script_names` VALUES (148023, 'spell_mastery_icicles_periodic');
REPLACE INTO `spell_script_names` VALUES (214124, 'spell_mastery_icicles_mod_aura');
REPLACE INTO `spell_script_names` VALUES (214125, 'spell_mastery_icicles_mod_aura');
REPLACE INTO `spell_script_names` VALUES (214126, 'spell_mastery_icicles_mod_aura');
REPLACE INTO `spell_script_names` VALUES (214127, 'spell_mastery_icicles_mod_aura');
REPLACE INTO `spell_script_names` VALUES (214130, 'spell_mastery_icicles_mod_aura');
REPLACE INTO `spell_script_names` VALUES (199786, 'spell_mastery_icicles_glacial_spike');
REPLACE INTO `spell_script_names` VALUES  (2120, 'spell_mage_flamestrike');

-- ----------------------------
-- DK spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (48792,	'spell_dk_icebound_fortitude');
REPLACE INTO `spell_script_names` VALUES (196782, 'aura_dk_outbreak_periodic');
REPLACE INTO `spell_script_names` VALUES (115994, 'spell_dk_unholy_blight');
REPLACE INTO `spell_script_names` VALUES (55090, 'spell_dk_scourge_strike');
REPLACE INTO `spell_script_names` VALUES (152280, 'aura_dk_defile');

REPLACE INTO `areatrigger_scripts` VALUES (6212, 'at_dk_defile');

REPLACE INTO `creature_template` VALUES (82521, 0, 0, 'Defile', '', '', NULL, '', 0, 0, 1, 0, 1, 1, 1, 0, 0, 2000, 2000, 1, 1, 1, 17826816, 16, 0, 0, 0, 10, 0, '', 0, 1, 0, 0, 0, 0, 1, 0, 0, 'npc_dk_defile', NULL, 53040);

REPLACE INTO `spell_script_names` VALUES (156004, 'spell_dk_defile_aura');
REPLACE INTO `spell_script_names` VALUES (215969, 'spell_dk_epidemic');
REPLACE INTO `spell_script_names` VALUES (215969, 'spell_dk_epidemic_aoe');

-- ----------------------------
-- Evoker spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (369536, 'spell_evo_soar');
REPLACE INTO `spell_script_names` VALUES (351239, 'spell_evo_cosmic_visage');
REPLACE INTO `spell_script_names` VALUES (359073, 'spell_evo_eternity_surge');

-- ----------------------------
-- Hunter spell fixes
-- ----------------------------
REPLACE INTO `areatrigger_create_properties` VALUES (4424, 0, 9170, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 60000, 4, 3, 3, 4, 4, 1, 1, 0, 0, 'at_hunter_freezing_trap', 48999);
REPLACE INTO `areatrigger_create_properties` VALUES (510, 0, 3678, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 20000, 4, 10, 10, 7, 7, 3, 3, 0, 0, 'at_hun_flare', 40120);

REPLACE INTO `spell_script_names` VALUES (34026, 'spell_hun_kill_command');
REPLACE INTO `spell_script_names` VALUES (259489, 'spell_hun_kill_command');
REPLACE INTO `spell_script_names` VALUES (83381, 'spell_hun_kill_command_proc');
REPLACE INTO `spell_script_names` VALUES (19577, 'spell_hun_intimidation');
REPLACE INTO `spell_script_names` VALUES (19574, 'spell_hun_bestial_wrath');
REPLACE INTO `spell_script_names` VALUES (217200, 'spell_hun_barbed_shot');
REPLACE INTO `spell_script_names` VALUES (120679, 'spell_hun_dire_beast');

REPLACE INTO `spell_script_names` VALUES (321530, 'spell_hun_bloodshed');
REPLACE INTO `spell_script_names` VALUES (260243, 'spell_hun_volley');
REPLACE INTO `spell_script_names` VALUES (186387, 'spell_bursting_shot');

REPLACE INTO `areatrigger_create_properties` VALUES (2392, 0, 6966, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 60000, 4, 3, 3, 4, 4, 1, 1, 0, 0, 'at_hun_steel_trap', 48999);

REPLACE INTO `spell_script_names` VALUES (120361, 'spell_hun_barrage');
REPLACE INTO `spell_script_names` VALUES (269751, 'spell_hun_flanking_strike');
REPLACE INTO `spell_script_names` VALUES (360966, 'spell_hun_spearhead');
REPLACE INTO `spell_script_names` VALUES (259495, 'spell_hunter_wildfire_bomb');
REPLACE INTO `spell_script_names` VALUES (271014, 'spell_hunter_wildfire_infusion_talent');
REPLACE INTO `spell_script_names` VALUES (271615, 'spell_hunter_wildfire_infusion_dummy');

REPLACE INTO `areatrigger_scripts` VALUES (12863, 'at_hunter_wildfire_bomb');

REPLACE INTO `areatrigger_template` VALUES (265163, 0, 0, 0, 0, 0);

REPLACE INTO `areatrigger_create_properties` VALUES (12863, 0, 265163, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 'at_hunter_wildfire_bomb', 0);

REPLACE INTO `spell_script_names` VALUES (3355, 'spell_hunter_freezing_trap_aura');

-- ----------------------------
-- Rogue spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (115191, 'spell_rog_stealth_with_subterfuge');
REPLACE INTO `spell_script_names` VALUES (115191, 'spell_rog_stealth');
REPLACE INTO `spell_script_names` VALUES (14062, 'spell_rog_nightstalker');
REPLACE INTO `spell_script_names` VALUES (5171, 'spell_rog_slice_and_dice');
REPLACE INTO `spell_script_names` VALUES (315496, 'spell_rog_slice_and_dice');
REPLACE INTO `spell_script_names` VALUES (385616, 'spell_rog_echoing_reprimand');
REPLACE INTO `spell_script_names` VALUES (2823, 'spell_rog_deadly_poison_instant_damage');
REPLACE INTO `spell_script_names` VALUES (51723, 'spell_rog_fan_of_knives');
REPLACE INTO `spell_script_names` VALUES (195457, 'spell_rog_grappling_hook');
REPLACE INTO `spell_script_names` VALUES (271877, 'spell_rog_blade_rush');
REPLACE INTO `spell_script_names` VALUES (197835, 'spell_rog_shuriken_storm');
REPLACE INTO `spell_script_names` VALUES (280719, 'spell_rog_secret_technique');
REPLACE INTO `spell_script_names` VALUES (91023, 'spell_rog_find_weakness');

-- ----------------------------
-- Priest spell fixes
-- ----------------------------
REPLACE INTO `spell_script_names` VALUES (186723, 'spell_pri_penance_620');
REPLACE INTO `spell_script_names` VALUES (271466, 'spell_pri_luminous_barrier');
REPLACE INTO `spell_script_names` VALUES (108945, 'spell_pri_angelic_bulwark');
REPLACE INTO `spell_script_names` VALUES (8122, 'spell_pri_mental_scream');
REPLACE INTO `spell_script_names` VALUES (194249, 'spell_pri_voidform');
REPLACE INTO `spell_script_names` VALUES (228260, 'spell_pri_void_eruption');
REPLACE INTO `spell_script_names` VALUES (335467, 'spell_pri_devouring_plague');
REPLACE INTO `spell_script_names` VALUES (108920, 'spell_pri_void_tendrils');
REPLACE INTO `spell_script_names` VALUES (421453, 'spell_ultimate_penitence');
REPLACE INTO `spell_script_names` VALUES (421434, 'spell_ultimate_penitence_effect');
REPLACE INTO `spell_script_names` VALUES (432154, 'spell_ultimate_penitence_jump');

REPLACE INTO `areatrigger_create_properties` VALUES (1489, 0, 5802, 0, 0, 0, 0, 0, 0, -256, 255, 0, NULL, 10000, 1, 0 ,0 ,8 ,8 ,0 ,0 ,0 ,0 ,0 ,0 ,'at_pri_power_word_barrier', 53040);
REPLACE INTO `areatrigger_scripts` VALUES (1489, 'at_pri_power_word_barrier');

-- ----------------------------
-- Paladin spell fixes
-- ----------------------------
REPLACE INTO `spell_script_names` VALUES (85043, 'spell_pal_grand_crusader');
REPLACE INTO `spell_script_names` VALUES (152261, 'spell_pal_holy_shield');

-- ----------------------------
-- Monk spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (126892, 'spell_monk_zen_pilgrimage');
REPLACE INTO `spell_script_names` VALUES (293866, 'spell_monk_zen_pilgrimage');
REPLACE INTO `spell_script_names` VALUES (126895, 'spell_monk_zen_pilgrimage_return');
REPLACE INTO `spell_script_names` VALUES (100780, 'spell_monk_tiger_palm');
REPLACE INTO `spell_script_names` VALUES (125883, 'spell_monk_zen_flight_check');
REPLACE INTO `spell_script_names` VALUES (116095, 'spell_monk_disable');
REPLACE INTO `spell_script_names` VALUES (116705, 'spell_monk_spear_hand_strike');
REPLACE INTO `spell_script_names` VALUES (115203, 'spell_monk_fortifying_brew');
REPLACE INTO `spell_script_names` VALUES (119582, 'spell_monk_purifying_brew');
REPLACE INTO `spell_script_names` VALUES (115181, 'spell_monk_breath_of_fire');
REPLACE INTO `spell_script_names` VALUES (122278, 'spell_monk_dampen_harm');
REPLACE INTO `spell_script_names` VALUES (122280, 'spell_monk_healing_elixirs_aura');

REPLACE INTO `areatrigger_scripts` VALUES (3983, 'at_monk_ring_of_peace');
REPLACE INTO `areatrigger_scripts` VALUES (5484, 'at_monk_song_of_chi_ji');

REPLACE INTO `areatrigger_create_properties` VALUES (718, 0, 3983, 0, 0, 0, 0, 0, 0, -256, 255, 0, 0, 0, 5000, 0, 8, 8, 0, 0, 0, 0, 0, 0, 'at_monk_ring_of_peace', 40120);
REPLACE INTO `areatrigger_create_properties` VALUES (5484, 0, 10191, 0, 0, 0, 1656, 0, 0, -1, 0, 0, 0, 5716, 5000, 0, 5, 5, 0, 0, 0, 0, 0, 0, 'at_monk_song_of_chi_ji', 48838);

REPLACE INTO `spell_script_names` VALUES (115098, 'spell_monk_chi_wave');
REPLACE INTO `spell_script_names` VALUES (132467, 'spell_monk_chi_wave_damage_missile');
REPLACE INTO `spell_script_names` VALUES (132464, 'spell_monk_chi_wave_heal_missile');
REPLACE INTO `spell_script_names` VALUES (132464, 'spell_monk_chi_wave_healing_bolt');

REPLACE INTO `areatrigger_scripts` VALUES (5300, 'at_monk_chi_burst');

REPLACE INTO `areatrigger_create_properties` VALUES (1316, 0, 5300, 0, 0, 392, 0, 0, 0, -1, 0, 0, 0, 931, 1000, 0, 5, 5, 0, 0, 0, 0, 0, 0, 'at_monk_chi_burst', 40120);

REPLACE INTO `spell_script_names` VALUES (130654, 'spell_monk_chi_burst_heal');
REPLACE INTO `spell_script_names` VALUES (115399, 'spell_monk_black_ox_brew');
REPLACE INTO `spell_script_names` VALUES (115313, 'spell_monk_jade_serpent_statue');

REPLACE INTO `creature_template` VALUES (60849, 0, 0, 'Jade Serpent Statue', '', '', NULL, '', 0, 0, 5, 0, 1, 1.14286, 1, 0, 0, 1798, 2000, 1, 1, 1, 512, 32800, 0, 0, 0, 11, 0, '', 0, 1, 0, 0, 0, 0, 1, 0, 0, 'npc_monk_jade_serpent_statue', NULL, 53040);

REPLACE INTO `spell_script_names` VALUES (101643, 'spell_monk_transcendence');
REPLACE INTO `spell_script_names` VALUES (101643, 'aura_monk_transcendence');
REPLACE INTO `spell_script_names` VALUES (119996, 'spell_monk_transcendence_transfer');
REPLACE INTO `spell_script_names` VALUES (116670, 'spell_monk_vivify');
REPLACE INTO `spell_script_names` VALUES (115175, 'spell_monk_soothing_mist');
REPLACE INTO `spell_script_names` VALUES (116680, 'spell_focused_thunder_talent_thunder_focus_tea');
REPLACE INTO `spell_script_names` VALUES (124081, 'spell_monk_zen_pulse');
REPLACE INTO `spell_script_names` VALUES (115151, 'spell_monk_renewing_mist');
REPLACE INTO `spell_script_names` VALUES (115151, 'spell_monk_renewing_mist_hot');
REPLACE INTO `spell_script_names` VALUES (119607, 'spell_monk_renewing_mist_jump');
REPLACE INTO `spell_script_names` VALUES (191837, 'spell_monk_essence_font');
REPLACE INTO `spell_script_names` VALUES (191840, 'spell_monk_essence_font_heal');
REPLACE INTO `spell_script_names` VALUES (124682, 'spell_monk_enveloping_mist');
REPLACE INTO `spell_script_names` VALUES (122470, 'spell_monk_touch_of_karma');
REPLACE INTO `spell_script_names` VALUES (125174, 'spell_monk_touch_of_karma_buff');
REPLACE INTO `spell_script_names` VALUES (113656, 'spell_monk_fists_of_fury');
REPLACE INTO `spell_script_names` VALUES (117418, 'spell_monk_fists_of_fury_damage');
REPLACE INTO `spell_script_names` VALUES (120086, 'spell_monk_fists_of_fury_stun');
REPLACE INTO `spell_script_names` VALUES (123154, 'spell_monk_fists_of_fury_visual');
REPLACE INTO `spell_script_names` VALUES (117418, 'spell_monk_fists_of_fury_visual_filter');

REPLACE INTO `creature_template` VALUES (63508, 0, 0, 'Xuen', '', NULL, NULL, NULL, 0, 0, 35, 0, 1, 1.54286, 1, 0, 0, 721, 2000, 1, 1, 1, 32784, 2048, 0, 0, 0, 1, 0, '', 0, 1, 0, 121, 0, 0, 1, 0, 0, 'npc_monk_xuen', NULL, 53040);

REPLACE INTO `spell_script_names` VALUES (115057, 'spell_monk_flying_serpent_kick');

REPLACE INTO `spell_linked_spell` VALUES (113656, 165144, 0, 'Fists of Fury visual fix');
REPLACE INTO `spell_linked_spell` VALUES (324312, 324383, 0, 'Monk Clash spell fix');
REPLACE INTO `spell_linked_spell` VALUES (152175, 158221, 0, 'Monk Whirling Dragon Punch Damage');

REPLACE INTO `spell_script_names` VALUES (113656, 'spell_fist_of_fury_proc');
REPLACE INTO `spell_script_names` VALUES (107428, 'spell_rising_sun_kick_proc');
REPLACE INTO `spell_script_names` VALUES (152175, 'spell_monk_whirling_dragon_punch');

-- ----------------------------
-- Shaman spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (192249, 'spell_shaman_generic_summon_elemental');
REPLACE INTO `spell_script_names` VALUES (198103, 'spell_sha_earth_elemental');
REPLACE INTO `spell_script_names` VALUES (198067, 'spell_sha_fire_elemental');
REPLACE INTO `spell_script_names` VALUES (196884, 'spell_sha_feral_lunge');
REPLACE INTO `spell_script_names` VALUES (51533, 'spell_sha_feral_spirit');

REPLACE INTO `creature_template` VALUES (29264, 0, 0, 'Spirit Wolf', '', NULL, NULL, NULL, 0, 0, 14, 0, 2, 2.14286, 1, 0, 0, 1500, 2000, 1, 1, 1, 0, 2048, 0, 0, 0, 1, 0, '', 0, 0, 0, 191, 0, 0, 1, 0, 64, 'npc_feral_spirit', NULL, 64743);

-- ----------------------------
-- DH spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (195072, 'spell_dh_fel_rush');
REPLACE INTO `spell_script_names` VALUES (197923, 'spell_dh_fel_rush_dash');
REPLACE INTO `spell_script_names` VALUES (197922, 'spell_dh_fel_rush_dash_ground');
REPLACE INTO `spell_script_names` VALUES (223107, 'spell_dh_fel_rush_damage');
REPLACE INTO `spell_script_names` VALUES (203556, 'spell_dh_master_of_the_glaive');
REPLACE INTO `spell_script_names` VALUES (211881, 'spell_dh_fel_eruption');
REPLACE INTO `spell_script_names` VALUES (209258, 'spell_dh_last_resort');
REPLACE INTO `spell_script_names` VALUES (191427, 'spell_dh_metamorphosis');
REPLACE INTO `spell_script_names` VALUES (201453, 'spell_dh_metamorphosis_immunity');
REPLACE INTO `spell_script_names` VALUES (206473, 'spell_dh_bloodlet');
REPLACE INTO `spell_script_names` VALUES (211053, 'spell_dh_fel_barrage');
REPLACE INTO `spell_script_names` VALUES (222703, 'spell_dh_fel_barrage_damage');
REPLACE INTO `spell_script_names` VALUES (222703, 'spell_dh_fel_barrage_aura');
REPLACE INTO `spell_script_names` VALUES (206491, 'spell_dh_nemesis');
REPLACE INTO `spell_script_names` VALUES (189110, 'spell_dh_infernal_strike');
REPLACE INTO `spell_script_names` VALUES (228477, 'spell_dh_soul_cleave');
REPLACE INTO `spell_script_names` VALUES (228478, 'spell_dh_soul_cleave_damage');
REPLACE INTO `spell_script_names` VALUES (204021, 'spell_dh_fiery_brand');
REPLACE INTO `spell_script_names` VALUES (207771, 'spell_dh_fiery_brand_dot');
REPLACE INTO `spell_script_names` VALUES (209400, 'spell_dh_razor_spikes');
REPLACE INTO `spell_script_names` VALUES (263648, 'spell_dh_soul_barrier');
REPLACE INTO `spell_script_names` VALUES (207810, 'spell_dh_nether_bond');
REPLACE INTO `spell_script_names` VALUES (207811, 'spell_dh_nether_bond_periodic');
REPLACE INTO `spell_script_names` VALUES (211509, 'spell_dh_solitude');
REPLACE INTO `spell_script_names` VALUES (205598, 'spell_dh_awaken_the_demon');
REPLACE INTO `spell_script_names` VALUES (205604, 'spell_dh_reverse_magic');
REPLACE INTO `spell_script_names` VALUES (206649, 'spell_dh_eye_of_leotheras');
REPLACE INTO `spell_script_names` VALUES (258920, 'spell_dh_immolation_aura');
REPLACE INTO `spell_script_names` VALUES (258922, 'spell_dh_immolation_aura_damage');
REPLACE INTO `spell_script_names` VALUES (208796, 'spell_dh_jagged_spikes');
REPLACE INTO `spell_script_names` VALUES (206966, 'spell_dh_fel_lance');
REPLACE INTO `spell_script_names` VALUES (206891, 'spell_dh_intimidated');
REPLACE INTO `spell_script_names` VALUES (203753, 'spell_dh_blade_turning');
REPLACE INTO `spell_script_names` VALUES (203783, 'spell_dh_shear_proc');
REPLACE INTO `spell_script_names` VALUES (210047, 'spell_dh_consume_soul_missile');
REPLACE INTO `spell_script_names` VALUES (209426, 'spell_dh_darkness_absorb');
REPLACE INTO `spell_script_names` VALUES (203650, 'spell_dh_prepared');
REPLACE INTO `spell_script_names` VALUES (206478, 'spell_dh_demonic_appetite');
REPLACE INTO `spell_script_names` VALUES (228532, 'spell_dh_soul_fragment_heals');
REPLACE INTO `spell_script_names` VALUES (206475, 'spell_dh_chaos_cleave');
REPLACE INTO `spell_script_names` VALUES (205411, 'spell_dh_desperate_instincts');
REPLACE INTO `spell_script_names` VALUES (209795, 'spell_dh_fracture');
REPLACE INTO `spell_script_names` VALUES (224509, 'spell_dh_frailty');
REPLACE INTO `spell_script_names` VALUES (247454, 'spell_dh_spirit_bomb');
REPLACE INTO `spell_script_names` VALUES (236189, 'spell_dh_demonic_infusion');
REPLACE INTO `spell_script_names` VALUES (206803, 'spell_demon_hunter_rain_from_above');
REPLACE INTO `spell_script_names` VALUES (162264, 'spell_demon_hunter_metamorphosis_buffs');
REPLACE INTO `spell_script_names` VALUES (212105, 'spell_dh_fel_devastation_damage');
REPLACE INTO `spell_script_names` VALUES (162794, 'spell_demon_hunter_chaos_strike');
REPLACE INTO `spell_script_names` VALUES (185244, 'spell_demon_hunter_pain');
REPLACE INTO `spell_script_names` VALUES (203704, 'spell_demon_hunter_mana_break');
REPLACE INTO `spell_script_names` VALUES (258881, 'spell_demon_hunter_trail_of_ruin');
REPLACE INTO `spell_script_names` VALUES (213480, 'spell_demon_hunter_unending_hatred');
REPLACE INTO `spell_script_names` VALUES (203720, 'spell_dh_demon_spikes');
REPLACE INTO `spell_script_names` VALUES (201427, 'spell_dh_annihilation');
REPLACE INTO `spell_script_names` VALUES (344862, 'spell_dh_chaos_strike_specless');
REPLACE INTO `spell_script_names` VALUES (344865, 'spell_dh_fel_rush_specless');

REPLACE INTO `areatrigger_scripts` VALUES (6615, 'at_dh_darkness');
REPLACE INTO `areatrigger_scripts` VALUES (9695, 'at_demon_hunter_mana_rift');
REPLACE INTO `areatrigger_scripts` VALUES (6482, 'at_demon_hunter_demonic_trample');

REPLACE INTO `areatrigger_create_properties` VALUES (6482, 1, 11107, 1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'at_demon_hunter_demonic_trample', 40120);

-- ----------------------------
-- Druid spell fixes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (103917, 'spell_dru_blessing_of_elune');
REPLACE INTO `spell_script_names` VALUES (194153, 'spell_dru_lunar_strike');
REPLACE INTO `spell_script_names` VALUES (190984, 'spell_dru_solar_wrath');
REPLACE INTO `spell_script_names` VALUES (106785, 'spell_dru_swipe');
REPLACE INTO `spell_script_names` VALUES (202028, 'spell_dru_brutal_slash');
REPLACE INTO `spell_script_names` VALUES (202157, 'aura_dru_feral_affinity');
REPLACE INTO `spell_script_names` VALUES (159286, 'spell_dru_primal_fury');

-- ----------------------------
-- Misc fix or changes
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (108897, 'spell_pandaren_faction_choice');
REPLACE INTO `spell_script_names` VALUES (83958, 'spell_gen_guild_chest');

REPLACE INTO `creature_template` VALUES (102199, 0, 0, 'Stampede', '', '', NULL, '',	0, 0, 35, 0, 1, 1.14286, 1, 0, 0, 1028, 2000, 1, 1, 1, 33554944, 0, 16777216, 0, 0, 10, 0, '', 0, 1, 0, 0, 0, 0, 1, 0, 0, '', NULL, 53040);
REPLACE INTO `creature_template` VALUES (73967, 0, 0, 'Niuzao', '', '', NULL, '', 0, 0, 35, 0, 1, 0.857143, 1, 0, 0, 1989, 2000, 1, 1, 1, 32768, 2048, 0, 0, 0, 1, 0, '', 0, 1, 0, 121, 0, 0, 1, 0, 0, '', NULL, 53040);

ALTER TABLE `scrapping_loot_template` ADD `ItemType` tinyint NOT NULL DEFAULT 0 AFTER `Entry`;

-- ----------------------------
-- Toys fix
-- ----------------------------

REPLACE INTO `spell_script_names` VALUES (340110, 'spell_necronom_i_nom');
REPLACE INTO `spell_script_names` VALUES (291106, 'spell_bowl_of_glowing_pufferfish');
REPLACE INTO `spell_script_names` VALUES (232592, 'spell_twelve_string_guitar');

-- ----------------------------
-- Advanced fly
-- ----------------------------

REPLACE INTO `spell_linked_spell` VALUES (372771, 372773, 0, 'Dragonriding energy aura');

REPLACE INTO `spell_script_names` VALUES (373646, 'spell_af_skyriding');
REPLACE INTO `spell_script_names` VALUES (406095, 'spell_af_skyriding');
REPLACE INTO `spell_script_names` VALUES (430747, 'spell_af_skyriding');
REPLACE INTO `spell_script_names` VALUES (430833, 'spell_af_skyriding');
REPLACE INTO `spell_script_names` VALUES (372771, 'spell_af_energy');
REPLACE INTO `spell_script_names` VALUES (372773, 'spell_af_energy');
REPLACE INTO `spell_script_names` VALUES (372610, 'spell_af_skyward_ascent');
REPLACE INTO `spell_script_names` VALUES (374763, 'spell_af_skyward_ascent');
REPLACE INTO `spell_script_names` VALUES (386451, 'spell_af_skyward_ascent');
REPLACE INTO `spell_script_names` VALUES (372608, 'spell_af_surge_forward');
REPLACE INTO `spell_script_names` VALUES (386449, 'spell_af_surge_forward');
REPLACE INTO `spell_script_names` VALUES (361584, 'spell_af_whirling_surge');
REPLACE INTO `spell_script_names` VALUES (436854, 'spell_switch_flight');