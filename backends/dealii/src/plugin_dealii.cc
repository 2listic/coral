#include "coral_plugin.h"
#include "register_types.h"

CORAL_PLUGIN_EXPORT void
coral_backend_register_types()
{
  coral::register_all_types();
}

CORAL_PLUGIN_EXPORT const char *
coral_backend_name()
{
  return "dealii";
}
