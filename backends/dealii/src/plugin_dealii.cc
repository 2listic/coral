#include <deal.II/base/mpi.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include "coral_plugin.h"
#include "register_types.h"

using json      = nlohmann::json;
using MPIHandle = dealii::Utilities::MPI::MPI_InitFinalize;

static std::unique_ptr<MPIHandle> mpi_session{};

CORAL_PLUGIN_EXPORT int
coral_load_plugin(const char *subjson)
{
  std::cout << "LOADING DEALII PLUGIN" << std::endl;

  bool                     mpi_enabled = false;
  std::vector<std::string> args;
  unsigned int max_num_threads = dealii::numbers::invalid_unsigned_int;


  if (subjson)
    {
      try
        {
          json init_json = json::parse(subjson);
          std::cout << init_json.dump() << std::endl;
          if (init_json.contains("MPI"))
            {
              if (init_json["MPI"].contains("enabled"))
                mpi_enabled = init_json["MPI"].value("enabled", mpi_enabled);

              if (init_json["MPI"].contains("args"))
                args = init_json["MPI"].value("args", args);

              if (init_json["MPI"].contains("max_num_threads"))
                max_num_threads =
                  init_json["MPI"].value("max_num_threads", max_num_threads);
            }
        }
      catch (json::parse_error &)
        {
          return 1;
        }
    }
  std::vector<char *> argv_storage;
  argv_storage.reserve(args.size());

  for (auto &s : args)
    argv_storage.push_back(s.data());

  int    argc = static_cast<int>(argv_storage.size());
  char **argv = argv_storage.data();

  std::cout << "MPI ENABLED: " << std::boolalpha << mpi_enabled << std::endl;

  if (mpi_enabled)
    mpi_session = std::make_unique<MPIHandle>(argc, argv, max_num_threads);

  coral::register_all_types();

  return 0;
}

CORAL_PLUGIN_EXPORT void
coral_unload_plugin()
{
  std::cout << "UNLOADING DEALII PLUGIN" << std::endl;
  mpi_session.reset();
}

CORAL_PLUGIN_EXPORT const char *
coral_plugin_name()
{
  return "dealii";
}
