# Fix Plan: Dynamic Input Resolution at Execution Time

## Solution Overview
Instead of storing static pointers to node outputs during connection setup (which may point to unready objects), we will **dynamically resolve inputs right before each node executes**. This ensures that every node always gets the latest, ready outputs from its predecessors.

## Root Cause Recap
Currently, `add_connection()` calls `set_input(target_input, source->output(source_output))` during network setup, storing a pointer that may point to an unready object. This pointer never updates, causing "Arguments not ready" exceptions even after source nodes execute successfully.

---

## Step 1: Understand and Document Current Implementation
Goal: Fully understand the current connection mechanism before modifying it.

- [ ] Read and document how `add_connection()` currently works (coral_network.h:317-392)
- [ ] Identify all places where `set_input()` is called
  - [ ] In `add_connection()` (coral_network.h:370-371)
  - [ ] In network executor for dynamic nodes (coral_network.h:1232-1238)
  - [ ] Any other locations?
- [ ] Document the `connections` map structure and what it contains
- [ ] Verify that `connections` map has all info needed (source_id, target_id, source_output, target_input)
- [ ] Confirm that `connections` map persists throughout network lifetime

**Expected outcome:** Clear understanding of where and how connections are established.

---

## Step 2: Design the Dynamic Resolution Logic
Goal: Design the input refresh mechanism that will run before each node execution.

- [ ] Determine the exact location for refresh logic: in task lambda, before `(*node)()` call
- [ ] Design the refresh loop that iterates through connections
  - [ ] Should only refresh inputs for the current node (filter by `conn.target_id == id`)
  - [ ] Should call `set_input()` for each matching connection
  - [ ] Should use fresh `output()` from source nodes
- [ ] Decide if refresh is needed in both:
  - [ ] `add_node()` lambda (coral_network.h:~245)
  - [ ] `rebuild_taskflow()` lambda (coral_network.h:~206)
- [ ] Consider edge cases:
  - [ ] What if a source node isn't ready yet? (Should not happen due to taskflow dependencies)
  - [ ] What if connections are empty? (Safe to skip)
  - [ ] What if node has no inputs? (Loop does nothing, safe)
- [ ] Design logging strategy to verify refresh happens correctly

**Expected outcome:** Clear pseudocode for the refresh logic.

---

## Step 3: Implement Input Refresh in add_node() Lambda
Goal: Add dynamic input resolution before node execution in the main execution path.

- [ ] Locate the task lambda in `add_node()` method (coral_network.h:~242-255)
- [ ] Add input refresh logic BEFORE the try block (or inside try, before `(*node)()` call)
- [ ] Implement the refresh loop:
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
- [ ] Add debug logging to track refresh:
  ```cpp
  std::cout << "[DEBUG] Refreshing inputs for node " << id << std::endl;
  ```
- [ ] Add logging for each connection refreshed:
  ```cpp
  std::cout << "[DEBUG]   Refreshed input " << conn.target_input
            << " from node " << conn.source_id << std::endl;
  ```
- [ ] Ensure `this->connections` and `this->nodes` are accessible in lambda capture
- [ ] Verify code compiles

**Expected outcome:** add_node() lambda refreshes inputs before execution.

---

## Step 4: Implement Input Refresh in rebuild_taskflow() Lambda
Goal: Ensure consistency by adding the same logic to rebuild mode.

- [ ] Locate the task lambda in `rebuild_taskflow()` method (coral_network.h:~203-220)
- [ ] Add the same input refresh logic as in Step 3
- [ ] Use consistent logging format
- [ ] Ensure code compiles

**Expected outcome:** rebuild_taskflow() lambda also refreshes inputs before execution.

---

## Step 5: Test the Fix with Original Problem Case
Goal: Verify the fix solves the original issue.

- [ ] Rebuild the project
- [ ] Run with the problematic test file: `/app/test_files/vtk-ko.json`
- [ ] Check output.log for:
  - [ ] "[DEBUG] Refreshing inputs for node 5" appears before task 5 execution
  - [ ] Task 5 does NOT throw "Arguments not ready" exception
  - [ ] Task 5 completes successfully (sees `[LAMBDA EXIT] Task 5 lambda finished!`)
  - [ ] Task 26 DOES execute (sees `[LAMBDA ENTRY] Task 26 lambda called!`)
  - [ ] Tasks 20 and 21 also execute
  - [ ] All tasks complete successfully
- [ ] Verify network execution completes without exceptions
- [ ] Check that grid-1.vtk file is created (the intended output)

**Expected outcome:** Original problem is fixed, all tasks execute successfully.

---

## Step 6: Analyze Performance and Behavior
Goal: Ensure the fix doesn't cause issues.

- [ ] Check output.log for input refresh patterns:
  - [ ] Verify refresh only happens for nodes with inputs
  - [ ] Count how many times each node's inputs are refreshed (should be once per execution)
  - [ ] Ensure no redundant refreshes
- [ ] Verify taskflow dependencies still work:
  - [ ] Task 5 still waits for tasks 3 and 25 to complete
  - [ ] Task 20 still waits for task 26
  - [ ] Task 21 still waits for tasks 8, 5, and 20
- [ ] Check for any new warnings or errors
- [ ] Verify execution order is correct

**Expected outcome:** Fix works correctly with no unintended side effects.

---

## Step 7: Test with Working Cases
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

---

## Step 8: Optimize and Clean Up
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

---

## Step 9: Document the Fix
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

---

## Step 10: Optional - Remove Old Connection Setup (Future Enhancement)
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

---

## Success Criteria

The fix is complete when:
✅ Task 26 executes successfully
✅ Task 5 does not throw "Arguments not ready" exception
✅ All tasks in network_1 execute in correct order
✅ Output file (grid-1.vtk) is created
✅ No regressions in other test cases
✅ Code is clean and well-documented

## Rollback Plan

If the fix causes issues:
1. Revert changes to coral_network.h
2. Git restore to previous commit
3. Re-analyze the problem with new insights
4. Consider alternative solutions (Option 2 or modified Option 1)
