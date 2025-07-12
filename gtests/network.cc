
#include <nlohmann/json.hpp>

#include <fstream>

#include "coral.h"
#include "coral_network.h"
#include "gtest/gtest.h"
#include "register_types.h"

// BareMinimal test moved to failing.cc

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
