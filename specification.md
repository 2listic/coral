# Specification: Functions and Networks as First-Class Citizens

## Overview

This specification defines the enhancement to CORAL to support **higher-order functions**: functions that take other functions as input parameters. This enables functional programming patterns like `map`, `reduce`, `filter`, and `apply` within the CORAL computational graph system.

## Current System

### Architecture
- **NodeObject**: Represents any typed value (elementary types, constructors, functions, methods, networks)
- **Network**: Graph of interconnected NodeObjects with dependency-based execution
- **Connection**: Directed edge from source node's output to target node's input
- **Execution Model**: Nodes execute when dependencies are satisfied; outputs flow to connected inputs

### Current Data Flow
Currently, edges represent **value dependencies**:
```
[Node A: double=5.0] --value--> [Node B: multiply by 2] --value--> [Node C: result=10.0]
```

## Requirement

Enable nodes (functions, methods, networks) to be passed as **values** to other nodes, allowing higher-order functions.

### Use Cases
1. **Map**: Apply a function to each element of a vector
2. **Reduce**: Aggregate vector elements using a binary function
3. **Filter**: Select vector elements matching a predicate
4. **Apply**: Evaluate a function at specific arguments
5. **Composition**: Combine functions in networks and pass the network as a function

## Agreed Solution

### Core Concept
Use `std::function<R(Args...)>` as the type for function values. A "function constructor" node converts registered functions/methods/networks into `std::function` values that can be passed between nodes.

### Architecture

```
[Node A: registered function] ---> [Constructor: make_function] ---> [std::function<R(Args...)> value]
                                                                             |
                                                                             v
[Node B: vector<T>] ----------------------------------------------> [Higher-order function: map]
```

### Key Components

#### 1. Automatic std::function Type Registration (Q1 - Option A)
When registering a function with signature `R(Args...)`, automatically register the corresponding `std::function<R(Args...)>` type if not already present.

**Example**:
```cpp
auto square = [](double x) { return x * x; };
NodeObject::register_function(square, {"square", "result", "x"});
// Automatically registers: std::function<double(double)>
```

#### 2. Function Constructor Nodes
Create constructor nodes that take a function/method/network as input and produce a `std::function<R(Args...)>` value as output.

**Example**:
```cpp
template<typename R, typename... Args>
void register_make_function() {
    // Creates std::function<R(Args...)> from a function node
}
```

#### 3. Partial Application Support (Q2 - Option A)
When creating a `std::function` with fewer parameters than the source function, extra parameters must be pre-bound.

**Example**:
```cpp
// Source: multiply(a, b) -> double
// Target: std::function<double(double)>
// Implementation: Bind b=5, expose a as parameter
std::function<double(double)> times_5 = [](double a) { return multiply(a, 5.0); };
```

#### 4. Network-to-Function Conversion (Q3 - Map by Index)
Networks can be converted to `std::function` by mapping:
- Function arguments[i] → Network inputs[i] (by index)
- Function return value ← Network outputs[0] (first output)

**Example**:
```
Network: inputs[x:double, y:double] -> outputs[z:double]
Becomes: std::function<double(double, double)>
```

#### 5. Higher-Order Functions
Register functions that accept `std::function` parameters:

**Map**:
```cpp
template<typename T>
std::vector<T> map(const std::vector<T>& vec, std::function<T(T)> fn) {
    std::vector<T> result;
    for (const auto& elem : vec) result.push_back(fn(elem));
    return result;
}
```

**Reduce**:
```cpp
template<typename T>
T reduce(const std::vector<T>& vec, std::function<T(T,T)> fn, T initial) {
    T result = initial;
    for (const auto& elem : vec) result = fn(result, elem);
    return result;
}
```

## Technical Specifications

### std::function as Value
The `std::function` created by a constructor node is a **self-contained, executable value**:
- It encapsulates the complete logic (not just a reference)
- It can be stored, passed, and invoked independently
- When passed to another function, that function calls it with its chosen arguments

### Example Scenarios

#### Scenario A: Simple Function Evaluation
```
Node A: Creates std::function<double(double)> containing times_2
Node B: evaluate_in_4(std::function<double(double)> f)
        Calls: f(4.0)
        Returns: 8.0
```

#### Scenario B: Map Operation
```
Node A: square function (double → double)
Node B: make_function<double(double)>(A) → std::function<double(double)>
Node C: vector [1, 2, 3, 4]
Node D: map(C, B) → vector [1, 4, 9, 16]
```

#### Scenario C: Network as Function
```
Node A: std::function<double(double)> for times_2
Node B: std::function<double(double,double)> for sum
Network C: Combines A and B
    Inputs: x, y
    Logic: sum(times_2(x), y)
    Becomes: std::function<double(double,double)>
Node D: evaluate_on_3_and_4(C)
        Calls: C(3.0, 4.0)
        Returns: sum(times_2(3.0), 4.0) = sum(6.0, 4.0) = 10.0
```

## JSON Representation (Q4)

### Example Workflow
```json
{
  "nodes": {
    "0": {
      "type": "square",
      "name": "square_function"
    },
    "1": {
      "type": "make_function<double(double)>",
      "name": "square_as_function"
    },
    "2": {
      "type": "std::vector<double>",
      "value": "[1.0, 2.0, 3.0, 4.0]",
      "name": "input_vector"
    },
    "3": {
      "type": "map<double>",
      "name": "map_square"
    }
  },
  "edges": {
    "0": {
      "source": 0,
      "target": 1,
      "source_output": 0,
      "target_input": 0,
      "note": "square function -> make_function"
    },
    "1": {
      "source": 1,
      "target": 3,
      "source_output": 0,
      "target_input": 1,
      "note": "std::function -> map.function"
    },
    "2": {
      "source": 2,
      "target": 3,
      "source_output": 0,
      "target_input": 0,
      "note": "vector -> map.vector"
    }
  }
}
```

## Implementation Constraints

1. **Minimal Changes**: Leverage existing type registration and execution infrastructure
2. **Type Safety**: Signature mismatches should be caught at registration/construction time
3. **Performance**: Function wrapping should have minimal overhead
4. **Serialization**: Must support round-trip JSON serialization/deserialization
5. **Compatibility**: Existing code and JSON files should continue to work unchanged

## Success Criteria

1. Can register functions and automatically get corresponding `std::function` types
2. Can create `std::function` values from registered functions via constructor nodes
3. Can create `std::function` values from networks via constructor nodes
4. Can pass `std::function` values to higher-order functions (map, reduce, etc.)
5. Can serialize and deserialize workflows with function-passing
6. Can compose functions in networks and use the network as a function
7. Partial application works for creating `std::function` with fewer parameters
8. All functionality works through JSON configuration files

## Non-Goals (Future Work)

- Runtime type inference for function signatures
- Automatic currying
- Lambda definitions in JSON
- Function composition operators
- Name-based (instead of index-based) network parameter mapping
