# High Priority Simplification Plan

This document outlines an actionable implementation plan for the high priority simplification recommendations identified in the analysis.

## Recent Update

**Step 0 Added**: After analyzing entt usage (see `entt_analysis.md`), it became clear that CORAL maintains a duplicate reflection system alongside entt. Step 0 has been added as a **foundational step** to properly integrate entt as the single source of truth for type metadata. This eliminates ~500 lines of duplication and simplifies the registration API before tackling other refactorings.

**Impact**: Step 0 makes subsequent steps easier and achieves most of the goals of the original Step 4 (registration consolidation).

## Overview

**Goal**: Reduce complexity in `coral.h` by ~40-50% while preserving all functionality.

**Target metrics**:
- Reduce `coral.h` from ~1800 lines to ~1000-1200 lines
- Properly integrate entt as single source of truth (eliminate duplication)
- Eliminate double pointer indirection (improve performance)
- Simplify port connection model (improve maintainability)
- Consolidate registration functions (reduce API surface)
- Remove elementary type special cases (unify code paths)

**Estimated effort**: 5-7 weeks of focused development

---

## Step 0: Properly Integrate entt Reflection System

**Goal**: Use entt as the single source of truth for type metadata, eliminating the parallel custom reflection system.

**Impact**: Foundation for all other simplifications. Removes ~500 lines of duplication, makes registration API cleaner.

**Rationale**: entt was introduced to simplify registration, but currently CORAL maintains a separate reflection database (`initializers` map) alongside entt's. This duplication adds complexity and maintenance burden. entt already provides everything needed: type registration, constructors, methods, base/derived relationships, and custom properties.

### Tasks

#### 0.1: Analyze current entt usage

- [ ] Audit how entt is currently used
  - [ ] List all uses of `entt::meta_any` in coral.h
  - [ ] List all uses of `entt::resolve<T>()`
  - [ ] List all uses of `entt::meta_factory<T>()`
  - [ ] Document what percentage of entt's reflection API is used

- [ ] Identify duplication with custom system
  - [ ] Compare `initializers` map contents vs what entt can store
  - [ ] List all data stored in `NodeObjectInitializer` struct
  - [ ] Categorize: what entt can handle vs what's CORAL-specific
  - [ ] Document findings in `docs/entt_duplication_audit.md`

- [ ] Study entt reflection API
  - [ ] Read entt meta documentation thoroughly
  - [ ] Create examples of: type registration, constructors, methods, properties
  - [ ] Test entt property system (can store lambdas, custom data)
  - [ ] Verify entt can iterate over all registered types

#### 0.2: Design entt-native architecture

- [ ] Design property schema for CORAL metadata
  ```cpp
  // What to store as entt properties:
  .prop("executor"_hs, executor_lambda)           // Lazy evaluation function
  .prop("parse_string"_hs, parse_lambda)          // String deserialization
  .prop("to_string"_hs, stringify_lambda)         // String serialization
  .prop("arg_names"_hs, vector<string>)           // Argument names for JSON
  .prop("connection_types"_hs, vector<ConnType>)  // Input/output/pass_through
  .prop("node_type"_hs, NodeType::constructor)    // Node type classification
  ```

- [ ] Design unified registration API using entt
  - [ ] Sketch `register_type<T>()` implementation
  - [ ] Sketch `register_type<T, Args...>(names)` implementation
  - [ ] Sketch `register_method()` implementation
  - [ ] Sketch `register_function()` implementation
  - [ ] Ensure all use entt underneath

- [ ] Design JSON schema generation from entt
  - [ ] Algorithm to iterate all registered entt types
  - [ ] Algorithm to extract constructors from entt
  - [ ] Algorithm to extract methods from entt
  - [ ] Algorithm to extract properties (arg names, etc)
  - [ ] Sketch `generate_json_from_entt()` function

- [ ] Create proof-of-concept
  - [ ] Implement for 3 test types (int, Point<2>, simple class)
  - [ ] Register with entt using new design
  - [ ] Generate JSON schema from entt metadata
  - [ ] Verify all information present
  - [ ] Compare JSON output to current system

#### 0.3: Migrate type registration to entt

- [ ] Create new registration implementation
  - [ ] Implement `register_type<T>()` using entt
  - [ ] Register type with `entt::meta<T>().type(name)`
  - [ ] Add default constructor if applicable
  - [ ] Store executor lambda as entt property
  - [ ] Store serialization lambdas as entt properties

- [ ] Handle elementary types
  - [ ] Detect with `std::is_trivially_copyable_v<T>`
  - [ ] Store parse_string/to_string as entt properties
  - [ ] Store default value as entt property
  - [ ] Remove separate `register_elementary_type` code path

