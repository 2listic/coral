# #63 — Implementation plan

Read `decisions.md` first: it holds the context and every decision (D1–D11)
this plan implements. Goals: `desiderata.md`.

Build/test only inside the `coral` container (repo mounted at `/app`):
`docker exec coral bash -lc 'cmake --build /app/build -j4 && ctest --test-dir /app/build --output-on-failure'`

## 1. Inheritance part of an entry (D3, D4, D6)
- [x] 1.1 In `detail::NodeObjectInitializer` (`core/include/coral.h`, ~`:417`):
  - [x] add `std::map<std::string, Caster> ancestor_casters` (ancestor hash → caster),
        `Caster = std::function<std::shared_ptr<entt::meta_any>(std::shared_ptr<entt::meta_any>)>`
- [x] 1.2 JSON keys of the inheritance part: `"bases"` (all ancestors),
      `"derived"` (all descendants), both arrays of hashes; each key omitted
      when its list is empty (D4)

## 2. Registration no longer wipes inheritance (D6)
- [x] 2.1 `register_json_header<T>()` (`coral.h:1415`), when the entry exists:
  - [x] save `ancestor_casters`, `json_serializer["bases"]`, `json_serializer["derived"]`
  - [x] reset as today (construction part replaced)
  - [x] restore the saved inheritance part

## 3. `register_base<T, B>()` (D1, D3, D7, D8, D9)
- [x] 3.1 New static member of `NodeObject`, declared next to `register_type`
- [x] 3.2 `static_assert(std::is_base_of_v<B, T> && !std::is_same_v<B, T>)`
- [x] 3.3 If `T` / `B` has no entry → `register_abstract_type<X>()`
- [x] 3.4 Direct caster `c_TB`: `try_cast<std::shared_ptr<T>>`, throw if null,
      return `meta_any(std::static_pointer_cast<B>(*ptr))`
- [x] 3.5 Build `S` = `{B → c_TB}` ∪ `{C → c_BC ∘ c_TB}` for each `(C, c_BC)`
      in `B`'s `ancestor_casters`
- [x] 3.6 For `D` in `{T}` ∪ `T.derived` (`c_DT` = identity for `T`, else
      `D.ancestor_casters[T]`), for each `(X, c)` in `S`, **if `X` is not already
      an ancestor of `D`** (idempotence + diamond skip):
  - [x] `D.ancestor_casters[X] = c ∘ c_DT`
  - [x] push `X` to `D.bases`
  - [x] push `D` to `X.derived`
- [x] 3.7 Casters captured by value (copies of `std::function`)
- [x] 3.8 Doxygen: usage example + one-line diamond note (D9)

## 4. `get_shared` (D2)
Applied together with step 7, as one checkpoint: `get_shared` stops reading
`to_base`/`"base"`, so call sites not yet migrated would fail in between.
- [x] 4.1 Non-const overload (`coral.h:1640`)
- [x] 4.2 Const overload (`coral.h:1716`)
- [x] 4.3 In both: exact-type branch unchanged; otherwise
  - [x] look up `detail::hash<type>()` in `initializer.ancestor_casters`;
        absent → throw "Cannot cast object of type … to …" (as today)
  - [x] apply the caster; check the result has a value
  - [x] replace the old `j.at("base")` hash checks with a check against `detail::hash<type>()`
  - [x] `try_cast` as today

## 5. Removals (D1, D10)
- [x] 5.1 `register_derived_type`:
  - [x] inline overload (`coral.h:740`)
  - [x] declarations (`:770`, `:789`)
  - [x] definitions (`:1575`, `:1609`)
- [x] 5.2 `detail::shared_ptr_to_base` (`coral.h:107`) and the entt `conv` registrations
- [x] 5.3 `NodeObjectInitializer::to_base` (kept until here so every step builds)

## 6. Sub-network SELF ports (D5)
- [x] 6.1 `core/include/coral_network_implementation.h:897`:
  - [x] `arg_json["type"] = info["type"]`
  - [x] if `info` has a non-empty `"bases"` → `arg_json["bases"] = info["bases"]`
  - [x] update the comment above it

## 7. Migrate call sites (D1)
Applied together with step 4 (see there).
- [x] 7.1 `backends/dealii/include/register_types.h:79`:
      `register_type<std::ofstream, std::string>("file_name");`
      `register_base<std::ofstream, std::ostream>();`
