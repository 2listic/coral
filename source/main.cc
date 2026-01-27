#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#define JSON_DIAGNOSTICS 1
#include <CLI11/CLI11.hpp>
#include <nlohmann/json.hpp> // JSON library

#include <filesystem>

#include "coral.h"
#include "coral_network.h"
#include "register_types.h"
#include "slog.h"
#include "taskflow/taskflow.hpp" // Taskflow library

using json   = nlohmann::json;
namespace fs = std::filesystem;

using namespace coral;

void
dump_registry(const fs::path &outpath)
{
  auto          json = coral::NodeObject::get_registry();
  std::ofstream output{outpath};
  output << std::setw(4) << json << std::endl;
}

namespace
{
  struct SlogGuard
  {
    SlogGuard()
    {
      slog_init("coral", SLOG_FLAGS_ALL, 1);
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
main(int argc, char *argv[])
{
  SlogGuard slog_guard;
  (void)slog_guard;

  // Parse command line arguments

  CLI::App app{"dealii-backend. A backend for coral."};

  app.failure_message([&](const CLI::App *app, const CLI::Error &error) {
    slog_error("Error: %s", error.what());
    slog_error("%s", app->help().c_str());
    return "";
  });

  app.require_subcommand(1, 1);

  CLI::App *register_sub = app.add_subcommand("register", "Register node type");

  fs::path register_path{"node_types.json"};
  register_sub
    ->add_option("register_path",
                 register_path,
                 "Output path of node registry json")
    ->capture_default_str()
    ->type_name("PATH");

  CLI::App *run_sub = app.add_subcommand("run", "Run a certain graph");
  fs::path  input_json;
  fs::path  graph_path{"network.dot"};

  run_sub
    ->add_option("input_json", input_json, "Input json of the graph to run")
    ->required()
    ->check(CLI::ExistingPath.description(""))
    ->type_name("PATH");

  run_sub
    ->add_option("--register",
                 register_path,
                 "Output path of node registry json")
    ->capture_default_str()
    ->type_name("PATH")
    ->expected(0, 1)
    ->multi_option_policy(CLI::MultiOptionPolicy::Throw);

  run_sub->add_option("--graph", graph_path, "Output path of graph dot file")
    ->capture_default_str()
    ->type_name("PATH")
    ->expected(0, 1)
    ->multi_option_policy(CLI::MultiOptionPolicy::Throw);

  CLI11_PARSE(app, argc, argv);

  bool run        = run_sub->parsed();
  bool dump_reg   = register_sub->parsed() || run_sub->count("--register");
  bool dump_graph = run_sub->count("--graph");


  // do the job

  coral::register_all_types();
  slog_info("Registered all types.");

  if (dump_reg)
    {
      dump_registry(register_path);
      slog_info("Dumped registered nodes to %s.", register_path.c_str());
    }

  if (!run)
    return EXIT_SUCCESS;

  std::ifstream input{input_json};
  if (!input.good())
    {
      slog_error("Could not open %s.", input_json.c_str());
      return EXIT_FAILURE;
    }
  slog_info("File %s opened.", input_json.c_str());

  json data;
  input >> data;
  slog_info("File %s read.", input_json.c_str());

  coral::Network network;
  network.from_json(data);
  slog_info("Built network from data.");

  if (dump_graph)
    {
      network.output_dot(graph_path);
      slog_info("Wrote network graph to %s.", graph_path.c_str());
    }

  network.run();
  slog_info("Network run completed.");

  return EXIT_SUCCESS;
}
