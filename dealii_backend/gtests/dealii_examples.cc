#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"

using namespace dealii;
using namespace coral;

// Void function test
TEST(dealiiExamples, step01)
{
  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

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

  // Check if the file exists
  std::ifstream file("grid-1.vtk");
  ASSERT_TRUE(file.good());

  // Remove the file
  file.close();
  std::remove("grid-1.vtk");
}