#include <fstream>

#include "coral_network.h"
#include "gtest/gtest.h"
#include "json.hpp"

TEST(NetworkTest, ParseAndExecuteNetwork)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/editor-state.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Validate nodes
  ASSERT_EQ(network.size(), 3);

  // Check node 1
  auto node1 = network.get_node(1);
  ASSERT_TRUE(node1 != nullptr);

  // Check node 2
  auto node2 = network.get_node(2);
  ASSERT_TRUE(node2 != nullptr);

  // Check node 3
  auto node3 = network.get_node(3);
  ASSERT_TRUE(node3 != nullptr);

  // Execute the network
  network.run();
}

TEST(NetworkTest, JsonBasedWorkflow)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/editor-state.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Create and populate the network
  coral::Network network;
  network.from_json(json_data);

  // Execute the network
  network.run();
}
