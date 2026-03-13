#pragma once

// C ABI logger interface for coral plugins.
//
// The host fills a CoralLogger and passes it to the plugin via
// coral_set_logger(). The plugin should include coral_log.h to get
// logging macros that route through this struct.

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
