# Issue #66: MutexedParameterAcceptor solution

Self-contained brief. A fresh session must be able to implement this from the
file alone. Rules from the user's CLAUDE.md still apply: show every diff and
wait for approval before touching a file, one decision at a time, never
commit, no Co-Authored-By lines, be brief.

## Problem
Two independent graph nodes whose types derive from `dealii::ParameterAcceptor`
run on different threads. Their constructors call `add_parameter`,
`enter_subsection`, `leave_subsection`, which all mutate the static
`ParameterAcceptor::prm` (and its subsection path). deal.II 9.7.1 locks only
`class_list` and the id counter in the base constructor (private
`class_list_mutex`); `prm` is unprotected. Observed failure:
`Node 3 [3_auto_3] failed: No such node (Domain.Polynomial_20degree.pattern)`:
the Space node declared its parameter inside the Domain subsection.

## State of the coral repo (uncommitted, branch 66-allow-selected-nodes-...)
Already done, compiled, and approved by the user:
- `backends/dealii/include/parameters.h`: stand-ins `coral::DomainParameters<dim,spacedim>`,
  `coral::FiniteElementSpaceParameters<dim,spacedim>` (same constructor pattern
  as the immersx classes) and `coral::initialize_parameters(ParameterAcceptor&,
  ParameterAcceptor&, const std::string&)` (calls `ParameterAcceptor::initialize`).
- `backends/dealii/include/register_types.h`: include of `parameters.h`;
  aliases `DomainParameters<2, 2>` / `FiniteElementSpaceParameters<2, 2>` (only
  for dim == spacedim, others show as `coral::...`); `ParameterAcceptor`
  alias + `register_abstract_type`; `register_type` + `register_base<..., ParameterAcceptor>`
  for both classes; `Initialize parameters` function node.
- `test_files/parameter_acceptor_race.json`: graph of the issue, with registry names.
- `backends/dealii/tests/parameter_acceptor.cc`: test `ParameterAcceptorRace.ConcurrentConstruction`,
  200 iterations, compares `prm` dump (`ShortPRM`) against a sequential reference.
  Currently FAILS at iteration 0 with the message above (this is the reproduction).
Untracked and NOT to be committed: `node_types.json` (repo root, a registry dump),
`test_files/minimal_failing.json` (the OP's file, immersx names, macOS path).

## Reproduce
```
docker exec coral bash -lc '
  cmake -S /coral -B /tmp/b66 -DCMAKE_BUILD_TYPE=Debug &&
  cmake --build /tmp/b66 -j8 --target dealii_backend_tests &&
  cd /tmp/b66/backends/dealii/tests &&
  ./dealii_backend_tests --gtest_filter="ParameterAcceptorRace.*"'
```
The repo is mounted at `/coral` in the container `coral` (deal.II 9.7.1 shared).

## Solution
1. Add to `parameters.h`:
```cpp
class MutexedParameterAcceptor : public dealii::ParameterAcceptor
{
public:
  explicit MutexedParameterAcceptor(const std::string &name)
    : dealii::ParameterAcceptor(name) {}

  /** One mutex shared by every subclass; protects the static prm. */
  static std::recursive_mutex &mutex()
  {
    static std::recursive_mutex m;   // function-local: safe init order
    return m;
  }
};
```
2. `DomainParameters` and `FiniteElementSpaceParameters` derive from
   `MutexedParameterAcceptor`, and the FIRST statement of each constructor body is
   `const std::lock_guard<std::recursive_mutex> lock(mutex());`.
   (The base constructor cannot hold the lock for the derived body: C++ has no
   "constructor finished" hook, so each leaf locks itself.) Recursive because an
   acceptor may build other acceptors as members on the same thread.
3. `initialize_parameters` takes the same lock (declare/parse touch `prm`).
4. No change in Coral core, no new registration API.

Expected result: the test passes (200 iterations, no exception, dump equals reference).
If it fails with a dump MISMATCH and no exception, the oracle is suspect (the
`initialize` pass may change defaults): investigate before blaming the lock.
Known unverified points: `Network::set_threads_number(2)` is ignored in the test
(log shows 32 workers); the dump comparison was never reached before the fix.

## Rejected alternatives (and why)
- Mutex only in the base constructor: cannot cover the derived body.
- Coral-side lock keyed on the `ParameterAcceptor` ancestor (`ancestor_casters`,
  `coral.h:533`) or a registration-time mutex argument: works, but protects only
  Coral-driven construction; the immersx plugin registers `ParameterAcceptor` on its own
  (`register_common_types`, `register.h:733`), so a mark in the dealii backend alone misses it.
- Hard-coding the name `dealii::ParameterAcceptor` in `core`: rejected by the user.

## Caveat to write in the PR (porting to immersx)
The real classes live in immersx, not in coral; the coral classes are stand-ins.
To fix the issue for real, in `immersx`:
- Add `MutexedParameterAcceptor` (in `include/immersx/core/`, no Coral dependency) and make
  every class registered with Coral as a parameter type derive from it and lock in its
  constructor: `DomainParameters`, `FiniteElementSpaceParameters`, `OutputHandlerParameters`,
  adapter, workflow/problem, `ElasticStatic`/`Elastodynamics` parameters
  (see the `register_parameter_type` call sites in `coral/include/immersx/coral/register.h`:
  ~511, 1013, 1319, 1321, 1683, 1813, 1863, 1899). Audit all of them: any class whose
  constructor calls `add_parameter`/`enter_subsection` must lock. A class that forgets stays racy.
- Define `mutex()` in a `.cc` of `libimmersx`, not inline in a header (header definitions may be
  duplicated per plugin: plugins are loaded with `dlopen(RTLD_NOW | RTLD_LOCAL)`).
- Static builds (`IMMERSX_BUILD_STATIC` / `IMMERSX_PORTABLE`) with several Coral plugins in
  one graph (1d/2d/3d) give one mutex per plugin but a single `prm` (deal.II is shared), so
  cross-plugin races remain. Options: (a) CMake refuses to build the Coral plugins against a
  static `libimmersx` (like the existing shared-`coral::core` check, root `CMakeLists.txt:243-270`);
  (b) hybrid: `MutexedParameterAcceptor::set_mutex(std::recursive_mutex&)` in immersx core,
  called by each plugin's `load_plugin<dim>` with a mutex from a pool defined once in
  `coral_core` (`coral::mutex_pool::get("dealii::ParameterAcceptor")`). (b) is not implemented.
  Not verified: whether anyone builds the Coral plugins statically; Coral's behaviour with
  duplicated non-template immersx types in static builds.
- The fix cannot be verified inside this repo against real immersx classes; it needs an
  immersx build (~15 min container) and a run of the OP's `minimal_failing.json` there.
