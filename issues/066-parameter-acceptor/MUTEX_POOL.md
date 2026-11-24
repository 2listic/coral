# Issue #66: MUTEX_POOL solution (Coral-side serialization)

Self-contained brief: a fresh session must be able to implement it from this
file alone. Rules from the user's CLAUDE.md still apply: show every diff and
wait for approval before touching a file, one decision at a time, never commit,
no Co-Authored-By lines, be brief, do not do what was not asked.

This is the ALTERNATIVE to `MUTEXED_SUBCLASS_SOLUTION.md` (leaf classes lock
themselves). Implement only one of them. Here the classes stay untouched.

## Problem (short)
Independent graph nodes whose types derive from `dealii::ParameterAcceptor` run on
different threads; their constructors mutate the static `ParameterAcceptor::prm`
(add_parameter / enter_subsection / leave_subsection). Reproduced by test
`ParameterAcceptorRace.ConcurrentConstruction` (see MUTEXED_SUBCLASS_SOLUTION.md for
the state of the repo, the repro command and the failure message
`No such node (Domain.Polynomial_20degree.pattern)`). Repo is mounted at `/coral` in the
docker container `coral`; build dir `/tmp/b66`.

## Idea
Coral owns a process-wide pool of named recursive mutexes. A node type (or function /
method) can be registered with a pointer to one of them; the node runner locks it
around the node's execution. Nodes sharing the mutex are mutually exclusive; all other
nodes run in parallel. Requirements from the user:
- the registry file (`NodeObject::get_registry()`, e.g. `node_types.json`) must NOT change;
- no dependency cycle: `coral_core` stays free of deal.II and immersx; the immersx Coral
  plugin layer already links `coral::core`, so it may call the pool.

## Verified facts about the code (core/include)
- Execution point of EVERY node kind: `NodeObject::operator()`,
  `coral_implementation.h` ~line 203: `object = initializer.executor(self, arguments);`.
- That `initializer` is a member of the node: a COPY of the registry entry made at node
  creation (`coral_implementation.h` ~line 36: `initializer = initializer_it->second;`).
  A pointer field in the entry is copied with it. Registration happens at plugin load,
  before any graph exists, so this is fine.
- `NodeObjectInitializer` (`coral.h` ~line 500). Registry entries:
  `static inline std::map<std::string, NodeObjectInitializer> initializers` (`coral.h` ~1693):
  stable references; registration is single-threaded by contract.
- `get_registry()` (`coral_implementation.h:58-66`) serializes ONLY
  `initializer.json_serializer`. A new non-JSON field is never written. => registry unchanged.
- `register_json_header` (`coral.h` ~1803) RESETS an existing entry to a fresh initializer
  on re-registration, keeping only `ancestor_casters` and `bases`/`derived`. A new field
  would be dropped then, so it must be set right after EVERY registration call
  (in the same helper), never in a separate earlier step.
- `register_type<T, Args...>(arg_names)` and `register_type<T, Arg>(name)` return
  `detail::NodeObjectInitializer &` (`coral.h` ~809, ~962): the field can be set on the
  returned reference: `register_type<...>(...).mutex = &m;`.
- `register_function` and `register_method` return `void` but take an optional last argument
  `RegistryMetadata`, and every branch calls `set_registry_metadata(initializer, metadata)`
  (`coral.h:598` definition; calls at 1008, 1042, 1107, 1145, 1216, 1248, 1330). That is
  the single funnel: no overloads and no refactor needed.
- `has_metadata` in `register_function` is `!metadata.operation.empty() || !metadata.display_name.empty()`
  (`coral.h` ~1304). A `RegistryMetadata` that sets ONLY `mutex` keeps `has_metadata == false`,
  i.e. the legacy naming path ({operation, output, inputs...}) is untouched. The registry
  key is `operation + "::" + hash<Function>` where operation defaults to `arg_names.front()`;
  with metadata.operation equal to the old name the key is the same. `register_method` always
  takes `arg_names[0]` as the method name, metadata does not change it.
- Plugins are loaded with `dlopen(RTLD_NOW | RTLD_LOCAL)` (`core/source/backend_main.cc:133`);
  `CORAL_BUILD_SHARED_CORE` defaults ON and the immersx build requires `coral::core` SHARED.

## Implementation steps
### A. coral core
1. Pool. New public header (or section of `coral.h`) and a `.cc` compiled INTO `coral_core`
   (add it to the source list of `add_library(coral_core ...)` in `core/CMakeLists.txt:7`;
   check how `CORAL_IMPL_INLINE` / existing non-template symbols are built and follow it):
   ```cpp
   namespace coral::mutex_pool
   {
     /** Process-wide recursive mutex for `key`; created on first use, never destroyed. */
     std::recursive_mutex &get(const std::string &key);
   }
   ```
   Implementation: a function-local static guard mutex and a function-local
   `static auto *pool = new std::map<std::string, std::recursive_mutex>;` (intentionally leaked
   so nodes destroyed during static destruction can still reference it). `std::map` keeps
   references stable. Defined once in the shared library => one mutex per process, whatever
   the plugins' own linkage (this is why it must not be an inline header definition).
