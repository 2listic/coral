#include <gtest/gtest.h>

#include <string>

#include "coral.h"
#include "coral_network.h"

using namespace coral;
using json = nlohmann::json;

// Edge type check (#64): an output of type T feeds an input expecting E iff
// T is E or E is an ancestor of T. Numbers refer to decisions.md, D6.
//
// The type registry is global to the test executable: every test uses its own
// types and function names, so that registrations do not leak.

namespace
{
  // Register a function node `name`, with input 0 of type `const E &` and
  // output 0 returning `E::v`. Return the callable (make_method_node needs it).
  template <typename E>
  auto
  register_consumer(const std::string &name)
  {
    auto f = [](const E &e) { return e.v; };
    NodeObject::register_elementary_type<int>();
    NodeObject::register_function(f, {name, "value", "in"});
    return f;
  }

  // Add to `net` a consumer node of E (see register_consumer); return its id.
  template <typename E>
  unsigned int
  add_consumer(Network &net, const std::string &name)
  {
    return net.add_node(make_method_node(name, register_consumer<E>(name)));
  }

  // Value computed by the consumer node `id` after a run.
  int
  consumed(const Network &net, const unsigned int id)
  {
    return net.get_node(id)->get_output(0)->get<int>();
  }
} // namespace

// 1.
namespace edge_types::exact
{
  struct A
  {
    int v = 1;
  };
} // namespace edge_types::exact

TEST(EdgeTypes, ExactType)
{
  using namespace edge_types::exact;
  NodeObject::register_type<A>();

  Network    net;
  const auto src = net.add_node(make_node<A>());
  const auto dst = add_consumer<A>(net, "edge_types_exact");
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));
  EXPECT_EQ(net.n_connections(), 1);
}

// 2.
namespace edge_types::chain
{
  struct A
  {
    int v = 1;
  };
  struct B : A
  {};
  struct C : B
  {
    C() { v = 3; }
  };
} // namespace edge_types::chain

TEST(EdgeTypes, TwoLevelChainRuns)
{
  using namespace edge_types::chain;
  NodeObject::register_type<C>();
  NodeObject::register_base<B, A>();
  NodeObject::register_base<C, B>();

  Network    net;
  const auto src = net.add_node(make_node<C>());
  const auto dst = add_consumer<A>(net, "edge_types_chain");
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));

  Network::set_touch_file_base_path("edge_types_touch");
  net.run();
  EXPECT_EQ(consumed(net, dst), 3);
}

// 3.
namespace edge_types::unrelated
{
  struct A
  {
    int v = 1;
  };
  struct U
  {};
} // namespace edge_types::unrelated

TEST(EdgeTypes, UnrelatedThrows)
{
  using namespace edge_types::unrelated;
  NodeObject::register_type<A>();
  NodeObject::register_type<U>();

  Network    net;
  const auto src = net.add_node(make_node<U>());
  const auto dst = add_consumer<A>(net, "edge_types_unrelated");
  EXPECT_THROW(net.add_connection(src, dst, 0, 0), TypeMismatchException);
  // Refused before anything runs, and not stored (D8).
  EXPECT_FALSE(net.get_node(src)->ready());
  EXPECT_EQ(net.n_connections(), 0);
}

// 4.
namespace edge_types::base_to_derived
{
  struct A
  {
    int v = 1;
  };
  struct B : A
  {};
} // namespace edge_types::base_to_derived

TEST(EdgeTypes, BaseToDerivedThrows)
{
  using namespace edge_types::base_to_derived;
  NodeObject::register_type<A>();
  NodeObject::register_base<B, A>();

  Network    net;
  const auto src = net.add_node(make_node<A>());
  const auto dst = add_consumer<B>(net, "edge_types_base_to_derived");
  EXPECT_THROW(net.add_connection(src, dst, 0, 0), TypeMismatchException);
}

// 5.
namespace edge_types::json_load
{
  struct A
  {
    int v = 1;
  };
  struct U
  {};
} // namespace edge_types::json_load

TEST(EdgeTypes, JsonLoadThrowsWithEdgeContext)
{
  using namespace edge_types::json_load;
  NodeObject::register_type<A>();
  NodeObject::register_type<U>();
  const std::string name = "edge_types_json_load";
  const auto consumer_type =
    make_method_node(name, register_consumer<A>(name))->hash();

  json j;
  j["workflow"]["nodes"]["0"]["type"]         = detail::hash<U>();
  j["workflow"]["nodes"]["0"]["qualified_id"] = "src";
  j["workflow"]["nodes"]["1"]["type"]         = consumer_type;
  j["workflow"]["nodes"]["1"]["qualified_id"] = "dst";
  j["workflow"]["edges"]["7"] = {{"source", 0},
                                 {"source_output", 0},
                                 {"target", 1},
                                 {"target_input", 0}};

  Network net;
  try
    {
      net.from_json(j);
      FAIL() << "Expected TypeMismatchException";
    }
  catch (const TypeMismatchException &e)
    {
      const std::string msg = e.what();
      EXPECT_NE(msg.find("Edge 7 (src[0] -> dst[0])"), std::string::npos)
        << msg;
    }
}

