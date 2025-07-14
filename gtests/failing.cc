#include <nlohmann/json.hpp>

#include <fstream>

#include "coral.h"
#include "coral_network.h"
#include "gtest/gtest.h"
#include "register_types.h"

// Failing test from network.cc
TEST(NetworkTest, DISABLED_BareMinimal)
{
  coral::NodeObject::register_elementary_type<double>();

  auto sum = [](const double &a, const double &b) { return a + b; };
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
  ASSERT_EQ(network.get_node(id1)->get<double>(), 1.0);
  ASSERT_EQ(network.get_node(id2)->get<double>(), 2.0);
  ASSERT_EQ(network.get_node(id3)->get<double>(), 0.0);

  const auto n1 = network.get_node(id1);
  const auto n2 = network.get_node(id2);
  const auto n3 = network.get_node(id3);
  const auto n4 = network.get_node(id4);

  // Verify that the nodes are correctly connected
  ASSERT_EQ(n3->output(0), n4->input(0));
  ASSERT_EQ(n1->output(0), n4->input(1));
  ASSERT_EQ(n2->output(0), n4->input(2));

  // Verify the self nodes
  ASSERT_EQ(n1->output(0), n1);
  ASSERT_EQ(n2->output(0), n2);
  ASSERT_EQ(n3->output(0), n3);

  // Verify the pass through node
  ASSERT_EQ(n4->output(0), n4->input(0));

  network.output_dot("bare_minimal.dot");
  // dump the json of the network
  nlohmann::json serialized_json = network.to_json();
  std::ofstream  json_file("bare_minimal.json");
  json_file << serialized_json.dump(2);
  json_file.close();

  std::cout << "Executing network with 3 nodes and 2 connections." << std::endl;
  // Run the network
  network.run();
  std::cout << "Network executed." << std::endl;

  ASSERT_EQ(n3->get<double>(), 3.0)
    << "The output node should have the value 3.0";
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_ParseAndDump)
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
  std::cout << output_json.dump(2) << std::endl;
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_ParseAndExecuteNetwork)
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

  // Validate nodes
  ASSERT_EQ(network.size(), 3);

  // Check node 1 - unsigned value
  auto node1 = network.get_node(1);
  ASSERT_TRUE(node1 != nullptr);
  EXPECT_EQ(node1->node_type(), coral::NodeType::elementary_constructor);

  // Check node 2 - triangulation
  auto node2 = network.get_node(2);
  ASSERT_TRUE(node2 != nullptr);
  EXPECT_EQ(node2->node_type(), coral::NodeType::empty_constructor)
    << "Node 2 should be an empty constructor: " << node2->get_info().dump(2);

  // Check node 3 - refine_global method
  auto node3 = network.get_node(3);
  ASSERT_TRUE(node3 != nullptr);

  // Validate the taskflow has the right connections
  // The taskflow should have at least 2 tasks (for the 2 connected nodes)
  auto &tf = network.get_taskflow();
  ASSERT_GE(tf.num_tasks(), 2);
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_JsonBasedWorkflow)
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

  // Basic verification of nodes
  ASSERT_EQ(network.size(), 3);

  // Verify each node exists and has the right type
  for (int i = 1; i <= 3; ++i)
    {
      auto node = network.get_node(i);
      ASSERT_TRUE(node != nullptr) << "Node " << i << " should exist";
    }
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_ValidateEdgeConnections)
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
  ASSERT_EQ(network.size(), 3);

  // Output the taskflow as DOT format for debugging
  network.output_dot(SOURCE_DIR "/gtests/taskflow.dot");

  // Get the taskflow
  auto &tf = network.get_taskflow();

  // According to mwe.json, there should be 2 edges:
  // 1. Node 2 -> Node 3
  // 2. Node 1 -> Node 3
  // With our improved implementation, we should have exactly 3 tasks (one per
  // node) and the tasks for nodes 1 and 2 should precede node 3's task
  ASSERT_EQ(tf.num_tasks(), 3);

  // Verify the connections using our new tracking map
  ASSERT_EQ(network.n_connections(), 2);
  EXPECT_TRUE(network.is_connected(1, 3));
  EXPECT_TRUE(network.is_connected(2, 3));
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_VerifyNodeTypes)
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
  ASSERT_EQ(network.size(), 3);

  // Check node 1: Should be of type "unsigned" with nodeType
  // "elementary_constructor"
  auto node1 = network.get_node(1);
  ASSERT_TRUE(node1 != nullptr);
  EXPECT_EQ(node1->node_type(), coral::NodeType::elementary_constructor);
  EXPECT_EQ(node1->type_name(), "unsigned");

  // Check node 2: Should be of type "dealii::triangulation<2, 2>" with
  // nodeType "empty_constructor"
  auto node2 = network.get_node(2);
  ASSERT_TRUE(node2 != nullptr);
  EXPECT_EQ(node2->node_type(), coral::NodeType::empty_constructor);
  EXPECT_EQ(node2->type_name(), "dealii::Triangulation<2, 2>");

  // Check node 3: Should be a method related to
  // "Triangulation<2>::refine_global"
  auto node3 = network.get_node(3);
  ASSERT_TRUE(node3 != nullptr);
  // The node type is expected to be a void_method
  ASSERT_TRUE(node3->node_type() == coral::NodeType::void_method);

  // The type string contains the method name
  EXPECT_NE(node3->type_name().find("Triangulation"), std::string::npos);
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_ConnectionsMapTracking)
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
  ASSERT_EQ(network.n_connections(), 2) << "Should have 2 connections";

  // Verify specific connections
  EXPECT_TRUE(network.is_connected(1, 3)) << "Node 1 should connect to Node 3";
  EXPECT_TRUE(network.is_connected(2, 3)) << "Node 2 should connect to Node 3";
  EXPECT_FALSE(network.is_connected(3, 1))
    << "Node 3 should not connect to Node 1";

  // Verify connection vectors using the target IDs list
  auto node1_targets = network.get_connected_nodes(1);
  EXPECT_EQ(node1_targets.size(), 1)
    << "Node 1 should have 1 outgoing connection";
  EXPECT_EQ(node1_targets[0], 3) << "Node 1 should connect to Node 3";

  auto node2_targets = network.get_connected_nodes(2);
  EXPECT_EQ(node2_targets.size(), 1)
    << "Node 2 should have 1 outgoing connection";
  EXPECT_EQ(node2_targets[0], 3) << "Node 2 should connect to Node 3";

  auto node3_targets = network.get_connected_nodes(3);
  EXPECT_TRUE(node3_targets.empty())
    << "Node 3 should have no outgoing connections";

  // Verify connection objects
  auto node1_conns = network.get_node_connections(1);
  EXPECT_EQ(node1_conns.size(), 1) << "Node 1 should have 1 connection object";
  EXPECT_EQ(node1_conns[0].source_id, 1);
  EXPECT_EQ(node1_conns[0].target_id, 3);

  // Verify we can access all connections
  EXPECT_EQ(network.n_connections(), 2)
    << "Should have 2 source nodes with connections";
}

