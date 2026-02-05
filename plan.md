# Implementation Plan: Functions as First-Class Citizens

## Overview
This plan implements higher-order functions in CORAL by enabling `std::function` values to be created from registered functions/methods/networks and passed between nodes.

## Testing Philosophy

**Unit Tests as Documentation**: Every implementation step must have corresponding unit tests that serve triple duty:
1. **Validation**: Verify the implementation works correctly
2. **Use Cases**: Demonstrate how to use the feature
3. **MWE (Minimal Working Examples)**: Serve as living documentation

**Test-Driven Approach**:
- Write tests alongside (or before) implementation
- Each test should be minimal, focused, and self-explanatory
- Test names should clearly describe what they validate
- Tests should include comments explaining the use case

**Test Organization**:
- Unit tests: In `gtests/` files, one test per feature/task
- Integration tests: Phase 8, combining multiple features
- All tests must pass before moving to next phase

---

## Phase 1: Foundation - std::function Type Support ✅

**Status**: Completed (2026-02-04)
**Goal**: Ensure `std::function` types can be registered, stored, and passed between nodes.

### Implementation Tasks
- [x] Investigate current `std::function` support in type registration
- [x] Create helper function `register_function_type<R, Args...>()` to register `std::function<R(Args...)>` as an elementary type
- [x] Implement storage and retrieval of `std::function` values in NodeObject

### Unit Tests (gtests/function_types.cc)

#### Test 1.1: Register std::function Type
- [x] **TEST**: `FunctionType_Registration`
  - Register `std::function<double(double)>` as elementary type
  - Verify it appears in registry with correct metadata
  - **MWE**: Shows how to register a function type

#### Test 1.2: Create Node with std::function Value
- [x] **TEST**: `FunctionType_CreateNodeWithStdFunction`
  - Create a simple lambda: `auto square = [](double x) { return x * x; }`
  - Create `std::function<double(double)> fn = square;`
  - Create NodeObject containing this `std::function`
  - Verify node is ready and contains the function
  - **MWE**: Shows how to store a function in a node

#### Test 1.3: Retrieve and Invoke std::function
- [x] **TEST**: `FunctionType_RetrieveAndInvoke`
  - Create node with `std::function<double(double)>` (square function)
  - Retrieve using `node->get<std::function<double(double)>>()`
  - Invoke the function with argument 5.0
  - Verify result is 25.0
  - **MWE**: Shows how to extract and call a function from a node

#### Test 1.4: Pass std::function Between Nodes
- [x] **TEST**: `FunctionType_PassBetweenNodes`
  - Create node A containing `std::function<double(double)>` (square)
  - Create node B that takes `std::function<double(double)>` and a value, calls function
  - Connect A's output to B's input
  - Execute network
  - Verify B produces correct result
  - **MWE**: Shows data flow of functions through graph

**Success Criteria**: ✅ All 4 tests pass. Can manually create and pass `std::function` values.

**Files to Create/Modify**:
- `include/coral.h` - Add `register_function_type<R, Args...>()` helper
- `gtests/function_types.cc` - New test file for function-related tests

---

## Phase 2: Automatic std::function Type Registration ✅

**Status**: Completed (2026-02-04)
**Goal**: When registering a function, automatically register the corresponding `std::function` type (Q1).

### Implementation Tasks
- [x] Modify `NodeObject::register_function<ReturnType, Args...>()` to detect function signature
- [x] After registering function, check if `std::function<ReturnType(Args...)>` is registered
- [x] If not registered, automatically call `register_function_type<ReturnType, Args...>()`
- [x] Store mapping from function type to `std::function` type for lookup

### Unit Tests (gtests/function_types.cc)

#### Test 2.1: Auto-Register Single Function
- [x] **TEST**: `AutoRegistration_SingleFunction`
  - Register `double square(double x)` using `register_function()`
  - Verify `std::function<double(double)>` automatically appears in registry
  - Verify registry entry has correct metadata
  - **MWE**: Shows that registering a function auto-creates the function type

#### Test 2.2: Multiple Functions Same Signature
- [x] **TEST**: `AutoRegistration_SameSignatureMultipleFunctions`
  - Register `double square(double)` and `double cube(double)` and `double times2(double)`
  - Verify only ONE `std::function<double(double)>` type is registered
  - Verify all three functions can use this type
  - **MWE**: Shows signature reuse

