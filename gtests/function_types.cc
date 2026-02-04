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


// ============================================================================
// Phase 4: Function Constructor for All Node Types
// ============================================================================

// -----------------------------------------------------------------------------
// Phase 4.1: Multi-argument and Void Functions
// -----------------------------------------------------------------------------

TEST(VoidFunction, Registration)
{
  // USE CASE: Register a void function and verify its std::function type is
  // auto-registered

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register a void function
  auto print_value = [](double x) {
    // In a real scenario, this would print or have side effects
    // For testing, we'll just verify it can be registered
    (void)x;
  };
  NodeObject::register_function(print_value,
                                 {"print_value_fn", "value"});

  // Verify the std::function<void(double)> type is auto-registered
  using VoidFunctionType = std::function<void(double)>;
  const std::string void_fn_type_hash = detail::hash<VoidFunctionType>();

  auto registry = NodeObject::get_registry();
  bool found = false;
  for (const auto &item : registry)
    {
      if (item.contains("type") && item.at("type") == void_fn_type_hash)
        {
          found = true;
          // Verify the type has correct metadata
          ASSERT_TRUE(item.contains("node_type"));
          EXPECT_EQ(item.at("node_type"), "empty_constructor");
          break;
        }
    }

  ASSERT_TRUE(found) << "std::function<void(double)> not auto-registered";
}

TEST(VoidFunction, StoreAndInvoke)
{
  // USE CASE: Create a void function, store in node, retrieve and invoke

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<void, double>();

  // Create a void function with observable side effect
  double side_effect_value = 0.0;
  auto set_value = [&side_effect_value](double x) { side_effect_value = x; };

  // Wrap in std::function and store in node
  std::function<void(double)> void_fn = set_value;
  NodeObjectPtr node = make_node(void_fn);

  ASSERT_TRUE(node->ready());

  // Retrieve and invoke
  auto retrieved_fn = node->get<std::function<void(double)>>();
  retrieved_fn(42.0);

  // Verify side effect
  EXPECT_DOUBLE_EQ(side_effect_value, 42.0);
}

TEST(MultiArgFunction, RegistrationAndInvoke)
{
  // USE CASE: Register and use a multi-argument function (3 arguments)

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register a 3-argument function
  auto sum3 = [](double a, double b, double c) { return a + b + c; };
  NodeObject::register_function(sum3, {"sum3", "result", "a", "b", "c"});

  // Verify std::function<double(double, double, double)> is auto-registered
  using MultiArgFunctionType = std::function<double(double, double, double)>;
  const std::string multi_arg_type_hash =
    detail::hash<MultiArgFunctionType>();

  auto registry = NodeObject::get_registry();
  bool found = false;
  for (const auto &item : registry)
    {
      if (item.contains("type") && item.at("type") == multi_arg_type_hash)
        {
          found = true;
          break;
        }
    }

  ASSERT_TRUE(found) << "std::function<double(double, double, double)> not auto-registered";

  // Create std::function and test invocation
  std::function<double(double, double, double)> fn = sum3;
  double result = fn(1.0, 2.0, 3.0);

  EXPECT_DOUBLE_EQ(result, 6.0);
}

TEST(MultiArgFunction, StoreInNode)
{
  // USE CASE: Store multi-argument function in node and use in network

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double, double, double>();

  // Create a multi-argument function (binary operation + third parameter)
  auto add3 = [](double a, double b, double c) { return a + b + c; };
  std::function<double(double, double, double)> fn = add3;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Create an evaluator that takes the function and arguments
  auto evaluator = [](const std::function<double(double, double, double)> &f,
                      const double &                                      x,
                      const double &                                      y,
                      const double &                                      z,
                      double &result) { result = f(x, y, z); };

  NodeObject::register_function(evaluator,
                                 {"evaluator3", "result", "fn", "x", "y", "z"});

  // Create network
  NodeObjectPtr eval_node = make_node("evaluator3");
  NodeObjectPtr x_node    = make_node(1.0);
  NodeObjectPtr y_node    = make_node(2.0);
  NodeObjectPtr z_node    = make_node(3.0);
  NodeObjectPtr output_node = make_node(0.0);

  // Connect function and arguments
  eval_node->bind_input(0, fn_node->get_output(0));     // fn
  eval_node->bind_input(1, x_node->get_output(0));      // x
  eval_node->bind_input(2, y_node->get_output(0));      // y
  eval_node->bind_input(3, z_node->get_output(0));      // z
  eval_node->bind_input(4, output_node->get_output(0)); // result (output)

  // Execute
  (*eval_node)();

  // Verify result
  ASSERT_TRUE(output_node->ready());

  double result = output_node->get<double>();
  EXPECT_DOUBLE_EQ(result, 6.0);
}


