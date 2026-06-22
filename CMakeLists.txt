cmake_minimum_required(VERSION 3.16)

project(coral LANGUAGES C CXX)

option(CORAL_BUILD_BACKEND_DEALII "Build the deal.II backend (plugin + CLI)." ON)
option(CORAL_BUILD_TESTS "Build tests (backend-specific where applicable)." ON)
option(CORAL_BUILD_STANDALONES "Build the standalone deal.II programs in standalones/." ON)
option(CORAL_BUILD_SHARED_CORE "Build coral_core as a shared library (recommended for plugins)." ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CORAL_BUILD_BACKEND_DEALII AND NOT CORAL_BUILD_SHARED_CORE)
  message(WARNING "CORAL_BUILD_BACKEND_DEALII=ON with CORAL_BUILD_SHARED_CORE=OFF may prevent plugins from sharing the type registry with the host.")
endif()

if(CORAL_BUILD_TESTS)
  # Enable CTest for comprehensive test discovery
  enable_testing()
  # Custom target for running all tests via custom targets (faster than ctest)
  # Note: "test" is reserved by CTest, so we use "run_all_tests"
  add_custom_target(run_all_tests)
endif()

add_subdirectory(core)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CORAL_BUILD_BACKEND_DEALII)
  add_subdirectory(backends/dealii)
endif()

if(CORAL_BUILD_STANDALONES)
  add_subdirectory(standalones)
endif()

if(CORAL_BUILD_TESTS)
  add_subdirectory(core/tests)
  if(TARGET run_all_tests AND TARGET run_coral_core_tests)
    add_dependencies(run_all_tests run_coral_core_tests)
  endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(coral PRIVATE Threads::Threads)

