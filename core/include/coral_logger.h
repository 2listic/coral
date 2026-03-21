#pragma once

// C ABI logger interface for coral plugins.
//
// The host fills a CoralLogger with a pointer to its slog_display() instance
// and passes it to the plugin as the second argument of coral_load_plugin().
// The plugin stores it in coral_active_logger (declared in coral_log.h) so
// that all coral_log_XXX macros route through the host's logging instance.

#include "slog/slog.h"

// Matches the signature of slog_display(), which is the single underlying
// function that all slog_XXX macros expand to.
using CoralLogFn = void (*)(slog_flag_t flag,
                            uint8_t     newline,
                            const char *fmt,
                            ...);

struct CoralLogger
{
  CoralLogFn display;
};
