cmake_minimum_required(VERSION 3.16)

project(coral VERSION 0.1.0 LANGUAGES C CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CORAL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/coral")

option(CORAL_BUILD_BACKEND_DEALII "Build the deal.II backend (plugin + CLI)." ON)
option(CORAL_BUILD_TESTS "Build tests (backend-specific where applicable)." ON)
option(CORAL_BUILD_SHARED_CORE "Build coral_core as a shared library (recommended for plugins)." ON)
option(CORAL_INSTALL "Install CORAL libraries, headers, and CMake package files." ON)

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

if(CORAL_BUILD_TESTS)
  add_subdirectory(core/tests)
  if(TARGET run_all_tests AND TARGET run_coral_core_tests)
    add_dependencies(run_all_tests run_coral_core_tests)
  endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(coral_core PUBLIC Threads::Threads)
target_link_libraries(coral PRIVATE Threads::Threads)

if(CORAL_INSTALL)
  install(
    TARGETS coral_core coral
    EXPORT coralTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # CORAL's public headers include the bundled header-only dependencies.  Keep
  # the same include layout in the installation prefix so that both CORAL
  # headers and their transitive includes remain self-contained.
  install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/core/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp")
  install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/core/entt/single_include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp")

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/coralConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/coralConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/coralConfig.cmake"
    INSTALL_DESTINATION "${CORAL_INSTALL_CMAKEDIR}")

  install(
    EXPORT coralTargets
    FILE coralTargets.cmake
    NAMESPACE coral::
    DESTINATION "${CORAL_INSTALL_CMAKEDIR}")
  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/coralConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/coralConfigVersion.cmake"
    DESTINATION "${CORAL_INSTALL_CMAKEDIR}")
endif()
