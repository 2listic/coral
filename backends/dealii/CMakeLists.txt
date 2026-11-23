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

if(CORAL_INSTALL)
  set_target_properties(coral_backend_dealii PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE)
  if(APPLE)
    set_property(TARGET coral_backend_dealii APPEND PROPERTY
      INSTALL_RPATH "@loader_path")
  elseif(UNIX)
    set_property(TARGET coral_backend_dealii APPEND PROPERTY
      INSTALL_RPATH "$ORIGIN")
  endif()
  install(TARGETS coral_backend_dealii
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>)
endif()

if(MSVC)
  target_compile_options(coral_backend_dealii PRIVATE /W0)
else()
  target_compile_options(coral_backend_dealii PRIVATE -w)
endif()

if(CORAL_BUILD_TESTS)
  add_subdirectory(tests)
  if(TARGET run_all_tests AND TARGET run_dealii_backend_tests)
    add_dependencies(run_all_tests run_dealii_backend_tests)
  endif()
  if(TARGET run_all_tests AND TARGET run_dealii_plugin_tests)
    add_dependencies(run_all_tests run_dealii_plugin_tests)
  endif()
endif()