- [ ] Handle types with constructor arguments
  - [ ] Implement `register_type<T, Args...>(names)`
  - [ ] Use `entt::meta<T>().ctor<Args...>()`
  - [ ] Store arg names as entt property
  - [ ] Store executor lambda as entt property
  - [ ] Generate connection types from Args

- [ ] Handle abstract types
  - [ ] Detect with `std::is_abstract_v<T>`
  - [ ] Register with entt but no constructor
  - [ ] Mark as abstract in entt properties
  - [ ] Store in derived type list

- [ ] Handle derived types
  - [ ] Use `entt::meta<Derived>().base<Base>()`
  - [ ] Use `entt::meta<Derived>().conv<&converter>()`
  - [ ] Let entt handle base class relationships
  - [ ] Remove custom `to_base` lambdas
  - [ ] Query base via `entt::resolve<T>().base()`

#### 0.4: Migrate method/function registration to entt

- [ ] Implement method registration using entt
  - [ ] Use `entt::meta<T>().func<&T::method>(name)`
  - [ ] Store method in entt reflection system
  - [ ] Store arg names as entt property on the func
  - [ ] Store executor lambda as entt property
  - [ ] Detect void/non-void, const/non-const via traits

- [ ] Implement function registration using entt
  - [ ] Create synthetic type for function (or use std::function type)
  - [ ] Register with entt
  - [ ] Store as entt func or as separate type
  - [ ] Store arg names as entt property
  - [ ] Store executor lambda as entt property

- [ ] Unify method registration
  - [ ] Single code path for all method variants
  - [ ] Use `if constexpr` to handle void/non-void
  - [ ] Use `if constexpr` to handle const/non-const
  - [ ] Eliminate 4 separate method registration functions

- [ ] Unify function registration
  - [ ] Single code path for all function variants
  - [ ] Auto-detect lambda vs function pointer vs std::function
  - [ ] Use `if constexpr` to handle void/non-void
  - [ ] Eliminate 3 separate function registration functions

#### 0.5: Implement JSON schema generation from entt

- [ ] Create `generate_registry_from_entt()` function
  - [ ] Iterate all types: `entt::resolve([](meta_type t) { ... })`
  - [ ] For each type, extract name: `t.name()`
  - [ ] Extract constructors: iterate `t.ctor()`
  - [ ] Extract methods: iterate `t.func()`
  - [ ] Extract base classes: iterate `t.base()`
  - [ ] Build JSON object matching current format

- [ ] Extract constructor information
  - [ ] Get argument count from entt
  - [ ] Retrieve arg names from entt property
  - [ ] Retrieve arg types from entt property
  - [ ] Generate inputs/outputs arrays
  - [ ] Store in JSON

- [ ] Extract method information
  - [ ] Get method name from entt
  - [ ] Retrieve arg names from entt property
  - [ ] Retrieve return type from entt property
  - [ ] Generate method JSON entry
  - [ ] Handle void/non-void variants

- [ ] Extract properties (CORAL-specific)
  - [ ] Check for "node_type" property
  - [ ] Check for "is_elementary" property
  - [ ] Check for "default_value" property
  - [ ] Add to JSON schema

- [ ] Validate JSON output
  - [ ] Compare to current `get_registry()` output
  - [ ] Ensure all fields present
  - [ ] Ensure format matches (for backward compatibility)
  - [ ] Test with existing JSON consumers

#### 0.6: Update NodeObject to query entt

- [ ] Modify NodeObject constructor
  - [ ] Replace `initializers.at(hash)` with `entt::resolve(hash)`
  - [ ] Query entt for type metadata
  - [ ] Extract executor from entt property
  - [ ] Extract serialization lambdas from entt properties
  - [ ] Build `NodeObjectInitializer` from entt data (or eliminate it)

- [ ] Update `get_registry()` method
  - [ ] Replace current implementation
  - [ ] Call `generate_registry_from_entt()`
  - [ ] Return generated JSON

- [ ] Update type queries
  - [ ] Replace `hash()` with entt type lookup
  - [ ] Use `entt::resolve<T>().name()` for type names
  - [ ] Remove custom `type_aliases` map
  - [ ] Use entt as canonical source

- [ ] Update base/derived conversions
  - [ ] Query entt for base class: `t.base()`
  - [ ] Use entt's conversion system: `t.conv()`
  - [ ] Simplify `get_shared<T>()` (no custom conversion logic)
  - [ ] Remove `to_base` lambdas from NodeObjectInitializer

#### 0.7: Remove duplicate custom reflection system

