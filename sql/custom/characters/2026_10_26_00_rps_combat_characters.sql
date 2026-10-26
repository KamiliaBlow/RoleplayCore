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
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`issuer_guid`,`request_id`)
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
