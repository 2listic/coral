#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"

using namespace coral;

// ============================================================================
// Phase 1: Foundation - std::function Type Support
// ============================================================================

// Test 1.1: Register std::function Type
TEST(FunctionType, Registration)
{
  // USE CASE: Register a std::function type and verify it appears in registry

  using FunctionType = std::function<double(double)>;

  // Set a clean type alias (like how std::string is aliased in register_types.h)
  coral::detail::set_type_alias<FunctionType>("std::function<double(double)>");

  // Register the function type (use register_function_type, not elementary_type)
  NodeObject::register_function_type<double, double>();

  // Get the expected hash for this type
  std::string expected_hash = detail::hash<FunctionType>();

  // Verify it appears in the registry
  auto registry = NodeObject::get_registry();

  bool found = false;
  for (const auto &item : registry)
    {
      if (item.contains("type") && item.at("type") == expected_hash)
        {
          found = true;
          // Verify it has correct metadata
          ASSERT_TRUE(item.contains("node_type"));
          ASSERT_EQ(item.at("node_type"), "empty_constructor");
          break;
        }
    }

  ASSERT_TRUE(found) << "std::function<double(double)> not found in registry. "
                     << "Expected hash: " << expected_hash;
}


// Test 1.2: Create Node with std::function Value
TEST(FunctionType, CreateNodeWithStdFunction)
{
  // USE CASE: Store a function in a node

  using FunctionType = std::function<double(double)>;

  // Set clean type alias and register
  coral::detail::set_type_alias<FunctionType>("std::function<double(double)>");
  NodeObject::register_function_type<double, double>();

  // Create a simple lambda
  auto square = [](double x) { return x * x; };

  // Create std::function from lambda
  FunctionType fn = square;

  // Create NodeObject containing this std::function
  NodeObjectPtr node = make_node(fn);

  // Verify node is ready
  ASSERT_TRUE(node->ready());

  // Verify node contains the function
  const auto &info = node->get_info();
  std::string type_str = info.at("type");
  std::string expected_hash = detail::hash<FunctionType>();
  ASSERT_EQ(type_str, expected_hash);
}


// Test 1.3: Retrieve and Invoke std::function
TEST(FunctionType, RetrieveAndInvoke)
{
  // USE CASE: Extract and call a function from a node

  using FunctionType = std::function<double(double)>;

  // Set clean type alias and register
  coral::detail::set_type_alias<FunctionType>("std::function<double(double)>");
  NodeObject::register_function_type<double, double>();

  // Create node with std::function (square function)
  auto square = [](double x) { return x * x; };
  FunctionType fn = square;
  NodeObjectPtr node = make_node(fn);

  // Retrieve the function using get<>
  FunctionType &retrieved_fn = node->get<FunctionType>();

  // Invoke the function with argument 5.0
  double result = retrieved_fn(5.0);

  // Verify result is 25.0
  ASSERT_DOUBLE_EQ(result, 25.0);
}


// Test 1.4: Pass std::function Between Nodes
TEST(FunctionType, PassBetweenNodes)
{
  // USE CASE: Data flow of functions through graph

  using FunctionType = std::function<double(double)>;

  // Set clean type alias and register
  coral::detail::set_type_alias<FunctionType>("std::function<double(double)>");
  NodeObject::register_function_type<double, double>();
  NodeObject::register_elementary_type<double>();

  // Create node A containing std::function (square)
  auto square = [](double x) { return x * x; };
  FunctionType fn = square;
  NodeObjectPtr node_a = make_node(fn);

  // Create a function that takes std::function and a value, calls function
  auto evaluator = [](FunctionType &func, const double &value, double &result) {
    result = func(value);
  };

  // Register the evaluator function
  NodeObject::register_function(
    std::function<void(FunctionType &, const double &, double &)>(evaluator),
    {"evaluator", "function", "value", "result"});

  // Create node B (the evaluator) - use the registered function name
  NodeObjectPtr node_b = make_node("evaluator");

  // Create value node (input = 5.0)
  NodeObjectPtr value_node = make_node(5.0);

  // Create output node
  NodeObjectPtr output_node = make_node(0.0);

  // Connect A's output to B's first input (function parameter)
  node_b->bind_input(0, node_a->get_output(0));

  // Connect value to B's second input
  node_b->bind_input(1, value_node->get_output(0));

  // Connect output to B's third input
  node_b->bind_input(2, output_node->get_output(0));

  // Execute the network
  (*node_b)();

  // Verify output is 25.0
  double result = output_node->get<double>();
  ASSERT_DOUBLE_EQ(result, 25.0);
}