- [ ] Mark for removal
  - [ ] Tag `static inline std::map<string, NodeObjectInitializer> initializers` for deletion
  - [ ] Tag `detail::type_aliases()` for deletion
  - [ ] Tag `detail::store_identifier()` for deletion
  - [ ] Document what's being removed

- [ ] Migrate remaining uses
  - [ ] Find all uses of `initializers` map
  - [ ] Replace with entt queries
  - [ ] Find all uses of `type_aliases`
  - [ ] Replace with `entt::resolve<T>().name()`

- [ ] Consider NodeObjectInitializer
  - [ ] If still needed, populate from entt on demand
  - [ ] Or eliminate entirely, query entt directly
  - [ ] Make it a view over entt data, not storage

- [ ] Delete obsolete code
  - [ ] Remove `initializers` static map
  - [ ] Remove `type_aliases` map
  - [ ] Remove `store_identifier` set
  - [ ] Remove custom hash functions (keep only entt hashing)
  - [ ] Update `hash<T>()` to use entt directly

#### 0.8: Simplify registration API

- [ ] Consolidate elementary type registration
  - [ ] Fold into unified `register_type<T>()`
  - [ ] Detect elementary via traits
  - [ ] No separate function needed
  - [ ] Users just call `register_type<int>()`

- [ ] Consolidate abstract type registration
  - [ ] Fold into unified `register_type<T>()`
  - [ ] Detect abstract via traits
  - [ ] No separate function needed
  - [ ] Users just call `register_type<AbstractBase>()`

- [ ] Consolidate derived type registration
  - [ ] Fold into unified `register_type<Derived>()`
  - [ ] Or provide `register_derived<Base, Derived>()`
  - [ ] Uses entt's `.base<Base>()` underneath
  - [ ] Simpler than current implementation

- [ ] Result: Reduced API surface
  - [ ] Down from 9+ functions to 3-4 functions
  - [ ] All using entt underneath
  - [ ] Clearer responsibility for each

#### 0.9: Testing and validation

- [ ] Test all type categories with new entt-based system
  - [ ] Elementary types (int, double, string)
  - [ ] Default-constructible types (Point<2>)
  - [ ] Types with constructor args (FE_Q)
  - [ ] Abstract types
  - [ ] Derived types

- [ ] Test method/function registration
  - [ ] Void methods, non-void methods
  - [ ] Const methods, non-const methods
  - [ ] Free functions, lambdas

- [ ] Test JSON generation
  - [ ] Generate registry from entt
  - [ ] Compare to baseline (should match)
  - [ ] Load into node editor (should work)
  - [ ] Verify all metadata present

- [ ] Test polymorphism
  - [ ] Base/derived type handling
  - [ ] Conversion functions
  - [ ] Type compatibility checking

- [ ] Measure improvements
  - [ ] Count lines removed (~500 expected)
  - [ ] Count registration functions removed (6+ expected)
  - [ ] Measure binary size change
  - [ ] Verify no performance regression

#### 0.10: Documentation and migration

- [ ] Document new entt-based architecture
  - [ ] Update architecture diagrams
  - [ ] Document how entt is used
  - [ ] Document property schema
  - [ ] Explain why entt vs custom system

- [ ] Update registration examples
  - [ ] Show new simplified API
  - [ ] Update README.md examples
  - [ ] Update test examples
  - [ ] Update register_types.h

- [ ] Create migration guide
  - [ ] Document API changes
  - [ ] Show before/after examples
  - [ ] Note: JSON format unchanged (backward compatible)
  - [ ] List deprecated functions

- [ ] Update comments in code
  - [ ] Remove outdated comments about custom reflection
  - [ ] Add comments explaining entt usage
  - [ ] Document entt property schema
  - [ ] Update file headers

**Exit criteria**:
- entt is the single source of truth for all type metadata
- Custom `initializers` map removed (~500 lines deleted)
- Registration functions reduced from 9+ to 3-4
- JSON schema generation works from entt metadata
- All tests passing
- No behavioral changes (backward compatible)

---

## Step 1: Preparation and Safety Net

**Goal**: Establish comprehensive test coverage and baseline metrics before refactoring.

### Tasks

- [ ] Audit existing test coverage in `gtests/`
  - [ ] List all existing test files and what they cover
  - [ ] Identify gaps in coverage (especially edge cases)
  - [ ] Document current pass/fail status

- [ ] Create baseline performance benchmarks
  - [ ] Add benchmark for node creation (1000 nodes)
  - [ ] Add benchmark for connection setup (1000 connections)
  - [ ] Add benchmark for graph execution (deep graph, wide graph)
  - [ ] Add benchmark for JSON serialization/deserialization
  - [ ] Record baseline numbers in `benchmarks/baseline.md`