2. `NodeObjectInitializer`: add
   `std::recursive_mutex *mutex = nullptr; // not serialized, not part of json_serializer`.
3. `RegistryMetadata`: add `std::recursive_mutex *mutex = nullptr;` and in
   `set_registry_metadata` add `initializer.mutex = metadata.mutex;` (after the string copies).
4. `NodeObject::operator()`: wrap only the executor call:
   ```cpp
   {
     std::unique_lock<std::recursive_mutex> lock;
     if (initializer.mutex)
       lock = std::unique_lock<std::recursive_mutex>(*initializer.mutex);
     object = initializer.executor(self, arguments);
   }
   ```
   Do not hold it over the argument checks or the "copy back the value" code that follows.

### B. dealii backend (`backends/dealii/include/register_types.h`)
The stand-ins `coral::DomainParameters` / `FiniteElementSpaceParameters` and the
`Initialize parameters` node are already registered (see MUTEXED_SUBCLASS_SOLUTION.md).
Do NOT apply the MutexedParameterAcceptor change if this solution is chosen. Add:
```cpp
auto &m = coral::mutex_pool::get("dealii::ParameterAcceptor");
NodeObject::register_type<DomainParameters<dim, spacedim>, const std::string &>("subsection").mutex = &m;
NodeObject::register_type<FiniteElementSpaceParameters<dim, spacedim>, const std::string &>("subsection").mutex = &m;
// Initialize parameters (touches prm too): metadata with ONLY the mutex => legacy names kept
RegistryMetadata metadata;
metadata.mutex = &mutex_pool::get("dealii::ParameterAcceptor");
NodeObject::register_function(initialize_parameters,
  {"Initialize parameters", "parameter_1", "parameter_2", "parameter_file"}, metadata);
```
The key string must be identical everywhere; a typo silently disables the lock (put it in one
named constant). Keep `register_base<..., ParameterAcceptor>()` as it is.

### C. Tests
- Existing `ParameterAcceptorRace.ConcurrentConstruction` must now PASS (200 iterations).
  If it fails with a dump mismatch and no exception, suspect the oracle (the `initialize` pass
  may change defaults) before blaming the lock. Known: `Network::set_threads_number(2)` is
  ignored in that test (log shows 32 workers).
- New core test: `mutex_pool::get("a") == &get("a")`, `get("a") != get("b")`.
- New core test: two independent nodes registered with the same mutex whose executors sleep
  100 ms and record start/end; assert the intervals do not overlap; same nodes without a mutex
  do overlap.
- New test: registry unchanged: dump `get_registry()` for a type registered with and without
  the mutex, assert equal JSON. Also diff `node_types.json` from `coral -p <plugin> register`
  before/after the change (must be byte-identical).
- Run the whole `dealii_backend_tests` and core tests; run ctest serially.

### D. Port to immersx (separate repo `/home/matteo/Projects/dealII-X/repos/immersx`)
Only the Coral plugin layer `coral/include/immersx/coral/register.h`; the immersx classes and
`libimmersx` are untouched, no new dependency edge:
- In `register_parameter_type<Parameters>(type_name)` (line ~983) set the mutex on the result of
  `register_type` every time it is called:
  `coral::NodeObject::register_type<Parameters, const std::string &>("subsection").mutex = &coral::mutex_pool::get("dealii::ParameterAcceptor");`
  Its ~8 call sites (lines ~511, 1013, 1319, 1321, 1683, 1813, 1863, 1899) need no change.
- In `register_initialize_parameters` (line ~943) add `metadata.mutex = &coral::mutex_pool::get(...)`.
- Static `IMMERSX_BUILD_STATIC` / `IMMERSX_PORTABLE` builds with several plugins: each plugin has
  its own copy of immersx code, but all obtain the SAME mutex from `coral_core`, so the
  cross-plugin race of the MutexedParameterAcceptor design does not exist here.
- Verification needs an immersx build (~15 min container) and a run of the OP's
  `minimal_failing.json` there. It cannot be verified inside this repo with the real classes.

## Caveats to write in the PR
- Coarser grain: the whole node execution (the whole constructor) is serialized, not only the
  parameter declarations; fine for acceptors (cheap constructors).
- Protects only Coral-driven execution; threaded use of acceptors outside Coral is not covered.
- Every registration that creates/uses a ParameterAcceptor must carry the mutex; one that does
  not stays racy (e.g. any new acceptor registered without going through the helper).
- A re-registration resets the field (see above): always set it in the registering helper.
- The registry JSON and graph files are unchanged.

## Not verified
- Behaviour of the leaked pool at exit with nodes in statics (leaking is the safeguard).
- That nothing else in `core` copies/serializes `NodeObjectInitializer` wholesale.
- How `core/CMakeLists.txt` lists sources / exports headers for the new `.cc`.
