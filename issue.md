This is an header-only library.
In this repo all critical code is in `include/*.h` (no subfolders).
The main is in `source/main.cc`.

## Context

### Project Overview
CORAL (Computational Object-oriented Representation And Library) is a C++ library for building and executing computational graphs. It wraps C++ types as graph nodes with typed inputs/outputs, enabling workflows to be defined as JSON and executed with automatic parallelization via Taskflow.

### Directory Structure
```
include/           # Header-only library core
  coral.h          # NodeObject class, type registration, connections (~1800 lines)
  coral_network.h  # Network class, graph execution, JSON serialization (~545 lines)
  register_types.h # deal.II specific type registrations
  type_name.h      # Type name demangling utility
  utils.h          # JSON validation helpers
source/main.cc     # CLI entry point (register/run subcommands)
gtests/            # Google Test unit tests
test_files/        # Example JSON workflow files
entt/              # Embedded dependency (metaprogramming)
```

### Core Components

**NodeObject** (`coral.h`): Central abstraction wrapping any C++ type.
- Uses `entt::meta_any` for type erasure
- Supports lazy evaluation via `operator()`
- Manages typed inputs/outputs for connections
- Node types: constructor, method, function, pass_through, abstract

**Network** (`coral_network.h`): Manages node collections and connections.
- Builds Taskflow execution graph from connections
- JSON serialization/deserialization of workflows
- Graphviz DOT output for visualization

**Type Registration System**: Manual reflection without compiler support.
- `register_elementary_type<T>()` - trivially copyable types
- `register_type<T>()` - non-trivial but default-constructible
- `register_abstract_type<T>()` - interfaces
- `register_derived_type<Base, Derived>()` - inheritance
- `register_method()` / `register_function()` - callables

### Dependencies
- **deal.II 9.5+**: FEM library (optional)
- **Taskflow**: Parallel task execution
- **nlohmann/json**: JSON handling
- **entt**: Metaprogramming utilities
- **CLI11**: Command-line parsing

### Workflow Pattern
1. Register types → export JSON registry
2. Frontend creates nodes/connections from registry
3. Export workflow as JSON
4. Backend loads JSON → builds Network → executes via Taskflow

---

## Simplification

### Analysis Summary
The codebase exhibits significant complexity, primarily stemming from building a runtime reflection system in C++ without language-level support. While some complexity is inherent to the problem domain, several architectural decisions introduce unnecessary indirection and special-case handling that could be simplified.

### Core Complexity Sources

#### 1. **Double Indirection in Type Storage** (`coral.h:1496`)
```cpp
std::shared_ptr<entt::meta_any> object;  // where meta_any contains std::shared_ptr<T>
```
**Issue**: Every value undergoes double heap allocation and double pointer indirection.
- First `shared_ptr` wraps the `meta_any` type-erased container
- Second `shared_ptr<T>` is stored inside `meta_any` itself

**Stated rationale** (coral.h:338): "to allow for serialization and to build non trivially constructible classes"

**Critique**: This justification is weak. Serialization doesn't require double indirection (could serialize `meta_any` directly). Non-trivial construction could be handled with placement new or deferred initialization patterns. The double indirection costs memory and cache performance for every single node.

**Alternative**: Single `std::shared_ptr<meta_any>` containing values directly, or use `std::any` with custom serialization hooks.

#### 2. **Explosion of Registration Functions**
The type registration system has 9 different registration functions with overlapping responsibilities:
- `register_elementary_type<T>()`
- `register_type<T>()`
- `register_type<T, Args...>(names)`
- `register_abstract_type<T>()`
- `register_derived_type<Base, T>()`
- `register_derived_type<Base, T, Args...>(names)`
- `register_method()` (4 overloads: void/non-void × const/non-const)
- `register_function()` (3 overloads: void/non-void, lambda/function pointer)

**Issue**: Each function duplicates similar logic with slight variations. Special cases include:
- Elementary types get `parse_string` / `to_string` functions
- Non-void functions need explicit output arguments (coral.h:916-919, 1100-1109)
- Derived types need `to_base` conversion lambdas (coral.h:739-752)
- String types have special JSON handling (coral.h:602-605, 618-621)

**Root cause**: Attempting to handle all C++ type categories through template specialization rather than a unified registration interface.

