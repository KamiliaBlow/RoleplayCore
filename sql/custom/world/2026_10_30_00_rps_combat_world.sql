-- RPS WORLD package, 2026-10-30.
-- Select this database explicitly; stop writers; never use --force.
-- Apply the committed RPS baseline first; this package is not a complete fresh database.
-- Sections preserve their own guards and commits; the entire file is not atomic.
-- Requires RoleplayCore WORLD schema with npc_vendor.OverrideGoldCost and native RP item flags.
-- Dice/action enablement and XP baselines remain separate committed migrations.
-- For starter loadout replacement, deploy RPS.StarterOutfit.ReplaceLoadout = 1 as well.

-- BEGIN RPS SECTION base
-- Apply explicitly to WORLD only.
-- Definitions/NPC baselines only; no changes to native spells or legacy role stats.

-- Durable duplicate protection. 'pending' records require reconciliation after a
-- cross-pool interruption; they must never cause a second roll or resource debit.
CREATE TABLE IF NOT EXISTS `rps_action_receipts` (
  `issuer_guid` bigint unsigned NOT NULL,
  `request_id` varchar(80) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `signature` text NOT NULL,
  `state` enum('pending','done') NOT NULL,
  `result_kind` varchar(16) NOT NULL,
  `stat_key` varchar(32) NOT NULL DEFAULT '',
  `total` int NOT NULL DEFAULT 0,
  `damage` int NOT NULL DEFAULT 0,
  `attack_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `result_payload` mediumtext DEFAULT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`issuer_guid`,`request_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
CREATE TABLE IF NOT EXISTS `rps_stat_definitions` (
  `stat_key` varchar(32) NOT NULL,
  `category` enum('general','combat','mastery','weapon','action') NOT NULL DEFAULT 'combat',
  `sort_order` int NOT NULL DEFAULT 0,
  `name_ru` varchar(64) NOT NULL,
  `icon` varchar(128) NOT NULL DEFAULT 'Interface\\Icons\\INV_Misc_Dice_01',
  `spell_id` int unsigned NOT NULL DEFAULT 0,
  `dice_formula` varchar(16) NOT NULL DEFAULT '3d6',
  `default_value` int NOT NULL DEFAULT 0,
  `is_active` tinyint unsigned NOT NULL DEFAULT 1,
  `description_ru` text,
  PRIMARY KEY (`stat_key`),
  KEY `idx_spell_id` (`spell_id`),
  KEY `idx_active_sort` (`is_active`,`sort_order`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_creature_template_stats` (
  `entry` int unsigned NOT NULL,
  `role_type` enum('minion','lieutenant','boss') NOT NULL DEFAULT 'minion',
  `is_static_threshold` tinyint unsigned NOT NULL DEFAULT 1,
  `max_health` int NOT NULL DEFAULT 10,
  `max_energy` int NOT NULL DEFAULT 0,
  `armor` int NOT NULL DEFAULT 0,
  `speed` int NOT NULL DEFAULT 10,
  `weapon_dice` varchar(16) NOT NULL DEFAULT '1d6',
  `block_dice` varchar(16) NOT NULL DEFAULT '1d4',
  `stats_json` json NOT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_creature_spawn_stats` (
  `spawn_guid` bigint unsigned NOT NULL COMMENT 'DB spawn ID; runtime instances are separate',
  `custom_name` varchar(128) DEFAULT NULL,
  `entry` int unsigned NOT NULL COMMENT 'Template at the time the override was authored',
  `role_type` enum('minion','lieutenant','boss') NOT NULL DEFAULT 'minion',
  `is_static_threshold` tinyint unsigned NOT NULL DEFAULT 1,
  `cur_health` int NOT NULL DEFAULT 10,
  `max_health` int NOT NULL DEFAULT 10,
  `cur_energy` int NOT NULL DEFAULT 0,
  `max_energy` int NOT NULL DEFAULT 0,
  `armor` int NOT NULL DEFAULT 0,
  `speed` int NOT NULL DEFAULT 10,
  `weapon_dice` varchar(16) NOT NULL DEFAULT '1d6',
  `block_dice` varchar(16) NOT NULL DEFAULT '1d4',
  `stats_json` json NOT NULL,
  `effects_json` json DEFAULT NULL,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`spawn_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Preserve operator edits on reapplication; no REPLACE/DELETE and no sample NPCs.
-- Runtime state is separate from the spawn baseline, including summons (spawn_guid=0).
-- The process epoch isolates recycled native GUIDs after server restart.
CREATE TABLE IF NOT EXISTS `rps_creature_runtime_stats` (
  `runtime_key` varchar(200) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `spawn_guid` bigint unsigned NOT NULL,
  `custom_name` varchar(128) DEFAULT NULL,
  `entry` int unsigned NOT NULL,
  `role_type` enum('minion','lieutenant','boss') NOT NULL DEFAULT 'minion',
  `is_static_threshold` tinyint unsigned NOT NULL DEFAULT 1,
  `cur_health` int NOT NULL,
  `max_health` int NOT NULL,
  `cur_energy` int NOT NULL,
  `max_energy` int NOT NULL,
  `armor` int NOT NULL DEFAULT 0,
  `speed` int NOT NULL DEFAULT 10,
  `weapon_dice` varchar(16) NOT NULL,
  `block_dice` varchar(16) NOT NULL,
  `stats_json` json NOT NULL,
  `effects_json` json DEFAULT NULL,
  `revision` int unsigned NOT NULL DEFAULT 0,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`runtime_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- spell_id remains 0 until matching client+server spell records are validated.
-- spell_id 1500001..1500010 is assigned by the enable script, not here.
INSERT INTO `rps_stat_definitions`
 (`stat_key`,`category`,`sort_order`,`name_ru`,`icon`,`spell_id`,`dice_formula`,`default_value`,`description_ru`)
VALUES
 ('health','general',10,'Здоровье','Interface\\Icons\\Spell_Holy_SealOfSacrifice',0,'none',15,'Текущие и максимальные ОЗ, Раны.'),
 ('energy','general',20,'Энергия','Interface\\Icons\\Spell_Magic_ManaGain',0,'none',10,'Текущие и максимальные ОЭ. Лимит способностей: 6 ОЭ за раунд.'),
 ('armor','general',30,'Броня','Interface\\Icons\\INV_Chest_Plate15',0,'none',0,'Поглощает физический урон; магия игнорирует Броню.'),
 ('speed','general',40,'Скорость','Interface\\Icons\\Ability_Rogue_Sprint',0,'none',10,'Шаги за действие движения.'),
 ('accuracy','combat',10,'Меткость','Interface\\Icons\\ability_hunter_zenarchery',0,'3d6',0,'Боевой бросок Меткости.'),
 ('healing','combat',20,'Исцеление','Interface\\Icons\\spell_shaman_blessingofeternals',0,'3d6',0,'Бросок против сложности исцеления, базово 8.'),
 ('evasion','combat',30,'Уклонение','Interface\\Icons\\ability_hunter_displacement',0,'3d6',0,'Защитный бросок Уклонения.'),
 ('resistance','combat',40,'Сопротивление','Interface\\Icons\\ability_warrior_shieldbash',0,'3d6',0,'Защитный бросок; модификатор Блока.'),
 ('skill','mastery',10,'Навык','Interface\\Icons\\ability_rogue_quickrecovery',0,'3d6',0,'Ловкость, скрытность и акробатика.'),
 ('knowledge','mastery',20,'Знание','Interface\\Icons\\spell_holy_spiritualguidence',0,'3d6',0,'Поиск и анализ.'),
 ('endurance','mastery',30,'Упорство','Interface\\Icons\\spell_holy_fistofjustice',0,'3d6',0,'Выносливость и концентрация.'),
 ('craft','mastery',40,'Ремесло','Interface\\Icons\\achievement_faction_craftsman',0,'3d6',0,'Создание, ремонт и медицинские заявки.'),
 ('weapon_damage','weapon',10,'Урон','Interface\\Icons\\INV_Sword_04',0,'weapon',0,'Кость урона ролевого оружия.'),
 ('shield_block','weapon',20,'Блокирование','Interface\\Icons\\INV_Shield_06',0,'shield',0,'Кость ролевого щита или оружия.')
ON DUPLICATE KEY UPDATE `stat_key` = VALUES(`stat_key`);
-- END RPS SECTION base

-- BEGIN RPS SECTION receipt_payload
-- WORLD upgrade after 2026_10_26_00_rps_combat_world.sql.
-- Persist full results; never reinterpret a historical receipt as a new operation.
SET @rps_receipt_payload_sql = (SELECT IF(COUNT(*) = 0,
  'ALTER TABLE rps_action_receipts ADD COLUMN result_payload MEDIUMTEXT DEFAULT NULL',
  'SELECT 1') FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'rps_action_receipts' AND COLUMN_NAME = 'result_payload');
PREPARE rps_receipt_payload_stmt FROM @rps_receipt_payload_sql;
EXECUTE rps_receipt_payload_stmt;
DEALLOCATE PREPARE rps_receipt_payload_stmt;
-- END RPS SECTION receipt_payload

-- BEGIN RPS SECTION starter_cloth
-- Generated by contrib/rps_combat/generate_starter_cloth.py; package rps-starter-cloth-v3.
-- Select WORLD explicitly. Requires the RP_ONLY native policy (FlagsCu=8).
-- Run during maintenance with writers stopped. Never use --force. This does not enable grants.
-- Exact package rerun is a no-op; partial, modified or foreign records fail without overwrite.
-- A registered previous kit version (replaces) is removed from the reserved range first.
-- CSV build is unknown. Appearance references require deployed DB2/client verification.
-- Use a fresh import connection after a failed attempt; temporary staging is connection-local.
SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS `rps_custom_data_packages` (
  `package_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `installed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`package_key`)
) ENGINE=InnoDB;

DROP PROCEDURE IF EXISTS rps_starter_cloth_world_20261027;
DELIMITER $$
CREATE PROCEDURE rps_starter_cloth_world_20261027()
package_body: BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    CREATE TEMPORARY TABLE `rps_expected_item_template_addon` ENGINE=InnoDB AS SELECT * FROM `item_template_addon` WHERE 0;
INSERT INTO `rps_expected_item_template_addon` (`Id`, `FlagsCu`, `FoodType`, `MinMoneyLoot`, `MaxMoneyLoot`, `SpellPPMChance`, `RandomBonusListTemplateId`, `QuestLogItemId`) VALUES
    (1800001, 8, 0, 0, 0, 0, 0, 0),
    (1800002, 8, 0, 0, 0, 0, 0, 0),
    (1800003, 8, 0, 0, 0, 0, 0, 0),
    (1800004, 8, 0, 0, 0, 0, 0, 0),
    (1800005, 8, 0, 0, 0, 0, 0, 0),
    (1800006, 8, 0, 0, 0, 0, 0, 0),
    (1800007, 8, 0, 0, 0, 0, 0, 0),
    (1800008, 8, 0, 0, 0, 0, 0, 0),
    (1800009, 8, 0, 0, 0, 0, 0, 0),
    (1800010, 8, 0, 0, 0, 0, 0, 0),
    (1800011, 8, 0, 0, 0, 0, 0, 0);
    START TRANSACTION;
    IF NOT EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3')
       AND EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` IN ('rps-starter-cloth-v1', 'rps-starter-cloth-v2')) THEN
        DELETE FROM `item_template_addon` WHERE `Id` BETWEEN 1800001 AND 1800011;
        DELETE FROM `rps_custom_data_packages` WHERE `package_key` IN ('rps-starter-cloth-v1', 'rps-starter-cloth-v2');
    END IF;
    IF EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3') THEN
        IF NOT EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3' AND `payload_sha256` = '4371f6739f725e0babf394f0e6e39bee8a08fc64b6f0f4e83d66791a6184ea39') THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter package checksum differs; use a reviewed upgrade';
        END IF;
        IF (SELECT COUNT(*) FROM `item_template_addon` WHERE `Id` BETWEEN 1800001 AND 1800011) <> 11
           OR (SELECT COUNT(*) FROM `item_template_addon` a JOIN `rps_expected_item_template_addon` e ON a.`Id` <=> e.`Id` AND a.`FlagsCu` <=> e.`FlagsCu` AND a.`FoodType` <=> e.`FoodType` AND a.`MinMoneyLoot` <=> e.`MinMoneyLoot` AND a.`MaxMoneyLoot` <=> e.`MaxMoneyLoot` AND a.`SpellPPMChance` <=> e.`SpellPPMChance` AND a.`RandomBonusListTemplateId` <=> e.`RandomBonusListTemplateId` AND a.`QuestLogItemId` <=> e.`QuestLogItemId`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item_template_addon';
        END IF;
    ELSE
        IF EXISTS (SELECT 1 FROM `item_template_addon` WHERE `Id` BETWEEN 1800001 AND 1800011) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item_template_addon';
        END IF;
        INSERT INTO `item_template_addon` (`Id`, `FlagsCu`, `FoodType`, `MinMoneyLoot`, `MaxMoneyLoot`, `SpellPPMChance`, `RandomBonusListTemplateId`, `QuestLogItemId`) SELECT `Id`, `FlagsCu`, `FoodType`, `MinMoneyLoot`, `MaxMoneyLoot`, `SpellPPMChance`, `RandomBonusListTemplateId`, `QuestLogItemId` FROM `rps_expected_item_template_addon`;
        INSERT INTO `rps_custom_data_packages` (`package_key`, `payload_sha256`) VALUES ('rps-starter-cloth-v3', '4371f6739f725e0babf394f0e6e39bee8a08fc64b6f0f4e83d66791a6184ea39');
    END IF;
    COMMIT;
    DROP TEMPORARY TABLE `rps_expected_item_template_addon`;
END$$
DELIMITER ;
CALL rps_starter_cloth_world_20261027();
DROP PROCEDURE rps_starter_cloth_world_20261027;

-- Read-only postcondition; inspect the package row and all entries before enabling grants.
SELECT * FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3';
-- END RPS SECTION starter_cloth

-- BEGIN RPS SECTION playercreateinfo_cloth
-- Generated by contrib/rps_combat/generate_starter_cloth.py; package rps-playercreateinfo-cloth-v2.
-- WORLD ONLY. Requires SQL06 item data and worldserver.conf RPS.StarterOutfit.ReplaceLoadout = 1,
-- otherwise the native DB2 CharStartOutfit loadout is still given on top of these rows.
-- Replaces ALL start items: no native weapons, bags, food or other DB2 loadout items.
-- The existing playercreateinfo_item table is the sole live source of the starter composition.
-- Run with the worldserver stopped; never use --force. No character inventory/profile is modified.
-- rps_backup_playercreateinfo_item_20261027 is a one-time historical copy for a reviewed rollback, never a second live catalog.
SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS `rps_custom_data_packages` (
  `package_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `installed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`package_key`)
) ENGINE=InnoDB;

DROP PROCEDURE IF EXISTS rps_playercreateinfo_cloth_20261027;
DELIMITER $$
CREATE PROCEDURE rps_playercreateinfo_cloth_20261027()
BEGIN
    DECLARE installed BOOL;
    DECLARE backup_exists BOOL;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    SET installed = EXISTS (SELECT 1 FROM rps_custom_data_packages WHERE package_key = 'rps-playercreateinfo-cloth-v2');
    SET backup_exists = EXISTS (SELECT 1 FROM information_schema.tables
        WHERE table_schema = DATABASE() AND table_name = 'rps_backup_playercreateinfo_item_20261027');
    IF (SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE()
        AND table_name IN ('playercreateinfo_item','rps_custom_data_packages') AND engine = 'InnoDB') <> 2
        OR EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = DATABASE()
            AND table_name = 'rps_backup_playercreateinfo_item_20261027' AND engine <> 'InnoDB') THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter loadout migration requires transactional InnoDB tables';
    END IF;
    IF (SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = DATABASE()
        AND table_name = 'playercreateinfo_item') <> 4 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter composition schema changed; review backup before replacing';
    END IF;
    IF (SELECT COUNT(*) FROM item_template_addon WHERE Id BETWEEN 1800001 AND 1800011 AND FlagsCu = 8) <> 11 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Install and verify the RP_ONLY starter item definitions first';
    END IF;
    IF installed AND NOT backup_exists THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter loadout historical backup is missing';
    END IF;
    IF NOT installed THEN
        IF backup_exists THEN
            IF EXISTS (SELECT 1 FROM `rps_backup_playercreateinfo_item_20261027`) THEN
                SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter loadout backup already has unowned rows';
            END IF;
        ELSE
            CREATE TABLE `rps_backup_playercreateinfo_item_20261027` LIKE playercreateinfo_item;
        END IF;
    END IF;
    CREATE TEMPORARY TABLE rps_expected_start_items ENGINE=InnoDB AS SELECT * FROM playercreateinfo_item WHERE 0;
INSERT INTO `rps_expected_start_items` (`race`, `class`, `itemid`, `amount`) VALUES
    (0, 0, 1800001, 1),
    (0, 0, 1800002, 1),
    (0, 0, 1800003, 1),
    (0, 0, 1800004, 1),
    (0, 0, 1800005, 1),
    (0, 0, 1800006, 1),
    (0, 0, 1800007, 1),
    (0, 0, 1800008, 1),
    (0, 0, 1800009, 1),
    (0, 0, 1800010, 1),
    (0, 0, 1800011, 1);
    START TRANSACTION;
    IF installed THEN
        IF NOT EXISTS (SELECT 1 FROM rps_custom_data_packages
            WHERE package_key = 'rps-playercreateinfo-cloth-v2' AND payload_sha256 = '82bc1f67cb43d853a85dc88ae7e16f2a6657fa265cd43bfcaf6be400cab37eed') THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter loadout package checksum differs';
        END IF;
        IF (SELECT COUNT(*) FROM playercreateinfo_item) <> 11
            OR (SELECT COUNT(*) FROM playercreateinfo_item a JOIN rps_expected_start_items e
                ON a.race=e.race AND a.class=e.class AND a.itemid=e.itemid AND a.amount=e.amount) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter loadout differs; refusing to replace edits';
        END IF;
    ELSE
        INSERT INTO `rps_backup_playercreateinfo_item_20261027` (`race`, `class`, `itemid`, `amount`) SELECT `race`, `class`, `itemid`, `amount` FROM playercreateinfo_item;
        DELETE FROM playercreateinfo_item;
        INSERT INTO playercreateinfo_item (`race`, `class`, `itemid`, `amount`) SELECT `race`, `class`, `itemid`, `amount` FROM rps_expected_start_items;
        INSERT INTO rps_custom_data_packages (package_key,payload_sha256) VALUES ('rps-playercreateinfo-cloth-v2','82bc1f67cb43d853a85dc88ae7e16f2a6657fa265cd43bfcaf6be400cab37eed');
    END IF;
    COMMIT;
    DROP TEMPORARY TABLE rps_expected_start_items;
END$$
DELIMITER ;
CALL rps_playercreateinfo_cloth_20261027();
DROP PROCEDURE rps_playercreateinfo_cloth_20261027;

SELECT `race`, `class`, `itemid`, `amount` FROM playercreateinfo_item ORDER BY race,class,itemid;
SELECT COUNT(*) AS preserved_previous_start_rows FROM `rps_backup_playercreateinfo_item_20261027`;
-- END RPS SECTION playercreateinfo_cloth

-- BEGIN RPS SECTION item_definitions
-- WORLD: RP properties per item entry and RP sets. Read only by NobleNext 07_RPS_Items at startup
-- (.reload eluna re-reads them); the core and native item tables are not involved.
-- modifiers is a JSON object with the same keys/bounds as instance metadata, e.g. '{"armor":1,"accuracy":1}'.
-- Properties apply to any equipped single, nonstackable item (D31), not only RP-only carriers;
-- invalid rows are skipped and logged as item_definition_skipped:<reason>.
-- Set pieces = equipped slots 0..17 except shirt (3); every reached bonus threshold applies.
CREATE TABLE IF NOT EXISTS `rps_item_set` (
  `set_key` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `name` varchar(80) NOT NULL DEFAULT '',
  PRIMARY KEY (`set_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_item_set_bonus` (
  `set_key` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `pieces` tinyint unsigned NOT NULL,
  `modifiers` text NOT NULL,
  PRIMARY KEY (`set_key`,`pieces`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_item_template` (
  `entry` int unsigned NOT NULL,
  `modifiers` text NOT NULL,
  `set_key` varchar(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
  `comment` varchar(120) NOT NULL DEFAULT '',
  PRIMARY KEY (`entry`),
  KEY `idx_set_key` (`set_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
-- END RPS SECTION item_definitions

-- BEGIN RPS SECTION item_enchants
-- Generated by contrib/rps_combat/generate_item_enchants.py from item_enchants_manifest.json; package rps-item-enchants.
-- WORLD ONLY. RP modifiers of native enchant/gem IDs; read by NobleNext 07_RPS_Items (.reload eluna).
-- Living catalog: rerun replaces the reserved range; the matching HOTFIXES file must be applied too.
SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS `rps_item_enchant` (
  `enchant_id` int unsigned NOT NULL,
  `kind` enum('enchant','gem') NOT NULL,
  `name` varchar(80) NOT NULL DEFAULT '',
  `modifiers` text NOT NULL,
  `description` varchar(1024) NOT NULL DEFAULT '',
  PRIMARY KEY (`enchant_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_item_enchant_effect` (
  `enchant_id` int unsigned NOT NULL,
  `effect_index` tinyint unsigned NOT NULL,
  `trigger_kind` enum('equip','use','proc') NOT NULL,
  `spell_id` int unsigned NOT NULL,
  PRIMARY KEY (`enchant_id`,`effect_index`)
) ENGINE=InnoDB;

START TRANSACTION;
DELETE FROM `rps_item_enchant_effect` WHERE `enchant_id` BETWEEN 18001 AND 18999;
DELETE FROM `rps_item_enchant` WHERE `enchant_id` BETWEEN 18001 AND 18999;
INSERT INTO `rps_item_enchant` (`enchant_id`, `kind`, `name`, `modifiers`, `description`) VALUES
    (18001, 'enchant', 'Чары точности: +1 Меткость', '{"accuracy":1}', ''),
    (18002, 'enchant', 'Чары живучести: +1 ОЗ', '{"maxHealth":1}', ''),
    (18003, 'enchant', 'Чары стойкости: +1 Сопротивление', '{"resistance":1}', ''),
    (18004, 'enchant', 'Чары ловкости: +1 Уклонение', '{"evasion":1}', ''),
    (18005, 'enchant', 'Слабое сияние', '{}', 'Вещь едва заметно светится. Только для отыгрыша.'),
    (18006, 'enchant', 'Чары бодрости: +1 ОЭ', '{"maxEnergy":1}', ''),
    (18007, 'enchant', 'Чары прочности: +1 Броня', '{"armor":1}', ''),
    (18008, 'enchant', 'Чары стремительности: +1 Скорость', '{"speed":1}', ''),
    (18009, 'enchant', 'Чары исцеления: +1 Исцеление', '{"healing":1}', ''),
    (18010, 'enchant', 'Чары сноровки: +1 Навык', '{"mastery.skill":1}', ''),
    (18011, 'enchant', 'Чары мудрости: +1 Знание', '{"mastery.knowledge":1}', ''),
    (18012, 'enchant', 'Чары упорства: +1 Упорство', '{"mastery.endurance":1}', ''),
    (18013, 'enchant', 'Чары мастерства: +1 Ремесло', '{"mastery.craft":1}', ''),
    (18014, 'enchant', 'Чары выносливости: +1 Предел ран', '{"woundLimit":1}', ''),
    (18015, 'enchant', 'Чары напора: +1 Лимит затрат энергии', '{"energySpendLimit":1}', ''),
    (18016, 'enchant', 'Чары восстановления: -1 Сложность исцеления', '{"healDifficulty":-1}', ''),
    (18501, 'gem', 'Руна живучести: +1 ОЗ', '{"maxHealth":1}', ''),
    (18502, 'gem', 'Руна точности: +1 Меткость', '{"accuracy":1}', ''),
    (18503, 'gem', 'Руна стойкости: +1 Сопротивление', '{"resistance":1}', ''),
    (18504, 'gem', 'Руна бодрости: +1 ОЭ', '{"maxEnergy":1}', ''),
    (18505, 'gem', 'Руна прочности: +1 Броня', '{"armor":1}', ''),
    (18506, 'gem', 'Руна стремительности: +1 Скорость', '{"speed":1}', ''),
    (18507, 'gem', 'Руна исцеления: +1 Исцеление', '{"healing":1}', ''),
    (18508, 'gem', 'Руна ловкости: +1 Уклонение', '{"evasion":1}', ''),
    (18509, 'gem', 'Руна сноровки: +1 Навык', '{"mastery.skill":1}', ''),
    (18510, 'gem', 'Руна мудрости: +1 Знание', '{"mastery.knowledge":1}', ''),
    (18511, 'gem', 'Руна упорства: +1 Упорство', '{"mastery.endurance":1}', ''),
    (18512, 'gem', 'Руна мастерства: +1 Ремесло', '{"mastery.craft":1}', ''),
    (18513, 'gem', 'Руна выносливости: +1 Предел ран', '{"woundLimit":1}', ''),
    (18514, 'gem', 'Руна напора: +1 Лимит затрат энергии', '{"energySpendLimit":1}', ''),
    (18515, 'gem', 'Руна восстановления: -1 Сложность исцеления', '{"healDifficulty":-1}', ''),
    (18017, 'enchant', 'Превращение Гогельмогеля', '{}', 'При использовании: превращение и возможность обходиться без дыхания.');
INSERT INTO `rps_item_enchant_effect` (`enchant_id`, `effect_index`, `trigger_kind`, `spell_id`) VALUES
    (18017, 1, 'use', 16591);
COMMIT;
-- END RPS SECTION item_enchants

-- BEGIN RPS SECTION starter_vendor
-- WORLD. Starter vendor near the common start point (playercreateinfo map 1, -8195.6 -4643.1 8.9).
-- Sells basic weapons (not part of the starter loadout) and the RP starter kit as a fallback.
-- Everything is free via npc_vendor.OverrideGoldCost = 0 (RoleplayCore column; needs the core fix
-- in Player::BuyItemFromVendorSlot so the purchase honours the override, not only the vendor list).
-- Requires starter_cloth (kit items 1800001..1800011). Rerun replaces entry/guid 1800101, preserving consumables.
-- Weapon types and damage follow the item_equip_type section below.
-- Run with the worldserver stopped or followed by `.reload npc_vendor` and a respawn.
DROP PROCEDURE IF EXISTS rps_starter_vendor_20261027;
DELIMITER //
CREATE PROCEDURE rps_starter_vendor_20261027()
BEGIN
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
  IF NOT EXISTS (SELECT 1 FROM information_schema.columns WHERE table_schema = DATABASE()
      AND table_name = 'npc_vendor' AND column_name = 'OverrideGoldCost') THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'npc_vendor.OverrideGoldCost is missing (RoleplayCore 4.1 world misc column)';
  END IF;
  IF (SELECT COUNT(*) FROM item_template_addon WHERE Id BETWEEN 1800001 AND 1800011 AND FlagsCu = 8) <> 11 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Install the starter kit v2 item definitions first';
  END IF;
  IF EXISTS (SELECT 1 FROM creature WHERE guid = 1800101 AND id <> 1800101) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Creature guid 1800101 is used by another spawn';
  END IF;

  START TRANSACTION;
  DELETE FROM creature WHERE guid = 1800101;
  -- Keep the consumable package range for its own checksum/drift checks below.
  DELETE FROM npc_vendor WHERE entry = 1800101
    AND NOT (item BETWEEN 1801001 AND 1801007 OR slot BETWEEN 40 AND 46);
  DELETE FROM creature_template_locale WHERE entry = 1800101;
  DELETE FROM creature_template_difficulty WHERE Entry = 1800101;
  DELETE FROM creature_template_model WHERE CreatureID = 1800101;
  DELETE FROM creature_template WHERE entry = 1800101;

  -- npcflag 128 = VENDOR; faction 35 = friendly to all; type 7 = humanoid.
  INSERT INTO creature_template (entry,name,subname,faction,npcflag,unit_class,unit_flags2,type,RegenHealth)
    VALUES (1800101,'Starter Quartermaster','Basic weapons and clothing',35,128,1,2048,7,1);
  INSERT INTO creature_template_locale (entry,locale,Name,Title)
    VALUES (1800101,'ruRU','Интендант новичков','Базовое оружие и одежда');
  INSERT INTO creature_template_model (CreatureID,Idx,CreatureDisplayID,DisplayScale,Probability)
    VALUES (1800101,0,1287,1,1);
  INSERT INTO creature_template_difficulty (Entry,DifficultyID,ContentTuningID)
    VALUES (1800101,0,73);

  INSERT INTO npc_vendor (entry,slot,item,maxcount,incrtime,ExtendedCost,type,OverrideGoldCost) VALUES
    (1800101, 1,   25,0,0,0,1,0),  -- Worn Shortsword
    (1800101, 2,   36,0,0,0,1,0),  -- Worn Mace
    (1800101, 3,   37,0,0,0,1,0),  -- Worn Axe
    (1800101, 4, 2092,0,0,0,1,0),  -- Worn Dagger
    (1800101, 5, 2362,0,0,0,1,0),  -- Worn Wooden Shield
    (1800101, 6,   35,0,0,0,1,0),  -- Bent Staff
    (1800101, 7, 1194,0,0,0,1,0),  -- Bastard Sword
    (1800101, 8,12282,0,0,0,1,0),  -- Worn Battleaxe
    (1800101, 9, 2361,0,0,0,1,0),  -- Battleworn Hammer
    (1800101,10, 2504,0,0,0,1,0),  -- Worn Shortbow
    (1800101,11, 2508,0,0,0,1,0),  -- Old Blunderbuss
    (1800101,12,57243,0,0,0,1,0),  -- Old Spear (two-handed long)
    (1800101,13,23347,0,0,0,1,0),  -- Weathered Crossbow (two-handed ranged)
    (1800101,14,223425,0,0,0,1,0), -- Simple Crimson Wand (one-handed magic)
    (1800101,15,152866,0,0,0,1,0), -- Kul Tiran Halberd (one-handed long)
    (1800101,16, 2129,0,0,0,1,0),  -- Large Round Shield (medium, rps_item_equip_type)
    (1800101,17, 1200,0,0,0,1,0),  -- Large Wooden Shield (large, rps_item_equip_type)
    (1800101,20,1800001,0,0,0,1,0),(1800101,21,1800002,0,0,0,1,0),(1800101,22,1800003,0,0,0,1,0),
    (1800101,23,1800004,0,0,0,1,0),(1800101,24,1800005,0,0,0,1,0),(1800101,25,1800006,0,0,0,1,0),
    (1800101,26,1800007,0,0,0,1,0),(1800101,27,1800008,0,0,0,1,0),(1800101,28,1800009,0,0,0,1,0),
    (1800101,29,1800010,0,0,0,1,0),(1800101,30,1800011,0,0,0,1,0);

  -- Five yards in front of the start point, facing it. Adjust in game with `.npc move` if needed.
  INSERT INTO creature (guid,id,map,spawnDifficulties,position_x,position_y,position_z,orientation,spawntimesecs)
    VALUES (1800101,1800101,1,'0',-8190.6,-4643.1,8.9,3.14159,120);
  COMMIT;
END//
DELIMITER ;
CALL rps_starter_vendor_20261027();
DROP PROCEDURE rps_starter_vendor_20261027;
-- END RPS SECTION starter_vendor

-- BEGIN RPS SECTION item_equip_type
-- WORLD. Design-document weapon/shield type of a native entry for NobleNext 07_RPS_Items (.reload eluna).
-- Without a row the type comes from the native weapon subclass (sword/mace/axe/fist -> one-handed or
-- two-handed ordinary, dagger -> throwing, polearm/spear -> long, staff/wand -> magic, bow -> long-range,
-- crossbow -> ranged, gun -> gunpowder) and any native shield is a small shield. Rows are needed for
-- shield sizes and for types without a native subclass (one-handed ranged and gunpowder weapons).
-- equip_key must be a key of K.Weapons or K.Shields (03_RPS_Catalog.lua); unknown keys are reported and ignored.
-- Rerun replaces the starter vendor rows only.
CREATE TABLE IF NOT EXISTS `rps_item_equip_type` (
  `entry` int unsigned NOT NULL,
  `equip_key` varchar(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `comment` varchar(120) NOT NULL DEFAULT '',
  PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `rps_item_equip_type` WHERE `entry` IN (2362, 2129, 1200, 152866);
INSERT INTO `rps_item_equip_type` (`entry`, `equip_key`, `comment`) VALUES
  (2362, 'shield_small', 'Worn Wooden Shield'),
  (2129, 'shield_medium', 'Large Round Shield'),
  (1200, 'shield_large', 'Large Wooden Shield'),
  (152866, 'one_handed_reach', 'Kul Tiran Halberd');
-- END RPS SECTION item_equip_type

-- BEGIN RPS SECTION free_vendor_items
-- WORLD. Install after the starter vendor package, with worldserver stopped.
-- Free native weapons cannot be sold at any vendor, including previously purchased copies.
-- Preserve other custom flags and addon columns. Rerunnable; no base schema changes.
-- Custom starter carriers already have zero prices and keep their package-owned flags.
-- Slots 40..46 belong to consumables; their own section checks the exact addon flags.
SET NAMES utf8mb4;
START TRANSACTION;
INSERT INTO item_template_addon (Id, FlagsCu)
SELECT DISTINCT item, 16 FROM npc_vendor
WHERE entry = 1800101 AND type = 1 AND OverrideGoldCost = 0 AND ExtendedCost = 0
  AND item NOT BETWEEN 1800001 AND 1800011
  AND slot NOT BETWEEN 40 AND 46
ON DUPLICATE KEY UPDATE FlagsCu = item_template_addon.FlagsCu | 16;
COMMIT;
-- END RPS SECTION free_vendor_items

-- BEGIN RPS SECTION consumables
-- Generated by contrib/rps_combat/generate_consumables.py; do not edit.
-- WORLD only; stop writers; no --force. Exact rerun is a no-op; drift/collisions abort.
-- Requires current RoleplayCore schema and NO_VENDOR_VALUE (FlagsCu=16).
SET NAMES utf8mb4;
CREATE TABLE IF NOT EXISTS rps_custom_data_packages (package_key varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY, payload_sha256 char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, installed_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP) ENGINE=InnoDB;
DROP PROCEDURE IF EXISTS rps_consumables_20261028_world_vendor_fix;
DELIMITER $$
CREATE PROCEDURE rps_consumables_20261028_world_vendor_fix()
body: BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
CREATE TEMPORARY TABLE `rps_consumables_item_template_addon` ENGINE=InnoDB AS SELECT * FROM `item_template_addon` WHERE 0;
INSERT INTO `rps_consumables_item_template_addon` (`Id`, `FlagsCu`, `FoodType`, `MinMoneyLoot`, `MaxMoneyLoot`, `SpellPPMChance`, `RandomBonusListTemplateId`, `QuestLogItemId`) VALUES
    (1801001, 16, 0, 0, 0, 0, 0, 0),
    (1801002, 16, 0, 0, 0, 0, 0, 0),
    (1801003, 16, 0, 0, 0, 0, 0, 0),
    (1801004, 16, 0, 0, 0, 0, 0, 0),
    (1801005, 16, 0, 0, 0, 0, 0, 0),
    (1801006, 16, 0, 0, 0, 0, 0, 0),
    (1801007, 16, 0, 0, 0, 0, 0, 0);
CREATE TEMPORARY TABLE `rps_consumables_npc_vendor` ENGINE=InnoDB AS SELECT * FROM `npc_vendor` WHERE 0;
INSERT INTO `rps_consumables_npc_vendor` (`entry`, `slot`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `type`, `BonusListIDs`, `PlayerConditionID`, `IgnoreFiltering`, `VerifiedBuild`, `OverrideGoldCost`) VALUES
    (1800101, 40, 1801001, 0, 0, 0, 1, '', 0, 0, 0, 0),
    (1800101, 41, 1801002, 0, 0, 0, 1, '', 0, 0, 0, 0),
    (1800101, 42, 1801003, 0, 0, 0, 1, '', 0, 0, 0, 0),
    (1800101, 43, 1801004, 0, 0, 0, 1, '', 0, 0, 0, 0),
    (1800101, 44, 1801005, 0, 0, 0, 1, '', 0, 0, 0, 0),
    (1800101, 45, 1801006, 0, 0, 0, 1, '', 0, 0, 0, 0),
    (1800101, 46, 1801007, 0, 0, 0, 1, '', 0, 0, 0, 0);
START TRANSACTION;
IF NOT EXISTS (SELECT 1 FROM creature_template WHERE entry=1800101) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Install the starter vendor first'; END IF;
IF EXISTS (SELECT 1 FROM rps_custom_data_packages WHERE package_key='rps-consumables-v1-world' AND payload_sha256='9051390ebfbed26649fa5f9e9cd4cd0542d076659fe2ff6be85199eec4e5d2a3') THEN
IF (SELECT COUNT(*) FROM npc_vendor WHERE `entry`=1800101 AND (`item` BETWEEN 1801001 AND 1801007 OR `slot` BETWEEN 40 AND 46) AND BINARY BonusListIDs=BINARY '0') <> 7 THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable legacy bonus drift'; END IF;
UPDATE npc_vendor SET BonusListIDs='' WHERE `entry`=1800101 AND (`item` BETWEEN 1801001 AND 1801007 OR `slot` BETWEEN 40 AND 46);
UPDATE rps_custom_data_packages SET payload_sha256='83d57abae4d74db73f8a33f975371a04e27e9a09cfd55938a57129194472b1cc' WHERE package_key='rps-consumables-v1-world';
END IF;
IF EXISTS (SELECT 1 FROM rps_custom_data_packages WHERE package_key='rps-consumables-v1-world') THEN
IF NOT EXISTS (SELECT 1 FROM rps_custom_data_packages WHERE package_key='rps-consumables-v1-world' AND payload_sha256='83d57abae4d74db73f8a33f975371a04e27e9a09cfd55938a57129194472b1cc') THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable checksum changed; reviewed upgrade required'; END IF;
IF (SELECT COUNT(*) FROM `item_template_addon` WHERE `Id` BETWEEN 1801001 AND 1801007) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_item_template_addon` e WHERE NOT EXISTS (SELECT 1 FROM `item_template_addon` a WHERE a.`Id` <=> e.`Id` AND a.`FlagsCu` <=> e.`FlagsCu` AND a.`FoodType` <=> e.`FoodType` AND a.`MinMoneyLoot` <=> e.`MinMoneyLoot` AND a.`MaxMoneyLoot` <=> e.`MaxMoneyLoot` AND a.`SpellPPMChance` <=> e.`SpellPPMChance` AND a.`RandomBonusListTemplateId` <=> e.`RandomBonusListTemplateId` AND a.`QuestLogItemId` <=> e.`QuestLogItemId`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_template_addon'; END IF;
IF (SELECT COUNT(*) FROM `npc_vendor` WHERE `entry`=1800101 AND (`item` BETWEEN 1801001 AND 1801007 OR `slot` BETWEEN 40 AND 46)) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_npc_vendor` e WHERE NOT EXISTS (SELECT 1 FROM `npc_vendor` a WHERE a.`entry` <=> e.`entry` AND a.`slot` <=> e.`slot` AND a.`item` <=> e.`item` AND a.`maxcount` <=> e.`maxcount` AND a.`incrtime` <=> e.`incrtime` AND a.`ExtendedCost` <=> e.`ExtendedCost` AND a.`type` <=> e.`type` AND CAST(a.`BonusListIDs` AS BINARY) <=> CAST(e.`BonusListIDs` AS BINARY) AND a.`PlayerConditionID` <=> e.`PlayerConditionID` AND a.`IgnoreFiltering` <=> e.`IgnoreFiltering` AND a.`VerifiedBuild` <=> e.`VerifiedBuild` AND a.`OverrideGoldCost` <=> e.`OverrideGoldCost`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: npc_vendor'; END IF;
ELSE
IF EXISTS (SELECT 1 FROM `item_template_addon` WHERE `Id` BETWEEN 1801001 AND 1801007) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_template_addon'; END IF;
IF EXISTS (SELECT 1 FROM `npc_vendor` WHERE `entry`=1800101 AND (`item` BETWEEN 1801001 AND 1801007 OR `slot` BETWEEN 40 AND 46)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: npc_vendor'; END IF;
INSERT INTO `item_template_addon` SELECT * FROM `rps_consumables_item_template_addon`;
INSERT INTO `npc_vendor` SELECT * FROM `rps_consumables_npc_vendor`;
INSERT INTO rps_custom_data_packages (package_key,payload_sha256) VALUES ('rps-consumables-v1-world','83d57abae4d74db73f8a33f975371a04e27e9a09cfd55938a57129194472b1cc');
END IF;
COMMIT;
DROP TEMPORARY TABLE `rps_consumables_item_template_addon`;
DROP TEMPORARY TABLE `rps_consumables_npc_vendor`;
END$$
DELIMITER ;
CALL rps_consumables_20261028_world_vendor_fix();
DROP PROCEDURE rps_consumables_20261028_world_vendor_fix;
-- END RPS SECTION consumables

-- BEGIN RPS SECTION dice_icons
-- Use the same icons in WORLD stat definitions delivered to the RP interface.
SET NAMES utf8mb4;
DROP PROCEDURE IF EXISTS rps_dice_world_icons_20261030;
DELIMITER $$
CREATE PROCEDURE rps_dice_world_icons_20261030()
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    START TRANSACTION;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key IN ('accuracy', 'healing', 'evasion', 'resistance', 'skill', 'knowledge', 'endurance', 'craft')) <> 8 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Expected eight RPS stat definitions for dice icons';
    END IF;
    UPDATE rps_stat_definitions SET icon = CASE stat_key
        WHEN 'accuracy' THEN 'Interface\\Icons\\ability_hunter_zenarchery'
        WHEN 'healing' THEN 'Interface\\Icons\\spell_shaman_blessingofeternals'
        WHEN 'evasion' THEN 'Interface\\Icons\\ability_hunter_displacement'
        WHEN 'resistance' THEN 'Interface\\Icons\\ability_warrior_shieldbash'
        WHEN 'skill' THEN 'Interface\\Icons\\ability_rogue_quickrecovery'
        WHEN 'knowledge' THEN 'Interface\\Icons\\spell_holy_spiritualguidence'
        WHEN 'endurance' THEN 'Interface\\Icons\\spell_holy_fistofjustice'
        WHEN 'craft' THEN 'Interface\\Icons\\achievement_faction_craftsman'
        END
    WHERE stat_key IN ('accuracy', 'healing', 'evasion', 'resistance', 'skill', 'knowledge', 'endurance', 'craft');
    COMMIT;
END$$
DELIMITER ;
CALL rps_dice_world_icons_20261030();
DROP PROCEDURE rps_dice_world_icons_20261030;
-- END RPS SECTION dice_icons
