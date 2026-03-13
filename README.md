# CORAL - Computational Object-oriented Representation And Library

## Introduction

CORAL is a C++ library for building, connecting, and executing computational
graphs. It provides a flexible framework for representing computational
workflows as directed graphs where nodes represent data or operations, and
edges represent dependencies. The library is designed with parallel
scientific computing in mind.

## Design Philosophy

### Functional approach

In CORAL, every node is designed to be interpreted as a function, represented
by its `operator()`. This functional philosophy ensures that nodes are not
merely containers of data or operations but are active participants in the
computational graph. The execution of a node's function is determined by its
type, which defines its behavior and role within the graph.

- **Node as a Function**: Each node encapsulates a callable function through
  its `operator()`. When invoked, the node performs its designated operation,
  which may include constructing an object, modifying inputs, or producing
  outputs.

- **Type-Driven Behavior**: The behavior of a node during execution is
  controlled by its type. For example:
  - **Constructor Nodes**: These nodes create new objects when executed.
  - **Pass-Through Nodes**: These nodes modify their inputs and pass them
    to their outputs.
  - **Method Nodes**: These nodes invoke a specific method on an object,
    potentially modifying its state or producing a result.
  - **Function Nodes**: These nodes execute a free function, using their
    inputs as arguments and producing outputs.

- **Lazy Evaluation**: Nodes are executed only when their outputs are
  explicitly required by other nodes or the user. This ensures that
  computations are performed efficiently and only when necessary.

- **Input and Output Management**: Each node manages its inputs and outputs
  through a type-safe system. Inputs are connected to other nodes' outputs,
  and the execution of the node ensures that the outputs are updated
  accordingly.

This functional approach ensures that nodes in CORAL are versatile and
adaptable, capable of representing a wide range of computational tasks.
By interpreting nodes as functions, CORAL provides a consistent and
intuitive framework for building and executing complex computational
workflows.

### Key Features

The core design principles of CORAL are:

- **Type Safety**: All connections between nodes are type-checked at runtime,
  ensuring that only compatible types can be connected.

- **Reflection System**: The library implements a runtime reflection system
  that allows for introspection of types, methods, and functions, without
  requiring compiler support for C++ reflection.

- **Polymorphism Support**: The system properly handles inheritance
  hierarchies, allowing derived types to be used where base types are
  expected.

- **Lazy Evaluation**: Computation nodes are only executed when explicitly
  requested or when their outputs are needed by other nodes, allowing for
  efficient execution.

- **Serialization**: All nodes can be serialized to and from JSON, enabling
  workflow persistence and reconstruction, and interfacing with graphical
  node editors and libraries.

## Core Architecture

### NodeObject Class

The `NodeObject` class is the central component of CORAL. Each NodeObject:

- Wraps any C++ type using entt::meta_any and shared pointers
- Maintains type information through a hash-based type system
- Provides inputs and outputs for connecting to other nodes
- Can execute computation through its operator() method

### Connections and Networks

Nodes are connected through a system of typed inputs and outputs:

- **ConnectionType::input**: Read-only parameters
- **ConnectionType::output**: Write-only results
- **ConnectionType::pass_through**: Parameters that are both read and
  modified
- **ConnectionType::self**: The node itself can be used as input and is
  returned as an output

The `Network` class manages collections of nodes and their connections,
allowing for the construction and execution of complex computational
workflows.

## Type Registration System

CORAL requires types to be registered before use. Registration functions
include:

- **register_elementary_type**: For trivially copyable and constructible
  types
- **register_type**: For non-trivially copyable but trivially constructible
  types
- **register_abstract_type**: For interface types that can't be instantiated
  directly
- **register_derived_type**: For types inheriting from a base class
- **register_method**: For member functions (void/non-void, const/non-const)
- **register_function**: For free functions

## Repository Layout

The codebase is intentionally split so the **core graph library** can be built
without any particular “backend” (e.g. deal.II), and UIs can be built without
linking backend types. Backends provide types at runtime via a small plugin ABI
and/or by dumping a `registry.json` for UI authoring.

### Top-level directories

- `core/`
  - `core/include/`: CORAL public headers (NodeObject, Network, JSON, etc.)
  - `core/source/`: CORAL core implementation + CLI sources
  - `core/include/coral_plugin.h`: minimal C ABI that backend plugins export