- [ ] Add comprehensive integration tests
  - [ ] Test workflow: register → build graph → serialize → deserialize → execute
  - [ ] Test polymorphic types (base/derived)
  - [ ] Test all connection types (input, output, pass_through, self)
  - [ ] Test error conditions (type mismatches, invalid connections)

- [ ] Document current architecture
  - [ ] Create sequence diagrams for node creation
  - [ ] Create sequence diagrams for connection establishment
  - [ ] Create sequence diagrams for execution
  - [ ] Document invariants (what must always be true)

- [ ] Set up refactoring branch
  - [ ] Create `feature/simplification` branch
  - [ ] Configure CI to run all tests on this branch
  - [ ] Set up draft PR for tracking progress

**Exit criteria**:
- All tests passing with 100% reproducibility
- Baseline benchmarks recorded
- Architecture documented
- Refactoring branch ready

---

## Step 2: Eliminate Double Indirection

**Goal**: Change from `shared_ptr<meta_any>` containing `shared_ptr<T>` to `shared_ptr<meta_any>` containing `T` directly.

**Impact**: Core data structure change. Builds on Step 0's entt integration.

### Tasks

#### 2.1: Prototype the new storage model

- [ ] Create experimental branch `experiment/single-indirection`

- [ ] Define new storage approach
  - [ ] Research `std::any` vs `entt::meta_any` trade-offs
  - [ ] Document decision: which type eraser to use
  - [ ] Create proof-of-concept with 5 simple types

- [ ] Implement basic operations on prototype
  - [ ] Construction from value
  - [ ] Type-safe retrieval
  - [ ] Copy semantics
  - [ ] Move semantics

- [ ] Validate prototype
  - [ ] Create mini test suite (10 tests)
  - [ ] Verify no double allocation (use heap profiler)
  - [ ] Measure memory usage improvement

#### 2.2: Update NodeObject storage

- [ ] Modify `NodeObject` class member
  - [ ] Change declaration from `std::shared_ptr<entt::meta_any> object` (line 1496)
  - [ ] Update to new storage type (decision from 2.1)
  - [ ] Update all constructors to use new storage

- [ ] Update registration executors
  - [ ] Fix `register_elementary_type<T>()` executor (line 632-638)
  - [ ] Fix `register_type<T>()` executor (line 677-683)
  - [ ] Fix `register_type<T, Args...>()` executor (line 773-786)
  - [ ] Fix all method registration executors (4 variants)
  - [ ] Fix all function registration executors (3 variants)

- [ ] Update accessor methods
  - [ ] Rewrite `get_shared<T>()` (line 1162-1212) - remove double cast
  - [ ] Simplify `get<T>()` (line 1218-1224)
  - [ ] Update `ready()` check (line 438-441)
  - [ ] Update `operator=` overloads (line 1385-1414)

#### 2.3: Update type conversion and polymorphism

- [ ] Fix base class conversion
  - [ ] Update `to_base` lambda in `register_derived_type` (line 739-752)
  - [ ] Simplify conversion logic in `get_shared<T>()` base case
  - [ ] Remove unnecessary meta_any wrapping/unwrapping

- [ ] Update cast operations
  - [ ] Fix `cast_args<Args...>()` implementation (line 1752-1769)
  - [ ] Update all call sites that expect double indirection
  - [ ] Search for `try_cast<std::shared_ptr<T>>()` and update to `try_cast<T>()`

#### 2.4: Update serialization

- [ ] Fix JSON serialization
  - [ ] Update `to_string()` implementation (line 456-469)
  - [ ] Update `parse_string()` implementation (line 444-453)
  - [ ] Update `from_json()` deserialization (line 1704-1747)

- [ ] Update NodeObjectInitializer functions
  - [ ] Fix `parse_string` lambdas (return type change)
  - [ ] Fix `to_string` lambdas (parameter type change)
  - [ ] Fix `to_base` lambdas (parameter type change)
  - [ ] Fix `executor` lambdas (return type change)

#### 2.5: Testing and validation

- [ ] Run existing test suite
  - [ ] Fix failing tests one by one
  - [ ] Document any behavioral changes
  - [ ] Verify all tests pass

- [ ] Run benchmark suite
  - [ ] Compare memory usage (expect 25-30% reduction)
  - [ ] Compare performance (expect 5-10% improvement)
  - [ ] Document results

- [ ] Test edge cases
  - [ ] Large objects (>1MB)
  - [ ] Many small objects (>10k nodes)
  - [ ] Deeply nested graphs (depth >100)
  - [ ] Circular references (should fail gracefully)

**Exit criteria**:
- All tests passing
- Memory usage reduced by 25-30%
- No performance regression
- Code compiles without warnings

---

