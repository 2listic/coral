find_package(GTest)
if(NOT GTest_FOUND)
  message(WARNING "GTest not found; skipping core tests")
  return()
endif()

file(GLOB _test_files CONFIGURE_DEPENDS *.cc)

add_executable(coral_core_tests ${_test_files})
target_link_libraries(coral_core_tests coral_core ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})

if(GTEST_INCLUDE_DIRS)
  target_include_directories(coral_core_tests SYSTEM PRIVATE ${GTEST_INCLUDE_DIRS})
endif()

if(MSVC)
  target_compile_options(coral_core_tests PRIVATE /W0)
else()
  target_compile_options(coral_core_tests PRIVATE -w)
endif()

add_custom_target(run_coral_core_tests
  COMMAND $<TARGET_FILE:coral_core_tests>
  DEPENDS coral_core_tests
  USES_TERMINAL)

# Register tests with CTest using GoogleTest discovery
include(GoogleTest)
gtest_discover_tests(coral_core_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  PROPERTIES LABELS "core")

