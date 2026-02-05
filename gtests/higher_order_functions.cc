// Test file for Phase 6: Higher-Order Functions
// Tests for map, reduce, filter, and apply functions

#include <gtest/gtest.h>

#include "coral.h"
#include "register_types.h"

using namespace coral;

// ============================================================================
// Phase 6.1: Map Tests
// ============================================================================

TEST(Map, SquareFunction)
{
  // USE CASE: Apply square function to each element of a vector using map

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register square function
  auto square = [](double x) { return x * x; };
  NodeObject::register_function(square, {"square", "result", "x"});

  // Create function node
  NodeObjectPtr square_node = make_node("square");

  // Create input vector with type-erased values
  std::vector<std::shared_ptr<entt::meta_any>> input;
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(1.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(2.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(3.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(4.0)));

  // Apply map
  auto result = map(input, square_node);

  // Verify results
  ASSERT_EQ(result.size(), 4);

  // Extract and verify each result
  auto r0 = result[0]->try_cast<std::shared_ptr<double>>();
  auto r1 = result[1]->try_cast<std::shared_ptr<double>>();
  auto r2 = result[2]->try_cast<std::shared_ptr<double>>();
  auto r3 = result[3]->try_cast<std::shared_ptr<double>>();

  ASSERT_NE(r0, nullptr);
  ASSERT_NE(r1, nullptr);
  ASSERT_NE(r2, nullptr);
  ASSERT_NE(r3, nullptr);

  EXPECT_DOUBLE_EQ(**r0, 1.0);
  EXPECT_DOUBLE_EQ(**r1, 4.0);
  EXPECT_DOUBLE_EQ(**r2, 9.0);
  EXPECT_DOUBLE_EQ(**r3, 16.0);
}

// ============================================================================
// Phase 6.2: Reduce Tests
// ============================================================================

TEST(Reduce, SumFunction)
{
  // USE CASE: Accumulate vector elements using addition with reduce

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register add function
  auto add = [](double a, double b) { return a + b; };
  NodeObject::register_function(add, {"add", "result", "a", "b"});

  // Create function node
  NodeObjectPtr add_node = make_node("add");

  // Create input vector
  std::vector<std::shared_ptr<entt::meta_any>> input;
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(1.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(2.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(3.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(4.0)));

  // Initial value
  auto initial =
    std::make_shared<entt::meta_any>(std::make_shared<double>(0.0));

  // Apply reduce
  auto result = reduce(input, add_node, initial);

  // Verify result: 1 + 2 + 3 + 4 = 10
  auto result_ptr = result->try_cast<std::shared_ptr<double>>();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_DOUBLE_EQ(**result_ptr, 10.0);
}

TEST(Reduce, ProductFunction)
{
  // USE CASE: Calculate product of vector elements using reduce

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register multiply function
  auto multiply = [](double a, double b) { return a * b; };
  NodeObject::register_function(multiply, {"multiply", "result", "a", "b"});

  // Create function node
  NodeObjectPtr mult_node = make_node("multiply");

  // Create input vector
  std::vector<std::shared_ptr<entt::meta_any>> input;
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(1.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(2.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(3.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(4.0)));

  // Initial value
  auto initial =
    std::make_shared<entt::meta_any>(std::make_shared<double>(1.0));

  // Apply reduce
  auto result = reduce(input, mult_node, initial);

  // Verify result: 1 * 2 * 3 * 4 = 24
  auto result_ptr = result->try_cast<std::shared_ptr<double>>();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_DOUBLE_EQ(**result_ptr, 24.0);
}

// ============================================================================
// Phase 6.3: Filter Tests
// ============================================================================

TEST(Filter, EvenNumbers)
{
  // USE CASE: Filter a vector to keep only even numbers

  // Register types
  NodeObject::register_elementary_type<int>();
  NodeObject::register_elementary_type<bool>();

  // Register is_even predicate
  auto is_even = [](int x) { return x % 2 == 0; };
  NodeObject::register_function(is_even, {"is_even", "result", "x"});

  // Create predicate node
  NodeObjectPtr is_even_node = make_node("is_even");

  // Create input vector
  std::vector<std::shared_ptr<entt::meta_any>> input;
  input.push_back(std::make_shared<entt::meta_any>(std::make_shared<int>(1)));
  input.push_back(std::make_shared<entt::meta_any>(std::make_shared<int>(2)));
  input.push_back(std::make_shared<entt::meta_any>(std::make_shared<int>(3)));
  input.push_back(std::make_shared<entt::meta_any>(std::make_shared<int>(4)));
  input.push_back(std::make_shared<entt::meta_any>(std::make_shared<int>(5)));
  input.push_back(std::make_shared<entt::meta_any>(std::make_shared<int>(6)));

  // Apply filter
  auto result = filter(input, is_even_node);

  // Verify result: [2, 4, 6]
  ASSERT_EQ(result.size(), 3);

  auto r0 = result[0]->try_cast<std::shared_ptr<int>>();
  auto r1 = result[1]->try_cast<std::shared_ptr<int>>();
  auto r2 = result[2]->try_cast<std::shared_ptr<int>>();

  ASSERT_NE(r0, nullptr);
  ASSERT_NE(r1, nullptr);
  ASSERT_NE(r2, nullptr);

  EXPECT_EQ(**r0, 2);
  EXPECT_EQ(**r1, 4);
  EXPECT_EQ(**r2, 6);
}

TEST(Filter, GreaterThanThreshold)
{
  // USE CASE: Filter values greater than a threshold

  // Register types
  NodeObject::register_elementary_type<double>();
  NodeObject::register_elementary_type<bool>();

  // Register predicate: x > 3.5
  auto greater_than_threshold = [](double x) { return x > 3.5; };
  NodeObject::register_function(
    greater_than_threshold, {"greater_than_3_5", "result", "x"});

  // Create predicate node
  NodeObjectPtr pred_node = make_node("greater_than_3_5");

  // Create input vector
  std::vector<std::shared_ptr<entt::meta_any>> input;
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(1.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(5.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(3.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(7.0)));
  input.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(2.0)));

  // Apply filter
  auto result = filter(input, pred_node);

  // Verify result: [5.0, 7.0]
  ASSERT_EQ(result.size(), 2);

  auto r0 = result[0]->try_cast<std::shared_ptr<double>>();
  auto r1 = result[1]->try_cast<std::shared_ptr<double>>();

  ASSERT_NE(r0, nullptr);
  ASSERT_NE(r1, nullptr);

  EXPECT_DOUBLE_EQ(**r0, 5.0);
  EXPECT_DOUBLE_EQ(**r1, 7.0);
}

// ============================================================================
// Phase 6.4: Apply Tests
// ============================================================================

TEST(Apply, UnaryFunction)
{
  // USE CASE: Apply a unary function to a single argument

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register square function
  auto square = [](double x) { return x * x; };
  NodeObject::register_function(square, {"square", "result", "x"});

  // Create function node
  NodeObjectPtr square_node = make_node("square");

  // Create argument
  std::vector<std::shared_ptr<entt::meta_any>> args;
  args.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(5.0)));

  // Apply function
  auto result = coral::apply(square_node, args);

  // Verify result: 25.0
  auto result_ptr = result->template try_cast<std::shared_ptr<double>>();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_DOUBLE_EQ(**result_ptr, 25.0);
}

TEST(Apply, BinaryFunction)
{
  // USE CASE: Apply a binary function to two arguments

  // Register types
  NodeObject::register_elementary_type<double>();

  // Register add function
  auto add = [](double a, double b) { return a + b; };
  NodeObject::register_function(add, {"add", "result", "a", "b"});

  // Create function node
  NodeObjectPtr add_node = make_node("add");

  // Create arguments
  std::vector<std::shared_ptr<entt::meta_any>> args;
  args.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(3.0)));
  args.push_back(
    std::make_shared<entt::meta_any>(std::make_shared<double>(4.0)));

  // Apply function
  auto result = coral::apply(add_node, args);

  // Verify result: 7.0
  auto result_ptr = result->template try_cast<std::shared_ptr<double>>();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_DOUBLE_EQ(**result_ptr, 7.0);
}
