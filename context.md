# CORAL Project Context

This document provides comprehensive context for the CORAL project and the current implementation of functions as first-class citizens.

## Project Overview

**CORAL** is a graph-based computational framework that allows users to define and execute computational workflows through JSON configuration files. It builds on top of the deal.II finite element library and uses Taskflow for parallel execution.

### Key Concepts
- **Node**: Represents a typed value, function, method, or network
- **Edge/Connection**: Directed link from one node's output to another node's input
- **Network**: A graph of interconnected nodes with dependency-based execution
- **Execution Model**: Nodes execute when all their input dependencies are satisfied

## Project Structure

```
coral-editor/
├── source/
│   └── main.cc                    # Entry point, CLI parsing, network execution
├── include/
│   ├── coral.h                    # NodeObject definition and registration
│   ├── coral_implementation.h     # Template implementations for coral.h
│   ├── coral_network.h            # Network class definition
│   ├── coral_network_implementation.h  # Template implementations for network
│   ├── register_types.h           # Declaration of type registration function
│   ├── type_name.h                # Type name utilities
│   └── utils.h                    # Utility functions
├── gtests/
│   ├── main.cc                    # Google Test main
│   ├── network.cc                 # Network tests
│   ├── node_types.cc              # Node type tests
│   ├── trivial_types.cc           # Elementary type tests
│   ├── serialize.cc               # Serialization tests
│   └── ...                        # Other test files
├── test_files/
│   ├── mwe.json                   # Minimal working example JSON
│   └── ...                        # Other test JSON files
├── specification.md               # Feature specification (new)
├── plan.md                        # Implementation plan (new)
└── context.md                     # This file
```

## Key Files and Their Roles

### `include/coral.h`
The core header defining the `NodeObject` class.

**Key Components**:
- `NodeObject`: Wraps any registered type in `std::shared_ptr<entt::meta_any>`
- `NodeType` enum: `none`, `abstract`, `elementary_constructor`, `empty_constructor`, `constructor`, `void_method`, `void_const_method`, `method`, `const_method`, `void_function`, `function`, `network`
- `ConnectionType` enum: `none`, `input`, `output`, `pass_through`, `self`
- Registration functions:
  - `register_elementary_type<T>()`: For trivially constructible/copyable types
  - `register_type<T>()`: For types with trivial constructors
  - `register_type<T, Args...>(arg_names)`: For types requiring constructor arguments
  - `register_function(fn, arg_names)`: For free functions
  - `register_method(method_ptr, arg_names)`: For class methods
  - `register_derived_type<Base, Derived>()`: For derived classes

**Important Methods**:
- `operator()()`: Executes the node's function/constructor
- `get<T>()`: Retrieves the stored value as type T
- `get_output(index)`: Returns the node at the given output index
- `get_input(index)`: Returns the node at the given input index
- `bind_input(index, node)`: Connects an input to another node's output
- `bind_output(index, node)`: Binds an output to a node

### `include/coral_network.h`
Defines the `Network` class for managing computational graphs.

**Key Components**:
- `Connection`: Struct representing an edge (source_id, target_id, source_output, target_input)
- `Network`: Main class for graph management
  - `nodes`: Map of node ID to NodeObjectPtr
  - `connections`: Map of connection ID to Connection
  - `taskflow`: Taskflow graph for execution

**Important Methods**:
- `add_node(id, node, name)`: Adds a node to the network
- `add_connection(id, source, target, source_out, target_in)`: Adds an edge
- `from_json(json_data)`: Deserializes network from JSON
- `to_json()`: Serializes network to JSON
- `run()`: Executes the network using Taskflow
- `get_inputs()`: Returns list of unbound inputs (network interface)
- `get_outputs()`: Returns list of unbound outputs (network interface)

### `source/main.cc`
Entry point with CLI interface.

**Commands**:
- `register`: Dumps node type registry to JSON
- `run <json_file>`: Executes a network from JSON file

