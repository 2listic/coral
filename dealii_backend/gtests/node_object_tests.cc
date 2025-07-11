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

// MethodRegistration test moved to failing.cc

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