// -----------------------------------------------------------------------------
// Phase 4.2: Non-const Methods
// -----------------------------------------------------------------------------

// Test class for method binding
class Counter
{
private:
  int count;

public:
  Counter() : count(0) {}
  explicit Counter(int initial) : count(initial) {}

  void increment() { count++; }
  void add(int value) { count += value; }
  int get_count() const { return count; }
};

TEST(NonConstMethod, BindAndInvoke)
{
  // USE CASE: Wrap a method + object into std::function and invoke it

  // Note: We don't need to register Counter as a type since we're not
  // storing Counter objects in nodes - only std::function objects that
  // capture Counter instances

  // Create a Counter instance
  Counter counter(10);

  // Create a std::function that binds the instance to the increment method
  // We capture the counter by reference and wrap the method call
  auto bound_increment = [&counter]() { counter.increment(); };
  std::function<void()> fn = bound_increment;

  // Verify initial state
  EXPECT_EQ(counter.get_count(), 10);

  // Invoke the function multiple times
  fn();
  EXPECT_EQ(counter.get_count(), 11);

  fn();
  EXPECT_EQ(counter.get_count(), 12);

  fn();
  EXPECT_EQ(counter.get_count(), 13);
}

TEST(NonConstMethod, StoreInNode)
{
  // USE CASE: Store a bound method (as std::function) in a node and invoke it

  // Register function type
  NodeObject::register_function_type<void>(); // std::function<void()>

  // Create a Counter instance
  auto counter = std::make_shared<Counter>(5);

  // Create a std::function that binds the counter to the increment method
  // Use shared_ptr to ensure the counter outlives the function
  auto bound_increment = [counter]() mutable { counter->increment(); };
  std::function<void()> fn = bound_increment;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Verify initial state
  EXPECT_EQ(counter->get_count(), 5);

  // Retrieve and invoke the function
  auto retrieved_fn = fn_node->get<std::function<void()>>();
  retrieved_fn();
  EXPECT_EQ(counter->get_count(), 6);

  retrieved_fn();
  EXPECT_EQ(counter->get_count(), 7);
}

TEST(NonConstMethod, WithArguments)
{
  // USE CASE: Bind a method that takes arguments

  // Register function type
  NodeObject::register_function_type<void, int>(); // std::function<void(int)>

  // Create a Counter instance
  auto counter = std::make_shared<Counter>(0);

  // Create a std::function that binds the counter to the add method
  auto bound_add = [counter](int value) mutable { counter->add(value); };
  std::function<void(int)> fn = bound_add;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Verify initial state
  EXPECT_EQ(counter->get_count(), 0);

  // Retrieve and invoke with different arguments
  auto retrieved_fn = fn_node->get<std::function<void(int)>>();
  retrieved_fn(5);
  EXPECT_EQ(counter->get_count(), 5);

  retrieved_fn(10);
  EXPECT_EQ(counter->get_count(), 15);

  retrieved_fn(3);
  EXPECT_EQ(counter->get_count(), 18);
}


// -----------------------------------------------------------------------------
// Phase 4.3: Const Methods
// -----------------------------------------------------------------------------

// Test class for const method binding
class Calculator
{
private:
  int value;

public:
  Calculator() : value(0) {}
  explicit Calculator(int initial) : value(initial) {}

  // Const methods (read-only)
  int get_value() const { return value; }
  int double_value() const { return value * 2; }
  int multiply(int factor) const { return value * factor; }

