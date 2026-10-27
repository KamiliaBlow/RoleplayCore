-- Race preset version stored with the confirmed character creation.
-- Apply explicitly to CHARACTERS after 2026_10_27_01_rps_character_creation.sql, worldserver STOPPED,
-- after a characters DB backup. Idempotent: the column is added only when missing, the backfill runs once.
-- Without this file the server reads the version as 0 and never offers a player race reset.

DROP PROCEDURE IF EXISTS `rps_2026_10_27_02_race_preset`;
DELIMITER //
CREATE PROCEDURE `rps_2026_10_27_02_race_preset`()
BEGIN
  IF NOT EXISTS (SELECT 1 FROM `information_schema`.`COLUMNS`
      WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_rps_creation'
        AND `COLUMN_NAME` = 'race_preset_version') THEN
    ALTER TABLE `character_rps_creation`
      ADD COLUMN `race_preset_version` int unsigned NOT NULL DEFAULT 0 AFTER `starter_granted_at`;
  END IF;
END//
DELIMITER ;
CALL `rps_2026_10_27_02_race_preset`();
DROP PROCEDURE IF EXISTS `rps_2026_10_27_02_race_preset`;

-- Records confirmed before this column existed used version 1 of every race preset (book races only).
-- The mark keeps a rerun from overwriting versions written by the server later.
UPDATE `character_rps_creation` SET `race_preset_version` = 1
WHERE `race_preset_version` = 0 AND `race_key` <> '' AND `creation_state` IN ('complete', 'reconfigure')
  AND NOT EXISTS (SELECT 1 FROM `rps_migration_marks` m WHERE m.`mark` = '2026_10_27_02_rps_race_preset');
INSERT IGNORE INTO `rps_migration_marks` (`mark`) VALUES ('2026_10_27_02_rps_race_preset');

-- Review queries (read-only):
-- SELECT race_key, race_preset_version, COUNT(*) FROM character_rps_creation GROUP BY race_key, race_preset_version;
