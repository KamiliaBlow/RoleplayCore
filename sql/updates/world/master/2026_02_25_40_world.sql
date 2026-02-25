-- ============================================================================
-- 5A: Quest POI cleanup
-- Removes quest_poi and quest_poi_points entries that reference quests
-- not present in quest_template (~10,350 errors) and orphaned poi points
-- that have no matching quest_poi entry (~137 errors).
-- ============================================================================

-- Delete quest_poi_points for quests that don't exist in quest_template
DELETE FROM quest_poi_points WHERE QuestID NOT IN (SELECT ID FROM quest_template);

-- Delete quest_poi entries for quests that don't exist in quest_template
DELETE FROM quest_poi WHERE QuestID NOT IN (SELECT ID FROM quest_template);

-- Delete orphaned quest_poi_points that have no matching quest_poi row
-- (handles the "references unknown quest points" errors)
DELETE qpp FROM quest_poi_points qpp
LEFT JOIN quest_poi qp ON qpp.QuestID = qp.QuestID AND qpp.Idx1 = qp.Idx1
WHERE qp.QuestID IS NULL;
