#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "coral.h"
#include "coral_plugin.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

using json = nlohmann::json;

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

  auto
  load_plugin(const std::string &path) -> PluginHandle
  {
    PluginHandle h;
#if defined(_WIN32)
    h.handle = LoadLibraryA(path.c_str());
    if (!h.handle)
      throw std::runtime_error("LoadLibrary failed for: " + path);
#else
    h.handle     = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h.handle)
      throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
#endif
    return h;
  }

  template <typename Fn>
  auto
  load_symbol(const PluginHandle &h, const char *name) -> Fn
  {
#if defined(_WIN32)
    auto *sym = GetProcAddress(h.handle, name);
    if (!sym)
      throw std::runtime_error(std::string("GetProcAddress failed for: ") +
                               name);
    return reinterpret_cast<Fn>(sym);
#else
    dlerror();
    void       *sym = dlsym(h.handle, name);
    const char *err = dlerror();
    if (err)
      throw std::runtime_error(std::string("dlsym failed: ") + err);
    return reinterpret_cast<Fn>(sym);
#endif
  }

  void
  unload_plugin(PluginHandle &h)
  {
#if defined(_WIN32)
    if (h.handle)
      FreeLibrary(h.handle);
#else
    if (h.handle)
      dlclose(h.handle);
#endif
    h.handle = nullptr;
  }
} // namespace

int
main(int argc, char **argv)
{
  if (argc != 3)
    {
      std::cerr
        << "Usage: coral_dump_registry <plugin.(so|dylib|dll)> <out.json>\n";
      return 2;
    }

  const std::string plugin_path = argv[1];
  const std::string out_path    = argv[2];

  PluginHandle plugin;
  try
    {
      plugin = load_plugin(plugin_path);
      auto reg =
        load_symbol<RegisterFn>(plugin, "coral_backend_register_types");
      auto name = load_symbol<NameFn>(plugin, "coral_backend_name");

      reg();

      json registry              = coral::NodeObject::get_registry();
      registry["__backend_name"] = name();

      std::ofstream out(out_path);
      out << std::setw(2) << registry << "\n";
      unload_plugin(plugin);
      return 0;
    }
  catch (const std::exception &e)
    {
      std::cerr << "Error: " << e.what() << "\n";
      unload_plugin(plugin);
      return 1;
    }
}
