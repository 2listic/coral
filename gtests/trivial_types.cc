#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"

using namespace dealii;
using namespace coral;

TEST(TrivialTypes, Int)
{
  using type = int;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = 42;
  ASSERT_EQ(42, obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::hash<type>());
}

TEST(TrivialTypes, Double)
{
  using type = double;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = 42.0;
  ASSERT_EQ(42.0, obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::hash<type>());
}

TEST(TrivialTypes, String)
{
  using type = std::string;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = std::string("Hello world!");
  ASSERT_EQ("Hello world!", obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::hash<type>());
}

TEST(TrivialTypes, Point)
{
  using type = Point<2>;
  // Now we can use register_elementary_type since we have JSON serialization
  // for Point<dim>
  NodeObject::register_elementary_type<type>();
  NodeObject obj = type(1.0, 2.0);
  ASSERT_EQ(type(1.0, 2.0), obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::hash<type>());

  // Verify JSON serialization is working correctly
  ASSERT_TRUE(j.contains("value"));
  auto json_value = json::parse(j.at("value").get<std::string>());
  ASSERT_EQ(json_value.size(), 2);
  ASSERT_EQ(json_value[0], 1.0);
  ASSERT_EQ(json_value[1], 2.0);
}


TEST(TrivialTypes, Unregistered)
{
  // Check we throw a runtime error if we try to get an unregistered type
  ASSERT_THROW(NodeObject((float)42.0), std::runtime_error);
}


TEST(TrivialTypes, CopyType)
{
  NodeObject::register_elementary_type<int>();
  NodeObject obj  = 42;
  NodeObject obj2 = obj;
  ASSERT_EQ(42, obj2.get<int>());
  ASSERT_EQ(42, obj2.get<const int>());
}


TEST(TrivialTypes, RunInitializerInt)
{
  NodeObject::register_elementary_type<int>();
  NodeObject obj = 42;
  ASSERT_EQ(42, obj.get<int>());

  // Check that the value is set correctly
  ASSERT_EQ(obj.get_info().at("value"), "42");

  // Check that the initializer does not change the value
  obj();
  ASSERT_EQ(42, obj.get<int>());
}
