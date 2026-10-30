-- RPS dice spells. Select the HOTFIXES database explicitly; run only with writers stopped.
-- Custom records use VerifiedBuild=0 (DB2DatabaseLoader custom pass).
-- First verify the deployed client/server DB2 IDs; SQL cannot inspect those files.
-- Set @rps_dice_push_id to a reviewed unused positive hotfix push range start (30 IDs) before SOURCE.
-- No REPLACE/IGNORE/upsert: any occupied range aborts; reruns intentionally fail.
-- Use a client that stops on SQL errors, never --force. Requires routine privileges.

SET @rps_dice_push_id = 1203075;

DELIMITER $$
CREATE PROCEDURE rps_install_dice_20261026(IN push_id BIGINT)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF push_id IS NULL OR push_id < 1 OR push_id > 2147483618 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Set an unused positive @rps_dice_push_id first';
    END IF;
    START TRANSACTION;
    IF EXISTS (SELECT 1 FROM `artifact_power_rank` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `azerite_power` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `flight_capability` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `glyph_bindable_spell` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `glyph_properties` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `guild_perk_spells` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `item_effect` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `item_set_spell` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `journal_encounter_section` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `liquid_type` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `maw_power` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `pvp_talent` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `soulbind_conduit_rank` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `specialization_spells` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_aura_options` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_aura_restrictions` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_casting_requirements` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_categories` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_class_options` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_cooldowns` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_effect` WHERE `ID` BETWEEN 1500001 AND 1500010 OR `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_empower` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_equipped_items` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_interrupts` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_label` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_learn_spell` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_levels` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_misc` WHERE `ID` BETWEEN 1500001 AND 1500010 OR `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_name` WHERE `ID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_name_locale` WHERE `ID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_power` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_reagents` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_reagents_currency` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_scaling` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_shapeshift` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_target_restrictions` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_totems` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `spell_x_spell_visual` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `talent` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `trait_definition` WHERE `SpellID` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `hotfix_blob` WHERE `TableHash` IN (0x46C66698, 0xC603EE28, 0xF04238A5) AND `RecordId` BETWEEN 1500001 AND 1500010)
       OR EXISTS (SELECT 1 FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 29 OR `UniqueId` BETWEEN push_id AND push_id + 29 OR (`TableHash` IN (0x46C66698, 0xC603EE28, 0xF04238A5) AND `RecordId` BETWEEN 1500001 AND 1500010)) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'RPS dice IDs or hotfix push ID already occupied; nothing inserted';
    END IF;
    INSERT INTO `spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES
        (1500001, 'Бросок: Меткость', 0),
        (1500002, 'Бросок: Исцеление', 0),
        (1500003, 'Бросок: Уклонение', 0),
        (1500004, 'Бросок: Сопротивление', 0),
        (1500005, 'Бросок: Навык', 0),
        (1500006, 'Бросок: Знание', 0),
        (1500007, 'Бросок: Упорство', 0),
        (1500008, 'Бросок: Ремесло', 0),
        (1500009, 'Бросок: Урон оружия', 0),
        (1500010, 'Бросок: Блокирование', 0);
    INSERT INTO `spell_name_locale` (`ID`, `locale`, `Name_lang`, `VerifiedBuild`) VALUES
        (1500001, 'ruRU', 'Бросок: Меткость', 0),
        (1500002, 'ruRU', 'Бросок: Исцеление', 0),
        (1500003, 'ruRU', 'Бросок: Уклонение', 0),
        (1500004, 'ruRU', 'Бросок: Сопротивление', 0),
        (1500005, 'ruRU', 'Бросок: Навык', 0),
        (1500006, 'ruRU', 'Бросок: Знание', 0),
        (1500007, 'ruRU', 'Бросок: Упорство', 0),
        (1500008, 'ruRU', 'Бросок: Ремесло', 0),
        (1500009, 'ruRU', 'Бросок: Урон оружия', 0),
        (1500010, 'ruRU', 'Бросок: Блокирование', 0);
    -- Instant self-targeted dummy: no native damage, power cost, cooldown or GCD records.
    INSERT INTO `spell_misc` (`ID`, `CastingTimeIndex`, `RangeIndex`, `SchoolMask`, `SpellIconFileDataID`, `SpellID`, `VerifiedBuild`) VALUES
        (1500001, 1, 1, 1, 132217, 1500001, 0),
        (1500002, 1, 1, 1, 237573, 1500002, 0),
        (1500003, 1, 1, 1, 132171, 1500003, 0),
        (1500004, 1, 1, 1, 132357, 1500004, 0),
        (1500005, 1, 1, 1, 132301, 1500005, 0),
        (1500006, 1, 1, 1, 135977, 1500006, 0),
        (1500007, 1, 1, 1, 135906, 1500007, 0),
        (1500008, 1, 1, 1, 643416, 1500008, 0),
        (1500009, 1, 1, 1, 237284, 1500009, 0),
        (1500010, 1, 1, 1, 237284, 1500010, 0);
    INSERT INTO `spell_effect` (`ID`, `Effect`, `EffectIndex`, `ImplicitTarget1`, `SpellID`, `VerifiedBuild`) VALUES
        (1500001, 3, 0, 1, 1500001, 0),
        (1500002, 3, 0, 1, 1500002, 0),
        (1500003, 3, 0, 1, 1500003, 0),
        (1500004, 3, 0, 1, 1500004, 0),
        (1500005, 3, 0, 1, 1500005, 0),
        (1500006, 3, 0, 1, 1500006, 0),
        (1500007, 3, 0, 1, 1500007, 0),
        (1500008, 3, 0, 1, 1500008, 0),
        (1500009, 3, 0, 1, 1500009, 0),
        (1500010, 3, 0, 1, 1500010, 0);
    INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
        (push_id + 0, push_id + 0, 0x46C66698, 1500001, 1, 0),
        (push_id + 1, push_id + 1, 0x46C66698, 1500002, 1, 0),
        (push_id + 2, push_id + 2, 0x46C66698, 1500003, 1, 0),
        (push_id + 3, push_id + 3, 0x46C66698, 1500004, 1, 0),
        (push_id + 4, push_id + 4, 0x46C66698, 1500005, 1, 0),
        (push_id + 5, push_id + 5, 0x46C66698, 1500006, 1, 0),
        (push_id + 6, push_id + 6, 0x46C66698, 1500007, 1, 0),
        (push_id + 7, push_id + 7, 0x46C66698, 1500008, 1, 0),
        (push_id + 8, push_id + 8, 0x46C66698, 1500009, 1, 0),
        (push_id + 9, push_id + 9, 0x46C66698, 1500010, 1, 0),
        (push_id + 10, push_id + 10, 0xC603EE28, 1500001, 1, 0),
        (push_id + 11, push_id + 11, 0xC603EE28, 1500002, 1, 0),
        (push_id + 12, push_id + 12, 0xC603EE28, 1500003, 1, 0),
        (push_id + 13, push_id + 13, 0xC603EE28, 1500004, 1, 0),
        (push_id + 14, push_id + 14, 0xC603EE28, 1500005, 1, 0),
        (push_id + 15, push_id + 15, 0xC603EE28, 1500006, 1, 0),
        (push_id + 16, push_id + 16, 0xC603EE28, 1500007, 1, 0),
        (push_id + 17, push_id + 17, 0xC603EE28, 1500008, 1, 0),
        (push_id + 18, push_id + 18, 0xC603EE28, 1500009, 1, 0),
        (push_id + 19, push_id + 19, 0xC603EE28, 1500010, 1, 0),
        (push_id + 20, push_id + 20, 0xF04238A5, 1500001, 1, 0),
        (push_id + 21, push_id + 21, 0xF04238A5, 1500002, 1, 0),
        (push_id + 22, push_id + 22, 0xF04238A5, 1500003, 1, 0),
        (push_id + 23, push_id + 23, 0xF04238A5, 1500004, 1, 0),
        (push_id + 24, push_id + 24, 0xF04238A5, 1500005, 1, 0),
        (push_id + 25, push_id + 25, 0xF04238A5, 1500006, 1, 0),
        (push_id + 26, push_id + 26, 0xF04238A5, 1500007, 1, 0),
        (push_id + 27, push_id + 27, 0xF04238A5, 1500008, 1, 0),
        (push_id + 28, push_id + 28, 0xF04238A5, 1500009, 1, 0),
        (push_id + 29, push_id + 29, 0xF04238A5, 1500010, 1, 0);
    COMMIT;
END$$
DELIMITER ;
CALL rps_install_dice_20261026(@rps_dice_push_id);
DROP PROCEDURE rps_install_dice_20261026;
