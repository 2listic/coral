#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <locale>
#include <set>

#include "coral.h"
#include "coral_network.h" // Added include for Network class
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using namespace coral;
using coral_test::ScopedTestOutputDir;

// Function to sum all numbers in a set
unsigned int
sum_set(const std::set<unsigned int> &input_set)
{
  unsigned int sum = 0;
  for (const auto &val : input_set)
    sum += val;
  return sum;
}

TEST(dealiiExamples, SumSetRegistry)
{
  ScopedTestOutputDir output_dir("dealiiExamples_SumSetRegistry");

  // Register the types
  NodeObject::register_elementary_type<unsigned int>();
  NodeObject::register_elementary_type<std::set<unsigned int>>();

  // Register the function
  NodeObject::register_function(sum_set, {"sum_set", "result", "input_set"});

  // Dump the registry to a file
  std::ofstream registry_file(output_dir.path() / "registry.json");
  registry_file << NodeObject::get_registry().dump(2) << std::endl;
  registry_file.close();

  // Check that the file exists
  std::ifstream check_file(output_dir.path() / "registry.json");
  ASSERT_TRUE(check_file.good());
  check_file.close();

  // Load and execute the network from SetSum.json
  Network network;
  network.set_touch_file_base_path(output_dir);
  network.clear_network();

  std::ifstream json_file(SOURCE_DIR "/test_files/SetSum.json");
  ASSERT_TRUE(json_file.is_open()) << "Failed to open SetSum.json file.";

  nlohmann::json json_data;
  json_file >> json_data;
  json_file.close();

  ASSERT_FALSE(json_data.empty()) << "JSON data is empty.";

  // Load the network from JSON
  network.from_json(json_data);

  // Print debugging information
  slog_debug("Network has %u nodes and %u connections",
             network.n_nodes(),
             network.n_connections());

  // Run the network
  network.run();

  // Get the function node (node 1)
  auto function_node = network.get_node(1);
  ASSERT_TRUE(function_node != nullptr) << "Node 1 not found.";

  // For non-void functions, the result is stored in the output of the function
  // node
  slog_debug("Function node has %zu outputs", function_node->n_outputs());

  auto result_node = function_node->get_output(0);
  ASSERT_TRUE(result_node != nullptr) << "Function output not found.";
  ASSERT_TRUE(result_node->ready()) << "Result node not ready.";

  // The result should be 15 (sum of 1 + 2 + 3 + 4 + 5)
  unsigned int result = result_node->get<unsigned int>();
  slog_debug("Function output is: %u.", result);
  ASSERT_EQ(15u, result) << "Expected sum to be 15, got " << result;
}

// Void function test
TEST(dealiiExamples, step01)
{
  ScopedTestOutputDir output_dir("dealiiExamples_step01");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  Network network;
  network.clear_network();

  auto make_grid =
    make_node("GridGenerator::generate_from_name_and_arguments<2>");

  std::string vtk_path       = output_dir.path() / "grid-1.vtk";
  auto        tria           = make_node<Triangulation<2>>();
  auto        grid_name      = make_node("hyper_cube");
  auto        grid_arguments = make_node("0: 1: false");
  auto        ref            = make_node("Triangulation<2>::refine_global");
  auto        n_ref          = make_node(2u);
  auto        filename       = make_node(vtk_path);
  auto        out_file       = make_node<std::ofstream>();
  auto        grid_out       = make_node<GridOut>();
  auto        write_vtk      = make_node("GridOut::write_vtk<2>");

  // connect(make_grid, {{tria, 0}, {grid_name, 0}, {grid_arguments, 0}});

  make_grid->set_arguments({tria, grid_name, grid_arguments});
  ref->set_arguments({tria, n_ref});

  // connect(ref, {{make_grid, 0}, {n_ref, 0}});

  out_file->set_arguments({filename});
  // connect(out_file, {{filename, 0}});

  write_vtk->set_arguments({grid_out, tria, out_file});
  // connect(write_vtk, {{grid_out, 0}, {tria, 0}, {out_file, 0}});

  network.output_dot(output_dir.path() / "taskflow.dot");

  // NodeObject::run_network(); // This is what we should have run

  (*tria)();
  (*make_grid)();

  ASSERT_EQ(1, tria->get<Triangulation<2>>().n_active_cells());

  (*ref)(); // Refine two times
  ASSERT_EQ(16, tria->get<Triangulation<2>>().n_active_cells());

  (*grid_out)();  // Create a new gridout object
  (*out_file)();  // Create a new file
  (*write_vtk)(); // Write the grid to a file

  // Check if the file exists
  std::ifstream file(output_dir.path() / "grid-1.vtk");
  ASSERT_TRUE(file.good());

  file.close();
}