**Important Environment Variables**:
- `THREADS`: Number of threads for Taskflow executor (defaults to hardware concurrency)

### `gtests/network.cc`
Contains network tests showing usage patterns.

**Key Tests**:
- `BareMinimal`: Creates simple network with two values and a sum function
- `ParseAndExecuteNetwork`: Loads and executes network from JSON
- `JsonBasedWorkflow`: Tests JSON serialization/deserialization

## Current Architecture

### Type System

CORAL uses `entt::meta` for type reflection. Every registered type can be:
1. Constructed from JSON
2. Stored in a `NodeObject`
3. Passed between nodes
4. Serialized back to JSON

**Type Storage**:
```cpp
std::shared_ptr<entt::meta_any> object;  // Inside NodeObject
```

The `meta_any` wraps a `std::shared_ptr<T>` for the actual type T.

### Node Execution Model

When a node executes (`operator()()`):
1. Check all arguments/inputs are ready
2. Call the `executor` function with the arguments
3. Store the result in `object`
4. Mark the node as ready
5. Return success/failure

**Executor Signature**:
```cpp
std::function<std::shared_ptr<entt::meta_any>(
    const NodeObjectPtr&,
    std::vector<NodeObjectPtr> args)>
```

### Connection Model

Connections link nodes:
- **Output**: A node's result value (index -1 means "self", index ≥0 means specific output)
- **Input**: A node's required argument (index maps to argument position)
- **Pass-through**: Non-const reference arguments that are both input and output

Connection establishment:
```cpp
target_node->bind_input(target_input, source_node->get_output(source_output));
```

### JSON Format

```json
{
  "version": 1,
  "workflow": {
    "nodes": {
      "0": {
        "type": "double",
        "value": "5.0",
        "name": "my_value"
      },
      "1": {
        "type": "some_function_name"
      }
    },
    "edges": {
      "0": {
        "source": 0,
        "source_output": 0,
        "target": 1,
        "target_input": 0
      }
    }
  }
}
```

## Current Feature: Functions as First-Class Citizens

### Goal
Enable nodes (functions, methods, networks) to be passed as values to other nodes, enabling higher-order functions like `map`, `reduce`, `filter`.

### Solution Approach
Use `std::function<R(Args...)>` as the type for function values. Create "constructor" nodes that convert registered functions into `std::function` values.

### Design Decisions

1. **Q1**: When registering a function, automatically register the corresponding `std::function` type
2. **Q2**: Partial application - bind extra parameters when creating `std::function` with fewer parameters
3. **Q3**: Network-to-function mapping - map by index (args[i] → inputs[i], return ← outputs[0])
4. **Q4**: JSON representation - explicit `make_function` constructor nodes

### Example Workflow

```cpp
// Register square function
auto square = [](double x) { return x * x; };
NodeObject::register_function(square, {"square", "result", "x"});
// This auto-registers std::function<double(double)>

// Create make_function constructor
template<typename R, typename... Args>
auto make_function_constructor(/* ... */) {
    // Returns std::function<R(Args...)> wrapping the source function
}

// Register map function
template<typename T>
std::vector<T> map(const std::vector<T>& vec, std::function<T(T)> fn) {
    std::vector<T> result;
    for (const auto& elem : vec) result.push_back(fn(elem));
    return result;
}
NodeObject::register_function(map<double>, {"map", "result", "vector", "function"});
```

**JSON Workflow**:
```json
{
  "nodes": {
    "0": {"type": "square"},
    "1": {"type": "make_function<double(double)>"},
    "2": {"type": "std::vector<double>", "value": "[1,2,3,4]"},
    "3": {"type": "map<double>"}
  },
  "edges": {
    "0": {"source": 0, "target": 1, "source_output": 0, "target_input": 0},
    "1": {"source": 1, "target": 3, "source_output": 0, "target_input": 1},
    "2": {"source": 2, "target": 3, "source_output": 0, "target_input": 0}
  }
}
```

## Build and Test