## Step 3: Introduce First-Class Port Objects

**Goal**: Replace index-based port access with direct `Port` objects.

**Impact**: Simplifies connection logic, removes synchronization burden, improves code clarity.

### Tasks

#### 3.1: Design Port abstraction

- [ ] Define `Port` struct
  ```cpp
  struct Port {
    std::string name;
    std::string type_hash;
    ConnectionType connection_type;
    std::shared_ptr<meta_any> value;  // Direct pointer to value
  };
  ```

- [ ] Design Port API
  - [ ] `connect(Port& source, Port& target)` function
  - [ ] `is_connected() const` method
  - [ ] `get_value()` / `set_value()` methods
  - [ ] `is_compatible(const Port& other)` type checking

- [ ] Write Port unit tests
  - [ ] Test construction
  - [ ] Test type compatibility checking
  - [ ] Test value access
  - [ ] Test connection semantics

#### 3.2: Refactor NodeObject to use Ports

- [ ] Add Port vectors to NodeObject
  - [ ] Add `std::vector<Port> inputs;` member
  - [ ] Add `std::vector<Port> outputs;` member
  - [ ] Keep `arguments` temporarily for migration

- [ ] Update registration to create Ports
  - [ ] Modify `register_json_header` to populate input/output Ports
  - [ ] Update `initialize_inputs()` to create Port objects (line 1570-1578)
  - [ ] Update `initialize_outputs()` to create Port objects (line 1580-1606)
  - [ ] Update `initialize_arguments()` to create Port objects (line 1608-1622)

- [ ] Update port access methods
  - [ ] Rewrite `input(unsigned int index)` to return `Port&` (line 1278-1299)
  - [ ] Rewrite `output(unsigned int index)` to return `Port&` (line 1249-1259)
  - [ ] Rewrite `set_input()` to work with Ports (line 1302-1322)
  - [ ] Rewrite `set_output()` to work with Ports (line 1264-1273)

#### 3.3: Update connection logic

- [ ] Update `set_inputs()` method (line 1625-1660)
  - [ ] Change to use Port objects directly
  - [ ] Simplify type checking using Port::is_compatible()
  - [ ] Remove index translation logic

- [ ] Update `set_outputs()` method (line 1662-1689)
  - [ ] Change to use Port objects directly
  - [ ] Remove index translation logic

- [ ] Update `set_arguments()` method (line 510-523)
  - [ ] Adapt to work with new Port-based system
  - [ ] Or deprecate if no longer needed

#### 3.4: Remove old index-based system

- [ ] Remove obsolete members
  - [ ] Delete `std::vector<int> input_indices;` (line 1561)
  - [ ] Delete `std::vector<int> output_indices;` (line 1567)
  - [ ] Delete `std::vector<ConnectionType> arguments_types;` (line 1512)
  - [ ] Migrate `arguments` to be derived from ports if still needed

- [ ] Remove special case handling
  - [ ] Remove `-1` sentinel value checks
  - [ ] Remove index synchronization logic
  - [ ] Simplify error messages (no longer need to explain indices)

- [ ] Update JSON serialization
  - [ ] Change JSON format to use Port names instead of indices
  - [ ] Add backward compatibility layer for old JSON format
  - [ ] Document JSON format change in migration guide

#### 3.5: Update Network class

- [ ] Update `Connection` class (coral_network.h:21-83)
  - [ ] Change to reference Ports by name instead of index
  - [ ] Or keep index-based for JSON compatibility, translate to Ports internally

- [ ] Update `add_connection()` method (coral_network.h:146-212)
  - [ ] Use new Port-based connection API
  - [ ] Simplify type checking logic

- [ ] Update connection validation
  - [ ] Use Port::is_compatible() for type checking
  - [ ] Simplify error messages

#### 3.6: Testing and validation

- [ ] Update existing tests
  - [ ] Fix all tests that use index-based port access
  - [ ] Update to use Port objects
  - [ ] Verify behavior unchanged

- [ ] Add new Port-specific tests
  - [ ] Test Port connection semantics
  - [ ] Test Port type compatibility
  - [ ] Test Port value propagation
  - [ ] Test error conditions

- [ ] Test JSON backward compatibility
  - [ ] Load old JSON files
  - [ ] Verify they still work
  - [ ] Save and verify new format

**Exit criteria**:
- All tests passing
- Index vectors removed
- Code is more readable (subjective but measurable via review)
- JSON backward compatibility maintained

---

## Step 4: Consolidate Registration Functions (Mostly completed by Step 0)

**Goal**: Review and polish the registration API after Step 0's entt integration.

**Impact**: Minimal - most work done in Step 0. This is primarily cleanup and documentation.

