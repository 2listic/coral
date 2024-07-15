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
  NodeObject::register_type<dealii::Triangulation<2>>();
  NodeObject::register_method<dealii::Triangulation<2>, void, unsigned int>(
    &dealii::Triangulation<2>::refine_global,
    {"dealii::Triangulation<2>::refine_global",
     "triangulation",
     "n_refinements"});

  auto j = NodeObject::get_registry();

  std::cout << std::setw(4) << j << std::endl;

  NodeObject tria_obj(std::make_shared<dealii::Triangulation<2>>());

  tria_obj();

  auto &tria = tria_obj.template get<dealii::Triangulation<2>>();

  dealii::GridGenerator::hyper_cube(tria, 0, 1, true);

  NodeObject ref("cb40d6a582660ec8"); // This is the refine function
  NodeObject n_ref = 2u;
  ref.set_args({tria_obj, n_ref});
  ref();

  std::cout << "Ref: " << std::endl
            << std::setw(4) << ref.get_info() << std::endl;

  // tria.refine_global(2);

  std::cout << "Number of active cells: " << tria.n_active_cells() << std::endl;
}