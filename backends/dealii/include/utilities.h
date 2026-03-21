#ifndef CORAL_BACKENDS_UTILITIES_H
#define CORAL_BACKENDS_UTILITIES_H

#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/tria.h>

namespace dealii
{
  template <int dim, int spacedim = dim>
  void
  read_grid(const std::string            &file_name,
            Triangulation<dim, spacedim> &triangulation)
  {
    GridIn<dim, spacedim> grid_in;
    grid_in.attach_triangulation(triangulation);
    grid_in.read(file_name);
  }
} // namespace dealii

#endif
