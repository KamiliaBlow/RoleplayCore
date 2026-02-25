# Workflow 3: Creature Spawn & Template Fixes

**Database**: `world`
**Estimated errors**: ~209,000
**SQL output**: `sql/updates/world/master/2026_02_25_2X_world.sql`
**Priority**: High (second-largest category, some cause visual/functional issues)

## Error Categories

### 3A. Creatures Not Spawned in Any Difficulty (~99,000 errors)

```
Table `creature` has creature (GUID: N) that is not spawned in any difficulty, skipped.
```

These creatures exist in the `creature` table but have no matching entry in
`creature_template_difficulty` for any valid difficulty, so they never spawn.

**Fix**: Delete the orphaned spawn records.

```sql
-- Find creature GUIDs that have no valid difficulty
-- The error log contains the exact GUIDs
DELETE FROM creature WHERE guid IN (<list from log>);
-- Also clean up related tables:
DELETE FROM creature_addon WHERE guid IN (<list>);
DELETE FROM game_event_creature WHERE guid IN (<list>);
DELETE FROM creature_formations WHERE memberGUID IN (<list>) OR leaderGUID IN (<list>);
```

**Important**: Extract the GUID list from the error log. To do this programmatically:
```bash
grep "is not spawned in any difficulty" DBErrorsclaude.log | grep -oP 'GUID: \K[0-9]+' | sort -un > /tmp/orphan_creature_guids.txt
```

### 3B. Unsupported Difficulty for Map (~80,000 errors)

```
Table `creature` has creature (GUID: N) has unsupported difficulty N for map (Id: N).
```

Creatures are assigned a difficulty ID that doesn't exist for their map.

**Fix**: Delete the `creature_template_difficulty` entries for invalid map/difficulty
combos, or delete the spawn itself if it has no valid difficulties remaining.

```sql
-- Approach: Remove the spawn's difficulty entry that is invalid
-- Then check if any valid difficulties remain; if not, delete the spawn
```

### 3C. Non-Existing Faction Template (~31,000 errors)

```
Creature (Entry: N) has non-existing faction template (N). This can lead to crashes, set to faction N.
```

**Fix**: Update the faction to a known-good default (e.g., 35 = Friendly or 7 = neutral).

```sql
UPDATE creature_template_difficulty SET FactionID = 35
WHERE FactionID NOT IN (SELECT ID FROM faction_template_dbc)
AND FactionID != 0;
```

Note: You'll need to check what faction table/DBC is available. The server auto-corrects
this at runtime, but fixing the data prevents the log spam.

### 3D. Orphaned creature_template_difficulty Records (~26,000 errors)

```
Creature template (Entry: N) does not exist but has a record in `creature_template_difficulty`
```

**Fix**: DELETE from `creature_template_difficulty` where the Entry doesn't exist in
`creature_template`.

```sql
DELETE FROM creature_template_difficulty
WHERE Entry NOT IN (SELECT entry FROM creature_template);
```

### 3E. Disallowed Unit Flags (~23,000 errors)

```
Table `creature_template` lists creature (Entry: N) with disallowed `unit_flags` N      (~9,100)
Table `creature_template` lists creature (Entry: N) with disallowed `unit_flags2` N     (~12,200)
Table `creature` has creature (GUID: N Entry: N) with disallowed `unit_flags` N         (~8,700)
Table `creature` has creature (GUID: N Entry: N) with disallowed `unit_flags2` N        (~670)
```

The server already auto-removes these flags at runtime. To fix the data:

**Fix**: Mask out the disallowed bits. Need to read the source to determine which bits
are disallowed per creature type, then apply bitwise AND to remove them.

```sql
-- Example: The log tells you which flag value was removed
-- Parse the log to build UPDATE statements
```

### 3F. No Display/Model Data (~11,000 errors)

```
No model data exist for `CreatureDisplayID` = N listed by creature (Entry: N).          (~8,600)
Creature (Entry: N) does not have any existing display id in creature_template_model.   (~2,800)
```

