
#include <fstream>

#include "coral.h"
#include "coral_network.h"
#include "gtest/gtest.h"
#include "register_types.h"

using namespace dealii;
using namespace coral;
using json = nlohmann::json;


// Failing test from network.cc
TEST(NetworkTest, BareMinimal)
{
  coral::NodeObject::register_elementary_type<double>();

  auto sum = [](const double &a, const double &b) {
    std::cout << "Computing sum of " << a << " and " << b << std::endl;
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

  auto id1 = network.add_node(coral::make_node(1.0));
  auto id2 = network.add_node(coral::make_node(2.0));
  auto id3 = network.add_node(coral::make_node(0.0));
  auto id4 = network.add_node(coral::make_method_node("sum", sum));

  // Output integer
  network.add_connection(id3, id4, 0, 0);

  // Int 1
  network.add_connection(id1, id4, 0, 1);

  // Int 2
  network.add_connection(id2, id4, 0, 2);

  // Verify all the connections
  ASSERT_EQ(network.n_connections(), 3);
  ASSERT_EQ(network.n_nodes(), 4);

  const auto n1 = network.get_node(id1);
  const auto n2 = network.get_node(id2);
  const auto n3 = network.get_node(id3);
  const auto n4 = network.get_node(id4);

  ASSERT_EQ(n1->get<double>(), 1.0);
  ASSERT_EQ(n2->get<double>(), 2.0);
  ASSERT_EQ(n3->get<double>(), 0.0);

  json n1_json = n1;
  json n2_json = n2;
  json n3_json = n3;

  // Verify the JSON "value" of the nodes
  ASSERT_EQ(n1_json["value"], "1.0");
  ASSERT_EQ(n2_json["value"], "2.0");
  ASSERT_EQ(n3_json["value"], "0.0");

  // Make sure that executing the nodes does not change their values
  (*n1)();
  (*n2)();
  (*n3)();

  // Verify the values after execution
  ASSERT_EQ(n1->get<double>(), 1.0);
  ASSERT_EQ(n2->get<double>(), 2.0);
  ASSERT_EQ(n3->get<double>(), 0.0);

  // Make sure the self outputs are ok
  ASSERT_EQ(n1->output(0), n1);
  ASSERT_EQ(n2->output(0), n2);
  ASSERT_EQ(n3->output(0), n3);

  // Make sure that asking for the value from the output node gives the
  // expected result
  ASSERT_EQ(n1->output(0)->get<double>(), 1.0);
  ASSERT_EQ(n2->output(0)->get<double>(), 2.0);
  ASSERT_EQ(n3->output(0)->get<double>(), 0.0);

  // Check the connections of the sum node
  ASSERT_EQ(n4->input(0), n3->output(0));
  ASSERT_EQ(n4->input(1), n1->output(0));
  ASSERT_EQ(n4->input(2), n2->output(0));

  // Verify the pass through node
  ASSERT_EQ(n4->output(0), n4->input(0));

  network.output_dot("bare_minimal.dot");
  // dump the json of the network
  json          serialized_json = network;
  std::ofstream json_file("bare_minimal.json");
  json_file << serialized_json.dump(2);
  json_file.close();

  std::cout << "Executing network with 3 nodes and 2 connections." << std::endl;
  // Run the network
  network.run();
  std::cout << "Network executed." << std::endl;

  ASSERT_EQ(n3->get<double>(), 3.0)
    << "The output node should have the value 3.0";
}

TEST(NetworkTest, ExplicitNodeNaming)
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

TEST(NetworkTest, AutoNameOnConnection)
{
  coral::NodeObject::register_elementary_type<double>();
  auto pass = [](const double &a) { return a; };
  coral::NodeObject::register_function(pass, {"pass", "out", "in"});

  coral::Network network;
  auto           src_id = network.add_node(coral::make_node(0.0)); // unnamed
  auto target_id = network.add_node(coral::make_method_node("pass", pass));

  // Connect unnamed source self-output (-1) to target input 1 (argument "in").
  network.add_connection(src_id, target_id, 0, 1);
  json network_json = network;
  std::cout << network_json.dump(2) << std::endl;

  ASSERT_EQ(network.get_node_name(src_id), "in");
}

TEST(NetworkTest, ParseAndDump)
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
  ASSERT_EQ(output_json["workflow"]["nodes"].size(), 10)
    << "Should have 6 nodes in the workflow";
  ASSERT_EQ(output_json["workflow"]["edges"].size(), 9)
    << "Should have 2 edges in the workflow";
}

// ParseAndExecuteNetwork test moved to failing.cc

// JsonBasedWorkflow test moved to failing.cc

// ValidateEdgeConnections test moved to failing.cc

// VerifyNodeTypes test moved to failing.cc

