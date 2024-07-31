#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe.h>
#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria.h>

#include "coral.h"

namespace coral
{
  using namespace dealii;

  inline void
  register_non_dimensional_types()
  {
    NodeObject::register_elementary_type<std::string>();
    NodeObject::register_elementary_type<unsigned int>();
    NodeObject::register_elementary_type<int>();
    NodeObject::register_elementary_type<double>();
    NodeObject::register_elementary_type<bool>();

    NodeObject::register_elementary_type<types::boundary_id>();
    NodeObject::register_elementary_type<types::subdomain_id>();
    NodeObject::register_elementary_type<types::manifold_id>();
    NodeObject::register_elementary_type<types::material_id>();

    NodeObject::register_type<GridOut>();

    NodeObject::register_derived_type<std::ostream, std::ofstream, std::string>(
      "file_name");
  }

  template <int dim, int spacedim>
  inline void
  register_dimensional_types()
  {
    NodeObject::register_type<Triangulation<dim, spacedim>>();
    NodeObject::register_type<DoFHandler<dim, spacedim>,
                              Triangulation<dim, spacedim>>("triangulation");

    NodeObject::register_function<void,
                                  Triangulation<dim, spacedim> &,
                                  const std::string &,
                                  const std::string &>(
      GridGenerator::generate_from_name_and_arguments<dim, spacedim>,
      {"GridGenerator::generate_from_name_and_arguments<" +
         Utilities::dim_string(dim, spacedim) + ">",
       "triangulation",
       "grid_generator_function_name",
       "grid_generator_function_arguments"});

    NodeObject::
      register_method<Triangulation<dim, spacedim>, void, unsigned int>(
        &Triangulation<dim, spacedim>::refine_global,
        {"Triangulation<" + Utilities::dim_string(dim, spacedim) +
           ">::refine_global",
         "triangulation",
         "n_refinements"});

    NodeObject::register_method<GridOut,
                                void,
                                const Triangulation<dim, spacedim> &,
                                std::ostream &>(
      &GridOut::write_vtk<dim, spacedim>,
      {"GridOut::write_vtk<" + Utilities::dim_string(dim, spacedim) + ">",
       "grid_out",
       "triangulation",
       "output_file"});
  }

  inline void
  register_all_types()
  {
    register_non_dimensional_types();
    register_dimensional_types<2, 2>();
    // register_dimensional_types<3, 3>();
    // register_dimensional_types<2, 3>();
  };

} // namespace coral
#endif