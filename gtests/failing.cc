#include <nlohmann/json.hpp>

#include <fstream>

#include "coral.h"
#include "coral_network.h"
#include "coral_utilities.h"
#include "gtest/gtest.h"
#include "register_types.h"

using namespace dealii;

// Disabled test from network.cc
TEST(NetworkTest, DISABLED_ParseAndExecuteNetwork)
{
  // Load the JSON file
  std::ifstream file(SOURCE_DIR "/test_files/mwe.json");
  ASSERT_TRUE(file.is_open()) << "Failed to open JSON file.";

  nlohmann::json json_data;
  file >> json_data;

  // Register types
  coral::register_all_types();

  // Fix hashes if needed
  nlohmann::json fixed_json = coral::fix_hashes(json_data);

  // Create and populate the network
  coral::Network network;
  network.from_json(fixed_json);

  // Output some debug information
  std::cout << "Network has " << network.size() << " nodes and "
            << network.n_connections() << " connections\n";

  // Execute the network
  network.run();

  // Verify results
  ASSERT_EQ(16, network.get_node(0)->get<Triangulation<2>>().n_active_cells());
}
