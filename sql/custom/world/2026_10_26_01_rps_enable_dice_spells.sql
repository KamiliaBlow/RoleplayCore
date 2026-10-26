-- WORLD database. Run only after the hotfix spells load on the client and the server.
-- Run without --force, with writers stopped. Existing operator mappings abort the batch.
DELIMITER $$
CREATE PROCEDURE rps_enable_dice_20261026()
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;
    START TRANSACTION;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'accuracy' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: accuracy';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'healing' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: healing';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'evasion' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: evasion';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'resistance' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: resistance';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'skill' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: skill';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'knowledge' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: knowledge';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'endurance' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: endurance';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'craft' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: craft';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'weapon_damage' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: weapon_damage';
    END IF;
    IF (SELECT COUNT(*) FROM rps_stat_definitions WHERE stat_key = 'shield_block' AND spell_id = 0) <> 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Missing or already mapped stat: shield_block';
    END IF;
    IF EXISTS (SELECT 1 FROM rps_stat_definitions WHERE spell_id BETWEEN 1500001 AND 1500010) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Dice spell range is already mapped';
    END IF;
    UPDATE rps_stat_definitions SET spell_id = 1500001 WHERE stat_key = 'accuracy' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500002 WHERE stat_key = 'healing' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500003 WHERE stat_key = 'evasion' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500004 WHERE stat_key = 'resistance' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500005 WHERE stat_key = 'skill' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500006 WHERE stat_key = 'knowledge' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500007 WHERE stat_key = 'endurance' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500008 WHERE stat_key = 'craft' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500009 WHERE stat_key = 'weapon_damage' AND spell_id = 0;
    UPDATE rps_stat_definitions SET spell_id = 1500010 WHERE stat_key = 'shield_block' AND spell_id = 0;
    COMMIT;
END$$
DELIMITER ;
CALL rps_enable_dice_20261026();
DROP PROCEDURE rps_enable_dice_20261026;