- `backends/`
  - `backends/dealii/`: one backend implementation (deal.II)
    - `backends/dealii/src/plugin_dealii.cc`: backend plugin (`coral_backend_dealii`)
    - `backends/dealii/include/register_types.h`: deal.II type registration
    - `backends/dealii/tests/`: backend-specific gtests
  - `core/source/backend_main.cc`: main CLI (`coral`)

## Build System (CMake)

The top-level `CMakeLists.txt` composes independent subprojects:

- `coral_core` (always): the core library under `core/`
- `coral_backend_dealii`: deal.II backend plugin under `backends/dealii/`
- `coral`: main CLI under `core/`

### Options

These options are enabled by default:

- `CORAL_BUILD_BACKEND_DEALII=ON` (auto-skips if `deal.II` is not found)
- `CORAL_BUILD_TESTS=ON`

Example:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
```

### Warning-free builds

External libraries (and translation units that include a lot of external
headers, such as deal.II-heavy code) are built with warnings disabled
to keep output clean by default.

## Backend Plugins and Registries

CORAL “registration” is backend-specific: a backend is responsible for calling
`coral::NodeObject::register_*` for its own node types.

### Creating a new plugin

To add a new backend plugin (e.g. `my_backend`), create a new subdirectory and
provide two pieces: (1) a normal C++ registration function that calls
`coral::NodeObject::register_*`, and (2) a shared library that exports the CORAL
plugin ABI from `core/include/coral_plugin.h`.

Suggested layout:

- `backends/my_backend/include/register_types.h` (declares `void register_types();`)
- `backends/my_backend/src/register_types.cc` (implements `register_types()`)
- `backends/my_backend/src/plugin_my_backend.cc` (exports the plugin ABI)
- `backends/my_backend/CMakeLists.txt` (builds the shared library target)

Minimal plugin entry points (`backends/my_backend/src/plugin_my_backend.cc`):

```cpp
#include "coral_log.h"    // instead of slog.h — do NOT link slog
#include "coral_plugin.h"
#include "register_types.h"

CORAL_PLUGIN_EXPORT const char *
coral_plugin_name()
{
  return "my_backend";
}

CORAL_PLUGIN_EXPORT int
coral_load_plugin(const char *subjson, const CoralLogger *logger)
{
  // Store the host logger so that coral_log_XXX macros become active.
  coral_active_logger      = logger;
  coral_active_plugin_name = coral_plugin_name();

  coral_log_info("Loading plugin.");
  register_types();
  return 0; // non-zero signals failure to the host
}

