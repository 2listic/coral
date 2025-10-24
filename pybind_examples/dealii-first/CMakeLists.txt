cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)

project(coral)

set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED)
find_package(deal.II 9.5 REQUIRED 
    HINTS ${deal.II_DIR} ${DEAL_II_DIR} ../ ../../ $ENV{DEAL_II_DIR})

message(STATUS "DealII found via: ${deal.II_DIR}")

# Check for the namespace targets
if(TARGET dealii::dealii_debug)
    message(STATUS "Found dealii::dealii_debug target")
endif()

if(TARGET dealii::dealii_release) 
    message(STATUS "Found dealii::dealii_release target")
endif()

# List ALL targets again to see what's available now
get_cmake_property(_targets TARGETS)
message(STATUS "=== ALL TARGETS AFTER find_package ===")
foreach(_target ${_targets})
    message(STATUS "Target: ${_target}")
endforeach()

# Try to use the targets
if(TARGET dealii::dealii_debug OR TARGET dealii::dealii_release)
    pybind11_add_module(coral register.cpp)
    
    # Use the appropriate target based on build type
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND TARGET dealii::dealii_debug)
        message(STATUS "Linking with dealii::dealii_debug (Debug build)")
        target_link_libraries(coral PRIVATE dealii::dealii_debug)
    else()
        message(STATUS "Linking with dealii::dealii_release (Release build)")
        target_link_libraries(coral PRIVATE dealii::dealii_release)
    endif()
else()
    message(FATAL_ERROR "No deal.II namespace targets found!")
endif()
