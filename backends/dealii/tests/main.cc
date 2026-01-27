#include <gtest/gtest.h>

#include "slog.h"

namespace
{
  struct SlogGuard
  {
    SlogGuard()
    {
      slog_init("gtests", SLOG_FLAGS_ALL, 1);
      slog_config_t cfg;
      slog_config_get(&cfg);
      cfg.nFlush = 1;
      slog_config_set(&cfg);
    }

    ~SlogGuard()
    {
      slog_destroy();
    }
  };
} // namespace

int
main(int argc, char **argv)
{
  SlogGuard slog_guard;
  (void)slog_guard;

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
