#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"

using namespace dealii;
using namespace coral;

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
  ASSERT_EQ(s, coral::detail::hash<type>());

  // Verify JSON serialization is working correctly
  ASSERT_TRUE(j.contains("value"));
  auto json_value = json::parse(j.at("value").get<std::string>());
  ASSERT_EQ(json_value.size(), 2);
  ASSERT_EQ(json_value[0], 1.0);
  ASSERT_EQ(json_value[1], 2.0);
}
