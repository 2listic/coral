cmake_minimum_required(VERSION 3.16)
project(CoralContainerSmoke LANGUAGES CXX)
find_package(coral CONFIG REQUIRED)
add_library(smoke MODULE plugin.cc)
target_link_libraries(smoke PRIVATE coral::core)
enable_testing()
add_test(NAME load_plugin
  COMMAND coral::coral --plugin $<TARGET_FILE:smoke> register)
