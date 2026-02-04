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


// ============================================================================
// Phase 2: Automatic std::function Type Registration
// ============================================================================

// Test 2.1: Auto-Register Single Function
TEST(AutoRegistration, SingleFunction)
{
  // USE CASE: Registering a function automatically creates the function type

  // Register basic types needed for this test
  NodeObject::register_elementary_type<double>();

  // Register a function
  auto square = [](double x) { return x * x; };
  NodeObject::register_function(
    std::function<double(double)>(square),
    {"square", "result", "x"});

  // Verify std::function<double(double)> automatically appears in registry
  using FunctionType = std::function<double(double)>;
  std::string expected_hash = detail::hash<FunctionType>();

  auto registry = NodeObject::get_registry();
  bool found = false;
  for (const auto &item : registry)
    {
      if (item.contains("type") && item.at("type") == expected_hash)
        {
          found = true;
          // Verify registry entry has correct metadata
          ASSERT_TRUE(item.contains("node_type"));
          ASSERT_EQ(item.at("node_type"), "empty_constructor");
          break;
        }
    }

  ASSERT_TRUE(found) << "std::function<double(double)> not auto-registered. "
                     << "Expected hash: " << expected_hash;
}


// Test 2.2: Multiple Functions Same Signature
TEST(AutoRegistration, SameSignatureMultipleFunctions)
{
  // USE CASE: Multiple functions with same signature share one function type

  // Register basic types needed for this test
  NodeObject::register_elementary_type<double>();

  // Register three functions with same signature: double(double)
  auto square = [](double x) { return x * x; };
  auto cube = [](double x) { return x * x * x; };
  auto times2 = [](double x) { return x * 2.0; };

  NodeObject::register_function(
    std::function<double(double)>(square),
    {"square", "result", "x"});

  NodeObject::register_function(
    std::function<double(double)>(cube),
    {"cube", "result", "x"});

  NodeObject::register_function(
    std::function<double(double)>(times2),
    {"times2", "result", "x"});

  // Verify only ONE std::function<double(double)> type is registered
  using FunctionType = std::function<double(double)>;
  std::string expected_hash = detail::hash<FunctionType>();

  auto registry = NodeObject::get_registry();
  int count = 0;
  for (const auto &item : registry)
    {
      if (item.contains("type") && item.at("type") == expected_hash)
        {
          count++;
        }
    }

  ASSERT_EQ(count, 1) << "Expected exactly 1 std::function<double(double)> "
                      << "type, found " << count;

  // Verify all three functions are registered (count function node types)
  int function_count = 0;
  for (const auto &item : registry)
    {
      if (item.contains("node_type") &&
          item.at("node_type") == "function")
        {
          std::string type_name = item.at("type");
          if (type_name == "square" || type_name == "cube" ||
              type_name == "times2")
            {
              function_count++;
            }
        }
    }

  ASSERT_EQ(function_count, 3) << "Expected 3 registered functions, found "
                               << function_count;
}


// Test 2.3: Multiple Functions Different Signatures
TEST(AutoRegistration, DifferentSignatures)
{
  // USE CASE: Different signatures create different function types

  // Register basic types needed for this test
  NodeObject::register_elementary_type<double>();
  NodeObject::register_elementary_type<int>();

  // Register three functions with different signatures
  auto square = [](double x) { return x * x; };
  auto add = [](double a, double b) { return a + b; };
  auto increment = [](int x) { return x + 1; };

  NodeObject::register_function(
    std::function<double(double)>(square),
    {"square_fn", "result", "x"});

  NodeObject::register_function(
    std::function<double(double, double)>(add),
    {"add_fn", "result", "a", "b"});

  NodeObject::register_function(
    std::function<int(int)>(increment),
    {"increment_fn", "result", "x"});

  // Verify three separate std::function types are registered
  using FunctionType1 = std::function<double(double)>;
  using FunctionType2 = std::function<double(double, double)>;
  using FunctionType3 = std::function<int(int)>;

  std::string hash1 = detail::hash<FunctionType1>();
  std::string hash2 = detail::hash<FunctionType2>();
  std::string hash3 = detail::hash<FunctionType3>();

  auto registry = NodeObject::get_registry();

  bool found1 = false, found2 = false, found3 = false;
  for (const auto &item : registry)
    {
      if (item.contains("type"))
        {
          std::string type_str = item.at("type");
          if (type_str == hash1) found1 = true;
          if (type_str == hash2) found2 = true;
          if (type_str == hash3) found3 = true;
        }
    }

  ASSERT_TRUE(found1) << "std::function<double(double)> not found";
  ASSERT_TRUE(found2) << "std::function<double(double, double)> not found";
  ASSERT_TRUE(found3) << "std::function<int(int)> not found";
}