  // Non-const method for setup
  void set_value(int new_value) { value = new_value; }
};

TEST(ConstMethod, BindAndInvoke)
{
  // USE CASE: Wrap a const method + object into std::function and invoke it

  // Create a Calculator instance
  Calculator calc(42);

  // Create a std::function that binds the instance to the const method
  auto bound_get = [&calc]() { return calc.get_value(); };
  std::function<int()> fn = bound_get;

  // Invoke the function
  int result = fn();
  EXPECT_EQ(result, 42);

  // Verify the object wasn't modified (const method)
  EXPECT_EQ(calc.get_value(), 42);
}

TEST(ConstMethod, StoreInNode)
{
  // USE CASE: Store a bound const method (as std::function) in a node

  // Register function type
  NodeObject::register_function_type<int>(); // std::function<int()>

  // Create a Calculator instance
  auto calc = std::make_shared<Calculator>(100);

  // Create a std::function that binds the calculator to the const method
  // No need for 'mutable' since we're calling a const method
  auto bound_get = [calc]() { return calc->get_value(); };
  std::function<int()> fn = bound_get;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Retrieve and invoke the function
  auto retrieved_fn = fn_node->get<std::function<int()>>();
  int result = retrieved_fn();
  EXPECT_EQ(result, 100);

  // Verify multiple invocations work
  EXPECT_EQ(retrieved_fn(), 100);
  EXPECT_EQ(retrieved_fn(), 100);
}

TEST(ConstMethod, WithArguments)
{
  // USE CASE: Bind a const method that takes arguments

  // Register function type
  NodeObject::register_function_type<int, int>(); // std::function<int(int)>

  // Create a Calculator instance
  auto calc = std::make_shared<Calculator>(7);

  // Create a std::function that binds the calculator to the multiply method
  auto bound_multiply = [calc](int factor) { return calc->multiply(factor); };
  std::function<int(int)> fn = bound_multiply;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Retrieve and invoke with different arguments
  auto retrieved_fn = fn_node->get<std::function<int(int)>>();

  EXPECT_EQ(retrieved_fn(1), 7);   // 7 * 1 = 7
  EXPECT_EQ(retrieved_fn(2), 14);  // 7 * 2 = 14
  EXPECT_EQ(retrieved_fn(10), 70); // 7 * 10 = 70

  // Verify the original object wasn't modified
  EXPECT_EQ(calc->get_value(), 7);
}

TEST(ConstMethod, ConstCorrectness)
{
  // USE CASE: Verify const methods don't modify object state

  // Register function type
  NodeObject::register_function_type<int>(); // std::function<int()>

  // Create a Calculator instance
  auto calc = std::make_shared<Calculator>(50);

  // Bind the double_value const method
  auto bound_double = [calc]() { return calc->double_value(); };
  std::function<int()> fn = bound_double;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Retrieve and invoke multiple times
  auto retrieved_fn = fn_node->get<std::function<int()>>();

  // Each call should return the same value (const method doesn't modify state)
  EXPECT_EQ(retrieved_fn(), 100);
  EXPECT_EQ(retrieved_fn(), 100);
  EXPECT_EQ(retrieved_fn(), 100);

  // Verify the original value is unchanged
  EXPECT_EQ(calc->get_value(), 50);
}


// -----------------------------------------------------------------------------
// Phase 4.4: Networks as Functions
// -----------------------------------------------------------------------------