// 6. and 7.
namespace edge_types::bind_inputs
{
  struct A
  {
    int v = 1;
  };
  struct B : A
  {};
  struct U
  {};
} // namespace edge_types::bind_inputs

TEST(EdgeTypes, BindInputsDerivedToBase)
{
  using namespace edge_types::bind_inputs;
  NodeObject::register_type<B>();
  NodeObject::register_base<B, A>();
  const std::string name = "edge_types_bind_inputs";

  NodeObjectPtr src = make_node<B>();
  NodeObjectPtr dst = make_method_node(name, register_consumer<A>(name));
  EXPECT_NO_THROW(dst->bind_inputs({{src, 0}}));
}

TEST(EdgeTypes, BindInputsUnrelatedThrows)
{
  using namespace edge_types::bind_inputs;
  NodeObject::register_type<U>();
  NodeObject::register_base<B, A>();
  const std::string name = "edge_types_bind_inputs";

  NodeObjectPtr src = make_node<U>();
  NodeObjectPtr dst = make_method_node(name, register_consumer<A>(name));
  EXPECT_THROW(dst->bind_inputs({{src, 0}}), TypeMismatchException);
}

// 8.
namespace edge_types::late_base
{
  struct A
  {
    int v = 1;
  };
  struct B : A
  {};
} // namespace edge_types::late_base

TEST(EdgeTypes, RegisterBaseAfterNodeCreation)
{
  using namespace edge_types::late_base;
  NodeObject::register_type<B>();
  NodeObject::register_abstract_type<A>();

  Network    net;
  const auto src = net.add_node(make_node<B>()); // entry copied here
  NodeObject::register_base<B, A>();              // too late for `src`
  const auto dst = add_consumer<A>(net, "edge_types_late_base");

  // Load check and run-time cast agree: both refuse.
  EXPECT_THROW(net.add_connection(src, dst, 0, 0), TypeMismatchException);
  (*net.get_node(src))();
  EXPECT_THROW(net.get_node(src)->get<A>(), std::runtime_error);
}

// 9.
namespace edge_types::multiple_bases
{
  struct B1
  {
    int v = 1;
  };
  struct B2
  {
    int v = 2;
  };
  struct A : B1, B2
  {};
} // namespace edge_types::multiple_bases

TEST(EdgeTypes, MultipleBases)
{
  using namespace edge_types::multiple_bases;
  NodeObject::register_type<A>();
  NodeObject::register_base<A, B1>();
  NodeObject::register_base<A, B2>();

  Network    net;
  const auto src  = net.add_node(make_node<A>());
  const auto dst1 = add_consumer<B1>(net, "edge_types_multiple_bases_1");
  const auto dst2 = add_consumer<B2>(net, "edge_types_multiple_bases_2");
  EXPECT_NO_THROW(net.add_connection(src, dst1, 0, 0));
  EXPECT_NO_THROW(net.add_connection(src, dst2, 0, 0));
}

// 10.
namespace edge_types::chain_derived_first
{
  struct A
  {
    int v = 1;
  };
  struct B : A
  {};
  struct C : B
  {};
} // namespace edge_types::chain_derived_first

TEST(EdgeTypes, ChainRegisteredDerivedFirst)
{
  using namespace edge_types::chain_derived_first;
  NodeObject::register_type<C>();
  NodeObject::register_base<C, B>();
  NodeObject::register_base<B, A>();

  Network    net;
  const auto src = net.add_node(make_node<C>());
  const auto dst = add_consumer<A>(net, "edge_types_chain_derived_first");
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));
}

// 11.
namespace edge_types::siblings
{
  struct B
  {
    int v = 1;
  };
  struct T1 : B
  {};
  struct T2 : B
  {};
} // namespace edge_types::siblings

TEST(EdgeTypes, SiblingThrows)
{
  using namespace edge_types::siblings;
  NodeObject::register_type<T1>();
  NodeObject::register_type<T2>();
  NodeObject::register_base<T1, B>();
  NodeObject::register_base<T2, B>();

  Network    net;
  const auto src = net.add_node(make_node<T1>());
  const auto dst = add_consumer<T2>(net, "edge_types_siblings");
  EXPECT_THROW(net.add_connection(src, dst, 0, 0), TypeMismatchException);
}

// 12.
namespace edge_types::virtual_diamond
{
  struct A
  {
    int v = 1;
  };
  struct B1 : virtual A
  {};
  struct B2 : virtual A
  {};
  struct D : B1, B2
  {
    D() { v = 4; }
  };
} // namespace edge_types::virtual_diamond

TEST(EdgeTypes, VirtualDiamondRuns)
{
  using namespace edge_types::virtual_diamond;
  NodeObject::register_type<D>();
  NodeObject::register_base<B1, A>();
  NodeObject::register_base<B2, A>();
  NodeObject::register_base<D, B1>();
  NodeObject::register_base<D, B2>();

  Network    net;
  const auto src = net.add_node(make_node<D>());
  const auto dst = add_consumer<A>(net, "edge_types_virtual_diamond");
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));

  Network::set_touch_file_base_path("edge_types_touch");
  net.run();
  EXPECT_EQ(consumed(net, dst), 4);
}
