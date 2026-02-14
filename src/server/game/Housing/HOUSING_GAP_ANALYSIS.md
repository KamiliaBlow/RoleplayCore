# Housing System Gap Analysis

## Audit Date: 2026-02-12
## Branch: feature/housing-system

---

## 1. IMPLEMENTATION STATUS OVERVIEW

### Core Backend Classes (Server-Side Logic)

| Component | File | Methods | Status | Notes |
|-----------|------|---------|--------|-------|
| Housing | Housing.cpp | 30/30 | **100% IMPLEMENTED** | Full DB persistence, decor/room/fixture CRUD |
| Neighborhood | Neighborhood.cpp | 21/21 | **100% IMPLEMENTED** | Full roster, invite, ownership management |
| NeighborhoodCharter | NeighborhoodCharter.cpp | 7/7 | **100% IMPLEMENTED** | Create, sign, finalize charter flow |
| NeighborhoodMgr | NeighborhoodMgr.cpp | 11/11 | **100% IMPLEMENTED** | Singleton lifecycle, finder, cross-queries |
| HousingMap | HousingMap.cpp | 5/5 | **100% IMPLEMENTED** | Instanced map creation, plot phasing |
| HousingMgr | HousingMgr.cpp | 10 impl / 8 skel | **~54% COMPLETE** | Load* functions are placeholders |

### Packet Layer

| Component | Status | Notes |
|-----------|--------|-------|
| CMSG Read() methods | **67/67 correct** | All wire formats verified against IDA analysis |
| SMSG Write() methods | **2/8 implemented** | QueryNeighborhoodNameResponse, InvalidateNeighborhoodName |
| Opcode registration | **58/59 mapped** | 1 remaining: CMSG_GUILD_GET_OTHERS_OWNED_HOUSES |

### Handler Layer

| File | Handlers | Functional | Status |
|------|----------|-----------|--------|
| HousingHandler.cpp | 46 | 0 | **ALL STUBS** - TC_LOG_DEBUG only |
| NeighborhoodHandler.cpp | 21 | 0 | **ALL STUBS** - TC_LOG_DEBUG only |

### Database Layer

| Component | Status | Notes |
|-----------|--------|-------|
| SQL schema | **10 tables defined** | housing_schema.sql complete |
| Prepared statements | **28 defined** | CharacterDatabase.h/.cpp |
| DB2 structures | **15/15 defined** | DB2Structure.h |
| DB2 stores | **17/17 loaded** | DB2Stores.h/.cpp |
| Hotfix selectors | **38 defined** | HotfixDatabase.h/.cpp |

---

## 2. CRITICAL GAPS

### GAP-1: Handlers Not Wired to Backend (SEVERITY: CRITICAL)

**Problem**: All 67 handlers log the incoming packet but never call Housing, Neighborhood, or NeighborhoodCharter methods.

**Example** (current):
```cpp
void WorldSession::HandleHousingDecorPlace(WorldPackets::Housing::HousingDecorPlace const& housingDecorPlace)
{
    Player* player = GetPlayer();
    if (!player) return;
    TC_LOG_DEBUG("network", "CMSG_HOUSING_DECOR_PLACE_NEW_DECOR ...");
    // DOES NOTHING - never calls Housing::PlaceDecor()
}
```

**Required**: Each handler must validate input, call the appropriate backend method, and send an SMSG response.

**Impact**: System is completely non-functional from a player perspective.

### GAP-2: Player Integration Missing (SEVERITY: CRITICAL)

**Problem**: No `m_housing` member on Player. Housing is never loaded on login or saved on logout.

**Required**:
- Add `Housing* m_housing` to Player class (or use pattern like `Player::GetGarrison()`)
- Load housing data in Player::_LoadFromDB or similar
- Save housing data on Player logout/disconnect
- Initialize housing on first-time setup

**Impact**: Even if handlers were wired, there's no Housing instance to call methods on.

### GAP-3: Server Startup Integration Missing (SEVERITY: CRITICAL)

**Problem**: HousingMgr::Initialize() and NeighborhoodMgr::Initialize() are never called during World::SetInitialWorldSettings().

**Required**:
- Call `sHousingMgr->Initialize()` during server startup (after DB2 stores loaded)
- Call `sNeighborhoodMgr->Initialize()` to load active neighborhoods from DB
- Register shutdown hooks for cleanup

**Impact**: Managers never populate their caches; all queries return empty/default.

### GAP-4: HousingMgr Load Functions Empty (SEVERITY: HIGH)

**Problem**: 8 Load* functions in HousingMgr are placeholders that don't read DB2 stores.

**Functions affected**:
- `LoadHouseData()` - Should iterate `sHouseStore`
- `LoadHouseDecorData()` - Should iterate `sHouseDecorStore`
- `LoadHouseRoomData()` - Should iterate `sHouseRoomStore`
- `LoadHouseThemeData()` - Should iterate `sHouseThemeStore`
- `LoadNeighborhoodMapData()` - Should iterate `sNeighborhoodMapStore`
- `LoadNeighborhoodPlotData()` - Should iterate `sNeighborhoodPlotStore`
- `LoadHouseDecorMaterialData()` - Should iterate `sHouseDecorMaterialStore`
- `LoadHouseDecorThemeSetData()` - Should iterate `sHouseDecorThemeSetStore`

**Impact**: Runtime data caches are empty. All GetXxxData() calls return nullptr.

### GAP-5: Missing SMSG Packet Classes (SEVERITY: HIGH)

**Problem**: 6 SMSG opcodes have no packet class implementations.

