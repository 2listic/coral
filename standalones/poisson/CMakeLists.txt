cmake_minimum_required(VERSION 3.13.4)

# This directory can be built either on its own or as part of the coral tree
# (via standalones/CMakeLists.txt). Only bootstrap deal.II when we are the
# top-level project; otherwise the parent has already found it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(poisson)
  find_package(deal.II 9.6.0 REQUIRED
    HINTS ${DEAL_II_DIR} ../ ../../ $ENV{DEAL_II_DIR}
    )
  deal_ii_initialize_cached_variables()
endif()

add_executable(poisson poisson.cc)

# Only third-party dependency: the vendored nlohmann/json single header.
target_include_directories(poisson PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include)

deal_ii_setup_target(poisson)