**Alternative**: Code generation from C++ headers using clang-libtooling. Generate registration code automatically, eliminating manual registration entirely.

#### 3. **Complex Input/Output Indexing** (`coral.h:1561-1606`)
Each `NodeObject` maintains three synchronized vectors:
```cpp
std::vector<NodeObjectPtr> arguments;        // Actual values
std::vector<ConnectionType> arguments_types; // Input/output/pass-through flags
std::vector<int> input_indices;              // Maps input port → argument index
std::vector<int> output_indices;             // Maps output port → argument index (or -1 for self)
```

**Issue**: Synchronization burden. Special value `-1` means "return self". Requires careful bookkeeping in `initialize_inputs()`, `initialize_outputs()`, `initialize_arguments()`.

**Consequence**: Simple operations become complex:
- `set_input()`: coral.h:1302-1322 (21 lines for bounds checking + special case handling)
- `input()`: coral.h:1278-1299 (22 lines with verbose error messages)

**Alternative**: First-class `Port` objects with direct pointers:
```cpp
struct Port { NodeObjectPtr node; std::string name; std::shared_ptr<meta_any> value; };
std::vector<Port> inputs;
std::vector<Port> outputs;
```
Eliminates indirection, makes code self-documenting, removes synchronization burden.

#### 4. **Custom Hash-Based Type System** (`coral.h:88-159`)
Instead of using `std::type_info` directly, implements custom hash strings:
- Type alias map for canonical names (coral.h:51-55)
- String identifier storage set (coral.h:58-62)
- Suffix handling for functions (coral.h:114-120)
- Hash collision handling via string prefix matching (coral.h:1396-1402)

**Issue**: Reinvents C++ RTTI poorly. `std::type_info::name()` mangling is compiler-specific, but this system creates its own fragile string matching that's equally non-portable.

**Example brittleness** (coral.h:1438):
```cpp
if (stored_hash.find(object_hash) != 0)  // String prefix matching for type compatibility!?
  throw std::runtime_error(...);
```

**Alternative**: Accept `std::type_index` as the key, use Boost.TypeName for display only. Let compiler handle type identity.

#### 5. **Polymorphism Workaround** (`coral.h:710-753`)
Custom base class conversion system instead of using C++ native polymorphism:
```cpp
std::function<std::shared_ptr<entt::meta_any>(std::shared_ptr<entt::meta_any>)> to_base;
```

**Issue**: The `get_shared<T>()` function (coral.h:1162-1212) has 50+ lines just to handle base class conversion. It manually checks if types match, tries custom conversion, validates hashes post-conversion.

**Root cause**: `meta_any` strips polymorphic type information. Must reconstruct it manually.

**Alternative**: Store `std::shared_ptr<Base>` directly where needed. Use `std::static_pointer_cast` / `std::dynamic_pointer_cast` natively.

#### 6. **Special Case Explosion**
Elementary types require special handling throughout the codebase:

| Location | Special Case |
|----------|-------------|
| coral.h:916-919 | Non-void methods check if output is elementary, auto-construct if needed |
| coral.h:1013-1016 | Const methods duplicate this check |
| coral.h:1106-1109 | Functions duplicate this check again |
| coral.h:1531-1556 | `set_output_only()` modifies JSON for elementary outputs |
| coral.h:602-609 | String gets special JSON value vs other elementary types |
| coral.h:618-626 | String parsing is different from JSON parsing |

**Issue**: No unified interface. Each node type must explicitly check `is_registered_elementary_type<T>()` and branch.

**Alternative**: Uniform value interface. All types support the same operations (construct, copy, serialize). Elementary types are not special; they're just types that implement a `Serializable` concept.

### Corner Cases and Technical Debt

1. **String Parsing Ambiguity** (coral.h:403-408): If a hash isn't found, assumes it's a string literal. This makes debugging failures confusing—typos in type names silently become string nodes.

2. **Static Registry Lifetime**: `static inline std::map<string, NodeObjectInitializer> initializers` (coral.h:1517). Initialization order fiasco potential. No clear ownership or cleanup.

3. **Verbose Error Handling**: Functions like `input()` (coral.h:1278-1299) have 4 runtime checks with hand-crafted error messages. Could use assertions for internal invariants vs user errors.

