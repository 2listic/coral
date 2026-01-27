#pragma once

// Minimal C ABI for backend plugins.
//
// Backends should build a shared library exporting coral_backend_register_types().
// A host application (runner/manipulator/tools) loads the plugin and calls this
// function to populate coral::NodeObject's registry.

#if defined(_WIN32)
#  define CORAL_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#  define CORAL_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

CORAL_PLUGIN_EXPORT void
coral_backend_register_types();

CORAL_PLUGIN_EXPORT const char *
coral_backend_name();

