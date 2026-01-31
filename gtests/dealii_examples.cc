#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <locale>

#include "coral.h"
#include "coral_network.h" // Added include for Network class
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using namespace coral;
using coral_test::ScopedTestOutputDir;

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

  std::string vtk_path = output_dir.path() / "grid-1.vtk";
  auto tria           = make_node<Triangulation<2>>();
  auto grid_name      = make_node("hyper_cube");
  auto grid_arguments = make_node("0: 1: false");
  auto ref            = make_node("Triangulation<2>::refine_global");
  auto n_ref          = make_node(2u);
  auto filename       = make_node(vtk_path);
  auto out_file       = make_node<std::ofstream>();
  auto grid_out       = make_node<GridOut>();
  auto write_vtk      = make_node("GridOut::write_vtk<2>");

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

  std::string vtk_path = output_dir.path() / "grid-1.vtk";
  auto tria           = make_node<Triangulation<2>>();
  auto grid_name      = make_node("hyper_cube");
  auto grid_arguments = make_node("0: 1: false");
  auto ref            = make_node("Triangulation<2>::refine_global");
  auto n_ref          = make_node(2u);
  auto filename       = make_node(vtk_path);
  auto out_file       = make_node<std::ofstream>();
  auto grid_out       = make_node<GridOut>();
  auto write_vtk      = make_node("GridOut::write_vtk<2>");

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
