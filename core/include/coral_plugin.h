#pragma once

// Minimal C ABI for backend plugins.
//
// A plugin is a shared library exporting the three functions below.
// The host loads the library and calls them in order:
//
//   1. coral_plugin_name()  — query the plugin's human-readable name.
//   2. coral_load_plugin()  — initialise the plugin, passing a JSON config
//                             string and a CoralLogger routed to the host's
//                             logging instance.  Returns 0 on success.
//   3. coral_unload_plugin() — tear down the plugin before the library is
//                              unloaded.
//
// The plugin must NOT link against slog.  Use coral_log.h for logging and
// store the CoralLogger received in coral_load_plugin() in
// coral_active_logger (also declared in coral_log.h).

#if defined(_WIN32)
#  define CORAL_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#  define CORAL_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#include "coral_logger.h"

CORAL_PLUGIN_EXPORT int
coral_load_plugin(const char *subjson, const CoralLogger *logger);

CORAL_PLUGIN_EXPORT void
coral_unload_plugin();

CORAL_PLUGIN_EXPORT const char *
coral_plugin_name();
