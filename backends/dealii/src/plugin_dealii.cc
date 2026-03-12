#include <nlohmann/json.hpp>

#include <iostream>

#include "coral_plugin.h"
#include "register_types.h"

using json = nlohmann::json;

CORAL_PLUGIN_EXPORT int
coral_load_plugin(const char *subjson)
{
  std::cout << "LOADING DEALII PLUGIN" << std::endl;

  if (subjson)
    {
      json init_json{subjson};
      std::cout << init_json.dump() << std::endl;
    }

  coral::register_all_types();

  return 0;
}

CORAL_PLUGIN_EXPORT void
coral_unload_plugin()
{
  std::cout << "UNLOADING DEALII PLUGIN" << std::endl;
}

CORAL_PLUGIN_EXPORT const char *
coral_backend_name()
{
  return "dealii";
}
