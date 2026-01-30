#include <gtest/gtest.h>

#include <fstream>
#include <filesystem>
#include <vector>
#include <string>

#include "coral_network.h"
#include "register_types.h"
#include "test_utils.h"

namespace
{
  void
  consume_unsigned(unsigned int)
  {}

  coral::Network
  build_hypercube_network()
  {
    coral::Network network;

    auto triangulation = coral::make_node<dealii::Triangulation<2>>();
    auto grid_generator =
      coral::make_node("GridGenerator::generate_from_name_and_arguments<2>");
    auto refine_global = coral::make_node("Triangulation<2>::refine_global");

    auto tri_id  = network.add_node(triangulation);
    auto grid_id = network.add_node(grid_generator);
    auto ref_id  = network.add_node(refine_global);

    auto name_node = coral::make_node(std::string("hyper_cube"));
    auto args_node = coral::make_node(std::string("0: 1: false"));
    auto name_id   = network.add_node(name_node);
    auto args_id   = network.add_node(args_node);

    // GridGenerator inputs: triangulation, name, args.
    network.add_connection(tri_id, grid_id, 0, 0);
    network.add_connection(name_id, grid_id, 0, 1);
    network.add_connection(args_id, grid_id, 0, 2);

    // refine_global inputs: triangulation, n_refinements.
    network.add_connection(grid_id, ref_id, 0, 0);

    return network;
  }

  void
  verify_status_files(const coral::Network &network,
                      const std::filesystem::path &touch_dir)
  {
    // Get all node names from the network
    const auto &nodes_name = network.get_nodes_name();

    // Verify status files for each node
    for (const auto &[node_id, name] : nodes_name)
      {
        // Use node name if available, otherwise skip empty names
        // (empty names would create files like ".running" which is problematic)
        if (name.empty())
          continue;

        std::filesystem::path running_file   = touch_dir / (std::to_string(node_id) + ".running");
        std::filesystem::path succeeded_file = touch_dir / (std::to_string(node_id) + ".succeeded");
        std::filesystem::path failed_file    = touch_dir / (std::to_string(node_id) + ".failed");

        // Check that .running file exists
        EXPECT_TRUE(std::filesystem::exists(running_file))
          << "Missing .running file for node " << node_id << " (name: '" << name
          << "')";

        // Check that .succeeded file exists
        EXPECT_TRUE(std::filesystem::exists(succeeded_file))
          << "Missing .succeeded file for node " << node_id << " (name: '"
          << name << "')";

        // Check that .failed file does NOT exist
        EXPECT_FALSE(std::filesystem::exists(failed_file))
          << "Unexpected .failed file for node " << node_id << " (name: '"
          << name << "')";
      }
  }
} // namespace

using coral_test::ScopedTestOutputDir;

TEST(Modules, NetworkFileParse)
{
  const std::string path = SOURCE_DIR "/test_files/mwe.json";

  coral::register_all_types();

  auto node = coral::make_node<coral::Network>();
  node->parse_string(path);

  ASSERT_TRUE(node->ready());
  EXPECT_GT(node->get<coral::Network>().n_nodes(), 0u);
}

TEST(Modules, SingleNodeNetwork)
{
  coral::register_all_types();

  coral::Network network;
  network.add_node(coral::make_node(1u));

  EXPECT_TRUE(network.get_inputs().empty());
  EXPECT_EQ(network.get_outputs().size(), 1u);
}

TEST(Modules, CompleteNetworkNoIO)
{
  coral::register_all_types();

  coral::NodeObject::register_function(consume_unsigned, {"consume", "value"});

  coral::Network network;
  auto           source_id = network.add_node(coral::make_node(1u));
  auto           sink_id =
    network.add_node(coral::make_method_node("consume", consume_unsigned));

  network.add_connection(source_id, sink_id, 0, 0);

  EXPECT_TRUE(network.get_inputs().empty());
  EXPECT_TRUE(network.get_outputs().empty());
}

