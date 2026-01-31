#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"
#include "test_utils.h"

using namespace dealii;
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

  // auto j2 = obj2->get_info();
  // ASSERT_TRUE(j2["value"] == "42");

  // Make sure that after executing the object, it still holds the same value
  (*obj2)();
  ASSERT_EQ("42", obj2->to_string());
}

TEST(Serialize, Point)
{
  using type = Point<2>;
  NodeObject::register_elementary_type<type>();
  NodeObjectPtr obj = make_node(type(0.0, 1.0));
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(type(0.0, 1.0), obj->get<type>());
  ASSERT_EQ("[0.0,1.0]", obj->to_string());

  // Serialize to json
  json j = obj;
  ASSERT_TRUE(j["node_type"] == "elementary_constructor");
  ASSERT_TRUE(j["value"] == "[0.0,1.0]");

  // Deserialize from json
  auto obj2 = j.get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());
  ASSERT_EQ(obj->get<type>(), obj2->get<type>());

  // Make sure that after executing the object, it still holds the same value
  (*obj2)();
  ASSERT_EQ("[0.0,1.0]", obj2->to_string());
}


TEST(Serialize, Triangulation)
{
  coral_test::ScopedTestOutputDir output_dir("Serialize_Triangulation");

  using type = Triangulation<2>;
  NodeObject::register_type<type>();
  NodeObjectPtr obj = make_node<type>();
  (*obj)();

  ASSERT_TRUE(obj->ready());

  // Serialize to json
  json j = obj;
  ASSERT_TRUE(j["node_type"] == "empty_constructor");

  // Deserialize from json. This will call the constructor.
  auto obj2 = j.get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());

  // The two objects are actually different objects in memory.
  ASSERT_NE(&obj->get<type>(), &obj2->get<type>());

  // Dump the json to a file
  std::ofstream ofs(output_dir.path() / "triangulation.json");
  ofs << j.dump(2);
  ofs.close();
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
  ASSERT_EQ(obj->get<type>(),
            obj2->get<type>()); // Check the value of the deserialized object
  ASSERT_EQ("42", obj2->to_string());
  // Make sure that after executing the object, it still holds the same value
  (*obj2)();
  ASSERT_EQ("42", obj2->to_string());

  // Same with obj
  (*obj)();
  ASSERT_EQ("42", obj->to_string());
}
