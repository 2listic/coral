# Debugging Plan: Task 26 Not Executing

## 🎯 ROOT CAUSE SUMMARY

**Primary Issue:** Task 26 never executes despite having 0 predecessors and being correctly added to the taskflow.

**Root Cause:** Task 5 throws an exception "Arguments not ready", which likely causes the Taskflow executor to stop scheduling remaining tasks (including task 26).

**Why Task 5 Fails:**
1. During network_1 initial setup, connection 0 (node 3 → node 5) is established
2. `add_connection()` calls `set_input(0, node3->output(0))`, storing a pointer to node 3's output
3. At this time, node 3 hasn't executed yet, so its output(0) is **ready=false** (output.log:11)
4. Node 5's input(0) stores this pointer to the unready output
5. Later, node 3 executes successfully and becomes ready=true
6. **However**, node 5's input(0) still holds the old pointer to the unready object - it never updates!
7. When node 5 tries to execute, it checks argument readiness and finds argument 0 not ready
8. Exception thrown, execution halts, task 26 never scheduled

**Why Dynamic Nodes Work:**
Nodes 22-26 are created with ready=true (they represent input arguments from the outer network), so connections to them work correctly (output.log:159).

**Fix Required:**
The connection mechanism needs to resolve inputs dynamically at execution time, OR nodes need to properly update their output references when executing.

---

## Step 1: Verify Connection Establishment ✓✓
Instrument `add_connection()` to verify the 26→20 connection is being made correctly.

- [x] Add logging in `add_connection()` (coral_network.h:257) to print when connection 26→20 is created
- [x] Log the source and target task states before calling `source_task.precede(target_task)`
- [x] Verify that `node_tasks[26]` and `node_tasks[20]` both exist and are valid at connection time
- [x] Check if `source_task.precede(target_task)` returns successfully
- [x] Rebuild and run, check output

**Changes made:**
- Added debug logging at start of `add_connection()` showing connection_id, source, target, and I/O ports
- Added debug logging before `precede()` showing task empty states for source and target
- Added debug logging after `precede()` confirming connection establishment

**KEY FINDINGS:**
- Connection 26→20 IS established correctly (output.log:121-123)
- Both tasks are valid (empty=false) at connection time
- Task 26 shows correct properties: 0 predecessors, 1 successor
- Task 20 correctly shows 1 predecessor (task 26)
- **BUT**: Task 26 never executes!
- **SMOKING GUN**: Line 218 shows "#### rebuild taskflow" - Network copy is happening
- After rebuild, only original nodes (20, 21, 3, 5, 8) remain - dynamic nodes 22-26 are lost!

## Step 1b: Investigate Network Copy and Rebuild ✓✓
Added instrumentation for network copy operations and rebuild.

- [x] Add logging to Network copy constructor to show when it's called and how many nodes are copied
- [x] Add logging to Network copy assignment operator
- [x] Add logging to `rebuild_taskflow()` to show which nodes are being rebuilt
- [x] Rebuild and run

**MOST CRITICAL FINDING:**
- **The .dot file is dumped at coral_network.h:520, immediately BEFORE `executor.run(taskflow).wait()` at line 522**
- **Task 26 EXISTS in the network_1.dot file with correct graph structure**
- **The taskflow graph is provably correct before execution starts**

**Detailed findings from output.log:**
1. Task 26 properties before execution (lines 189-194):
   - predecessors: 0 (should execute immediately)
   - successors: 1 (node 20 depends on it)
   - empty: false (valid task)
2. Connection 26→20 established correctly (lines 121-123, both tasks valid)
3. Task 26 never executes - no observer message during execution (lines 196-217)
4. Other tasks with 0 predecessors DO execute: 8, 22, 23, 24, 25

**Secondary finding - rebuild is NOT the cause:**
- `rebuild_taskflow()` is called AFTER execution completes (line 218)
- Called from cleanup code (coral_network.h:1091) which removes dynamic nodes
- At rebuild time, only 5 nodes remain: 3, 5, 8, 20, 21
- This cleanup is unrelated to why task 26 doesn't execute

