# Make nodes (functions, methods, network...) first class citizen that can be input of other nodes

The program CORAL reads json file. In a file a certain graph is described in terms of nodes and edges. In fist approximation each node is a function (free function, method, network of functions..) and each edge is a value connecting two functions. We want that these nodes can be theirselves input of another node. If the node A is itself input of node B this means that B represent a function (or method...) that takes the function represented bt A as input.

## Project structure

The main program in in `source/main.cc`.
The files in which nodes and networks are defined in `include/*.h`.
There are tests in `gtests` filder.

## Operations

The program run in a docker container named `coral`.
Ask me if you need to compile or run program or tests.

## Present situation

A node represents a function or method or a group of nodes having some input, output or pass through value (an argument passed by non-const ref).
A network of node is built from json file input. If output of note A is connected to input of node B, this means that in order that the execution do node B depends on the output of node A.

## Desiderata

I want to build some nodes (function, methods or network of nodes) having a function as input.
