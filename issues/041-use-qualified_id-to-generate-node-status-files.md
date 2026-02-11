# Issue #41: Use Qualified_id To Generate Node Status Files

**Status:** In Progress
**Branch:** `41-use-qualified_id-to-generate-node-status-files`
**Issue:** #41

---

## Description

Add a `qualified_id` field to node JSON serialization for human-readable identification in touch files and logs. Currently, touch files use numeric `node_id` which is not descriptive. The `qualified_id` provides meaningful identifiers (e.g., "12_3" for node 3 inside network node 12).

**Pattern observed in `test_files/qualified_id.json`:**
- Top-level nodes: simple string IDs ("0", "1", "2")
- Nested nodes: parent_child format ("12_3", "12_5", "12_10")

**Requirements:**
1. Serialize/deserialize `qualified_id` field in NodeObject
2. Use `qualified_id` in touch files (`.running`, `.succeeded`, `.failed`)
3. Display `qualified_id` in execution logs
4. Ensure uniqueness of `qualified_id` within a network
5. Handle missing `qualified_id` with auto-generation and warnings
6. Update all test files to include `qualified_id`
7. Add tests to verify functionality

---

## Design Decisions

### 1. Storage Location for qualified_id
- **Question:** Where should `qualified_id` be stored in NodeObject?
- **Decision:** Store in `initializer.json_serializer["qualified_id"]`
- **Rationale:** Consistent with existing architecture where JSON fields like "name", "type", "value" are stored in `initializer.json_serializer` rather than as direct member variables
- **Location:** `core/include/coral_implementation.h` (from_json/get_info functions)

### 2. Uniqueness Validation
- **Question:** When and how to validate `qualified_id` uniqueness?
- **Decision:** Validate during `Network::from_json` first pass (node creation), using `std::set` to track seen values
- **Rationale:** Early detection prevents conflicts in touch files/logs; doesn't require major structural changes; validates before any connections are made
- **Location:** `core/include/coral_network_implementation.h:529-578` (Network::from_json)

### 3. Missing qualified_id Handling
- **Question:** What to do when `qualified_id` is missing from JSON?
- **Decision:** Auto-generate with format `<node_id>_auto_<counter>` (e.g., "5_auto_0")
- **Rationale:**
  - Backward compatible with existing JSON files
  - `_auto_` marker clearly indicates it was generated
  - Counter ensures uniqueness even with repeated node IDs
  - Warning logged to notify users
- **Location:** `core/include/coral_network_implementation.h` (Network::from_json)

### 4. Auto-Generated ID Serialization
- **Question:** Should auto-generated qualified_ids be serialized to JSON?
- **Decision:** No - auto-generated IDs are NOT serialized (only user-provided IDs are serialized)
- **Rationale:**
  - Preserves backward compatibility and roundtrip consistency
  - Auto-generated IDs are transient (regenerated on each load)
  - Available at runtime for touch files and logs (their main purpose)
  - Prevents "pollution" of JSON with implementation-specific IDs
  - User-provided IDs are always preserved
- **Location:** `core/include/coral_network_implementation.h` (Network::nodes_to_json)
- **Implementation:** Track auto-generated IDs in `std::set<std::string> auto_generated_qualified_ids`; skip serialization for IDs in this set

---

## Implementation Plan

### Step 1: Add qualified_id to NodeObject JSON serialization ✅
- [x] **Task 1.1:** Add `get_qualified_id()` helper method to NodeObject
  - Location: `core/include/coral.h` (NodeObject public methods)
  - Return `qualified_id` from `initializer.json_serializer` or empty string if not present

- [x] **Task 1.2:** Add `set_qualified_id()` method and modify `from_json`
  - Location: `core/include/coral.h:1260` (set_qualified_id declaration)
  - Location: `core/include/coral_implementation.h:624` (set_qualified_id implementation)
  - Location: `core/include/coral_implementation.h:888` (from_json usage)
  - Uses public setter method (coherent with `parse_string` pattern)

- [x] **Task 1.3:** Verify `get_info` automatically includes `qualified_id`
  - Location: `core/include/coral_implementation.h:522` (get_info function)
  - No changes needed (automatic since it returns `initializer.json_serializer`)

- [x] **CHECKPOINT:** Build + run tests (verify nothing broke) ✅
  - Added tests: `Serialize.QualifiedId` and `Serialize.QualifiedIdMissing`
  - All tests passing

### Step 2: Add qualified_id handling to Network class ✅
- [x] **Task 2.1:** Add `get_node_qualified_id()` method to Network
  - Location: `core/include/coral_network.h:125` (declaration)
  - Location: `core/include/coral_network_implementation.h:348` (implementation)
  - Returns node's `qualified_id` or fallback to `std::to_string(node_id)`

- [x] **Task 2.2:** Validate uniqueness and auto-generate in `Network::from_json`
  - Location: `core/include/coral_network_implementation.h:565-625` (first pass)
  - Track seen `qualified_id` values with `std::set` for uniqueness validation
  - Throw error on duplicate qualified_ids
  - Auto-generate missing `qualified_id` with format `<node_id>_auto_<counter>`
  - Track auto-generated IDs in `auto_generated_qualified_ids` set
  - Log warning when auto-generating

- [x] **Task 2.3:** Include `qualified_id` in `Network::nodes_to_json` output
  - Location: `core/include/coral_network_implementation.h:1050-1059`
  - Add `qualified_id` to serialized node JSON
  - Only serialize user-provided qualified_ids (skip auto-generated ones)

- [x] **CHECKPOINT:** Build + run tests ✅
  - Added tests: `Network.DuplicateQualifiedIdThrows` and `Network.AutoGeneratedQualifiedId`
  - Fixed `Network.HyperCubeNetworkRoundTrip` compatibility
  - All tests passing

