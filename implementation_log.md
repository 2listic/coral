# Implementation Log: Functions as First-Class Citizens

This document tracks the implementation of higher-order functions in CORAL, enabling `std::function` values to be created from registered functions/methods/networks and passed between nodes.

## Overview

**Goal**: Enable functions to be passed as values between nodes, allowing higher-order functions like `map`, `reduce`, `filter`, and `apply` within the CORAL computational graph system.

**Approach**: Use `std::function<R(Args...)>` as the type for function values, with specialized registration and constructor nodes.

---

## Phase 1: Foundation - std::function Type Support

**Status**: ✅ Completed
**Date**: 2026-02-04

### Objective
Ensure `std::function` types can be registered, stored, and passed between nodes as values.

### Problem Identified
The existing `register_elementary_type<T>()` method attempts to serialize types to JSON using:
- `json(T()).dump()` - Serialize default-constructed value
- `json::parse(value).get<T>()` - Deserialize from JSON string

This fails for `std::function` because:
1. Function pointers and closures cannot be serialized to JSON
2. They are runtime entities with no textual representation
3. nlohmann::json has no built-in support for function serialization

### Solution Implemented

#### 1. Added `register_function_type<R, Args...>()` Helper
**File**: `include/coral.h` (after line 733)

Created a specialized registration method for `std::function<R(Args...)>` types:

```cpp
template <typename R, typename... Args>
static auto
register_function_type() -> detail::NodeObjectInitializer &
{
  using FunctionType = std::function<R(Args...)>;
  auto &initializer = register_json_header<FunctionType>();
  initializer.json_serializer["node_type"] = "empty_constructor";
  initializer.node_type                    = NodeType::empty_constructor;
  initializer.json_serializer["outputs"].push_back(-1);

  // Add executor that creates empty std::function
  initializer.executor = [](const NodeObjectPtr &,
                            const std::vector<NodeObjectPtr> &args)
    -> std::shared_ptr<entt::meta_any> {
    if (args.size() != 0)
      throw std::runtime_error("Wrong number of arguments.");
    return std::make_shared<entt::meta_any>(
      std::make_shared<FunctionType>());
  };

  // Note: parse_string and to_string intentionally not set
  // (functions cannot be serialized to/from JSON)
  return initializer;
}
```

