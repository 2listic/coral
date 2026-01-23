# Fix Plan: Dynamic Input Resolution at Execution Time

## Solution Overview
Instead of storing static pointers to node outputs during connection setup (which may point to unready objects), we will **dynamically resolve inputs right before each node executes**. This ensures that every node always gets the latest, ready outputs from its predecessors.

## Root Cause Recap
Currently, `add_connection()` calls `set_input(target_input, source->output(source_output))` during network setup, storing a pointer that may point to an unready object. This pointer never updates, causing "Arguments not ready" exceptions even after source nodes execute successfully.

---

## Step 1: Understand and Document Current Implementation ✅ COMPLETED
Goal: Fully understand the current connection mechanism before modifying it.

- [x] Read and document how `add_connection()` currently works (coral_network.h:317-392)
- [x] Identify all places where `set_input()` is called
  - [x] In `add_connection()` (coral_network.h:370-371)
  - [x] In network executor for dynamic nodes (coral_network.h:1232-1238)
  - [x] Any other locations?
- [x] Document the `connections` map structure and what it contains
- [x] Verify that `connections` map has all info needed (source_id, target_id, source_output, target_input)
- [x] Confirm that `connections` map persists throughout network lifetime

**Expected outcome:** Clear understanding of where and how connections are established.
**Result:** ✅ Understanding achieved through context.md documentation.

---

## Step 2: Design the Dynamic Resolution Logic ✅ COMPLETED
Goal: Design the input refresh mechanism that will run before each node execution.

- [x] Determine the exact location for refresh logic: in task lambda, before `(*node)()` call
- [x] Design the refresh loop that iterates through connections
  - [x] Should only refresh inputs for the current node (filter by `conn.target_id == id`)
  - [x] Should call `set_input()` for each matching connection
  - [x] Should use fresh `output()` from source nodes
- [x] Decide if refresh is needed in both:
  - [x] `add_node()` lambda (coral_network.h:~245)
  - [x] `rebuild_taskflow()` lambda (coral_network.h:~206)
- [x] Consider edge cases:
  - [x] What if a source node isn't ready yet? (Should not happen due to taskflow dependencies)
  - [x] What if connections are empty? (Safe to skip)
  - [x] What if node has no inputs? (Loop does nothing, safe)
- [x] Design logging strategy to verify refresh happens correctly

**Expected outcome:** Clear pseudocode for the refresh logic.
**Result:** ✅ Design completed and documented in context.md.

---

## Step 3: Implement Input Refresh in add_node() Lambda ✅ COMPLETED
Goal: Add dynamic input resolution before node execution in the main execution path.

- [x] Locate the task lambda in `add_node()` method (coral_network.h:~242-255)
- [x] Add input refresh logic BEFORE the try block (or inside try, before `(*node)()` call)
- [x] Implement the refresh loop:
  ```cpp
  // Refresh all inputs for this node from connections
  for (const auto &[conn_id, conn] : this->connections) {
    if (conn.target_id == id) {
      this->nodes[conn.target_id]->set_input(
        conn.target_input,
        this->nodes[conn.source_id]->output(conn.source_output)
      );
    }
  }
  ```
- [x] Add debug logging to track refresh:
  ```cpp
  std::cout << "[DEBUG] Refreshing inputs for node " << id << std::endl;
  ```
- [x] Add logging for each connection refreshed:
  ```cpp
  std::cout << "[DEBUG]   Refreshed input " << conn.target_input
            << " from node " << conn.source_id << std::endl;
  ```
- [x] Ensure `this->connections` and `this->nodes` are accessible in lambda capture
- [x] Verify code compiles

**Expected outcome:** add_node() lambda refreshes inputs before execution.
**Result:** ✅ Implemented successfully in coral_network.h lines ~249-263.

---

## Step 4: Implement Input Refresh in rebuild_taskflow() Lambda ✅ COMPLETED
Goal: Ensure consistency by adding the same logic to rebuild mode.

- [x] Locate the task lambda in `rebuild_taskflow()` method (coral_network.h:~203-220)
- [x] Add the same input refresh logic as in Step 3
- [x] Use consistent logging format
- [x] Ensure code compiles

**Expected outcome:** rebuild_taskflow() lambda also refreshes inputs before execution.
**Result:** ✅ Implemented successfully, added `this` to lambda capture and refresh logic.

---

## Step 5: Test the Fix with Original Problem Case ✅ COMPLETED
Goal: Verify the fix solves the original issue.

- [x] Rebuild the project
- [x] Run with the problematic test file: `/app/test_files/vtk-ko.json`
- [x] Check output.log for:
  - [x] "[DEBUG] Refreshing inputs for node 5" appears before task 5 execution ✅ Line 336
  - [x] Task 5 does NOT throw "Arguments not ready" exception ✅ No exception!
  - [x] Task 5 completes successfully (sees `[LAMBDA EXIT] Task 5 lambda finished!`) ✅ Line 341
  - [x] Task 26 DOES execute (sees `[LAMBDA ENTRY] Task 26 lambda called!`) ✅ Line 344
  - [x] Tasks 20 and 21 also execute ✅ Lines 349-366
  - [x] All tasks complete successfully ✅ Lines 367-378
- [x] Verify network execution completes without exceptions ✅ Line 367
- [x] Check that grid-1.vtk file is created (the intended output) ✅ File created!

**Expected outcome:** Original problem is fixed, all tasks execute successfully.
**Result:** ✅ ALL TESTS PASSED! Bug completely fixed!

