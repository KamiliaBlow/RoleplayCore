-- Update for stands where 2026_10_27_01_rps_spells_skill_line.sql was applied with ClassMask 0.
-- The client spellbook hides SkillLine 183 rows with ClassMask 0; Auto Attack 6603 uses 16383 (all classes).
-- Fresh installs already get ClassMask 16383 from 2026_10_27_01 and must skip this file (the guard aborts).
-- New hotfix push IDs make clients re-request the changed SkillLineAbility records instead of using their cache.
-- Select the HOTFIXES database explicitly; restart worldserver afterwards (DB2 stores load at startup).
-- Set @rps_skill_line_fix_push_id to a reviewed unused positive hotfix push range start (13 IDs) before SOURCE.
-- Default 1203127..1203139 follows the skill line range 1203114..1203126.
-- No REPLACE/IGNORE/upsert: any occupied range aborts; reruns intentionally fail.
-- Use a client that stops on SQL errors, never --force. Requires routine privileges.

SET @rps_skill_line_fix_push_id = 1203127;

DELIMITER $$
CREATE PROCEDURE rps_fix_skill_line_classmask_20261027(IN push_id BIGINT)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    IF push_id IS NULL OR push_id < 1 OR push_id > 2147483635 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Set an unused positive @rps_skill_line_fix_push_id first';
    END IF;
    START TRANSACTION;
    IF (SELECT COUNT(*) FROM `skill_line_ability` WHERE `ID` BETWEEN 1500001 AND 1500013 AND `Spell` = `ID` AND `SkillLine` = 183 AND `ClassMask` = 0) <> 13 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Expected 13 RPS skill line rows with ClassMask 0; nothing changed';
    END IF;
    IF EXISTS (SELECT 1 FROM `hotfix_data` WHERE `Id` BETWEEN push_id AND push_id + 12 OR `UniqueId` BETWEEN push_id AND push_id + 12) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Hotfix push ID range already occupied; nothing changed';
    END IF;
    UPDATE `skill_line_ability` SET `ClassMask` = 16383 WHERE `ID` BETWEEN 1500001 AND 1500013 AND `SkillLine` = 183;
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
CALL rps_fix_skill_line_classmask_20261027(@rps_skill_line_fix_push_id);
DROP PROCEDURE rps_fix_skill_line_classmask_20261027;