TEST(NetworkAsFunction, SimpleAddition)
{
  // USE CASE: Wrap a simple network (add two numbers) as std::function

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double, double>();

  // Register an add function
  auto add_func = [](const double &a, const double &b, double &result) {
    result = a + b;
  };
  NodeObject::register_function(add_func, {"add", "result", "a", "b"});

  // Create network: output = input0 + input1
  // Input nodes (will be updated by the function)
  NodeObjectPtr input0 = make_node(0.0);
  NodeObjectPtr input1 = make_node(0.0);

  // Output node
  NodeObjectPtr output = make_node(0.0);

  // Add node
  NodeObjectPtr add_node = make_node("add");
  add_node->bind_input(0, input0->get_output(0)); // a
  add_node->bind_input(1, input1->get_output(0)); // b
  add_node->bind_input(2, output->get_output(0)); // result

  // Create std::function that wraps the network
  auto network_fn = [input0, input1, output, add_node](double a, double b) {
    // Update inputs
    input0->get<double>() = a;
    input1->get<double>() = b;

    // Execute the network
    (*add_node)();

    // Return the output
    return output->get<double>();
  };

  std::function<double(double, double)> fn = network_fn;

  // Test the function
  EXPECT_DOUBLE_EQ(fn(3.0, 4.0), 7.0);
  EXPECT_DOUBLE_EQ(fn(10.0, 20.0), 30.0);
  EXPECT_DOUBLE_EQ(fn(1.5, 2.5), 4.0);
}

TEST(NetworkAsFunction, ComplexComputation)
{
  // USE CASE: Wrap a multi-node network as std::function
  // Network computes: result = (input0 * 2) + input1

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double, double>();

  // Register multiply function
  auto multiply_func = [](const double &a, const double &b, double &result) {
    result = a * b;
  };
  NodeObject::register_function(multiply_func,
                                 {"multiply", "result", "a", "b"});

  // Register add function
  auto add_func = [](const double &a, const double &b, double &result) {
    result = a + b;
  };
  NodeObject::register_function(add_func, {"add_fn", "result", "a", "b"});

  // Create network nodes
  NodeObjectPtr input0 = make_node(0.0);     // First input
  NodeObjectPtr input1 = make_node(0.0);     // Second input
  NodeObjectPtr constant2 = make_node(2.0);  // Constant for multiplication
  NodeObjectPtr temp = make_node(0.0);       // Intermediate result
  NodeObjectPtr output = make_node(0.0);     // Final output

  // Multiply node: temp = input0 * 2
  NodeObjectPtr mult_node = make_node("multiply");
  mult_node->bind_input(0, input0->get_output(0));     // a = input0
  mult_node->bind_input(1, constant2->get_output(0));  // b = 2
  mult_node->bind_input(2, temp->get_output(0));       // result = temp

  // Add node: output = temp + input1
  NodeObjectPtr add_node = make_node("add_fn");
  add_node->bind_input(0, temp->get_output(0));     // a = temp
  add_node->bind_input(1, input1->get_output(0));   // b = input1
  add_node->bind_input(2, output->get_output(0));   // result = output

  // Create std::function that wraps the network
  auto network_fn = [input0, input1, output, mult_node, add_node](double a,
                                                                    double b) {
    // Update inputs
    input0->get<double>() = a;
    input1->get<double>() = b;

    // Execute the network in order
    (*mult_node)(); // First compute temp = input0 * 2
    (*add_node)();  // Then compute output = temp + input1

    // Return the output
    return output->get<double>();
  };

  std::function<double(double, double)> fn = network_fn;

  // Test: result = (a * 2) + b
  EXPECT_DOUBLE_EQ(fn(3.0, 4.0), 10.0);   // (3 * 2) + 4 = 10
  EXPECT_DOUBLE_EQ(fn(5.0, 10.0), 20.0);  // (5 * 2) + 10 = 20
  EXPECT_DOUBLE_EQ(fn(1.0, 1.0), 3.0);    // (1 * 2) + 1 = 3
}

