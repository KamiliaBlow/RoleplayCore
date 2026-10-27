-- Native levels: raise every character below server level 10 (RP level 1) to 10 with zero XP.
-- Apply ONLY with the owner's permission, with worldserver STOPPED and after a characters DB backup.
-- Select the CHARACTERS database explicitly. Idempotent: a rerun changes no rows.
-- Stats, spells and skills are recalculated by the core on the next login.
-- Characters above 19 (above MaxPlayerLevel) are not touched; review them manually with the query below.

UPDATE `characters` SET `level` = 10, `xp` = 0 WHERE `level` < 10;

-- Review query (read-only): SELECT `guid`, `name`, `level`, `xp` FROM `characters` WHERE `level` > 19;
