#pragma once

// Plugin-side logging macros.
//
// Include this header in plugin translation units instead of slog.h.
// Call coral_set_logger() before using any macro.
//
// Each message is automatically tagged with the plugin name returned by
// coral_plugin_name(), which is stored on the first coral_set_logger() call.
//
// The plugin must NOT link against slog.

#include "coral_logger.h"
#include "coral_plugin.h"

inline const CoralLogger *coral_active_logger      = nullptr;
inline const char        *coral_active_plugin_name = nullptr;

// clang-format off
#define coral_log(fmt, ...)       do { if (coral_active_logger) coral_active_logger->display(SLOG_NOTAG, 1, "[%s] " fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_note(fmt, ...)  do { if (coral_active_logger) coral_active_logger->display(SLOG_NOTE,  1, "[%s] " fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_info(fmt, ...)  do { if (coral_active_logger) coral_active_logger->display(SLOG_INFO,  1, "[%s] " fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_warn(fmt, ...)  do { if (coral_active_logger) coral_active_logger->display(SLOG_WARN,  1, "[%s] " fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_debug(fmt, ...) do { if (coral_active_logger) coral_active_logger->display(SLOG_DEBUG, 1, "[%s] " fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_error(fmt, ...) do { if (coral_active_logger) coral_active_logger->display(SLOG_ERROR, 1, "[%s] " fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_trace(fmt, ...) do { if (coral_active_logger) coral_active_logger->display(SLOG_TRACE, 1, "[%s] " SLOG_THROW_LOCATION fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
#define coral_log_fatal(fmt, ...) do { if (coral_active_logger) coral_active_logger->display(SLOG_FATAL, 1, "[%s] " SLOG_THROW_LOCATION fmt, coral_active_plugin_name, ##__VA_ARGS__); } while (0)
// clang-format on
