-- Action spells: Атака/Избегание/Блок, separate from dice checks 1500001..1500010.
-- Select the HOTFIXES database explicitly; run only with writers stopped.
-- Custom records use VerifiedBuild=0 (DB2DatabaseLoader custom pass).
-- First verify the deployed client/server DB2 IDs; SQL cannot inspect those files.
-- Set @rps_action_push_id to a reviewed unused positive hotfix push range start (9 IDs) before SOURCE.
-- Default 1203105..1203113 follows the dice range 1203075..1203104.
-- No REPLACE/IGNORE/upsert: any occupied range aborts; reruns intentionally fail.
-- Use a client that stops on SQL errors, never --force. Requires routine privileges.

SET @rps_action_push_id = 1203105;

DELIMITER $$
CREATE PROCEDURE rps_install_action_20261027(IN push_id BIGINT)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF push_id IS NULL OR push_id < 1 OR push_id > 2147483639 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Set an unused positive @rps_action_push_id first';
    END IF;
    START TRANSACTION;
    IF EXISTS (SELECT 1 FROM `artifact_power_rank` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `azerite_power` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `flight_capability` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `glyph_bindable_spell` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `glyph_properties` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `guild_perk_spells` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `item_effect` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `item_set_spell` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `journal_encounter_section` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `liquid_type` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `maw_power` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `pvp_talent` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `soulbind_conduit_rank` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `specialization_spells` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_aura_options` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_aura_restrictions` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_casting_requirements` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_categories` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_class_options` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_cooldowns` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_effect` WHERE `ID` BETWEEN 1500011 AND 1500013 OR `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_empower` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_equipped_items` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_interrupts` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_label` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_learn_spell` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_levels` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_misc` WHERE `ID` BETWEEN 1500011 AND 1500013 OR `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_name` WHERE `ID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_name_locale` WHERE `ID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_power` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_reagents` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_reagents_currency` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_scaling` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_shapeshift` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_target_restrictions` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_totems` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `spell_x_spell_visual` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `talent` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `trait_definition` WHERE `SpellID` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `hotfix_blob` WHERE `TableHash` IN (0x46C66698, 0xC603EE28, 0xF04238A5) AND `RecordId` BETWEEN 1500011 AND 1500013)
       OR EXISTS (SELECT 1 FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 8 OR `UniqueId` BETWEEN push_id AND push_id + 8 OR (`TableHash` IN (0x46C66698, 0xC603EE28, 0xF04238A5) AND `RecordId` BETWEEN 1500011 AND 1500013)) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'RPS action IDs or hotfix push ID already occupied; nothing inserted';
    END IF;
    INSERT INTO `spell_name` (`ID`, `Name`, `VerifiedBuild`) VALUES
        (1500011, 'Атака', 0),
        (1500012, 'Избегание', 0),
        (1500013, 'Блок', 0);
    INSERT INTO `spell_name_locale` (`ID`, `locale`, `Name_lang`, `VerifiedBuild`) VALUES
        (1500011, 'ruRU', 'Атака', 0),
        (1500012, 'ruRU', 'Избегание', 0),
        (1500013, 'ruRU', 'Блок', 0);
    -- Instant self-targeted dummy: no native damage, power cost, cooldown or GCD records.
    -- Icons: 132223 Ability_MeleeDamage, 132294 Ability_Rogue_Feint, 132110 Ability_Defend.
    INSERT INTO `spell_misc` (`ID`, `CastingTimeIndex`, `RangeIndex`, `SchoolMask`, `SpellIconFileDataID`, `SpellID`, `VerifiedBuild`) VALUES
        (1500011, 1, 1, 1, 132223, 1500011, 0),
        (1500012, 1, 1, 1, 132294, 1500012, 0),
        (1500013, 1, 1, 1, 132110, 1500013, 0);
    INSERT INTO `spell_effect` (`ID`, `Effect`, `EffectIndex`, `ImplicitTarget1`, `SpellID`, `VerifiedBuild`) VALUES
        (1500011, 3, 0, 1, 1500011, 0),
        (1500012, 3, 0, 1, 1500012, 0),
        (1500013, 3, 0, 1, 1500013, 0);
    INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
        (push_id + 0, push_id + 0, 0x46C66698, 1500011, 1, 0),
        (push_id + 1, push_id + 1, 0x46C66698, 1500012, 1, 0),
        (push_id + 2, push_id + 2, 0x46C66698, 1500013, 1, 0),
        (push_id + 3, push_id + 3, 0xC603EE28, 1500011, 1, 0),
        (push_id + 4, push_id + 4, 0xC603EE28, 1500012, 1, 0),
        (push_id + 5, push_id + 5, 0xC603EE28, 1500013, 1, 0),
        (push_id + 6, push_id + 6, 0xF04238A5, 1500011, 1, 0),
        (push_id + 7, push_id + 7, 0xF04238A5, 1500012, 1, 0),
        (push_id + 8, push_id + 8, 0xF04238A5, 1500013, 1, 0);
    COMMIT;
END$$
DELIMITER ;
CALL rps_install_action_20261027(@rps_action_push_id);
DROP PROCEDURE rps_install_action_20261027;
