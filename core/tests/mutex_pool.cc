#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "coral.h"
#include "coral_network.h"

using namespace coral;

namespace
{
  using clock_type = std::chrono::steady_clock;

  struct Interval
  {
    clock_type::time_point begin;
    clock_type::time_point end;
  };

  std::mutex            intervals_mutex;
  std::vector<Interval> intervals;

  // Run two independent nodes of one function that sleeps 100 ms; the
  // function is registered with `mutex` (possibly null). Returns the two
  // recorded execution intervals, ordered by start time.
  std::vector<Interval>
  run_two_sleepers(const std::string                    &name,
                   std::shared_ptr<std::recursive_mutex> mutex)
  {
    intervals.clear();
    auto sleeper = [](const int &value) {
      const auto begin = clock_type::now();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      const auto end = clock_type::now();
      std::lock_guard<std::mutex> lock(intervals_mutex);
      intervals.push_back({begin, end});
      return value;
    };

    NodeObject::register_elementary_type<int>();
    RegistryMetadata metadata;
    metadata.mutex = mutex;
    NodeObject::register_function(sleeper, {name, "value", "in"}, metadata);

    Network net;
    for (int i = 0; i < 2; ++i)
      {
        auto node = make_method_node(name, sleeper);
        node->set_arguments({make_node(0), make_node(i)});
        net.add_node(node);
      }
    Network::set_touch_file_base_path("mutex_pool_touch");
    net.run();

    std::sort(intervals.begin(), intervals.end(), [](auto &a, auto &b) {
      return a.begin < b.begin;
    });
    return intervals;
  }
} // namespace

namespace
{
  struct RegistryProbe
  {
    explicit RegistryProbe(int) {}
  };

  auto registry_probe_function = [](const int &value) { return value; };
} // namespace

TEST(MutexPool, MutexDoesNotChangeRegistry)
{
  NodeObject::register_elementary_type<int>();

  NodeObject::register_type<RegistryProbe, int>("value");
  NodeObject::register_function(registry_probe_function,
                                {"mutex_pool_probe", "result", "value"});
  const json before = NodeObject::get_registry();

  const auto mutex = mutex_pool::get("mutex_pool_registry");
  NodeObject::register_type<RegistryProbe, int>("value").mutex = mutex;
  RegistryMetadata metadata;
  metadata.mutex = mutex;
  NodeObject::register_function(registry_probe_function,
                                {"mutex_pool_probe", "result", "value"},
                                metadata);
  const json after = NodeObject::get_registry();

  EXPECT_EQ(before, after);
}

TEST(MutexPool, SameKeySameMutex)
{
  EXPECT_EQ(mutex_pool::get("mutex_pool_a"), mutex_pool::get("mutex_pool_a"));
}

TEST(MutexPool, DifferentKeysDifferentMutexes)
{
  EXPECT_NE(mutex_pool::get("mutex_pool_a"), mutex_pool::get("mutex_pool_b"));
}

TEST(MutexPool, SharedMutexSerializesNodes)
{
  const auto result =
    run_two_sleepers("mutex_pool_serial", mutex_pool::get("mutex_pool_test"));
  ASSERT_EQ(result.size(), 2u);
  EXPECT_GE(result[1].begin, result[0].end);
}

TEST(MutexPool, NoMutexRunsNodesInParallel)
{
  const auto result = run_two_sleepers("mutex_pool_parallel", nullptr);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_LT(result[1].begin, result[0].end);
}
