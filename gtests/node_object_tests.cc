#include <deal.II/base/point.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

#include <fstream>

#include "coral.h"

using namespace dealii;
using namespace coral;

TEST(NodeObject, ElementaryTypeInt)
{
  NodeObject::register_elementary_type<int>();
  NodeObjectPtr obj = make_node(42);
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(42, obj->get<int>());
  ASSERT_EQ("42", obj->to_string());
}

TEST(NodeObject, TypePoint)
{
  using type = Point<2>;
  NodeObject::register_type<type>();
  NodeObjectPtr obj = make_node(type(0.0, 1.0));
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(type(0.0, 1.0), obj->get<type>());
  // No to_string for non-elementary types
}

TEST(NodeObject, TriviallyConstructibleType)
{
  using type = Triangulation<2>;
  NodeObject::register_type<type>();
  NodeObjectPtr obj = make_node<type>();
  (*obj)();
  ASSERT_TRUE(obj->ready());
}

TEST(NodeObject, NonTriviallyConstructibleType)
{
  using type = FE_Q<2>;
  NodeObject::register_elementary_type<int>();
  NodeObject::register_type<type, int>("degree");
  NodeObjectPtr fe     = make_node<type>(1);
  NodeObjectPtr degree = make_node(1);
  fe->set_arguments({degree});
  (*fe)();
  ASSERT_TRUE(fe->ready());

  // Dump the json to a file
  std::ofstream ofs("fe_q.json");
  json          j = fe;
  ofs << j.dump(2);
  ofs.close();
}

TEST(NodeObject, AbstractType)
{
  struct Base
  {};
  struct Derived : Base
  {};
  NodeObject::register_abstract_type<Base>();
  NodeObject::register_derived_type<Base, Derived>();
  NodeObjectPtr obj = make_node<Derived>();
  (*obj)();
  ASSERT_TRUE(obj->ready());
}

// Test for void method registration
struct MyClass
{
  void
  set_value(int v)
  {
    value = v;
  }
  int
  add_value(int v)
  {
    value += v;
    return value;
  }
  int value = 1;
};

TEST(NodeObject, VoidMethodRegistration)
{
  coral::NodeObject::register_type<MyClass>();
  coral::NodeObject::register_type<int>();
  coral::NodeObject::register_method(&MyClass::set_value,
                                     {"set_value", "my_class", "value"});

  coral::NodeObjectPtr obj = coral::make_node(MyClass());
  coral::NodeObjectPtr fun =
    coral::make_method_node("set_value", &MyClass::set_value);
  coral::NodeObjectPtr arg = coral::make_node(42);

  ASSERT_EQ(obj->get<MyClass>().value, 1)
    << "The initial value of MyClass should be 1";

  // Check that arguments are set ready
  ASSERT_TRUE(arg->ready());
  ASSERT_TRUE(obj->ready());

  fun->set_arguments({obj, arg});
  (*fun)();
  ASSERT_EQ(obj->get<MyClass>().value, 42);
}

TEST(NodeObject, NonVoidMethodRegistration)
{
  coral::NodeObject::register_type<MyClass>();
  coral::NodeObject::register_type<int>();
  coral::NodeObject::register_method(
    &MyClass::add_value, {"add_value", "my_class", "output", "value"});

  coral::NodeObjectPtr obj = coral::make_node(MyClass());
  coral::NodeObjectPtr fun =
    coral::make_method_node("add_value", &MyClass::add_value);
  coral::NodeObjectPtr out = coral::make_node(0);
  coral::NodeObjectPtr arg = coral::make_node(42);

  fun->set_arguments({obj, out, arg});
  (*fun)();
  ASSERT_EQ(obj->get<MyClass>().value, 43);
  ASSERT_EQ(out->get<int>(), 43)
    << "The output node should have the value 43 after adding 42";
}

TEST(NodeObject, FunctionRegistration)
{
  auto my_function = [](int a, int b) { return a + b; };

  NodeObject::register_type<int>();
  NodeObject::register_function(my_function,
                                {{"my_function", "sum", "a", "b"}});

  std::cout << NodeObject::get_registry().dump(2) << std::endl;

  NodeObjectPtr obj = coral::make_method_node("my_function", my_function);

  // Check number of inputs and outputs
  ASSERT_EQ(obj->n_inputs(), 3);
  ASSERT_EQ(obj->n_outputs(), 1);

  // Check output type
  ASSERT_EQ(obj->type_name(), "std::function<int(int, int)>");
}

TEST(NodeObject, LambdaNoInputIntOutput)
{
  auto guide = [answer]() -> int { return 42; };

  NodeObject::register_elementary_type<int>();
  NodeObject::register_function(guide,
                                {"life_the_universe_everything", "answer"});

  NodeObjectPtr fun =
    coral::make_method_node("life_the_universe_everything", guide);
  NodeObjectPtr out = coral::make_node(0);

  ASSERT_EQ(fun->n_inputs(), 0);
  ASSERT_EQ(fun->n_outputs(), 1);

  fun->set_arguments({out});
  (*fun)();
  ASSERT_TRUE(out->ready());
  ASSERT_EQ(out->get<int>(), 42);
}