#### Test 2.3: Multiple Functions Different Signatures
- [x] **TEST**: `AutoRegistration_DifferentSignatures`
  - Register `double square(double)`, `double add(double, double)`, `int increment(int)`
  - Verify three separate `std::function` types registered:
    - `std::function<double(double)>`
    - `std::function<double(double, double)>`
    - `std::function<int(int)>`
  - **MWE**: Shows multiple function types coexisting

#### Test 2.4: Void Function Auto-Registration
- [x] **TEST**: `AutoRegistration_VoidFunction`
  - Register `void print_value(double)` function
  - Verify `std::function<void(double)>` is auto-registered
  - **MWE**: Shows void functions work too

**Success Criteria**: ✅ All 4 tests pass. Function registration automatically creates corresponding `std::function` types.

**Files to Modify**:
- `include/coral.h` - Modify `register_function()` template
- `include/coral_implementation.h` - Implementation
- `gtests/function_types.cc` - Add auto-registration tests

---

## Phase 3: Simple Function-to-std::function Constructor ✅

**Status**: Completed (2026-02-04)
**Goal**: Create constructor that converts a registered function into a `std::function` value.

### Implementation Tasks
- [x] Design interface: `std::function<R(Args...)> make_function(lambda/fn_ptr)`
- [x] Implement constructor for free functions with signature `R(Args...)`
- [x] Constructor takes function pointer/lambda, returns `std::function<R(Args...)>` VALUE
- [x] Register the constructor itself as a callable node type
- [x] Add helper: `make_function_node<R, Args...>()` for easy node creation

### Unit Tests (gtests/function_types.cc)

#### Test 3.1: Convert Lambda to std::function
- [x] **TEST**: `MakeFunction_LambdaToStdFunction`
  - Define lambda: `auto sq = [](double x) { return x * x; }`
  - Call make_function constructor: `auto fn = make_function(sq)`
  - Verify `fn` is `std::function<double(double)>` type
  - Call `fn(5.0)`, verify result is 25.0
  - **MWE**: Basic function wrapping

#### Test 3.2: Convert Function Pointer to std::function
- [x] **TEST**: `MakeFunction_FunctionPointerToStdFunction`
  - Define free function: `double square(double x) { return x * x; }`
  - Call make_function constructor: `auto fn = make_function(&square)`
  - Verify and invoke
  - **MWE**: Function pointer wrapping

#### Test 3.3: Store std::function in Node
- [x] **TEST**: `MakeFunction_StoreInNode`
  - Create square lambda
  - Use `make_function_node<double(double)>()` to create a node
  - Pass lambda to node
  - Execute node
  - Verify node's output contains `std::function<double(double)>`
  - **MWE**: Function constructor as a node

#### Test 3.4: Pass Constructed Function to Another Node
- [x] **TEST**: `MakeFunction_PassToConsumer`
  - Create node A: uses make_function to create `std::function<double(double)>` (square)
  - Create node B: evaluator that takes `std::function<double(double)>` and calls it with 5.0
  - Connect A → B
  - Execute network
  - Verify B outputs 25.0
  - **MWE**: End-to-end function passing workflow

**Success Criteria**: ✅ All 4 tests pass. Can wrap simple functions in `std::function` and pass them between nodes.

**Files to Modify**:
- `include/coral.h` - Add `make_function` constructors
- `source/` - Implementation (if not header-only)
- `gtests/function_types.cc` - Add constructor tests

---

## Phase 4: Function Constructor for All Node Types ✅

**Status**: Completed (2026-02-04)
**Goal**: Support converting different node types (functions, methods, networks) to `std::function`.

### Implementation Tasks

#### 4.1: Free Functions (Multiple Signatures) ✅
- [x] Extend Phase 3 constructor to handle void functions
- [x] Extend to handle multi-argument functions
- [x] Add validation for signature matching

#### 4.2: Methods (Non-const) ✅
- [x] Implement constructor that wraps method + bound object
- [x] Extract method and object, create `std::function` closure
- [x] Handle this pointer correctly

#### 4.3: Methods (Const) ✅
- [x] Similar to 4.2 but for const methods
- [x] Ensure const-correctness

