# #64 — Implementation plan

Read `decisions.md` first: it holds the context and every decision (D1–D8)
this plan implements. Goals: `desiderata.md`.
Line numbers refer to the base commit `d2b89a5`; they shift as steps land.

Build/test only inside the `coral` container (repo mounted at `/app`):
`docker exec coral bash -lc 'cmake --build /app/build -j4 && ctest --test-dir /app/build --output-on-failure'`

## 1. `TypeMismatchException` (D5)
- [x] 1.1 New class in `core/include/coral.h` (`bind_input` throws it;
      `coral_network.h` includes `coral.h`), `: public std::runtime_error`,
      `explicit` ctor taking the message, style of `DuplicateQualifiedIdException`
- [x] 1.2 Doxygen on the class (D7)

## 2. `NodeObject::is_compatible_with` (D1, D2)
- [x] 2.1 Public `const` member, declared in `coral.h` after `hash()`,
      implemented in `coral_implementation.h`:
      `bool is_compatible_with(const std::string &type) const`
- [x] 2.2 `true` iff `hash() == type` or `initializer.ancestor_casters` has `type`

## 3. Check in `bind_input` (D1, D4)
- [x] 3.1 In `NodeObject::bind_input` (`coral_implementation.h:498`), after the
      existing null/index checks and before binding: if
      `!value->is_compatible_with(arguments-entry "type")` → throw
      `TypeMismatchException("Input <i> '<name>' of '<hash>' expects '<expected>', got '<value hash>'.")`
      (`hash()`, not `type_name()`: for function nodes the latter is the
      `std::function<…>` signature, not the registry/JSON name — audit fix)
- [x] 3.2 Doxygen on `bind_input`: throws `TypeMismatchException` if… (D7)

## 4. `bind_inputs` delegates (D3)
- [x] 4.1 `NodeObject::bind_inputs` (`coral_implementation.h:312`): keep the
      count check; loop `bind_input(i, inputs[i].first->get_output(inputs[i].second))`
- [x] 4.2 Delete its old type check (the `"base"` read and its messages)
- [x] 4.3 Doxygen on `bind_inputs`: throws `TypeMismatchException` if… (D7)

## 5. Edge context in `add_connection` (D4, D5)
- [x] 5.1 In `Network::add_connection(id, conn)` (`coral_network_implementation.h:411`),
      wrap `bind_input` in `try`; `catch (const TypeMismatchException &e)` →
      rethrow `TypeMismatchException("Edge <id> (<src qid>[<out>] -> <tgt qid>[<in>]): " + e.what())`,
      qids via `get_node_qualified_id`
- [x] 5.2 Move `connections[id] = conn` after a successful `bind_input` (D8)
- [x] 5.3 Doxygen on `add_connection`: throws `TypeMismatchException` if… (D7)

## 6. Core tests (D6.1–D6.12)
- [x] 6.1 New `core/tests/edge_types.cc` (picked up by the glob in
      `core/tests/CMakeLists.txt`), local test types as in `inheritance.cc`
- [x] 6.2 Cases 1–4 (`add_connection`: exact, 2-level chain + run, unrelated, base → derived)
- [x] 6.3 Case 5 (JSON load, bad edge; message has edge id and `qualified_id`s)
- [x] 6.4 Cases 6–7 (`bind_inputs`: derived → base, unrelated)
- [x] 6.5 Case 8 (`register_base` after node creation: load and `get_shared` both refuse)
- [x] 6.6 Cases 9–12 (multiple bases, chain derived-first, siblings, virtual diamond + run)
- [x] 6.7 Build and run core tests: all pass

## 7. deal.II test (D6.13)
- [x] 7.1 In `backends/dealii/tests/dealii_types.cc`: `FE_Q<2>` → `FiniteElement<2>`,
      `ofstream` → `ostream` edges accepted
- [x] 7.2 Build and run: passes

## 8. Full suite (D6, Risk)
- [x] 8.1 Run the whole suite (core + deal.II): 111/111 pass (2 MPI skipped)
- [x] 8.2 Any failure from a wrong edge (incl. `refresh_dynamic_inputs`): none;
      report to user, decide case by case

## 9. Docs (D7)
- [x] 9.1 `README.md:60`: wording shown to user before editing
- [x] 9.2 Check Doxygen of steps 1, 3, 4, 5 is in place

---

# Revision 2 — migration to whole-graph validation

Read `decisions.md` first ("Why revision 2", "Principle (R2)"): this part
implements D1, D3–D5, D8–D12 and the R2 test and docs lists. Steps 1–9 above are
revision 1 (done); part of their code changes here.