**Note**: Step 0 achieves the core goal of using entt and consolidating registration. This step ensures completeness and handles any edge cases.

### Tasks

#### 4.1: Review registration API after Step 0

- [ ] Audit the new registration API from Step 0
  - [ ] List all remaining registration functions
  - [ ] Verify they're all necessary
  - [ ] Check for any remaining duplication
  - [ ] Document why each exists

- [ ] Check for edge cases
  - [ ] Are there any types that don't fit the new API?
  - [ ] Are there any special cases still needed?
  - [ ] Can any remaining functions be merged?

#### 4.2: Polish registration API

- [ ] Simplify function signatures if possible
  - [ ] Check if any parameters can have better defaults
  - [ ] Check if any overloads can be eliminated
  - [ ] Ensure consistent naming

- [ ] Improve error messages
  - [ ] Update error messages to reference entt
  - [ ] Make error messages more helpful
  - [ ] Add suggestions for common mistakes

- [ ] Add convenience helpers
  - [ ] Helper for bulk registration (register multiple types at once)
  - [ ] Helper for registering type hierarchies
  - [ ] Consider fluent API if beneficial

#### 4.3: Testing and validation (lightweight)

- [ ] Run all existing tests
  - [ ] Verify all pass with new API
  - [ ] Check test code quality (is it cleaner?)

- [ ] Test edge cases discovered during Step 0
  - [ ] Any types that were problematic?
  - [ ] Any registration patterns that felt awkward?

- [ ] Measure final improvements
  - [ ] Count final registration functions (target: 3-4)
  - [ ] Compare API surface to original
  - [ ] Document simplification

#### 4.4: Documentation

- [ ] Document final registration API
  - [ ] Update API docs
  - [ ] Add examples for each function
  - [ ] Document best practices

- [ ] Create registration guide
  - [ ] How to register different type categories
  - [ ] How to handle special cases
  - [ ] Common patterns and idioms

**Exit criteria**:
- Registration API polished and well-documented
- No remaining duplication or redundancy
- All tests passing
- User-facing API is clean and consistent
- (Note: Most code reduction already achieved in Step 0)

---

## Step 5: Remove Elementary Type Special Cases

**Goal**: Treat all types uniformly, eliminate branching on `is_registered_elementary_type<T>()`.

**Impact**: Cleaner code paths, fewer branches, more uniform behavior.

### Tasks

#### 5.1: Identify all special cases

- [ ] Search codebase for `is_registered_elementary_type`
  - [ ] List all call sites
  - [ ] Document what each special case does
  - [ ] Categorize by type of special handling

- [ ] Document current special case behavior
  - [ ] Auto-construction of elementary outputs (lines 916-919, 1013-1016, 1106-1109)
  - [ ] Special JSON serialization for strings (lines 602-609, 618-626)
  - [ ] Output-only port generation (lines 1531-1556)

- [ ] Design uniform interface
  - [ ] What should ALL types support?
  - [ ] How to handle serialization uniformly?
  - [ ] How to handle construction uniformly?

#### 5.2: Unify serialization interface

- [ ] Define Serializable concept
  ```cpp
  template<typename T>
  concept Serializable = requires(T t) {
    { to_json(t) } -> std::convertible_to<json>;
    { from_json(json{}) } -> std::convertible_to<T>;
  };
  ```

- [ ] Implement for elementary types
  - [ ] int, double, float: use JSON native support
  - [ ] std::string: direct JSON string
  - [ ] bool: JSON boolean

- [ ] Implement for deal.II types
  - [ ] Point<dim>: already has JSON serialization
  - [ ] Triangulation: serialize to mesh format
  - [ ] Others: document serialization format

- [ ] Update parse_string/to_string
  - [ ] Make available for all types with Serializable concept
  - [ ] Remove special case checking
  - [ ] Use SFINAE or concepts to enable

#### 5.3: Unify construction interface

- [ ] Remove auto-construction special case
  - [ ] Lines 916-919: Remove `if (output_is_elementary)` check in register_method
  - [ ] Lines 1013-1016: Remove from const method registration
  - [ ] Lines 1106-1109: Remove from function registration

- [ ] Require explicit output node construction
  - [ ] Update all tests to explicitly construct output nodes
  - [ ] Update all examples to show explicit construction
  - [ ] Document this requirement

- [ ] Or implement uniform auto-construction
  - [ ] Auto-construct ALL types when used as outputs
  - [ ] Not just elementary types
  - [ ] Use default constructor or throw clear error

#### 5.4: Unify output port handling

- [ ] Remove `set_output_only()` function (lines 1531-1556)
  - [ ] This modifies JSON based on elementary type check
  - [ ] Replace with uniform port creation logic

