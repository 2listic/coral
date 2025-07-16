
#include <nlohmann/json.hpp>

#include <fstream>

#include "coral.h"
#include "coral_network.h"
#include "gtest/gtest.h"
#include "register_types.h"


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
// ParseAndDump test moved to failing.cc

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

// NetworkSerialization test moved to failing.cc