#### 4.4: Networks ✅
- [x] Implement constructor that wraps Network node
- [x] Map network inputs[i] to function args[i] (Q3)
- [x] Map network outputs[0] to function return value (Q3)
- [x] Handle network cloning/lifetime

#### 4.5: Unified Constructor ✅
- [x] Create unified approach using C++ type system (lambda captures)
- [x] Demonstrate interchangeability of all function sources
- [x] Add function composition and polymorphic storage

### Unit Tests (gtests/function_types.cc)

#### Phase 4.1: Void and Multi-Argument Functions (4 tests) ✅
- [x] **TEST**: `VoidFunction.Registration`
  - Register void function and verify auto-registration
  - **MWE**: Void function type registration

- [x] **TEST**: `VoidFunction.StoreAndInvoke`
  - Create void function with side effects, store in node
  - Retrieve and invoke, verify side effect
  - **MWE**: Void function storage and invocation

- [x] **TEST**: `MultiArgFunction.RegistrationAndInvoke`
  - Register 3-argument function
  - Verify auto-registration of `std::function<double(double, double, double)>`
  - **MWE**: Multi-argument function

- [x] **TEST**: `MultiArgFunction.StoreInNode`
  - Store multi-argument function in node and use in network
  - **MWE**: Multi-argument function in computational graph

#### Phase 4.2: Non-Const Methods (3 tests) ✅
- [x] **TEST**: `NonConstMethod.BindAndInvoke`
  - Bind Counter object method using lambda capture
  - Invoke multiple times, verify state changes
  - **MWE**: Basic method binding

- [x] **TEST**: `NonConstMethod.StoreInNode`
  - Store bound method in node using shared_ptr
  - **MWE**: Method binding with lifetime management

- [x] **TEST**: `NonConstMethod.WithArguments`
  - Bind method that takes arguments
  - **MWE**: Parameterized method binding

#### Phase 4.3: Const Methods (4 tests) ✅
- [x] **TEST**: `ConstMethod.BindAndInvoke`
  - Bind const method using lambda capture
  - **MWE**: Basic const method binding

- [x] **TEST**: `ConstMethod.StoreInNode`
  - Store bound const method in node
  - **MWE**: Const method storage

- [x] **TEST**: `ConstMethod.WithArguments`
  - Bind const method with parameters
  - **MWE**: Parameterized const method

- [x] **TEST**: `ConstMethod.ConstCorrectness`
  - Verify const methods don't modify state
  - **MWE**: Const-correctness validation

#### Phase 4.4: Networks as Functions (3 tests) ✅
- [x] **TEST**: `NetworkAsFunction.SimpleAddition`
  - Wrap simple network (addition) as std::function
  - Lambda captures network nodes and executes on call
  - **MWE**: Basic network-to-function

- [x] **TEST**: `NetworkAsFunction.ComplexComputation`
  - Multi-node network: `result = (input0 * 2) + input1`
  - **MWE**: Multi-node network as function

- [x] **TEST**: `NetworkAsFunction.StoreInNode`
  - Store network-as-function in node
  - **MWE**: Network function storage

#### Phase 4.5: Unified Constructor (3 tests) ✅
- [x] **TEST**: `UnifiedFunction.AllSourceTypesInterchangeable`
  - Create std::function from free function, method, and network
  - Store all in nodes and use identically
  - **MWE**: Unified interface for all node types

- [x] **TEST**: `UnifiedFunction.FunctionComposition`
  - Compose functions from different sources
  - **MWE**: Cross-source function composition

- [x] **TEST**: `UnifiedFunction.PolymorphicStorage`
  - Store functions from different sources in vector
  - **MWE**: Polymorphic storage and uniform usage

**Success Criteria**: ✅ All 17 tests pass. Can convert any function/method/network to `std::function`.

**Files to Modify**:
- `include/coral.h` - Add method/network function constructors
- `include/coral_network.h` - Network-to-function helpers
- `gtests/function_types.cc` - Tests for each node type

---

## Phase 5: Partial Application Support ✅

**Status**: Completed (2026-02-04)
**Goal**: Support creating `std::function` with fewer parameters by pre-binding arguments (Q2).

### Implementation Tasks
- [x] Design API for specifying which parameters to bind (lambda value captures)
- [x] Implement parameter binding using lambda captures
- [x] Verify unbound parameters map correctly to `std::function` signature
- [x] Ensure bound values are captured by value (not reference)

