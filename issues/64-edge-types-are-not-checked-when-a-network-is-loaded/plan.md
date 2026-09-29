# #64 — Implementation plan

Read `decisions.md` first: it holds the context and every decision (D1–D7)
this plan implements. Goals: `desiderata.md`.

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
      `TypeMismatchException("Input <i> '<name>' of '<type_name>' expects '<expected>', got '<value hash>'.")`
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
- [ ] 7.1 In `backends/dealii/tests/dealii_types.cc`: `FE_Q<2>` → `FiniteElement<2>`,
      `ofstream` → `ostream` edges accepted
- [ ] 7.2 Build and run: passes

## 8. Full suite (D6, Risk)
- [ ] 8.1 Run the whole suite (core + deal.II)
- [ ] 8.2 Any failure from a wrong edge (incl. `refresh_dynamic_inputs`):
      report to user, decide case by case

## 9. Docs (D7)
- [ ] 9.1 `README.md:60`: wording shown to user before editing
- [ ] 9.2 Check Doxygen of steps 1, 3, 4, 5 is in place