CORAL_PLUGIN_EXPORT void
coral_unload_plugin()
{
  coral_log_info("Unloading plugin.");
}
```

The host passes the optional JSON initialisation string and its own
`CoralLogger` to `coral_load_plugin()`. A non-zero return value is treated as
a load failure by the host.

CMake should build a shared library and link it against `coral_core` plus any
backend dependencies. The plugin must **not** link against slog directly:

```cmake
add_library(coral_backend_my_backend SHARED ...)
target_link_libraries(coral_backend_my_backend PRIVATE coral_core ...)
```

### Logging inside a plugin

Plugins must not initialise or link slog themselves. Instead they use
`core/include/coral_log.h`, which provides macros that forward log calls to
the host's slog instance through the `CoralLogger` pointer received in
`coral_load_plugin()`.

Available macros (mirror the slog levels):

| Macro | slog level |
|---|---|
| `coral_log(fmt, ...)` | `SLOG_NOTAG` |
| `coral_log_note(fmt, ...)` | `SLOG_NOTE` |
| `coral_log_info(fmt, ...)` | `SLOG_INFO` |
| `coral_log_warn(fmt, ...)` | `SLOG_WARN` |
| `coral_log_debug(fmt, ...)` | `SLOG_DEBUG` |
| `coral_log_error(fmt, ...)` | `SLOG_ERROR` |
| `coral_log_trace(fmt, ...)` | `SLOG_TRACE` (includes `[file:line]`) |
| `coral_log_fatal(fmt, ...)` | `SLOG_FATAL` (includes `[file:line]`) |

Every message is automatically prefixed with `[<plugin_name>]` using the value
returned by `coral_plugin_name()`. All macros are safe no-ops if
`coral_active_logger` is null (i.e. before `coral_load_plugin()` is called).

### Dump a registry from a plugin

Use `coral register` (built under `core/`) to load a plugin and write the
registry JSON:

```bash
./build/core/coral --plugin ./build/backends/dealii/libcoral_backend_dealii.(dylib|so|dll) register plugin_init.json --registry-path registry.json
```

Here in `plugin_init.json` the field `plugin`, if present, is passed as json to plugin for initialization. Coral is transparent to this initialization.

## Tests

Core tests live in `core/tests/`. Backend-specific tests live next to the backend
(`backends/dealii/tests/`).
Instead of using `ctest`, the build provides a fast path to run the gtest
executable directly:

```bash
cmake --build build --target run_dealii_backend_tests
```

Run core tests:

```bash
cmake --build build --target run_coral_core_tests
```

Or run the binary yourself:

```bash
./build/backends/dealii/tests/dealii_backend_tests
```

```bash
./build/core/tests/coral_core_tests
```

## Execution Model

The execution model is based on the Taskflow library:

1. Nodes are registered as tasks in a taskflow graph
2. Dependencies between tasks are established based on node connections
3. When execution is requested, tasks are run in the correct order
   (potentially in parallel)
4. Results flow through the network as tasks complete

## JSON Serialization

The library provides comprehensive JSON serialization for:

- Type information and relationships
- Node configurations and connections
- Input/output specifications
- Method and function registrations

This enables workflows to be saved, loaded, and shared between applications.

## API Reference (User-Facing)

This section lists the public entry points, grouped by class and purpose.
Each entry is shown with a short example. All symbols are referenced with
their fully qualified names for Doxygen navigation.

### NodeObject (core node API)

#### Construction and helpers

- \ref coral::make_node (value or type)
- \ref coral::make_method_node (function or method)

```cpp
coral::NodeObjectPtr a = coral::make_node(42);
coral::NodeObjectPtr b = coral::make_node<std::string>();
auto sum = [](int x, int y) { return x + y; };
coral::NodeObjectPtr f = coral::make_method_node("sum_ints", sum);
```

#### Execution and value access

- \ref coral::NodeObject::operator()()
- \ref coral::NodeObject::ready
- \ref coral::NodeObject::get
- \ref coral::NodeObject::get_shared
- \ref coral::NodeObject::parse_string
- \ref coral::NodeObject::to_string

```cpp
coral::NodeObject::register_elementary_type<int>();
coral::NodeObjectPtr n = coral::make_node(7);
if (n->ready())
  (void)(*n)();
int value = n->get<int>();
auto ptr = n->get_shared<int>();
std::string as_text = n->to_string();
n->parse_string("8");
```

#### Wiring: inputs and outputs

- \ref coral::NodeObject::get_input
- \ref coral::NodeObject::get_output
- \ref coral::NodeObject::bind_input
- \ref coral::NodeObject::bind_output
- \ref coral::NodeObject::bind_inputs
- \ref coral::connect

```cpp
auto add = [](int a, int b) { return a + b; };
coral::NodeObject::register_function(add, {"add_ints", "sum", "a", "b"});
coral::NodeObjectPtr add_node = coral::make_method_node("add_ints", add);
coral::NodeObjectPtr a = coral::make_node(1);
coral::NodeObjectPtr b = coral::make_node(2);
coral::NodeObjectPtr out = coral::make_node(0);
add_node->bind_input(0, a);
add_node->bind_input(1, b);
add_node->bind_output(0, out);
coral::connect(add_node, {{a, 0}, {b, 0}});
```

#### Binding state and topology queries

- \ref coral::NodeObject::is_bindable
- \ref coral::NodeObject::is_input_bound
- \ref coral::NodeObject::is_output_bound
- \ref coral::NodeObject::is_passthrough_input
- \ref coral::NodeObject::has_unbound_inputs
- \ref coral::NodeObject::has_unbound_outputs

```cpp
bool can_bind = add_node->is_bindable(0);
bool in0 = add_node->is_input_bound(0);
bool out0 = add_node->is_output_bound(0);
bool pass0 = add_node->is_passthrough_input(0);
bool any_in = add_node->has_unbound_inputs();
bool any_out = add_node->has_unbound_outputs();
```

#### Introspection and serialization

- \ref coral::NodeObject::hash
- \ref coral::NodeObject::type_name
- \ref coral::NodeObject::node_type
- \ref coral::NodeObject::n_arguments
- \ref coral::NodeObject::n_inputs
- \ref coral::NodeObject::n_outputs
- \ref coral::NodeObject::get_info
- \ref coral::NodeObject::get_registry

```cpp
std::string h = n->hash();
std::string tn = n->type_name();
coral::NodeType kind = n->node_type();
size_t args = n->n_arguments();
size_t ins = n->n_inputs();
size_t outs = n->n_outputs();
nlohmann::json info = n->get_info();
nlohmann::json registry = coral::NodeObject::get_registry();
```

#### Type registration

- \ref coral::NodeObject::register_elementary_type
- \ref coral::NodeObject::register_type
- \ref coral::NodeObject::register_abstract_type
- \ref coral::NodeObject::register_derived_type
- \ref coral::NodeObject::register_function
- \ref coral::NodeObject::register_method
- \ref coral::NodeObject::register_json_header

```cpp
coral::NodeObject::register_elementary_type<int>();
coral::NodeObject::register_type<dealii::Triangulation<2>>();
struct Base {};
struct Derived : Base {};
struct Example
{
  void set_value(int) {}
};
coral::NodeObject::register_abstract_type<Base>();
coral::NodeObject::register_derived_type<Base, Derived>();
auto fn = [](int x) { return x + 1; };
coral::NodeObject::register_function(fn, {"inc_int", "out", "in"});
coral::NodeObject::register_method(&Example::set_value,
                                   {"set_value", "obj", "value"});
