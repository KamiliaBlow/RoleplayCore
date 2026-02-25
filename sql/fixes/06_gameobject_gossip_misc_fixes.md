# Workflow 6: Gameobject, Gossip & Miscellaneous Fixes

**Database**: `world`
**Estimated errors**: ~25,000+
**SQL output**: `sql/updates/world/master/2026_02_25_5X_world.sql`
**Priority**: Medium (mixed severity)

## Error Categories

### 6A. Gameobject Spawn Issues (~20,000 errors)

#### Unsupported Difficulty (~19,200)
```
Table `gameobject` has gameobject (GUID: N) has unsupported difficulty N for map (Id: N).
```

Same pattern as creature difficulty errors. The gameobject is assigned a difficulty
that doesn't exist for its map.

**Fix**: Delete spawns with invalid difficulties (or fix the difficulty assignment).

#### Invalid State (~789)
```
Table `gameobject` has gameobject (GUID: N Entry: N) with invalid `state` (N) value, skip
```

**Fix**: Set state to 1 (ready/closed) for invalid states.

```sql
UPDATE gameobject SET state = 1 WHERE state NOT IN (0, 1, 2);
```

#### Invalid Rotation (~430)
```
Table `gameobject` has gameobject (GUID: N Entry: N) with invalid rotationZ value, skip
Table `gameobject` has gameobject (GUID: N Entry: N) with invalid rotation quaternion
```

**Fix**: Reset rotation to default (0,0,0,1 quaternion) or recalculate from orientation.

#### terrainSwapMap Issues (~510)
```
Table `gameobject` have gameobject (GUID: N Entry: N) with `terrainSwapMap` N ...
```

**Fix**: Set `terrainSwapMap` to -1 for invalid entries.

#### phaseGroup Issues (~247)
```
Table `gameobject` have gameobject (GUID: N Entry: N) with `phaseGroup` N does not exist
```

**Fix**: Set `phaseGroup` to 0.

#### Spawntimesecs Warning (~1,778)
```
Table `gameobject` has gameobject (GUID: N Entry: N) with `spawntimesecs` (N) value, but the gameobject is marked as despawnable at action.
```

**Fix**: Low priority warning. Either set spawntimesecs to 0 or remove the despawnable flag.

### 6B. Gameobject Template Issues (~600 errors)

```
GameObject (Entry: N GoType: N) have dataN=N but PageText (Entry N) not exist.         (~280)
Gameobject (Entry: N GoType: N) have dataN=N but lock (Id: N) not found.               (~152)
Gameobject (Entry: N GoType: N) have dataN=N but Spell (Entry N) not exist.            (~83)
GameObject (Entry: N) Has non existing Destructible Hitpoint Record N.                 (~64)
GameObject (Entry: N GoType: N) have dataN=N but SpellFocus (Id: N) not exist.         (~44)
```

**Fix**: Clear the invalid data field references (set to 0).

### 6C. Gossip Issues (~420 errors)

```
[Condition] must have a `ScriptName` in `condition` table, ignoring.                   (~289)
Table gossip_menu: ID N is using non-existing TextID N                                 (~54)
Table `gossip_menu_option` ... can not use ActionMenuID for GossipOptionNpc...          (~79)
```

**Fix for conditions**: Add dummy script names or delete the conditions.

**Fix for TextID**: Update gossip_menu to use a valid NPC text ID (e.g., default 0 or existing text).

**Fix for ActionMenuID**: Set ActionMenuID to 0 for incompatible GossipOptionNpc types.

### 6D. NPC Vendor Issues (~1,070 errors)

```
Table `(game_event_)npc_vendor` has Item with wrong ExtendedCost for vendor, ignore    (~617)
Table `(game_event_)npc_vendor` for Vendor have non-existed item, ignore               (~244)
Table `(game_event_)npc_vendor` has data for nonexistent creature template, ignore     (~210)
```

**Fix**: Delete vendor entries with invalid items, costs, or for non-existent creatures.

```sql
DELETE FROM npc_vendor WHERE item NOT IN (SELECT entry FROM item_template) AND type = 0;
DELETE FROM npc_vendor WHERE entry NOT IN (SELECT entry FROM creature_template);
```

### 6E. Phase Issues (~290 errors)

```
[Condition] Area N does not have phase N.                                              (~261)
Phase N defined in `phase_area` does not exist, skipped.                               (~30)
```

**Fix**: Delete invalid phase_area entries and conditions referencing non-existent phases.

### 6F. Spawn Group Issues (~3,770 errors)

```
Spawn data with ID (N,N) not found, but is listed as a member of spawn group N!        (~3,527)
Spawn group N has map ID N, but spawn (N,N) has map id N                               (~244)
```

**Fix**: Delete broken spawn_group_member entries.

```sql
-- Type 0 = creature, Type 1 = gameobject
DELETE FROM spawn_group_member
WHERE spawnType = 0 AND spawnId NOT IN (SELECT guid FROM creature);

DELETE FROM spawn_group_member
WHERE spawnType = 1 AND spawnId NOT IN (SELECT guid FROM gameobject);
```

### 6G. Pool Issues (~1,800 errors)

```
Pool Id N is empty (has no creatures and no gameobjects...)                             (~1,806)
```

**Fix**: Delete empty pool definitions.

```sql
DELETE FROM pool_template WHERE entry NOT IN (
    SELECT pool_entry FROM pool_members  -- check actual table names
    UNION SELECT pool_entry FROM pool_creature
    UNION SELECT pool_entry FROM pool_gameobject
    UNION SELECT mother_pool FROM pool_pool
);
```

