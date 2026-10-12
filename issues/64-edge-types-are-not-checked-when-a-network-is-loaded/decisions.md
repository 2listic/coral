# #64 — Decisions

Context for implementing GitHub issue #64 (2listic/coral). Agreed in discussion;
see `desiderata.md` for the goals. Plan: `plan.md`. Builds on #63 (implemented).

This is **revision 2**. Revision 1 (plan steps 1–9) is implemented; revision 2
(plan steps 10–17) migrates it to the design below. Decisions marked *(R2)* are
new or changed in revision 2.

## Why revision 2
- Revision 1 checked each edge in `NodeObject::bind_input`, i.e. when
  `Network::add_connection` bound it.
- Merging `main` brought #68 (PR #69): pass-through regression tests in
  `core/tests/edge_types.cc` (namespace `edge_types::passthrough`). Four fail:
  `PassThroughDownstreamEdgeFirst`, `PassThroughJsonLoadEdgeKeyOrder`,
  `PassThroughChainDownstreamEdgesFirst`, `PassThroughChainJsonLoadEdgeKeyOrder`,
  all with `Input 0 'obj' of 'edge_types_pt_consume' expects '…::Derived', got '…::Base'.`
- Cause: a pass-through output (argument taken by non-const reference, e.g.
  `setup(Base &)`) returns whatever its input is bound to
  (`NodeObject::get_output`, `coral_implementation.h:327`); while unbound it
  holds a placeholder of its declared type (`:257`). Binding the downstream edge
  first sees `Base`, so `Base` → `const Derived &` is refused although the
  complete graph is valid. Moreover `bind_input` copies pointers: a later
  upstream binding does not update the downstream copies, which stay stale until
  `refresh_dynamic_inputs` (`coral_network_implementation.h:142`) rebinds them
  at run time.