4. **Redundant Constructors**: 5 different `NodeObject` constructors (coral.h:349-419) with overlapping logic. Routes through `try-catch` for type lookup (coral.h:399-412).

5. **JSON Serialization Mixed with Logic**: `NodeObjectInitializer::json_serializer` is both metadata storage AND mutable state (coral.h:330, 1420-1422). Violates separation of concerns.

### Alternative Architectural Approaches

#### Option A: Code Generation
**Approach**: Use clang-libtooling to parse C++ headers and auto-generate registration code.

**Benefits**:
- Eliminates ~500 lines of manual registration boilerplate
- No risk of registration mismatches
- Can infer connection types from function signatures
- Can generate optimal code paths (no runtime type checking)

**Cost**: Build complexity (need compilation step)

#### Option B: Constrained Type Universe
**Approach**: Limit to a fixed set of types, use `std::variant<Types...>` instead of `meta_any`.

**Benefits**:
- Compile-time type safety via `std::visit`
- No double indirection
- ~50% reduction in code size (eliminate reflection system)
- Better compiler optimizations

**Cost**: Lose ability to add types without recompilation

#### Option C: Entity-Component-System (ECS) via entt
**Approach**: Since entt is already a dependency, use it fully:
- Nodes are entities (integer IDs)
- `ValueComponent<T>` for storage
- `ConnectionComponent` for edges
- Systems for execution

**Benefits**:
- Leverages entt's optimized sparse sets
- Natural support for node metadata
- Efficient iteration patterns
- Reduces custom data structures

**Cost**: Different mental model from current object-oriented design

#### Option D: Simplified Value Representation
**Approach**:
```cpp
struct NodeObject {
  std::any value;  // Single indirection
  std::function<void()> execute;
  std::vector<NodeObject*> inputs;
  std::vector<NodeObject*> outputs;
};
```

**Benefits**:
- ~80% code reduction
- Single indirection (`std::any` contains value directly)
- No complex registration system
- Easier to understand and maintain

**Cost**: Lose some type safety (but JSON boundary already loses it anyway)

### Concrete Simplification Recommendations

#### High Priority (Large Impact, Low Risk)
1. **Eliminate double indirection**: Change to `std::any` containing values directly
2. **Direct port connections**: Replace index vectors with first-class `Port` objects
3. **Consolidate registration functions**: Single `register_type<T>()` with trait-based dispatch
4. **Remove elementary type special cases**: Uniform value interface

#### Medium Priority (Moderate Impact)
5. **Code generation for registration**: Automate boilerplate, reduce manual errors
6. **Simplify error handling**: Use assertions for internal invariants, exceptions for user errors
7. **Separate JSON schema from runtime state**: `NodeTypeInfo` struct vs `NodeObjectInitializer`

#### Low Priority (Nice to Have)
8. **Replace custom hash system**: Use `std::type_index` as canonical key
9. **Native polymorphism**: Store `shared_ptr<Base>` where appropriate
10. **ECS exploration**: Evaluate entt-based redesign for v2.0

### Trade-offs and Constraints

**Constraint 1: No C++ Reflection**
C++26 will add reflection. The current system is a stopgap. Consider waiting vs continued investment in manual reflection.

**Constraint 2: JSON Serialization**
Requires runtime type information. However, doesn't require double indirection—custom serialization hooks suffice.

**Constraint 3: deal.II Integration**
Large, complex types (Triangulation, DoFHandler) drive non-trivial constructor handling. But these are the minority; most nodes are simple functions.

**Question: Is the complexity justified?**
The answer is: **partially**. Building runtime reflection in C++ is inherently complex. However, ~40% of the complexity stems from architectural choices (double indirection, index-based ports, type hash system) rather than fundamental requirements. A redesign could cut `coral.h` from 1800 lines to ~800-1000 lines while preserving all functionality.

### Conclusion

The codebase is more complex than necessary. The core insight—wrap C++ values as nodes for graph execution—is sound. But the implementation accretes complexity through:
- Defensive over-engineering (double indirection "just in case")
- Special case proliferation (elementary types need different code paths)
- Reinventing language features (custom reflection, polymorphism)

A focused simplification effort targeting the high-priority items could cut complexity by 30-40% while improving performance (fewer indirections, better cache locality) and maintainability (fewer special cases, clearer code).