- [ ] Update port creation
  - [ ] All output ports created the same way
  - [ ] No special handling for elementary types
  - [ ] Simplify JSON serialization format

- [ ] Update registration functions
  - [ ] Remove calls to `set_output_only()`
  - [ ] Remove `output_is_elementary` variables
  - [ ] Simplify control flow

#### 5.5: Remove string special cases

- [ ] Unify string handling in register_elementary_type (lines 602-609)
  - [ ] Use same JSON format as other types
  - [ ] Remove `if constexpr (std::is_same_v<T, std::string>)` branches
  - [ ] Use JSON string directly for all string operations

- [ ] Unify string parsing (lines 618-626)
  - [ ] Remove separate code path for strings
  - [ ] Use JSON parsing uniformly
  - [ ] May need custom JSON converter for std::string

- [ ] Test string edge cases
  - [ ] Empty strings
  - [ ] Strings with special characters (quotes, newlines)
  - [ ] Very long strings (>10k chars)
  - [ ] Unicode strings

#### 5.6: Clean up NodeType enum

- [ ] Review NodeType enum (lines 236-259)
  - [ ] Is `elementary_constructor` still needed?
  - [ ] Can we merge some categories?
  - [ ] Simplify to fewer categories

- [ ] Update node_type() checks
  - [ ] Remove checks for elementary_constructor
  - [ ] Merge with empty_constructor if possible
  - [ ] Simplify control flow

- [ ] Update JSON node_type field
  - [ ] Ensure backward compatibility
  - [ ] Document any format changes
  - [ ] Add migration support if needed

#### 5.7: Testing and validation

- [ ] Test all type categories with new uniform interface
  - [ ] Elementary types still work
  - [ ] Complex types work the same way
  - [ ] No behavior changes

- [ ] Test edge cases
  - [ ] Types that were previously elementary
  - [ ] Types that were previously special-cased
  - [ ] String edge cases

- [ ] Measure code reduction
  - [ ] Count branches removed
  - [ ] Count lines removed
  - [ ] Document simplification

- [ ] Run full benchmark suite
  - [ ] Verify no performance regression
  - [ ] Document any improvements
  - [ ] Compare to baseline from Step 1

**Exit criteria**:
- No more `is_registered_elementary_type` calls
- All types use uniform interface
- All tests passing
- Code reduced by 100-150 lines

---

## Step 6: Final Integration and Cleanup

**Goal**: Integrate all changes, clean up, optimize, and prepare for merge.

### Tasks

#### 6.1: Code cleanup

- [ ] Remove dead code
  - [ ] Search for unused functions
  - [ ] Remove commented-out code
  - [ ] Remove unused includes

- [ ] Improve code organization
  - [ ] Group related functions
  - [ ] Add section comments
  - [ ] Reorder for logical flow

- [ ] Apply consistent formatting
  - [ ] Run clang-format
  - [ ] Fix any style inconsistencies
  - [ ] Update copyright headers if needed

#### 6.2: Documentation updates

- [ ] Update README.md
  - [ ] Document new simplified API
  - [ ] Update examples
  - [ ] Add migration guide section

- [ ] Update code comments
  - [ ] Update outdated comments
  - [ ] Add comments for complex sections
  - [ ] Document design decisions

- [ ] Create migration guide
  - [ ] Document all breaking changes
  - [ ] Provide before/after examples
  - [ ] List deprecated functions

- [ ] Update Doxygen documentation
  - [ ] Regenerate API docs
  - [ ] Fix any warnings
  - [ ] Update class diagrams if they exist

#### 6.3: Performance optimization

- [ ] Profile hot paths
  - [ ] Node creation
  - [ ] Connection establishment
  - [ ] Graph execution
  - [ ] JSON serialization

- [ ] Optimize based on profiling
  - [ ] Reduce allocations
  - [ ] Improve cache locality
  - [ ] Consider move semantics

- [ ] Re-run benchmarks
  - [ ] Compare to baseline
  - [ ] Document improvements
  - [ ] Investigate any regressions

#### 6.4: Final testing

- [ ] Run complete test suite
  - [ ] All unit tests
  - [ ] All integration tests
  - [ ] All benchmarks

- [ ] Test on different platforms
  - [ ] Linux (GCC, Clang)
  - [ ] macOS (Clang)
  - [ ] Windows (MSVC) if applicable

- [ ] Test with different deal.II versions
  - [ ] Minimum supported version
  - [ ] Latest stable version

- [ ] Test with sanitizers
  - [ ] AddressSanitizer (memory errors)
  - [ ] UndefinedBehaviorSanitizer
  - [ ] ThreadSanitizer if parallel execution

#### 6.5: Measure success