// Test 2.4: Void Function Auto-Registration
TEST(AutoRegistration, VoidFunction)
{
  // USE CASE: Void functions also get auto-registered

  // Register basic types needed for this test
  NodeObject::register_elementary_type<double>();

  // Register a void function
  auto print_value = [](double x) {
    // In a real test, this would do something observable
    (void)x; // Suppress unused parameter warning
  };

  NodeObject::register_function(
    std::function<void(double)>(print_value),
    {"print_value", "x"});

  // Verify std::function<void(double)> is auto-registered
  using FunctionType = std::function<void(double)>;
  std::string expected_hash = detail::hash<FunctionType>();

  auto registry = NodeObject::get_registry();
  bool found = false;
  for (const auto &item : registry)
    {
      if (item.contains("type") && item.at("type") == expected_hash)
        {
          found = true;
          ASSERT_EQ(item.at("node_type"), "empty_constructor");
          break;
        }
    }

  ASSERT_TRUE(found) << "std::function<void(double)> not auto-registered";
}


// ============================================================================
// Phase 3: Simple Function-to-std::function Constructor
// ============================================================================

// Test 3.1: Convert Lambda to std::function
TEST(MakeFunction, LambdaToStdFunction)
{
  // USE CASE: Basic function wrapping (C++ level)

  // Define lambda
  auto sq = [](double x) { return x * x; };

  // Wrap in std::function - this should work with std::function's constructor
  std::function<double(double)> fn = sq;

  // Verify we can call it
  double result = fn(5.0);
  ASSERT_DOUBLE_EQ(result, 25.0);
}


// Test 3.2: Convert Function Pointer to std::function
TEST(MakeFunction, FunctionPointerToStdFunction)
{
  // USE CASE: Function pointer wrapping

  // Define free function
  auto square_func = [](double x) { return x * x; };
  auto square_ptr = +square_func;  // Convert lambda to function pointer

  // Wrap in std::function
  std::function<double(double)> fn = square_ptr;

  // Verify we can call it
  double result = fn(5.0);
  ASSERT_DOUBLE_EQ(result, 25.0);
}


// Test 3.3: Store std::function in Node
TEST(MakeFunction, StoreInNode)
{
  // USE CASE: Function constructor as a node

  // Register basic types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double>();  // Register std::function<double(double)>

  // Create square lambda
  auto square = [](double x) { return x * x; };

  // Register a constructor function that wraps callables in std::function
  // For now, we'll create the std::function directly and store it
  using FunctionType = std::function<double(double)>;

  // Create node containing the std::function
  FunctionType fn = square;
  NodeObjectPtr node = make_node(fn);

  // Execute node (should already be ready since we passed a value)
  // Verify node's output contains std::function<double(double)>
  ASSERT_TRUE(node->ready());

  FunctionType &retrieved = node->get<FunctionType>();
  double result = retrieved(5.0);
  ASSERT_DOUBLE_EQ(result, 25.0);
}


// Test 3.4: Pass Constructed Function to Another Node
TEST(MakeFunction, PassToConsumer)
{
  // USE CASE: End-to-end function passing workflow

  // Register basic types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double>();  // Register std::function<double(double)>

  using FunctionType = std::function<double(double)>;

  // Create node A: contains std::function (square)
  auto square = [](double x) { return x * x; };
  FunctionType fn = square;
  NodeObjectPtr node_a = make_node(fn);

  // Create an evaluator that takes std::function and a value
  auto evaluator = [](const FunctionType &func, const double &value, double &result) {
    result = func(value);
  };

  // Register the evaluator
  NodeObject::register_function(
    std::function<void(const FunctionType &, const double &, double &)>(evaluator),
    {"evaluator_fn", "function", "value", "result"});

  // Create node B (evaluator)
  NodeObjectPtr node_b = make_node("evaluator_fn");

  // Create input value node
  NodeObjectPtr value_node = make_node(5.0);

  // Create output node
  NodeObjectPtr output_node = make_node(0.0);

  // Connect A → B (function input)
  node_b->bind_input(0, node_a->get_output(0));

  // Connect value → B
  node_b->bind_input(1, value_node->get_output(0));

  // Connect output → B
  node_b->bind_input(2, output_node->get_output(0));

  // Execute the network
  (*node_b)();

  // Verify output is 25.0
  double result = output_node->get<double>();
  ASSERT_DOUBLE_EQ(result, 25.0);
}
