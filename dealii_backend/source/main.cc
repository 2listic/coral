#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#include "coral.h"
#include "json/json.hpp"         // JSON library
#include "taskflow/taskflow.hpp" // Taskflow library

using json = nlohmann::json;

using namespace coral;

int
main()
{
  NodeObject::register_elementary_type<unsigned int>();
  NodeObject::register_elementary_type<double>();
  NodeObject::register_type<dealii::Triangulation<2>>();
  NodeObject::register_method<dealii::Triangulation<2>, void, unsigned int>(
    &dealii::Triangulation<2>::refine_global,
    {"dealii::Triangulation<2>::refine_global",
     "triangulation",
     "n_refinements"});

  auto j = NodeObject::get_registry();
  std::cout << std::setw(4) << j << std::endl;
}