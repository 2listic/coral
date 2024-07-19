#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include "coral.h"

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
  ASSERT_TRUE(j["run_type"] == "elementary_constructor");
  ASSERT_TRUE(j["value"] == "42");

  // Deserialize from json
  auto obj2 = j.template get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());
  ASSERT_EQ(obj->get<type>(), obj2->get<type>());
}

TEST(Serialize, Point)
{
  using type = Point<2>;
  NodeObject::register_elementary_type<type>();
  NodeObjectPtr obj = make_node(type(0.0, 1.0));
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(type(0.0, 1.0), obj->get<type>());
  ASSERT_EQ("0, 1", obj->to_string());

  // Serialize to json
  json j = obj;
  ASSERT_TRUE(j["run_type"] == "elementary_constructor");
  ASSERT_TRUE(j["value"] == "0, 1");

  // Deserialize from json
  auto obj2 = j.template get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());
  ASSERT_EQ(obj->get<type>(), obj2->get<type>());
}


TEST(Serialize, Triangulation)
{
  using type = Triangulation<2>;
  NodeObject::register_type<type>();
  NodeObjectPtr obj = make_node<type>();
  (*obj)();

  ASSERT_TRUE(obj->ready());

  // Serialize to json
  json j = obj;
  ASSERT_TRUE(j["run_type"] == "empty_constructor");

  // Deserialize from json. This will call the constructor.
  auto obj2 = j.template get<NodeObjectPtr>();

  ASSERT_EQ(obj->hash(), obj2->hash());
  ASSERT_TRUE(obj2->ready());

  // The two objects are actually different objects in memory.
  ASSERT_NE(&obj->get<type>(), &obj2->get<type>());
}
