cmake_minimum_required(VERSION 3.13.4)

# This directory can be built either on its own or as part of the coral tree
# (via standalones/CMakeLists.txt). Only bootstrap deal.II when we are the
# top-level project; otherwise the parent has already found it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(poisson_mpi)
  find_package(deal.II 9.6.0 REQUIRED
    HINTS ${DEAL_II_DIR} ../ ../../ $ENV{DEAL_II_DIR}
    )
  deal_ii_initialize_cached_variables()
endif()

# This program requires a parallel deal.II (MPI + p4est, plus PETSc or Trilinos
# for the distributed linear algebra). Skip it gracefully otherwise.
if(NOT DEAL_II_WITH_MPI OR NOT DEAL_II_WITH_P4EST OR
   (NOT DEAL_II_WITH_PETSC AND NOT DEAL_II_WITH_TRILINOS))
  message(STATUS
    "poisson_mpi: deal.II lacks MPI/p4est/(PETSc|Trilinos); skipping target.")
  return()
endif()

add_executable(poisson_mpi poisson_mpi.cc)

# No third-party dependencies: parameters are read with deal.II's
# ParameterAcceptor / ParameterHandler instead of the vendored nlohmann/json.
deal_ii_setup_target(poisson_mpi)
