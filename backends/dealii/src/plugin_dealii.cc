#include <deal.II/base/mpi.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include "coral_log.h"
#include "coral_plugin.h"
#include "register_types.h"

using json      = nlohmann::json;
using MPIHandle = dealii::Utilities::MPI::MPI_InitFinalize;

static std::unique_ptr<MPIHandle> mpi_session{nullptr};

CORAL_PLUGIN_EXPORT int
coral_load_plugin(const char *subjson, const CoralLogger *logger)
{
  coral_active_logger      = logger;
  coral_active_plugin_name = coral_plugin_name();

  std::cout << "\tcoral_active_logger " << coral_active_logger
            << ", coral_active_plugin_name " << coral_active_plugin_name
            << std::endl;
  coral_log_info("Loading plugin.");

  bool                     mpi_enabled = false;
  std::vector<std::string> args;
  unsigned int max_num_threads = dealii::numbers::invalid_unsigned_int;

  if (subjson)
    {
      coral_log_info("Found initialization file.");

      try
        {
          json init_json = json::parse(subjson);
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
          coral_log_error("Initialization file is not correct.");

          return 1;
        }
    }
  std::vector<char *> argv_storage;
  argv_storage.reserve(args.size());

  for (auto &s : args)
    argv_storage.push_back(s.data());

  int    argc = static_cast<int>(argv_storage.size());
  char **argv = argv_storage.data();

  if (mpi_enabled)
    {
      coral_log_info("MPI enabled with %u max threads.", max_num_threads);
      mpi_session.reset(new MPIHandle(argc, argv, max_num_threads));
    }
  else
    {
      coral_log_info("MPI not enabled.");
    }

  coral::register_all_types();

  return 0;
}

CORAL_PLUGIN_EXPORT void
coral_unload_plugin()
{
  coral_log_info("Unloading plugin.");
  mpi_session.reset();
}

CORAL_PLUGIN_EXPORT const char *
coral_plugin_name()
{
  return "dealii";
}

CORAL_PLUGIN_EXPORT void
coral_set_logger(const CoralLogger *logger)
{
  coral_log_info("Plugin loaded");
}
