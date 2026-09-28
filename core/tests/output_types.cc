#include <gtest/gtest.h>

#include "coral.h"

namespace
{
  struct OutputOnly
  {
    explicit OutputOnly(const int value)
      : value(value)
    {}

    OutputOnly() = delete;

    int value;
  };

  OutputOnly
  make_output(const int &value)
  {
    return OutputOnly(value);
  }
} // namespace

TEST(OutputType, RegistersValueWithoutConstructor)
{
  coral::NodeObject::register_elementary_type<int>();
  coral::NodeObject::register_output_type<OutputOnly>();

  const auto registry = coral::NodeObject::get_registry();
  const auto entry    = registry.at(coral::detail::hash<OutputOnly>());
  ASSERT_EQ(entry.at("node_type"), "output_only");
  EXPECT_TRUE(entry.at("arguments").empty());
  EXPECT_TRUE(entry.at("inputs").empty());
  EXPECT_TRUE(entry.at("outputs").empty());

  auto value = coral::make_node(coral::detail::hash<OutputOnly>());
  EXPECT_EQ(value->node_type(), coral::NodeType::output_only);
  EXPECT_FALSE(value->ready());
}

TEST(OutputType, FreeFunctionRegistersAndProducesOutputOnlyValue)
{
  coral::NodeObject::register_elementary_type<int>();
  coral::NodeObject::register_function(std::function<OutputOnly(const int &)>(
                                         make_output),
                                       {"make_output", "value"});

  auto function = coral::make_node("make_output");
  function->set_arguments({coral::make_node(7)});

  ASSERT_TRUE((*function)());
  EXPECT_EQ(function->get<OutputOnly>().value, 7);
  EXPECT_EQ(function->output_hash(), coral::detail::hash<OutputOnly>());
}
