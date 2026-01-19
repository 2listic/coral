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

int
main(int argc, char *argv[])
{
  // Parse command line arguments

  CLI::App app{"dealii-backend. A backend for coral."};

  app.failure_message([&](const CLI::App *app, const CLI::Error &error) {
    std::cerr << "Error: " << error.what() << "\n\n\n";
    std::cerr << app->help() << std::endl;
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
  std::cout << "Registered all types." << std::endl;

  if (dump_reg)
    {
      dump_registry(register_path);
      std::cout << "Dumped registered node to " << register_path << "."
                << std::endl;
    }

  if (!run)
    return EXIT_SUCCESS;

  std::ifstream input{input_json};
  if (!input.good())
    {
      std::cerr << "Could not open " << input_json << "." << std::endl;
      return EXIT_FAILURE;
    }
  std::cout << "File " << input_json << " opened." << std::endl;

  json data;
  input >> data;
  std::cout << "File " << input_json << " read." << std::endl;

  coral::Network network;
  network.from_json(data);
  std::cout << "Build network from data." << std::endl;

  if (dump_graph)
    {
      network.output_dot(graph_path);
      std::cout << "Network graph " << graph_path << "." << std::endl;
    }

  network.run();
  std::cout << "Network run." << std::endl;

  return EXIT_SUCCESS;
}
