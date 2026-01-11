#include <gtest/gtest.h>

#include "coral.h"
#include "coral_plugin.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace
{
  using RegisterFn = void (*)();
  using NameFn     = const char *(*)();

  struct PluginHandle
  {
#if defined(_WIN32)
    HMODULE handle = nullptr;
#else
    void *handle = nullptr;
#endif
  };

  auto load_plugin(const char *path) -> PluginHandle
  {
    PluginHandle h;
#if defined(_WIN32)
    h.handle = LoadLibraryA(path);
    if (!h.handle)
      throw std::runtime_error("LoadLibrary failed.");
#else
    h.handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h.handle)
      throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
#endif
    return h;
  }

  template <typename Fn>
  auto load_symbol(const PluginHandle &h, const char *name) -> Fn
  {
#if defined(_WIN32)
    auto *sym = GetProcAddress(h.handle, name);
    if (!sym)
      throw std::runtime_error(std::string("GetProcAddress failed for: ") + name);
    return reinterpret_cast<Fn>(sym);
#else
    dlerror();
    void *sym = dlsym(h.handle, name);
    const char *err = dlerror();
    if (err)
      throw std::runtime_error(std::string("dlsym failed: ") + err);
    return reinterpret_cast<Fn>(sym);
#endif
  }
} // namespace

TEST(Plugin, DealiiRegistersTypes)
{
#ifndef CORAL_TEST_PLUGIN_PATH
  GTEST_SKIP() << "CORAL_TEST_PLUGIN_PATH not provided by build.";
#else
  const auto before = coral::NodeObject::get_registry().size();

  PluginHandle plugin = load_plugin(CORAL_TEST_PLUGIN_PATH);
  auto reg = load_symbol<RegisterFn>(plugin, "coral_backend_register_types");
  auto name = load_symbol<NameFn>(plugin, "coral_backend_name");

  ASSERT_STREQ(name(), "dealii");
  reg();

  const auto after = coral::NodeObject::get_registry().size();
  EXPECT_GT(after, before);
  EXPECT_GT(after, 10u);
#endif
}

