-- Workflow 3C: Fix creatures with non-existing faction templates
-- Sets faction to 35 (Friendly to All) which is what ObjectMgr::CheckCreatureTemplate()
-- falls back to at runtime anyway (see ObjectMgr.cpp: "has non-existing faction template").
-- Persisting the fix in DB eliminates ~500 error log entries on every startup.
--
-- The `faction` column is in `creature_template`, not `creature_template_difficulty`.
-- Two invalid faction IDs found in the error log:
--   faction = 0  (497 creatures) -- no such FactionTemplate entry
--   faction = 13 (3 creatures: 4818, 4819, 4820) -- no such FactionTemplate entry

-- Bulk fix: all creatures with faction = 0 (not a valid FactionTemplate)
UPDATE `creature_template` SET `faction` = 35 WHERE `faction` = 0;

-- Individual fixes: creatures with faction = 13 (not a valid FactionTemplate)
UPDATE `creature_template` SET `faction` = 35 WHERE `entry` IN (4818, 4819, 4820) AND `faction` = 13;
