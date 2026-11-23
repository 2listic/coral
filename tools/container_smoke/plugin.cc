#include "coral.h"
#include "coral_plugin.h"

CORAL_PLUGIN_EXPORT int
coral_load_plugin(const char *, const CoralLogger *)
{
  const auto registry = coral::NodeObject::get_registry();
  return registry.is_null() || registry.is_object() ? 0 : 1;
}

CORAL_PLUGIN_EXPORT void
coral_unload_plugin()
{}

CORAL_PLUGIN_EXPORT const char *
coral_plugin_name()
{
  return "container-sdk-smoke";
}
