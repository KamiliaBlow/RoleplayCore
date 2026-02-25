# Workflow 4: SmartAI Fixes

**Database**: `world`
**Estimated errors**: ~173,000
**SQL output**: `sql/updates/world/master/2026_02_25_3X_world.sql`
**Priority**: Medium-High (broken AI scripts, but many are for content not actively used)

## Problem Description

SmartAI is TrinityCore's data-driven scripting system (table: `smart_scripts`). These
errors indicate broken or invalid SmartAI entries — missing links, invalid spells,
invalid creatures, deprecated events, etc.

## Error Categories

### 4A. Link Event Not Found (~162,500 errors)

```
SmartAIMgr::LoadSmartAIFromDB: Entry 185226 SourceType 0, Event 10001, Link Event 10000 not found or invalid.
```

SmartAI events can link to other events (event chains). These errors mean the linked
event doesn't exist. This is the **single largest SmartAI error** by far.

**Fix**: Either delete the broken chain entries, or fix the link references.

```sql
-- Find smart_scripts entries where link points to non-existent event
-- For bulk cleanup, delete the broken entries:
DELETE FROM smart_scripts
WHERE entryorguid = <entry> AND source_type = <srctype> AND id = <event_id>
AND link != 0
AND NOT EXISTS (
    SELECT 1 FROM smart_scripts ss2
    WHERE ss2.entryorguid = smart_scripts.entryorguid
    AND ss2.source_type = smart_scripts.source_type
    AND ss2.id = smart_scripts.link
);
```

**Note**: This is a self-referential query. You may need a temp table approach:
```sql
CREATE TEMPORARY TABLE broken_links AS
SELECT s1.entryorguid, s1.source_type, s1.id
FROM smart_scripts s1
WHERE s1.link != 0
AND NOT EXISTS (
    SELECT 1 FROM smart_scripts s2
    WHERE s2.entryorguid = s1.entryorguid
    AND s2.source_type = s1.source_type
    AND s2.id = s1.link
);

DELETE FROM smart_scripts
WHERE (entryorguid, source_type, id) IN (
    SELECT entryorguid, source_type, id FROM broken_links
);

DROP TEMPORARY TABLE broken_links;
```

### 4B. Invalid Quest Objective for SMART_EVENT_QUEST_OBJ_COMPLETION (~47,000 errors)

```
SmartAIMgr: Event SMART_EVENT_QUEST_OBJ_COMPLETION using invalid objective id N, skipped.
```

**Fix**: Delete SmartAI entries using this event type with invalid objective IDs.

```sql
-- SMART_EVENT_QUEST_OBJ_COMPLETION = event_type 81 (verify in SmartScriptMgr.h)
DELETE FROM smart_scripts
WHERE event_type = 81
AND event_param1 NOT IN (SELECT ID FROM quest_objectives);
```

### 4C. Invalid Spell References (~1,100 errors)

```
SmartAIMgr: Entry N SourceType N Event N Action N uses non-existent Spell entry N, skipped.
```

**Fix**: Delete SmartAI entries that cast non-existent spells.

```sql
-- Actions that reference spells: SMART_ACTION_CAST (11), SMART_ACTION_INVOKER_CAST (85), etc.
DELETE FROM smart_scripts
WHERE action_type IN (11, 85, 86)
AND action_param1 NOT IN (SELECT ID FROM spell_dbc)
AND action_param1 != 0;
```

### 4D. Kill Credit / Spell Effect Issues (~8,700 errors)

```
SmartAIMgr: Entry N ... Effect: SPELL_EFFECT_KILL_CREDIT ... has invalid target      (~6,370)
SmartAIMgr: Entry N ... Kill Credit: There is a killcredit spell for creatureEntry N  (~2,350)
```

These are warnings about suboptimal SmartAI usage. The kill credit errors mean there's
already a spell that handles kill credit, so the SmartAI action is redundant.

**Fix**: Low priority. Either delete the redundant SmartAI kill credit actions, or leave
as warnings. For the invalid target ones, fix the target_type field.

### 4E. Invalid WaypointPath (~740 errors)

```
SmartAIMgr: Creature N Event N Action N uses non-existent WaypointPath id N, skipped.
SmartAIMgr: Creature -N Event N Action N uses non-existent WaypointPath id N, skipped.
```

