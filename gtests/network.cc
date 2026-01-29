
#include <fstream>

#include "coral.h"
#include "coral_network.h"
#include "gtest/gtest.h"
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
using namespace coral;
using json = nlohmann::json;
using coral_test::ScopedTestOutputDir;


// Failing test from network.cc
TEST(Network, BareMinimal)
{
  ScopedTestOutputDir output_dir("Network_BareMinimal");

  coral::NodeObject::register_elementary_type<double>();

  auto sum = [](const double &a, const double &b) {
    slog_debug("Computing sum of %g and %g", a, b);
    return a + b;
  };
  coral::NodeObject::register_function(sum,
                                       {"sum", "output", "input1", "input2"});

  // Output the registry for these node types
  auto          registry = coral::NodeObject::get_registry();
  std::ofstream registry_file("bare_minimal_registry.json");
  registry_file << std::setw(2) << registry << std::endl;
  registry_file.close();

  // Now create a network and add nodes and connections
  coral::Network network;
  network.set_touch_file_base_path(output_dir);

  auto id1 = network.add_node(coral::make_node(1.0));
  auto id2 = network.add_node(coral::make_node(2.0));

  auto id4 = network.add_node(coral::make_method_node("sum", sum), "sum");

  // Int 1
  network.add_connection(id1, id4, 0, 0);

  // Int 2
  network.add_connection(id2, id4, 0, 1);

  // Verify all the connections
  ASSERT_EQ(network.n_connections(), 2);
  ASSERT_EQ(network.n_nodes(), 3);

  const auto n1 = network.get_node(id1);
  const auto n2 = network.get_node(id2);
  const auto n4 = network.get_node(id4);

  ASSERT_EQ(n1->get<double>(), 1.0);
  ASSERT_EQ(n2->get<double>(), 2.0);

  json n1_json = n1;
  json n2_json = n2;
  json n3_json = n4->get_output(0);

  // Verify the JSON "value" of the nodes
  ASSERT_EQ(n1_json["value"], "1.0");
  ASSERT_EQ(n2_json["value"], "2.0");
  ASSERT_EQ(n3_json["value"], "0.0");

  // Make sure that executing the nodes does not change their values
  (*n1)();
  (*n2)();

  // Verify the values after execution
  ASSERT_EQ(n1->get<double>(), 1.0);
  ASSERT_EQ(n2->get<double>(), 2.0);

  // Make sure the self outputs are ok
  ASSERT_EQ(n1->get_output(0), n1);
  ASSERT_EQ(n2->get_output(0), n2);

  // Make sure that asking for the value from the output node gives the
  // expected result
  ASSERT_EQ(n1->get_output(0)->get<double>(), 1.0);
  ASSERT_EQ(n2->get_output(0)->get<double>(), 2.0);

  // Check the connections of the sum node
  ASSERT_EQ(n4->get_input(0), n1->get_output(0));
  ASSERT_EQ(n4->get_input(1), n2->get_output(0));

  // Verify the output node is not a pass-through input
  ASSERT_NE(n4->get_output(0), n4->get_input(0));

  network.output_dot("bare_minimal.dot");
  // dump the json of the network
  json          serialized_json = network;
  std::ofstream json_file("bare_minimal.json");
  json_file << serialized_json.dump(2);
  json_file.close();

  slog_debug("Executing network with 3 nodes and 2 connections.");
  // Run the network
  network.run();
  slog_debug("Network executed.");

  ASSERT_EQ(n4->get_output(0)->get<double>(), 3.0)
    << "The output node should have the value 3.0";
}

TEST(Network, ExplicitNodeNaming)
{
  coral::NodeObject::register_elementary_type<double>();
  auto sum = [](const double &a, const double &b) { return a + b; };
  coral::NodeObject::register_function(sum,
                                       {"sum", "output", "input1", "input2"});

  coral::Network network;
  auto           id1 = network.add_node(coral::make_node(1.0));
  auto           id2 = network.add_node(coral::make_node(2.0));
  auto           id3 = network.add_node(coral::make_node(0.0));
  auto           id4 = network.add_node(coral::make_method_node("sum", sum));

  network.set_node_name(id3, "output");
  network.set_node_name(id4, "adder");

  network.set_node_name(id1, "one");
  network.set_node_name(id2, "two");

  ASSERT_EQ(network.get_node_name(id1), "one");
  ASSERT_EQ(network.get_node_name(id2), "two");
  ASSERT_EQ(network.get_node_name(id3), "output");
  ASSERT_EQ(network.get_node_name(id4), "adder");
}

