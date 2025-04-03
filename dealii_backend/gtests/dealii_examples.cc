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

  NodeObject::clear_network();

  auto make_grid =
    make_method_node("GridGenerator::generate_from_name_and_arguments<2>",
                     &GridGenerator::generate_from_name_and_arguments<2, 2>);

  auto tria           = make_node<Triangulation<2>>();
  auto grid_name      = make_node("hyper_cube");
  auto grid_arguments = make_node("0: 1: false");
  auto ref            = make_method_node("Triangulation<2>::refine_global",
                              &Triangulation<2>::refine_global);
  auto n_ref          = make_node(2u);
  auto filename       = make_node("grid-1.vtk");
  auto out_file       = make_node<std::ofstream>();
  auto grid_out       = make_node<GridOut>();
  auto write_vtk =
    make_method_node("GridOut::write_vtk<2>", &GridOut::write_vtk<2, 2>);

  connect(make_grid, {{tria, 0}, {grid_name, 0}, {grid_arguments, 0}});

  // make_grid->set_arguments({tria, grid_name, grid_arguments});
  // ref->set_arguments({tria, n_ref});

  connect(ref, {{make_grid, 0}, {n_ref, 0}});

  // out_file->set_arguments({filename});
  connect(out_file, {{filename, 0}});

  // write_vtk->set_arguments({grid_out, tria, out_file});
  connect(write_vtk, {{grid_out, 0}, {tria, 0}, {out_file, 0}});

  auto         &taskflow = NodeObject::get_taskflow();
  std::ofstream dot_file("taskflow.dot");
  taskflow.dump(dot_file);
  dot_file.close();

  // NodeObject::run_network(); // This is what we should have run

  (*tria)();
  (*make_grid)();

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