TEST(NetworkAsFunction, StoreInNode)
{
  // USE CASE: Store a network-as-function in a node and use it

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double, double>();

  // Register subtract function
  auto subtract_func = [](const double &a, const double &b, double &result) {
    result = a - b;
  };
  NodeObject::register_function(subtract_func,
                                 {"subtract", "result", "a", "b"});

  // Create network: output = input0 - input1
  NodeObjectPtr input0 = make_node(0.0);
  NodeObjectPtr input1 = make_node(0.0);
  NodeObjectPtr output = make_node(0.0);

  NodeObjectPtr sub_node = make_node("subtract");
  sub_node->bind_input(0, input0->get_output(0));
  sub_node->bind_input(1, input1->get_output(0));
  sub_node->bind_input(2, output->get_output(0));

  // Wrap network as std::function
  auto network_fn = [input0, input1, output, sub_node](double a, double b) {
    input0->get<double>() = a;
    input1->get<double>() = b;
    (*sub_node)();
    return output->get<double>();
  };

  std::function<double(double, double)> fn = network_fn;

  // Store in node
  NodeObjectPtr fn_node = make_node(fn);
  ASSERT_TRUE(fn_node->ready());

  // Retrieve and use the function
  auto retrieved_fn = fn_node->get<std::function<double(double, double)>>();

  EXPECT_DOUBLE_EQ(retrieved_fn(10.0, 3.0), 7.0);   // 10 - 3 = 7
  EXPECT_DOUBLE_EQ(retrieved_fn(100.0, 25.0), 75.0); // 100 - 25 = 75
  EXPECT_DOUBLE_EQ(retrieved_fn(5.0, 5.0), 0.0);     // 5 - 5 = 0
}


// -----------------------------------------------------------------------------
// Phase 4.5: Unified Constructor - All Function Types Together
// -----------------------------------------------------------------------------

TEST(UnifiedFunction, AllSourceTypesInterchangeable)
{
  // USE CASE: Demonstrate that functions from different sources (free functions,
  // methods, networks) all work as std::function values and can be stored,
  // retrieved, and used interchangeably

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double, double>();

  // Register a multiply function for the network
  auto multiply_func = [](const double &a, const double &b, double &result) {
    result = a * b;
  };
  NodeObject::register_function(multiply_func,
                                 {"mult_fn", "result", "a", "b"});

  // =========================================================================
  // Source 1: Free function (lambda)
  // =========================================================================
  auto free_fn_lambda = [](double a, double b) { return a + b; };
  std::function<double(double, double)> fn_from_free = free_fn_lambda;

  // =========================================================================
  // Source 2: Method (bound to object)
  // =========================================================================
  auto calc = std::make_shared<Calculator>(10);
  auto method_lambda = [calc](double a, double b) {
    return calc->multiply(static_cast<int>(a)) + b;
  };
  std::function<double(double, double)> fn_from_method = method_lambda;

  // =========================================================================
  // Source 3: Network
  // =========================================================================
  NodeObjectPtr net_in0 = make_node(0.0);
  NodeObjectPtr net_in1 = make_node(0.0);
  NodeObjectPtr net_out = make_node(0.0);
  NodeObjectPtr mult_node = make_node("mult_fn");
  mult_node->bind_input(0, net_in0->get_output(0));
  mult_node->bind_input(1, net_in1->get_output(0));
  mult_node->bind_input(2, net_out->get_output(0));

  auto network_lambda = [net_in0, net_in1, net_out, mult_node](double a,
                                                                 double b) {
    net_in0->get<double>() = a;
    net_in1->get<double>() = b;
    (*mult_node)();
    return net_out->get<double>();
  };
  std::function<double(double, double)> fn_from_network = network_lambda;

  // =========================================================================
  // All three are now std::function<double(double, double)> values
  // They can be stored in nodes, passed around, and used identically
  // =========================================================================

  // Store all three in nodes
  NodeObjectPtr node1 = make_node(fn_from_free);
  NodeObjectPtr node2 = make_node(fn_from_method);
  NodeObjectPtr node3 = make_node(fn_from_network);

  ASSERT_TRUE(node1->ready());
  ASSERT_TRUE(node2->ready());
  ASSERT_TRUE(node3->ready());

  // Retrieve and verify they all work
  auto retrieved1 = node1->get<std::function<double(double, double)>>();
  auto retrieved2 = node2->get<std::function<double(double, double)>>();
  auto retrieved3 = node3->get<std::function<double(double, double)>>();

  // Test Source 1: Free function (addition)
  EXPECT_DOUBLE_EQ(retrieved1(3.0, 4.0), 7.0); // 3 + 4 = 7

  // Test Source 2: Method (calc.multiply(a) + b, where calc.value = 10)
  EXPECT_DOUBLE_EQ(retrieved2(2.0, 5.0), 25.0); // (10 * 2) + 5 = 25

  // Test Source 3: Network (multiplication)
  EXPECT_DOUBLE_EQ(retrieved3(6.0, 7.0), 42.0); // 6 * 7 = 42
}