**Fix**: For creatures with no valid display at all, they'll be invisible. Either:
- Delete their spawns (if they're not needed)
- Assign a placeholder model (e.g., human male = DisplayID 49)

### 3G. Creature Addon / Aura Issues (~1,800 errors)

```
Creature (GUID: N) has temporary aura (spell N) in `auras` field in `creature_addon`.  (~1,500)
Creature (Entry: N) has temporary aura (spell N) in `auras` field in `creature_template_addon`. (~290)
Creature (GUID: N) has wrong spell 'N' defined in `auras` field in `creature_addon`.   (~34)
Creature (GUID: N/Entry: N) has SPELL_AURA_CONTROL_VEHICLE aura...                     (~130)
```

**Fix**: Remove invalid/temporary aura spell IDs from the `auras` field. The `auras`
field is space-separated spell IDs.

### 3H. Miscellaneous Creature Issues (~remainder)

- **terrainSwapMap** invalid (~3,400): Set to -1
- **FEIGN_DEATH without IMMUNE flags** (~170): Remove the FEIGN_DEATH flag
- **equipment_id not found** (~690): Set equipment_id to 0
- **invalid curHealthPct** (~610): Set to 100
- **idle with wander_distance / random with no wander** (~950): Fix MovementType/wander_distance mismatch
- **invalid modelid** (~200): Set to 0
- **phaseGroup/phaseid doesn't exist** (~400): Set to 0
- **spawned at nonexistent map** (~340): Delete spawns
- **non-existing SpellN** (~160): Set the spell field to 0
- **non-existing KillCreditN** (~300): Set kill credit to 0
- **UNIT_NPC_FLAG_SPELLCLICK but no data** (~1,700): Remove the spellclick flag
- **ReactorAI not registered** (~190): Clear AIName field
- **non-existing creature_template_model** (~69): Delete orphaned records
- **creature_formations invalid leader/member** (~390): Delete broken formation entries
- **creature_model_info non-existent DisplayID** (~190): Delete bad model info records
- **creature_template_gossip for non-existent creature** (~57): Delete orphaned gossip

## Recommended Execution Order

1. **3D** — Remove orphaned `creature_template_difficulty` (cleans base data)
2. **3A** — Remove creatures with no valid difficulty
3. **3B** — Fix unsupported difficulty assignments
4. **3C** — Fix non-existing faction templates
5. **3E** — Fix disallowed unit flags
6. **3F** — Handle missing display/model data
7. **3G** — Clean up invalid auras
8. **3H** — All miscellaneous fixes

## SQL File Naming

- `2026_02_25_20_world.sql` — Orphaned creature_template_difficulty cleanup (3D)
- `2026_02_25_21_world.sql` — Creature no-difficulty spawn cleanup (3A)
- `2026_02_25_22_world.sql` — Unsupported difficulty fixes (3B)
- `2026_02_25_23_world.sql` — Faction template fixes (3C)
- `2026_02_25_24_world.sql` — Unit flags corrections (3E)
- `2026_02_25_25_world.sql` — Display/model data fixes (3F)
- `2026_02_25_26_world.sql` — Aura cleanup (3G)
- `2026_02_25_27_world.sql` — Miscellaneous creature fixes (3H)

## Key Tables
- `creature` — spawn data (GUID, position, map, difficulty, equipment, etc.)
- `creature_template` — base template (entry, name, flags, etc.)
- `creature_template_difficulty` — per-difficulty stats, loot, faction
- `creature_template_model` — display/model assignments
- `creature_addon` / `creature_template_addon` — auras, mount, emote, etc.
- `creature_equip_template` — equipment sets
- `creature_formations` — group movement formations
- `creature_model_info` — bounding box, gender, display info

## Key Source Files
- `src/server/game/Entities/Creature/CreatureData.h` — data structures
- `src/server/game/World/World.cpp` — `LoadCreatures()` validation
- `src/server/game/Globals/ObjectMgr.cpp` — template loading and validation
