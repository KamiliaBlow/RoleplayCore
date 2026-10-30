-- RPS CHARACTERS package, 2026-10-30.
-- Select this database explicitly; stop writers; never use --force.
-- Apply the committed RPS baseline first; this package is not a complete fresh database.
-- Sections preserve their own guards and commits; the entire file is not atomic.
-- Requires the committed 2026_10_27_01_rps_character_creation.sql baseline before this package.
-- Historical mail campaign IDs and prerequisites identify the ordered sections below.
-- Run with worldserver stopped: mail/item IDs are allocated from the native maxima.

-- BEGIN RPS SECTION base
-- Apply explicitly to CHARACTERS only.
-- No seed profiles: login must not overwrite an unavailable existing character.

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
  PRIMARY KEY (`issuer_guid`,`request_id`),
  KEY `idx_pending_xp` (`result_kind`,`state`,`attack_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
CREATE TABLE IF NOT EXISTS `character_rps_stats` (
  `guid` bigint unsigned NOT NULL,
  `archetype` varchar(32) NOT NULL DEFAULT 'fighter',
  `race_id` varchar(32) NOT NULL DEFAULT 'human',
  `racial_trait` varchar(64) DEFAULT NULL,
  `common_trait` varchar(64) DEFAULT NULL,
  `rp_level` tinyint unsigned NOT NULL DEFAULT 1,
  `rp_xp` int unsigned NOT NULL DEFAULT 0,
  `cur_health` int NOT NULL DEFAULT 15,
  `max_health` int NOT NULL DEFAULT 15,
  `cur_energy` int NOT NULL DEFAULT 10,
  `max_energy` int NOT NULL DEFAULT 10,
  `energy_spent_round` int NOT NULL DEFAULT 0,
  `cur_wounds` tinyint unsigned NOT NULL DEFAULT 0,
  `max_wounds` tinyint unsigned NOT NULL DEFAULT 3,
  `armor` int NOT NULL DEFAULT 0,
  `speed` int NOT NULL DEFAULT 10,
  `weapon_dice` varchar(16) NOT NULL DEFAULT '1d8',
  `block_dice` varchar(16) NOT NULL DEFAULT '1d4',
  `stats_json` json NOT NULL,
  `equip_json` json DEFAULT NULL,
  `effects_json` json DEFAULT NULL,
  `revision` int unsigned NOT NULL DEFAULT 0,
  `death_outcome` varchar(32) NOT NULL DEFAULT '',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_pending_attacks` (
  `attack_id` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `attacker_guid` bigint unsigned NOT NULL,
  `defender_guid` bigint unsigned NOT NULL,
  `attacker_name` varchar(128) NOT NULL,
  `attack_total` int NOT NULL,
  `weapon_dice` varchar(16) NOT NULL,
  `damage_type` enum('physical','magic') NOT NULL,
  `expires_at` int unsigned NOT NULL,
  PRIMARY KEY (`attack_id`),
  KEY `idx_defender` (`defender_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
-- END RPS SECTION base

-- BEGIN RPS SECTION receipt_payload
-- CHARACTERS upgrade after 2026_10_26_00_rps_combat_characters.sql.
-- Existing receipts are retained. Missing historical payloads require reconciliation,
-- never a new grant/roll. Repeat application preserves existing columns and rows.
SET @rps_receipt_payload_sql = (SELECT IF(COUNT(*) = 0,
  'ALTER TABLE rps_action_receipts ADD COLUMN result_payload MEDIUMTEXT DEFAULT NULL',
  'SELECT 1') FROM information_schema.COLUMNS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'rps_action_receipts' AND COLUMN_NAME = 'result_payload');
PREPARE rps_receipt_payload_stmt FROM @rps_receipt_payload_sql;
EXECUTE rps_receipt_payload_stmt;
DEALLOCATE PREPARE rps_receipt_payload_stmt;
SET @rps_receipt_payload_sql = (SELECT IF(COUNT(*) = 0,
  'ALTER TABLE rps_action_receipts ADD INDEX idx_pending_xp (result_kind,state,attack_id)',
  'SELECT 1') FROM information_schema.STATISTICS
  WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'rps_action_receipts' AND INDEX_NAME = 'idx_pending_xp');
PREPARE rps_receipt_payload_stmt FROM @rps_receipt_payload_sql;
EXECUTE rps_receipt_payload_stmt;
DEALLOCATE PREPARE rps_receipt_payload_stmt;
-- END RPS SECTION receipt_payload

-- BEGIN RPS SECTION item_metadata
-- CHARACTERS: RP-only sidecar. No native table is altered or duplicated.
-- Apply to every character schema used by cross-realm account-bank transfers.
-- Ownership/location come only from item_instance and its native containers.
-- A row without a native item has no effect; NobleNext 07_RPS_Items drops such rows on startup.
CREATE TABLE IF NOT EXISTS `rps_item_metadata` (
  `item_guid` BIGINT UNSIGNED NOT NULL,
  `metadata` MEDIUMTEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  PRIMARY KEY (`item_guid`)
) ENGINE=InnoDB;

DROP PROCEDURE IF EXISTS rps_check_item_metadata_sidecar;
DELIMITER //
CREATE PROCEDURE rps_check_item_metadata_sidecar()
BEGIN
  IF NOT EXISTS (SELECT 1 FROM information_schema.TABLES
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata' AND ENGINE='InnoDB')
    OR (SELECT COUNT(*) FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata') <> 2
    OR NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata' AND COLUMN_NAME='item_guid'
        AND COLUMN_TYPE IN ('bigint unsigned', 'bigint(20) unsigned') -- MySQL 8 / MariaDB spelling
        AND IS_NULLABLE='NO' AND COLUMN_DEFAULT IS NULL AND EXTRA='')
    OR NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata' AND COLUMN_NAME='metadata'
        AND DATA_TYPE='mediumtext' AND IS_NULLABLE='NO' AND COLUMN_DEFAULT IS NULL
        AND CHARACTER_SET_NAME='utf8mb4' AND COLLATION_NAME='utf8mb4_unicode_ci' AND EXTRA='')
    OR (SELECT COUNT(*) FROM information_schema.STATISTICS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata') <> 1
    OR NOT EXISTS (SELECT 1 FROM information_schema.STATISTICS
      WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata'
        AND INDEX_NAME='PRIMARY' AND COLUMN_NAME='item_guid' AND NON_UNIQUE=0 AND SEQ_IN_INDEX=1 AND SUB_PART IS NULL)
    OR EXISTS (SELECT 1 FROM information_schema.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA=DATABASE() AND TABLE_NAME='rps_item_metadata') THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Existing rps_item_metadata schema differs; reconcile without changing native tables';
  END IF;
END//
DELIMITER ;
CALL rps_check_item_metadata_sidecar();
DROP PROCEDURE rps_check_item_metadata_sidecar;
-- END RPS SECTION item_metadata

-- BEGIN RPS SECTION starter_outfit_mail
-- CHARACTERS, one-time and OFFLINE: worldserver must be stopped, because the core caches
-- MAX(item_instance.guid) and MAX(mail.id) at startup and would reuse the IDs allocated here.
-- New characters receive the kit natively (world playercreateinfo_item + RPS.StarterOutfit.ReplaceLoadout).
-- Existing living characters without any kit item get one GM mail with the 8 RP-only items.
-- Requires 2026_10_26_00 (rps_action_receipts) and 2026_10_27_01 (rps_migration_marks).
-- Once the mark exists the script is a no-op, so later deletions of kit items are never refilled.
DROP PROCEDURE IF EXISTS rps_grant_starter_outfit_mail;
DELIMITER //
CREATE PROCEDURE rps_grant_starter_outfit_mail()
BEGIN
  DECLARE item_base BIGINT UNSIGNED;
  DECLARE mail_base BIGINT UNSIGNED;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; DROP TEMPORARY TABLE IF EXISTS rps_starter_targets; RESIGNAL; END;

  START TRANSACTION;
  IF NOT EXISTS (SELECT 1 FROM rps_migration_marks WHERE mark='starter-outfit-mail:1') THEN
    DROP TEMPORARY TABLE IF EXISTS rps_starter_targets;
    CREATE TEMPORARY TABLE rps_starter_targets (
      n INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
      guid BIGINT UNSIGNED NOT NULL UNIQUE
    ) ENGINE=InnoDB;
    INSERT INTO rps_starter_targets (guid)
      SELECT c.guid FROM characters c
      WHERE c.deleteInfos_Name IS NULL
        AND NOT EXISTS (SELECT 1 FROM item_instance ii
          WHERE ii.owner_guid=c.guid AND ii.itemEntry BETWEEN 1800001 AND 1800008)
        AND NOT EXISTS (SELECT 1 FROM rps_action_receipts r
          WHERE r.issuer_guid=c.guid AND r.request_id='native-starter:1')
      ORDER BY c.guid;

    SELECT IFNULL(MAX(guid),0) INTO item_base FROM item_instance;
    SELECT IFNULL(MAX(id),0) INTO mail_base FROM mail;

    INSERT INTO item_instance (guid,itemEntry,owner_guid,count,charges,flags,enchantments,durability,createTime,bonusListIDs)
      SELECT item_base+(t.n-1)*8+k.idx, k.entry, t.guid, 1, '', 1, '', 0, UNIX_TIMESTAMP(), ''
      FROM rps_starter_targets t
      CROSS JOIN (SELECT 1 idx,1800001 entry UNION ALL SELECT 2,1800002 UNION ALL SELECT 3,1800003
        UNION ALL SELECT 4,1800004 UNION ALL SELECT 5,1800005 UNION ALL SELECT 6,1800006
        UNION ALL SELECT 7,1800007 UNION ALL SELECT 8,1800008) k;

    -- stationery 61 = GM; checked 0xA10 = HAS_BODY | NOT_RETURNABLE | NOT_DELETABLE_WITH_ATTACHMENTS.
    INSERT INTO mail (id,messageType,stationery,mailTemplateId,sender,receiver,subject,body,has_items,expire_time,deliver_time,money,cod,checked)
      SELECT mail_base+t.n, 0, 61, 0, 0, t.guid,
        'Стартовая одежда',
        'Комплект простой одежды для ролевой игры. Предметы не дают обычных характеристик.',
        1, 4294967295, UNIX_TIMESTAMP(), 0, 0, 2576
      FROM rps_starter_targets t;

    INSERT INTO mail_items (mail_id,item_guid,receiver)
      SELECT mail_base+t.n, item_base+(t.n-1)*8+k.idx, t.guid
      FROM rps_starter_targets t
      CROSS JOIN (SELECT 1 idx UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4
        UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7 UNION ALL SELECT 8) k;

    INSERT INTO rps_action_receipts (issuer_guid,request_id,signature,state,result_kind,result_payload)
      SELECT t.guid, 'native-starter:1', 'starter-outfit:campaign:1', 'done', 'starter_outfit',
        JSON_OBJECT('schema',1,'kit_version',1,'mail',CAST(mail_base+t.n AS CHAR),'items',JSON_ARRAY(
          CAST(item_base+(t.n-1)*8+1 AS CHAR), CAST(item_base+(t.n-1)*8+2 AS CHAR),
          CAST(item_base+(t.n-1)*8+3 AS CHAR), CAST(item_base+(t.n-1)*8+4 AS CHAR),
          CAST(item_base+(t.n-1)*8+5 AS CHAR), CAST(item_base+(t.n-1)*8+6 AS CHAR),
          CAST(item_base+(t.n-1)*8+7 AS CHAR), CAST(item_base+(t.n-1)*8+8 AS CHAR)))
      FROM rps_starter_targets t;

    INSERT INTO rps_migration_marks (mark) VALUES ('starter-outfit-mail:1');
    DROP TEMPORARY TABLE rps_starter_targets;
  END IF;
  COMMIT;
END//
DELIMITER ;
CALL rps_grant_starter_outfit_mail();
DROP PROCEDURE rps_grant_starter_outfit_mail;
-- END RPS SECTION starter_outfit_mail

-- BEGIN RPS SECTION starter_outfit_mail_v2
-- CHARACTERS, one-time and OFFLINE: worldserver must be stopped, because the core caches
-- MAX(item_instance.guid) and MAX(mail.id) at startup and would reuse the IDs allocated here.
-- Starter kit v2 adds shirt (1800009), cloak (1800010) and tabard (1800011). New characters receive
-- them natively (world 2026_10_27_07 v2); existing living characters without any of the three get one
-- GM mail with just these items. Requires 2026_10_27_10 (kit v1 mail) to have been applied first.
-- Once the mark exists the script is a no-op, so later deletions of kit items are never refilled.
DROP PROCEDURE IF EXISTS rps_grant_starter_outfit_mail_v2;
DELIMITER //
CREATE PROCEDURE rps_grant_starter_outfit_mail_v2()
BEGIN
  DECLARE item_base BIGINT UNSIGNED;
  DECLARE mail_base BIGINT UNSIGNED;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; DROP TEMPORARY TABLE IF EXISTS rps_starter_targets; RESIGNAL; END;

  START TRANSACTION;
  IF NOT EXISTS (SELECT 1 FROM rps_migration_marks WHERE mark='starter-outfit-mail:1') THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Apply 2026_10_27_10 (starter kit v1 mail) first';
  END IF;
  IF NOT EXISTS (SELECT 1 FROM rps_migration_marks WHERE mark='starter-outfit-mail:2') THEN
    DROP TEMPORARY TABLE IF EXISTS rps_starter_targets;
    CREATE TEMPORARY TABLE rps_starter_targets (
      n INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
      guid BIGINT UNSIGNED NOT NULL UNIQUE
    ) ENGINE=InnoDB;
    INSERT INTO rps_starter_targets (guid)
      SELECT c.guid FROM characters c
      WHERE c.deleteInfos_Name IS NULL
        AND NOT EXISTS (SELECT 1 FROM item_instance ii
          WHERE ii.owner_guid=c.guid AND ii.itemEntry BETWEEN 1800009 AND 1800011)
        AND NOT EXISTS (SELECT 1 FROM rps_action_receipts r
          WHERE r.issuer_guid=c.guid AND r.request_id='native-starter:2')
      ORDER BY c.guid;

    SELECT IFNULL(MAX(guid),0) INTO item_base FROM item_instance;
    SELECT IFNULL(MAX(id),0) INTO mail_base FROM mail;

    INSERT INTO item_instance (guid,itemEntry,owner_guid,count,charges,flags,enchantments,durability,createTime,bonusListIDs)
      SELECT item_base+(t.n-1)*3+k.idx, k.entry, t.guid, 1, '', 1, '', 0, UNIX_TIMESTAMP(), ''
      FROM rps_starter_targets t
      CROSS JOIN (SELECT 1 idx,1800009 entry UNION ALL SELECT 2,1800010 UNION ALL SELECT 3,1800011) k;

    -- stationery 61 = GM; checked 0xA10 = HAS_BODY | NOT_RETURNABLE | NOT_DELETABLE_WITH_ATTACHMENTS.
    INSERT INTO mail (id,messageType,stationery,mailTemplateId,sender,receiver,subject,body,has_items,expire_time,deliver_time,money,cod,checked)
      SELECT mail_base+t.n, 0, 61, 0, 0, t.guid,
        'Стартовая одежда: дополнение',
        'Рубашка, плащ и гербовая накидка к комплекту простой одежды. Предметы не дают обычных характеристик.',
        1, 4294967295, UNIX_TIMESTAMP(), 0, 0, 2576
      FROM rps_starter_targets t;

    INSERT INTO mail_items (mail_id,item_guid,receiver)
      SELECT mail_base+t.n, item_base+(t.n-1)*3+k.idx, t.guid
      FROM rps_starter_targets t
      CROSS JOIN (SELECT 1 idx UNION ALL SELECT 2 UNION ALL SELECT 3) k;

    INSERT INTO rps_action_receipts (issuer_guid,request_id,signature,state,result_kind,result_payload)
      SELECT t.guid, 'native-starter:2', 'starter-outfit:campaign:2', 'done', 'starter_outfit',
        JSON_OBJECT('schema',1,'kit_version',2,'mail',CAST(mail_base+t.n AS CHAR),'items',JSON_ARRAY(
          CAST(item_base+(t.n-1)*3+1 AS CHAR), CAST(item_base+(t.n-1)*3+2 AS CHAR),
          CAST(item_base+(t.n-1)*3+3 AS CHAR)))
      FROM rps_starter_targets t;

    INSERT INTO rps_migration_marks (mark) VALUES ('starter-outfit-mail:2');
    DROP TEMPORARY TABLE rps_starter_targets;
  END IF;
  COMMIT;
END//
DELIMITER ;
CALL rps_grant_starter_outfit_mail_v2();
DROP PROCEDURE rps_grant_starter_outfit_mail_v2;
-- END RPS SECTION starter_outfit_mail_v2

-- BEGIN RPS SECTION catalog_keys_en
-- Catalog keys: transliterated slugs -> English (catalog lv3-book-2026-10-27.4).
-- Apply to CHARACTERS with worldserver STOPPED, after a characters DB backup. Idempotent: English keys
-- are not in the map, so a second run changes nothing. Generated from
-- Docs/Plans/WarChronicles3d6/sources/key_map.py. character_rps_creation_log.snapshot_json keeps the
-- historical keys on purpose (audit, never read back).

CREATE TEMPORARY TABLE `rps_catalog_key_map` (
  `old_key` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `new_key` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  PRIMARY KEY (`old_key`)
) ENGINE=MEMORY;

INSERT INTO `rps_catalog_key_map` (`old_key`, `new_key`) VALUES
  ('proryv', 'breakthrough'),
  ('usilenie', 'empower'),
  ('blok', 'block_ability'),
  ('priliv_sil', 'second_wind'),
  ('udar_po_ploschadi', 'area_strike'),
  ('kontsentratsiya', 'concentration'),
  ('uskorenie', 'haste'),
  ('kost_sudby', 'fate_die'),
  ('ispolzovanie_sposobnosti', 'use_ability'),
  ('udar', 'strike'),
  ('brosok_vystrel_magicheskiy_snaryad', 'ranged_attack'),
  ('perezaryadka', 'reload'),
  ('prikaz', 'command'),
  ('pervaya_pomosch', 'first_aid'),
  ('ochischenie', 'cleanse'),
  ('skrytnost', 'stealth'),
  ('peredyshka', 'respite'),
  ('obmen', 'exchange'),
  ('bystryy_udar', 'quick_strike'),
  ('bystryy_brosok_vystrel_navskidku_magicheskaya_iskra', 'quick_ranged_attack'),
  ('dvizhenie', 'move'),
  ('vzaimodeystvie', 'interact'),
  ('poisk', 'search'),
  ('ataka_po_vozmozhnosti', 'opportunity_attack'),
  ('zabiyaka', 'brawler'),
  ('iskusstvo_voyny', 'art_of_war'),
  ('tselebnye_chary', 'healing_magic'),
  ('taynye_znaniya', 'arcane_knowledge'),
  ('uveche', 'maimed'),
  ('sbit_s_nog', 'knocked_down'),
  ('zamedlenie', 'slowed'),
  ('obezdvizhivanie', 'immobilized'),
  ('oglushenie', 'stunned'),
  ('sytost', 'well_fed'),
  ('tyazhelaya_travma', 'severe_injury'),
  ('postoyannyy', 'permanent'),
  ('rasseivaemyy', 'dispellable'),
  ('summiruetsya', 'stacking'),
  ('kontrol', 'control'),
  ('zaschischennyy', 'protected'),
  ('v_boyu', 'in_combat'),
  ('vne_boya', 'out_of_combat'),
  ('otdyh', 'rest'),
  ('bezopasnost', 'safety'),
  ('bezoruzhnyy_udar', 'unarmed_strike'),
  ('vrozhdennye_chary', 'innate_spell'),
  ('one_handed_obychnoe', 'one_handed_common'),
  ('one_handed_metatelnoe', 'one_handed_throwing'),
  ('one_handed_dlinnoe', 'one_handed_reach'),
  ('one_handed_magicheskoe', 'one_handed_magic'),
  ('one_handed_strelkovoe', 'one_handed_ranged'),
  ('one_handed_porohovoe', 'one_handed_gunpowder'),
  ('two_handed_obychnoe', 'two_handed_common'),
  ('two_handed_dlinnoe', 'two_handed_reach'),
  ('two_handed_magicheskoe', 'two_handed_magic'),
  ('two_handed_dalnoboynoe', 'two_handed_long_range'),
  ('two_handed_strelkovoe', 'two_handed_ranged'),
  ('two_handed_porohovoe', 'two_handed_gunpowder'),
  ('shield_malyy', 'shield_small'),
  ('shield_sredniy', 'shield_medium'),
  ('shield_bolshoy', 'shield_large'),
  ('armor_legkiy', 'armor_light'),
  ('armor_tyazhelyy', 'armor_heavy'),
  ('strela', 'arrow'),
  ('pulya', 'bullet'),
  ('zele_lecheniya', 'healing_potion'),
  ('zele_many', 'mana_potion'),
  ('paek', 'ration'),
  ('remontnyy_nabor', 'repair_kit'),
  ('aptechka', 'first_aid_kit'),
  ('krovavoe_neistovstvo', 'blood_fury'),
  ('vlastnost', 'dominance'),
  ('regeneratsiya', 'regeneration'),
  ('zakalka', 'hardiness'),
  ('traditsii_predkov', 'ancestral_traditions'),
  ('neupokoennyy', 'restless_dead'),
  ('vechnyy_golod', 'eternal_hunger'),
  ('rodstvo_s_charami', 'arcane_affinity'),
  ('manaotbor', 'mana_tap'),
  ('myasnoy_schit', 'meat_shield'),
  ('dve_golovy_luchshe', 'two_heads_better'),
  ('zhulnichestvo', 'cheating'),
  ('vremya_dengi', 'time_is_money'),
  ('pivka_dlya_ryvka', 'liquid_courage'),
  ('gurman', 'gourmet'),
  ('polnoe_pokrytie', 'full_coverage'),
  ('seriya_priemov', 'combo'),
  ('palach', 'executioner'),
  ('lyubimets_sudby', 'fortunes_favorite'),
  ('telohranitel', 'bodyguard'),
  ('zhivuchest', 'tenacity'),
  ('takticheskoe_preimuschestvo', 'tactical_advantage'),
  ('odarennost', 'gifted'),
  ('ekspert', 'expert'),
  ('razmer_imeet_znachenie', 'size_matters'),
  ('moguchee_teloslozhenie', 'mighty_build'),
  ('vezunchik', 'lucky'),
  ('meditatsiya', 'meditation'),
  ('zaschita_ot_magii', 'magic_ward'),
  ('refleksy_kobry', 'cobra_reflexes'),
  ('boevoy_opyt', 'combat_experience'),
  ('prirozhdennyy_lekar', 'natural_healer'),
  ('okno_vozmozhnostey', 'window_of_opportunity'),
  ('razmer_ne_glavnoe', 'size_doesnt_matter'),
  ('taran', 'battering_ram'),
  ('odinochka', 'loner'),
  ('pogromschik', 'wrecker'),
  ('neutomimost', 'tireless'),
  ('ni_shagu_nazad', 'not_one_step_back'),
  ('ratsionalnost', 'rationality'),
  ('prizyvatel', 'summoner'),
  ('zveroboy', 'beast_slayer'),
  ('polevaya_meditsina', 'field_medicine'),
  ('vybros_adrenalina', 'adrenaline_rush'),
  ('bez_tormozov', 'reckless'),
  ('kavalerist', 'cavalier'),
  ('ekzortsist', 'exorcist'),
  ('pobednyy_razh', 'victory_rush'),
  ('zatmitel_nebes', 'sky_eclipser'),
  ('instinkt_samosohraneniya', 'self_preservation'),
  ('prirozhdennyy_lekar_rework', 'natural_healer_rework'),
  ('fontan_krovi', 'blood_fountain'),
  ('falanga', 'phalanx'),
  ('vo_vseoruzhii', 'fully_armed'),
  ('master_zasad', 'ambush_master'),
  ('beshitrostnost', 'guileless'),
  ('perepolnyayuschaya_mosch', 'overwhelming_power'),
  ('gruppovaya_ohota', 'pack_hunt');

START TRANSACTION;

UPDATE `character_rps_creation` t JOIN `rps_catalog_key_map` m ON t.`racial_feature_key` = m.old_key
  SET t.`racial_feature_key` = m.new_key;
UPDATE `character_rps_creation` t JOIN `rps_catalog_key_map` m ON t.`trait_choice` = m.old_key
  SET t.`trait_choice` = m.new_key;
-- trait_keys / common_trait are comma lists; one pass renames one element per row, four passes cover the list.
UPDATE `character_rps_creation` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`trait_keys`)
  SET t.`trait_keys` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`trait_keys`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));
UPDATE `character_rps_creation` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`trait_keys`)
  SET t.`trait_keys` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`trait_keys`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));
UPDATE `character_rps_creation` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`trait_keys`)
  SET t.`trait_keys` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`trait_keys`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));
UPDATE `character_rps_creation` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`trait_keys`)
  SET t.`trait_keys` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`trait_keys`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));

UPDATE `character_rps_stats` t JOIN `rps_catalog_key_map` m ON t.`racial_trait` = m.old_key
  SET t.`racial_trait` = m.new_key;
UPDATE `character_rps_stats` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`common_trait`)
  SET t.`common_trait` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`common_trait`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));
UPDATE `character_rps_stats` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`common_trait`)
  SET t.`common_trait` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`common_trait`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));
UPDATE `character_rps_stats` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`common_trait`)
  SET t.`common_trait` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`common_trait`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));
UPDATE `character_rps_stats` t JOIN `rps_catalog_key_map` m ON FIND_IN_SET(m.old_key, t.`common_trait`)
  SET t.`common_trait` = TRIM(BOTH ',' FROM REPLACE(CONCAT(',', t.`common_trait`, ','), CONCAT(',', m.old_key, ','), CONCAT(',', m.new_key, ',')));

UPDATE `character_rps_items` t JOIN `rps_catalog_key_map` m ON t.`item_key` = m.old_key
  SET t.`item_key` = m.new_key;

INSERT IGNORE INTO `rps_migration_marks` (`mark`) VALUES ('catalog_keys_en_20261027');

COMMIT;

DROP TEMPORARY TABLE `rps_catalog_key_map`;
-- END RPS SECTION catalog_keys_en