### Unit Tests (gtests/function_types.cc)

#### Conceptual Tests - Pure C++ Partial Application (6 tests) ✅

- [x] **TEST**: `PartialApplication.BindOneParameter`
  - Bind one parameter of 2-parameter function
  - Result: `times_5(x) = x * 5`
  - **MWE**: Basic partial application using value capture

- [x] **TEST**: `PartialApplication.BindMultipleParameters`
  - Bind 2 parameters of 3-parameter function
  - Result: `f(x) = x + 2 + 3`
  - **MWE**: Multiple parameter binding

- [x] **TEST**: `PartialApplication.BindFirstVsLastParameter`
  - Compare binding first vs last parameter
  - Test 1: `f(b) = 10 / b`, Test 2: `f(a) = a / 2`
  - **MWE**: Parameter order matters

- [x] **TEST**: `PartialApplication.ValueCaptureNotReference`
  - Modify bound variable after capture
  - Verify function uses original captured value (5.0, not 10.0)
  - **MWE**: Safe value capture semantics

- [x] **TEST**: `PartialApplication.StoreInNode`
  - Store partially applied function in node
  - Retrieve and invoke
  - **MWE**: Node storage of partial applications

- [x] **TEST**: `PartialApplication.ChainedPartialApplication`
  - Apply partial application multiple times in sequence
  - `f(a,b,c) → f(a,b) → f(a)` by binding c then b
  - **MWE**: Chained partial application

#### Integration Tests - Registered CORAL Functions (3 tests) ✅

- [x] **TEST**: `RegisteredFunctionPartialApplication.BindOneParameter`
  - Register CORAL function `multiply(a, b, result)`
  - Create partial application by binding b=5.0
  - Lambda wraps node creation and execution
  - **MWE**: Partial application of registered function

- [x] **TEST**: `RegisteredFunctionPartialApplication.BindMultipleParameters`
  - Register `sum3(a, b, c, result)`
  - Bind b=2.0 and c=3.0
  - **MWE**: Multiple parameter binding with registered function

- [x] **TEST**: `RegisteredFunctionPartialApplication.UseInNetwork`
  - Register power and subtract functions
  - Create partial application (square from power)
  - Use in larger network: `square(5) - 10 = 15`
  - **MWE**: Full integration with computational graph

**Success Criteria**: ✅ All 9 tests pass (6 conceptual + 3 integration). Can bind arbitrary parameters using lambda value captures.

**Files to Modify**:
- `include/coral.h` - Extend `make_function` with binding support
- `gtests/function_types.cc` - Partial application tests

---

## Phase 6: Higher-Order Functions Registration

**Goal**: Register useful higher-order functions that consume `std::function` arguments.

### Key Design Decision: Type-Erased Implementation

**Challenge**: Cannot register templated functions in CORAL. Need non-templated signatures.

**Solution**: Use type-erased vectors and leverage executor infrastructure:
- **Input**: `std::vector<std::shared_ptr<entt::meta_any>>` (type-erased values)
- **Function**: `NodeObjectPtr` (function node with executor)
- **Output**: `std::vector<std::shared_ptr<entt::meta_any>>` (type-erased results)

**Why This Works**:
1. **No templates needed** - single function signature works for all types
2. **Executor handles type casting** - function's executor already knows how to cast `entt::meta_any` to concrete types
3. **Dynamic output type creation** - extract output type from function metadata and create nodes using initializer's executor
4. **Fully generic** - works with any registered function, no type enumeration

### Implementation Pattern

```cpp
std::vector<std::shared_ptr<entt::meta_any>>
map(const std::vector<std::shared_ptr<entt::meta_any>>& input,
    NodeObjectPtr function_node)
{
  std::vector<std::shared_ptr<entt::meta_any>> result;

  // Extract output type from function metadata
  std::string output_type_hash =
    function_node->initializer.json_serializer["arguments"][0]["type"];
  auto& output_type_init = NodeObject::initializers[output_type_hash];

  for (const auto& elem_any : input) {
    // Create input node
    NodeObjectPtr input_node = std::make_shared<NodeObject>(elem_any);

    // Create output node of correct type
    auto output_any = output_type_init.executor(nullptr, {});
    NodeObjectPtr output_node = std::make_shared<NodeObject>(output_any);

    // Execute function
    function_node->set_arguments({output_node, input_node});
    (*function_node)();

    // Collect result
    result.push_back(output_node->object);
  }
  return result;
}
```