// ConnectionsMapTracking test moved to failing.cc

TEST(NetworkTest, ConnectionSerialization)
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
TEST(NetworkTest, JsonBasedWorkflow)
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

  // Basic verification of nodes - should have 10 nodes
  ASSERT_EQ(network.size(), 10);

  // Verify each node exists and has the right type
  for (int i = 0; i < 10; ++i)
    {
      auto node = network.get_node(i);
      ASSERT_TRUE(node != nullptr) << "Node " << i << " should exist";
    }
}

// Test for validating edge connections
TEST(NetworkTest, ValidateEdgeConnections)
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
  ASSERT_EQ(network.size(), 10);

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
  // 5. Node 6 -> Node 7 (std::string)
  // 6. Node 5 -> Node 9
  // 7. Node 7 -> Node 9
  // 8. Node 8 -> Node 9
  ASSERT_EQ(tf.num_tasks(), 10);

  // Verify the connections using our tracking map
  ASSERT_EQ(network.n_connections(), 9);
  EXPECT_TRUE(network.is_connected(0, 3));
  EXPECT_TRUE(network.is_connected(1, 3));
  EXPECT_TRUE(network.is_connected(2, 3));
  EXPECT_TRUE(network.is_connected(3, 5));
  EXPECT_TRUE(network.is_connected(4, 5));
  EXPECT_TRUE(network.is_connected(6, 7));
  EXPECT_TRUE(network.is_connected(5, 9));
  EXPECT_TRUE(network.is_connected(7, 9));
  EXPECT_TRUE(network.is_connected(8, 9));
}

// Test for verifying node types
TEST(NetworkTest, VerifyNodeTypes)
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
  ASSERT_EQ(network.size(), 10);

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

  // Check node 6: Should be of type "std::string" with nodeType
  // "elementary_constructor"
  auto node6 = network.get_node(6);
  ASSERT_TRUE(node6 != nullptr);
  EXPECT_EQ(node6->node_type(), coral::NodeType::elementary_constructor);
  EXPECT_EQ(node6->type_name(), "std::string");

  // Check node 7: Should be of type "std::basic_ofstream<char>" with nodeType
  // "elementary_constructor"
  auto node7 = network.get_node(7);
  ASSERT_TRUE(node7 != nullptr);
  EXPECT_EQ(node7->node_type(), coral::NodeType::constructor);
  EXPECT_EQ(node7->type_name(), "std::basic_ofstream<char>");

  // Check node 8: Should be of type dealii::GridOut
  // with nodeType "empty_constructor"
  auto node8 = network.get_node(8);
  ASSERT_TRUE(node8 != nullptr);
  ASSERT_EQ(node8->node_type(), coral::NodeType::empty_constructor);
  ASSERT_EQ(node8->type_name(), "dealii::GridOut");

  // Check node 9: Shoud be of type
  // void(dealii::GridOut::*)(dealii::Triangulation<2, 2> const&, std::ostream&)
  // const with nodeType "void_const_method"
  auto node9 = network.get_node(9);
  ASSERT_TRUE(node9 != nullptr);
  ASSERT_EQ(node9->node_type(), coral::NodeType::void_const_method);
  ASSERT_EQ(
    node9->type_name(),
    "void(dealii::GridOut::*)(dealii::Triangulation<2, 2> const&, std::ostream&) const");
}

