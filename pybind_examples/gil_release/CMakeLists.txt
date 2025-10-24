cmake_minimum_required(VERSION 3.22)

project(primes LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 3.0.1 REQUIRED)

pybind11_add_module(primes primes.cpp)
install(TARGETS primes DESTINATION .)
