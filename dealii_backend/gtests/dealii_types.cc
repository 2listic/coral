#include <deal.II/dofs/dof_handler.h>

#include <deal.II/grid/grid_generator.h>

#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"

using namespace dealii;
using namespace coral;

// Trivial constructor test
TEST(dealiiTypes, Triangulation)
{
  using type = Triangulation<2>;
  NodeObject::register_type<type>();

  // This builds a Triangulation<2> object
  NodeObject obj(coral::hash<type>());
  obj();
  auto &tria = obj.get<type>();
  GridGenerator::hyper_cube(tria, 0, 1, true);
  ASSERT_EQ(1, tria.n_active_cells());
}

// Non-trivial constructor test
TEST(dealiiTypes, FE_Q)
{
  using type = FE_Q<2>;
  NodeObject::register_type<unsigned int>();
  NodeObject::register_derived_type<FiniteElement<2>, type, unsigned int>(
    "fe_degree");

  // This builds a FE_Q<2> object
  NodeObjectPtr obj    = make_node<type>();
  NodeObjectPtr degree = make_node(1u);
  obj->set_arguments({degree});
  (*obj)();
  auto &fe = obj->get<type>();
  ASSERT_EQ(1, fe.degree);
  ASSERT_EQ(4, fe.dofs_per_cell);

  const auto &fe_base = obj->get<FiniteElement<2>>();
  ASSERT_EQ(1, fe_base.degree);
  ASSERT_EQ(4, fe_base.dofs_per_cell);
}

// Non-trivial constructor test, with non trivial argument
TEST(dealiiTypes, DoFHandler)
{
  using type = DoFHandler<2>;
  NodeObject::register_type<Triangulation<2>>();
  NodeObject::register_type<type, Triangulation<2>>("triangulation");

  // This builds a DoFHandler<2> object
  NodeObjectPtr tria = make_node<Triangulation<2>>();
  NodeObjectPtr dh   = make_node<type>();

  (*tria)();
  dh->set_arguments({tria});
  (*dh)();

  // Check that we are the same guy
  auto &dh_   = dh->get<type>();
  auto &tria_ = tria->get<Triangulation<2>>();
  ASSERT_EQ(&tria_, &dh_.get_triangulation());
}

// Void method test
TEST(dealiiTypes, TriangulationRefineGlobal)
{
  using type = Triangulation<2>;
  NodeObject::register_type<type>();
  NodeObject::register_type<unsigned int>();
  NodeObject::register_method<type, void, unsigned int>(
    &type::refine_global,
    {"Triangulation<2>::refine_global", "triangulation", "n_refinements"});

  // This builds a Triangulation<2> object
  NodeObjectPtr obj = make_node<type>();
  (*obj)();
  auto &tria = obj->get<type>();
  GridGenerator::hyper_cube(tria, 0, 1, true);

  NodeObjectPtr ref   = make_method_node("Triangulation<2>::refine_global",
                                       &Triangulation<2>::refine_global);
  NodeObjectPtr n_ref = make_node(2u);
  ref->set_arguments({obj, n_ref});
  (*ref)();
  ASSERT_EQ(16, tria.n_active_cells());
}

// Void function test
TEST(dealiiTypes, TriangulationHyperCube)
{
  register_all_types();
  // This builds a Triangulation<2> object
  NodeObjectPtr obj = make_node<Triangulation<2>>();
  (*obj)();
  auto &tria = obj->get<Triangulation<2>>();

  NodeObjectPtr make_grid =
    make_method_node("GridGenerator::generate_from_name_and_arguments<2>",
                     &GridGenerator::generate_from_name_and_arguments<2, 2>);

  ASSERT_EQ(
    make_grid->hash(),
    "52ebbe41807005b2GridGenerator::generate_from_name_and_arguments<2>");

  NodeObjectPtr name      = make_node("hyper_cube");
  NodeObjectPtr arguments = make_node("-1.0: 1.0: false");

  make_grid->set_arguments({obj, name, arguments});
  (*make_grid)();
  ASSERT_EQ(1, tria.n_active_cells());
}

// Test for n_inputs() and n_outputs()
TEST(dealiiTypes, NodeObjectInputsOutputs)
{
  using type = Triangulation<2>;
  NodeObject::register_type<type>();

  NodeObjectPtr obj = make_node<type>();
  obj->set_arguments({}); // No arguments for this example

  // Check the number of inputs and outputs
  ASSERT_EQ(obj->n_inputs(), 0);  // Assuming no inputs are registered
  ASSERT_EQ(obj->n_outputs(), 1); // Assuming one output is registered (self)
}