- **Requirement**: every graph the frontend (`2listic/dealiiX-platform`)
  accepts must be accepted by the backend. The frontend validates the complete
  graph and follows pass-through edges whatever the edge order
  (`validateGraphData` → `protocolOutputTypeCandidates`,
  `src/lib/utils/graphParser.ts`, dealiiX-platform PR #227). A frontend JSON can
  list the downstream edge first (edge keys follow creation order, e.g. an
  upstream edge deleted and redrawn).
- Rejected in discussion (do not re-propose): re-check downstream edges when an
  upstream edge is bound; defer only pass-through edges ("pending"); rebind all
  inputs inside `validate()` (non-`const`); forward propagation in
  `add_connection`; any recursion or per-edge upstream walk, also written as a
  loop (the user does not want recursion); removing the check from
  `bind_input` (see D3).

## Principle (R2)
Before `run()`, the edge set (`Network::connections`) is the only truth.
`add_connection` records an edge and binds nothing. Types are a property of the
complete graph, checked by `Network::validate()`. Inputs are bound only in
`run()`, by `refresh_dynamic_inputs`, right before each node executes (already
the case today).

```cpp
Network net;
net.add_connection(s, c, 0, 0);    // never a type error; c's input stays unbound
net.add_connection(d, s, 0, 0);
net.get_input_connection(c, 0);    // the recorded edge s[0] -> c[0]
net.get_node(c)->get_input(0);     // nullptr: unbound until run()
net.validate();                    // optional; throws listing all bad edges
net.run();                         // validates first, then binds and runs

net.from_json(j);                  // validates the complete graph at the end
```

## Problem (verified, revision 1)
- `NodeObject::bind_input` (`core/include/coral_implementation.h:498`, at the base commit) checks the
  index only. Every network path binds through it: `Network::add_connection`
  (`core/include/coral_network_implementation.h:411`), hence JSON load (`:638`),
  `refresh_dynamic_inputs` (`:150`), sub-network wiring (`:1363`). A wrongly
  typed edge fails only when the target node runs (`get_shared`).
- `NodeObject::bind_inputs` (`coral_implementation.h:312`) has its own check,
  reading `"base"` from the target's argument entry. #63 no longer writes
  `"base"`, so it accepts exact matches only (refuses `FE_Q` → `FiniteElement`).

## Rule
An edge is valid iff `source.type == expected` or `expected` is an ancestor of
`source` (a key of its `ancestor_casters`, mirrored in JSON as `"bases"`).

## Decisions
1. *(R2)* **Check in `Network::validate() const`**, public. Called
   automatically at the end of `from_json` (after all edges are recorded) and at
   the start of `run()` (before the executor). The user may call it. No node
   runs before it.
2. **Ancestors read from the source value's own entry copy**
   (`initializer.ancestor_casters`) via `NodeObject::is_compatible_with`
   (`coral_implementation.h:544`), the same table `get_shared` casts with
   (`coral.h:1683`). Load check and run-time cast agree; a `register_base`
   after node creation is invisible to both. Not `get_info()`: it stringifies a
   built value into `j["value"]` (cost + side effect).
3. *(R2)* **`bind_input` keeps its type check** (revision 1,
   `coral_implementation.h:479-485`); `bind_inputs` keeps its count check and
   calls `bind_input` per input. The network no longer calls it when an edge is
   added (`add_connection` binds nothing); it is called by
   `refresh_dynamic_inputs` in `run()` with the real objects, so a graph
   accepted by `validate()` always passes, and by `connect()` / `bind_inputs`.
   `connect()` is order-dependent by nature: it copies the object currently at
   the source output and has no `refresh_dynamic_inputs`. Connecting a
   pass-through downstream first throws `TypeMismatchException`; without the
   check it would bind a placeholder that is never built and fail at run time
   ("Arguments are not ready"), as on `main`.
4. *(R2)* **Message**, built by `validate()`: one line per bad edge, ordered by
   target node in topological order (D9), then by edge id:
   ```
   Type mismatch in 2 edge(s):
   Edge 3 (mesh[0] -> dofh[1]): Input 1 'fe' of 'dealii::DoFHandler<2, 2>' expects 'dealii::FiniteElement<2, 2>', got 'dealii::Vector<double>'.
   Edge 9 (src[0] -> dst[0]): Input 0 'obj' of 'consume' expects 'Derived', got 'Base'.
   ```
   Nodes by `get_node_qualified_id`; target by `hash()` (not `type_name()`:
   for function nodes that is the `std::function<…>` signature); input name from
   the target's `arguments` entry `"name"`; "got" is the `hash()` of the
   resolved source (D9). `add_connection`'s catch-and-prefix
   (`coral_network_implementation.h:409-424`) is removed.
5. *(R2)* **Exception**: `coral::TypeMismatchException` (`coral.h:56`), kept.
   Thrown once by `validate()`, listing **all** bad edges. The message is the
   only payload (no list of edge ids).
6. *(R2)* **Tests**: see "Tests (R2)".
7. *(R2)* **Docs**: see "Docs (R2)".
8. *(R2)* **Edge storage**: `add_connection` throws and does not store the edge
   only on structural errors: missing source or target node (as today),
   `source_output >= n_outputs()`, `target_input >= n_inputs()`, target input
   is `self`. The last three were thrown by `get_output` / `bind_input` before;
   they become explicit checks in `add_connection`. A wrongly typed edge is
   stored, and `validate()` refuses the graph. After a failing `from_json` the
   network holds all nodes and edges of the JSON (no rollback).
9. *(R2)* **Type resolution: one iterative pass in topological order (Kahn), no
   recursion.**
   ```
   order = Kahn(nodes, connections)       // ready set ordered by node id: deterministic
   type  = local map (node, output) -> NodeObjectPtr
   for node in order:
     for each output o of node:
       if o is pass-through and an edge e feeds its input:
         type[node, o] = type[e.source_id, e.source_output]   // set already: upstream first
       else:
         type[node, o] = node->get_output(o)   // SELF: the node; plain output or unfed pass-through: placeholder of declared type
     for each edge e entering node, by edge id:
       if !type[e.source_id, e.source_output]->is_compatible_with(expected type of e.target_input):
         record the D4 line
   if any line recorded: throw TypeMismatchException
   ```
   The pass-through helpers are private (`is_passthrough_output`,
   `input_index_for_argument`, `coral.h:1271-1282`); `Network` is a friend of
   `NodeObject` (`coral.h:520`). Expected type: the target's
   `arguments[input_indices[i]]["type"]`, as `bind_input` reads it today
   (`coral_implementation.h:479-480`). Checking only the resolved object is
   equivalent to the frontend's "any candidate matches": ancestors are
   transitive (#63) and the upstream edge is checked on its own.
10. *(R2)* **Cycles**: nodes left over by Kahn →
    `std::runtime_error("Cycle in network: nodes <qids>.")`, not a
    `TypeMismatchException`; no type check is done. Nodes are named by
    `get_node_qualified_id`, as in D4, and are listed by node id. The list
    includes the nodes downstream of the cycle.
11. *(R2)* **`Network::get_input_connection(node_id, input) const -> std::optional<Connection>`**,
    public: the recorded edge feeding `(node_id, input)`, read from
    `connections` (like `get_inputs`, `coral_network_implementation.h:784`);
    if several, the one with the highest id (the one `refresh_dynamic_inputs`
    binds last, so `validate()` and `run()` agree); `std::nullopt` if none.
    Used by D9.
12. *(R2)* **Bindings before `run()`**: an input fed by an edge is unbound:
    `get_input(i)` is `nullptr` for a plain input, the node's own placeholder for
    a pass-through. After `run()` the bindings are those of the last run.

## Tests (R2)
Revision 1 cases (D6.1–D6.13 of revision 1: `core/tests/edge_types.cc` tests up
to `VirtualDiamondRuns`, deal.II case in `backends/dealii/tests/dealii_types.cc`)
remain, adapted as below.

Existing:
1. The 4 `PassThrough*` tests (#68): unchanged, must pass.
2. To adapt. The exact changes are decided with the user at implementation; these
   are starting drafts:
   - `UnrelatedThrows` (`edge_types.cc:111`): throw at `run()`; `src` still not
     ready; `n_connections() == 1` (D8).
   - `BaseToDerivedThrows` (`:137`), `RegisterBaseAfterNodeCreation` (`:241`),
     `SiblingThrows` (`:327`): `add_connection` does not throw; `validate()`
     throws `TypeMismatchException`.
   - `Network.BareMinimal` (`backends/dealii/tests/network.cc:89-90`) asserts
     `get_input(i)` before `run()`: move after `run()` or use
     `get_input_connection`.
3. `JsonLoadThrowsWithEdgeContext` (`:160`) must still pass (D4 keeps
   `Edge 7 (src[0] -> dst[0])`).

New, in `core/tests/edge_types.cc`:
- a. Two bad edges in one JSON → one `TypeMismatchException`; message has both
  `Edge …` lines.
- b. Cycle `x → y → x` → `std::runtime_error` mentioning the cycle.
- c. `get_input_connection`: fed input → its `Connection`; unfed → `std::nullopt`.
- d. `connect()`, pass-through, downstream first → `TypeMismatchException`
  (D3: order-dependent by nature).
- e. Input fed by an edge, before `run()` → `get_input(i) == nullptr` (D12).

The whole suite (core + deal.II) must pass.

## Docs (R2)
| Where | Will say |
|---|---|
| `README.md:61-64` | complete graph checked by `Network::validate()`, called by `from_json` and `run()`, before any node runs; all bad edges reported; `connect()`/`bind_inputs` checked when bound (order-dependent, D3) |
| `coral.h:48-55` (`TypeMismatchException`) | thrown by `validate()` (listing every bad edge) and by `bind_input` |
| `coral_network.h:154-161` (`add_connection`) | records the edge; throws only on structural errors (D8) |
| new | Doxygen for `validate()` and `get_input_connection()` |

Wording is shown to the user before editing.

## Out of scope
- Frontend (`dealiiX-platform`).
- Registry format: #63 writes `"bases": [...]` where `main` wrote `"base"`; the
  frontend (PR #227) reads only `base` (`output_type ?? base ?? type`).
  Separate frontend/backend issue.
- Two edges feeding the same input: not checked (the frontend prevents it); the
  last bound wins, as today.
- `get_shared` throwing `TypeMismatchException` (it throws `std::runtime_error`).
- A public getter for the resolved pass-through source.
- Making `connect()` order-independent (inputs referring to ports instead of
  objects).
- Rollback of `from_json` on failure.
- `set_arguments` (`coral_implementation.h:206`) assigns arguments without any
  check. Not an edge path (unused by the network); left as is.

## Risk
- `get_taskflow()` (`coral_network.h:214`) lets a caller run the taskflow
  without `run()`, hence without `validate()`. Only
  `backends/dealii/tests/network.cc:273` uses it, to count tasks.
- A sub-network validated on its own, at its own `from_json` (network node
  `parse_string`, `coral_network_implementation.h:1245`), sees its interface
  pass-through inputs unfed, i.e. with declared types: stricter than when the
  outer network wires the real arguments and calls the inner `run()`
  (`:1378-1410`). Whether the frontend validates subgraphs the same way is not
  verified; if it accepts a subgraph that this refuses, revisit.

## Audit fixes (revision 1)
- Message names the target by `hash()` (registry/JSON name), not `type_name()`.
- `from_json` logs a failing edge (`slog_error`) and rethrows, like nodes.

## Implementation notes (R2)
- `Network::describe_edge(id, conn)` (private) builds `Edge <id> (<src qid>[<out>] -> <tgt qid>[<in>])`;
  used by `add_connection` (D8 errors) and `validate()` (D4 lines).
- `NodeObject::input_type_mismatch(index, value)` (private) holds the Rule and
  the D4 text `Input <i> '<name>' of '<hash>' expects '<expected>', got '<hash>'.`;
  used by `bind_input` and `validate()`. Reads the JSON only (`.at`), never writes it.
- `validate()` builds the per-node in/out edge lists once: O(N + E).
- Tests beyond the R2 list: (a) no rollback after a failing `from_json` (D8);
  (c) highest id wins, not last added (D11); (d) upstream first is accepted
  (D3); (e) `run()` binds the input (D12).
