-- Confirmed character creation, audit log and the separate inventory.
-- Apply explicitly to CHARACTERS after 2026_10_26_00_rps_combat_characters.sql, worldserver STOPPED,
-- after a characters DB backup. Idempotent: CREATE IF NOT EXISTS and a one-shot legacy backfill mark.
-- character_rps_stats is NOT altered: combat profiles keep loading even before this file is applied.

-- One row per character that has a creation state. No row = 'none' (a profile auto-created after
-- this migration). Choice columns keep the last confirmed choice; reconfigure only flips the state.
CREATE TABLE IF NOT EXISTS `character_rps_creation` (
  `guid` bigint unsigned NOT NULL,
  `creation_state` enum('none','legacy','reconfigure','complete') NOT NULL DEFAULT 'none',
  `catalog_version` varchar(64) NOT NULL DEFAULT '',
  `rp_name` varchar(64) NOT NULL DEFAULT '',
  `race_key` varchar(32) NOT NULL DEFAULT '',
  `racial_feature_key` varchar(64) NOT NULL DEFAULT '',
  `archetype_key` varchar(32) NOT NULL DEFAULT '',
  `trait_keys` varchar(255) NOT NULL DEFAULT '',
  `trait_choice` varchar(32) NOT NULL DEFAULT '',
  `mastery_skill` tinyint unsigned NOT NULL DEFAULT 0,
  `mastery_knowledge` tinyint unsigned NOT NULL DEFAULT 0,
  `mastery_endurance` tinyint unsigned NOT NULL DEFAULT 0,
  `mastery_craft` tinyint unsigned NOT NULL DEFAULT 0,
  `revision` int unsigned NOT NULL DEFAULT 0,
  `confirmed_at` int unsigned NOT NULL DEFAULT 0,
  `confirmed_by` bigint unsigned NOT NULL DEFAULT 0,
  `starter_granted` tinyint unsigned NOT NULL DEFAULT 0,
  `starter_granted_at` int unsigned NOT NULL DEFAULT 0,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Append-only history: every confirm/reconfigure with author, reason and the previous snapshot.
CREATE TABLE IF NOT EXISTS `character_rps_creation_log` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `guid` bigint unsigned NOT NULL,
  `revision` int unsigned NOT NULL,
  `action` enum('confirm','reconfigure') NOT NULL,
  `from_state` varchar(16) NOT NULL,
  `to_state` varchar(16) NOT NULL,
  `actor_guid` bigint unsigned NOT NULL,
  `request_id` varchar(80) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `reason` varchar(255) NOT NULL DEFAULT '',
  `snapshot_json` mediumtext NOT NULL,
  `created_at` int unsigned NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_request` (`actor_guid`,`request_id`),
  KEY `idx_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Inventory, separate from native items. One row = one item instance
-- in exactly one location. starter_slot is set only for the starter kit: the unique key makes a
-- second grant fail the whole transaction even if the starter_granted flag were bypassed.
CREATE TABLE IF NOT EXISTS `character_rps_items` (
  `item_id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `owner_guid` bigint unsigned NOT NULL,
  `item_key` varchar(64) NOT NULL,
  `item_kind` enum('weapon','shield','armor','consumable') NOT NULL,
  `quality` varchar(16) NOT NULL DEFAULT 'simple',
  `location` enum('equipment','backpack','storage','loot') NOT NULL DEFAULT 'storage',
  `slot` varchar(16) NOT NULL DEFAULT '',
  `stack` smallint unsigned NOT NULL DEFAULT 1,
  `source` varchar(32) NOT NULL DEFAULT '',
  `starter_slot` tinyint unsigned DEFAULT NULL,
  `created_at` int unsigned NOT NULL,
  PRIMARY KEY (`item_id`),
  UNIQUE KEY `uk_starter` (`owner_guid`,`starter_slot`),
  KEY `idx_owner` (`owner_guid`,`location`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `rps_migration_marks` (
  `mark` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `applied_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`mark`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Profiles that existed before this migration are 'legacy': playable as is, finalized once by the
-- player. Nothing in character_rps_stats is reset. The mark keeps a rerun from turning profiles
-- auto-created later (state 'none') into 'legacy'.
INSERT IGNORE INTO `character_rps_creation` (`guid`, `creation_state`, `archetype_key`, `race_key`)
SELECT s.`guid`, 'legacy', s.`archetype`, s.`race_id` FROM `character_rps_stats` s
WHERE NOT EXISTS (SELECT 1 FROM `rps_migration_marks` m WHERE m.`mark` = '2026_10_27_01_rps_creation_legacy');
INSERT IGNORE INTO `rps_migration_marks` (`mark`) VALUES ('2026_10_27_01_rps_creation_legacy');

-- Review queries (read-only):
-- SELECT creation_state, COUNT(*) FROM character_rps_creation GROUP BY creation_state;
-- Legacy profiles with DM edits/wounds/XP to review before the player finalizes them:
-- SELECT s.guid, s.archetype, s.race_id, s.rp_xp, s.cur_wounds, s.armor, s.cur_health, s.max_health
--   FROM character_rps_stats s JOIN character_rps_creation c ON c.guid = s.guid
--   WHERE c.creation_state = 'legacy' AND (s.cur_wounds > 0 OR s.rp_xp > 0 OR s.armor <> 0
--     OR s.cur_health <> s.max_health OR JSON_LENGTH(COALESCE(s.equip_json, '{}')) > 0
--     OR JSON_LENGTH(COALESCE(s.effects_json, '{}')) > 0);
