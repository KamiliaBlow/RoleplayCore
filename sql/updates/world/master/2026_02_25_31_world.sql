-- SmartAI Fix 4A: Remove broken link chain entries (multi-pass cleanup)
-- When a smart_scripts row has link != 0, it expects another row in the same
-- (entryorguid, source_type) group with id == link and event_type == 61 (SMART_EVENT_LINK).
-- If no such row exists, the link is broken and the entry is skipped at load time.
-- Source: SmartScriptMgr.cpp FindLinkedEvent() / LoadSmartAIFromDB()
--
-- Deleting broken links can cascade: removing a link target orphans rows that
-- linked to it. We repeat the cleanup multiple passes to handle chains.
-- In practice, chain depth was observed at ~8 levels, so 10 passes is sufficient.

-- ============================================================================
-- Pass 1
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

-- Clean orphaned SMART_EVENT_LINK targets
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 2
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 3
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 4
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 5
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 6
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 7
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 8
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 9
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;

-- ============================================================================
-- Pass 10
-- ============================================================================
CREATE TEMPORARY TABLE IF NOT EXISTS tmp_bl (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_bl SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.link != 0 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.id = s1.link AND s2.event_type = 61);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_bl bl ON ss.entryorguid = bl.entryorguid AND ss.source_type = bl.source_type AND ss.id = bl.id;
DROP TEMPORARY TABLE IF EXISTS tmp_bl;

CREATE TEMPORARY TABLE IF NOT EXISTS tmp_ol (entryorguid BIGINT NOT NULL, source_type TINYINT UNSIGNED NOT NULL, id SMALLINT UNSIGNED NOT NULL, PRIMARY KEY (entryorguid, source_type, id));
INSERT INTO tmp_ol SELECT s1.entryorguid, s1.source_type, s1.id FROM smart_scripts s1 WHERE s1.event_type = 61 AND NOT EXISTS (SELECT 1 FROM smart_scripts s2 WHERE s2.entryorguid = s1.entryorguid AND s2.source_type = s1.source_type AND s2.link = s1.id);
DELETE ss FROM smart_scripts ss INNER JOIN tmp_ol ol ON ss.entryorguid = ol.entryorguid AND ss.source_type = ol.source_type AND ss.id = ol.id;
DROP TEMPORARY TABLE IF EXISTS tmp_ol;