**CONCLUSION:**
The graph is correct. Task 26 exists with 0 predecessors before executor.run(). Yet Taskflow executor doesn't schedule it. This points to a Taskflow executor bug or a subtle scheduling issue we don't understand yet.

## Step 2: Add Detailed Taskflow Executor Debugging ✓✓
Since the graph is correct but task 26 doesn't execute, add instrumentation to understand Taskflow's scheduling behavior.

- [x] Add pre-execution state dump showing:
  - Total number of tasks in taskflow
  - Number of entries in node_tasks map
  - List of all tasks with 0 predecessors (should be ready to execute)
  - Detailed state of task 26 (empty, predecessors, successors, dependencies, name)
  - State of task 20 (the successor of task 26)
- [x] Add lambda entry/exit logging to detect if task lambda is ever called
- [x] Add post-execution marker
- [x] Rebuild and run
- [x] Analyze output to determine:
  - Is task 26 in the taskflow before execution?
  - Is task 26 recognized as having 0 predecessors?
  - Is task 26's lambda ever entered?
  - Is there any difference between task 26 and working tasks 22-25?

**Instrumentation location:**
- coral_network.h:522-558: Pre-execution debugging (after .dot dump, before executor.run())
- coral_network.h:239-250: Lambda entry/exit markers for add_node()
- coral_network.h:203-214: Lambda entry/exit markers for rebuild_taskflow()
- coral_network.h:327-332: Predecessor/successor count verification after precede()

**CRITICAL FINDINGS:**
1. **precede() works correctly** - After calling task26.precede(task20):
   - Task 26 has 1 successor (output.log:167)
   - Task 20 has 1 predecessor (output.log:168)
2. **Task 26 is in the ready list** - Line 244 shows: "8 22 23 24 25 26" all have 0 predecessors
3. **Executor selectively skips task 26** - Execution order (lines 255-289):
   - Runs: 8, 22, 23, 24 (4 of 6 zero-predecessor tasks)
   - Runs: 3 (waited for dependencies)
   - Runs: 25 (5th zero-predecessor task)
   - Runs: 5 (waited for dependencies)
   - **SKIPS task 26** (6th zero-predecessor task) ✗
   - Stops execution
4. **Task 5 exits abnormally** - Line 288 shows lambda entry but NO lambda exit (line 289 shows observer finish)
5. **Execution completes prematurely** - Three tasks never run: 26, 20, 21

