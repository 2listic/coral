cmake_minimum_required(VERSION 3.16)

find_package(deal.II 9.5 QUIET
  HINTS ${deal.II_DIR} ${DEAL_II_DIR} ../../ $ENV{DEAL_II_DIR})

if(NOT deal.II_FOUND AND NOT DEAL_II_FOUND)
  message(WARNING "deal.II not found; skipping backends/dealii (set CORAL_BUILD_BACKEND_DEALII=OFF to silence)")
  return()
endif()

deal_ii_initialize_cached_variables()

add_library(coral_backend_dealii MODULE
  src/plugin_dealii.cc)

target_compile_features(coral_backend_dealii PRIVATE cxx_std_17)

target_include_directories(coral_backend_dealii
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(coral_backend_dealii
  coral_core)

deal_ii_setup_target(coral_backend_dealii)

if(MSVC)
  target_compile_options(coral_backend_dealii PRIVATE /W0)
else()
  target_compile_options(coral_backend_dealii PRIVATE -w)
endif()

if(CORAL_BUILD_TESTS)
  add_subdirectory(tests)
  if(TARGET test AND TARGET run_dealii_backend_tests)
    add_dependencies(test run_dealii_backend_tests)
  endif()
  if(TARGET test AND TARGET run_dealii_plugin_tests)
    add_dependencies(test run_dealii_plugin_tests)
  endif()
endif()
