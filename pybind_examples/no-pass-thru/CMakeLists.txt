cmake_minimum_required(VERSION 3.22)

project(no-pass-thru LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall          # all important warnings
        -Wextra        # extra warnings
        -Wpedantic     # strict ISO C++
        -Wconversion   # implicit conversions
        -Wsign-conversion
        -Wshadow
        -Wfloat-equal
        -Wcast-align
        -Wlogical-op
        -Wundef
        -Wdouble-promotion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wredundant-decls
        -Wunused
        -Wuninitialized
        -Wmaybe-uninitialized
    )
endif()

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 3.0 REQUIRED)

pybind11_add_module(no_pass_thru register.cpp)