-- RPS HOTFIXES package, 2026-10-30.
-- Select this database explicitly; stop writers; never use --force.
-- Apply the committed RPS baseline first; this package is not a complete fresh database.
-- Sections preserve their own guards and commits; the entire file is not atomic.
-- Requires the existing dice/action/skill-line packages and RoleplayCore Spell schema.

-- BEGIN RPS SECTION spell_records
-- Supplement existing RPS dice/action packages with RoleplayCore Spell.db2 records.
-- Select HOTFIXES explicitly and stop writers before execution. Never use --force.
-- Requires sql/RoleplayCore/2. hotfixes db.sql schema; do not reimport that destructive base file.
-- Review the target build DB2 and live push range first. Exact installed records are a no-op; drift aborts.
-- Spell TableHash 0xE111669E is distinct from its client LayoutHash.
SET @rps_spell_records_push_id = 1203140;

DROP PROCEDURE IF EXISTS rps_install_spell_records_20261027;
DELIMITER $$
CREATE PROCEDURE rps_install_spell_records_20261027(IN push_id BIGINT)
package_body: BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF push_id IS NULL OR push_id < 1 OR push_id > 2147483635 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Set an unused positive @rps_spell_records_push_id first';
    END IF;
    START TRANSACTION;
    -- These IDs must already belong to the installed RPS dummy spells.
    IF (SELECT COUNT(*) FROM `spell_name` WHERE `ID` BETWEEN 1500001 AND 1500013 AND `VerifiedBuild` = 0) <> 13
       OR (SELECT COUNT(*) FROM `spell_misc` WHERE `ID` BETWEEN 1500001 AND 1500013 AND `SpellID` = `ID` AND `VerifiedBuild` = 0) <> 13
       OR (SELECT COUNT(*) FROM `spell_effect` WHERE `ID` BETWEEN 1500001 AND 1500013 AND `SpellID` = `ID` AND `Effect` = 3 AND `EffectIndex` = 0 AND `ImplicitTarget1` = 1 AND `VerifiedBuild` = 0) <> 13 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Expected 13 installed RPS dummy spell records; nothing inserted';
    END IF;
    IF EXISTS (SELECT 1 FROM `spell` WHERE `ID` BETWEEN 1500001 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_locale` WHERE `ID` BETWEEN 1500001 AND 1500013)
       OR EXISTS (SELECT 1 FROM `hotfix_blob` WHERE `TableHash` = 0xE111669E AND `RecordId` BETWEEN 1500001 AND 1500013)
       OR EXISTS (SELECT 1 FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 12 OR `UniqueId` BETWEEN push_id AND push_id + 12 OR (`TableHash` = 0xE111669E AND `RecordId` BETWEEN 1500001 AND 1500013)) THEN
        -- A consolidated upgrade must pass an already installed, unchanged supplement.
        -- Check every scoped row, including byte-exact text and extra locales/push records.
        IF (SELECT COUNT(*) FROM `spell` WHERE `ID` BETWEEN 1500001 AND 1500013) = 13
           AND (SELECT COUNT(*) FROM `spell` WHERE `ID` BETWEEN 1500001 AND 1500013
                AND CAST(`NameSubtext` AS BINARY) <=> CAST('' AS BINARY)
                AND CAST(`Description` AS BINARY) <=> CAST(CASE `ID`
                    WHEN 1500001 THEN 'Бросок Меткости.'
                    WHEN 1500002 THEN 'Бросок Исцеления.'
                    WHEN 1500003 THEN 'Бросок Уклонения.'
                    WHEN 1500004 THEN 'Бросок Сопротивления.'
                    WHEN 1500005 THEN 'Бросок Навыка.'
                    WHEN 1500006 THEN 'Бросок Знания.'
                    WHEN 1500007 THEN 'Бросок Упорства.'
                    WHEN 1500008 THEN 'Бросок Ремесла.'
                    WHEN 1500009 THEN 'Бросок урона оружия.'
                    WHEN 1500010 THEN 'Бросок блокирования.'
                    WHEN 1500011 THEN 'Ролевая атака по выбранной цели.'
                    WHEN 1500012 THEN 'Реакция избегания на входящую атаку.'
                    WHEN 1500013 THEN 'Реакция блока на входящую атаку.'
                END AS BINARY)
                AND CAST(`AuraDescription` AS BINARY) <=> CAST('' AS BINARY)
                AND `VerifiedBuild` = 0) = 13
           AND (SELECT COUNT(*) FROM `spell_locale` WHERE `ID` BETWEEN 1500001 AND 1500013) = 13
           AND (SELECT COUNT(*) FROM `spell_locale` WHERE `ID` BETWEEN 1500001 AND 1500013
                AND CAST(`locale` AS BINARY) <=> CAST('ruRU' AS BINARY)
                AND CAST(`NameSubtext_lang` AS BINARY) <=> CAST('' AS BINARY)
                AND CAST(`Description_lang` AS BINARY) <=> CAST(CASE `ID`
                    WHEN 1500001 THEN 'Бросок Меткости.'
                    WHEN 1500002 THEN 'Бросок Исцеления.'
                    WHEN 1500003 THEN 'Бросок Уклонения.'
                    WHEN 1500004 THEN 'Бросок Сопротивления.'
                    WHEN 1500005 THEN 'Бросок Навыка.'
                    WHEN 1500006 THEN 'Бросок Знания.'
                    WHEN 1500007 THEN 'Бросок Упорства.'
                    WHEN 1500008 THEN 'Бросок Ремесла.'
                    WHEN 1500009 THEN 'Бросок урона оружия.'
                    WHEN 1500010 THEN 'Бросок блокирования.'
                    WHEN 1500011 THEN 'Ролевая атака по выбранной цели.'
                    WHEN 1500012 THEN 'Реакция избегания на входящую атаку.'
                    WHEN 1500013 THEN 'Реакция блока на входящую атаку.'
                END AS BINARY)
                AND CAST(`AuraDescription_lang` AS BINARY) <=> CAST('' AS BINARY)
                AND `VerifiedBuild` = 0) = 13
           AND NOT EXISTS (SELECT 1 FROM `hotfix_blob` WHERE `TableHash` = 0xE111669E AND `RecordId` BETWEEN 1500001 AND 1500013)
           AND (SELECT COUNT(*) FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 12
                OR `UniqueId` BETWEEN push_id AND push_id + 12
                OR (`TableHash` = 0xE111669E AND `RecordId` BETWEEN 1500001 AND 1500013)) = 13
           AND (SELECT COUNT(*) FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 12
                AND `UniqueId` = `Id` AND `TableHash` = 0xE111669E
                AND `RecordId` = 1500001 + (`Id` - push_id)
                AND `Status` = 1 AND `VerifiedBuild` = 0) = 13 THEN
            COMMIT;
            LEAVE package_body;
        END IF;
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'RPS Spell records or hotfix push IDs occupied; nothing inserted';
    END IF;
    INSERT INTO `spell` (`ID`, `NameSubtext`, `Description`, `AuraDescription`, `VerifiedBuild`) VALUES
        (1500001, '', 'Бросок Меткости.', '', 0),
        (1500002, '', 'Бросок Исцеления.', '', 0),
        (1500003, '', 'Бросок Уклонения.', '', 0),
        (1500004, '', 'Бросок Сопротивления.', '', 0),
        (1500005, '', 'Бросок Навыка.', '', 0),
        (1500006, '', 'Бросок Знания.', '', 0),
        (1500007, '', 'Бросок Упорства.', '', 0),
        (1500008, '', 'Бросок Ремесла.', '', 0),
        (1500009, '', 'Бросок урона оружия.', '', 0),
        (1500010, '', 'Бросок блокирования.', '', 0),
        (1500011, '', 'Ролевая атака по выбранной цели.', '', 0),
        (1500012, '', 'Реакция избегания на входящую атаку.', '', 0),
        (1500013, '', 'Реакция блока на входящую атаку.', '', 0);
    INSERT INTO `spell_locale` (`ID`, `locale`, `NameSubtext_lang`, `Description_lang`, `AuraDescription_lang`, `VerifiedBuild`) VALUES
        (1500001, 'ruRU', '', 'Бросок Меткости.', '', 0),
        (1500002, 'ruRU', '', 'Бросок Исцеления.', '', 0),
        (1500003, 'ruRU', '', 'Бросок Уклонения.', '', 0),
        (1500004, 'ruRU', '', 'Бросок Сопротивления.', '', 0),
        (1500005, 'ruRU', '', 'Бросок Навыка.', '', 0),
        (1500006, 'ruRU', '', 'Бросок Знания.', '', 0),
        (1500007, 'ruRU', '', 'Бросок Упорства.', '', 0),
        (1500008, 'ruRU', '', 'Бросок Ремесла.', '', 0),
        (1500009, 'ruRU', '', 'Бросок урона оружия.', '', 0),
        (1500010, 'ruRU', '', 'Бросок блокирования.', '', 0),
        (1500011, 'ruRU', '', 'Ролевая атака по выбранной цели.', '', 0),
        (1500012, 'ruRU', '', 'Реакция избегания на входящую атаку.', '', 0),
        (1500013, 'ruRU', '', 'Реакция блока на входящую атаку.', '', 0);
    INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
        (push_id + 0, push_id + 0, 0xE111669E, 1500001, 1, 0),
        (push_id + 1, push_id + 1, 0xE111669E, 1500002, 1, 0),
        (push_id + 2, push_id + 2, 0xE111669E, 1500003, 1, 0),
        (push_id + 3, push_id + 3, 0xE111669E, 1500004, 1, 0),
        (push_id + 4, push_id + 4, 0xE111669E, 1500005, 1, 0),
        (push_id + 5, push_id + 5, 0xE111669E, 1500006, 1, 0),
        (push_id + 6, push_id + 6, 0xE111669E, 1500007, 1, 0),
        (push_id + 7, push_id + 7, 0xE111669E, 1500008, 1, 0),
        (push_id + 8, push_id + 8, 0xE111669E, 1500009, 1, 0),
        (push_id + 9, push_id + 9, 0xE111669E, 1500010, 1, 0),
        (push_id + 10, push_id + 10, 0xE111669E, 1500011, 1, 0),
        (push_id + 11, push_id + 11, 0xE111669E, 1500012, 1, 0),
        (push_id + 12, push_id + 12, 0xE111669E, 1500013, 1, 0);
    COMMIT;
END$$
DELIMITER ;
CALL rps_install_spell_records_20261027(@rps_spell_records_push_id);
DROP PROCEDURE rps_install_spell_records_20261027;
-- END RPS SECTION spell_records

-- BEGIN RPS SECTION starter_cloth
-- Generated by contrib/rps_combat/generate_starter_cloth.py; package rps-starter-cloth-v3.
-- Select HOTFIXES explicitly. Requires the RP_ONLY native policy (FlagsCu=8).
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

DROP PROCEDURE IF EXISTS rps_starter_cloth_hotfixes_20261027;
DELIMITER $$
CREATE PROCEDURE rps_starter_cloth_hotfixes_20261027()
package_body: BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    CREATE TEMPORARY TABLE `rps_expected_item` ENGINE=InnoDB AS SELECT * FROM `item` WHERE 0;
INSERT INTO `rps_expected_item` (`ID`, `ClassID`, `SubclassID`, `Material`, `InventoryType`, `SheatheType`, `SoundOverrideSubclassID`, `IconFileDataID`, `ItemGroupSoundsID`, `ContentTuningID`, `ModifiedCraftingReagentItemID`, `Unknown1200`, `CraftingQualityID`, `ItemSquishEraID`, `RecraftReagentCountPercentage`, `OrderSource`, `VerifiedBuild`) VALUES
    (1800001, 4, 1, 7, 1, 0, -1, 133134, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800002, 4, 1, 7, 3, 0, -1, 135049, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800003, 4, 1, 7, 5, 0, -1, 135011, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800004, 4, 1, 7, 9, 0, -1, 132609, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800005, 4, 1, 7, 10, 0, -1, 132957, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800006, 4, 1, 7, 6, 0, -1, 132513, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800007, 4, 1, 7, 7, 0, -1, 134586, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800008, 4, 1, 7, 8, 0, -1, 132543, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800009, 4, 0, 7, 4, 0, -1, 135006, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800010, 4, 1, 7, 16, 0, -1, 133766, 7, 0, 0, 0, 0, 0, 0, 0, 0),
    (1800011, 4, 0, 4, 19, 0, -1, 134473, 7, 0, 0, 0, 0, 0, 0, 0, 0);
    CREATE TEMPORARY TABLE `rps_expected_item_sparse` ENGINE=InnoDB AS SELECT * FROM `item_sparse` WHERE 0;
INSERT INTO `rps_expected_item_sparse` (`ID`, `Description`, `Display3`, `Display2`, `Display1`, `Display`, `ExpansionID`, `DmgVariance`, `LimitCategory`, `DurationInInventory`, `QualityModifier`, `BagFamily`, `StartQuestID`, `LanguageID`, `ItemRange`, `StatPercentageOfSocket1`, `StatPercentageOfSocket2`, `StatPercentageOfSocket3`, `StatPercentageOfSocket4`, `StatPercentageOfSocket5`, `StatPercentageOfSocket6`, `StatPercentageOfSocket7`, `StatPercentageOfSocket8`, `StatPercentageOfSocket9`, `StatPercentageOfSocket10`, `StatPercentEditor1`, `StatPercentEditor2`, `StatPercentEditor3`, `StatPercentEditor4`, `StatPercentEditor5`, `StatPercentEditor6`, `StatPercentEditor7`, `StatPercentEditor8`, `StatPercentEditor9`, `StatPercentEditor10`, `StatModifierBonusStat1`, `StatModifierBonusStat2`, `StatModifierBonusStat3`, `StatModifierBonusStat4`, `StatModifierBonusStat5`, `StatModifierBonusStat6`, `StatModifierBonusStat7`, `StatModifierBonusStat8`, `StatModifierBonusStat9`, `StatModifierBonusStat10`, `Stackable`, `MaxCount`, `MinReputation`, `RequiredAbility`, `AllowableRace1`, `AllowableRace2`, `SellPrice`, `BuyPrice`, `VendorStackCount`, `PriceVariance`, `PriceRandomValue`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `FactionRelated`, `ModifiedCraftingReagentItemID`, `ContentTuningID`, `PlayerLevelToItemLevelCurveID`, `ItemLevelOffsetCurveID`, `ItemLevelOffsetItemLevel`, `ItemSquishEraID`, `ItemNameDescriptionID`, `RequiredTransmogHoliday`, `RequiredHoliday`, `GemProperties`, `SocketMatchEnchantmentId`, `TotemCategoryID`, `InstanceBound`, `ZoneBound1`, `ZoneBound2`, `ItemSet`, `LockID`, `PageID`, `ItemDelay`, `MinFactionID`, `RequiredSkillRank`, `RequiredSkill`, `ItemLevel`, `AllowableClass`, `ArtifactID`, `SpellWeight`, `SpellWeightCategory`, `SocketType1`, `SocketType2`, `SocketType3`, `SheatheType`, `Material`, `PageMaterialID`, `Bonding`, `DamageDamageType`, `ContainerSlots`, `RequiredPVPMedal`, `RequiredPVPRank`, `RequiredLevel`, `InventoryType`, `OverallQualityID`, `VerifiedBuild`) VALUES
    (1800001, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Hood', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0),
    (1800002, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Shoulders', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 3, 1, 0),
    (1800003, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Vest', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 5, 1, 0),
    (1800004, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Bracers', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 9, 1, 0),
    (1800005, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Gloves', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 10, 1, 0),
    (1800006, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Belt', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 6, 1, 0),
    (1800007, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Trousers', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 7, 1, 0),
    (1800008, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Boots', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 8, 1, 0),
    (1800009, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Shirt', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 4, 1, 0),
    (1800010, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Cloak', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 16, 1, 0),
    (1800011, 'Starter clothing. Grants no attributes and does not wear out.', '', '', '', 'Wayfarer Tabard', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 25174016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 1, 19, 1, 0);
    CREATE TEMPORARY TABLE `rps_expected_item_sparse_locale` ENGINE=InnoDB AS SELECT * FROM `item_sparse_locale` WHERE 0;
INSERT INTO `rps_expected_item_sparse_locale` (`ID`, `locale`, `Description_lang`, `Display3_lang`, `Display2_lang`, `Display1_lang`, `Display_lang`, `VerifiedBuild`) VALUES
    (1800001, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Капюшон путника', 0),
    (1800002, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Наплечники путника', 0),
    (1800003, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Жилет путника', 0),
    (1800004, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Наручи путника', 0),
    (1800005, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Перчатки путника', 0),
    (1800006, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Пояс путника', 0),
    (1800007, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Штаны путника', 0),
    (1800008, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Сапоги путника', 0),
    (1800009, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Рубашка путника', 0),
    (1800010, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Плащ путника', 0),
    (1800011, 'ruRU', 'Стартовая одежда. Не даёт характеристик и не изнашивается.', '', '', '', 'Накидка путника', 0);
    CREATE TEMPORARY TABLE `rps_expected_item_modified_appearance` ENGINE=InnoDB AS SELECT * FROM `item_modified_appearance` WHERE 0;
INSERT INTO `rps_expected_item_modified_appearance` (`ID`, `ItemID`, `ItemAppearanceModifierID`, `ItemAppearanceID`, `OrderIndex`, `TransmogSourceTypeEnum`, `Flags`, `VerifiedBuild`) VALUES
    (1800001, 1800001, 0, 3059, 0, 4, 0, 0),
    (1800002, 1800002, 0, 3061, 0, 4, 0, 0),
    (1800003, 1800003, 0, 803, 0, 3, 0, 0),
    (1800004, 1800004, 0, 1299, 0, 3, 0, 0),
    (1800005, 1800005, 0, 806, 0, 3, 0, 0),
    (1800006, 1800006, 0, 1298, 0, 3, 0, 0),
    (1800007, 1800007, 0, 804, 0, 3, 0, 0),
    (1800008, 1800008, 0, 805, 0, 3, 0, 0),
    (1800009, 1800009, 0, 1596, 0, 8, 0, 0),
    (1800010, 1800010, 0, 412, 0, 8, 0, 0),
    (1800011, 1800011, 0, 4397, 0, 5, 0, 0);
    CREATE TEMPORARY TABLE `rps_expected_item_search_name` ENGINE=InnoDB AS SELECT * FROM `item_search_name` WHERE 0;
INSERT INTO `rps_expected_item_search_name` (`ID`, `Display`, `OverallQualityID`, `ExpansionID`, `MinFactionID`, `MinReputation`, `AllowableClass`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `RequiredAbility`, `ItemLevel`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `AllowableRace1`, `AllowableRace2`, `VerifiedBuild`) VALUES
    (1800001, 'Wayfarer Hood', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800002, 'Wayfarer Shoulders', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800003, 'Wayfarer Vest', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800004, 'Wayfarer Bracers', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800005, 'Wayfarer Gloves', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800006, 'Wayfarer Belt', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800007, 'Wayfarer Trousers', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800008, 'Wayfarer Boots', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800009, 'Wayfarer Shirt', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800010, 'Wayfarer Cloak', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0),
    (1800011, 'Wayfarer Tabard', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 25174016, 0, 0, 0, -1, -1, 0);
    CREATE TEMPORARY TABLE `rps_expected_item_search_name_locale` ENGINE=InnoDB AS SELECT * FROM `item_search_name_locale` WHERE 0;
INSERT INTO `rps_expected_item_search_name_locale` (`ID`, `locale`, `Display_lang`, `VerifiedBuild`) VALUES
    (1800001, 'ruRU', 'Капюшон путника', 0),
    (1800002, 'ruRU', 'Наплечники путника', 0),
    (1800003, 'ruRU', 'Жилет путника', 0),
    (1800004, 'ruRU', 'Наручи путника', 0),
    (1800005, 'ruRU', 'Перчатки путника', 0),
    (1800006, 'ruRU', 'Пояс путника', 0),
    (1800007, 'ruRU', 'Штаны путника', 0),
    (1800008, 'ruRU', 'Сапоги путника', 0),
    (1800009, 'ruRU', 'Рубашка путника', 0),
    (1800010, 'ruRU', 'Плащ путника', 0),
    (1800011, 'ruRU', 'Накидка путника', 0);
    CREATE TEMPORARY TABLE `rps_expected_hotfix_data` ENGINE=InnoDB AS SELECT * FROM `hotfix_data` WHERE 0;
INSERT INTO `rps_expected_hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
    (1203153, 1790790802, 1344507586, 1800001, 1, 0),
    (1203154, 582058951, 1344507586, 1800002, 1, 0),
    (1203155, 1027101353, 1344507586, 1800003, 1, 0),
    (1203156, 553642775, 1344507586, 1800004, 1, 0),
    (1203157, 20297584, 1344507586, 1800005, 1, 0),
    (1203158, 1451208968, 1344507586, 1800006, 1, 0),
    (1203159, 688720684, 1344507586, 1800007, 1, 0),
    (1203160, 1105370524, 1344507586, 1800008, 1, 0),
    (1203161, 1593276996, 1344507586, 1800009, 1, 0),
    (1203162, 2024701104, 1344507586, 1800010, 1, 0),
    (1203163, 252240580, 1344507586, 1800011, 1, 0),
    (1203164, 265998746, 2442913102, 1800001, 1, 0),
    (1203165, 1433281185, 2442913102, 1800002, 1, 0),
    (1203166, 368077461, 2442913102, 1800003, 1, 0),
    (1203167, 2146439361, 2442913102, 1800004, 1, 0),
    (1203168, 354116145, 2442913102, 1800005, 1, 0),
    (1203169, 369329153, 2442913102, 1800006, 1, 0),
    (1203170, 1169987450, 2442913102, 1800007, 1, 0),
    (1203171, 763998283, 2442913102, 1800008, 1, 0),
    (1203172, 2046005995, 2442913102, 1800009, 1, 0),
    (1203173, 1727524321, 2442913102, 1800010, 1, 0),
    (1203174, 40313555, 2442913102, 1800011, 1, 0),
    (1203175, 1800579019, 3834752085, 1800001, 1, 0),
    (1203176, 762160089, 3834752085, 1800002, 1, 0),
    (1203177, 439645272, 3834752085, 1800003, 1, 0),
    (1203178, 1101460582, 3834752085, 1800004, 1, 0),
    (1203179, 1173378602, 3834752085, 1800005, 1, 0),
    (1203180, 1791142571, 3834752085, 1800006, 1, 0),
    (1203181, 752950389, 3834752085, 1800007, 1, 0),
    (1203182, 28871219, 3834752085, 1800008, 1, 0),
    (1203183, 339203895, 3834752085, 1800009, 1, 0),
    (1203184, 1444013022, 3834752085, 1800010, 1, 0),
    (1203185, 1146285801, 3834752085, 1800011, 1, 0),
    (1203186, 571048950, 428746933, 1800001, 1, 0),
    (1203187, 1106771872, 428746933, 1800002, 1, 0),
    (1203188, 1736882283, 428746933, 1800003, 1, 0),
    (1203189, 371243536, 428746933, 1800004, 1, 0),
    (1203190, 941851996, 428746933, 1800005, 1, 0),
    (1203191, 266250533, 428746933, 1800006, 1, 0),
    (1203192, 1964637981, 428746933, 1800007, 1, 0),
    (1203193, 452042317, 428746933, 1800008, 1, 0),
    (1203194, 1321976631, 428746933, 1800009, 1, 0),
    (1203195, 1096916007, 428746933, 1800010, 1, 0),
    (1203196, 2038752101, 428746933, 1800011, 1, 0);
    START TRANSACTION;
    IF EXISTS (SELECT 1 FROM `hotfix_blob` WHERE `TableHash` IN (0x50238ec2, 0x919be54e, 0xe491ac55, 0x198e28b5) AND `RecordId` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `azerite_empowered_item` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `azerite_item` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `character_loadout_item` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `heirloom` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `item_child_equipment` WHERE `ParentItemID` BETWEEN 1800001 AND 1800011 OR `ChildItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `item_currency_cost` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `item_spec_override` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `item_x_bonus_tree` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `item_x_item_effect` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `quest_package_item` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `reward_pack_x_item` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `toy` WHERE `ItemID` BETWEEN 1800001 AND 1800011)
       OR EXISTS (SELECT 1 FROM `trait_system` WHERE `ItemID` BETWEEN 1800001 AND 1800011) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign item references or hotfix blobs occupy the starter range';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3')
       AND EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` IN ('rps-starter-cloth-v1', 'rps-starter-cloth-v2')) THEN
        IF EXISTS (SELECT 1 FROM `item` WHERE `ID` BETWEEN 1800001 AND 1800011 AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: item';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_sparse` WHERE `ID` BETWEEN 1800001 AND 1800011 AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: item_sparse';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_sparse_locale` WHERE `ID` BETWEEN 1800001 AND 1800011 AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: item_sparse_locale';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_modified_appearance` WHERE (`ID` BETWEEN 1800001 AND 1800011 OR `ItemID` BETWEEN 1800001 AND 1800011) AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: item_modified_appearance';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_search_name` WHERE `ID` BETWEEN 1800001 AND 1800011 AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: item_search_name';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_search_name_locale` WHERE `ID` BETWEEN 1800001 AND 1800011 AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: item_search_name_locale';
        END IF;
        IF EXISTS (SELECT 1 FROM `hotfix_data` WHERE (`Id` BETWEEN 1203153 AND 1203196 OR (`TableHash` IN (0x50238ec2, 0x919be54e, 0xe491ac55, 0x198e28b5) AND `RecordId` BETWEEN 1800001 AND 1800011)) AND `VerifiedBuild` <> 0) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records in the starter range: hotfix_data';
        END IF;
        DELETE FROM `item` WHERE `ID` BETWEEN 1800001 AND 1800011;
        DELETE FROM `item_sparse` WHERE `ID` BETWEEN 1800001 AND 1800011;
        DELETE FROM `item_sparse_locale` WHERE `ID` BETWEEN 1800001 AND 1800011;
        DELETE FROM `item_modified_appearance` WHERE (`ID` BETWEEN 1800001 AND 1800011 OR `ItemID` BETWEEN 1800001 AND 1800011);
        DELETE FROM `item_search_name` WHERE `ID` BETWEEN 1800001 AND 1800011;
        DELETE FROM `item_search_name_locale` WHERE `ID` BETWEEN 1800001 AND 1800011;
        DELETE FROM `hotfix_data` WHERE (`Id` BETWEEN 1203153 AND 1203196 OR (`TableHash` IN (0x50238ec2, 0x919be54e, 0xe491ac55, 0x198e28b5) AND `RecordId` BETWEEN 1800001 AND 1800011));
        DELETE FROM `rps_custom_data_packages` WHERE `package_key` IN ('rps-starter-cloth-v1', 'rps-starter-cloth-v2');
    END IF;
    IF EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3') THEN
        IF NOT EXISTS (SELECT 1 FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3' AND `payload_sha256` = 'ce3623ef7247765739ec280fe4af5a4631bd6ad9f15b007dbfd582616e29ccd4') THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter package checksum differs; use a reviewed upgrade';
        END IF;
        IF (SELECT COUNT(*) FROM `item` WHERE `ID` BETWEEN 1800001 AND 1800011) <> 11
           OR (SELECT COUNT(*) FROM `item` a JOIN `rps_expected_item` e ON a.`ID` <=> e.`ID` AND a.`ClassID` <=> e.`ClassID` AND a.`SubclassID` <=> e.`SubclassID` AND a.`Material` <=> e.`Material` AND a.`InventoryType` <=> e.`InventoryType` AND a.`SheatheType` <=> e.`SheatheType` AND a.`SoundOverrideSubclassID` <=> e.`SoundOverrideSubclassID` AND a.`IconFileDataID` <=> e.`IconFileDataID` AND a.`ItemGroupSoundsID` <=> e.`ItemGroupSoundsID` AND a.`ContentTuningID` <=> e.`ContentTuningID` AND a.`ModifiedCraftingReagentItemID` <=> e.`ModifiedCraftingReagentItemID` AND a.`Unknown1200` <=> e.`Unknown1200` AND a.`CraftingQualityID` <=> e.`CraftingQualityID` AND a.`ItemSquishEraID` <=> e.`ItemSquishEraID` AND a.`RecraftReagentCountPercentage` <=> e.`RecraftReagentCountPercentage` AND a.`OrderSource` <=> e.`OrderSource` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item';
        END IF;
        IF (SELECT COUNT(*) FROM `item_sparse` WHERE `ID` BETWEEN 1800001 AND 1800011) <> 11
           OR (SELECT COUNT(*) FROM `item_sparse` a JOIN `rps_expected_item_sparse` e ON a.`ID` <=> e.`ID` AND CAST(a.`Description` AS BINARY) <=> CAST(e.`Description` AS BINARY) AND CAST(a.`Display3` AS BINARY) <=> CAST(e.`Display3` AS BINARY) AND CAST(a.`Display2` AS BINARY) <=> CAST(e.`Display2` AS BINARY) AND CAST(a.`Display1` AS BINARY) <=> CAST(e.`Display1` AS BINARY) AND CAST(a.`Display` AS BINARY) <=> CAST(e.`Display` AS BINARY) AND a.`ExpansionID` <=> e.`ExpansionID` AND a.`DmgVariance` <=> e.`DmgVariance` AND a.`LimitCategory` <=> e.`LimitCategory` AND a.`DurationInInventory` <=> e.`DurationInInventory` AND a.`QualityModifier` <=> e.`QualityModifier` AND a.`BagFamily` <=> e.`BagFamily` AND a.`StartQuestID` <=> e.`StartQuestID` AND a.`LanguageID` <=> e.`LanguageID` AND a.`ItemRange` <=> e.`ItemRange` AND a.`StatPercentageOfSocket1` <=> e.`StatPercentageOfSocket1` AND a.`StatPercentageOfSocket2` <=> e.`StatPercentageOfSocket2` AND a.`StatPercentageOfSocket3` <=> e.`StatPercentageOfSocket3` AND a.`StatPercentageOfSocket4` <=> e.`StatPercentageOfSocket4` AND a.`StatPercentageOfSocket5` <=> e.`StatPercentageOfSocket5` AND a.`StatPercentageOfSocket6` <=> e.`StatPercentageOfSocket6` AND a.`StatPercentageOfSocket7` <=> e.`StatPercentageOfSocket7` AND a.`StatPercentageOfSocket8` <=> e.`StatPercentageOfSocket8` AND a.`StatPercentageOfSocket9` <=> e.`StatPercentageOfSocket9` AND a.`StatPercentageOfSocket10` <=> e.`StatPercentageOfSocket10` AND a.`StatPercentEditor1` <=> e.`StatPercentEditor1` AND a.`StatPercentEditor2` <=> e.`StatPercentEditor2` AND a.`StatPercentEditor3` <=> e.`StatPercentEditor3` AND a.`StatPercentEditor4` <=> e.`StatPercentEditor4` AND a.`StatPercentEditor5` <=> e.`StatPercentEditor5` AND a.`StatPercentEditor6` <=> e.`StatPercentEditor6` AND a.`StatPercentEditor7` <=> e.`StatPercentEditor7` AND a.`StatPercentEditor8` <=> e.`StatPercentEditor8` AND a.`StatPercentEditor9` <=> e.`StatPercentEditor9` AND a.`StatPercentEditor10` <=> e.`StatPercentEditor10` AND a.`StatModifierBonusStat1` <=> e.`StatModifierBonusStat1` AND a.`StatModifierBonusStat2` <=> e.`StatModifierBonusStat2` AND a.`StatModifierBonusStat3` <=> e.`StatModifierBonusStat3` AND a.`StatModifierBonusStat4` <=> e.`StatModifierBonusStat4` AND a.`StatModifierBonusStat5` <=> e.`StatModifierBonusStat5` AND a.`StatModifierBonusStat6` <=> e.`StatModifierBonusStat6` AND a.`StatModifierBonusStat7` <=> e.`StatModifierBonusStat7` AND a.`StatModifierBonusStat8` <=> e.`StatModifierBonusStat8` AND a.`StatModifierBonusStat9` <=> e.`StatModifierBonusStat9` AND a.`StatModifierBonusStat10` <=> e.`StatModifierBonusStat10` AND a.`Stackable` <=> e.`Stackable` AND a.`MaxCount` <=> e.`MaxCount` AND a.`MinReputation` <=> e.`MinReputation` AND a.`RequiredAbility` <=> e.`RequiredAbility` AND a.`AllowableRace1` <=> e.`AllowableRace1` AND a.`AllowableRace2` <=> e.`AllowableRace2` AND a.`SellPrice` <=> e.`SellPrice` AND a.`BuyPrice` <=> e.`BuyPrice` AND a.`VendorStackCount` <=> e.`VendorStackCount` AND a.`PriceVariance` <=> e.`PriceVariance` AND a.`PriceRandomValue` <=> e.`PriceRandomValue` AND a.`Flags1` <=> e.`Flags1` AND a.`Flags2` <=> e.`Flags2` AND a.`Flags3` <=> e.`Flags3` AND a.`Flags4` <=> e.`Flags4` AND a.`Flags5` <=> e.`Flags5` AND a.`FactionRelated` <=> e.`FactionRelated` AND a.`ModifiedCraftingReagentItemID` <=> e.`ModifiedCraftingReagentItemID` AND a.`ContentTuningID` <=> e.`ContentTuningID` AND a.`PlayerLevelToItemLevelCurveID` <=> e.`PlayerLevelToItemLevelCurveID` AND a.`ItemLevelOffsetCurveID` <=> e.`ItemLevelOffsetCurveID` AND a.`ItemLevelOffsetItemLevel` <=> e.`ItemLevelOffsetItemLevel` AND a.`ItemSquishEraID` <=> e.`ItemSquishEraID` AND a.`ItemNameDescriptionID` <=> e.`ItemNameDescriptionID` AND a.`RequiredTransmogHoliday` <=> e.`RequiredTransmogHoliday` AND a.`RequiredHoliday` <=> e.`RequiredHoliday` AND a.`GemProperties` <=> e.`GemProperties` AND a.`SocketMatchEnchantmentId` <=> e.`SocketMatchEnchantmentId` AND a.`TotemCategoryID` <=> e.`TotemCategoryID` AND a.`InstanceBound` <=> e.`InstanceBound` AND a.`ZoneBound1` <=> e.`ZoneBound1` AND a.`ZoneBound2` <=> e.`ZoneBound2` AND a.`ItemSet` <=> e.`ItemSet` AND a.`LockID` <=> e.`LockID` AND a.`PageID` <=> e.`PageID` AND a.`ItemDelay` <=> e.`ItemDelay` AND a.`MinFactionID` <=> e.`MinFactionID` AND a.`RequiredSkillRank` <=> e.`RequiredSkillRank` AND a.`RequiredSkill` <=> e.`RequiredSkill` AND a.`ItemLevel` <=> e.`ItemLevel` AND a.`AllowableClass` <=> e.`AllowableClass` AND a.`ArtifactID` <=> e.`ArtifactID` AND a.`SpellWeight` <=> e.`SpellWeight` AND a.`SpellWeightCategory` <=> e.`SpellWeightCategory` AND a.`SocketType1` <=> e.`SocketType1` AND a.`SocketType2` <=> e.`SocketType2` AND a.`SocketType3` <=> e.`SocketType3` AND a.`SheatheType` <=> e.`SheatheType` AND a.`Material` <=> e.`Material` AND a.`PageMaterialID` <=> e.`PageMaterialID` AND a.`Bonding` <=> e.`Bonding` AND a.`DamageDamageType` <=> e.`DamageDamageType` AND a.`ContainerSlots` <=> e.`ContainerSlots` AND a.`RequiredPVPMedal` <=> e.`RequiredPVPMedal` AND a.`RequiredPVPRank` <=> e.`RequiredPVPRank` AND a.`RequiredLevel` <=> e.`RequiredLevel` AND a.`InventoryType` <=> e.`InventoryType` AND a.`OverallQualityID` <=> e.`OverallQualityID` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item_sparse';
        END IF;
        IF (SELECT COUNT(*) FROM `item_sparse_locale` WHERE `ID` BETWEEN 1800001 AND 1800011) <> 11
           OR (SELECT COUNT(*) FROM `item_sparse_locale` a JOIN `rps_expected_item_sparse_locale` e ON a.`ID` <=> e.`ID` AND CAST(a.`locale` AS BINARY) <=> CAST(e.`locale` AS BINARY) AND CAST(a.`Description_lang` AS BINARY) <=> CAST(e.`Description_lang` AS BINARY) AND CAST(a.`Display3_lang` AS BINARY) <=> CAST(e.`Display3_lang` AS BINARY) AND CAST(a.`Display2_lang` AS BINARY) <=> CAST(e.`Display2_lang` AS BINARY) AND CAST(a.`Display1_lang` AS BINARY) <=> CAST(e.`Display1_lang` AS BINARY) AND CAST(a.`Display_lang` AS BINARY) <=> CAST(e.`Display_lang` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item_sparse_locale';
        END IF;
        IF (SELECT COUNT(*) FROM `item_modified_appearance` WHERE (`ID` BETWEEN 1800001 AND 1800011 OR `ItemID` BETWEEN 1800001 AND 1800011)) <> 11
           OR (SELECT COUNT(*) FROM `item_modified_appearance` a JOIN `rps_expected_item_modified_appearance` e ON a.`ID` <=> e.`ID` AND a.`ItemID` <=> e.`ItemID` AND a.`ItemAppearanceModifierID` <=> e.`ItemAppearanceModifierID` AND a.`ItemAppearanceID` <=> e.`ItemAppearanceID` AND a.`OrderIndex` <=> e.`OrderIndex` AND a.`TransmogSourceTypeEnum` <=> e.`TransmogSourceTypeEnum` AND a.`Flags` <=> e.`Flags` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item_modified_appearance';
        END IF;
        IF (SELECT COUNT(*) FROM `item_search_name` WHERE `ID` BETWEEN 1800001 AND 1800011) <> 11
           OR (SELECT COUNT(*) FROM `item_search_name` a JOIN `rps_expected_item_search_name` e ON a.`ID` <=> e.`ID` AND CAST(a.`Display` AS BINARY) <=> CAST(e.`Display` AS BINARY) AND a.`OverallQualityID` <=> e.`OverallQualityID` AND a.`ExpansionID` <=> e.`ExpansionID` AND a.`MinFactionID` <=> e.`MinFactionID` AND a.`MinReputation` <=> e.`MinReputation` AND a.`AllowableClass` <=> e.`AllowableClass` AND a.`RequiredLevel` <=> e.`RequiredLevel` AND a.`RequiredSkill` <=> e.`RequiredSkill` AND a.`RequiredSkillRank` <=> e.`RequiredSkillRank` AND a.`RequiredAbility` <=> e.`RequiredAbility` AND a.`ItemLevel` <=> e.`ItemLevel` AND a.`Flags1` <=> e.`Flags1` AND a.`Flags2` <=> e.`Flags2` AND a.`Flags3` <=> e.`Flags3` AND a.`Flags4` <=> e.`Flags4` AND a.`Flags5` <=> e.`Flags5` AND a.`AllowableRace1` <=> e.`AllowableRace1` AND a.`AllowableRace2` <=> e.`AllowableRace2` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item_search_name';
        END IF;
        IF (SELECT COUNT(*) FROM `item_search_name_locale` WHERE `ID` BETWEEN 1800001 AND 1800011) <> 11
           OR (SELECT COUNT(*) FROM `item_search_name_locale` a JOIN `rps_expected_item_search_name_locale` e ON a.`ID` <=> e.`ID` AND CAST(a.`locale` AS BINARY) <=> CAST(e.`locale` AS BINARY) AND CAST(a.`Display_lang` AS BINARY) <=> CAST(e.`Display_lang` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 11 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: item_search_name_locale';
        END IF;
        IF (SELECT COUNT(*) FROM `hotfix_data` WHERE (`Id` BETWEEN 1203153 AND 1203196 OR (`TableHash` IN (0x50238ec2, 0x919be54e, 0xe491ac55, 0x198e28b5) AND `RecordId` BETWEEN 1800001 AND 1800011))) <> 44
           OR (SELECT COUNT(*) FROM `hotfix_data` a JOIN `rps_expected_hotfix_data` e ON a.`Id` <=> e.`Id` AND a.`UniqueId` <=> e.`UniqueId` AND a.`TableHash` <=> e.`TableHash` AND a.`RecordId` <=> e.`RecordId` AND a.`Status` <=> e.`Status` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`) <> 44 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Installed starter rows differ: hotfix_data';
        END IF;
    ELSE
        IF EXISTS (SELECT 1 FROM `item` WHERE `ID` BETWEEN 1800001 AND 1800011) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_sparse` WHERE `ID` BETWEEN 1800001 AND 1800011) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item_sparse';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_sparse_locale` WHERE `ID` BETWEEN 1800001 AND 1800011) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item_sparse_locale';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_modified_appearance` WHERE (`ID` BETWEEN 1800001 AND 1800011 OR `ItemID` BETWEEN 1800001 AND 1800011)) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item_modified_appearance';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_search_name` WHERE `ID` BETWEEN 1800001 AND 1800011) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item_search_name';
        END IF;
        IF EXISTS (SELECT 1 FROM `item_search_name_locale` WHERE `ID` BETWEEN 1800001 AND 1800011) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: item_search_name_locale';
        END IF;
        IF EXISTS (SELECT 1 FROM `hotfix_data` WHERE (`Id` BETWEEN 1203153 AND 1203196 OR (`TableHash` IN (0x50238ec2, 0x919be54e, 0xe491ac55, 0x198e28b5) AND `RecordId` BETWEEN 1800001 AND 1800011))) THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Starter IDs occupied: hotfix_data';
        END IF;
        INSERT INTO `item` (`ID`, `ClassID`, `SubclassID`, `Material`, `InventoryType`, `SheatheType`, `SoundOverrideSubclassID`, `IconFileDataID`, `ItemGroupSoundsID`, `ContentTuningID`, `ModifiedCraftingReagentItemID`, `Unknown1200`, `CraftingQualityID`, `ItemSquishEraID`, `RecraftReagentCountPercentage`, `OrderSource`, `VerifiedBuild`) SELECT `ID`, `ClassID`, `SubclassID`, `Material`, `InventoryType`, `SheatheType`, `SoundOverrideSubclassID`, `IconFileDataID`, `ItemGroupSoundsID`, `ContentTuningID`, `ModifiedCraftingReagentItemID`, `Unknown1200`, `CraftingQualityID`, `ItemSquishEraID`, `RecraftReagentCountPercentage`, `OrderSource`, `VerifiedBuild` FROM `rps_expected_item`;
        INSERT INTO `item_sparse` (`ID`, `Description`, `Display3`, `Display2`, `Display1`, `Display`, `ExpansionID`, `DmgVariance`, `LimitCategory`, `DurationInInventory`, `QualityModifier`, `BagFamily`, `StartQuestID`, `LanguageID`, `ItemRange`, `StatPercentageOfSocket1`, `StatPercentageOfSocket2`, `StatPercentageOfSocket3`, `StatPercentageOfSocket4`, `StatPercentageOfSocket5`, `StatPercentageOfSocket6`, `StatPercentageOfSocket7`, `StatPercentageOfSocket8`, `StatPercentageOfSocket9`, `StatPercentageOfSocket10`, `StatPercentEditor1`, `StatPercentEditor2`, `StatPercentEditor3`, `StatPercentEditor4`, `StatPercentEditor5`, `StatPercentEditor6`, `StatPercentEditor7`, `StatPercentEditor8`, `StatPercentEditor9`, `StatPercentEditor10`, `StatModifierBonusStat1`, `StatModifierBonusStat2`, `StatModifierBonusStat3`, `StatModifierBonusStat4`, `StatModifierBonusStat5`, `StatModifierBonusStat6`, `StatModifierBonusStat7`, `StatModifierBonusStat8`, `StatModifierBonusStat9`, `StatModifierBonusStat10`, `Stackable`, `MaxCount`, `MinReputation`, `RequiredAbility`, `AllowableRace1`, `AllowableRace2`, `SellPrice`, `BuyPrice`, `VendorStackCount`, `PriceVariance`, `PriceRandomValue`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `FactionRelated`, `ModifiedCraftingReagentItemID`, `ContentTuningID`, `PlayerLevelToItemLevelCurveID`, `ItemLevelOffsetCurveID`, `ItemLevelOffsetItemLevel`, `ItemSquishEraID`, `ItemNameDescriptionID`, `RequiredTransmogHoliday`, `RequiredHoliday`, `GemProperties`, `SocketMatchEnchantmentId`, `TotemCategoryID`, `InstanceBound`, `ZoneBound1`, `ZoneBound2`, `ItemSet`, `LockID`, `PageID`, `ItemDelay`, `MinFactionID`, `RequiredSkillRank`, `RequiredSkill`, `ItemLevel`, `AllowableClass`, `ArtifactID`, `SpellWeight`, `SpellWeightCategory`, `SocketType1`, `SocketType2`, `SocketType3`, `SheatheType`, `Material`, `PageMaterialID`, `Bonding`, `DamageDamageType`, `ContainerSlots`, `RequiredPVPMedal`, `RequiredPVPRank`, `RequiredLevel`, `InventoryType`, `OverallQualityID`, `VerifiedBuild`) SELECT `ID`, `Description`, `Display3`, `Display2`, `Display1`, `Display`, `ExpansionID`, `DmgVariance`, `LimitCategory`, `DurationInInventory`, `QualityModifier`, `BagFamily`, `StartQuestID`, `LanguageID`, `ItemRange`, `StatPercentageOfSocket1`, `StatPercentageOfSocket2`, `StatPercentageOfSocket3`, `StatPercentageOfSocket4`, `StatPercentageOfSocket5`, `StatPercentageOfSocket6`, `StatPercentageOfSocket7`, `StatPercentageOfSocket8`, `StatPercentageOfSocket9`, `StatPercentageOfSocket10`, `StatPercentEditor1`, `StatPercentEditor2`, `StatPercentEditor3`, `StatPercentEditor4`, `StatPercentEditor5`, `StatPercentEditor6`, `StatPercentEditor7`, `StatPercentEditor8`, `StatPercentEditor9`, `StatPercentEditor10`, `StatModifierBonusStat1`, `StatModifierBonusStat2`, `StatModifierBonusStat3`, `StatModifierBonusStat4`, `StatModifierBonusStat5`, `StatModifierBonusStat6`, `StatModifierBonusStat7`, `StatModifierBonusStat8`, `StatModifierBonusStat9`, `StatModifierBonusStat10`, `Stackable`, `MaxCount`, `MinReputation`, `RequiredAbility`, `AllowableRace1`, `AllowableRace2`, `SellPrice`, `BuyPrice`, `VendorStackCount`, `PriceVariance`, `PriceRandomValue`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `FactionRelated`, `ModifiedCraftingReagentItemID`, `ContentTuningID`, `PlayerLevelToItemLevelCurveID`, `ItemLevelOffsetCurveID`, `ItemLevelOffsetItemLevel`, `ItemSquishEraID`, `ItemNameDescriptionID`, `RequiredTransmogHoliday`, `RequiredHoliday`, `GemProperties`, `SocketMatchEnchantmentId`, `TotemCategoryID`, `InstanceBound`, `ZoneBound1`, `ZoneBound2`, `ItemSet`, `LockID`, `PageID`, `ItemDelay`, `MinFactionID`, `RequiredSkillRank`, `RequiredSkill`, `ItemLevel`, `AllowableClass`, `ArtifactID`, `SpellWeight`, `SpellWeightCategory`, `SocketType1`, `SocketType2`, `SocketType3`, `SheatheType`, `Material`, `PageMaterialID`, `Bonding`, `DamageDamageType`, `ContainerSlots`, `RequiredPVPMedal`, `RequiredPVPRank`, `RequiredLevel`, `InventoryType`, `OverallQualityID`, `VerifiedBuild` FROM `rps_expected_item_sparse`;
        INSERT INTO `item_sparse_locale` (`ID`, `locale`, `Description_lang`, `Display3_lang`, `Display2_lang`, `Display1_lang`, `Display_lang`, `VerifiedBuild`) SELECT `ID`, `locale`, `Description_lang`, `Display3_lang`, `Display2_lang`, `Display1_lang`, `Display_lang`, `VerifiedBuild` FROM `rps_expected_item_sparse_locale`;
        INSERT INTO `item_modified_appearance` (`ID`, `ItemID`, `ItemAppearanceModifierID`, `ItemAppearanceID`, `OrderIndex`, `TransmogSourceTypeEnum`, `Flags`, `VerifiedBuild`) SELECT `ID`, `ItemID`, `ItemAppearanceModifierID`, `ItemAppearanceID`, `OrderIndex`, `TransmogSourceTypeEnum`, `Flags`, `VerifiedBuild` FROM `rps_expected_item_modified_appearance`;
        INSERT INTO `item_search_name` (`ID`, `Display`, `OverallQualityID`, `ExpansionID`, `MinFactionID`, `MinReputation`, `AllowableClass`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `RequiredAbility`, `ItemLevel`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `AllowableRace1`, `AllowableRace2`, `VerifiedBuild`) SELECT `ID`, `Display`, `OverallQualityID`, `ExpansionID`, `MinFactionID`, `MinReputation`, `AllowableClass`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `RequiredAbility`, `ItemLevel`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `AllowableRace1`, `AllowableRace2`, `VerifiedBuild` FROM `rps_expected_item_search_name`;
        INSERT INTO `item_search_name_locale` (`ID`, `locale`, `Display_lang`, `VerifiedBuild`) SELECT `ID`, `locale`, `Display_lang`, `VerifiedBuild` FROM `rps_expected_item_search_name_locale`;
        INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) SELECT `Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild` FROM `rps_expected_hotfix_data`;
        INSERT INTO `rps_custom_data_packages` (`package_key`, `payload_sha256`) VALUES ('rps-starter-cloth-v3', 'ce3623ef7247765739ec280fe4af5a4631bd6ad9f15b007dbfd582616e29ccd4');
    END IF;
    COMMIT;
    DROP TEMPORARY TABLE `rps_expected_item`;
    DROP TEMPORARY TABLE `rps_expected_item_sparse`;
    DROP TEMPORARY TABLE `rps_expected_item_sparse_locale`;
    DROP TEMPORARY TABLE `rps_expected_item_modified_appearance`;
    DROP TEMPORARY TABLE `rps_expected_item_search_name`;
    DROP TEMPORARY TABLE `rps_expected_item_search_name_locale`;
    DROP TEMPORARY TABLE `rps_expected_hotfix_data`;
END$$
DELIMITER ;
CALL rps_starter_cloth_hotfixes_20261027();
DROP PROCEDURE rps_starter_cloth_hotfixes_20261027;

-- Read-only postcondition; inspect the package row and all entries before enabling grants.
SELECT * FROM `rps_custom_data_packages` WHERE `package_key` = 'rps-starter-cloth-v3';
-- END RPS SECTION starter_cloth

-- BEGIN RPS SECTION item_enchants
-- Generated by contrib/rps_combat/generate_item_enchants.py from item_enchants_manifest.json; package rps-item-enchants.
-- HOTFIXES ONLY, writers stopped. Living catalog: rerun replaces exactly the reserved ranges
-- (enchants 18001-18999, item_bonus 1800101-1800199, bonus lists 60001-60005, pushes 1203200-1203399).
-- Foreign rows (VerifiedBuild<>0) or hotfix blobs inside the ranges abort without changes.
-- Numeric RP modifiers stay in WORLD; configured native spells use equip/use/proc enchant effects.
SET NAMES utf8mb4;

CREATE TABLE IF NOT EXISTS `rps_custom_data_packages` (
  `package_key` varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `payload_sha256` char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  `installed_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`package_key`)
) ENGINE=InnoDB;