**Evidence from output.log:**
- Line 336-338: Node 5 inputs refreshed from nodes 3 and 25
- Line 341: Task 5 completed successfully without exception
- Line 344-347: Task 26 executed (previously never ran!)
- Line 349-355: Task 20 executed with refreshed input from node 26
- Line 357-365: Task 21 executed with all inputs refreshed
- Lines 367-378: All nodes ready=true, execution completed successfully

---

## Step 6: Analyze Performance and Behavior ✅ COMPLETED
Goal: Ensure the fix doesn't cause issues.

- [x] Check output.log for input refresh patterns:
  - [x] Verify refresh only happens for nodes with inputs ✅ Confirmed
  - [x] Count how many times each node's inputs are refreshed (should be once per execution) ✅ Correct
  - [x] Ensure no redundant refreshes ✅ No redundancy
- [x] Verify taskflow dependencies still work:
  - [x] Task 5 still waits for tasks 3 and 25 to complete ✅ Correct order
  - [x] Task 20 still waits for task 26 ✅ Executed after 26
  - [x] Task 21 still waits for tasks 8, 5, and 20 ✅ Executed last
- [x] Check for any new warnings or errors ✅ None
- [x] Verify execution order is correct ✅ Perfect

**Expected outcome:** Fix works correctly with no unintended side effects.
**Result:** ✅ No side effects, all dependencies respected, correct execution order maintained.

**Execution order observed:**
- network_1: 8 → 22 → 23 → 24 → 3 → 25 → 5 → 26 → 20 → 21 ✅ Perfect!

---

## Step 7: Test with Working Cases ⏳ PENDING
Goal: Ensure the fix doesn't break scenarios that already worked.

- [ ] Identify other test files (check test_files/ directory)
- [ ] Run with test_files/anc.json (if it exists)
- [ ] Run with test_files/vtk-single.json (if it exists)
- [ ] For each test file:
  - [ ] Verify no new exceptions
  - [ ] Verify all expected tasks execute
  - [ ] Verify output files are created correctly
- [ ] If any regressions found, investigate and fix

**Expected outcome:** All previously working cases still work.
**Status:** Ready for testing with additional test files.

---

## Step 8: Optimize and Clean Up ⏳ PENDING
Goal: Clean up debug output and optimize if needed.

- [ ] Review all debug logging added during debugging investigation
- [ ] Decide which debug logs to keep:
  - [ ] Keep essential logs (e.g., "[DEBUG] Refreshing inputs for node X")
  - [ ] Remove verbose connection details if not needed
  - [ ] Consider adding a compile-time or runtime flag for detailed debugging
- [ ] Check if input refresh logic can be optimized:
  - [ ] Current O(connections) per node execution - acceptable for most cases
  - [ ] Consider caching connections per node if performance critical (probably not needed)
- [ ] Ensure code follows project style guidelines
- [ ] Add code comments explaining the dynamic resolution approach

**Expected outcome:** Clean, well-documented code.
**Status:** Debug logging currently active, to be reviewed for production.

---

## Step 9: Document the Fix ⏳ PENDING
Goal: Document the solution for future reference.

- [ ] Update findings.md with:
  - [ ] Root cause explanation
  - [ ] Solution implemented
  - [ ] Why dynamic resolution was chosen
- [ ] Update issue.md if it exists with resolution status
- [ ] Add comments in coral_network.h explaining:
  - [ ] Why input refresh is needed
  - [ ] What problem it solves
  - [ ] Reference to the original issue
- [ ] Consider creating a brief technical note explaining:
  - [ ] The static pointer problem
  - [ ] The dynamic resolution solution
  - [ ] Performance considerations

**Expected outcome:** Fix is well-documented for future maintainers.
**Status:** Ready for documentation phase.

---

## Step 10: Optional - Remove Old Connection Setup (Future Enhancement) ⏳ NOT STARTED
Goal: Consider if the setup-time set_input() call is still needed.

⚠️ **WARNING:** This is an optional, advanced step. Only do this after confirming the fix works perfectly.

- [ ] Analyze if `set_input()` in `add_connection()` (line 370-371) is still necessary
  - [ ] It was meant to establish connections early
  - [ ] Now we refresh before execution anyway
  - [ ] Keeping it might be harmless (just gets overwritten)
  - [ ] Removing it might break something subtle
- [ ] If considering removal:
  - [ ] Test thoroughly with all test cases
  - [ ] Check if any code depends on inputs being set early
  - [ ] Verify no regressions
- [ ] Document decision (keep or remove) and rationale

**Expected outcome:** Decision made on whether to keep or remove setup-time set_input().
**Status:** Deferred for future consideration.

---

## Success Criteria

The fix is complete when:
- ✅ Task 26 executes successfully
- ✅ Task 5 does not throw "Arguments not ready" exception
- ✅ All tasks in network_1 execute in correct order
- ✅ Output file (grid-1.vtk) is created
- ⏳ No regressions in other test cases (needs additional testing)
- ⏳ Code is clean and well-documented (needs cleanup phase)

## Primary Fix Status: ✅ SUCCESSFUL!

The core bug has been completely fixed. All primary success criteria met:
- Task 26 now executes (previously never ran)
- Task 5 executes without "Arguments not ready" exception
- All tasks execute in correct order
- Output file created successfully
- No new errors or warnings introduced

## Rollback Plan

If the fix causes issues:
1. Revert changes to coral_network.h
2. Git restore to previous commit
3. Re-analyze the problem with new insights
4. Consider alternative solutions (Option 2 or modified Option 1)
