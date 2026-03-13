#pragma once

// Minimal C ABI for backend plugins.
//
// Backends should build a shared library exporting
// coral_backend_register_types(). A host application (runner/manipulator/tools)
// loads the plugin and calls this function to populate coral::NodeObject's
// registry.

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
