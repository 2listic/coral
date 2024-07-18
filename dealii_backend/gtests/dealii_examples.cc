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

  NodeObject::register_method<Triangulation<2>, void, const unsigned int>(
    &Triangulation<2>::refine_global,
    {"dealii::Triangulation<2>::refine_global",
     "triangulation",
     "n_refinements"});

  auto make_grid =
    make_node(&GridGenerator::generate_from_name_and_arguments<2, 2>);
  auto tria           = make_node<Triangulation<2>>();
  auto grid_name      = make_node("hyper_cube");
  auto grid_arguments = make_node("0: 1: false");
  auto ref      = make_node(coral::hash(&Triangulation<2>::refine_global));
  auto n_ref    = make_node(2u);
  auto filename = make_node("grid-1.vtk");

  make_grid->set_args({tria, grid_name, grid_arguments});
  ref->set_args({tria, n_ref});

  (*tria)();      // Build an empty triangulation
  (*make_grid)(); // Now Tria is a hyper_cube
  ASSERT_EQ(1, tria->get<Triangulation<2>>().n_active_cells());

  (*ref)(); // Refine two times
  ASSERT_EQ(16, tria->get<Triangulation<2>>().n_active_cells());
}