# Coral Debug Issue and Plan

This project is called CORAL (see `README.md`).

## Operation

The program run on a docker container named "coral".
The work dir is `/app` inside the container.
The number of threads can be set exporting `export THREADS=1`. It *must* be set 1, for the moment (single thread, serialization).
To run the program, inside the container `/app/build/dealii_backend.g run FILENAME`. 
The filename (`FILENAME` in the above line) causing problem is `/app/test_files/vtk-ko.json`.

## Issue

The program (whose main is `/app/source/main.cc`) reads the .json file and builds the tensorflow network(s). Each task is a lazily-evalued function (see `/app/include/coral.h` and `/app/include/coral_network.h`).
The code have been instrumented. So in the output you can read in the log (`output.log`):
   1. When a node (task) is added to the network(s);
   2. Before execution all the tasks of the networks(s) are printed out;
   3. When the networks get executed there is a log both from inside the lambda function of the task and using a taskflow observer;
   4. Taskflow dumps the network executed .dot file (`network_0.dot` and `network_1.dot`).
All these logs, on our test case (file `/app/test_files/vtk-ko.json`) agrees on the fact that:
   1. the node "26" has no predecessors;
   2. it must be execuded *before* node "20";
   3. it is *NOT* executed for some reason.
**We want to investigate why this node "26" is not executed although it is (seems) correctly emplaced.**
