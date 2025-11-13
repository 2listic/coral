cmake_minimum_required(VERSION 3.22)

project(virtual LANGUAGES CXX)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 CONFIG REQUIRED)

pybind11_add_module(virtual virtual.cpp)
install(TARGETS virtual DESTINATION .)