TEST(Network, AutoNameOnConnection)
{
  coral::NodeObject::register_elementary_type<double>();
  auto pass = [](const double &a) { return a; };
  coral::NodeObject::register_function(pass, {"pass", "out", "in"});

  coral::Network network;
  auto           src_id = network.add_node(coral::make_node(0.0)); // unnamed
  auto target_id = network.add_node(coral::make_method_node("pass", pass));

  // Connect unnamed source self-output (-1) to target input 0 (argument "in").
  network.add_connection(src_id, target_id, 0, 0);

  ASSERT_EQ(network.get_node_name(src_id), "in");
}

TEST(Network, ParseAndDump)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Dump the network to JSON
  nlohmann::json output_json = network.to_json();

  // Check if the output JSON has the expected structure
  ASSERT_TRUE(output_json.contains("workflow"))
    << "Output JSON must contain 'workflow' object";
  ASSERT_TRUE(output_json["workflow"].contains("nodes"))
    << "Output JSON must contain 'workflow.nodes' object";
  ASSERT_TRUE(output_json["workflow"].contains("edges"))
    << "Output JSON must contain 'workflow.edges' object";

  // Check the number of nodes and edges
  ASSERT_EQ(output_json["workflow"]["nodes"].size(), 6)
    << "Should have 6 nodes in the workflow";
  ASSERT_EQ(output_json["workflow"]["edges"].size(), 5)
    << "Should have 5 edges in the workflow";
}

// ParseAndExecuteNetwork test moved to failing.cc

// JsonBasedWorkflow test moved to failing.cc

// ValidateEdgeConnections test moved to failing.cc

// VerifyNodeTypes test moved to failing.cc

// ConnectionsMapTracking test moved to failing.cc

TEST(Network, ConnectionSerialization)
{ // Create a connection
  coral::Connection conn(1, 3, 2, 4);

  // Serialize to JSON
  auto json = conn.to_json();

  // Verify JSON contents has all required fields
  EXPECT_EQ(json["source"], 1);
  EXPECT_EQ(json["target"], 3);
  EXPECT_EQ(json["source_output"], 2);
  EXPECT_EQ(json["target_input"], 4);

  // Deserialize back to a Connection
  auto deserialized = coral::Connection::from_json(json);

  // Verify deserialized values
  EXPECT_EQ(deserialized.source_id, 1);
  EXPECT_EQ(deserialized.target_id, 3);
  EXPECT_EQ(deserialized.source_output, 2);
  EXPECT_EQ(deserialized.target_input, 4);
}

// Test for JSON-based workflow
TEST(Network, JsonBasedWorkflow)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Basic verification of nodes - should have 6 nodes
  ASSERT_EQ(network.size(), 6);

  // Verify each node exists and has the right type
  for (int i = 0; i < 6; ++i)
    {
      auto node = network.get_node(i);
      ASSERT_TRUE(node != nullptr) << "Node " << i << " should exist";
    }
}

// Test for validating edge connections
TEST(Network, ValidateEdgeConnections)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Validate the network size
  ASSERT_EQ(network.size(), 6);

  // Output the taskflow as DOT format for debugging
  network.output_dot("validate_edges_taskflow.dot");

  // Get the taskflow
  auto &tf = network.get_taskflow();

  // According to mwe.json, there should be 5 edges:
  // 0. Node 0 -> Node 3 (triangulation to generate_from_name_and_arguments)
  // 1. Node 1 -> Node 3 (string "hyper_cube" to
  // generate_from_name_and_arguments)
  // 2. Node 2 -> Node 3 (string arguments to generate_from_name_and_arguments)
  // 3. Node 3 -> Node 5 (triangulation after generate to refine_global)
  // 4. Node 4 -> Node 5 (unsigned value to refine_global)
  ASSERT_EQ(tf.num_tasks(), 6);

  // Verify the connections using our tracking map
  ASSERT_EQ(network.n_connections(), 5);
  EXPECT_TRUE(network.is_connected(0, 3));
  EXPECT_TRUE(network.is_connected(1, 3));
  EXPECT_TRUE(network.is_connected(2, 3));
  EXPECT_TRUE(network.is_connected(3, 5));
  EXPECT_TRUE(network.is_connected(4, 5));
}