TEST(Modules, HyperCubeNetworkInterface)
{
  ScopedTestOutputDir output_dir("Modules_HyperCubeNetworkInterface");

  coral::register_all_types();

  auto network = build_hypercube_network();

  const auto inputs  = network.get_inputs();
  const auto outputs = network.get_outputs();

  ASSERT_EQ(inputs.size(), 1u);
  ASSERT_EQ(outputs.size(), 1u);

  // Dump the network to the file hyper_cube_network.json
  nlohmann::json network_json = network;
  std::ofstream  ofs(output_dir.path() / "hyper_cube_network.json");
  ofs << network_json.dump(2);
  ofs.close();

  // Now dump the node to the file hyper_cube_node.json
  auto           node      = coral::make_node(network);
  nlohmann::json node_json = node;

  std::ofstream ofs_node(output_dir.path() / "hyper_cube_node.json");
  ofs_node << node_json.dump(2);
  ofs_node.close();
}

TEST(Modules, HyperCubeNetworkRoundTrip)
{
  coral::register_all_types();

  auto network      = build_hypercube_network();
  auto network_node = coral::make_node(network);

  // Check equality of the number of inputs and outputs
  ASSERT_EQ(network.get_inputs().size(), 1u);
  ASSERT_EQ(network.get_outputs().size(), 1u);
  ASSERT_EQ(network_node->n_inputs(), 1u);
  ASSERT_EQ(network_node->n_outputs(), 1u);

  // Dump the network_node to json, rebuild, and compare network json.
  json node_json     = network_node;
  auto network_node2 = node_json.get<coral::NodeObjectPtr>();

  json node_json2 = network_node2;
  EXPECT_EQ(node_json, node_json2);

  auto &network2      = network_node2->get<coral::Network>();
  json  network_json1 = network.to_json();
  json  network_json2 = network2.to_json();
  network_json1.erase("date_time_utc");
  network_json2.erase("date_time_utc");
  EXPECT_EQ(network_json1, network_json2);
}


TEST(Modules, HyperCubeNetworkConnectedRun)
{
  ScopedTestOutputDir output_dir("Modules_HyperCubeNetworkConnectedRun");

  coral::register_all_types();

  auto network_node = coral::make_node(build_hypercube_network());
  auto refinements  = coral::make_node(4u);

  // json refinements_json = refinements;
  // std::cout << "Refinements node JSON: " << refinements_json.dump(2)
  //           << std::endl;

  ASSERT_EQ(refinements->get<unsigned int>(), 4u);

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  auto           net_id = network.add_node(network_node);
  ASSERT_EQ(refinements->get<unsigned int>(), 4u);
  auto ref_id = network.add_node(refinements);
  ASSERT_EQ(refinements->get<unsigned int>(), 4u);
  network.add_connection(ref_id, net_id, 0, 0);
  ASSERT_EQ(refinements->get<unsigned int>(), 4u);
  network.run();
  ASSERT_EQ(refinements->get<unsigned int>(), 4u);
  auto tri_node = network.get_output(0);

  ASSERT_EQ(refinements->get<unsigned int>(), 4u);
  ASSERT_EQ(network_node->get_input(0), refinements);
  ASSERT_EQ(network_node->get_input(0)->get<unsigned int>(), 4u);

  ASSERT_EQ(tri_node->get<dealii::Triangulation<2>>().n_active_cells(), 256);
}

TEST(Modules, NetworkNodeArgumentsOrder1)
{
  ScopedTestOutputDir output_dir("Modules_NetworkNodeArgumentsOrder1");

  const std::string path = SOURCE_DIR "/test_files/networknode-order1.json";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON";

  ASSERT_NO_THROW(network.run()) << "Failed to run network";

  verify_status_files(network, output_dir);
}

TEST(Modules, NetworkNodeArgumentsOrder2)
{
  ScopedTestOutputDir output_dir("Modules_NetworkNodeArgumentsOrder2");

  const std::string path = SOURCE_DIR "/test_files/networknode-order2.json";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON with different argument order";

  ASSERT_NO_THROW(network.run())
    << "Failed to run network with different argument order";

  verify_status_files(network, output_dir);
}