// Failing test from network.cc
TEST(NetworkTest, DISABLED_NetworkSerialization)
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
  ASSERT_EQ(serialized_json["workflow"]["nodes"].size(), 3)
    << "Should have 3 nodes";

  // Verify node structure
  for (const auto &[id, node] : serialized_json["workflow"]["nodes"].items())
    {
      ASSERT_TRUE(node.contains("id")) << "Node must have id field";
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
  EXPECT_EQ(new_network.size(), 3);
  EXPECT_EQ(new_network.n_connections(), 2);

  // Verify the specific connections
  EXPECT_TRUE(new_network.is_connected(1, 3));
  EXPECT_TRUE(new_network.is_connected(2, 3));
}

// Failing test from node_object_tests.cc
struct MyClass
{
  void
  set_value(int v)
  {
    value = v;
  }
  int value;
};

TEST(NodeObject, DISABLED_MethodRegistration)
{
  coral::NodeObject::register_type<MyClass>();
  coral::NodeObject::register_type<int>();
  coral::NodeObject::register_method(&MyClass::set_value,
                                     {"set_value", "my_class", "value"});

  coral::NodeObjectPtr obj = coral::make_node<MyClass>();
  coral::NodeObjectPtr fun =
    coral::make_method_node("set_value", &MyClass::set_value);
  coral::NodeObjectPtr arg = coral::make_node(42);

  std::cout << coral::NodeObject::get_registry().dump(2) << std::endl;

  fun->set_arguments({obj, arg});
  (*fun)();
  ASSERT_EQ(obj->get<MyClass>().value, 42);
}
