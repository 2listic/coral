#include <deal.II/algorithms/general_data_storage.h>

#include <deal.II/base/mutable_bind.h>

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#include <boost/core/type_name.hpp>

#include <any>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "coral.h"
#include "json/json.hpp"         // JSON library
#include "taskflow/taskflow.hpp" // Taskflow library

using json = nlohmann::json;

using namespace coral;

int
main()
{
  NodeObject::register_elementary_type<std::string>();
  NodeObject::register_elementary_type<unsigned int>();
  NodeObject::register_elementary_type<int>();
  NodeObject::register_elementary_type<double>();
  NodeObject::register_elementary_type<dealii::Point<2>>();

  NodeObject::register_type<dealii::Triangulation<2>>();
  NodeObject::register_type<dealii::DoFHandler<2>, dealii::Triangulation<2>>(
    "triangulation");
  NodeObject::register_type<dealii::FE_Q<2>, unsigned int>("degree");
  NodeObject::register_method<dealii::Triangulation<2>, void, unsigned int>(
    &dealii::Triangulation<2>::refine_global,
    {"dealii::Triangulation<2>::refine_global",
     "triangulation",
     "n_refinements"});


  auto j = NodeObject::get_registry();

  std::cout << std::setw(4) << j << std::endl;

  NodeObject tria_obj(std::make_shared<dealii::Triangulation<2>>());
  NodeObject dh_obj(std::make_shared<dealii::DoFHandler<2>>());

  tria_obj();

  dh_obj.set_args({tria_obj});

  dh_obj();

  auto &tria = tria_obj.template get<dealii::Triangulation<2>>();

  dealii::GridGenerator::hyper_cube(tria, 0, 1, true);
  tria.refine_global(2);

  // Now try serializing an object to json.
  json j1 = tria_obj;

  auto tria_obj2 = j1.template get<NodeObject>();

  std::cout << "j1: " << std::endl << std::setw(4) << j1 << std::endl;

  tria_obj2();
}