TEST(Modules, NetworkNodeNoArguments)
{
  ScopedTestOutputDir output_dir("Modules_NetworkNodeNoArguments");

  const std::string path =
    SOURCE_DIR "/test_files/networknode-noarguments.json";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON with different argument order";

  ASSERT_NO_THROW(network.run())
    << "Failed to run network with different argument order";

  verify_status_files(network, output_dir);
}

TEST(Modules, VtkGen1)
{
  ScopedTestOutputDir output_dir("Modules_VtkGen1");

  const std::string path        = SOURCE_DIR "/test_files/vtk-gen1.json";
  const std::string output_file = "grid-1.vtk";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON";

  ASSERT_NO_THROW(network.run()) << "Failed to run network";

  // Check that the output file was created and is not empty
  std::ifstream output{output_file};
  ASSERT_TRUE(output.good())
    << "Output file " << output_file << " was not created";

  output.seekg(0, std::ios::end);
  std::streampos file_size = output.tellg();
  ASSERT_GT(file_size, 0) << "Output file " << output_file << " is empty";
  output.close();

  // Verify status files
  verify_status_files(network, output_dir);

  // Remove the output file
  std::remove(output_file.c_str());
}

TEST(Modules, VtkGen2)
{
  ScopedTestOutputDir output_dir("Modules_VtkGen2");

  const std::string path        = SOURCE_DIR "/test_files/vtk-gen2.json";
  const std::string output_file = "grid-1.vtk";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON";

  ASSERT_NO_THROW(network.run()) << "Failed to run network";

  // Check that the output file was created and is not empty
  std::ifstream output{output_file};
  ASSERT_TRUE(output.good())
    << "Output file " << output_file << " was not created";

  output.seekg(0, std::ios::end);
  std::streampos file_size = output.tellg();
  ASSERT_GT(file_size, 0) << "Output file " << output_file << " is empty";
  output.close();

  // Verify status files
  verify_status_files(network, output_dir);

  // Remove the output file
  std::remove(output_file.c_str());
}

TEST(Modules, VtkGen3)
{
  ScopedTestOutputDir output_dir("Modules_VtkGen3");

  const std::string path        = SOURCE_DIR "/test_files/vtk-gen3.json";
  const std::string output_file = "grid-1.vtk";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON";

  ASSERT_NO_THROW(network.run()) << "Failed to run network";

  // Check that the output file was created and is not empty
  std::ifstream output{output_file};
  ASSERT_TRUE(output.good())
    << "Output file " << output_file << " was not created";

  output.seekg(0, std::ios::end);
  std::streampos file_size = output.tellg();
  ASSERT_GT(file_size, 0) << "Output file " << output_file << " is empty";
  output.close();

  // Verify status files
  verify_status_files(network, output_dir);

  // Remove the output file
  std::remove(output_file.c_str());
}

TEST(Modules, VtkSingle)
{
  ScopedTestOutputDir output_dir("Modules_VtkSingle");

  const std::string path        = SOURCE_DIR "/test_files/vtk-single.json";
  const std::string output_file = "grid-1.vtk";

  coral::register_all_types();

  std::ifstream input{path};
  ASSERT_TRUE(input.good()) << "Failed to open " << path;

  nlohmann::json data;
  input >> data;

  coral::Network network;
  network.set_touch_file_base_path(output_dir);
  ASSERT_NO_THROW(network.from_json(data))
    << "Failed to parse network from JSON";

  ASSERT_NO_THROW(network.run()) << "Failed to run network";

  // Check that the output file was created and is not empty
  std::ifstream output{output_file};
  ASSERT_TRUE(output.good())
    << "Output file " << output_file << " was not created";

  output.seekg(0, std::ios::end);
  std::streampos file_size = output.tellg();
  ASSERT_GT(file_size, 0) << "Output file " << output_file << " is empty";
  output.close();

  // Verify status files
  verify_status_files(network, output_dir);

  // Remove the output file
  std::remove(output_file.c_str());
}
