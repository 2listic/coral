#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include "coral.h"

using namespace dealii;
using namespace coral;

// Void function test
TEST(dealiiExamples, step01)
{
  NodeObject::register_type<Triangulation<2>>();
  NodeObject::register_elementary_type<std::string>();
  NodeObject::register_elementary_type<unsigned int>();
  NodeObject::register_function<void,
                                Triangulation<2> &,
                                const std::string &,
                                const std::string &>(
    GridGenerator::generate_from_name_and_arguments<2, 2>,
    {"dealii::GridGenerator<2>::generate_from_name_and_arguments",
     "triangulation",
     "grid_generator_function_name",
     "grid_generator_function_arguments"});

  NodeObject::register_method<Triangulation<2>, void, unsigned int>(
    &Triangulation<2>::refine_global,
    {"dealii::Triangulation<2>::refine_global",
     "triangulation",
     "n_refinements"});

  NodeObject::register_derived_type<std::ostream, std::ofstream, std::string>(
    "file_name");

  NodeObject::register_type<GridOut>();
  NodeObject::
    register_method<GridOut, void, const Triangulation<2> &, std::ostream &>(
      &GridOut::write_vtk<2, 2>,
      {"dealii::GridOut<2>::write_vtk",
       "grid_out",
       "triangulation",
       "output_file"});

  auto make_grid =
    make_node(&GridGenerator::generate_from_name_and_arguments<2, 2>);
  auto tria           = make_node<Triangulation<2>>();
  auto grid_name      = make_node("hyper_cube");
  auto grid_arguments = make_node("0: 1: false");
  auto ref            = make_node(&Triangulation<2>::refine_global);
  auto n_ref          = make_node(2u);
  auto filename       = make_node("grid-1.vtk");
  auto out_file       = make_node<std::ofstream>();
  auto grid_out       = make_node<GridOut>();
  auto write_vtk      = make_node(&GridOut::write_vtk<2, 2>);


  make_grid->set_args({tria, grid_name, grid_arguments});
  ref->set_args({tria, n_ref});
  out_file->set_args({filename});
  write_vtk->set_args({grid_out, tria, out_file});

  (*tria)();      // Build an empty triangulation
  (*make_grid)(); // Now Tria is a hyper_cube

  ASSERT_EQ(1, tria->get<Triangulation<2>>().n_active_cells());

  (*ref)(); // Refine two times
  ASSERT_EQ(16, tria->get<Triangulation<2>>().n_active_cells());

  (*grid_out)();  // Create a new gridout object
  (*out_file)();  // Create a new file
  (*write_vtk)(); // Write the grid to a file
}