**Key Design Decisions**:
- Uses `NodeType::empty_constructor` (not `elementary_constructor`)
- No `parse_string` or `to_string` functions (functions aren't JSON-serializable)
- Creates empty `std::function` by default (will be populated later)
- Still uses type aliases for clean naming

#### 2. Created Comprehensive Test Suite
**File**: `gtests/function_types.cc` (new file)

Implemented 4 unit tests serving as both validation and documentation:

##### Test 1.1: `FunctionType.Registration`
**Purpose**: Verify `std::function` types can be registered and appear in the node registry

**Key Points**:
- Uses `register_function_type<double, double>()` to register `std::function<double(double)>`
- Sets clean type alias: `"std::function<double(double)>"`
- Verifies registry contains the type with correct metadata
- Checks `node_type` is `"empty_constructor"`

**MWE**: Shows how to register a function type with clean naming

##### Test 1.2: `FunctionType.CreateNodeWithStdFunction`
**Purpose**: Store a function in a node

**Key Points**:
- Creates lambda: `auto square = [](double x) { return x * x; }`
- Wraps in `std::function<double(double)>`
- Creates `NodeObjectPtr` containing the function
- Verifies node is ready and contains correct type

**MWE**: Basic function storage in a node

##### Test 1.3: `FunctionType.RetrieveAndInvoke`
**Purpose**: Extract and call a function from a node

**Key Points**:
- Stores `std::function<double(double)>` (square) in a node
- Retrieves using `node->get<FunctionType>()`
- Invokes retrieved function: `retrieved_fn(5.0)`
- Verifies result: `25.0`

**MWE**: Function retrieval and invocation workflow

##### Test 1.4: `FunctionType.PassBetweenNodes`
**Purpose**: Data flow of functions through graph

**Key Points**:
- Node A: Contains `std::function<double(double)>` (square function)
- Node B: Evaluator function that takes `std::function` and value as inputs
- Registered evaluator: `void(FunctionType&, const double&, double&)`
- Connects nodes: A's output → B's function input, value node → B's value input
- Executes network and verifies result

**MWE**: End-to-end function passing through computational graph

### Files Modified

1. **`include/coral.h`**
   - Added `register_function_type<R, Args...>()` method (30 lines)
   - Location: After `register_abstract_type()` at line ~734

2. **`gtests/function_types.cc`** (new file)
   - Created complete test suite for Phase 1 (159 lines)
   - 4 tests covering registration, storage, retrieval, and graph passing

3. **`gtests/CMakeLists.txt`**
   - No changes needed (uses `file(GLOB *cc)` to auto-discover tests)

### Compilation Issues Encountered

#### Issue 1: JSON Serialization Errors
**Error**: `no matching function for call to 'nlohmann::json_abi_diag_v3_11_3::basic_json<>::basic_json(std::function<double(double)>)'`

**Root Cause**: `register_elementary_type<T>()` attempts `json(T()).dump()` which fails for `std::function`

**Resolution**: Created specialized `register_function_type<R, Args...>()` that skips JSON serialization

#### Issue 2: Type Name Resolution
**Error**: Substring search for "function" in registry unreliable due to compiler name mangling

**Root Cause**: `entt::type_id<T>().name()` produces compiler-specific mangled names

**Resolution**:
- Use `detail::set_type_alias<T>("clean_name")` for readable names
- Check exact hash match using `detail::hash<FunctionType>()`

#### Issue 3: Node Creation from Lambda Type
**Error**: Test 1.4 tried to create node from `decltype(evaluator)` (lambda type)

**Root Cause**: Cannot create node from unregistered lambda type

**Resolution**: Create node from registered function name string: `make_node("evaluator")`

### Success Criteria Met

✅ All 4 unit tests pass
✅ Can register `std::function` types with clean naming
✅ Can create nodes containing `std::function` values
✅ Can retrieve and invoke functions from nodes
✅ Can pass `std::function` values between nodes through connections
✅ Network execution works with function-valued nodes

### Technical Notes

1. **Type Aliasing**: Always use `detail::set_type_alias<T>("name")` for `std::function` types to ensure readable registry entries

2. **No JSON Serialization**: `std::function` values exist only at runtime and cannot be serialized to configuration files (this will be addressed in later phases with function constructors)

3. **Empty Default Construction**: Registered `std::function` types default to empty functions; actual function values must be assigned after construction

4. **Memory Safety**: Functions stored in nodes are held by `shared_ptr`, ensuring proper lifetime management even with closures

### Next Steps (Phase 2)

Phase 2 will implement automatic `std::function` type registration: when registering a function `R(Args...)`, automatically register the corresponding `std::function<R(Args...)>` type.

---

## Phase 2: Automatic std::function Type Registration

**Status**: 🔄 Pending
**Planned Date**: TBD

### Objective
When registering a function, automatically register the corresponding `std::function` type.

[To be completed...]

---

## Appendix: Build and Test Commands

```bash
# Build in Docker container
cd /workspace/build
cmake ..
make -j$(nproc)

# Run Phase 1 tests only
./gtests/coral_tests --gtest_filter="FunctionType.*"

# Run all tests
ctest --output-on-failure

# Check specific test output
./gtests/coral_tests --gtest_filter="FunctionType.Registration" --gtest_verbose
```

## References

- **Plan**: `plan.md` - Complete implementation plan with 10 phases
- **Specification**: `specification.md` - Feature requirements and design decisions
- **Context**: `context.md` - CORAL project overview and architecture