**Fix**: Delete SmartAI entries that reference non-existent waypoint paths.

### 4F. Unused/Deprecated Events and Actions (~800 errors)

```
SmartAIMgr: Unused action_type(N), event_type(N), Entry N SourceType N, skipped.      (~745)
SmartAIMgr: Unused event_type(N), Entry N SourceType N Event N, skipped.              (~48)
SmartAIMgr: EntryOrGuid N using event (N) has deprecated event flags (N), skipped.    (~112)
SmartAIMgr: EntryOrGuid N using event (N) has invalid action type (N), skipped.       (~30)
```

**Fix**: Delete SmartAI entries with deprecated/invalid event types and action types.

### 4G. Missing Repeat Flag (~747 errors)

```
SmartAIMgr::LoadSmartAIFromDB: Entry N SourceType N, Event N, Missing Repeat flag.
```

**Fix**: Add the repeat flag to events that need it, or set event_flags |= 1 (SMART_EVENT_FLAG_NOT_REPEATABLE inverted — need to check source).

### 4H. Non-SmartAI Creatures with SmartAI Data (~710 errors)

```
SmartAIMgr::LoadSmartAIFromDB: Creature entry (N) is not using SmartAI, skipped loading.  (~670)
SmartAIMgr::LoadSmartAIFromDB: Creature guid (N) does not exist, skipped loading.         (~91)
```

**Fix**: Either:
- Delete the smart_scripts data for these creatures
- Or set their AIName to 'SmartAI' if they should be using it

For non-existent GUIDs, just delete the smart_scripts entries.

### 4I. Invalid Target Parameters (~130 errors)

```
SmartAIMgr: Entry N ... using invalid creature entry N ... for guid N as target_paramN    (~79)
SmartAIMgr: Entry N ... using invalid creature guid N as target_paramN                    (~48)
SmartAIMgr: Entry N ... has invoker target, but event does not provide any invoker!       (~29)
```

**Fix**: Delete or fix the target parameters in the affected smart_scripts rows.

### 4J. Invalid Text References (~74 errors)

```
SmartAIMgr: Entry NSourceType N Event N Action N using non-existent Text id N, skipped.
```

**Fix**: Delete SmartAI entries referencing non-existent creature_text groups.

### 4K. Creature Summon Warnings (~40 errors)

```
SmartAIMgr: Entry N ... creature summon: There is a summon spell for creature entry N
```

**Fix**: Low priority informational warning. Can be ignored or the redundant SmartAI
summon action can be removed.

## Recommended Execution Order

1. **4H** — Clean up SmartAI data for non-SmartAI creatures and non-existent GUIDs
2. **4A** — Fix broken link chains (biggest category)
3. **4B** — Fix invalid quest objective events
4. **4C** — Fix invalid spell references
5. **4E** — Fix invalid waypoint references
6. **4F** — Remove deprecated/invalid events and actions
7. **4G** — Fix missing repeat flags
8. **4I/4J** — Fix invalid targets and text refs
9. **4D/4K** — Address warnings (lowest priority)

## SQL File Naming

- `2026_02_25_30_world.sql` — Non-SmartAI creature cleanup (4H)
- `2026_02_25_31_world.sql` — Broken link chain fixes (4A)
- `2026_02_25_32_world.sql` — Invalid quest objective events (4B)
- `2026_02_25_33_world.sql` — Invalid spell/waypoint/target/text refs (4C, 4E, 4I, 4J)
- `2026_02_25_34_world.sql` — Deprecated events and actions (4F)
- `2026_02_25_35_world.sql` — Missing repeat flags (4G)
- `2026_02_25_36_world.sql` — Kill credit and summon warnings (4D, 4K)

## Key Tables
- `smart_scripts` — all SmartAI data
- `creature_template` — check AIName field
- `quest_objectives` — for quest objective validation
- `waypoint_path` / `waypoint_path_node` — for waypoint validation
- `creature_text` — for text group validation

## Key Source Files
- `src/server/game/AI/SmartScripts/SmartScriptMgr.cpp` — SmartAI loading and validation
- `src/server/game/AI/SmartScripts/SmartScriptMgr.h` — event/action type enums
- `src/server/game/AI/SmartScripts/SmartScript.cpp` — runtime execution
