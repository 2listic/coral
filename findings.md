# Investigation Findings: Node 26 Not Executing

## ✅ ROOT CAUSE IDENTIFIED

Task 26 never executes because Task 5 throws an exception, causing the Taskflow executor to halt before scheduling remaining tasks.

**The Bug:** Connection pointers are established during network setup before nodes execute. When `add_connection(3→5)` is called, node 3 hasn't executed yet, so `set_input()` stores a pointer to node 3's **unready** output. Even after node 3 executes successfully, node 5's input still points to the old unready object. When node 5 tries to execute, it finds argument 0 not ready and throws an exception.

**Evidence:** output.log:11 shows connection 3→5 established with `source_node[3]->output(0) ready=0`. Later at output.log:289, task 5 throws "Arguments not ready... Not ready argument indices: 0" even though node 3 executed successfully.

---

## Problem Statement (ORIGINAL)
Task 26 ("file_name") in network_1 is not executed by Taskflow, despite having correct configuration.

## Confirmed Facts (from output.log and .dot files)

### Task 26 Configuration (Lines 147-152)
- **Exists**: Yes
- **Valid handle**: `empty: false`
- **Predecessors**: 0 (should execute immediately)
- **Successors**: 1 (task 20 depends on it)
- **Connection**: 26 → 20 (confirmed in network_1.dot, line 21-22)

### Graph Structure
- All tasks are valid (empty: false)
- Graph structure is correct before execution (verified via .dot dump)
- Task 20 has 1 predecessor (line 105-110)
- Connection 26→20 exists in the taskflow

### Execution Behavior
**Tasks with 0 predecessors that DO execute:**
- Task 8 ✓
- Task 22 ✓
- Task 23 ✓
- Task 24 ✓
- Task 25 ✓

**Tasks with 0 predecessors that DO NOT execute:**
- Task 26 ✗ (no observer message between lines 154-175)

**Cascade effect:**
- Task 20 doesn't execute (depends on 26)
- Task 21 doesn't execute (depends on 20)

### Timeline
1. Lines 77-81: Nodes 22-26 added dynamically during execution of node 10
2. Line 82: Executor created
3. Lines 84-152: .dot dump shows correct structure
4. Lines 154-175: Execution - task 26 never runs

## Key Observations
1. Task 26 is added the **same way** as tasks 22-25 (which execute successfully)
2. Task 26 is the only dynamically-added task connecting to node 20 (which was created before dynamic nodes)
3. Graph is correct, but executor doesn't schedule task 26
4. No errors or exceptions thrown

## Mystery
Why does Taskflow executor skip task 26 when it:
- Has a valid task handle
- Has zero dependencies
- Is part of a correctly constructed task graph
