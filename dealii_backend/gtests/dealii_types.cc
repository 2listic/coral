#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include "coral.h"

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
  NodeObject::register_type<type, unsigned int>("fe_degree");

  // This builds a FE_Q<2> object
  NodeObjectPtr obj    = make_node<type>();
  NodeObjectPtr degree = make_node(1u);
  obj->set_args({degree});
  (*obj)();
  auto &fe = obj->get<type>();
  ASSERT_EQ(1, fe.degree);
  ASSERT_EQ(4, fe.dofs_per_cell);
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
  dh->set_args({tria});
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
    {"dealii::Triangulation<2>::refine_global",
     "triangulation",
     "n_refinements"});

  // This builds a Triangulation<2> object
  NodeObjectPtr obj = make_node<type>();
  (*obj)();
  auto &tria = obj->get<type>();
  GridGenerator::hyper_cube(tria, 0, 1, true);

  NodeObjectPtr ref   = make_node(&type::refine_global);
  NodeObjectPtr n_ref = make_node(2u);
  ref->set_args({obj, n_ref});
  (*ref)();
  ASSERT_EQ(16, tria.n_active_cells());
}

// Void function test
TEST(dealiiTypes, TriangulationHyperCube)
{
  using type = Triangulation<2>;
  NodeObject::register_type<type>();
  NodeObject::register_elementary_type<bool>();
  NodeObject::register_elementary_type<unsigned int>();
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function<void,
                                Triangulation<2> &,
                                const double,
                                const double,
                                const bool>(
    GridGenerator::hyper_cube<2, 2>,
    {"dealii::GridGenerator<2>::hyper_cube",
     "triangulation",
     "left",
     "right",
     "colorize"});

  // This builds a Triangulation<2> object
  NodeObjectPtr obj = make_node<type>();
  (*obj)();
  auto &tria = obj->get<type>();

  NodeObjectPtr left     = make_node(0.0);
  NodeObjectPtr right    = make_node(1.0);
  NodeObjectPtr colorize = make_node(true);

  NodeObjectPtr make_grid = make_node(&GridGenerator::hyper_cube<2, 2>);

  make_grid->set_args({obj, left, right, colorize});
  (*make_grid)();
  ASSERT_EQ(1, tria.n_active_cells());
}