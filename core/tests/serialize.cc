#include <gtest/gtest.h>

#include "coral.h"

using namespace coral;

TEST(Serialize, Int)
{
  using type = int;
  NodeObject::register_elementary_type<type>();
  NodeObjectPtr obj = make_node(42);
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(42, obj->get<type>());
  ASSERT_EQ("42", obj->to_string());

  // Serialize to json
  json j = obj;
  ASSERT_TRUE(j["node_type"] == "elementary_constructor");
  ASSERT_TRUE(j["value"] == "42");

  // Deserialize from json
  auto obj2 = j.get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());
  ASSERT_EQ(obj->get<type>(), obj2->get<type>());

  // Make sure that the deserialized object has the same value
  ASSERT_EQ(42, obj2->get<type>());
  ASSERT_EQ("42", obj2->to_string());

  // Make sure that after executing the object, it still holds the same value
  (*obj2)();
  ASSERT_EQ("42", obj2->to_string());
}

TEST(Serialize, String)
{
  using type = std::string;
  NodeObject::register_elementary_type<type>();
  NodeObjectPtr obj = make_node(type("test"));
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(type("test"), obj->get<type>());
  ASSERT_EQ("test", obj->to_string());

  // Serialize to json
  json j = obj;
  ASSERT_TRUE(j["node_type"] == "elementary_constructor");
  ASSERT_TRUE(j["value"] == "test");

  // Deserialize from json
  auto obj2 = j.get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());
  ASSERT_EQ(obj->get<type>(), obj2->get<type>());

  // Check the value of the deserialized object
  ASSERT_EQ("test", obj2->to_string());
  ASSERT_EQ(type("test"), obj2->get<type>());

  // Make sure that after executing the object, it still holds the same value
  (*obj2)();
  ASSERT_EQ("test", obj2->to_string());
}

TEST(Serialize, ToFromJson)
{
  using type = int;
  NodeObject::register_elementary_type<type>();
  NodeObjectPtr obj = make_node(type(42));

  json          j    = obj;
  NodeObjectPtr obj2 = j;
  json          j2   = obj2;

  ASSERT_EQ(j.dump(2), j2.dump(2));
  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());
  ASSERT_EQ(obj->get<type>(), obj2->get<type>());
  ASSERT_EQ("42", obj2->to_string());

  (*obj2)();
  ASSERT_EQ("42", obj2->to_string());

  (*obj)();
  ASSERT_EQ("42", obj->to_string());
}

TEST(Serialize, QualifiedId)
{
  using type = int;
  NodeObject::register_elementary_type<type>();

  // Create a JSON object with qualified_id field
  json j;
  j["type"]         = "int";
  j["node_type"]    = "elementary_constructor";
  j["value"]        = "123";
  j["qualified_id"] = "test_node_42";

  // Deserialize from JSON
  NodeObjectPtr obj = j.get<NodeObjectPtr>();

  // Verify qualified_id was stored correctly
  ASSERT_EQ("test_node_42", obj->get_qualified_id());
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(123, obj->get<type>());

  // Serialize back to JSON and verify qualified_id is included
  json j2 = obj;
  ASSERT_TRUE(j2.contains("qualified_id"));
  ASSERT_EQ("test_node_42", j2["qualified_id"].get<std::string>());

  // Test that get_info() includes qualified_id
  const auto &info = obj->get_info();
  ASSERT_TRUE(info.contains("qualified_id"));
  ASSERT_EQ("test_node_42", info["qualified_id"].get<std::string>());
}

TEST(Serialize, QualifiedIdMissing)
{
  using type = std::string;
  NodeObject::register_elementary_type<type>();

  // Create a proper object first to get the correct type hash
  NodeObjectPtr temp = make_node(type("hello"));
  json          j    = temp; // Serialize to get correct structure

  // Remove qualified_id if it exists (to test missing case)
  j.erase("qualified_id");

  // Deserialize from JSON
  NodeObjectPtr obj = j.get<NodeObjectPtr>();

  // Verify get_qualified_id() returns empty string when not set
  ASSERT_EQ("", obj->get_qualified_id());
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ("hello", obj->get<type>());

  // Serialize back to JSON - should not have qualified_id
  json j2 = obj;
  ASSERT_FALSE(j2.contains("qualified_id"));
}
