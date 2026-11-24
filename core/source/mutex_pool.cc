#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "coral.h"

namespace coral::mutex_pool
{
  std::shared_ptr<std::recursive_mutex>
  get(const std::string &key)
  {
    static std::mutex                                                   guard;
    static std::map<std::string, std::shared_ptr<std::recursive_mutex>> pool;

    std::lock_guard<std::mutex> lock(guard);
    auto                       &entry = pool[key];
    if (!entry)
      entry = std::make_shared<std::recursive_mutex>();
    return entry;
  }
} // namespace coral::mutex_pool
