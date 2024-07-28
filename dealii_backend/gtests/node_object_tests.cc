#include <deal.II/base/point.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/tria.h>

#include <gtest/gtest.h>

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

TEST(NodeObject, ElementaryTypePoint)
{
  using type = Point<2>;
  NodeObject::register_elementary_type<type>();
  NodeObjectPtr obj = make_node(type(0.0, 1.0));
  ASSERT_TRUE(obj->ready());
  ASSERT_EQ(type(0.0, 1.0), obj->get<type>());
  ASSERT_EQ("0, 1", obj->to_string());
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
  fe->set_args({degree});
  (*fe)();
  ASSERT_TRUE(fe->ready());
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

// TEST(NodeObject, MethodRegistration)
// {
//   struct MyClass
//   {
//     void
//     set_value(int v)
//     {
//       value = v;
//     }
//     int value;
//   };

//   NodeObject::register_type<MyClass>();
//   NodeObject::register_type<int>();
//   NodeObject::register_method(&MyClass::set_value,
//                               {"set_value", "my_class", "value"});

//   NodeObjectPtr obj = make_node<MyClass>();
//   NodeObjectPtr fun = make_node(&MyClass::set_value);
//   NodeObjectPtr arg = make_node(42);
//   fun->set_args({obj, arg});
//   (*fun)();
//   ASSERT_EQ(obj->get<MyClass>().value, 42);
// }

// TEST(NodeObject, FunctionRegistration)
// {
//   std::function<int(int, int)> my_function = [](int a, int b) -> int {
//     return a + b;
//   };

//   NodeObject::register_function(my_function, {{"my_function", "a", "b"}});

//   NodeObjectPtr obj  = make_node<decltype(my_function)>();
//   NodeObjectPtr arg1 = make_node(1);
//   NodeObjectPtr arg2 = make_node(2);
//   obj->set_args({arg1, arg2});
//   (*obj)();
//   ASSERT_EQ(obj->get<int>(), 3);
// }