// Test for verifying node types
TEST(Network, VerifyNodeTypes)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Verify the nodes exist
  ASSERT_EQ(network.size(), 6);

  // Check node 0: Should be of type "dealii::Triangulation<2, 2>" with
  // nodeType "empty_constructor"
  auto node0 = network.get_node(0);
  ASSERT_TRUE(node0 != nullptr);
  EXPECT_EQ(node0->node_type(), coral::NodeType::empty_constructor);
  EXPECT_EQ(node0->type_name(), "dealii::Triangulation<2, 2>");

  // Check node 1: Should be of type "std::string" with nodeType
  // "elementary_constructor" and value "hyper_cube"
  auto node1 = network.get_node(1);
  ASSERT_TRUE(node1 != nullptr);
  EXPECT_EQ(node1->node_type(), coral::NodeType::elementary_constructor);
  EXPECT_EQ(node1->type_name(), "std::string");

  // Check node 2: Should be of type "std::string" with nodeType
  // "elementary_constructor" and value "0: 1: false"
  auto node2 = network.get_node(2);
  ASSERT_TRUE(node2 != nullptr);
  EXPECT_EQ(node2->node_type(), coral::NodeType::elementary_constructor);
  EXPECT_EQ(node2->type_name(), "std::string");

  // Check node 3: Should be related to
  // GridGenerator::generate_from_name_and_arguments
  auto node3 = network.get_node(3);
  ASSERT_TRUE(node3 != nullptr);
  EXPECT_EQ(node3->node_type(), coral::NodeType::void_function);
  EXPECT_NE(node3->type_name().find("std::function"), std::string::npos);

  // Check node 4: Should be of type "unsigned" with nodeType
  // "elementary_constructor" and value "2"
  auto node4 = network.get_node(4);
  ASSERT_TRUE(node4 != nullptr);
  EXPECT_EQ(node4->node_type(), coral::NodeType::elementary_constructor);
  EXPECT_EQ(node4->type_name(), "unsigned");

  // Check node 5: Should be related to Triangulation<2>::refine_global
  auto node5 = network.get_node(5);
  ASSERT_TRUE(node5 != nullptr);
  EXPECT_EQ(node5->node_type(), coral::NodeType::void_method);
  EXPECT_NE(node5->type_name().find("Triangulation"), std::string::npos);
}

// Test for connections map tracking
TEST(Network, ConnectionsMapTracking)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Verify total connection count
  ASSERT_EQ(network.n_connections(), 5) << "Should have 9 connections";

  // Verify specific connections
  EXPECT_TRUE(network.is_connected(0, 3)) << "Node 0 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(1, 3)) << "Node 1 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(2, 3)) << "Node 2 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(3, 5)) << "Node 3 should connect to Node 5";
  EXPECT_TRUE(network.is_connected(4, 5)) << "Node 4 should connect to Node 5";

  // Verify connection vectors using the target IDs list
  auto node0_targets = network.get_connected_nodes(0);
  EXPECT_EQ(node0_targets.size(), 1)
    << "Node 0 should have 1 outgoing connection";
  EXPECT_EQ(node0_targets[0], 3) << "Node 0 should connect to Node 3";

  auto node1_targets = network.get_connected_nodes(1);
  EXPECT_EQ(node1_targets.size(), 1)
    << "Node 1 should have 1 outgoing connection";
  EXPECT_EQ(node1_targets[0], 3) << "Node 1 should connect to Node 3";

  auto node2_targets = network.get_connected_nodes(2);
  EXPECT_EQ(node2_targets.size(), 1)
    << "Node 2 should have 1 outgoing connection";
  EXPECT_EQ(node2_targets[0], 3) << "Node 2 should connect to Node 3";

  auto node3_targets = network.get_connected_nodes(3);
  EXPECT_EQ(node3_targets.size(), 1)
    << "Node 3 should have 1 outgoing connection";
  EXPECT_EQ(node3_targets[0], 5) << "Node 3 should connect to Node 5";

  auto node4_targets = network.get_connected_nodes(4);
  EXPECT_EQ(node4_targets.size(), 1)
    << "Node 4 should have 1 outgoing connection";
  EXPECT_EQ(node4_targets[0], 5) << "Node 4 should connect to Node 5";

  auto node5_targets = network.get_connected_nodes(5);
  EXPECT_EQ(node5_targets.size(), 0)
    << "Node 5 should have 0 outgoing connection";

  // Verify connection objects
  auto node0_conns = network.get_node_connections(0);
  EXPECT_EQ(node0_conns.size(), 1) << "Node 0 should have 1 connection object";
  EXPECT_EQ(node0_conns[0].source_id, 0);
  EXPECT_EQ(node0_conns[0].target_id, 3);

  auto node1_conns = network.get_node_connections(1);
  EXPECT_EQ(node1_conns.size(), 1) << "Node 1 should have 1 connection object";
  EXPECT_EQ(node1_conns[0].source_id, 1);
  EXPECT_EQ(node1_conns[0].target_id, 3);

  auto node2_conns = network.get_node_connections(2);
  EXPECT_EQ(node2_conns.size(), 1) << "Node 2 should have 1 connection object";
  EXPECT_EQ(node2_conns[0].source_id, 2);
  EXPECT_EQ(node2_conns[0].target_id, 3);

  auto node3_conns = network.get_node_connections(3);
  EXPECT_EQ(node3_conns.size(), 1) << "Node 3 should have 1 connection object";
  EXPECT_EQ(node3_conns[0].source_id, 3);
  EXPECT_EQ(node3_conns[0].target_id, 5);

  auto node4_conns = network.get_node_connections(4);
  EXPECT_EQ(node4_conns.size(), 1) << "Node 4 should have 1 connection object";
  EXPECT_EQ(node4_conns[0].source_id, 4);
  EXPECT_EQ(node4_conns[0].target_id, 5);
}

