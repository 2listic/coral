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

  int
  metadata_output(const int &value)
  {
    return value;
  }

  int
  legacy_metadata_output(const int &value)
  {
    return value;
  }

  struct ExampleOwner
  {
    void
    set(const int &)
    {}
  };
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

TEST(OutputType, PreservesOptionalRegistryMetadata)
{
  coral::NodeObject::register_elementary_type<int>();

  coral::RegistryMetadata metadata;
  metadata.operation    = "metadata_operation";
  metadata.display_name = "Metadata operation";
  metadata.variant_name = "Integer variant";
  metadata.description  = "A test operation with optional metadata.";
  metadata.class_name   = "ExampleClass";
  metadata.method_name  = "example_method";

  coral::NodeObject::register_function(
    std::function<int(const int &)>(metadata_output), {"value"}, metadata);

  const auto registry_key =
    coral::detail::function_registry_key<std::function<int(const int &)>>(
      "metadata_operation");
  const auto registry = coral::NodeObject::get_registry();
  const auto entry    = registry.at(registry_key);
  EXPECT_EQ(entry.at("operation"), "metadata_operation");
  EXPECT_EQ(entry.at("display_name"), "Metadata operation");
  EXPECT_EQ(entry.at("variant_name"), "Integer variant");
  EXPECT_EQ(entry.at("description"),
            "A test operation with optional metadata.");
  EXPECT_EQ(entry.at("class_name"), "ExampleClass");
  EXPECT_EQ(entry.at("method_name"), "example_method");
}

TEST(OutputType, OmitsOptionalRegistryMetadataWhenNotProvided)
{
  coral::NodeObject::register_elementary_type<int>();
  coral::NodeObject::register_function(
    std::function<int(const int &)>(legacy_metadata_output),
    {"value"},
    {"legacy_operation", "Legacy operation"});

  const auto registry_key =
    coral::detail::function_registry_key<std::function<int(const int &)>>(
      "legacy_operation");
  const auto registry = coral::NodeObject::get_registry();
  const auto entry    = registry.at(registry_key);
  EXPECT_EQ(entry.at("operation"), "legacy_operation");
  EXPECT_EQ(entry.at("display_name"), "Legacy operation");
  EXPECT_FALSE(entry.contains("variant_name"));
  EXPECT_FALSE(entry.contains("description"));
}

TEST(OutputType, RegistersMethodIdentity)
{
  coral::NodeObject::register_elementary_type<int>();
  coral::NodeObject::register_output_type<ExampleOwner>();

  coral::RegistryMetadata metadata;
  metadata.operation    = "ExampleOwner::set";
  metadata.display_name = "Set";

  coral::NodeObject::register_method<ExampleOwner, void, const int &>(
    &ExampleOwner::set,
    {"ExampleOwner::set", "owner", "value"},
    metadata);

  const auto registry = coral::NodeObject::get_registry();
  const auto entry    = registry.at("ExampleOwner::set");
  EXPECT_EQ(entry.at("node_type"), "void_method");
  EXPECT_EQ(entry.at("operation"), "ExampleOwner::set");
  EXPECT_EQ(entry.at("display_name"), "Set");
  EXPECT_EQ(entry.at("class_name"), "ExampleOwner");
  EXPECT_EQ(entry.at("method_name"), "set");
}