TEST(UnifiedFunction, FunctionComposition)
{
  // USE CASE: Use functions from different sources together in composition

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double>();
  NodeObject::register_function_type<double, double, double>();

  // Create three functions from different sources, all with compatible types

  // Function 1: Free function - square a number
  auto square = [](double x) { return x * x; };
  std::function<double(double)> fn_square = square;

  // Function 2: Method - double a number (using Calculator)
  auto calc = std::make_shared<Calculator>(1);
  auto doubler = [calc](double x) { return calc->multiply(2) * x; };
  std::function<double(double)> fn_double = doubler;

  // Function 3: Network - add 10 to a number
  NodeObject::register_elementary_type<double>();
  auto add_func = [](const double &a, const double &b, double &result) {
    result = a + b;
  };
  NodeObject::register_function(add_func, {"add_10", "result", "a", "b"});

  NodeObjectPtr net_in = make_node(0.0);
  NodeObjectPtr constant10 = make_node(10.0);
  NodeObjectPtr net_out = make_node(0.0);
  NodeObjectPtr add_node = make_node("add_10");
  add_node->bind_input(0, net_in->get_output(0));
  add_node->bind_input(1, constant10->get_output(0));
  add_node->bind_input(2, net_out->get_output(0));

  auto add10 = [net_in, net_out, add_node](double x) {
    net_in->get<double>() = x;
    (*add_node)();
    return net_out->get<double>();
  };
  std::function<double(double)> fn_add10 = add10;

  // Compose them: result = add10(double(square(x)))
  auto composed = [fn_square, fn_double, fn_add10](double x) {
    double step1 = fn_square(x);  // Square
    double step2 = fn_double(step1); // Double
    double step3 = fn_add10(step2);  // Add 10
    return step3;
  };

  // Test composition: (5^2) * 2 + 10 = 25 * 2 + 10 = 60
  EXPECT_DOUBLE_EQ(composed(5.0), 60.0);

  // Test with different input: (3^2) * 2 + 10 = 9 * 2 + 10 = 28
  EXPECT_DOUBLE_EQ(composed(3.0), 28.0);
}

TEST(UnifiedFunction, PolymorphicStorage)
{
  // USE CASE: Store different function sources in a vector and use them uniformly

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_function_type<double, double>();

  // Create a vector of std::function from different sources
  std::vector<std::function<double(double)>> functions;

  // Add free function
  functions.push_back([](double x) { return x + 1; });

  // Add method-based function
  auto calc = std::make_shared<Calculator>(5);
  functions.push_back([calc](double x) { return calc->multiply(static_cast<int>(x)); });

  // Add network-based function
  auto negate_func = [](const double &a, double &result) { result = -a; };
  NodeObject::register_function(negate_func, {"negate", "result", "a"});

  NodeObjectPtr net_in = make_node(0.0);
  NodeObjectPtr net_out = make_node(0.0);
  NodeObjectPtr neg_node = make_node("negate");
  neg_node->bind_input(0, net_in->get_output(0));
  neg_node->bind_input(1, net_out->get_output(0));

  functions.push_back([net_in, net_out, neg_node](double x) {
    net_in->get<double>() = x;
    (*neg_node)();
    return net_out->get<double>();
  });

  // Use all functions uniformly through the same interface
  double input = 10.0;

  EXPECT_DOUBLE_EQ(functions[0](input), 11.0);  // 10 + 1 = 11 (free function)
  EXPECT_DOUBLE_EQ(functions[1](input), 50.0);  // 5 * 10 = 50 (method)
  EXPECT_DOUBLE_EQ(functions[2](input), -10.0); // -10 (network)

  // All functions are the same type and can be stored/used interchangeably
  EXPECT_EQ(functions.size(), 3u);
}
