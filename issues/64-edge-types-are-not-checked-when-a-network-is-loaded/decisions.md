# #64 — Decisions

Context for implementing GitHub issue #64 (2listic/coral). Agreed in discussion;
see `desiderata.md` for the goals. Plan: `plan.md`. Builds on #63 (implemented).

## Problem (verified)
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
1. **Check in `bind_input`**, via a public member
   `value->is_compatible_with(expected_type)`. Fail fast: the first bad edge
   throws, before any node runs.
2. **Ancestors read from the source value's own entry copy**
   (`initializer.ancestor_casters`), the same table `get_shared` casts with
   (`coral.h:1656`). Load check and run-time cast agree; a `register_base`
   after node creation is invisible to both. Not `get_info()`: it stringifies
   a built value into `j["value"]` (cost + side effect).
3. **`bind_inputs`** keeps only its count check, then calls `bind_input` per
   input. Its own type check is deleted.
4. **Message**: `bind_input` gives the type information; `Network::add_connection`
   catches and prefixes the edge context (nodes by `qualified_id`):
   ```
   Edge 3 (mesh[0] -> dofh[1]): Input 1 'fe' of 'dealii::DoFHandler<2, 2>' expects 'dealii::FiniteElement<2, 2>', got 'dealii::Vector<double>'.
   ```
5. **Exception**: new `coral::TypeMismatchException : std::runtime_error`,
   same style as `DuplicateQualifiedIdException` (`coral_network.h:21`).
   The prefixed rethrow keeps this type.
6. **Tests**: new `core/tests/edge_types.cc` (core types only), plus one deal.II
   case in `backends/dealii/tests/`:
   1. `add_connection`, exact type → accepted.
   2. `add_connection`, `C : B : A`, `C` → `A` → accepted, run succeeds.
   3. `add_connection`, unrelated → `TypeMismatchException`, nothing run.
   4. base → derived → `TypeMismatchException`.
   5. JSON load, bad edge → `TypeMismatchException`; message has edge id and
      `qualified_id`s.
   6. `bind_inputs`, derived → base → accepted.
   7. `bind_inputs`, unrelated → `TypeMismatchException`.
   8. `register_base` after node creation → refused at load; `get_shared`
      also refuses.
   9. Multiple bases `A : B1, B2`: `A` → `B1`, `A` → `B2` → both accepted.
   10. Chain registered derived-first; `C` → `A` → accepted.
   11. Siblings `T1, T2 : B`: `T1` → `T2` → `TypeMismatchException`.
   12. Virtual diamond `D : B1, B2 : A`; `D` → `A` → accepted, run succeeds.
   13. deal.II: `FE_Q<2>` → `FiniteElement<2>`, `ofstream` → `ostream` → accepted.

   The whole existing suite (core + deal.II) must pass; an existing network
   with a wrong edge is decided case by case.
7. **Docs**: `README.md:60` says when the check happens (edge binding:
   `add_connection`, JSON load, `bind_inputs`; before any node runs; derived →
   base allowed). Doxygen "throws `TypeMismatchException` if…" on `bind_input`,
   `bind_inputs`, `add_connection`, and the exception class.
8. **Refused edge not stored**: `add_connection` stores `connections[id]` only
   after `bind_input` succeeds (also covers the "node not found" throws).

## Out of scope
- Frontend (`dealiiX-platform`).
- Collecting all bad edges at once (`Network::validate()`), not needed now.

## Risk
- `refresh_dynamic_inputs` (`coral_network_implementation.h:142`) now checks
  types too; a sub-network relying on a mismatched binding would throw.
  The existing suite is the detector.
  Outcome: no existing test failed (plan 8.2).

## Audit fixes
- Message names the target by `hash()` (registry/JSON name), not `type_name()`.
- `from_json` logs a failing edge (`slog_error`) and rethrows, like nodes.
