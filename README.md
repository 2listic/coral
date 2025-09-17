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

- Wraps any C++ type using std::any and shared pointers
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

To dump all the registered type on `noed_types.json` just

```
./dealii_backend.g
```

To execute a graph `input.json`

```
./dealii_backend.g input.json
```
this will also produce `node_types.json` as above and `network.dot` containing
the information to plot the graph.


## Author

Luca Heltai <luca.heltai@unipi.it>
