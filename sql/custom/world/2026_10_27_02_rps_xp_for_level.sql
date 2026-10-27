-- Native levels: RP level = server level - 9; server levels 10..19, MaxPlayerLevel = 19.
-- Select the WORLD database explicitly. Idempotent; takes effect on the next worldserver start.
-- Level = current server level, Experience = XP needed at that level to reach Level + 1
-- (ObjectMgr::LoadPlayerInfo; rows with Level >= MaxPlayerLevel are ignored, so 19 has no row).
-- Levels 1..9 keep the client GameTable values; nobody stays below 10 after the characters migration.

REPLACE INTO `player_xp_for_level` (`Level`, `Experience`) VALUES
(10, 20),
(11, 25),
(12, 30),
(13, 40),
(14, 50),
(15, 60),
(16, 75),
(17, 90),
(18, 110);
