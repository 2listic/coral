find_package(GTest)
find_package(MPI)
if(NOT GTest_FOUND)
  message(WARNING "GTest not found; skipping deal.II backend tests")
  return()
endif()

file(GLOB _test_files CONFIGURE_DEPENDS *.cc)

include(GoogleTest)

# Keep plugin smoke test in its own executable so it doesn't get "help" from
# backend tests that register types directly.
set(_plugin_test_file ${CMAKE_CURRENT_LIST_DIR}/plugin_smoke_test.cc)
set(_mpi_test_file ${CMAKE_CURRENT_LIST_DIR}/dealii_mpi.cc)
list(REMOVE_ITEM _test_files ${_plugin_test_file} ${_mpi_test_file})

# BACKEND TESTS

add_executable(dealii_backend_tests ${_test_files})

target_include_directories(dealii_backend_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include)

target_link_libraries(dealii_backend_tests
  coral_core
  ${GTEST_LIBRARIES}
  ${GTEST_MAIN_LIBRARIES})

if(GTEST_INCLUDE_DIRS)
  target_include_directories(dealii_backend_tests SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
endif()

deal_ii_setup_target(dealii_backend_tests)

if(MSVC)
  target_compile_options(dealii_backend_tests PRIVATE /W0)
else()
  target_compile_options(dealii_backend_tests PRIVATE -w)
endif()

# Prefer running the gtest binary directly (faster iteration than ctest).
add_custom_target(run_dealii_backend_tests
  COMMAND $<TARGET_FILE:dealii_backend_tests>
  DEPENDS dealii_backend_tests
  USES_TERMINAL)

# Register backend tests with CTest
gtest_discover_tests(dealii_backend_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  PROPERTIES LABELS "dealii;backend")

# MPI TESTS

set(_plugin_file ${CMAKE_CURRENT_LIST_DIR}/../src/plugin_dealii.cc)
add_executable(dealii_mpi_tests ${_mpi_test_file} ${_plugin_file})

target_include_directories(dealii_mpi_tests
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include)

target_link_libraries(dealii_mpi_tests
  coral_core
  ${GTEST_LIBRARIES}
  ${GTEST_MAIN_LIBRARIES})

if(GTEST_INCLUDE_DIRS)
  target_include_directories(dealii_mpi_tests SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
endif()

deal_ii_setup_target(dealii_mpi_tests)

if(MSVC)
  target_compile_options(dealii_mpi_tests PRIVATE /W0)
else()
  target_compile_options(dealii_mpi_tests PRIVATE -w)
endif()

# Prefer running the gtest binary directly (faster iteration than ctest).
add_custom_target(run_mpi_backend_tests
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4 "$<TARGET_FILE:dealii_mpi_tests>"
  DEPENDS dealii_mpi_tests
  USES_TERMINAL)

# Register MPI tests with CTest.
# gtest_discover_tests(EXECUTOR) requires CMake >= 3.29. For 3.28 we set the
# CROSSCOMPILING_EMULATOR target property, which gtest_discover_tests respects.
# A wrapper script is used so discovery (--gtest_list_tests) runs with -n 1
# (single clean output) while actual test execution uses -n 4.
if(MPI_FOUND)
  set(_mpi_wrapper ${CMAKE_CURRENT_BINARY_DIR}/mpi_gtest_wrapper.sh)
  file(WRITE ${_mpi_wrapper}
"#!/bin/bash
if [[ \"$*\" == *\"--gtest_list_tests\"* ]]; then
  exec \"$@\"
else
  exec ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4 \"$@\"
fi
")
  file(CHMOD ${_mpi_wrapper}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  set_target_properties(dealii_mpi_tests PROPERTIES
    CROSSCOMPILING_EMULATOR ${_mpi_wrapper})

  gtest_discover_tests(dealii_mpi_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
      LABELS "dealii;mpi"
      SKIP_REGULAR_EXPRESSION "\\[  SKIPPED")
else()
  message(WARNING "MPI not found; skipping MPI tests registration with ctest")
endif()

# PLUGIN TESTS

add_executable(dealii_plugin_tests ${_plugin_test_file})
target_link_libraries(dealii_plugin_tests coral_core dl ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})
if(GTEST_INCLUDE_DIRS)
  target_include_directories(dealii_plugin_tests SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
endif()

target_compile_definitions(dealii_plugin_tests PRIVATE
  CORAL_TEST_PLUGIN_PATH="$<TARGET_FILE:coral_backend_dealii>")

# Ensure the plugin exists before we try to load it.
add_dependencies(dealii_plugin_tests coral_backend_dealii)

if(MSVC)
  target_compile_options(dealii_plugin_tests PRIVATE /W0)
else()
  target_compile_options(dealii_plugin_tests PRIVATE -w)
endif()

add_custom_target(run_dealii_plugin_tests
  COMMAND $<TARGET_FILE:dealii_plugin_tests>
  DEPENDS dealii_plugin_tests coral_backend_dealii
  USES_TERMINAL)

# Register plugin tests with CTest
gtest_discover_tests(dealii_plugin_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  PROPERTIES LABELS "dealii;plugin")
