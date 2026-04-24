#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe.h>
#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>

#include "coral.h"
#include "coral_network.h"
#include "laplace.h"
#include "poisson.h"
#include "utilities.h"

/** \cond INTERNAL */
namespace nlohmann
{
  template <int dim>
  struct adl_serializer<dealii::Point<dim>>
  {
    static void
    to_json(json &j, const dealii::Point<dim> &p)
    {
      j = json::array();
      for (unsigned int i = 0; i < dim; ++i)
        j.push_back(p[i]);
    }



    static void
    from_json(const json &j, dealii::Point<dim> &p)
    {
      for (unsigned int i = 0; i < dim; ++i)
        p[i] = j.at(i).get<double>();
    }
  };
} // namespace nlohmann

/** \endcond */

namespace coral
{
  using namespace dealii;

  inline void
  register_non_dimensional_types()
  {
    // Canonicalize some common standard-library types across compilers.
    coral::detail::set_type_alias<std::string>("std::string");
    coral::detail::set_type_alias<std::ostream>("std::ostream");
    coral::detail::set_type_alias<std::ofstream>("std::ofstream");

    NodeObject::register_elementary_type<std::string>();
    NodeObject::register_elementary_type<unsigned int>();
    NodeObject::register_elementary_type<int>();
    NodeObject::register_elementary_type<double>();
    NodeObject::register_elementary_type<bool>();

    NodeObject::register_elementary_type<types::boundary_id>();
    NodeObject::register_elementary_type<types::subdomain_id>();
    NodeObject::register_elementary_type<types::manifold_id>();
    NodeObject::register_elementary_type<types::material_id>();

    NodeObject::register_elementary_type<std::set<types::boundary_id>>();

    NodeObject::register_type<GridOut>();

    NodeObject::register_derived_type<std::ostream, std::ofstream, std::string>(
      "file_name");
    Network::register_node();
  }



  template <int dim, int spacedim>
  inline void
  register_dimensional_types()
  {
    // When dim == spacedim, create aliases to handle both forms:
    // - Static linking produces: Type<2>
    // - Shared library produces: Type<2, 2>
    // This ensures JSON compatibility between plugin and test builds
    if constexpr (dim == spacedim)
      {
        const std::string dims_short = std::to_string(dim);
        const std::string dims_full =
          dims_short + ", " + std::to_string(spacedim);

        // Alias for all dimensional types
        coral::detail::set_type_alias<Triangulation<dim, spacedim>>(
          "dealii::Triangulation<" + dims_full + ">");
        coral::detail::set_type_alias<FiniteElement<dim, spacedim>>(
          "dealii::FiniteElement<" + dims_full + ">");
        coral::detail::set_type_alias<FE_Q<dim, spacedim>>("dealii::FE_Q<" +
                                                           dims_full + ">");
        coral::detail::set_type_alias<DoFHandler<dim, spacedim>>(
          "dealii::DoFHandler<" + dims_full + ">");
        coral::detail::set_type_alias<PoissonSolver<dim, spacedim>>(
          "PoissonSolver<" + dims_full + ">");
      }

    NodeObject::register_type<Triangulation<dim, spacedim>>();
    NodeObject::register_type<DoFHandler<dim, spacedim>,
                              Triangulation<dim, spacedim>>("triangulation");

    NodeObject::register_function(
      GridGenerator::generate_from_name_and_arguments<dim, spacedim>,
      {"GridGenerator::generate_from_name_and_arguments<" +
         Utilities::dim_string(dim, spacedim) + ">",
       "triangulation",
       "grid_generator_function_name",
       "grid_generator_function_arguments"});

    NodeObject::register_function(read_grid<dim, spacedim>,
                                  {"dealii::read_grid<" +
                                     Utilities::dim_string(dim, spacedim) + ">",
                                   "file_name",
                                   "triangulation"});

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

    NodeObject::register_derived_type<FiniteElement<dim, spacedim>,
                                      FE_Q<dim, spacedim>,
                                      unsigned int>("fe_degree");

    NodeObject::register_type<PoissonSolver<dim, spacedim>,
                              const Triangulation<dim, spacedim> &,
                              const FiniteElement<dim, spacedim> &,
                              const std::string &,
                              const std::string &,
                              const std::set<types::boundary_id> &,
                              const std::string &,
                              const std::set<types::boundary_id> &,
                              const std::string &>(
      {{"triangulation",
        "fe",
        "output_file_name",
        "rhs_function_expression",
        "dirichlet_boundary_ids",
        "dirichlet_function_expression",
        "neumann_boundary_ids",
        "neumann_function_expression"}});

    NodeObject::register_method<PoissonSolver<dim, spacedim>, void>(
      &PoissonSolver<dim, spacedim>::solve,
      {"PoissonSolver::solve<" + Utilities::dim_string(dim, spacedim) + ">",
       "poisson_solver"});

    NodeObject::register_type<LaplaceProblem<dim>>();
    NodeObject::register_method<LaplaceProblem<dim>,
                                void,
                                const std::string &,
                                const std::string &,
                                unsigned int>(
      &LaplaceProblem<dim>::make_grid_from_generator,
      {"LaplaceProblem::make_grid_from_generator<" +
         Utilities::dim_string(dim, spacedim) + ">",
       "laplace_problem",
       "generator_name",
       "generator_arguments",
       "n_refinements"});
    NodeObject::register_method<LaplaceProblem<dim>,
                                void,
                                const std::string &,
                                unsigned int>(
      &LaplaceProblem<dim>::make_grid_from_file,
      {"LaplaceProblem::make_grid_from_file<" +
         Utilities::dim_string(dim, spacedim) + ">",
       "laplace_problem",
       "file_name",
       "n_refinements"});
    NodeObject::register_method<LaplaceProblem<dim>,
                                void,
                                unsigned int,
                                const std::string &>(
      &LaplaceProblem<dim>::run,
      {"LaplaceProblem::run<" + Utilities::dim_string(dim, spacedim) + ">",
       "laplace_problem",
       "n_cycles",
       "output_dir"});
  }



  inline void
  register_all_types()
  {
    register_non_dimensional_types();
    register_dimensional_types<1, 1>();
    register_dimensional_types<1, 2>();
    register_dimensional_types<1, 3>();
    register_dimensional_types<2, 2>();
    register_dimensional_types<2, 3>();
    register_dimensional_types<3, 3>();
  };

} // namespace coral
#endif