- [ ] Code metrics
  - [ ] Lines of code in coral.h (target: 1000-1200 from 1800)
  - [ ] Number of functions (target: 30% reduction)
  - [ ] Cyclomatic complexity (should decrease)

- [ ] Performance metrics
  - [ ] Memory usage per node (target: 25-30% reduction)
  - [ ] Node creation time (target: 5-10% improvement)
  - [ ] Graph execution time (target: no regression)

- [ ] Quality metrics
  - [ ] Test coverage (maintain or improve)
  - [ ] Compiler warnings (should be 0)
  - [ ] Static analysis warnings (should decrease)

#### 6.6: Prepare for merge

- [ ] Rebase on main
  - [ ] Resolve any conflicts
  - [ ] Ensure tests still pass
  - [ ] Update changelog

- [ ] Create pull request
  - [ ] Write comprehensive description
  - [ ] Link to this plan document
  - [ ] Include before/after metrics

- [ ] Request reviews
  - [ ] Tag relevant reviewers
  - [ ] Address review comments
  - [ ] Update documentation as needed

- [ ] Plan merge strategy
  - [ ] Squash commits vs preserve history
  - [ ] Coordinate with other ongoing work
  - [ ] Plan rollout communication

**Exit criteria**:
- All goals achieved (see metrics above)
- All tests passing on all platforms
- Documentation complete
- PR approved and ready to merge

---

## Risk Management

### High Risk Items

1. **entt integration** (Step 0)
   - **Risk**: Fundamental change to reflection system, could break type registration
   - **Mitigation**: Extensive prototype phase, maintain parallel systems temporarily, comprehensive testing

2. **Double indirection removal** (Step 2)
   - **Risk**: Could break many things in subtle ways
   - **Mitigation**: Extensive testing, prototype first, incremental migration

3. **Port object refactoring** (Step 3)
   - **Risk**: Complex refactoring touching many files
   - **Mitigation**: Keep both systems temporarily, migrate incrementally

4. **JSON format changes** (Step 3)
   - **Risk**: Break existing workflows
   - **Mitigation**: Maintain backward compatibility, version JSON format

### Medium Risk Items

5. **Registration consolidation** (Step 4)
   - **Risk**: API breaking changes for users (largely mitigated by Step 0)
   - **Mitigation**: Deprecation period, compatibility layer, clear migration guide

6. **Elementary type unification** (Step 5)
   - **Risk**: Behavior changes for edge cases
   - **Mitigation**: Comprehensive testing, document behavior changes

### Rollback Plan

For each step:
- Commit regularly with clear messages
- Tag before major changes
- Keep feature flag for new behavior where possible
- Document rollback procedure

If critical issues found:
1. Revert to last known good state
2. Fix issue in isolation
3. Re-apply changes incrementally
4. Add tests to prevent regression

---

## Success Criteria

The refactoring is successful when:

- ✅ All tests passing (including new tests added)
- ✅ entt is the single source of truth for type metadata (no parallel reflection system)
- ✅ Code size reduced by 40-50% in coral.h (including ~500 lines from entt integration)
- ✅ Registration functions reduced from 9+ to 3-4
- ✅ Memory usage per node reduced by 25-30%
- ✅ No performance regression (ideally 5-10% improvement)
- ✅ API is simpler and more consistent
- ✅ Code is more maintainable (fewer special cases, less duplication)
- ✅ Documentation is complete and up-to-date
- ✅ Backward compatibility maintained for JSON format
- ✅ Migration path clear for API users

---

## Timeline Estimate

| Step | Duration | Dependencies |
|------|----------|--------------|
| Step 0: entt integration | 7-10 days | Step 1 |
| Step 1: Preparation | 3-5 days | None |
| Step 2: Double indirection | 5-7 days | Steps 0, 1 |
| Step 3: Port objects | 7-10 days | Step 2 |
| Step 4: Registration cleanup | 2-3 days | Step 0 (mostly done by Step 0) |
| Step 5: Elementary types | 3-5 days | Steps 0, 2 |
| Step 6: Integration | 3-5 days | Steps 0, 2, 3, 4, 5 |

**Total: 30-45 days (5-7 weeks)**

This assumes:
- One person working full-time
- No major blocking issues
- Reasonable review turnaround time

Can be parallelized:
- Step 0 is foundational, must complete first
- Steps 2 and 3 can be started in parallel after Step 0
- Step 4 is minimal work after Step 0
- Step 5 can start once Steps 0 and 2 are partially complete

---

## Notes

- This plan is a living document - update as you progress
- Check off tasks as completed
- Document deviations from the plan and reasons
- Add notes for future refactoring opportunities discovered
- Keep stakeholders informed of progress
