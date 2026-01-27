add_executable(coral_dump_registry
  src/coral_dump_registry.cc)

target_compile_features(coral_dump_registry PRIVATE cxx_std_17)
target_link_libraries(coral_dump_registry PRIVATE coral_core)

if(MSVC)
  target_compile_options(coral_dump_registry PRIVATE /W0)
else()
  target_compile_options(coral_dump_registry PRIVATE -w)
endif()

if(APPLE)
  # dlopen is in libSystem on macOS.
elseif(UNIX)
  target_link_libraries(coral_dump_registry PRIVATE dl)
endif()
