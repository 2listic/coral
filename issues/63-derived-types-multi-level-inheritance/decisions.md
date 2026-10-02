# #63 — Decisions

Context for implementing GitHub issue #63 (2listic/coral). Agreed in discussion;
see `desiderata.md` for the goals. Plan: `plan.md`.

## Problem (verified)
- `register_json_header<T>()` resets an existing entry (`core/include/coral.h:1419`).
  `register_derived_type<B, T>` calls `register_abstract_type<B>()` → resets `B`.
  Chains (`E : D : B`) fail in any order; siblings (`<B,T1>`, `<B,T2>`) leave
  `B.derived == [T2]`.
- An entry holds one `base` string and one `to_base` (one step).
  `get_shared<T>()` (`coral.h:1640`, const overload `:1716`) accepts only the
  direct `base`.

## Scope
- #63: registration + runtime casting (`get_shared`).
- #64 (separate issue, depends on #63): edge type check at network load time.
  Out of scope here, including the wrong `base` read in `bind_inputs`
  (`core/include/coral_implementation.h:334`) — leave it untouched.

## Decisions
1. **API**: `NodeObject::register_base<T, B>()` declares "T is-a B".
   `register_derived_type` (all overloads) is **removed**; migrate its call sites:
   `backends/dealii/include/register_types.h:79` (ofstream→ostream) and `:149`
   (FE_Q→FiniteElement), `backends/dealii/tests/dealii_types.cc:32`,
   `backends/dealii/tests/node_object_tests.cc:73`, `README.md:437`.
   ```cpp
   register_type<A, unsigned int>("degree");
   register_base<A, B1>();
   register_base<A, B2>();
   ```
2. **Liskov at runtime**: `get_shared<X>()` (both overloads) returns the object
   if its type is `X`, else looks `X` up among the object's ancestors and applies
   that ancestor's caster; throws otherwise. One caster (`static_pointer_cast`)
   serves `B &`, `const B &` and by-value `B` (`cast_args` strips cv/ref,
   `coral.h:1499`). By-value slicing is accepted, as in plain C++.
3. **All ancestors stored**: each entry holds every ancestor (not only direct
   bases), each with its own caster. Casters of indirect ancestors are composed
   at registration (`A→C` = `A→B` then `B→C`). `register_base<T, B>` adds `B` and
   `B`'s ancestors to `T` **and to all descendants of `T`**. Registration order
   does not matter; `get_shared` does one lookup, no walk.
4. **JSON**: `"bases"`: list of all ancestor hashes; `"derived"`: list of all
   descendant hashes. Each key is omitted when its list is empty. The key
   `"base"` is no longer written.
5. **Sub-network SELF ports** (`core/include/coral_network_implementation.h:897`,
   today `type = base ?? type`): write `"type": <own type>, "bases": [...]`.
   The only backend test there (`backends/dealii/tests/modules.cc`,
   `NetworkNodeExposesDanglingSelfOutput`) uses `Triangulation` (no base) — unaffected.
6. **Entry split**: an entry has a *construction* part (`node_type`, executor,
   `arguments`/`inputs`/`outputs`, …), replaced by `register_type` /
   `register_abstract_type` / etc. as today, and an *inheritance* part
   (`bases`, `derived`, casters), changed only by `register_base` and never reset.
7. **`register_base<T, B>` checks/creates**:
   `static_assert(std::is_base_of_v<B, T> && !std::is_same_v<B, T>)`.
   If `T` or `B` is not registered, it is created as abstract; a later
   `register_type` replaces only its construction part.
8. **Idempotent**: repeating `register_base<T, B>` changes nothing; no duplicates
   in `bases` / `derived` (tests call `register_all_types()` repeatedly).
9. **Diamond**: an ancestor reached a second time is skipped (first caster
   wins). Correct for virtual inheritance; ambiguous for non-virtual diamonds,
   accepted. One-line note in the docs, no warning.
10. **entt conversion dropped**: `register_base` does not register an entt `conv`;
    delete `detail::shared_ptr_to_base` (`coral.h:107`) and its uses
    (`coral.h:753`, `:1590`). Nothing calls `allow_cast`/`cast`.
11. **Node copies of entries**: a `NodeObject` copies its entry at construction
    (`coral.h:1285`, `:1395`); a `register_base` after node creation is invisible
    to that node. Same as today for everything — accepted, not changed.

## Frontend (separate repo `dealiiX-platform`, not part of this work)
Today it reads `base` as the single connection type (`base ?? type`,
`src/lib/utils/canvasNodeUtils.ts:145`, `:405`; `graphParser.ts:346`;
`networkNodeCanvas.ts:102`). Transition: accept an edge if the target type is in
`{type} ∪ bases ∪ {base}`, for both node entries and sub-network ports.
