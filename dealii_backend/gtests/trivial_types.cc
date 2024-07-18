#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include "coral.h"

using namespace dealii;
using namespace coral;

TEST(TrivialTypes, Int)
{
  using type = int;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = 42;
  ASSERT_EQ(42, obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type_hash");
  ASSERT_EQ(s, coral::hash<type>());
}

TEST(TrivialTypes, Double)
{
  using type = double;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = 42.0;
  ASSERT_EQ(42.0, obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type_hash");
  ASSERT_EQ(s, coral::hash<type>());
}

TEST(TrivialTypes, String)
{
  using type = std::string;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = std::string("Hello world!");
  ASSERT_EQ("Hello world!", obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type_hash");
  ASSERT_EQ(s, coral::hash<type>());
}

TEST(TrivialTypes, Point)
{
  using type = Point<2>;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = type(1.0, 2.0);
  ASSERT_EQ(type(1.0, 2.0), obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type_hash");
  ASSERT_EQ(s, coral::hash<type>());
}


TEST(TrivialTypes, Unregistered)
{
  // Check we throw an assert if we try to get an unregistered type
  ASSERT_THROW(NodeObject((float)42.0), dealii::ExceptionBase);
}


TEST(TrivialTypes, CopyType)
{
  NodeObject::register_elementary_type<int>();
  NodeObject obj  = 42;
  NodeObject obj2 = obj;
  ASSERT_EQ(42, obj2.get<int>());
  ASSERT_EQ(42, obj2.get<const int>());
}