DROP PROCEDURE IF EXISTS rps_item_enchants_hotfixes;
DELIMITER $$
CREATE PROCEDURE rps_item_enchants_hotfixes()
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    START TRANSACTION;
    IF EXISTS (SELECT 1 FROM `spell_item_enchantment` WHERE `ID` BETWEEN 18001 AND 18999 AND `VerifiedBuild` <> 0)
       OR EXISTS (SELECT 1 FROM `spell_item_enchantment_locale` WHERE `ID` BETWEEN 18001 AND 18999 AND `VerifiedBuild` <> 0)
       OR EXISTS (SELECT 1 FROM `item_bonus` WHERE (`ID` BETWEEN 1800101 AND 1800199 OR `ParentItemBonusListID` BETWEEN 60001 AND 60005) AND `VerifiedBuild` <> 0)
       OR EXISTS (SELECT 1 FROM `hotfix_data` WHERE (`Id` BETWEEN 1203200 AND 1203399 OR (`TableHash` = 0xe05ac589 AND `RecordId` BETWEEN 18001 AND 18999) OR (`TableHash` = 0x8318900a AND `RecordId` BETWEEN 1800101 AND 1800199)) AND `VerifiedBuild` <> 0)
       OR EXISTS (SELECT 1 FROM `hotfix_blob` WHERE (`TableHash` = 0xe05ac589 AND `RecordId` BETWEEN 18001 AND 18999) OR (`TableHash` = 0x8318900a AND `RecordId` BETWEEN 1800101 AND 1800199)) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign records occupy the RP enchant ranges';
    END IF;
    DELETE FROM `spell_item_enchantment` WHERE `ID` BETWEEN 18001 AND 18999;
    DELETE FROM `spell_item_enchantment_locale` WHERE `ID` BETWEEN 18001 AND 18999;
    DELETE FROM `item_bonus` WHERE (`ID` BETWEEN 1800101 AND 1800199 OR `ParentItemBonusListID` BETWEEN 60001 AND 60005);
    DELETE FROM `hotfix_data` WHERE (`Id` BETWEEN 1203200 AND 1203399 OR (`TableHash` = 0xe05ac589 AND `RecordId` BETWEEN 18001 AND 18999) OR (`TableHash` = 0x8318900a AND `RecordId` BETWEEN 1800101 AND 1800199));
    INSERT INTO `spell_item_enchantment` (`ID`, `Name`, `HordeName`, `Duration`, `EffectArg1`, `EffectArg2`, `EffectArg3`, `Flags`, `EffectScalingPoints1`, `EffectScalingPoints2`, `EffectScalingPoints3`, `IconFileDataID`, `MinItemLevel`, `MaxItemLevel`, `TransmogUseConditionID`, `TransmogCost`, `EffectPointsMin1`, `EffectPointsMin2`, `EffectPointsMin3`, `ItemVisual`, `RequiredSkillID`, `RequiredSkillRank`, `ItemLevel`, `Charges`, `Effect1`, `Effect2`, `Effect3`, `ScalingClass`, `ScalingClassRestricted`, `ConditionID`, `MinLevel`, `MaxLevel`, `VerifiedBuild`) VALUES
        (18001, 'Accuracy +1', 'Accuracy +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18002, 'Vitality +1', 'Vitality +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18003, 'Resistance +1', 'Resistance +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18004, 'Evasion +1', 'Evasion +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18005, 'Faint glow', 'Faint glow', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18006, 'Vigor +1', 'Vigor +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18007, 'Armor +1', 'Armor +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18008, 'Swiftness +1', 'Swiftness +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18009, 'Healing +1', 'Healing +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18010, 'Skill +1', 'Skill +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18011, 'Knowledge +1', 'Knowledge +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18012, 'Endurance +1', 'Endurance +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18013, 'Craft +1', 'Craft +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18014, 'Fortitude +1', 'Fortitude +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18015, 'Drive +1', 'Drive +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18016, 'Recovery -1', 'Recovery -1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18501, 'Rune: Vitality +1', 'Rune: Vitality +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18502, 'Rune: Accuracy +1', 'Rune: Accuracy +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18503, 'Rune: Resistance +1', 'Rune: Resistance +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18504, 'Rune: Vigor +1', 'Rune: Vigor +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18505, 'Rune: Armor +1', 'Rune: Armor +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18506, 'Rune: Swiftness +1', 'Rune: Swiftness +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18507, 'Rune: Healing +1', 'Rune: Healing +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18508, 'Rune: Evasion +1', 'Rune: Evasion +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18509, 'Rune: Skill +1', 'Rune: Skill +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18510, 'Rune: Knowledge +1', 'Rune: Knowledge +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18511, 'Rune: Endurance +1', 'Rune: Endurance +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18512, 'Rune: Craft +1', 'Rune: Craft +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18513, 'Rune: Fortitude +1', 'Rune: Fortitude +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18514, 'Rune: Drive +1', 'Rune: Drive +1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18515, 'Rune: Recovery -1', 'Rune: Recovery -1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (18017, 'Noggenfogger transformation', 'Noggenfogger transformation', 0, 16591, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0);
    INSERT INTO `spell_item_enchantment_locale` (`ID`, `locale`, `Name_lang`, `HordeName_lang`, `VerifiedBuild`) VALUES
        (18001, 'ruRU', 'Чары точности: +1 Меткость', 'Чары точности: +1 Меткость', 0),
        (18002, 'ruRU', 'Чары живучести: +1 ОЗ', 'Чары живучести: +1 ОЗ', 0),
        (18003, 'ruRU', 'Чары стойкости: +1 Сопротивление', 'Чары стойкости: +1 Сопротивление', 0),
        (18004, 'ruRU', 'Чары ловкости: +1 Уклонение', 'Чары ловкости: +1 Уклонение', 0),
        (18005, 'ruRU', 'Слабое сияние', 'Слабое сияние', 0),
        (18006, 'ruRU', 'Чары бодрости: +1 ОЭ', 'Чары бодрости: +1 ОЭ', 0),
        (18007, 'ruRU', 'Чары прочности: +1 Броня', 'Чары прочности: +1 Броня', 0),
        (18008, 'ruRU', 'Чары стремительности: +1 Скорость', 'Чары стремительности: +1 Скорость', 0),
        (18009, 'ruRU', 'Чары исцеления: +1 Исцеление', 'Чары исцеления: +1 Исцеление', 0),
        (18010, 'ruRU', 'Чары сноровки: +1 Навык', 'Чары сноровки: +1 Навык', 0),
        (18011, 'ruRU', 'Чары мудрости: +1 Знание', 'Чары мудрости: +1 Знание', 0),
        (18012, 'ruRU', 'Чары упорства: +1 Упорство', 'Чары упорства: +1 Упорство', 0),
        (18013, 'ruRU', 'Чары мастерства: +1 Ремесло', 'Чары мастерства: +1 Ремесло', 0),
        (18014, 'ruRU', 'Чары выносливости: +1 Предел ран', 'Чары выносливости: +1 Предел ран', 0),
        (18015, 'ruRU', 'Чары напора: +1 Лимит затрат энергии', 'Чары напора: +1 Лимит затрат энергии', 0),
        (18016, 'ruRU', 'Чары восстановления: -1 Сложность исцеления', 'Чары восстановления: -1 Сложность исцеления', 0),
        (18501, 'ruRU', 'Руна живучести: +1 ОЗ', 'Руна живучести: +1 ОЗ', 0),
        (18502, 'ruRU', 'Руна точности: +1 Меткость', 'Руна точности: +1 Меткость', 0),
        (18503, 'ruRU', 'Руна стойкости: +1 Сопротивление', 'Руна стойкости: +1 Сопротивление', 0),
        (18504, 'ruRU', 'Руна бодрости: +1 ОЭ', 'Руна бодрости: +1 ОЭ', 0),
        (18505, 'ruRU', 'Руна прочности: +1 Броня', 'Руна прочности: +1 Броня', 0),
        (18506, 'ruRU', 'Руна стремительности: +1 Скорость', 'Руна стремительности: +1 Скорость', 0),
        (18507, 'ruRU', 'Руна исцеления: +1 Исцеление', 'Руна исцеления: +1 Исцеление', 0),
        (18508, 'ruRU', 'Руна ловкости: +1 Уклонение', 'Руна ловкости: +1 Уклонение', 0),
        (18509, 'ruRU', 'Руна сноровки: +1 Навык', 'Руна сноровки: +1 Навык', 0),
        (18510, 'ruRU', 'Руна мудрости: +1 Знание', 'Руна мудрости: +1 Знание', 0),
        (18511, 'ruRU', 'Руна упорства: +1 Упорство', 'Руна упорства: +1 Упорство', 0),
        (18512, 'ruRU', 'Руна мастерства: +1 Ремесло', 'Руна мастерства: +1 Ремесло', 0),
        (18513, 'ruRU', 'Руна выносливости: +1 Предел ран', 'Руна выносливости: +1 Предел ран', 0),
        (18514, 'ruRU', 'Руна напора: +1 Лимит затрат энергии', 'Руна напора: +1 Лимит затрат энергии', 0),
        (18515, 'ruRU', 'Руна восстановления: -1 Сложность исцеления', 'Руна восстановления: -1 Сложность исцеления', 0),
        (18017, 'ruRU', 'Превращение Гогельмогеля', 'Превращение Гогельмогеля', 0);
    INSERT INTO `item_bonus` (`ID`, `Value1`, `Value2`, `Value3`, `Value4`, `ParentItemBonusListID`, `Type`, `OrderIndex`, `VerifiedBuild`) VALUES
        (1800101, 1, 0, 0, 0, 60001, 3, 0, 0),
        (1800102, 2, 0, 0, 0, 60002, 3, 0, 0),
        (1800103, 3, 0, 0, 0, 60003, 3, 0, 0),
        (1800104, 4, 0, 0, 0, 60004, 3, 0, 0),
        (1800105, 5, 0, 0, 0, 60005, 3, 0, 0);
    INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
        (1203200, 1574653623, 3764045193, 18001, 1, 0),
        (1203201, 1513480832, 3764045193, 18002, 1, 0),
        (1203202, 1380453596, 3764045193, 18003, 1, 0),
        (1203203, 1322975094, 3764045193, 18004, 1, 0),
        (1203204, 861724824, 3764045193, 18005, 1, 0),
        (1203205, 1141960540, 3764045193, 18006, 1, 0),
        (1203206, 1911187476, 3764045193, 18007, 1, 0),
        (1203207, 297641439, 3764045193, 18008, 1, 0),
        (1203208, 1666718060, 3764045193, 18009, 1, 0),
        (1203209, 2116560279, 3764045193, 18010, 1, 0),
        (1203210, 1257567895, 3764045193, 18011, 1, 0),
        (1203211, 1070836552, 3764045193, 18012, 1, 0),
        (1203212, 1753714836, 3764045193, 18013, 1, 0),
        (1203213, 203121888, 3764045193, 18014, 1, 0),
        (1203214, 1503697342, 3764045193, 18015, 1, 0),
        (1203215, 1405029197, 3764045193, 18016, 1, 0),
        (1203216, 726913703, 3764045193, 18501, 1, 0),
        (1203217, 798516989, 3764045193, 18502, 1, 0),
        (1203218, 1060767522, 3764045193, 18503, 1, 0),
        (1203219, 556734497, 3764045193, 18504, 1, 0),
        (1203220, 2042437501, 3764045193, 18505, 1, 0),
        (1203221, 1164954107, 3764045193, 18506, 1, 0),
        (1203222, 212623924, 3764045193, 18507, 1, 0),
        (1203223, 1762015044, 3764045193, 18508, 1, 0),
        (1203224, 1557154759, 3764045193, 18509, 1, 0),
        (1203225, 556363046, 3764045193, 18510, 1, 0),
        (1203226, 184596051, 3764045193, 18511, 1, 0),
        (1203227, 1626363145, 3764045193, 18512, 1, 0),
        (1203228, 761072827, 3764045193, 18513, 1, 0),
        (1203229, 1000082303, 3764045193, 18514, 1, 0),
        (1203230, 572040310, 3764045193, 18515, 1, 0),
        (1203231, 346247156, 3764045193, 18017, 1, 0),
        (1203232, 1536408823, 2199425034, 1800101, 1, 0),
        (1203233, 1767974002, 2199425034, 1800102, 1, 0),
        (1203234, 836784590, 2199425034, 1800103, 1, 0),
        (1203235, 1475679033, 2199425034, 1800104, 1, 0),
        (1203236, 259286661, 2199425034, 1800105, 1, 0);
    REPLACE INTO `rps_custom_data_packages` (`package_key`, `payload_sha256`) VALUES ('rps-item-enchants-hotfixes', 'c4ebc9a8a9a1cf8e769c7f459b2d0a62c6d2ae2e3a4c5c4213917c5c9ea7c883');
    COMMIT;
END$$
DELIMITER ;
CALL rps_item_enchants_hotfixes();
DROP PROCEDURE rps_item_enchants_hotfixes;
-- END RPS SECTION item_enchants

-- BEGIN RPS SECTION consumables
-- Generated by contrib/rps_combat/generate_consumables.py; do not edit.
-- HOTFIXES only; stop writers; no --force. Exact rerun is a no-op; drift/collisions abort.
-- Requires current RoleplayCore schema and NO_VENDOR_VALUE (FlagsCu=16).
SET NAMES utf8mb4;
CREATE TABLE IF NOT EXISTS rps_custom_data_packages (package_key varchar(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY, payload_sha256 char(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, installed_at timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP) ENGINE=InnoDB;
DROP PROCEDURE IF EXISTS rps_consumables_20261028_hotfixes;
DELIMITER $$
CREATE PROCEDURE rps_consumables_20261028_hotfixes()
body: BEGIN
DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
CREATE TEMPORARY TABLE `rps_consumables_item` ENGINE=InnoDB AS SELECT * FROM `item` WHERE 0;
INSERT INTO `rps_consumables_item` (`ID`, `ClassID`, `SubclassID`, `Material`, `InventoryType`, `SheatheType`, `SoundOverrideSubclassID`, `IconFileDataID`, `ItemGroupSoundsID`, `ContentTuningID`, `ModifiedCraftingReagentItemID`, `Unknown1200`, `CraftingQualityID`, `ItemSquishEraID`, `RecraftReagentCountPercentage`, `OrderSource`, `VerifiedBuild`) VALUES
    (1801001, 0, 1, 3, 0, 0, -1, 134829, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    (1801002, 0, 1, 3, 0, 0, -1, 134850, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    (1801003, 15, 0, 0, 0, 0, -1, 132382, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    (1801004, 15, 0, 0, 0, 0, -1, 132384, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    (1801005, 15, 0, 0, 0, 0, -1, 133972, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    (1801006, 15, 0, 0, 0, 0, -1, 237296, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    (1801007, 15, 0, 0, 0, 0, -1, 133685, 0, 0, 0, 0, 0, 0, 0, 0, 0);
CREATE TEMPORARY TABLE `rps_consumables_item_sparse` ENGINE=InnoDB AS SELECT * FROM `item_sparse` WHERE 0;
INSERT INTO `rps_consumables_item_sparse` (`ID`, `Description`, `Display3`, `Display2`, `Display1`, `Display`, `ExpansionID`, `DmgVariance`, `LimitCategory`, `DurationInInventory`, `QualityModifier`, `BagFamily`, `StartQuestID`, `LanguageID`, `ItemRange`, `StatPercentageOfSocket1`, `StatPercentageOfSocket2`, `StatPercentageOfSocket3`, `StatPercentageOfSocket4`, `StatPercentageOfSocket5`, `StatPercentageOfSocket6`, `StatPercentageOfSocket7`, `StatPercentageOfSocket8`, `StatPercentageOfSocket9`, `StatPercentageOfSocket10`, `StatPercentEditor1`, `StatPercentEditor2`, `StatPercentEditor3`, `StatPercentEditor4`, `StatPercentEditor5`, `StatPercentEditor6`, `StatPercentEditor7`, `StatPercentEditor8`, `StatPercentEditor9`, `StatPercentEditor10`, `StatModifierBonusStat1`, `StatModifierBonusStat2`, `StatModifierBonusStat3`, `StatModifierBonusStat4`, `StatModifierBonusStat5`, `StatModifierBonusStat6`, `StatModifierBonusStat7`, `StatModifierBonusStat8`, `StatModifierBonusStat9`, `StatModifierBonusStat10`, `Stackable`, `MaxCount`, `MinReputation`, `RequiredAbility`, `AllowableRace1`, `AllowableRace2`, `SellPrice`, `BuyPrice`, `VendorStackCount`, `PriceVariance`, `PriceRandomValue`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `FactionRelated`, `ModifiedCraftingReagentItemID`, `ContentTuningID`, `PlayerLevelToItemLevelCurveID`, `ItemLevelOffsetCurveID`, `ItemLevelOffsetItemLevel`, `ItemSquishEraID`, `ItemNameDescriptionID`, `RequiredTransmogHoliday`, `RequiredHoliday`, `GemProperties`, `SocketMatchEnchantmentId`, `TotemCategoryID`, `InstanceBound`, `ZoneBound1`, `ZoneBound2`, `ItemSet`, `LockID`, `PageID`, `ItemDelay`, `MinFactionID`, `RequiredSkillRank`, `RequiredSkill`, `ItemLevel`, `AllowableClass`, `ArtifactID`, `SpellWeight`, `SpellWeightCategory`, `SocketType1`, `SocketType2`, `SocketType3`, `SheatheType`, `Material`, `PageMaterialID`, `Bonding`, `DamageDamageType`, `ContainerSlots`, `RequiredPVPMedal`, `RequiredPVPRank`, `RequiredLevel`, `InventoryType`, `OverallQualityID`, `VerifiedBuild`) VALUES
    (1801001, 'Restores up to 10 RP health to you or the selected player outside combat.', '', '', '', 'Health Potion', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0),
    (1801002, 'Restores up to 5 RP energy to you or the selected player outside combat.', '', '', '', 'Mana Potion', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0),
    (1801003, 'Roleplay supply. Automatic ammunition consumption is not implemented yet.', '', '', '', 'Arrow', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 200, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0),
    (1801004, 'Roleplay supply. Automatic ammunition consumption is not implemented yet.', '', '', '', 'Bullet', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 200, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0),
    (1801005, 'Roleplay supply. Automatic recovery on use is not implemented yet.', '', '', '', 'Ration', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 200, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0),
    (1801006, 'Roleplay supply. Automatic armor restoration is not implemented yet.', '', '', '', 'Repair Kit', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 200, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0),
    (1801007, 'Roleplay supply. Automatic wound treatment is not implemented yet.', '', '', '', 'First Aid Kit', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 200, 0, 0, 0, -1, -1, 0, 0, 1, 0, 0, 32768, 16777216, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0);
CREATE TEMPORARY TABLE `rps_consumables_item_search_name` ENGINE=InnoDB AS SELECT * FROM `item_search_name` WHERE 0;
INSERT INTO `rps_consumables_item_search_name` (`ID`, `Display`, `OverallQualityID`, `ExpansionID`, `MinFactionID`, `MinReputation`, `AllowableClass`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `RequiredAbility`, `ItemLevel`, `Flags1`, `Flags2`, `Flags3`, `Flags4`, `Flags5`, `AllowableRace1`, `AllowableRace2`, `VerifiedBuild`) VALUES
    (1801001, 'Health Potion', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0),
    (1801002, 'Mana Potion', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0),
    (1801003, 'Arrow', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0),
    (1801004, 'Bullet', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0),
    (1801005, 'Ration', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0),
    (1801006, 'Repair Kit', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0),
    (1801007, 'First Aid Kit', 1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 32768, 16777216, 0, 0, 0, -1, -1, 0);
CREATE TEMPORARY TABLE `rps_consumables_item_effect` ENGINE=InnoDB AS SELECT * FROM `item_effect` WHERE 0;
INSERT INTO `rps_consumables_item_effect` (`ID`, `LegacySlotIndex`, `TriggerType`, `Charges`, `CoolDownMSec`, `CategoryCoolDownMSec`, `SpellCategoryID`, `SpellID`, `ChrSpecializationID`, `PlayerConditionID`, `VerifiedBuild`) VALUES
    (1801001, 0, 0, 0, -1, -1, 0, 1801101, 0, 0, 0),
    (1801002, 0, 0, 0, -1, -1, 0, 1801102, 0, 0, 0);
CREATE TEMPORARY TABLE `rps_consumables_item_x_item_effect` ENGINE=InnoDB AS SELECT * FROM `item_x_item_effect` WHERE 0;
INSERT INTO `rps_consumables_item_x_item_effect` (`ID`, `ItemEffectID`, `ItemID`, `VerifiedBuild`) VALUES
    (1801001, 1801001, 1801001, 0),
    (1801002, 1801002, 1801002, 0);
CREATE TEMPORARY TABLE `rps_consumables_spell` ENGINE=InnoDB AS SELECT * FROM `spell` WHERE 0;
INSERT INTO `rps_consumables_spell` (`ID`, `NameSubtext`, `Description`, `AuraDescription`, `VerifiedBuild`) VALUES
    (1801101, '', 'Restores up to 10 RP health to you or the selected player outside combat.', '', 0),
    (1801102, '', 'Restores up to 5 RP energy to you or the selected player outside combat.', '', 0);
CREATE TEMPORARY TABLE `rps_consumables_spell_name` ENGINE=InnoDB AS SELECT * FROM `spell_name` WHERE 0;
INSERT INTO `rps_consumables_spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES
    (1801101, 'Health Potion', 0),
    (1801102, 'Mana Potion', 0);
CREATE TEMPORARY TABLE `rps_consumables_spell_misc` ENGINE=InnoDB AS SELECT * FROM `spell_misc` WHERE 0;
INSERT INTO `rps_consumables_spell_misc` (`ID`, `Attributes1`, `Attributes2`, `Attributes3`, `Attributes4`, `Attributes5`, `Attributes6`, `Attributes7`, `Attributes8`, `Attributes9`, `Attributes10`, `Attributes11`, `Attributes12`, `Attributes13`, `Attributes14`, `Attributes15`, `Attributes16`, `Attributes17`, `DifficultyID`, `CastingTimeIndex`, `DurationIndex`, `PvPDurationIndex`, `RangeIndex`, `SchoolMask`, `Speed`, `LaunchDelay`, `MinDuration`, `SpellIconFileDataID`, `ActiveIconFileDataID`, `ContentTuningID`, `ShowFutureSpellPlayerConditionID`, `SpellVisualScript`, `ActiveSpellVisualScript`, `SpellID`, `VerifiedBuild`) VALUES
    (1801101, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 4, 1, 0, 0, 0, 134829, 0, 0, 0, 0, 0, 1801101, 0),
    (1801102, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 4, 1, 0, 0, 0, 134850, 0, 0, 0, 0, 0, 1801102, 0);
CREATE TEMPORARY TABLE `rps_consumables_spell_effect` ENGINE=InnoDB AS SELECT * FROM `spell_effect` WHERE 0;
INSERT INTO `rps_consumables_spell_effect` (`ID`, `EffectAura`, `DifficultyID`, `EffectIndex`, `Effect`, `EffectAmplitude`, `EffectAttributes`, `EffectAuraPeriod`, `EffectBonusCoefficient`, `EffectChainAmplitude`, `EffectChainTargets`, `EffectItemType`, `EffectMechanic`, `EffectPointsPerResource`, `EffectPosFacing`, `EffectRealPointsPerLevel`, `EffectTriggerSpell`, `BonusCoefficientFromAP`, `PvpMultiplier`, `Coefficient`, `Variance`, `ResourceCoefficient`, `GroupSizeBasePointsCoefficient`, `EffectBasePoints`, `ScalingClass`, `TargetNodeGraph`, `EffectMiscValue1`, `EffectMiscValue2`, `EffectRadiusIndex1`, `EffectRadiusIndex2`, `EffectSpellClassMask1`, `EffectSpellClassMask2`, `EffectSpellClassMask3`, `EffectSpellClassMask4`, `ImplicitTarget1`, `ImplicitTarget2`, `SpellID`, `VerifiedBuild`) VALUES
    (1801101, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 0, 1801101, 0),
    (1801102, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 0, 1801102, 0);
CREATE TEMPORARY TABLE `rps_consumables_item_sparse_locale` ENGINE=InnoDB AS SELECT * FROM `item_sparse_locale` WHERE 0;
INSERT INTO `rps_consumables_item_sparse_locale` (`ID`, `locale`, `Description_lang`, `Display3_lang`, `Display2_lang`, `Display1_lang`, `Display_lang`, `VerifiedBuild`) VALUES
    (1801001, 'ruRU', 'Восстанавливает до 10 ОЗ вам или выбранному игроку вне боя.', '', '', '', 'Зелье здоровья', 0),
    (1801002, 'ruRU', 'Восстанавливает до 5 ОЭ вам или выбранному игроку вне боя.', '', '', '', 'Зелье маны', 0),
    (1801003, 'ruRU', 'Ролевой припас. Автоматический расход при стрельбе пока не реализован.', '', '', '', 'Стрела', 0),
    (1801004, 'ruRU', 'Ролевой припас. Автоматический расход при стрельбе пока не реализован.', '', '', '', 'Пуля', 0),
    (1801005, 'ruRU', 'Ролевой припас. Автоматическое восстановление при использовании пока не реализовано.', '', '', '', 'Паёк', 0),
    (1801006, 'ruRU', 'Ролевой припас. Автоматическое восстановление брони пока не реализовано.', '', '', '', 'Ремонтный набор', 0),
    (1801007, 'ruRU', 'Ролевой припас. Автоматическое лечение ран пока не реализовано.', '', '', '', 'Аптечка', 0);
CREATE TEMPORARY TABLE `rps_consumables_item_search_name_locale` ENGINE=InnoDB AS SELECT * FROM `item_search_name_locale` WHERE 0;
INSERT INTO `rps_consumables_item_search_name_locale` (`ID`, `locale`, `Display_lang`, `VerifiedBuild`) VALUES
    (1801001, 'ruRU', 'Зелье здоровья', 0),
    (1801002, 'ruRU', 'Зелье маны', 0),
    (1801003, 'ruRU', 'Стрела', 0),
    (1801004, 'ruRU', 'Пуля', 0),
    (1801005, 'ruRU', 'Паёк', 0),
    (1801006, 'ruRU', 'Ремонтный набор', 0),
    (1801007, 'ruRU', 'Аптечка', 0);
CREATE TEMPORARY TABLE `rps_consumables_spell_locale` ENGINE=InnoDB AS SELECT * FROM `spell_locale` WHERE 0;
INSERT INTO `rps_consumables_spell_locale` (`ID`, `locale`, `NameSubtext_lang`, `Description_lang`, `AuraDescription_lang`, `VerifiedBuild`) VALUES
    (1801101, 'ruRU', '', 'Восстанавливает до 10 ОЗ вам или выбранному игроку вне боя.', '', 0),
    (1801102, 'ruRU', '', 'Восстанавливает до 5 ОЭ вам или выбранному игроку вне боя.', '', 0);
CREATE TEMPORARY TABLE `rps_consumables_spell_name_locale` ENGINE=InnoDB AS SELECT * FROM `spell_name_locale` WHERE 0;
INSERT INTO `rps_consumables_spell_name_locale` (`ID`, `locale`, `Name_lang`, `VerifiedBuild`) VALUES
    (1801101, 'ruRU', 'Зелье здоровья', 0),
    (1801102, 'ruRU', 'Зелье маны', 0);
CREATE TEMPORARY TABLE `rps_consumables_hotfix_data` ENGINE=InnoDB AS SELECT * FROM `hotfix_data` WHERE 0;
INSERT INTO `rps_consumables_hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
    (1203400, 1470691422, 1344507586, 1801001, 1, 0),
    (1203401, 1201497668, 1344507586, 1801002, 1, 0),
    (1203402, 568408724, 1344507586, 1801003, 1, 0),
    (1203403, 1590009510, 1344507586, 1801004, 1, 0),
    (1203404, 961161737, 1344507586, 1801005, 1, 0),
    (1203405, 440423623, 1344507586, 1801006, 1, 0),
    (1203406, 938299241, 1344507586, 1801007, 1, 0),
    (1203407, 2025875162, 2442913102, 1801001, 1, 0),
    (1203408, 129347979, 2442913102, 1801002, 1, 0),
    (1203409, 1997590001, 2442913102, 1801003, 1, 0),
    (1203410, 17257556, 2442913102, 1801004, 1, 0),
    (1203411, 952071150, 2442913102, 1801005, 1, 0),
    (1203412, 1721167342, 2442913102, 1801006, 1, 0),
    (1203413, 1359577385, 2442913102, 1801007, 1, 0),
    (1203414, 710745934, 428746933, 1801001, 1, 0),
    (1203415, 1711064303, 428746933, 1801002, 1, 0),
    (1203416, 526124583, 428746933, 1801003, 1, 0),
    (1203417, 2047475239, 428746933, 1801004, 1, 0),
    (1203418, 1748288299, 428746933, 1801005, 1, 0),
    (1203419, 2072742767, 428746933, 1801006, 1, 0),
    (1203420, 344336641, 428746933, 1801007, 1, 0),
    (1203421, 1589121735, 1073915313, 1801001, 1, 0),
    (1203422, 195338505, 1073915313, 1801002, 1, 0),
    (1203423, 449437858, 13330255, 1801001, 1, 0),
    (1203424, 183503063, 13330255, 1801002, 1, 0),
    (1203425, 75542336, 3776013982, 1801101, 1, 0),
    (1203426, 110631181, 3776013982, 1801102, 1, 0),
    (1203427, 1787688759, 1187407512, 1801101, 1, 0),
    (1203428, 894847996, 1187407512, 1801102, 1, 0),
    (1203429, 769759527, 3322146344, 1801101, 1, 0),
    (1203430, 1435184974, 3322146344, 1801102, 1, 0),
    (1203431, 825031353, 4030871717, 1801101, 1, 0),
    (1203432, 335594623, 4030871717, 1801102, 1, 0);
START TRANSACTION;
IF EXISTS (SELECT 1 FROM `artifact_power_rank` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `azerite_empowered_item` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `azerite_item` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `azerite_power` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `character_loadout_item` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `flight_capability` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `glyph_bindable_spell` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `glyph_properties` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `guild_perk_spells` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `heirloom` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `item_child_equipment` WHERE `ParentItemID` BETWEEN 1801001 AND 1801007 OR `ChildItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `item_currency_cost` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `item_modified_appearance` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `item_set_spell` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `item_spec_override` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `item_x_bonus_tree` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `journal_encounter_section` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `liquid_type` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `maw_power` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `pvp_talent` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `quest_package_item` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `reward_pack_x_item` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `soulbind_conduit_rank` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `specialization_spells` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_aura_options` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_aura_restrictions` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_casting_requirements` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_categories` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_class_options` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_cooldowns` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_empower` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_equipped_items` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_interrupts` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_label` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_learn_spell` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_levels` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_power` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_reagents` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_reagents_currency` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_scaling` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_shapeshift` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_target_restrictions` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_totems` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `spell_x_spell_visual` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `talent` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `toy` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM `trait_definition` WHERE `SpellID` BETWEEN 1801101 AND 1801102) OR
EXISTS (SELECT 1 FROM `trait_system` WHERE `ItemID` BETWEEN 1801001 AND 1801007) OR
EXISTS (SELECT 1 FROM hotfix_blob WHERE (`TableHash`=1344507586 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=2442913102 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=428746933 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=1073915313 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=13330255 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=3776013982 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=1187407512 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=3322146344 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=4030871717 AND `RecordId` BETWEEN 1801101 AND 1801102)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Foreign consumable references or blobs'; END IF;
IF EXISTS (SELECT 1 FROM rps_custom_data_packages WHERE package_key='rps-consumables-v1-hotfixes') THEN
IF NOT EXISTS (SELECT 1 FROM rps_custom_data_packages WHERE package_key='rps-consumables-v1-hotfixes' AND payload_sha256='8f73d4bb173a818a32f9a681824623175e07744b6cc725fa755a97f58d66eed6') THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable checksum changed; reviewed upgrade required'; END IF;
IF (SELECT COUNT(*) FROM `item` WHERE `ID` BETWEEN 1801001 AND 1801007) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_item` e WHERE NOT EXISTS (SELECT 1 FROM `item` a WHERE a.`ID` <=> e.`ID` AND a.`ClassID` <=> e.`ClassID` AND a.`SubclassID` <=> e.`SubclassID` AND a.`Material` <=> e.`Material` AND a.`InventoryType` <=> e.`InventoryType` AND a.`SheatheType` <=> e.`SheatheType` AND a.`SoundOverrideSubclassID` <=> e.`SoundOverrideSubclassID` AND a.`IconFileDataID` <=> e.`IconFileDataID` AND a.`ItemGroupSoundsID` <=> e.`ItemGroupSoundsID` AND a.`ContentTuningID` <=> e.`ContentTuningID` AND a.`ModifiedCraftingReagentItemID` <=> e.`ModifiedCraftingReagentItemID` AND a.`Unknown1200` <=> e.`Unknown1200` AND a.`CraftingQualityID` <=> e.`CraftingQualityID` AND a.`ItemSquishEraID` <=> e.`ItemSquishEraID` AND a.`RecraftReagentCountPercentage` <=> e.`RecraftReagentCountPercentage` AND a.`OrderSource` <=> e.`OrderSource` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item'; END IF;
IF (SELECT COUNT(*) FROM `item_sparse` WHERE `ID` BETWEEN 1801001 AND 1801007) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_item_sparse` e WHERE NOT EXISTS (SELECT 1 FROM `item_sparse` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`Description` AS BINARY) <=> CAST(e.`Description` AS BINARY) AND CAST(a.`Display3` AS BINARY) <=> CAST(e.`Display3` AS BINARY) AND CAST(a.`Display2` AS BINARY) <=> CAST(e.`Display2` AS BINARY) AND CAST(a.`Display1` AS BINARY) <=> CAST(e.`Display1` AS BINARY) AND CAST(a.`Display` AS BINARY) <=> CAST(e.`Display` AS BINARY) AND a.`ExpansionID` <=> e.`ExpansionID` AND a.`DmgVariance` <=> e.`DmgVariance` AND a.`LimitCategory` <=> e.`LimitCategory` AND a.`DurationInInventory` <=> e.`DurationInInventory` AND a.`QualityModifier` <=> e.`QualityModifier` AND a.`BagFamily` <=> e.`BagFamily` AND a.`StartQuestID` <=> e.`StartQuestID` AND a.`LanguageID` <=> e.`LanguageID` AND a.`ItemRange` <=> e.`ItemRange` AND a.`StatPercentageOfSocket1` <=> e.`StatPercentageOfSocket1` AND a.`StatPercentageOfSocket2` <=> e.`StatPercentageOfSocket2` AND a.`StatPercentageOfSocket3` <=> e.`StatPercentageOfSocket3` AND a.`StatPercentageOfSocket4` <=> e.`StatPercentageOfSocket4` AND a.`StatPercentageOfSocket5` <=> e.`StatPercentageOfSocket5` AND a.`StatPercentageOfSocket6` <=> e.`StatPercentageOfSocket6` AND a.`StatPercentageOfSocket7` <=> e.`StatPercentageOfSocket7` AND a.`StatPercentageOfSocket8` <=> e.`StatPercentageOfSocket8` AND a.`StatPercentageOfSocket9` <=> e.`StatPercentageOfSocket9` AND a.`StatPercentageOfSocket10` <=> e.`StatPercentageOfSocket10` AND a.`StatPercentEditor1` <=> e.`StatPercentEditor1` AND a.`StatPercentEditor2` <=> e.`StatPercentEditor2` AND a.`StatPercentEditor3` <=> e.`StatPercentEditor3` AND a.`StatPercentEditor4` <=> e.`StatPercentEditor4` AND a.`StatPercentEditor5` <=> e.`StatPercentEditor5` AND a.`StatPercentEditor6` <=> e.`StatPercentEditor6` AND a.`StatPercentEditor7` <=> e.`StatPercentEditor7` AND a.`StatPercentEditor8` <=> e.`StatPercentEditor8` AND a.`StatPercentEditor9` <=> e.`StatPercentEditor9` AND a.`StatPercentEditor10` <=> e.`StatPercentEditor10` AND a.`StatModifierBonusStat1` <=> e.`StatModifierBonusStat1` AND a.`StatModifierBonusStat2` <=> e.`StatModifierBonusStat2` AND a.`StatModifierBonusStat3` <=> e.`StatModifierBonusStat3` AND a.`StatModifierBonusStat4` <=> e.`StatModifierBonusStat4` AND a.`StatModifierBonusStat5` <=> e.`StatModifierBonusStat5` AND a.`StatModifierBonusStat6` <=> e.`StatModifierBonusStat6` AND a.`StatModifierBonusStat7` <=> e.`StatModifierBonusStat7` AND a.`StatModifierBonusStat8` <=> e.`StatModifierBonusStat8` AND a.`StatModifierBonusStat9` <=> e.`StatModifierBonusStat9` AND a.`StatModifierBonusStat10` <=> e.`StatModifierBonusStat10` AND a.`Stackable` <=> e.`Stackable` AND a.`MaxCount` <=> e.`MaxCount` AND a.`MinReputation` <=> e.`MinReputation` AND a.`RequiredAbility` <=> e.`RequiredAbility` AND a.`AllowableRace1` <=> e.`AllowableRace1` AND a.`AllowableRace2` <=> e.`AllowableRace2` AND a.`SellPrice` <=> e.`SellPrice` AND a.`BuyPrice` <=> e.`BuyPrice` AND a.`VendorStackCount` <=> e.`VendorStackCount` AND a.`PriceVariance` <=> e.`PriceVariance` AND a.`PriceRandomValue` <=> e.`PriceRandomValue` AND a.`Flags1` <=> e.`Flags1` AND a.`Flags2` <=> e.`Flags2` AND a.`Flags3` <=> e.`Flags3` AND a.`Flags4` <=> e.`Flags4` AND a.`Flags5` <=> e.`Flags5` AND a.`FactionRelated` <=> e.`FactionRelated` AND a.`ModifiedCraftingReagentItemID` <=> e.`ModifiedCraftingReagentItemID` AND a.`ContentTuningID` <=> e.`ContentTuningID` AND a.`PlayerLevelToItemLevelCurveID` <=> e.`PlayerLevelToItemLevelCurveID` AND a.`ItemLevelOffsetCurveID` <=> e.`ItemLevelOffsetCurveID` AND a.`ItemLevelOffsetItemLevel` <=> e.`ItemLevelOffsetItemLevel` AND a.`ItemSquishEraID` <=> e.`ItemSquishEraID` AND a.`ItemNameDescriptionID` <=> e.`ItemNameDescriptionID` AND a.`RequiredTransmogHoliday` <=> e.`RequiredTransmogHoliday` AND a.`RequiredHoliday` <=> e.`RequiredHoliday` AND a.`GemProperties` <=> e.`GemProperties` AND a.`SocketMatchEnchantmentId` <=> e.`SocketMatchEnchantmentId` AND a.`TotemCategoryID` <=> e.`TotemCategoryID` AND a.`InstanceBound` <=> e.`InstanceBound` AND a.`ZoneBound1` <=> e.`ZoneBound1` AND a.`ZoneBound2` <=> e.`ZoneBound2` AND a.`ItemSet` <=> e.`ItemSet` AND a.`LockID` <=> e.`LockID` AND a.`PageID` <=> e.`PageID` AND a.`ItemDelay` <=> e.`ItemDelay` AND a.`MinFactionID` <=> e.`MinFactionID` AND a.`RequiredSkillRank` <=> e.`RequiredSkillRank` AND a.`RequiredSkill` <=> e.`RequiredSkill` AND a.`ItemLevel` <=> e.`ItemLevel` AND a.`AllowableClass` <=> e.`AllowableClass` AND a.`ArtifactID` <=> e.`ArtifactID` AND a.`SpellWeight` <=> e.`SpellWeight` AND a.`SpellWeightCategory` <=> e.`SpellWeightCategory` AND a.`SocketType1` <=> e.`SocketType1` AND a.`SocketType2` <=> e.`SocketType2` AND a.`SocketType3` <=> e.`SocketType3` AND a.`SheatheType` <=> e.`SheatheType` AND a.`Material` <=> e.`Material` AND a.`PageMaterialID` <=> e.`PageMaterialID` AND a.`Bonding` <=> e.`Bonding` AND a.`DamageDamageType` <=> e.`DamageDamageType` AND a.`ContainerSlots` <=> e.`ContainerSlots` AND a.`RequiredPVPMedal` <=> e.`RequiredPVPMedal` AND a.`RequiredPVPRank` <=> e.`RequiredPVPRank` AND a.`RequiredLevel` <=> e.`RequiredLevel` AND a.`InventoryType` <=> e.`InventoryType` AND a.`OverallQualityID` <=> e.`OverallQualityID` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_sparse'; END IF;
IF (SELECT COUNT(*) FROM `item_search_name` WHERE `ID` BETWEEN 1801001 AND 1801007) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_item_search_name` e WHERE NOT EXISTS (SELECT 1 FROM `item_search_name` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`Display` AS BINARY) <=> CAST(e.`Display` AS BINARY) AND a.`OverallQualityID` <=> e.`OverallQualityID` AND a.`ExpansionID` <=> e.`ExpansionID` AND a.`MinFactionID` <=> e.`MinFactionID` AND a.`MinReputation` <=> e.`MinReputation` AND a.`AllowableClass` <=> e.`AllowableClass` AND a.`RequiredLevel` <=> e.`RequiredLevel` AND a.`RequiredSkill` <=> e.`RequiredSkill` AND a.`RequiredSkillRank` <=> e.`RequiredSkillRank` AND a.`RequiredAbility` <=> e.`RequiredAbility` AND a.`ItemLevel` <=> e.`ItemLevel` AND a.`Flags1` <=> e.`Flags1` AND a.`Flags2` <=> e.`Flags2` AND a.`Flags3` <=> e.`Flags3` AND a.`Flags4` <=> e.`Flags4` AND a.`Flags5` <=> e.`Flags5` AND a.`AllowableRace1` <=> e.`AllowableRace1` AND a.`AllowableRace2` <=> e.`AllowableRace2` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_search_name'; END IF;
IF (SELECT COUNT(*) FROM `item_effect` WHERE (`ID` BETWEEN 1801001 AND 1801007 OR `SpellID` BETWEEN 1801101 AND 1801102)) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_item_effect` e WHERE NOT EXISTS (SELECT 1 FROM `item_effect` a WHERE a.`ID` <=> e.`ID` AND a.`LegacySlotIndex` <=> e.`LegacySlotIndex` AND a.`TriggerType` <=> e.`TriggerType` AND a.`Charges` <=> e.`Charges` AND a.`CoolDownMSec` <=> e.`CoolDownMSec` AND a.`CategoryCoolDownMSec` <=> e.`CategoryCoolDownMSec` AND a.`SpellCategoryID` <=> e.`SpellCategoryID` AND a.`SpellID` <=> e.`SpellID` AND a.`ChrSpecializationID` <=> e.`ChrSpecializationID` AND a.`PlayerConditionID` <=> e.`PlayerConditionID` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_effect'; END IF;
IF (SELECT COUNT(*) FROM `item_x_item_effect` WHERE ((`ID` BETWEEN 1801001 AND 1801007 OR `ItemID` BETWEEN 1801001 AND 1801007) OR `ItemEffectID` BETWEEN 1801001 AND 1801007)) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_item_x_item_effect` e WHERE NOT EXISTS (SELECT 1 FROM `item_x_item_effect` a WHERE a.`ID` <=> e.`ID` AND a.`ItemEffectID` <=> e.`ItemEffectID` AND a.`ItemID` <=> e.`ItemID` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_x_item_effect'; END IF;
IF (SELECT COUNT(*) FROM `spell` WHERE `ID` BETWEEN 1801101 AND 1801102) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_spell` e WHERE NOT EXISTS (SELECT 1 FROM `spell` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`NameSubtext` AS BINARY) <=> CAST(e.`NameSubtext` AS BINARY) AND CAST(a.`Description` AS BINARY) <=> CAST(e.`Description` AS BINARY) AND CAST(a.`AuraDescription` AS BINARY) <=> CAST(e.`AuraDescription` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: spell'; END IF;
IF (SELECT COUNT(*) FROM `spell_name` WHERE `ID` BETWEEN 1801101 AND 1801102) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_spell_name` e WHERE NOT EXISTS (SELECT 1 FROM `spell_name` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`Name` AS BINARY) <=> CAST(e.`Name` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: spell_name'; END IF;
IF (SELECT COUNT(*) FROM `spell_misc` WHERE (`ID` BETWEEN 1801101 AND 1801102 OR `SpellID` BETWEEN 1801101 AND 1801102)) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_spell_misc` e WHERE NOT EXISTS (SELECT 1 FROM `spell_misc` a WHERE a.`ID` <=> e.`ID` AND a.`Attributes1` <=> e.`Attributes1` AND a.`Attributes2` <=> e.`Attributes2` AND a.`Attributes3` <=> e.`Attributes3` AND a.`Attributes4` <=> e.`Attributes4` AND a.`Attributes5` <=> e.`Attributes5` AND a.`Attributes6` <=> e.`Attributes6` AND a.`Attributes7` <=> e.`Attributes7` AND a.`Attributes8` <=> e.`Attributes8` AND a.`Attributes9` <=> e.`Attributes9` AND a.`Attributes10` <=> e.`Attributes10` AND a.`Attributes11` <=> e.`Attributes11` AND a.`Attributes12` <=> e.`Attributes12` AND a.`Attributes13` <=> e.`Attributes13` AND a.`Attributes14` <=> e.`Attributes14` AND a.`Attributes15` <=> e.`Attributes15` AND a.`Attributes16` <=> e.`Attributes16` AND a.`Attributes17` <=> e.`Attributes17` AND a.`DifficultyID` <=> e.`DifficultyID` AND a.`CastingTimeIndex` <=> e.`CastingTimeIndex` AND a.`DurationIndex` <=> e.`DurationIndex` AND a.`PvPDurationIndex` <=> e.`PvPDurationIndex` AND a.`RangeIndex` <=> e.`RangeIndex` AND a.`SchoolMask` <=> e.`SchoolMask` AND a.`Speed` <=> e.`Speed` AND a.`LaunchDelay` <=> e.`LaunchDelay` AND a.`MinDuration` <=> e.`MinDuration` AND a.`SpellIconFileDataID` <=> e.`SpellIconFileDataID` AND a.`ActiveIconFileDataID` <=> e.`ActiveIconFileDataID` AND a.`ContentTuningID` <=> e.`ContentTuningID` AND a.`ShowFutureSpellPlayerConditionID` <=> e.`ShowFutureSpellPlayerConditionID` AND a.`SpellVisualScript` <=> e.`SpellVisualScript` AND a.`ActiveSpellVisualScript` <=> e.`ActiveSpellVisualScript` AND a.`SpellID` <=> e.`SpellID` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: spell_misc'; END IF;
IF (SELECT COUNT(*) FROM `spell_effect` WHERE (`ID` BETWEEN 1801101 AND 1801102 OR `SpellID` BETWEEN 1801101 AND 1801102)) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_spell_effect` e WHERE NOT EXISTS (SELECT 1 FROM `spell_effect` a WHERE a.`ID` <=> e.`ID` AND a.`EffectAura` <=> e.`EffectAura` AND a.`DifficultyID` <=> e.`DifficultyID` AND a.`EffectIndex` <=> e.`EffectIndex` AND a.`Effect` <=> e.`Effect` AND a.`EffectAmplitude` <=> e.`EffectAmplitude` AND a.`EffectAttributes` <=> e.`EffectAttributes` AND a.`EffectAuraPeriod` <=> e.`EffectAuraPeriod` AND a.`EffectBonusCoefficient` <=> e.`EffectBonusCoefficient` AND a.`EffectChainAmplitude` <=> e.`EffectChainAmplitude` AND a.`EffectChainTargets` <=> e.`EffectChainTargets` AND a.`EffectItemType` <=> e.`EffectItemType` AND a.`EffectMechanic` <=> e.`EffectMechanic` AND a.`EffectPointsPerResource` <=> e.`EffectPointsPerResource` AND a.`EffectPosFacing` <=> e.`EffectPosFacing` AND a.`EffectRealPointsPerLevel` <=> e.`EffectRealPointsPerLevel` AND a.`EffectTriggerSpell` <=> e.`EffectTriggerSpell` AND a.`BonusCoefficientFromAP` <=> e.`BonusCoefficientFromAP` AND a.`PvpMultiplier` <=> e.`PvpMultiplier` AND a.`Coefficient` <=> e.`Coefficient` AND a.`Variance` <=> e.`Variance` AND a.`ResourceCoefficient` <=> e.`ResourceCoefficient` AND a.`GroupSizeBasePointsCoefficient` <=> e.`GroupSizeBasePointsCoefficient` AND a.`EffectBasePoints` <=> e.`EffectBasePoints` AND a.`ScalingClass` <=> e.`ScalingClass` AND a.`TargetNodeGraph` <=> e.`TargetNodeGraph` AND a.`EffectMiscValue1` <=> e.`EffectMiscValue1` AND a.`EffectMiscValue2` <=> e.`EffectMiscValue2` AND a.`EffectRadiusIndex1` <=> e.`EffectRadiusIndex1` AND a.`EffectRadiusIndex2` <=> e.`EffectRadiusIndex2` AND a.`EffectSpellClassMask1` <=> e.`EffectSpellClassMask1` AND a.`EffectSpellClassMask2` <=> e.`EffectSpellClassMask2` AND a.`EffectSpellClassMask3` <=> e.`EffectSpellClassMask3` AND a.`EffectSpellClassMask4` <=> e.`EffectSpellClassMask4` AND a.`ImplicitTarget1` <=> e.`ImplicitTarget1` AND a.`ImplicitTarget2` <=> e.`ImplicitTarget2` AND a.`SpellID` <=> e.`SpellID` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: spell_effect'; END IF;
IF (SELECT COUNT(*) FROM `item_sparse_locale` WHERE `ID` BETWEEN 1801001 AND 1801007) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_item_sparse_locale` e WHERE NOT EXISTS (SELECT 1 FROM `item_sparse_locale` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`locale` AS BINARY) <=> CAST(e.`locale` AS BINARY) AND CAST(a.`Description_lang` AS BINARY) <=> CAST(e.`Description_lang` AS BINARY) AND CAST(a.`Display3_lang` AS BINARY) <=> CAST(e.`Display3_lang` AS BINARY) AND CAST(a.`Display2_lang` AS BINARY) <=> CAST(e.`Display2_lang` AS BINARY) AND CAST(a.`Display1_lang` AS BINARY) <=> CAST(e.`Display1_lang` AS BINARY) AND CAST(a.`Display_lang` AS BINARY) <=> CAST(e.`Display_lang` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_sparse_locale'; END IF;
IF (SELECT COUNT(*) FROM `item_search_name_locale` WHERE `ID` BETWEEN 1801001 AND 1801007) <> 7 OR EXISTS (SELECT 1 FROM `rps_consumables_item_search_name_locale` e WHERE NOT EXISTS (SELECT 1 FROM `item_search_name_locale` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`locale` AS BINARY) <=> CAST(e.`locale` AS BINARY) AND CAST(a.`Display_lang` AS BINARY) <=> CAST(e.`Display_lang` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: item_search_name_locale'; END IF;
IF (SELECT COUNT(*) FROM `spell_locale` WHERE `ID` BETWEEN 1801101 AND 1801102) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_spell_locale` e WHERE NOT EXISTS (SELECT 1 FROM `spell_locale` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`locale` AS BINARY) <=> CAST(e.`locale` AS BINARY) AND CAST(a.`NameSubtext_lang` AS BINARY) <=> CAST(e.`NameSubtext_lang` AS BINARY) AND CAST(a.`Description_lang` AS BINARY) <=> CAST(e.`Description_lang` AS BINARY) AND CAST(a.`AuraDescription_lang` AS BINARY) <=> CAST(e.`AuraDescription_lang` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: spell_locale'; END IF;
IF (SELECT COUNT(*) FROM `spell_name_locale` WHERE `ID` BETWEEN 1801101 AND 1801102) <> 2 OR EXISTS (SELECT 1 FROM `rps_consumables_spell_name_locale` e WHERE NOT EXISTS (SELECT 1 FROM `spell_name_locale` a WHERE a.`ID` <=> e.`ID` AND CAST(a.`locale` AS BINARY) <=> CAST(e.`locale` AS BINARY) AND CAST(a.`Name_lang` AS BINARY) <=> CAST(e.`Name_lang` AS BINARY) AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: spell_name_locale'; END IF;
IF (SELECT COUNT(*) FROM `hotfix_data` WHERE (`Id` BETWEEN 1203400 AND 1203432 OR `UniqueId` IN (1470691422,1201497668,568408724,1590009510,961161737,440423623,938299241,2025875162,129347979,1997590001,17257556,952071150,1721167342,1359577385,710745934,1711064303,526124583,2047475239,1748288299,2072742767,344336641,1589121735,195338505,449437858,183503063,75542336,110631181,1787688759,894847996,769759527,1435184974,825031353,335594623) OR (`TableHash`=1344507586 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=2442913102 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=428746933 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=1073915313 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=13330255 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=3776013982 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=1187407512 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=3322146344 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=4030871717 AND `RecordId` BETWEEN 1801101 AND 1801102))) <> 33 OR EXISTS (SELECT 1 FROM `rps_consumables_hotfix_data` e WHERE NOT EXISTS (SELECT 1 FROM `hotfix_data` a WHERE a.`Id` <=> e.`Id` AND a.`UniqueId` <=> e.`UniqueId` AND a.`TableHash` <=> e.`TableHash` AND a.`RecordId` <=> e.`RecordId` AND a.`Status` <=> e.`Status` AND a.`VerifiedBuild` <=> e.`VerifiedBuild`)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable row drift: hotfix_data'; END IF;
ELSE
IF EXISTS (SELECT 1 FROM `item` WHERE `ID` BETWEEN 1801001 AND 1801007) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item'; END IF;
IF EXISTS (SELECT 1 FROM `item_sparse` WHERE `ID` BETWEEN 1801001 AND 1801007) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_sparse'; END IF;
IF EXISTS (SELECT 1 FROM `item_search_name` WHERE `ID` BETWEEN 1801001 AND 1801007) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_search_name'; END IF;
IF EXISTS (SELECT 1 FROM `item_effect` WHERE (`ID` BETWEEN 1801001 AND 1801007 OR `SpellID` BETWEEN 1801101 AND 1801102)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_effect'; END IF;
IF EXISTS (SELECT 1 FROM `item_x_item_effect` WHERE ((`ID` BETWEEN 1801001 AND 1801007 OR `ItemID` BETWEEN 1801001 AND 1801007) OR `ItemEffectID` BETWEEN 1801001 AND 1801007)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_x_item_effect'; END IF;
IF EXISTS (SELECT 1 FROM `spell` WHERE `ID` BETWEEN 1801101 AND 1801102) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: spell'; END IF;
IF EXISTS (SELECT 1 FROM `spell_name` WHERE `ID` BETWEEN 1801101 AND 1801102) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: spell_name'; END IF;
IF EXISTS (SELECT 1 FROM `spell_misc` WHERE (`ID` BETWEEN 1801101 AND 1801102 OR `SpellID` BETWEEN 1801101 AND 1801102)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: spell_misc'; END IF;
IF EXISTS (SELECT 1 FROM `spell_effect` WHERE (`ID` BETWEEN 1801101 AND 1801102 OR `SpellID` BETWEEN 1801101 AND 1801102)) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: spell_effect'; END IF;
IF EXISTS (SELECT 1 FROM `item_sparse_locale` WHERE `ID` BETWEEN 1801001 AND 1801007) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_sparse_locale'; END IF;
IF EXISTS (SELECT 1 FROM `item_search_name_locale` WHERE `ID` BETWEEN 1801001 AND 1801007) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: item_search_name_locale'; END IF;
IF EXISTS (SELECT 1 FROM `spell_locale` WHERE `ID` BETWEEN 1801101 AND 1801102) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: spell_locale'; END IF;
IF EXISTS (SELECT 1 FROM `spell_name_locale` WHERE `ID` BETWEEN 1801101 AND 1801102) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: spell_name_locale'; END IF;
IF EXISTS (SELECT 1 FROM `hotfix_data` WHERE (`Id` BETWEEN 1203400 AND 1203432 OR `UniqueId` IN (1470691422,1201497668,568408724,1590009510,961161737,440423623,938299241,2025875162,129347979,1997590001,17257556,952071150,1721167342,1359577385,710745934,1711064303,526124583,2047475239,1748288299,2072742767,344336641,1589121735,195338505,449437858,183503063,75542336,110631181,1787688759,894847996,769759527,1435184974,825031353,335594623) OR (`TableHash`=1344507586 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=2442913102 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=428746933 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=1073915313 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=13330255 AND `RecordId` BETWEEN 1801001 AND 1801007) OR (`TableHash`=3776013982 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=1187407512 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=3322146344 AND `RecordId` BETWEEN 1801101 AND 1801102) OR (`TableHash`=4030871717 AND `RecordId` BETWEEN 1801101 AND 1801102))) THEN
SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Consumable collision: hotfix_data'; END IF;
INSERT INTO `item` SELECT * FROM `rps_consumables_item`;
INSERT INTO `item_sparse` SELECT * FROM `rps_consumables_item_sparse`;
INSERT INTO `item_search_name` SELECT * FROM `rps_consumables_item_search_name`;
INSERT INTO `item_effect` SELECT * FROM `rps_consumables_item_effect`;
INSERT INTO `item_x_item_effect` SELECT * FROM `rps_consumables_item_x_item_effect`;
INSERT INTO `spell` SELECT * FROM `rps_consumables_spell`;
INSERT INTO `spell_name` SELECT * FROM `rps_consumables_spell_name`;
INSERT INTO `spell_misc` SELECT * FROM `rps_consumables_spell_misc`;
INSERT INTO `spell_effect` SELECT * FROM `rps_consumables_spell_effect`;
INSERT INTO `item_sparse_locale` SELECT * FROM `rps_consumables_item_sparse_locale`;
INSERT INTO `item_search_name_locale` SELECT * FROM `rps_consumables_item_search_name_locale`;
INSERT INTO `spell_locale` SELECT * FROM `rps_consumables_spell_locale`;
INSERT INTO `spell_name_locale` SELECT * FROM `rps_consumables_spell_name_locale`;
INSERT INTO `hotfix_data` SELECT * FROM `rps_consumables_hotfix_data`;
INSERT INTO rps_custom_data_packages (package_key,payload_sha256) VALUES ('rps-consumables-v1-hotfixes','8f73d4bb173a818a32f9a681824623175e07744b6cc725fa755a97f58d66eed6');
END IF;
COMMIT;
DROP TEMPORARY TABLE `rps_consumables_item`;
DROP TEMPORARY TABLE `rps_consumables_item_sparse`;
DROP TEMPORARY TABLE `rps_consumables_item_search_name`;
DROP TEMPORARY TABLE `rps_consumables_item_effect`;
DROP TEMPORARY TABLE `rps_consumables_item_x_item_effect`;
DROP TEMPORARY TABLE `rps_consumables_spell`;
DROP TEMPORARY TABLE `rps_consumables_spell_name`;
DROP TEMPORARY TABLE `rps_consumables_spell_misc`;
DROP TEMPORARY TABLE `rps_consumables_spell_effect`;
DROP TEMPORARY TABLE `rps_consumables_item_sparse_locale`;
DROP TEMPORARY TABLE `rps_consumables_item_search_name_locale`;
DROP TEMPORARY TABLE `rps_consumables_spell_locale`;
DROP TEMPORARY TABLE `rps_consumables_spell_name_locale`;
DROP TEMPORARY TABLE `rps_consumables_hotfix_data`;
END$$
DELIMITER ;
CALL rps_consumables_20261028_hotfixes();
DROP PROCEDURE rps_consumables_20261028_hotfixes;
-- END RPS SECTION consumables

-- BEGIN RPS SECTION dice_icons
-- Refresh eight existing dice spell icons; FileDataIDs resolve in community-listfile.csv.
-- Keep other SpellMisc fields and the two weapon/shield dice unchanged.
-- New notices invalidate client caches without replacing baseline notices.
SET NAMES utf8mb4;
DROP PROCEDURE IF EXISTS rps_dice_icons_20261030;
DELIMITER $$
CREATE PROCEDURE rps_dice_icons_20261030()
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; RESIGNAL; END;
    START TRANSACTION;
    IF (SELECT COUNT(*) FROM spell_misc WHERE ID BETWEEN 1500001 AND 1500008) <> 8
       OR (SELECT COUNT(*) FROM spell_misc WHERE ID BETWEEN 1500001 AND 1500008
           AND SpellID = ID AND VerifiedBuild = 0) <> 8 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Expected eight installed RPS dice SpellMisc records';
    END IF;
    IF EXISTS (SELECT 1 FROM hotfix_blob WHERE TableHash = 0xC603EE28
               AND RecordId BETWEEN 1500001 AND 1500008) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Foreign blob overrides RPS dice icons';
    END IF;
    IF EXISTS (
        SELECT 1 FROM hotfix_data
        WHERE (Id BETWEEN 1203433 AND 1203440 OR UniqueId IN (833760798, 1589346815, 1965394681, 1747856099, 1467819184, 851579588, 1096048190, 336940239))
          AND NOT (Id = 1203433 + RecordId - 1500001
              AND TableHash = 0xC603EE28 AND RecordId BETWEEN 1500001 AND 1500008
              AND Status = 1 AND VerifiedBuild = 0
              AND UniqueId = CASE RecordId
                    WHEN 1500001 THEN 833760798
                    WHEN 1500002 THEN 1589346815
                    WHEN 1500003 THEN 1965394681
                    WHEN 1500004 THEN 1747856099
                    WHEN 1500005 THEN 1467819184
                    WHEN 1500006 THEN 851579588
                    WHEN 1500007 THEN 1096048190
                    WHEN 1500008 THEN 336940239
              END)
    ) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'RPS dice icon hotfix notice range is occupied';
    END IF;
    UPDATE spell_misc SET SpellIconFileDataID = CASE ID
        WHEN 1500001 THEN 132217 -- ability_hunter_zenarchery
        WHEN 1500002 THEN 237573 -- spell_shaman_blessingofeternals
        WHEN 1500003 THEN 132171 -- ability_hunter_displacement
        WHEN 1500004 THEN 132357 -- ability_warrior_shieldbash
        WHEN 1500005 THEN 132301 -- ability_rogue_quickrecovery
        WHEN 1500006 THEN 135977 -- spell_holy_spiritualguidence
        WHEN 1500007 THEN 135906 -- spell_holy_fistofjustice
        WHEN 1500008 THEN 643416 -- achievement_faction_craftsman
        END
    WHERE ID BETWEEN 1500001 AND 1500008 AND VerifiedBuild = 0;
    INSERT INTO hotfix_data (Id, UniqueId, TableHash, RecordId, Status, VerifiedBuild) VALUES
        (1203433, 833760798, 0xC603EE28, 1500001, 1, 0),
        (1203434, 1589346815, 0xC603EE28, 1500002, 1, 0),
        (1203435, 1965394681, 0xC603EE28, 1500003, 1, 0),
        (1203436, 1747856099, 0xC603EE28, 1500004, 1, 0),
        (1203437, 1467819184, 0xC603EE28, 1500005, 1, 0),
        (1203438, 851579588, 0xC603EE28, 1500006, 1, 0),
        (1203439, 1096048190, 0xC603EE28, 1500007, 1, 0),
        (1203440, 336940239, 0xC603EE28, 1500008, 1, 0)
    ON DUPLICATE KEY UPDATE UniqueId = VALUES(UniqueId);
    COMMIT;
END$$
DELIMITER ;
CALL rps_dice_icons_20261030();
DROP PROCEDURE rps_dice_icons_20261030;
-- END RPS SECTION dice_icons
