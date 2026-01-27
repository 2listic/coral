cmake_minimum_required(VERSION 3.16)

project(coral LANGUAGES C CXX)

option(CORAL_BUILD_TOOLS "Build small CLI tools." ON)
option(CORAL_BUILD_BACKEND_DEALII "Build the deal.II backend (plugin + CLI)." ON)
option(CORAL_BUILD_TESTS "Build tests (backend-specific where applicable)." ON)
option(CORAL_BUILD_SHARED_CORE "Build coral_core as a shared library (recommended for plugins)." ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CORAL_BUILD_BACKEND_DEALII AND NOT CORAL_BUILD_SHARED_CORE)
  message(WARNING "CORAL_BUILD_BACKEND_DEALII=ON with CORAL_BUILD_SHARED_CORE=OFF may prevent plugins from sharing the type registry with the host.")
endif()

if(CORAL_BUILD_TESTS)
  # We intentionally run GoogleTest executables directly (faster than ctest).
  # Do not call enable_testing() so CMake doesn't generate its own `test` target.
  add_custom_target(test)
endif()

add_subdirectory(core)

if(CORAL_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CORAL_BUILD_BACKEND_DEALII)
  add_subdirectory(backends/dealii)
endif()

if(CORAL_BUILD_TESTS)
  add_subdirectory(core/tests)
  if(TARGET test AND TARGET run_coral_core_tests)
    add_dependencies(test run_coral_core_tests)
  endif()
endif()