// Test for network serialization
TEST(Network, NetworkSerialization)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Serialize the network back to JSON
  auto serialized_json = network.to_json();

  // Verify the serialized JSON has the correct structure
  ASSERT_TRUE(serialized_json.contains("workflow"))
    << "Must contain workflow object";
  ASSERT_TRUE(serialized_json["workflow"].contains("nodes"))
    << "Must contain nodes object";
  ASSERT_TRUE(serialized_json["workflow"].contains("edges"))
    << "Must contain edges object";
  ASSERT_TRUE(serialized_json.contains("version")) << "Must contain version";
  ASSERT_TRUE(serialized_json.contains("date_time_utc"))
    << "Must contain date_time_utc";

  // Verify node count
  ASSERT_EQ(serialized_json["workflow"]["nodes"].size(), 6)
    << "Should have 6 nodes";

  // Verify node structure
  for (const auto &[id, node] : serialized_json["workflow"]["nodes"].items())
    {
      ASSERT_TRUE(node.contains("type")) << "Node must have type field";
      EXPECT_FALSE(node.contains("node_type"))
        << "Node should not include node_type in serialized form";
      EXPECT_FALSE(node.contains("outputs"))
        << "Node should not include outputs in serialized form";
      EXPECT_FALSE(node.contains("arguments"))
        << "Node should not include arguments in serialized form";
      EXPECT_FALSE(node.contains("inputs"))
        << "Node should not include inputs in serialized form";

      const auto node_ptr = network.get_node(std::stoi(id));
      ASSERT_NE(node_ptr, nullptr);
      if (node_ptr->node_type() == coral::NodeType::elementary_constructor)
        {
          EXPECT_TRUE(node.contains("value"))
            << "Elementary nodes should carry a value";
        }
      else
        {
          EXPECT_FALSE(node.contains("value"))
            << "Non-elementary nodes should not carry a value";
        }
    }

  // Verify edge structure
  for (const auto &[id, edge] : serialized_json["workflow"]["edges"].items())
    {
      ASSERT_TRUE(edge.contains("source")) << "Edge must have source field";
      ASSERT_TRUE(edge.contains("target")) << "Edge must have target field";
      ASSERT_TRUE(edge.contains("source_output"))
        << "Edge must have source_output field";
      ASSERT_TRUE(edge.contains("target_input"))
        << "Edge must have target_input field";
    }

  // Create a new network from the serialized JSON
  coral::Network new_network;
  new_network.from_json(serialized_json);

  // Verify the new network has the same nodes and connections
  EXPECT_EQ(new_network.size(), 6);
  EXPECT_EQ(new_network.n_connections(), 5);

  // Verify the specific connections
  EXPECT_TRUE(new_network.is_connected(0, 3));
  EXPECT_TRUE(new_network.is_connected(1, 3));
  EXPECT_TRUE(new_network.is_connected(2, 3));
  EXPECT_TRUE(new_network.is_connected(3, 5));
  EXPECT_TRUE(new_network.is_connected(4, 5));
}

TEST(Network, RegistrySubset)
{
  coral::NodeObject::register_elementary_type<int>();
  coral::NodeObject::register_elementary_type<double>();

  coral::Network network;
  network.add_node(coral::make_node(1));
  network.add_node(coral::make_node(2.5));

  auto registry = network.get_registry();
  ASSERT_EQ(registry.size(), 2);
  EXPECT_TRUE(registry.contains(coral::detail::hash<int>()));
  EXPECT_TRUE(registry.contains(coral::detail::hash<double>()));
  EXPECT_EQ(registry[coral::detail::hash<int>()]["node_type"],
            "elementary_constructor");
  EXPECT_EQ(registry[coral::detail::hash<double>()]["node_type"],
            "elementary_constructor");
}

TEST(Network, ParseAndExecuteNetwork)
{
  ScopedTestOutputDir output_dir("Network_ParseAndExecuteNetwork");

  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network = json_data;
  network.set_touch_file_base_path(output_dir);

  // Output some debug information
  slog_debug("Network has %zu nodes and %u connections",
             network.size(),
             network.n_connections());

  // Execute the network
  network.run();

  // Verify results
  ASSERT_EQ(16, network.get_node(0)->get<Triangulation<2>>().n_active_cells());
}
