-- Warband group ids are account-wide, so the groups primary key must be (accountId, id).
-- The old composite key (accountId, realmId, id) allowed the same group id to exist once
-- per realm, which duplicates ids in the merged account-wide group list.
--
-- realmId stays in both tables as provenance (which realm created the row) but leaves the keys.
-- Existing rows with a colliding (accountId, id) pair are merged, the lowest realmId wins.
--
-- The procedure drops foreign keys by looking their names up in information_schema, so the
-- script does not depend on provisioning-specific constraint names, and every step is safe
-- to re-run on a partially migrated database. Run it once on the auth database (mysql CLI
-- or any client that supports DELIMITER).

DELIMITER $$

DROP PROCEDURE IF EXISTS warband_group_pk_migration $$
CREATE PROCEDURE warband_group_pk_migration()
BEGIN
    DECLARE fkName VARCHAR(64);
    DECLARE fkDone INT DEFAULT 0;
    DECLARE fkCursor CURSOR FOR
        SELECT k.CONSTRAINT_NAME
        FROM information_schema.TABLE_CONSTRAINTS k
        WHERE k.TABLE_SCHEMA = DATABASE() AND k.TABLE_NAME = 'account_warband_group_members'
          AND k.CONSTRAINT_TYPE = 'FOREIGN KEY';
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET fkDone = 1;

    -- merge colliding group rows, lowest realmId wins
    DELETE g1 FROM account_warband_groups g1
    JOIN account_warband_groups g2
      ON g1.accountId = g2.accountId AND g1.id = g2.id AND g1.realmId > g2.realmId;

    -- drop every foreign key on the members table, names vary between installs
    OPEN fkCursor;
    fkLoop: LOOP
        FETCH fkCursor INTO fkName;
        IF fkDone = 1 THEN
            LEAVE fkLoop;
        END IF;
        SET @fkSql = CONCAT('ALTER TABLE account_warband_group_members DROP FOREIGN KEY ', fkName);
        PREPARE fkStmt FROM @fkSql;
        EXECUTE fkStmt;
        DEALLOCATE PREPARE fkStmt;
    END LOOP;
    CLOSE fkCursor;

    -- remove members of merged-away groups and member rows colliding in the new key
    DELETE m FROM account_warband_group_members m
    LEFT JOIN account_warband_groups g
      ON g.accountId = m.accountId AND g.id = m.groupId
    WHERE g.accountId IS NULL;

    DELETE m1 FROM account_warband_group_members m1
    JOIN account_warband_group_members m2
      ON m1.accountId = m2.accountId AND m1.groupId = m2.groupId AND m1.characterGuid = m2.characterGuid
     AND m1.realmId > m2.realmId;

    ALTER TABLE account_warband_groups DROP PRIMARY KEY, ADD PRIMARY KEY (accountId, id);
    ALTER TABLE account_warband_group_members DROP PRIMARY KEY, ADD PRIMARY KEY (accountId, groupId, characterGuid);

    IF NOT EXISTS (SELECT 1 FROM information_schema.TABLE_CONSTRAINTS
                   WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'account_warband_group_members'
                     AND CONSTRAINT_NAME = 'fk_warband_group' AND CONSTRAINT_TYPE = 'FOREIGN KEY') THEN
        ALTER TABLE account_warband_group_members
            ADD CONSTRAINT fk_warband_group FOREIGN KEY (accountId, groupId)
            REFERENCES account_warband_groups (accountId, id) ON DELETE CASCADE;
    END IF;
END $$

DELIMITER ;

CALL warband_group_pk_migration();
DROP PROCEDURE IF EXISTS warband_group_pk_migration;