### Step 3: Use qualified_id in touch files and logs
- [ ] **Task 3.1:** Modify `execute_node_task` to use `qualified_id` for touch files
  - Location: `core/include/coral_network_implementation.h:207-239`
  - Replace `std::to_string(node_id)` with `get_node_qualified_id(node_id)` in touch_file calls (lines 216, 227, 237)

- [ ] **Task 3.2:** Update log messages to include `qualified_id`
  - Location: `core/include/coral_network_implementation.h:212-215, 232-235`
  - Modify slog_info calls to show both `node_id` and `qualified_id`

- [ ] **CHECKPOINT:** Build + run tests

### Step 4: Update all test JSON files
- [ ] **Task 4.1:** Add `qualified_id` to all test files in `test_files/` directory
  - Files: vtk-gen1.json, vtk-gen2.json, vtk-gen3.json, vtk-single.json
  - Files: step-0_network.json, mwe.json, mwe_imgui.json
  - Files: networknode-order1.json, networknode-order2.json, networknode-noarguments.json
  - Files: hyper_cube_node.json, graph-no-name.json, graph-named.json
  - Files: editor-state.json, editor_state.json
  - Rule: Add `"qualified_id": "<node_id>"` (string version of numeric ID)

- [ ] **CHECKPOINT:** Build + run all tests

### Step 5: Write tests for qualified_id functionality
- [ ] **Task 5.1:** Test qualified_id deserialization
  - Verify NodeObject correctly reads and stores `qualified_id` from JSON

- [ ] **Task 5.2:** Test qualified_id serialization
  - Verify NodeObject includes `qualified_id` in output JSON

- [ ] **Task 5.3:** Test uniqueness validation
  - Verify Network::from_json rejects duplicate `qualified_id` values

- [ ] **Task 5.4:** Test auto-generation for missing qualified_id
  - Verify auto-generated format is `<node_id>_auto_<counter>`
  - Verify warning is logged

- [ ] **Task 5.5:** Test touch files use qualified_id as filename
  - Run a network and verify touch files use `qualified_id` in names
  - Check for `.running`, `.succeeded`, `.failed` files

- [ ] **CHECKPOINT:** Build + run all tests

---

## Implementation Log

### 2026-02-11
- Started planning with Claude Code
- Analyzed project structure and requirements
- Defined implementation plan with 5 major steps
- Documented 3 key design decisions

**Step 1 Implementation (Completed):**
- Added `get_qualified_id()` method to NodeObject (coral.h:1254)
- Added `set_qualified_id()` method to NodeObject (coral.h:1260, coral_implementation.h:624)
- Modified `from_json` to store qualified_id using public setter (coral_implementation.h:888)
- Verified `get_info()` automatically includes qualified_id
- Added two unit tests to verify functionality:
  - `Serialize.QualifiedId` - tests reading and writing qualified_id
  - `Serialize.QualifiedIdMissing` - tests behavior when qualified_id is absent
- Fixed test to use proper type hash (learned to serialize existing object first)
- All core tests passing ✅

**Design Refinement:**
- Initially tried to access `obj->initializer` directly from `from_json`
- Learned this violates encapsulation (initializer is private)
- Solution: Added public `set_qualified_id()` method, coherent with existing `parse_string()` pattern

**Step 2 Implementation (Completed):**
- Added `get_node_qualified_id()` method to Network (coral_network.h:125, coral_network_implementation.h:348)
- Implemented uniqueness validation in `Network::from_json` (coral_network_implementation.h:565-625)
- Auto-generate qualified_ids with format `<node_id>_auto_<counter>` when missing
- Added tracking set `auto_generated_qualified_ids` to Network class (coral_network.h:69)
- Modified `nodes_to_json` to conditionally serialize qualified_id (coral_network_implementation.h:1050-1059)
- Added two comprehensive tests:
  - `Network.DuplicateQualifiedIdThrows` - validates uniqueness enforcement
  - `Network.AutoGeneratedQualifiedId` - validates auto-generation and roundtrip behavior
- All tests passing ✅

**Design Decision: Auto-Generated ID Serialization**
- **Issue:** `HyperCubeNetworkRoundTrip` test failed because roundtrip added auto-generated qualified_ids
- **Problem:** Programmatically created networks → serialize → deserialize → auto-generate IDs → JSONs differ
- **Options considered:**
  1. Don't serialize auto-generated qualified_ids (only user-provided ones)
  2. Exclude qualified_id from test comparison (like date_time_utc)
  3. Always assign qualified_ids to programmatically created nodes
- **Decision:** Option 1 - Don't serialize auto-generated qualified_ids
- **Rationale:**
  - User-provided qualified_ids are preserved across serialization
  - Auto-generated IDs are transient (regenerated on each load)
  - Backward compatible with existing tests and JSON files
  - Auto-generated IDs still available at runtime for touch files and logs
  - Roundtrip works perfectly (no JSON pollution)
- **Implementation:** Added `auto_generated_qualified_ids` set to track which IDs were auto-generated; check this set in `nodes_to_json` to skip serialization

**Design Refinement: String Pattern vs Explicit Tracking**
- **Issue:** Initially used `.find("_auto_")` to detect auto-generated IDs during serialization
- **Problem:** User could legitimately name qualified_id like "my_auto_script" causing incorrect filtering
- **Solution:** Track auto-generated IDs explicitly in `std::set<std::string> auto_generated_qualified_ids`
- **Benefits:** Robust, no reliance on string patterns, clear intent

---

## Outcome

[To be filled when completed]
