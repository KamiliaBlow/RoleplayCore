# Workflow 5: Quest Data Fixes

**Database**: `world`
**Estimated errors**: ~16,000
**SQL output**: `sql/updates/world/master/2026_02_25_4X_world.sql`
**Priority**: Medium (quests mostly non-functional on RP server, but data cleanliness matters)

## Error Categories

### 5A. quest_poi References (~10,500 errors)

```
`quest_poi` quest id (N) Idx (N) does not exist in `quest_template`                   (~10,350)
Table quest_poi references unknown quest points for quest N POI id N                   (~137)
```

**Fix**: Delete `quest_poi` / `quest_poi_points` entries for quests not in `quest_template`.

```sql
DELETE FROM quest_poi WHERE QuestID NOT IN (SELECT ID FROM quest_template);
DELETE FROM quest_poi_points WHERE QuestID NOT IN (SELECT ID FROM quest_template);
```

### 5B. Non-Existing Creature/GO for Quest Objectives (~1,000 errors)

```
Quest N objective N has non existing creature entry N, quest can't be done.            (~638)
Quest N objective N has non existing gameobject entry N, quest can't be done.          (~367)
Quest N objective N has non existing areatrigger id N                                  (~213)
```

**Fix**: These are quest objectives referencing entities that don't exist in this build.
Update `quest_objectives` to clear the invalid ObjectID, or delete the objective.

```sql
-- Clear objectives that reference non-existent creatures
UPDATE quest_objectives SET ObjectID = 0
WHERE Type = 0  -- QUEST_OBJECTIVE_MONSTER
AND ObjectID NOT IN (SELECT entry FROM creature_template);

-- Clear objectives that reference non-existent gameobjects
UPDATE quest_objectives SET ObjectID = 0
WHERE Type = 1  -- QUEST_OBJECTIVE_ITEM... check the actual type enum
AND ObjectID NOT IN (SELECT entry FROM gameobject_template);
```

**Note**: Check `src/server/game/Quests/QuestDef.h` for the `QuestObjectiveType` enum
to get the correct Type values.

### 5C. Quest StartItem / ItemDrop Issues (~1,900 errors)

```
Quest N has `StartItem` = N but `ProvidedItemCount` = N, set to N but need fix in DB.  (~1,585)
Quest N has `ItemDropN` = N but item with entry N does not exist                       (~189)
Quest N has `SourceItemId` = N but item with entry N does not exist                    (~127)
Quest N has `ItemDropN` = N but `ItemDropQuantityN` = 0.                               (~113)
```

**Fix for ProvidedItemCount**: Set `ProvidedItemCount` = 1 where `StartItem` != 0 but count is 0.

```sql
UPDATE quest_template_addon SET ProvidedItemCount = 1
WHERE ProvidedItemCount = 0
AND ID IN (SELECT ID FROM quest_template WHERE StartItem != 0);
```

**Fix for non-existent items**: Clear the ItemDrop/SourceItemId fields.

**Fix for zero quantity**: Set quantity to 1.

### 5D. Reward Issues (~1,200 errors)

```
Quest N has `RewardSpellCast` = N but spell N does not exist                           (~880)
Quest N has `RewardItemIdN` = N but item with entry N does not exist                   (~36)
Quest N has `RewardFactionIdN` = N but `RewardFactionValueIdOverrideN` = N.            (~256)
```

**Fix**: Clear invalid reward spell/item references. Fix faction reward overrides.

```sql
-- Clear invalid reward spells
UPDATE quest_template SET RewardSpell = 0
WHERE RewardSpell != 0
AND RewardSpell NOT IN (SELECT ID FROM spell_dbc_or_equivalent);
```

### 5E. Quest Chain Issues (~280 errors)

```
Quest N has PrevQuestId N, but no such quest                                           (~223)
Quest N has NextQuestId N, but no such quest                                           (~60)
Quest N has `NextQuestInChain` = N but quest N does not exist                          (~61)
```

**Fix**: Clear the PrevQuestId/NextQuestId/NextQuestInChain references to non-existent quests.

```sql
UPDATE quest_template_addon SET PrevQuestID = 0
WHERE PrevQuestID != 0
AND ABS(PrevQuestID) NOT IN (SELECT ID FROM quest_template);
```

### 5F. Quest Visual Effects (~317 errors)

```
Table `quest_visual_effect` has visual effect for objective N but such objective does not exist.
```

**Fix**: Delete orphaned visual effect records.

```sql
DELETE FROM quest_visual_effect
WHERE QuestObjectiveId NOT IN (SELECT ID FROM quest_objectives);
```

### 5G. Quest SortID / SpecialFlags (~140 errors)

```
Quest N has `QuestSortID` = -N but `RequiredSkillId` does not have a corresponding value  (~32)
Quest N has `SpecialFlags` = N > max allowed value. Correct `SpecialFlags` to...           (~108)
```

**Fix**: Cap SpecialFlags to the max allowed value. Fix SortID/skill mismatches.

### 5H. Quest Starter/Ender for Non-Existent Entities (~135 errors)

```
Table `creature_queststarter` has data for nonexistent creature entry (N) and existed quest N  (~102)
Table `gameobject_questender` has data for nonexistent gameobject entry (N) and existed quest N (~34)
```

**Fix**: Delete quest starter/ender records for non-existent creatures/gameobjects.

```sql
DELETE FROM creature_queststarter WHERE id NOT IN (SELECT entry FROM creature_template);
DELETE FROM creature_questender WHERE id NOT IN (SELECT entry FROM creature_template);
DELETE FROM gameobject_queststarter WHERE id NOT IN (SELECT entry FROM gameobject_template);
DELETE FROM gameobject_questender WHERE id NOT IN (SELECT entry FROM gameobject_template);
```

## Recommended Execution Order

1. **5A** — Clean up quest_poi (biggest category, simple DELETE)
2. **5H** — Clean up quest starters/enders for missing entities
3. **5B** — Fix invalid quest objectives
4. **5C** — Fix item references
5. **5D** — Fix reward references
6. **5E** — Fix quest chains
7. **5F/5G** — Visual effects and flag fixes

## SQL File Naming

- `2026_02_25_40_world.sql` — Quest POI cleanup (5A)
- `2026_02_25_41_world.sql` — Quest starter/ender cleanup (5H)
- `2026_02_25_42_world.sql` — Quest objectives fixes (5B)
- `2026_02_25_43_world.sql` — Quest item/reward fixes (5C, 5D)
- `2026_02_25_44_world.sql` — Quest chain and misc fixes (5E, 5F, 5G)

## Key Tables
- `quest_template` — base quest data
- `quest_template_addon` — extended quest fields (PrevQuestID, ProvidedItemCount, etc.)
- `quest_objectives` — objective definitions
- `quest_poi` / `quest_poi_points` — map markers
- `quest_visual_effect` — visual effects for objectives
- `creature_queststarter` / `creature_questender` — NPC quest givers/enders
- `gameobject_queststarter` / `gameobject_questender` — GO quest givers/enders

## Key Source Files
- `src/server/game/Quests/QuestDef.h` — quest structures and enums
- `src/server/game/Globals/ObjectMgr.cpp` — quest loading validation (search for "quest_poi", "quest_template")