// Void function test
TEST(dealiiExamples, NetworkStep00)
{
  ScopedTestOutputDir output_dir("dealiiExamples_NetworkStep00");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  Network network;
  network.set_touch_file_base_path(output_dir);
  network.clear_network();

  auto make_grid =
    make_node("GridGenerator::generate_from_name_and_arguments<2>");

  auto tria           = make_node<Triangulation<2>>();
  auto grid_name      = make_node("hyper_cube");
  auto grid_arguments = make_node("0: 1: false");
  auto ref            = make_node("Triangulation<2>::refine_global");
  auto n_ref          = make_node(2u);

  // Add nodes to the network
  unsigned int tria_id           = network.add_node(tria);
  unsigned int grid_name_id      = network.add_node(grid_name);
  unsigned int grid_arguments_id = network.add_node(grid_arguments);
  unsigned int make_grid_id      = network.add_node(make_grid);
  unsigned int n_ref_id          = network.add_node(n_ref);
  unsigned int ref_id            = network.add_node(ref);

  // Create connections between nodes
  // Connect make_grid inputs
  network.add_connection(tria_id, make_grid_id, 0, 0);      // tria
  network.add_connection(grid_name_id, make_grid_id, 0, 1); // grid_name
  network.add_connection(grid_arguments_id,
                         make_grid_id,
                         0,
                         2); // grid_arguments

  // Connect ref inputs to make_grid output
  network.add_connection(make_grid_id, ref_id, 0, 0); // tria
  network.add_connection(n_ref_id, ref_id, 0, 1);     // n_ref

  network.output_dot(output_dir.path() / "step-0_taskflow.dot");
  {
    std::ofstream ofile(output_dir.path() / "step-0_network.json");
    ofile << network.to_json().dump(2) << std::endl;
    ofile.close();
  }

  {
    std::ofstream ofile(output_dir.path() / "step-0_registry.json");
    ofile << NodeObject::get_registry().dump(2) << std::endl;
    ofile.close();
  }

  // Execute the network
  network.run();

  // Verify results
  ASSERT_EQ(16, tria->get<Triangulation<2>>().n_active_cells());
}