| SMSG Opcode | Purpose | Priority |
|-------------|---------|----------|
| SMSG_ACCOUNT_HOUSING_ROOM_ADDED | Notify room unlock | HIGH |
| SMSG_ACCOUNT_HOUSING_FIXTURE_ADDED | Notify fixture unlock | HIGH |
| SMSG_ACCOUNT_HOUSING_THEME_ADDED | Notify theme unlock | MEDIUM |
| SMSG_ACCOUNT_HOUSING_ROOM_COMPONENT_TEXTURE_ADDED | Notify wallpaper unlock | MEDIUM |
| SMSG_CRAFTING_HOUSE_HELLO_RESPONSE | Crafting station interaction | LOW |
| SMSG_GUILD_OTHERS_OWNED_HOUSES_RESULT | Guild house listing | LOW |

**Impact**: Server cannot notify clients of state changes. Client UI won't update.

### GAP-6: Missing CMSG Handler (SEVERITY: LOW)

**Problem**: `CMSG_GUILD_GET_OTHERS_OWNED_HOUSES` still mapped to `Handle_NULL`.

**Impact**: Minor - guild house browsing won't work.

---

## 3. KNOWLEDGE AVAILABLE BUT NOT YET USED

### 3.1 IDA-Derived Wire Formats (FULLY USED)

All 67 CMSG packet Read() methods correctly implement the wire format from IDA analysis. **No gaps.**

### 3.2 DB2 Table Structures (DEFINED, NOT CONSUMED)

All 15 DB2 structures are correctly defined in DB2Structure.h and registered in DB2Stores. However, HousingMgr never iterates these stores to populate runtime caches.

**Justification**: The DB2 stores ARE loaded by the engine automatically. The gap is that HousingMgr::LoadXxx() functions don't copy the data into the runtime `std::unordered_map` caches. This is a wiring issue, not a knowledge gap.

### 3.3 Client Lua API (~80 functions identified)

The IDA analysis identified ~80 C_Housing* Lua API functions. These map 1:1 to CMSG/SMSG pairs. All corresponding CMSG handlers exist; the gap is SMSG responses.

### 3.4 Client-Side Structures Not Replicated

8-10 client-side structures identified in IDA have no server-side equivalents:

| Client Structure | Server Need | Priority |
|-----------------|-------------|----------|
| HousingCatalogSearcher | Catalog filtering/search | MEDIUM |
| HousingRoomSystem_C::DoorConnection | Room connectivity graph | HIGH |
| FHousingPlotAreaTrigger_C | Plot boundary enforcement | MEDIUM |
| FHousingFixture_C | Fixture visual state | LOW (client-only) |
| FHousingRoom_C | Room visual state | LOW (client-only) |
| FHousingRoomComponentMesh_C | Mesh state | LOW (client-only) |
| FNeighborhoodMirrorData_C | Server→client sync blob | HIGH |
| CHousingUI | UI controller | LOW (client-only) |

**Justification**: Client-only visual structures (Fixture_C, Room_C, Mesh_C, CHousingUI) don't need server equivalents. DoorConnection and MirrorData should be implemented for validation.

### 3.5 GarrisonType Relationship

IDA confirmed housing does NOT use a new GarrisonType. It's a fully separate system with its own GUID type (HighGuid::Housing). This is correctly implemented.

### 3.6 GameTable Data

No GameTable entries exist for housing. All balance data comes from DB2 tables (HouseLevelData, HouseDecor costs, etc.). **No gap.**

---

## 4. ENHANCEMENT ROADMAP

### Phase A: Critical Wiring (Must-Have for Functionality)

1. **Wire all 67 handlers** to call backend methods
2. **Add Player::GetHousing()** integration with load/save
3. **Call HousingMgr/NeighborhoodMgr::Initialize()** at server startup
4. **Implement HousingMgr::Load*()** functions to consume DB2 stores
5. **Implement 4 critical SMSG packets** (room/fixture/theme/texture added)

### Phase B: Response Packets (Required for Client UI)

6. **Add SMSG response packets** for all handler categories:
   - Decor placement confirmations
   - Room layout updates
   - Fixture selection confirmations
   - Neighborhood roster responses
   - House finder results
   - Charter status updates

### Phase C: Validation & Polish

7. **Input validation** in handlers (permission checks, range validation)
8. **DoorConnection graph** for room connectivity validation
9. **Catalog search** server-side implementation
10. **Neighborhood initiative** system (tasks, rewards, progress)

### Phase D: Integration

11. **Housing kiosk** reset functionality
12. **Guild neighborhood** creation flow
13. **House finder** browsing system
14. **Tutorial** system integration
15. **Virtual currency** purchase hooks

---

## 5. JUSTIFICATION FOR UNIMPLEMENTED ITEMS

| Item | Reason |
|------|--------|
| HousingMgr Load* empty | Phase 1 focused on schema + packet layer; DB2 iteration is Phase A enhancement |
| Handlers are stubs | Phase 6 created handler skeletons; wiring requires Player integration (circular dependency resolved now) |
| Missing SMSGs | Wire format for SMSG responses requires client behavior analysis; CMSG-first approach was correct |
| No Player integration | Modifying Player.h/cpp is a core change requiring careful planning per CLAUDE.md hierarchy |
| Startup hooks missing | World.cpp modification is core; deferred to minimize initial PR footprint |
| Client structures | Server doesn't need client visual state; only DoorConnection and MirrorData are server-relevant |
| Guild houses opcode | Lower priority; personal housing is the primary flow |