// Test for connections map tracking
TEST(NetworkTest, ConnectionsMapTracking)
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
  ASSERT_EQ(network.n_connections(), 9) << "Should have 9 connections";

  // Verify specific connections
  EXPECT_TRUE(network.is_connected(0, 3)) << "Node 0 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(1, 3)) << "Node 1 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(2, 3)) << "Node 2 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(3, 5)) << "Node 3 should connect to Node 5";
  EXPECT_TRUE(network.is_connected(4, 5)) << "Node 4 should connect to Node 5";
  EXPECT_FALSE(network.is_connected(5, 0))
    << "Node 5 should not connect to Node 0";
  EXPECT_TRUE(network.is_connected(6, 7)) << "Node 6 should connect to Node 7";
  EXPECT_TRUE(network.is_connected(5, 9)) << "Node 5 should connect to Node 9";
  EXPECT_TRUE(network.is_connected(7, 9)) << "Node 7 should connect to Node 9";
  EXPECT_TRUE(network.is_connected(8, 9)) << "Node 8 should connect to Node 9";
  EXPECT_FALSE(network.is_connected(6, 9))
    << "Node 6 should not connect to node 9";

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
  EXPECT_EQ(node5_targets.size(), 1)
    << "Node 5 should have 1 outgoing connection";
  EXPECT_EQ(node5_targets[0], 9) << "Node 5 should connect to Node 9";

  auto node6_targets = network.get_connected_nodes(6);
  EXPECT_EQ(node6_targets.size(), 1)
    << "Node 6 should have 1 outgoing connection";
  EXPECT_EQ(node6_targets[0], 7) << "Node 6 should connect to Node 7";

  auto node7_targets = network.get_connected_nodes(7);
  EXPECT_EQ(node7_targets.size(), 1)
    << "Node 7 should have 1 outgoing connection";
  EXPECT_EQ(node7_targets[0], 9) << "Node 7 should connect to Node 9";

  auto node8_targets = network.get_connected_nodes(8);
  EXPECT_EQ(node8_targets.size(), 1)
    << "Node 8 should have 1 outgoing connection";
  EXPECT_EQ(node8_targets[0], 9) << "Node 8 should connect to Node 9";

  auto node9_targets = network.get_connected_nodes(9);
  EXPECT_TRUE(node9_targets.empty())
    << "Node 9 should not have outoing connections";

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

  auto node5_conns = network.get_node_connections(5);
  EXPECT_EQ(node5_conns.size(), 1) << "Node 5 should have 1 connection object";
  EXPECT_EQ(node5_conns[0].source_id, 5);
  EXPECT_EQ(node5_conns[0].target_id, 9);

  auto node6_conns = network.get_node_connections(6);
  EXPECT_EQ(node6_conns.size(), 1) << "Node 6 should have 1 connection object";
  EXPECT_EQ(node6_conns[0].source_id, 6);
  EXPECT_EQ(node6_conns[0].target_id, 7);

  auto node7_conns = network.get_node_connections(7);
  EXPECT_EQ(node7_conns.size(), 1) << "Node 7 should have 1 connection object";
  EXPECT_EQ(node7_conns[0].source_id, 7);
  EXPECT_EQ(node7_conns[0].target_id, 9);

  auto node8_conns = network.get_node_connections(8);
  EXPECT_EQ(node8_conns.size(), 1) << "Node 8 should have 1 connection object";
  EXPECT_EQ(node8_conns[0].source_id, 8);
  EXPECT_EQ(node8_conns[0].target_id, 9);

  auto node9_conns = network.get_node_connections(9);
  EXPECT_TRUE(node9_conns.empty())
    << "Node 9 should not have connection objects";
}

// Test for network serialization
TEST(NetworkTest, NetworkSerialization)
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
  ASSERT_EQ(serialized_json["workflow"]["nodes"].size(), 10)
    << "Should have 10 nodes";

  // Verify node structure
  for (const auto &[id, node] : serialized_json["workflow"]["nodes"].items())
    {
      ASSERT_TRUE(node.contains("type")) << "Node must have type field";
      ASSERT_TRUE(node.contains("node_type"))
        << "Node must have node_type field";
      ASSERT_TRUE(node.contains("outputs")) << "Node must have outputs field";
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
  EXPECT_EQ(new_network.size(), 10);
  EXPECT_EQ(new_network.n_connections(), 9);

  // Verify the specific connections
  EXPECT_TRUE(new_network.is_connected(0, 3));
  EXPECT_TRUE(new_network.is_connected(1, 3));
  EXPECT_TRUE(new_network.is_connected(2, 3));
  EXPECT_TRUE(new_network.is_connected(3, 5));
  EXPECT_TRUE(new_network.is_connected(4, 5));
  EXPECT_TRUE(new_network.is_connected(6, 7));
  EXPECT_TRUE(new_network.is_connected(5, 9));
  EXPECT_TRUE(new_network.is_connected(7, 9));
  EXPECT_TRUE(new_network.is_connected(8, 9));
}

TEST(NetworkTest, RegistrySubset)
{
  coral::NodeObject::register_elementary_type<int>();
  coral::NodeObject::register_elementary_type<double>();

  coral::Network network;
  network.add_node(coral::make_node(1));
  network.add_node(coral::make_node(2.5));

  auto registry = network.get_registry();
  ASSERT_EQ(registry.size(), 2);
  EXPECT_TRUE(registry.contains(coral::hash<int>()));
  EXPECT_TRUE(registry.contains(coral::hash<double>()));
  EXPECT_EQ(registry[coral::hash<int>()]["node_type"], "elementary_constructor");
  EXPECT_EQ(registry[coral::hash<double>()]["node_type"],
            "elementary_constructor");
}

TEST(NetworkTest, ParseAndExecuteNetwork)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Create and populate the network
  coral::Network network = json_data;

  // Output some debug information
  std::cout << "Network has " << network.size() << " nodes and "
            << network.n_connections() << " connections\n";

  // Execute the network
  network.run();

  // Verify results
  ASSERT_EQ(16, network.get_node(0)->get<Triangulation<2>>().n_active_cells());
}