Starting state: branch `64-edge-types-are-not-checked-when-a-network-is-loaded`
(PR #67), HEAD `0ec05c5`, a merge of branch `63-…` (PR #65), which merged
`main` (including #69). The 4 `PassThrough*` tests in `core/tests/edge_types.cc`
fail; everything else passes. Line numbers below refer to `0ec05c5`.

Build and test in the `coral` container, in a fresh build directory:
`docker exec coral bash -lc 'cd /app && cmake -S . -B /tmp/b64 -DCMAKE_BUILD_TYPE=Debug && cmake --build /tmp/b64 -j16 && cd /tmp/b64 && ctest --output-on-failure'`
Run `ctest` serially: with `-j16`, two VTK tests (`vtk-gen2`, `graph-no-name`)
failed once and passed when run serially.

As `desiderata.md` asks: one step at a time; show every code or doc change to the
user before editing.

## 10. `Network::get_input_connection` (D11)
- [x] 10.1 Declare in `core/include/coral_network.h` after
      `get_node_connections` (`:223`):
      `auto get_input_connection(unsigned int node_id, unsigned int input) const -> std::optional<Connection>;`
      add `#include <optional>`
- [x] 10.2 Implement in `coral_network_implementation.h` after
      `get_node_connections` (`:765`): last entry (highest id) of `connections`
      with `target_id == node_id && target_input == input`, else `std::nullopt`
- [x] 10.3 Doxygen (D7)

## 11. `add_connection` records only (Principle, D8)
- [x] 11.1 In `Network::add_connection(id, conn)`
      (`coral_network_implementation.h:357`): remove the `bind_input` call and
      its `TypeMismatchException` catch-and-prefix (`:409-424`)
- [x] 11.2 After the missing-node checks (`:368-378`) add the structural checks
      of D8: `source_output < n_outputs()`, `target_input < n_inputs()`, target
      input not `self` (`input_indices[target_input] != -1`; private, `Network`
      is a friend); throw `std::runtime_error` naming the edge id; edge not stored
- [x] 11.3 Store `connections[id] = conn` after the checks; keep auto-naming
      (`:380-407`) and task precedence (`:428-443`)
- [x] 11.4 Doxygen on `add_connection` (`coral_network.h:154-161`) (D7)

## 12. `bind_input` keeps its check (D3)
- [x] 12.1 No code change: the type check (`coral_implementation.h:479-485`)
      and the Doxygen of `bind_input` (`coral.h:1170-1176`) and `bind_inputs`
      (`coral.h:1378-1388`) stay as in revision 1

## 13. `Network::validate()` (D1, D4, D5, D9, D10)
- [x] 13.1 Declare public `void validate() const;` in `coral_network.h`, next
      to `run()` (`:202`)
- [x] 13.2 Kahn order over `nodes` / `connections`, iterative, ready set ordered
      by node id; nodes left over → `std::runtime_error("Cycle in network: nodes <ids>.")` (D10)
- [x] 13.3 Type pass with a local map, as in D9 (uses `get_input_connection`)
- [x] 13.4 One D4 line per bad edge; if any, throw one `TypeMismatchException`
      with header `Type mismatch in <N> edge(s):`
- [x] 13.5 Doxygen on `validate()` and on `TypeMismatchException`
      (`coral.h:48-55`) (D7)

## 14. Call `validate()` (D1)
- [x] 14.1 At the end of `Network::from_json` (`coral_network_implementation.h:554`),
      after the edges loop; on failure `slog_error` and rethrow, like the edges
      loop (`:656`)
- [x] 14.2 At the start of `Network::run()` (`:672`), before `executor.run`

## 15. Tests (decisions.md, "Tests (R2)")
- [x] 15.1 Build and run: failures expected exactly in the "To adapt" list;
      report anything else to the user
- [x] 15.2 Adapt `UnrelatedThrows`, `BaseToDerivedThrows`,
      `RegisterBaseAfterNodeCreation`, `SiblingThrows` (exact changes agreed
      with the user)
- [x] 15.3 Adapt `Network.BareMinimal` (`backends/dealii/tests/network.cc:89-90`)
- [x] 15.4 New tests a–e in `core/tests/edge_types.cc`
- [x] 15.5 Whole suite (core + deal.II), serial: all pass (2 MPI skipped),
      including the 4 `PassThrough*` tests

## 16. Docs (decisions.md, "Docs (R2)")
- [x] 16.1 `README.md:61-64`: wording shown to the user before editing
- [x] 16.2 Check that the Doxygen of steps 10–14 is in place and the old wording
      on edges ("type-checked when it is made", "done when the edge is bound",
      `add_connection` throwing `TypeMismatchException`) is gone:
      `grep -n TypeMismatchException core/include README.md`

## 17. Audit (desiderata, phase 4)
- [ ] 17.1 Check code, tests and docs for consistency against `decisions.md`;
      report to the user
