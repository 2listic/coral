#include <gtest/gtest.h>

#include <set>
#include <string>

#include "coral.h"

using namespace coral;

// The type registry is global to the test executable: every test uses its own
// types, so that registrations do not leak from one test to another.

namespace
{
  // Hashes listed under `key` ("bases" or "derived") in the registry entry of
  // T; a multiset, so that duplicates are detected.
  template <typename T>
  std::multiset<std::string>
  listed(const std::string &key)
  {
    const auto entry = NodeObject::get_registry().at(detail::hash<T>());
    if (!entry.contains(key))
      return {};
    return entry.at(key).template get<std::multiset<std::string>>();
  }

  template <typename... Ts>
  std::multiset<std::string>
  hashes()
  {
    return {detail::hash<Ts>()...};
  }
} // namespace

namespace single_base
{
  struct B
  {
    int b = 1;
  };
  struct A : B
  {
    int a = 2;
  };
} // namespace single_base

TEST(Inheritance, SingleBase)
{
  using namespace single_base;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_EQ(obj->get<B>().b, 1);

  EXPECT_EQ(listed<A>("bases"), hashes<B>());
  EXPECT_EQ(listed<B>("derived"), hashes<A>());
  EXPECT_FALSE(
    NodeObject::get_registry().at(detail::hash<A>()).contains("base"));
}

namespace multiple_bases
{
  struct B1
  {
    int b1 = 1;
  };
  struct B2
  {
    int b2 = 2;
  };
  struct A : B1, B2
  {
    int a = 3;
  };
} // namespace multiple_bases

TEST(Inheritance, MultipleBases)
{
  using namespace multiple_bases;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B1>();
  NodeObject::register_base<A, B2>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_EQ(obj->get<B1>().b1, 1);
  EXPECT_EQ(obj->get<B2>().b2, 2);
  // B2 is not at offset zero in A: the pointer must be adjusted.
  EXPECT_EQ(&obj->get<B2>(), static_cast<B2 *>(&obj->get<A>()));

  EXPECT_EQ(listed<A>("bases"), (hashes<B1, B2>()));
}

namespace chain_base_first
{
  struct C
  {
    int c = 1;
  };
  struct B : C
  {
    int b = 2;
  };
  struct A : B
  {
    int a = 3;
  };
} // namespace chain_base_first

TEST(Inheritance, ChainBaseFirst)
{
  using namespace chain_base_first;
  NodeObject::register_type<A>();
  NodeObject::register_base<B, C>();
  NodeObject::register_base<A, B>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_EQ(obj->get<C>().c, 1);

  EXPECT_EQ(listed<A>("bases"), (hashes<B, C>()));
  EXPECT_EQ(listed<C>("derived"), (hashes<A, B>()));
}

namespace chain_derived_first
{
  struct C
  {
    int c = 1;
  };
  struct B : C
  {
    int b = 2;
  };
  struct A : B
  {
    int a = 3;
  };
} // namespace chain_derived_first

TEST(Inheritance, ChainDerivedFirst)
{
  using namespace chain_derived_first;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B>();
  NodeObject::register_base<B, C>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_EQ(obj->get<C>().c, 1);

  EXPECT_EQ(listed<A>("bases"), (hashes<B, C>()));
  EXPECT_EQ(listed<C>("derived"), (hashes<A, B>()));
}

namespace mixed_graph
{
  struct E
  {
    int e = 1;
  };
  struct C : E
  {
    int c = 2;
  };
  struct B1
  {
    int b1 = 3;
  };
  struct B2 : C
  {
    int b2 = 4;
  };
  struct A : B1, B2
  {
    int a = 5;
  };
} // namespace mixed_graph

TEST(Inheritance, MixedGraph)
{
  using namespace mixed_graph;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B1>();
  NodeObject::register_base<A, B2>();
  NodeObject::register_base<C, E>();
  // Propagates C and E to B2 and to its descendant A.
  NodeObject::register_base<B2, C>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_EQ(obj->get<E>().e, 1);
  EXPECT_EQ(obj->get<C>().c, 2);
  EXPECT_EQ(&obj->get<E>(), static_cast<E *>(&obj->get<A>()));

  EXPECT_EQ(listed<A>("bases"), (hashes<B1, B2, C, E>()));
  EXPECT_EQ(listed<E>("derived"), (hashes<A, B2, C>()));
}

namespace siblings
{
  struct B
  {
    int b = 1;
  };
  struct T1 : B
  {};
  struct T2 : B
  {};
} // namespace siblings

