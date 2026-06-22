# Standalone deal.II executables. These are NOT linked against coral; each is a
# self-contained program that happens to live in this repository.

# Make sure deal.II is available (the parent tree may not have looked for it,
# e.g. when CORAL_BUILD_BACKEND_DEALII is OFF).
if(NOT deal.II_FOUND AND NOT DEAL_II_FOUND)
  find_package(deal.II 9.6.0 REQUIRED
    HINTS ${DEAL_II_DIR} ../ $ENV{DEAL_II_DIR}
    )
  deal_ii_initialize_cached_variables()
endif()

add_subdirectory(poisson)