// Void function test
TEST(dealiiExamples, NetworkStep01)
{
  ScopedTestOutputDir output_dir("dealiiExamples_NetworkStep01");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  Network network;
  network.set_touch_file_base_path(output_dir);
  network.clear_network();

  auto make_grid =
    make_node("GridGenerator::generate_from_name_and_arguments<2>");

  std::string vtk_path       = output_dir.path() / "grid-1.vtk";
  auto        tria           = make_node<Triangulation<2>>();
  auto        grid_name      = make_node("hyper_cube");
  auto        grid_arguments = make_node("0: 1: false");
  auto        ref            = make_node("Triangulation<2>::refine_global");
  auto        n_ref          = make_node(2u);
  auto        filename       = make_node(vtk_path);
  auto        out_file       = make_node<std::ofstream>();
  auto        grid_out       = make_node<GridOut>();
  auto        write_vtk      = make_node("GridOut::write_vtk<2>");

  // Add nodes to the network
  unsigned int tria_id           = network.add_node(tria);
  unsigned int grid_name_id      = network.add_node(grid_name);
  unsigned int grid_arguments_id = network.add_node(grid_arguments);
  unsigned int make_grid_id      = network.add_node(make_grid);
  unsigned int n_ref_id          = network.add_node(n_ref);
  unsigned int ref_id            = network.add_node(ref);
  unsigned int filename_id       = network.add_node(filename);
  unsigned int out_file_id       = network.add_node(out_file);
  unsigned int grid_out_id       = network.add_node(grid_out);
  unsigned int write_vtk_id      = network.add_node(write_vtk);

  // Create connections between nodes
  // Connect make_grid inputs
  network.add_connection(tria_id, make_grid_id, 0, 0);      // tria
  network.add_connection(grid_name_id, make_grid_id, 0, 1); // grid_name
  network.add_connection(grid_arguments_id,
                         make_grid_id,
                         0,
                         2); // grid_arguments

  // Connect ref inputs to make_grid output
  network.add_connection(make_grid_id, ref_id, 0, 0); // tria
  network.add_connection(n_ref_id, ref_id, 0, 1);     // n_ref

  // Connect out_file input
  network.add_connection(filename_id, out_file_id, 0, 0); // filename

  // Connect write_vtk inputs
  network.add_connection(grid_out_id, write_vtk_id, 0, 0); // grid_out
  network.add_connection(ref_id, write_vtk_id, 0, 1);      // tria
  network.add_connection(out_file_id, write_vtk_id, 0, 2); // out_file

  network.output_dot(output_dir.path() / "step-1_taskflow.dot");
  {
    std::ofstream ofile(output_dir.path() / "step-1_network.json");
    ofile << network.to_json().dump(2) << std::endl;
    ofile.close();
  }

  {
    std::ofstream ofile(output_dir.path() / "step-1_registry.json");
    ofile << NodeObject::get_registry().dump(2) << std::endl;
    ofile.close();
  }

  // Execute the network
  network.run();

  // // Verify results
  // ASSERT_EQ(1, tria->get<Triangulation<2>>().n_active_cells());
  ASSERT_EQ(16, tria->get<Triangulation<2>>().n_active_cells());

  // Check if the file exists
  std::ifstream file(output_dir.path() / "grid-1.vtk");
  ASSERT_TRUE(file.good());

  file.close();
}

