#include <gtest/gtest.h>

#include "coral.h"

using namespace coral;

TEST(TrivialTypes, Int)
{
  using type = int;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = 42;
  ASSERT_EQ(42, obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::detail::hash<type>());
}

TEST(TrivialTypes, Double)
{
  using type = double;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = 42.0;
  ASSERT_EQ(42.0, obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::detail::hash<type>());
}

TEST(TrivialTypes, String)
{
  using type = std::string;
  NodeObject::register_elementary_type<type>();
  NodeObject obj = std::string("Hello world!");
  ASSERT_EQ("Hello world!", obj.get<type>());
  const auto &j = obj.get_info();
  std::string s = j.at("type");
  ASSERT_EQ(s, coral::detail::hash<type>());
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
  NodeObjectPtr obj = make_node(42);
  ASSERT_EQ(42, obj->get<int>());

  // Check that the value is set correctly
  ASSERT_EQ(obj->get_info().at("value"), "42");

  // Check that the initializer does not change the value
  (*obj)();
  ASSERT_EQ(42, obj->get<int>());
}
