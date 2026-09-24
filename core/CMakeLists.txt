if(CORAL_BUILD_SHARED_CORE)
  set(_coral_core_library_type SHARED)
else()
  set(_coral_core_library_type STATIC)
endif()

add_library(coral_core ${_coral_core_library_type}
  source/coral_implementation.cc
  source/coral_network_implementation.cc
  source/node_object.cc
  source/slog/slog.c)

set_target_properties(coral_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(coral_core PUBLIC cxx_std_17)

target_include_directories(coral_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/entt/single_include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_definitions(coral_core
  PUBLIC
    $<BUILD_INTERFACE:SOURCE_DIR="${PROJECT_SOURCE_DIR}">
    JSON_DIAGNOSTICS=1)

set_target_properties(coral_core PROPERTIES EXPORT_NAME core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(coral_core PRIVATE
    -Wno-array-bounds -Wno-maybe-uninitialized -Wno-class-memaccess)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(coral_core PRIVATE
    -Wno-array-bounds -Wno-uninitialized -Wno-self-assign-overloaded)
endif()

# Main CLI (loads backend plugins, dumps registries, runs graphs).
# Main CLI (loads backend plugins, dumps registries, runs graphs).
add_executable(coral
  source/backend_main.cc)

target_compile_features(coral PRIVATE cxx_std_17)
target_link_libraries(coral PRIVATE coral_core)

# The installed executable and library are kept in configuration-specific
# directories (for example, bin/Debug and lib/Debug).  Make the executable
# find the matching installed coral_core without requiring LD_LIBRARY_PATH or
# DYLD_LIBRARY_PATH.
if(APPLE)
  set(_coral_install_rpath
    "@loader_path/../../${CMAKE_INSTALL_LIBDIR}/$<CONFIG>")
elseif(UNIX)
  set(_coral_install_rpath
    "$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}/$<CONFIG>")
endif()

if(_coral_install_rpath)
  set_target_properties(coral PROPERTIES
    INSTALL_RPATH "${_coral_install_rpath}")
endif()

# Keep build output warning-free by default.
if(MSVC)
  target_compile_options(coral PRIVATE /W0)
else()
  target_compile_options(coral PRIVATE -w)
endif()

if(APPLE)
  # dlopen is in libSystem on macOS.
elseif(UNIX)
  target_link_libraries(coral PRIVATE dl)
endif()

# Keep build output warning-free by default.
if(MSVC)
  target_compile_options(coral_core PRIVATE /W0)
else()
  target_compile_options(coral_core PRIVATE -w)
endif()