- [x] 7.2 `register_types.h:149`:
      `register_type<FE_Q<dim, spacedim>, unsigned int>("fe_degree");`
      `register_base<FE_Q<dim, spacedim>, FiniteElement<dim, spacedim>>();`
- [x] 7.3 `backends/dealii/tests/dealii_types.cc:32` — same pattern
- [x] 7.4 `backends/dealii/tests/node_object_tests.cc:73` — same pattern
- [x] 7.5 `README.md:437` and any doc mentioning `register_derived_type` / `base`

## 8. Tests
- [x] 8.1 New file `core/tests/inheritance.cc` (no deal.II). Types at namespace
      scope, **unique per test** (the registry is global to the executable).
      Bases carry data members so pointer adjustment is exercised.
  - [x] `SingleBase`: `A : B`; `get<B>()` on an `A` node; JSON `A.bases == [B]`,
        `B.derived == [A]`; no `"base"` key
  - [x] `MultipleBases`: `A : B1, B2`; `get<B1>()`, `get<B2>()` read the right members
  - [x] `ChainBaseFirst`: `register_base<B, C>` then `<A, B>`; `get<C>()` on `A`;
        `A.bases ⊇ {B, C}`, `C.derived ⊇ {A, B}`
  - [x] `ChainDerivedFirst`: same, reverse order
  - [x] `MixedGraph`: `A : B1, B2`; `B2 : C`; `C : E` → `A.bases == {B1, B2, C, E}`
        (as sets); `get<E>()` on `A`
  - [x] `Siblings`: `<T1, B>`, `<T2, B>` → `B.derived == {T1, T2}`
  - [x] `SurvivesRegisterType`: `register_base<A, B>` then `register_type<A>()`
        and `register_type<B>()` → `A` constructible, `A.bases` / `B.derived` kept
  - [x] `CreatesMissingAsAbstract`: `register_base<A, B>` with neither registered
        → both exist, `B` is `abstract`
  - [x] `Idempotent`: `register_base<A, B>` twice → no duplicates
  - [x] `VirtualDiamond`: `D : B1, B2`; `B1, B2 : virtual C` → `get<C>()` works;
        `C` appears once in `D.bases`
  - [x] `UnrelatedThrows`: `get<Unrelated>()` on `A` → `std::runtime_error`
  - [x] `FunctionTakesBase`: `f(const C &)` and `g(C &)` via `register_function`,
        fed an `A` node (`A : B : C`), executed; `g`'s output is the same `A` node
        (pattern: `make_method_node`, `set_arguments`, `(*fun)()` — see
        `backends/dealii/tests/node_object_tests.cc:163-175`)
- [x] 8.2 Backend (`backends/dealii/tests`):
  - [x] `dealii_types.cc` `FE_Q` (migrated) still passes, incl. `get<FiniteElement<2>>()`
  - [x] new `modules.cc` test `NetworkNodeExposesDerivedSelfOutput`: like
        `NetworkNodeExposesDanglingSelfOutput`, with an `FE_Q<2>` node → port
        `type == FE_Q<2, 2>`, `bases == [FiniteElement<2, 2>]`
- [x] 8.3 Manual (not automated): `register_base<B, A>()` with swapped order
      fails to compile (D7)

## 9. Done when
- [x] 9.1 Full build + `ctest` green (core, backend, MPI tests)
- [x] 9.2 No `register_derived_type` or `shared_ptr_to_base` left in the repo

## 10. After the audit
- [x] 10.1 JSON files: `"base": X` → `"bases": [X]` in `test_files/*.json`
      (10 files) and in `dealiiX-platform/examples/graphs/*.json` (2 files)
- [x] 10.2 Pre-existing bug: const `get_shared` / `get` always threw, because
      it did `try_cast<std::shared_ptr<const T>>` on a stored `std::shared_ptr<T>`.
      Fix: `try_cast<std::shared_ptr<T>>`, converted on assignment.
      Tests `ConstGetExactType`, `ConstGetBase` (failed before the fix)
- [x] 10.3 Test `FunctionTakesBaseByValue`: `f(C)` fed an `A` node
      (`A : B : C`); sliced copy, `A` node unchanged (D2)
- [x] 10.4 `README.md`: `register_base` description reworded

## Out of scope
- `bind_inputs` `base` read (`core/include/coral_implementation.h:334`) → #64
- Load-time edge checks → #64
- Frontend (`dealiiX-platform`) → see `decisions.md`, "Frontend"
