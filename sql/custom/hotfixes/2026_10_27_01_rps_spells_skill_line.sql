-- ЭКСПЕРИМЕНТАЛЬНО: SkillLineAbility rows so the 13 RPS spells (1500001..1500013) are listed in the client spellbook.
-- Optional; apply only after 2026_10_26_00_rps_dice_spells.sql and 2026_10_27_00_rps_action_spells.sql.
-- SkillLine 183 = GENERIC (DND) (CSVData/skillline.csv; Auto Attack 6603 lives there, SkillRaceClassInfo 5 = all races/classes).
-- AcquireMethod 0 (Learned): the core never auto-learns or removes these spells; learning stays with the RPS bridge.
-- ClassMask 16383 (all classes, like Auto Attack 6603): the client spellbook hides SkillLine 183 rows with ClassMask 0. RaceMask 0 = no race restriction.
-- Select the HOTFIXES database explicitly; run only with writers stopped.
-- Set @rps_skill_line_push_id to a reviewed unused positive hotfix push range start (13 IDs) before SOURCE.
-- Default 1203114..1203126 follows the action range 1203105..1203113.
-- No REPLACE/IGNORE/upsert: any occupied range aborts; reruns intentionally fail.
-- Use a client that stops on SQL errors, never --force. Requires routine privileges.

SET @rps_skill_line_push_id = 1203114;

DELIMITER $$
CREATE PROCEDURE rps_install_skill_line_20261027(IN push_id BIGINT)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF push_id IS NULL OR push_id < 1 OR push_id > 2147483635 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Set an unused positive @rps_skill_line_push_id first';
    END IF;
    START TRANSACTION;
    IF (SELECT COUNT(*) FROM `spell_name` WHERE `ID` BETWEEN 1500001 AND 1500013 AND `VerifiedBuild` = 0) <> 13 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Apply RPS dice and action spell packages first; nothing inserted';
    END IF;
    IF EXISTS (SELECT 1 FROM `skill_line_ability` WHERE `ID` BETWEEN 1500001 AND 1500013 OR `Spell` BETWEEN 1500001 AND 1500013)
       OR EXISTS (SELECT 1 FROM `skill_line_ability_locale` WHERE `ID` BETWEEN 1500001 AND 1500013)
       OR EXISTS (SELECT 1 FROM `hotfix_blob` WHERE `TableHash` = 0xFF4446F6 AND `RecordId` BETWEEN 1500001 AND 1500013)
       OR EXISTS (SELECT 1 FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 12 OR `UniqueId` BETWEEN push_id AND push_id + 12 OR (`TableHash` = 0xFF4446F6 AND `RecordId` BETWEEN 1500001 AND 1500013)) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'RPS skill line ability IDs or hotfix push ID already occupied; nothing inserted';
    END IF;
    INSERT INTO `skill_line_ability` (`AbilityVerb`, `AbilityAllVerb`, `ID`, `SkillLine`, `Spell`, `MinSkillLineRank`, `ClassMask`, `AcquireMethod`, `NumSkillUps`, `VerifiedBuild`) VALUES
        ('', '', 1500001, 183, 1500001, 1, 16383, 0, 1, 0),
        ('', '', 1500002, 183, 1500002, 1, 16383, 0, 1, 0),
        ('', '', 1500003, 183, 1500003, 1, 16383, 0, 1, 0),
        ('', '', 1500004, 183, 1500004, 1, 16383, 0, 1, 0),
        ('', '', 1500005, 183, 1500005, 1, 16383, 0, 1, 0),
        ('', '', 1500006, 183, 1500006, 1, 16383, 0, 1, 0),
        ('', '', 1500007, 183, 1500007, 1, 16383, 0, 1, 0),
        ('', '', 1500008, 183, 1500008, 1, 16383, 0, 1, 0),
        ('', '', 1500009, 183, 1500009, 1, 16383, 0, 1, 0),
        ('', '', 1500010, 183, 1500010, 1, 16383, 0, 1, 0),
        ('', '', 1500011, 183, 1500011, 1, 16383, 0, 1, 0),
        ('', '', 1500012, 183, 1500012, 1, 16383, 0, 1, 0),
        ('', '', 1500013, 183, 1500013, 1, 16383, 0, 1, 0);
    INSERT INTO `hotfix_data` (`Id`, `UniqueId`, `TableHash`, `RecordId`, `Status`, `VerifiedBuild`) VALUES
        (push_id + 0, push_id + 0, 0xFF4446F6, 1500001, 1, 0),
        (push_id + 1, push_id + 1, 0xFF4446F6, 1500002, 1, 0),
        (push_id + 2, push_id + 2, 0xFF4446F6, 1500003, 1, 0),
        (push_id + 3, push_id + 3, 0xFF4446F6, 1500004, 1, 0),
        (push_id + 4, push_id + 4, 0xFF4446F6, 1500005, 1, 0),
        (push_id + 5, push_id + 5, 0xFF4446F6, 1500006, 1, 0),
        (push_id + 6, push_id + 6, 0xFF4446F6, 1500007, 1, 0),
        (push_id + 7, push_id + 7, 0xFF4446F6, 1500008, 1, 0),
        (push_id + 8, push_id + 8, 0xFF4446F6, 1500009, 1, 0),
        (push_id + 9, push_id + 9, 0xFF4446F6, 1500010, 1, 0),
        (push_id + 10, push_id + 10, 0xFF4446F6, 1500011, 1, 0),
        (push_id + 11, push_id + 11, 0xFF4446F6, 1500012, 1, 0),
        (push_id + 12, push_id + 12, 0xFF4446F6, 1500013, 1, 0);
    COMMIT;
END$$
DELIMITER ;
CALL rps_install_skill_line_20261027(@rps_skill_line_push_id);
DROP PROCEDURE rps_install_skill_line_20261027;