TEST(dealiiExamples, NetworkFromJsonStep00)
{
  ScopedTestOutputDir output_dir("dealiiExamples_NetworkFromJsonStep00");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  Network network;
  network.set_touch_file_base_path(output_dir);
  network.clear_network();

  std::ifstream file(SOURCE_DIR "/test_files/step-0_network.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;
  file.close();

  ASSERT_FALSE(json_data.empty()) << "JSON data is empty.";
  network.from_json(json_data);

  // Print some debugging information
  slog_debug("Network has %u nodes and %u connections",
             network.n_nodes(),
             network.n_connections());

  // Check for node types
  for (unsigned int i = 0; i < network.n_nodes(); ++i)
    {
      auto node = network.get_node(i);
      slog_debug("Node %u: %s (%s)",
                 i,
                 network.get_node_name(i).c_str(),
                 (node ? "exists" : "nullptr"));

      if (node->node_type() == NodeType::elementary_constructor)
        {
          slog_debug(" value: %s", node->to_string().c_str());
        }
      else
        {
          slog_debug(" type: %s", node->type_name().c_str());
        }
    }

  // Run the network
  network.run();
  auto tria_node = network.get_node(0);

  // Print the triangulation state after running the network
  slog_debug("After execution, triangulation has %u active cells",
             tria_node->get<Triangulation<2>>().n_active_cells());

  // Verify results
  auto &tria = tria_node->get<Triangulation<2>>();
  ASSERT_EQ(16, tria.n_active_cells());
}

TEST(dealiiExamples, PoissonSolver)
{
  ScopedTestOutputDir output_dir("dealiiExamples_PoissonSolver");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  auto tria = make_node<Triangulation<2>>();
  (*tria)();
  auto &tria_ref = tria->get<Triangulation<2>>();
  GridGenerator::hyper_cube(tria_ref, 0, 1, true);
  tria_ref.refine_global(1);

  auto fe_degree = make_node(1u);
  auto fe        = make_node<FE_Q<2>>();
  fe->set_arguments({fe_degree});
  (*fe)();

  auto output_name = make_node((output_dir.path() / "solution.vtu").string());
  auto rhs_expr    = make_node(std::string("1"));
  auto dirichlet_ids =
    make_node(std::set<types::boundary_id>{types::boundary_id{0}});
  auto dirichlet_expr = make_node(std::string("0"));
  auto neumann_ids    = make_node(std::set<types::boundary_id>{});
  auto neumann_expr   = make_node(std::string("0"));

  auto poisson = make_node<PoissonSolver<2, 2>>();
  poisson->set_arguments({tria,
                          fe,
                          output_name,
                          rhs_expr,
                          dirichlet_ids,
                          dirichlet_expr,
                          neumann_ids,
                          neumann_expr});

  ASSERT_TRUE((*poisson)());
  ASSERT_TRUE(poisson->ready());

  // Create a method node for solve() and invoke it using coral machinery
  auto solve_method = make_node("PoissonSolver::solve<2>");
  solve_method->set_arguments({poisson});
  (*solve_method)();

  // check that the output file was created
  std::ifstream file(output_dir.path() / "solution.vtu");
  ASSERT_TRUE(file.good());
  file.close();
}

TEST(dealiiExamples, ReadGridVtu)
{
  Triangulation<2> triangulation;

  read_grid<2>((std::filesystem::path(SOURCE_DIR) / "test_files" /
                "test_grid.vtu")
                 .string(),
               triangulation);

  EXPECT_EQ(triangulation.n_active_cells(), 16);
  EXPECT_EQ(triangulation.n_used_vertices(), 25);

  std::set<types::material_id> material_ids;
  std::set<types::boundary_id> boundary_ids;
  for (const auto &cell : triangulation.active_cell_iterators())
    {
      material_ids.insert(cell->material_id());
      for (const auto &f : cell->face_iterators())
        if (f->at_boundary())
          boundary_ids.insert(f->boundary_id());
    }


  EXPECT_EQ(material_ids.size(), 1u);
  EXPECT_TRUE(material_ids.find(types::material_id{0}) != material_ids.end());
  EXPECT_EQ(boundary_ids.size(), 2u);
  EXPECT_TRUE(boundary_ids.find(types::boundary_id{0}) != boundary_ids.end());
  EXPECT_TRUE(boundary_ids.find(types::boundary_id{1}) != boundary_ids.end());
}

TEST(dealiiExamples, NetworkFromJsonPoissonSolverSolution)
{
  ScopedTestOutputDir output_dir(
    "dealiiExamples_NetworkFromJsonPoissonSolverSolution");

  register_non_dimensional_types();
  register_dimensional_types<2, 2>();

  Network network;
  network.set_touch_file_base_path(output_dir);
  network.clear_network();

  std::ifstream file(SOURCE_DIR "/test_files/PoissonSolverSolution.json");
  ASSERT_TRUE(file.is_open())
    << "Failed to open PoissonSolverSolution.json file.";

  nlohmann::json json_data;
  file >> json_data;
  file.close();

  ASSERT_FALSE(json_data.empty()) << "JSON data is empty.";

  // Update the output file path to use the test output directory
  // Node 9 contains the output filename
  if (json_data["workflow"]["nodes"].contains("9") &&
      json_data["workflow"]["nodes"]["9"].contains("value"))
    {
      std::string output_filename =
        json_data["workflow"]["nodes"]["9"]["value"];
      json_data["workflow"]["nodes"]["9"]["value"] =
        (output_dir.path() / output_filename).string();
    }

  network.from_json(json_data);

  // Print some debugging information
  slog_debug("Network has %u nodes and %u connections",
             network.n_nodes(),
             network.n_connections());

  // Run the network
  network.run();

  // Check that the output file was created (assuming the JSON specifies
  // solution.vtu)
  std::ifstream solution_file(output_dir.path() / "solution.vtu");
  ASSERT_TRUE(solution_file.good()) << "Solution VTU file was not created.";
  solution_file.close();
}
