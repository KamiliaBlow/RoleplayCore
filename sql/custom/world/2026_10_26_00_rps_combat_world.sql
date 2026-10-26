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
 ('accuracy','combat',10,'Меткость','Interface\\Icons\\Ability_Marksmanship',0,'3d6',0,'Боевой бросок Меткости.'),
 ('healing','combat',20,'Исцеление','Interface\\Icons\\Spell_Holy_FlashHeal',0,'3d6',0,'Бросок против сложности исцеления, базово 8.'),
 ('evasion','combat',30,'Уклонение','Interface\\Icons\\Ability_Rogue_Feint',0,'3d6',0,'Защитный бросок Уклонения.'),
 ('resistance','combat',40,'Сопротивление','Interface\\Icons\\Spell_Magic_MageArmor',0,'3d6',0,'Защитный бросок; модификатор Блока.'),
 ('skill','mastery',10,'Навык','Interface\\Icons\\Ability_Stealth',0,'3d6',0,'Ловкость, скрытность и акробатика.'),
 ('knowledge','mastery',20,'Знание','Interface\\Icons\\INV_Misc_Book_09',0,'3d6',0,'Поиск и анализ.'),
 ('endurance','mastery',30,'Упорство','Interface\\Icons\\Spell_Nature_Strength',0,'3d6',0,'Выносливость и концентрация.'),
 ('craft','mastery',40,'Ремесло','Interface\\Icons\\Trade_BlackSmithing',0,'3d6',0,'Создание, ремонт и медицинские заявки.'),
 ('weapon_damage','weapon',10,'Урон','Interface\\Icons\\INV_Sword_04',0,'weapon',0,'Кость урона ролевого оружия.'),
 ('shield_block','weapon',20,'Блокирование','Interface\\Icons\\INV_Shield_06',0,'shield',0,'Кость ролевого щита или оружия.')
ON DUPLICATE KEY UPDATE `stat_key` = VALUES(`stat_key`);
