#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp> // JSON library

#include "coral.h"
#include "register_types.h"
#include "taskflow/taskflow.hpp" // Taskflow library

using json = nlohmann::json;

using namespace coral;

int
main()
{
  coral::register_all_types();

  auto j = NodeObject::get_registry();
  std::cout << std::setw(4) << j << std::endl;

  // Now write the registry to a file
  std::ofstream file("node_types.json");
  file << std::setw(4) << j << std::endl;
}