### How Executors Work (Key Insights)

**Structure of NodeObject**:
```
NodeObject
  └─ shared_ptr<entt::meta_any> object
       └─ shared_ptr<T> (actual value pointer)
            └─ T (actual value)
```

**Executor signature**:
```cpp
std::function<std::shared_ptr<entt::meta_any>(
  const NodeObjectPtr&,           // the node itself
  std::vector<NodeObjectPtr> args // arguments
)>
```

**What executors do**:
1. Extract values: `args[i]->get<T>()` or `args[i]->get_shared<T>()`
2. For output: Get reference `auto &ret = args[0]->get<OutputType>()`
3. Execute function and write to output
4. Result stored IN the output node (not executor's return value)

**Creating output nodes**:
- Elementary type initializers have executors that create default instances
- Call `initializer.executor(nullptr, {})` to create a new instance
- No need to know concrete type - initializer handles it!

### Implementation Tasks

#### 6.1: Map
- [ ] Implement `map(vector<meta_any>, NodeObjectPtr)` following pattern above
- [ ] Register using custom executor (not `register_function()`)

#### 6.2: Reduce
- [ ] Implement `reduce(vector<meta_any>, NodeObjectPtr, meta_any initial)`
- [ ] Binary function expects 2 inputs + 1 output
- [ ] Register with custom executor

#### 6.3: Filter
- [ ] Implement `filter(vector<meta_any>, NodeObjectPtr predicate)`
- [ ] Predicate returns bool - only keep elements where predicate is true
- [ ] Register with custom executor

#### 6.4: Apply
- [ ] Implement `apply(NodeObjectPtr function, vector<meta_any> args)`
- [ ] Generic function application
- [ ] Register with custom executor

#### 6.5: Register All
- [ ] Add all higher-order functions to `register_non_dimensional_types()`
- [ ] Register vector types: `std::vector<std::shared_ptr<entt::meta_any>>`
- [ ] Verify they appear in node registry JSON

### Unit Tests (gtests/higher_order_functions.cc - new file)

#### Test 6.1.1: Map with Square Function
- [ ] **TEST**: `Map_SquareFunction`
  - Create vector [1.0, 2.0, 3.0, 4.0]
  - Create `std::function<double(double)>` for square
  - Call map(vec, square_fn)
  - Verify result: [1.0, 4.0, 9.0, 16.0]
  - **MWE**: Basic map usage

#### Test 6.1.2: Map in Network
- [ ] **TEST**: `Map_InNetwork`
  - Create network with:
    - Node A: vector [1, 2, 3, 4]
    - Node B: square function
    - Node C: make_function (converts B to std::function)
    - Node D: map(A, C)
  - Execute network
  - Verify D outputs [1, 4, 9, 16]
  - **MWE**: Map in graph workflow

#### Test 6.2.1: Reduce with Sum
- [ ] **TEST**: `Reduce_SumFunction`
  - Create vector [1.0, 2.0, 3.0, 4.0]
  - Create `std::function<double(double, double)>` for add
  - Call reduce(vec, add_fn, 0.0)
  - Verify result: 10.0
  - **MWE**: Basic reduce usage

#### Test 6.2.2: Reduce with Product
- [ ] **TEST**: `Reduce_ProductFunction`
  - Same vector
  - Create multiply function
  - Call reduce(vec, mult_fn, 1.0)
  - Verify result: 24.0
  - **MWE**: Reduce with different operation

#### Test 6.3.1: Filter Even Numbers
- [ ] **TEST**: `Filter_EvenNumbers`
  - Create vector [1, 2, 3, 4, 5, 6]
  - Create `std::function<bool(int)>` for is_even
  - Call filter(vec, is_even_fn)
  - Verify result: [2, 4, 6]
  - **MWE**: Basic filter usage

#### Test 6.3.2: Filter Greater Than Threshold
- [ ] **TEST**: `Filter_GreaterThanThreshold`
  - Create vector [1.0, 5.0, 3.0, 7.0, 2.0]
  - Create `std::function<bool(double)>` for `x > 3.5`
  - Filter and verify result: [5.0, 7.0]
  - **MWE**: Filter with predicate

#### Test 6.4.1: Apply Unary Function
- [ ] **TEST**: `Apply_UnaryFunction`
  - Create `std::function<double(double)>` for square
  - Call apply(square_fn, 5.0)
  - Verify result: 25.0
  - **MWE**: Basic apply usage

#### Test 6.4.2: Apply Binary Function
- [ ] **TEST**: `Apply_BinaryFunction`
  - Create `std::function<double(double, double)>` for add
  - Call apply(add_fn, 3.0, 4.0)
  - Verify result: 7.0
  - **MWE**: Apply with multiple arguments

**Success Criteria**: All 8 tests pass. Can use map, reduce, filter, apply with function values.

**Files to Create/Modify**:
- `include/register_types.h` - Declare higher-order functions
- `source/register_types.cc` - Implement and register
- `gtests/higher_order_functions.cc` - New test file with 8 tests

---

## Phase 7: JSON Serialization Support

**Goal**: Define JSON representation and implement serialization/deserialization.

### Implementation Tasks
- [ ] Define JSON schema for function constructor nodes
- [ ] Implement serialization for function constructor nodes
- [ ] Implement deserialization from JSON
- [ ] Handle `std::function` values in serialization
- [ ] Support round-trip serialization

### Unit Tests (gtests/serialize.cc)

#### Test 7.1: Serialize Function Constructor Node
- [ ] **TEST**: `Serialize_FunctionConstructorNode`
  - Create network with make_function node
  - Serialize to JSON
  - Verify JSON structure:
    - Node has correct type (e.g., "make_function<double(double)>")
    - Connections are preserved
  - **MWE**: Function constructor in JSON

#### Test 7.2: Deserialize Function Constructor Node
- [ ] **TEST**: `Deserialize_FunctionConstructorNode`
  - Create JSON with function constructor node
  - Deserialize to network
  - Verify node is created with correct type
  - Verify connections are established
  - **MWE**: Loading function constructors from JSON

#### Test 7.3: Round-Trip Serialization
- [ ] **TEST**: `Serialize_RoundTrip`
  - Create network: square function → make_function → map
  - Serialize to JSON (json1)
  - Deserialize json1 to network2
  - Serialize network2 to JSON (json2)
  - Verify json1 == json2 (structural equality)
  - **MWE**: Serialization preserves structure

#### Test 7.4: Serialize Map Workflow
- [ ] **TEST**: `Serialize_MapWorkflow`
  - Create complete map workflow in C++
  - Serialize to JSON
  - Save to `test_files/unit_test_map.json`
  - Deserialize and execute
  - Verify correct output
  - **MWE**: Complete workflow serialization

**Success Criteria**: All 4 tests pass. Can serialize/deserialize workflows with function-passing.

**Files to Modify**:
- `include/coral.h` - JSON support for function types
- `source/coral_implementation.cc` - Implementation
- `gtests/serialize.cc` - Add 4 serialization tests
- `test_files/unit_test_map.json` - Generated test file

---

## Phase 8: End-to-End Integration Tests

**Goal**: Validate complete workflows from JSON files. These are integration tests, not unit tests.

### Integration Tests (gtests/integration_tests.cc - new file)

#### Test 8.1: Map Square from JSON
- [ ] **TEST**: `Integration_MapSquare`
  - Load `test_files/map_square.json`
  - JSON defines: vector [1,2,3,4], square function, map
  - Execute network
  - Verify output: [1,4,9,16]
  - **MWE**: Complete map workflow from JSON

#### Test 8.2: Reduce Sum from JSON
- [ ] **TEST**: `Integration_ReduceSum`
  - Load `test_files/reduce_sum.json`
  - JSON defines: vector [1,2,3,4], sum function, reduce with initial=0
  - Execute network
  - Verify output: 10
  - **MWE**: Complete reduce workflow from JSON

#### Test 8.3: Network as Function from JSON
- [ ] **TEST**: `Integration_NetworkAsFunction`
  - Load `test_files/network_as_function.json`
  - JSON defines: network combining times_2 and add operations
  - Network converted to std::function
  - Passed to evaluator
  - Verify correct execution
  - **MWE**: Network-to-function in JSON

#### Test 8.4: Function Composition from JSON
- [ ] **TEST**: `Integration_FunctionComposition`
  - Load `test_files/function_composition.json`
  - JSON defines: multiple functions composed in network
  - Execute and verify
  - **MWE**: Composing functions via networks

#### Test 8.5: Partial Application from JSON
- [ ] **TEST**: `Integration_PartialApplication`
  - Load `test_files/partial_application.json`
  - JSON defines: multiply function with bound parameter
  - Result used in map
  - Verify output
  - **MWE**: Partial application in JSON

### JSON Files to Create
- [ ] Create `test_files/map_square.json`
- [ ] Create `test_files/reduce_sum.json`
- [ ] Create `test_files/network_as_function.json`
- [ ] Create `test_files/function_composition.json`
- [ ] Create `test_files/partial_application.json`

**Success Criteria**: All 5 integration tests pass. Complete workflows work end-to-end from JSON.

**Files to Create**:
- `gtests/integration_tests.cc` - New file with 5 integration tests
- `test_files/*.json` - 5 example JSON workflows

---

## Phase 9: Documentation and Examples

**Goal**: Document the feature for users.

### Documentation Tasks
- [ ] Update README.md with higher-order functions section
- [ ] Document function type registration
- [ ] Document function constructors (make_function)
- [ ] Document each higher-order function (map, reduce, filter, apply)
- [ ] Add C++ code examples (extracted from unit tests)
- [ ] Add JSON examples (point to test_files/)
- [ ] Document limitations and edge cases
- [ ] Update node registry JSON schema docs

### Documentation Tests (optional)
- [ ] **TEST**: `Documentation_CodeExamplesCompile`
  - Extract code snippets from documentation
  - Verify they compile and run correctly
  - **MWE**: Ensures docs stay up-to-date

**Success Criteria**: Complete documentation with working examples.

**Files to Modify**:
- `README.md` - Add higher-order functions section
- `docs/higher_order_functions.md` (new) - Detailed documentation

---

## Phase 10: Performance and Refinement

**Goal**: Optimize and polish the implementation.

### Implementation Tasks
- [ ] Profile function wrapping overhead
- [ ] Optimize network-to-function conversion
- [ ] Ensure efficient captures (move semantics)
- [ ] Review error messages for clarity
- [ ] Code review and refactoring
- [ ] Ensure const-correctness

### Performance Tests (gtests/benchmarks.cc - new file)

#### Test 10.1: Function Call Overhead
- [ ] **TEST**: `Benchmark_FunctionCallOverhead`
  - Compare direct function call vs wrapped std::function call
  - Measure time for 1M iterations
  - Verify overhead < 10%
  - **MWE**: Performance characteristics

#### Test 10.2: Map Operation Performance
- [ ] **TEST**: `Benchmark_MapPerformance`
  - Compare hand-written loop vs map with std::function
  - Large vector (100K elements)
  - Measure time difference
  - **MWE**: Real-world performance

#### Test 10.3: Network-as-Function Overhead
- [ ] **TEST**: `Benchmark_NetworkAsFunctionOverhead`
  - Compare direct network execution vs wrapped as std::function
  - Measure overhead
  - **MWE**: Network wrapping cost

**Success Criteria**: All benchmarks show acceptable performance (< 10% overhead).

**Files to Create**:
- `gtests/benchmarks.cc` - Performance tests

---

## Dependencies Between Phases

```
Phase 1 (Foundation)
    ↓ [unit tests]
Phase 2 (Auto-registration)
    ↓ [unit tests]
Phase 3 (Simple Constructor)
    ↓ [unit tests]
Phase 4 (All Node Types) ← Phase 5 (Partial Application) [both with unit tests]
    ↓ [unit tests]         ↓ [unit tests]
Phase 6 (Higher-Order Functions)
    ↓ [unit tests]
Phase 7 (JSON Serialization)
    ↓ [unit tests]
Phase 8 (Integration Tests) [comprehensive tests using JSON]
    ↓
Phase 9 (Documentation) ← Phase 10 (Performance) [benchmarks]
    ↓                      ↓
DONE
```

## Testing Strategy

### Unit Test Requirements
Each unit test must:
1. **Be focused**: Test one specific feature/behavior
2. **Be minimal**: Use simplest possible setup
3. **Be documented**: Include comment explaining use case
4. **Be self-contained**: No dependencies on other tests
5. **Have clear assertions**: Verify expected behavior explicitly

### ⚠️ CRITICAL: Test Independence
**Every test MUST be completely independent and register ALL types it uses.**

Common mistakes to avoid:
- ❌ Assuming `double`, `int`, or other basic types are pre-registered
- ❌ Relying on test execution order for type registration
- ❌ Depending on other test files to register types

**Always include at the start of each test:**
```cpp
// Register ALL types this test uses
NodeObject::register_elementary_type<double>();
NodeObject::register_elementary_type<int>();
NodeObject::register_function_type<double, double>();  // If using std::function
// ... any other types needed
```

**Why this matters:**
- Tests may run in different orders
- Test filters (`--gtest_filter`) may run tests in isolation
- Other test files may not be compiled/linked in all build configurations

**Testing test independence:**
```bash
# Always verify tests pass in isolation
./gtests/coral_tests --gtest_filter="YourTest.YourTestName"
```

### Test Naming Convention
```cpp
TEST(Category, FeatureDescription)
// Examples:
TEST(FunctionType, Registration)
TEST(MakeFunction, LambdaToStdFunction)
TEST(PartialApplication, BindOneParam)
TEST(Map, SquareFunction)
TEST(Integration, MapSquare)
```

### Running Tests
```bash
# In Docker container
cd /workspace
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run all tests
ctest --output-on-failure

# Run tests for specific phase
./gtests/coral_tests --gtest_filter="FunctionType.*"
./gtests/coral_tests --gtest_filter="MakeFunction.*"

# Run specific test
./gtests/coral_tests --gtest_filter="Map.SquareFunction"
```

### Test-as-Documentation Example
```cpp
// Example of a good unit test that serves as MWE
TEST(Map, SquareFunction) {
  // USE CASE: Apply square function to each element of a vector

  // Step 1: Register types
  coral::NodeObject::register_elementary_type<double>();
  coral::NodeObject::register_elementary_type<std::vector<double>>();

  // Step 2: Create square function
  auto square = [](double x) { return x * x; };
  coral::NodeObject::register_function(square, {"square", "result", "x"});

  // Step 3: Create function value
  auto square_fn = std::function<double(double)>(square);

  // Step 4: Create input vector
  std::vector<double> input = {1.0, 2.0, 3.0, 4.0};

  // Step 5: Apply map
  auto result = coral::map(input, square_fn);

  // Step 6: Verify
  ASSERT_EQ(result.size(), 4);
  EXPECT_EQ(result[0], 1.0);
  EXPECT_EQ(result[1], 4.0);
  EXPECT_EQ(result[2], 9.0);
  EXPECT_EQ(result[3], 16.0);
}
```

## Progress Tracking

Mark tests as completed:
- [x] Phase 1: 4 unit tests ✅
- [x] Phase 2: 4 unit tests ✅
- [x] Phase 3: 4 unit tests ✅
- [x] Phase 4: 17 unit tests ✅ (expanded from 7 to cover all scenarios comprehensively)
- [x] Phase 5: 9 unit tests ✅ (expanded from 4: 6 conceptual + 3 integration)
- [x] Phase 6: 8 unit tests ✅ (type-erased higher-order functions)
- [ ] Phase 7: 4 unit tests
- [ ] Phase 8: 5 integration tests
- [ ] Phase 9: Documentation
- [ ] Phase 10: 3 performance tests

**Total Planned**: 58 tests (53 unit + 5 integration + 3 benchmark)
**Completed**: 46 tests (Phases 1-6)

## Rollback Plan

If a phase encounters issues:
1. Document the issue in `plan.md`
2. Mark problematic tasks/tests with `[BLOCKED: reason]`
3. Discuss alternative approaches
4. Consider simplifying requirements
5. Ensure existing tests still pass

## Success Metrics

Overall success measured by:
1. **All 43 tests pass** ✓
2. Can create map/reduce/filter workflows entirely in JSON ✓
3. Can convert networks to functions and pass them ✓
4. Round-trip JSON serialization works ✓
5. Performance overhead < 10% compared to direct calls ✓
6. Documentation is complete with working examples ✓
7. **Tests serve as living documentation** ✓
