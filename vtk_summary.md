# VTK Issue Summary - Current Status

## Problem Statement
`vtk-ko.json` generates an empty VTK file while `vtk-ok.json` works correctly. The difference: vtk-ko passes an external triangulation to the embedded network, while vtk-ok creates the triangulation internally.

## Root Cause (Current Understanding)

### Critical Finding: Node 11 (write_vtk) Never Executes

**Observed Behavior:**
- In vtk-ko.json: Embedded network node 11 (`GridOut::write_vtk`) never runs
- All its dependencies execute successfully:
  - Node 3 (generate_from_name_and_arguments) ✓ runs
  - Node 5 (refine_global) ✓ runs
  - Node 8 (GridOut constructor) ✓ runs
  - Node 10 (ofstream constructor) ✓ runs
- Node 11 appears in the taskflow DAG with correct dependencies
- But the taskflow executor never invokes node 11

### Bugs Fixed So Far

**Bug #1: Argument Index Remapping (FIXED) ✅**
- **Problem:** Embedded network's "inputs" arrays had hardcoded indices that didn't match when outer network argument order changed
- **Symptom:** Triangulation incorrectly bound to ofstream node (type mismatch)
- **Fix:** Implemented `build_argument_remap_table()` to match arguments by name+type instead of position
- **Status:** Working correctly - args_map is now correct

### Investigated Hypotheses

**Hypothesis #1: Node ID Namespace Collision ❌ INCORRECT**
- **Theory:** Outer network nodes 10/11 conflict with embedded network nodes 10/11 causing circular dependency
- **Investigation:**
  - Implemented node ID remapping (offset by 1000) to separate namespaces
  - Re-established input/output pointers after remapping
  - Result: Node ID remapping didn't solve the problem
- **Conclusion:** Each Network object has its own taskflow (line 108 in coral_network.h). They don't share taskflow or node_tasks. The circular dependency seen in logs (`10 precedes 11` AND `11 precedes 10`) is from separate networks, not a conflict.

**Hypothesis #2: Missing Taskflow Dependencies ❌ INCORRECT**
- **Theory:** Dynamic node addition doesn't call `.precede()` to establish taskflow dependencies
- **Investigation:** Added logging, confirmed `add_connection()` DOES call `.precede()` at line 287
- **Conclusion:** Taskflow dependencies ARE being established correctly

## Current Status: UNSOLVED

**What Works:**
- Argument remapping (name+type matching) ✅
- args_map generation is correct ✅
- External argument nodes added properly ✅
- Taskflow dependencies established correctly ✅
- All prerequisite nodes for node 11 execute successfully ✅

**What's Broken:**
- Node 11 (write_vtk) in embedded network never executes ❌
- VTK file remains empty (0 bytes) ❌
- No error messages or exceptions thrown ❌

**Evidence of the Problem:**
```
# With remapping disabled (original node IDs):
Nodes that RUN: 3, 5, 8, 10, 12, 13, 14, 15, 16
Nodes that DON'T RUN: 11 (write_vtk)

# Taskflow DAG shows node 11 with all dependencies:
- Node 8 (GridOut) → Node 11 ✓
- Node 5 (refine_global) → Node 11 ✓
- Node 10 (ofstream) → Node 11 ✓

# All dependencies execute, but node 11 never runs!
```

## Next Steps to Investigate

1. **Check if node 11 inputs are properly set:**
   - All three inputs should be valid NodeObject pointers
   - Check if any input is nullptr or invalid

2. **Examine taskflow execution order:**
   - Dump full taskflow execution sequence
   - Check if taskflow completes without running all nodes
   - Investigate if taskflow detects a cycle/deadlock silently

3. **Compare vtk-ok vs vtk-ko embedded networks:**
   - Both have identical embedded network JSON
   - vtk-ok has node 0 (triangulation constructor) internally
   - vtk-ko receives triangulation externally as argument node 12
   - Check if the externally-provided triangulation has different properties

4. **Check node ready() status:**
   - Node 11 ready()=false before execution
   - Investigate why node 11 isn't becoming ready even after dependencies complete
   - Check if node execution is conditional on ready() status

5. **Examine pass-through argument handling:**
   - Node 11 has output argument (output_file, pass_through)
   - Check if pass-through output binding affects execution

## Diagnostic Commands

```bash
# Run vtk-ko test
docker exec coral bash -c "cd /app && ./build/dealii_backend.g run test_files/vtk-ko.json"

# Check which nodes execute
docker exec coral bash -c "cd /app && ./build/dealii_backend.g run test_files/vtk-ko.json 2>&1 | grep 'Running node'"

# Check VTK output
docker exec coral bash -c "cd /app && ls -la grid-1.vtk && wc -l grid-1.vtk"
```

## File Locations
- Test files: `/home/matteo/Projects/dealII-X/repos/coral-editor/test_files/vtk-{ok,ko}.json`
- Main code: `include/coral_network.h` (Network class, lines 86-1327)
- Network executor: `coral_network.h` lines 1113-1427
- Argument remapping: `coral_network.h` lines 767-893