**HYPOTHESIS:**
The executor is not executing all ready tasks. It runs 5 of 6 zero-predecessor tasks, then stops. This could be:
- A Taskflow executor bug with specific graph topologies
- An issue with task queue size or scheduling
- A problem with nested taskflow execution (network_1.run() is called inside network_0's task 10)

## Step 2b: Add Exception Handling to Detect Task Failures ✓✓
User observation: Task 5's lambda shows entry but no exit marker - likely throwing an exception.

- [x] Add try-catch around `(*node)()` in add_node() lambda
- [x] Add try-catch around `(*node)()` in rebuild_taskflow() lambda
- [x] Add try-catch around `executor.run(taskflow).wait()`
- [x] Log exception details when caught
- [x] Rebuild and run
- [x] Check if task 5 (or any other task) throws an exception

**Goal**: Determine if an exception in task 5 is causing the executor to stop before scheduling task 26.

**Instrumentation:**
- coral_network.h:242-254: Try-catch in add_node() lambda
- coral_network.h:206-220: Try-catch in rebuild_taskflow() lambda
- coral_network.h:569-581: Try-catch around executor.run().wait()

**EXCEPTION FOUND (output.log:289):**
```
[EXCEPTION] Task 5 threw exception: Arguments are not ready. You can only call this function after all arguments are ready.
```

**Analysis:**
1. **Task 5 throws exception** - Says "Arguments are not ready"
   - Task 5 is `refine_global` method with 2 predecessors: task 3 and task 25
   - Both predecessors executed successfully (lines 275-285)
   - Yet task 5 claims arguments not ready - possible bug in argument checking
2. **Exception is caught and logged** - Doesn't crash the executor
3. **Executor completes "successfully"** - Line 291 shows success despite exception
4. **Task 26 STILL never runs** - This is the key mystery!
   - Task 26 has 0 predecessors (should run immediately)
   - 5 of 6 zero-predecessor tasks run (8, 22, 23, 24, 25)
   - Task 26 is the 6th task with 0 predecessors - never scheduled
   - Post-execution shows Node 26 ready: true (line 302)

**CRITICAL INSIGHT:**
The exception in task 5 is a **symptom**, not the root cause. The real problem is that **Taskflow executor doesn't schedule all ready tasks**. Task 26 should have been scheduled alongside tasks 8, 22, 23, 24, 25 (all have 0 predecessors), but the executor only ran 5 of them and skipped task 26.

**Two separate issues:**
1. Why does task 5 throw "Arguments are not ready" when its predecessors executed?
2. Why does Taskflow skip task 26 when it has 0 predecessors?

## Step 2c: Identify Which Specific Arguments Are Not Ready ✓✓
User suggestion: Instead of throwing at the first non-ready argument, collect ALL non-ready arguments and report which source nodes should provide them.

- [x] Modify coral.h operator()() to collect all non-ready argument indices
- [x] Build detailed error message listing all non-ready argument indices
- [x] Enhance catch block in coral_network.h to analyze connections
- [x] For each connection targeting the failing node, log source node ID, name, and ready status
- [x] Rebuild and run
- [x] Identify which specific source node(s) are not providing ready outputs to task 5

**Goal**: Determine exactly which predecessor node(s) are failing to provide ready outputs to task 5.

**Changes made:**
- coral.h:538-564: Modified operator()() to collect all not-ready argument indices before throwing
- coral_network.h:245-267: Enhanced exception handler to look up and log all connections targeting the failing node

**KEY FINDINGS (output.log:289-292):**
- **Argument 0** is not ready for task 5
- Connection 0: Node 3 → Node 5 input 0, **source ready=true**
- Connection 7: Node 25 → Node 5 input 1, **source ready=true**

**CRITICAL INSIGHT:**
Node 3 executed successfully (ready=true), but its output is NOT propagating to node 5's input 0. The problem is **data flow**, not taskflow graph structure. The connection exists and node 3 is ready, but node 5 still can't access the triangulation from node 3's output.

## Step 2d: Check Output Ready Status at Connection Time ✓✓ ROOT CAUSE FOUND
Investigate when set_input() is called and whether outputs are ready at that time.

- [x] Add logging before set_input() to show source output ready status
- [x] Add logging after set_input() to show target input ready status
- [x] Rebuild and run
- [x] Determine if outputs are ready when connections are made
- [x] Check if there's a timing issue where outputs aren't initialized at connection time

**Hypothesis CONFIRMED!**

**Changes made:**
- coral_network.h:370-383: Log source output ready status before set_input, and target input ready status after set_input

**ROOT CAUSE IDENTIFIED (output.log:11, 159):**

Connection 0 (3→5) at initial setup:
```
source_node[3]->output(0) ready=0 -> target_node[5]->input(0)
After set_input: target_node[5]->input(0) ready=0
```

Connection 4 (22→3) during dynamic addition:
```
source_node[22]->output(0) ready=true -> target_node[3]->input(0)
After set_input: target_node[3]->input(0) ready=true
```

**THE BUG:**
1. During network setup, `add_connection()` calls `set_input(target_input, source->output(source_output))`
2. This stores a **pointer** to the source node's output object in the target node's input
3. If the source node hasn't executed yet, this pointer points to an **unready** object
4. Later, when the source node executes, it updates its internal state to "ready"
5. **BUT** the target node's input still holds the **old pointer** to the unready object
6. The pointer never gets updated, so the target node can never access the source's output

**Why dynamic nodes (22-26) work:**
- They represent input arguments from the outer network
- Created with values already set, so outputs are immediately ready=true
- Connections made to them (e.g., 22→3) work correctly

**Why original nodes fail:**
- Node 3 hasn't executed when connection 3→5 is made (during initial setup)
- Node 5's input stores pointer to unready output
- Even after node 3 executes successfully, node 5's input pointer doesn't update
- Node 5 throws "Arguments not ready" exception
- Exception likely causes Taskflow executor to stop scheduling remaining tasks
- **This is why task 26 never runs!**

**SOLUTION NEEDED:**
The connection mechanism needs to resolve inputs dynamically at execution time, OR nodes need to update their output pointers when they execute, OR connections need to be re-established after nodes become ready.

## Step 3: Check Task Addition Order vs Connection Order
Verify that nodes 22-26 are fully added before their connections are established.

- [ ] Add logging at the start of `add_node()` (coral_network.h:207) to print timestamp/sequence
- [ ] Add logging at the start of `add_connection()` (coral_network.h:257) to print timestamp/sequence
- [ ] Run and verify that all nodes 22-26 are added before any connections involving them
- [ ] Check if there's any interleaving that could cause issues

## Step 3: Isolate Node 26 vs Other Dynamic Nodes
Compare how node 26 is added/connected vs nodes 22-25.

- [ ] Add detailed logging showing the node_id and target_id for each connection in network_1
- [ ] Verify node 26's connection target (20) vs other nodes' targets (3, 5, 21)
- [ ] Check if node 20 has any special properties (it's a constructor node: std::ofstream)
- [ ] Run and compare the connection patterns

## Step 4: Verify Taskflow Graph Topology
After all nodes and connections are added but before execution, query Taskflow for reachable tasks.

- [ ] Before `executor.run()` (coral_network.h:486), iterate through `node_tasks` and print each task's status
- [ ] For each task, check if it's reachable from any source node in the graph
- [ ] Verify that task 26 is in the executor's work queue
- [ ] Check if there are any isolated subgraphs

## Step 5: Test Minimal Reproduction
Create a minimal test case to isolate the problem.

- [ ] Create a simple test: add 2 nodes dynamically (A, B) to an existing taskflow with 1 node (C)
- [ ] Connect A→C (like 22-25 do to their targets)
- [ ] Connect B→C (like 26 does to 20)
- [ ] Check if B executes or not
- [ ] If B doesn't execute, we have a simpler reproduction case

## Step 6: Investigate Node 20 Specifically
Check if there's something special about node 20 that prevents dynamic nodes from connecting to it.

- [ ] Add logging when node 20 is created (should be in original network creation)
- [ ] Check node 20's task handle state when nodes 22-26 are added
- [ ] Verify that node 20's task is not in some "finalized" state that rejects new predecessors
- [ ] Compare node 20's state vs nodes 3, 5, 21 (which successfully receive connections from dynamic nodes)

## Step 7: Check Taskflow Version and Documentation
Verify if this is a known Taskflow limitation or bug.

- [ ] Check which Taskflow version is being used
- [ ] Search Taskflow documentation for dynamic task addition restrictions
- [ ] Search Taskflow issues/discussions for similar problems
- [ ] Check if `precede()` has any documented limitations when called after graph construction

## Step 8: Add Explicit Error Checking
Add error handling to catch any silent failures.

- [ ] Wrap `source_task.precede(target_task)` in try-catch
- [ ] Check return values (if any) from Taskflow operations
- [ ] Add assertions to verify task states before critical operations
- [ ] Look for any Taskflow error reporting mechanisms

## Step 9: Compare with Working Case
Compare network_1 behavior with network_0 to find differences.

- [ ] Check if network_0 has any similar dynamic node additions
- [ ] Compare the network construction code paths for both networks
- [ ] Identify any differences in how connections are established
- [ ] Check if single-threaded execution (THREADS=1) is relevant

## Step 10: Nuclear Option - Rebuild Taskflow After Dynamic Additions
If all else fails, test if rebuilding the taskflow helps.

- [ ] After adding nodes 22-26 and their connections, call `rebuild_taskflow()` before execution
- [ ] Check if this forces Taskflow to re-analyze the graph
- [ ] Test if task 26 now executes
- [ ] If yes, investigate why rebuild is needed
