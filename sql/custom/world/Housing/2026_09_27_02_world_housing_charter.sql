-- Housing: charter neighborhood founding support.
-- Retail wires charter founding to quest 89450 "Create a Neighborhood": the housing
-- steward provides the Neighborhood Charter (item 239098), whose use opens the charter
-- UI; gathering the signatures and turning the completed charter in to the steward
-- founds the neighborhood. The steward gossip in npc_housing_steward.cpp hands the
-- charter out directly, so the quest wiring below is optional and only applied when
-- the retail quest exists in the world database (TDB 12.x ships it; run-time guard:
-- the core only touches the quest when the player actually has it).
--
-- Apply order: after 2026_09_27_01_world_housing.sql.

-- Offer / turn in quest 89450 at the housing stewards (Alliance 233063, Horde 233708).
INSERT IGNORE INTO `creature_queststarter` (`id`, `quest`)
SELECT c.entry, 89450
FROM (SELECT 233063 AS entry UNION ALL SELECT 233708) AS c
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 89450);

INSERT IGNORE INTO `creature_questender` (`id`, `quest`)
SELECT c.entry, 89450
FROM (SELECT 233063 AS entry UNION ALL SELECT 233708) AS c
WHERE EXISTS (SELECT 1 FROM `quest_template` WHERE `ID` = 89450);