### Build System
CMake-based build with Docker container support.

**Build Commands**:
```bash
# In Docker container named 'coral'
cd /workspace
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Running Tests
```bash
# Run all tests
ctest --output-on-failure

# Run specific test
./build/gtests/coral_tests --gtest_filter="Network.BareMinimal"

# Run main program
./build/coral run test_files/mwe.json
```

### Docker Container
The project runs in a Docker container with deal.II and dependencies pre-installed.

**Access Container**:
```bash
docker exec -it coral bash
```

## Code Patterns and Conventions

### Type Registration Pattern

```cpp
// Elementary type (trivially constructible/copyable)
NodeObject::register_elementary_type<double>();

// Empty constructor type (non-copyable, no constructor args)
NodeObject::register_type<dealii::Triangulation<2>>();

// Constructor with arguments
NodeObject::register_type<MyClass, int, std::string>({"arg1", "arg2"});

// Free function
auto my_fn = [](const double& a, const double& b) { return a + b; };
NodeObject::register_function(my_fn, {"my_fn", "result", "a", "b"});

// Method
NodeObject::register_method(&MyClass::my_method,
    {"method_name", "object", "result", "arg1"});
```

### Node Creation Pattern

```cpp
// From value
auto node1 = make_node(5.0);

// From type name
auto node2 = make_node<dealii::Triangulation<2>>();

// From function name
auto node3 = make_method_node("my_fn", my_fn);
```

### Connection Pattern

```cpp
network.add_connection(source_id, target_id, source_output, target_input);
// Or equivalently:
target_node->bind_input(target_input, source_node->get_output(source_output));
```

### Error Handling

- Exceptions are thrown for type mismatches, unready nodes, missing registrations
- Use `slog_*` macros for logging (`slog_info`, `slog_debug`, `slog_error`)
- Check `node->ready()` before accessing values

## Important Notes

### Thread Safety
- Type registration is NOT thread-safe - do it single-threaded before parallel execution
- Node execution is thread-safe via Taskflow
- Network copying creates independent copies (nodes are cloned)

### Node Lifetime
- Nodes are managed via `std::shared_ptr<NodeObject>` (aliased as `NodeObjectPtr`)
- Circular references are possible - be careful with network-as-node scenarios
- When a network is copied, nodes are deep-copied

### Pass-Through Parameters
- Non-const reference arguments (`T&`) are marked as `ConnectionType::pass_through`
- They act as both input and output
- The same node appears in both input and output lists

### Network as Node
- Networks can themselves be nodes
- This enables hierarchical composition
- Network interface is determined by unbound inputs/outputs

## Common Pitfalls

1. **Forgetting to register types**: All types must be registered before use
2. **Type name mismatches**: JSON type strings must exactly match registered names
3. **Unready nodes**: Accessing values before node execution throws
4. **Connection indices**: Output -1 means "self", but in connections it's mapped to 0
5. **Argument order**: Method arguments include object as first argument, then optional return, then parameters

## Next Steps

Follow `plan.md` for implementing the higher-order functions feature. Start with Phase 1 (Foundation) and proceed sequentially.

## References

- **deal.II Documentation**: https://www.dealii.org/
- **Taskflow Documentation**: https://taskflow.github.io/
- **EnTT Meta**: https://github.com/skypjack/entt
- **nlohmann/json**: https://github.com/nlohmann/json

## Troubleshooting

### Build Errors
- Ensure you're in the Docker container
- Check that CMake configuration succeeded
- Verify all dependencies are present

### Test Failures
- Check logs in `build/Testing/Temporary/LastTest.log`
- Use `--output-on-failure` flag with ctest
- Run individual tests with `--gtest_filter` for isolation

### Runtime Errors
- "Type not registered": Call appropriate `register_*` function
- "Arguments not ready": Check node dependencies and execution order
- "Could not cast": Type mismatch between expected and actual types

## Contact and Support

For questions or issues, refer to the project's GitHub issues or contact the maintainer.