coral::NodeObject::register_json_header<int>("custom_int");
```

### Network (graph API)

#### Construction, mutation, execution

- \ref coral::Network::add_node
- \ref coral::Network::add_connection
- \ref coral::Network::remove_nodes_and_connections
- \ref coral::Network::run
- \ref coral::Network::clear_network

```cpp
coral::Network net;
auto id_a = net.add_node(coral::make_node(1.0), "a");
auto id_b = net.add_node(coral::make_node(2.0), "b");
auto id_sum = net.add_node(coral::make_method_node("sum_ints", sum), "sum");
net.add_connection(id_a, id_sum, 0, 0);
net.add_connection(id_b, id_sum, 0, 1);
net.run();
```

#### Inputs/outputs and inspection

- \ref coral::Network::get_inputs
- \ref coral::Network::get_outputs
- \ref coral::Network::get_input
- \ref coral::Network::get_output
- \ref coral::Network::get_node
- \ref coral::Network::get_node_name
- \ref coral::Network::set_node_name
- \ref coral::Network::n_nodes
- \ref coral::Network::n_connections
- \ref coral::Network::size
- \ref coral::Network::is_connected
- \ref coral::Network::get_connected_nodes
- \ref coral::Network::get_node_connections
- \ref coral::Network::get_registry

```cpp
auto inputs = net.get_inputs();
auto outputs = net.get_outputs();
auto out0 = net.get_output(0);
auto node = net.get_node(id_sum);
net.set_node_name(id_sum, "sum");
bool linked = net.is_connected(id_a, id_sum);
auto targets = net.get_connected_nodes(id_a);
auto conns = net.get_node_connections(id_a);
nlohmann::json reg = net.get_registry();
```

#### Serialization and debug helpers

- \ref coral::Network::from_json
- \ref coral::Network::to_json
- \ref coral::Network::output_dot

```cpp
nlohmann::json serialized = net.to_json();
coral::Network copy;
copy.from_json(serialized);
net.output_dot("network.dot");
```

#### Network node registration

- \ref coral::Network::register_node

```cpp
coral::Network::register_node();
```

### Register* helpers (type bundles)

- \ref coral::register_non_dimensional_types
- \ref coral::register_dimensional_types
- \ref coral::register_all_types

```cpp
coral::register_non_dimensional_types();
coral::register_dimensional_types<2, 2>();
coral::register_all_types();
```

## Example Usage

A typical workflow using CORAL involves:

1. Registering relevant types, and dump the correpsonding json representation
   of the known node types
2. Sending the JSON representation to a remote server (the frontend) for
   processing
3. Creating nodes representing data or computational steps in the frontend,
   and connecting them to the appropriate inputs and outputs
4. Send the resulting json to the backend for execution
5. Executing the graph to perform the computation

## Prectical Usage

The program `coral` has two subcommands (both require `--plugin <path>`):

- `register [register_path]`: simply register all types and dump them to
`register_path`, a json file which defaults to `node_types.json`.
- `run [OPTIONS] input_json`: register all types and run the graph described
in the json file `input_json`. The options are:
  - `--register [register_path]`: dump the types to `register_path`, which
    defaults to `nodes_type.json`;
  - `--graph [graph_path]`: dump the dot file of the network to `graph_path`,
    which defaults to `network.dot`.
  - `--plugin <path>`: path to the backend plugin to load.

Of course `-h` or `--help` to get a usage guide.

## Author

Luca Heltai <luca.heltai@unipi.it>