TEST(Inheritance, Siblings)
{
  using namespace siblings;
  NodeObject::register_type<T1>();
  NodeObject::register_type<T2>();
  NodeObject::register_base<T1, B>();
  NodeObject::register_base<T2, B>();

  NodeObjectPtr obj1 = make_node<T1>();
  NodeObjectPtr obj2 = make_node<T2>();
  (*obj1)();
  (*obj2)();
  EXPECT_EQ(obj1->get<B>().b, 1);
  EXPECT_EQ(obj2->get<B>().b, 1);

  EXPECT_EQ(listed<B>("derived"), (hashes<T1, T2>()));
}

namespace survives_register_type
{
  struct B
  {
    int b = 1;
  };
  struct A : B
  {};
} // namespace survives_register_type

TEST(Inheritance, SurvivesRegisterType)
{
  using namespace survives_register_type;
  NodeObject::register_base<A, B>();
  NodeObject::register_type<A>();
  NodeObject::register_type<B>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_EQ(obj->get<B>().b, 1);

  EXPECT_EQ(listed<A>("bases"), hashes<B>());
  EXPECT_EQ(listed<B>("derived"), hashes<A>());
}

namespace creates_missing_as_abstract
{
  struct B
  {};
  struct A : B
  {};
} // namespace creates_missing_as_abstract

TEST(Inheritance, CreatesMissingAsAbstract)
{
  using namespace creates_missing_as_abstract;
  NodeObject::register_base<A, B>();

  const auto registry = NodeObject::get_registry();
  ASSERT_TRUE(registry.contains(detail::hash<A>()));
  ASSERT_TRUE(registry.contains(detail::hash<B>()));
  EXPECT_EQ(registry.at(detail::hash<A>()).at("node_type"), "abstract");
  EXPECT_EQ(registry.at(detail::hash<B>()).at("node_type"), "abstract");
}

namespace idempotent
{
  struct B
  {};
  struct A : B
  {};
} // namespace idempotent

TEST(Inheritance, Idempotent)
{
  using namespace idempotent;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B>();
  NodeObject::register_base<A, B>();

  EXPECT_EQ(listed<A>("bases"), hashes<B>());
  EXPECT_EQ(listed<B>("derived"), hashes<A>());
}

namespace virtual_diamond
{
  struct C
  {
    int c = 1;
  };
  struct B1 : virtual C
  {
    int b1 = 2;
  };
  struct B2 : virtual C
  {
    int b2 = 3;
  };
  struct D : B1, B2
  {
    int d = 4;
  };
} // namespace virtual_diamond

TEST(Inheritance, VirtualDiamond)
{
  using namespace virtual_diamond;
  NodeObject::register_type<D>();
  NodeObject::register_base<B1, C>();
  NodeObject::register_base<B2, C>();
  NodeObject::register_base<D, B1>();
  NodeObject::register_base<D, B2>();

  NodeObjectPtr obj = make_node<D>();
  (*obj)();
  EXPECT_EQ(obj->get<C>().c, 1);
  EXPECT_EQ(&obj->get<C>(), static_cast<C *>(&obj->get<D>()));

  EXPECT_EQ(listed<D>("bases"), (hashes<B1, B2, C>()));
}

namespace unrelated_throws
{
  struct B
  {};
  struct A : B
  {};
  struct Unrelated
  {};
} // namespace unrelated_throws

TEST(Inheritance, UnrelatedThrows)
{
  using namespace unrelated_throws;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B>();

  NodeObjectPtr obj = make_node<A>();
  (*obj)();
  EXPECT_THROW(obj->get<Unrelated>(), std::runtime_error);
}

namespace function_takes_base
{
  struct C
  {
    int c = 1;
  };
  struct B : C
  {
    int b = 2;
  };
  struct A : B
  {
    int a = 3;
  };
} // namespace function_takes_base

TEST(Inheritance, FunctionTakesBase)
{
  using namespace function_takes_base;
  NodeObject::register_elementary_type<int>();
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B>();
  NodeObject::register_base<B, C>();

  auto set_c = [](C &c) { c.c = 42; };
  auto get_c = [](const C &c) { return c.c; };
  NodeObject::register_function(set_c, {"inheritance_set_c", "c"});
  NodeObject::register_function(get_c, {"inheritance_get_c", "value", "c"});

  NodeObjectPtr obj = make_node<A>();
  (*obj)();

  NodeObjectPtr setter = make_method_node("inheritance_set_c", set_c);
  setter->set_arguments({obj});
  (*setter)();
  EXPECT_EQ(obj->get<A>().c, 42);
  EXPECT_EQ(setter->get_output(0), obj);

  NodeObjectPtr getter = make_method_node("inheritance_get_c", get_c);
  NodeObjectPtr value  = make_node(0);
  getter->set_arguments({value, obj});
  (*getter)();
  EXPECT_EQ(value->get<int>(), 42);
}
