-- Transmog outfit situation auto-switch persistence
-- Stores per-outfit situation rules (e.g., "use this outfit when in spec X with loadout Y").
-- The 12.x client sends these via CMSG_TRANSMOG_OUTFIT_UPDATE_SITUATIONS.
-- Columns: guid (character), setguid (equipment set), situationID, specID, loadoutID, equipmentSetID.

CREATE TABLE IF NOT EXISTS `character_transmog_outfit_situations` (
  `guid` bigint unsigned NOT NULL COMMENT 'Character GUID',
  `setguid` bigint unsigned NOT NULL COMMENT 'Equipment set GUID',
  `situationID` int unsigned NOT NULL DEFAULT '0',
  `specID` int unsigned NOT NULL DEFAULT '0',
  `loadoutID` int unsigned NOT NULL DEFAULT '0',
  `equipmentSetID` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`setguid`,`situationID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Transmog outfit auto-switch situations';