### 6H. Game Event Issues (~1,350 errors)

```
`game_event_creature` contains creature (GUID: N) not found in `creature` table.       (~1,116)
`game_event_gameobject` contains gameobject (GUID: N) not found in `gameobject` table.  (~241)
```

**Fix**: Delete broken game event references.

```sql
DELETE FROM game_event_creature WHERE guid NOT IN (SELECT guid FROM creature);
DELETE FROM game_event_gameobject WHERE guid NOT IN (SELECT guid FROM gameobject);
```

### 6I. Creature Text Issues (~1,370 errors)

```
CreatureTextMgr: Entry N, Group N in table `creature_text` has PlayType N but does not exist.  (~1,276)
CreatureTextMgr: Entry N, Group N ... using Language N but Language does not exist.             (~64)
NPCText (ID: N) has a probability sum N, no text can be selected.                              (~91)
NPCText (ID: N) has a non-existing BroadcastText (ID: N, Index: N)                            (~63)
```

**Fix**: Fix or delete invalid creature_text entries. Fix NPCText probability sums.

### 6J. Conversation Actors (~177 errors)

```
Table `conversation_actors` references an invalid creature guid (GUID: N) ...
```

**Fix**: Delete conversation_actors entries referencing non-existent creature GUIDs.

### 6K. Areatrigger Issues (~490 errors)

```
Table `areatrigger` has areatrigger (GUID: N) has unsupported difficulty N for map     (~449)
Table `areatrigger` has listed AreaTriggerCreatePropertiesId with non-zero flags        (~42)
```

**Fix**: Delete areatrigger spawns with invalid difficulties. Clear invalid flags.

### 6L. Spell Proc Warnings (~1,370 errors)

```
Spell Id N has DBC ProcFlags ... non-proc aura type, needs `spell_proc` table entry    (~1,270+)
Spell Id N has SPELL_ATTR3_CAN_PROC_FROM_PROCS ... no restriction ... infinite proc    (~45)
The `spell_proc` table entry for spellId N has a `SpellPhaseMask` value defined...     (~55)
```

**Fix**: These require adding entries to `spell_proc` table. Lower priority — these are
warnings about potential proc issues, not data corruption.

### 6M. Miscellaneous (~500+ errors)

- **Serverside spell difficulty** issues (~33): Serverside spells referencing regular spells
- **Race/Class/Level stats** missing (~32): Missing stat data for certain combinations
- **World location** invalid (~31): Bad WorldSafeLocs entries
- **LFG dungeon** wrong coords (~70): Fix lfg_dungeon_template coordinates
- **Trainer spell** bad skill refs (~36): Fix trainer_spell ReqSkillLine
- **vehicle_template_accessory** missing spellclick (~29): Add spellclick data or remove accessory
- **ui_map_quest_line** empty questline (~67): Delete orphaned references
- **spawn_tracking_quest_objective** invalid (~44): Delete orphaned tracking entries
- **skill_discovery_template** missing (~31): Add discovery template entries or ignore

## Recommended Execution Order

1. **6H** — Game event cleanup (removes broken creature/GO refs)
2. **6F** — Spawn group cleanup
3. **6A** — Gameobject spawn fixes (biggest category)
4. **6G** — Empty pool cleanup
5. **6D** — NPC vendor cleanup
6. **6B** — Gameobject template data fixes
7. **6C** — Gossip fixes
8. **6I/6J** — Creature text and conversation fixes
9. **6K** — Areatrigger fixes
10. **6E** — Phase fixes
11. **6L/6M** — Low-priority warnings

## SQL File Naming

- `2026_02_25_50_world.sql` — Game event creature/GO cleanup (6H)
- `2026_02_25_51_world.sql` — Spawn group and pool cleanup (6F, 6G)
- `2026_02_25_52_world.sql` — Gameobject spawn fixes (6A)
- `2026_02_25_53_world.sql` — Gameobject template data fixes (6B)
- `2026_02_25_54_world.sql` — NPC vendor cleanup (6D)
- `2026_02_25_55_world.sql` — Gossip, creature text, conversation fixes (6C, 6I, 6J)
- `2026_02_25_56_world.sql` — Areatrigger and phase fixes (6K, 6E)
- `2026_02_25_57_world.sql` — Spell proc, trainer, misc warnings (6L, 6M)

## Key Tables
- `gameobject` — spawn data
- `gameobject_template` — template data and data0-dataN fields
- `gossip_menu` / `gossip_menu_option` — NPC gossip menus
- `npc_vendor` / `game_event_npc_vendor` — vendor item lists
- `conditions` — conditional logic for gossip, loot, etc.
- `spawn_group_member` / `spawn_group_template` — spawn groups
- `pool_template` / `pool_creature` / `pool_gameobject` / `pool_pool` — pooling system
- `game_event_creature` / `game_event_gameobject` — event-based spawns
- `creature_text` / `npc_text` / `broadcast_text` — NPC dialogue
- `conversation_actors` / `conversation_template` — scripted conversations
- `areatrigger` — area trigger spawns
- `spell_proc` — spell proc overrides
- `phase_area` — phase-to-area mappings

## Key Source Files
- `src/server/game/Globals/ObjectMgr.cpp` — most validation happens here
- `src/server/game/Entities/GameObject/GameObjectData.h` — GO data structures
- `src/server/game/Conditions/ConditionMgr.cpp` — condition validation
- `src/server/game/Spells/SpellMgr.cpp` — spell proc loading
