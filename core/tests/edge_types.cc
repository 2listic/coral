#include <gtest/gtest.h>

#include <string>

#include "coral.h"
#include "coral_network.h"

using namespace coral;
using json = nlohmann::json;

// Edge type check (#64): an output of type T feeds an input expecting E iff
// T is E or E is an ancestor of T. Numbers refer to decisions.md, D6 of
// revision 1; "R2 <letter>" to its "Tests (R2)" list.
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
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));
  // Stored (D8); refused by run() before anything runs (D1).
  EXPECT_EQ(net.n_connections(), 1);
  EXPECT_THROW(net.run(), TypeMismatchException);
  EXPECT_FALSE(net.get_node(src)->ready());
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
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));
  EXPECT_THROW(net.validate(), TypeMismatchException);
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

// R2 a. Every bad edge is reported, in one exception.
namespace edge_types::two_bad_edges
{
  struct A
  {
    int v = 1;
  };
  struct U
  {};
} // namespace edge_types::two_bad_edges

TEST(EdgeTypes, JsonLoadReportsAllBadEdges)
{
  using namespace edge_types::two_bad_edges;
  NodeObject::register_type<A>();
  NodeObject::register_type<U>();
  const std::string name = "edge_types_two_bad_edges";
  const auto        consumer_type =
    make_method_node(name, register_consumer<A>(name))->hash();

  json j;
  j["workflow"]["nodes"]["0"]["type"]         = detail::hash<U>();
  j["workflow"]["nodes"]["0"]["qualified_id"] = "src";
  j["workflow"]["nodes"]["1"]["type"]         = consumer_type;
  j["workflow"]["nodes"]["1"]["qualified_id"] = "dst1";
  j["workflow"]["nodes"]["2"]["type"]         = consumer_type;
  j["workflow"]["nodes"]["2"]["qualified_id"] = "dst2";
  j["workflow"]["edges"]["3"]                 = {{"source", 0},
                                                 {"source_output", 0},
                                                 {"target", 1},
                                                 {"target_input", 0}};
  j["workflow"]["edges"]["4"]                 = {{"source", 0},
                                                 {"source_output", 0},
                                                 {"target", 2},
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
      EXPECT_NE(msg.find("Type mismatch in 2 edge(s):"), std::string::npos)
        << msg;
      EXPECT_NE(msg.find("Edge 3 (src[0] -> dst1[0])"), std::string::npos)
        << msg;
      EXPECT_NE(msg.find("Edge 4 (src[0] -> dst2[0])"), std::string::npos)
        << msg;
    }
  // No rollback: the network keeps every edge of the JSON (D8).
  EXPECT_EQ(net.n_connections(), 2);
}

// R2 b. A cycle is reported as such, not as a type mismatch (D10).
namespace edge_types::cycle
{
  struct A
  {
    int v = 1;
  };
} // namespace edge_types::cycle

TEST(EdgeTypes, CycleThrows)
{
  using namespace edge_types::cycle;
  NodeObject::register_type<A>();

  Network    net;
  const auto x = add_consumer<A>(net, "edge_types_cycle");
  const auto y = add_consumer<A>(net, "edge_types_cycle");
  net.add_connection(x, y, 0, 0);
  net.add_connection(y, x, 0, 0);
  try
    {
      net.validate();
      FAIL() << "Expected std::runtime_error";
    }
  catch (const TypeMismatchException &e)
    {
      FAIL() << "Expected a cycle error, got: " << e.what();
    }
  catch (const std::runtime_error &e)
    {
      const std::string msg = e.what();
      EXPECT_NE(msg.find("Cycle in network: nodes " + std::to_string(x) + ", " +
                         std::to_string(y) + "."),
                std::string::npos)
        << msg;
    }
}

// R2 c. get_input_connection: the recorded edge feeding an input (D11).
namespace edge_types::input_connection
{
  struct A
  {
    int v = 1;
  };
} // namespace edge_types::input_connection

TEST(EdgeTypes, GetInputConnection)
{
  using namespace edge_types::input_connection;
  NodeObject::register_type<A>();

  Network    net;
  const auto src1 = net.add_node(make_node<A>());
  const auto src2 = net.add_node(make_node<A>());
  const auto dst  = add_consumer<A>(net, "edge_types_input_connection");
  EXPECT_FALSE(net.get_input_connection(dst, 0).has_value());

  net.add_connection(7, src1, dst, 0, 0);
  const auto conn = net.get_input_connection(dst, 0);
  ASSERT_TRUE(conn.has_value());
  EXPECT_EQ(conn->source_id, src1);
  EXPECT_EQ(conn->source_output, 0);
  EXPECT_EQ(conn->target_id, dst);
  EXPECT_EQ(conn->target_input, 0);

  // Several edges feeding one input: the highest id wins, as in run().
  net.add_connection(3, src2, dst, 0, 0);
  EXPECT_EQ(net.get_input_connection(dst, 0)->source_id, src1);
  net.add_connection(9, src2, dst, 0, 0);
  EXPECT_EQ(net.get_input_connection(dst, 0)->source_id, src2);
}

// R2 e. Before run() an input fed by an edge is unbound; run() binds it (D12).
namespace edge_types::unbound
{
  struct A
  {
    int v = 1;
  };
} // namespace edge_types::unbound

TEST(EdgeTypes, InputUnboundBeforeRun)
{
  using namespace edge_types::unbound;
  NodeObject::register_type<A>();

  Network    net;
  const auto src = net.add_node(make_node<A>());
  const auto dst = add_consumer<A>(net, "edge_types_unbound");
  net.add_connection(src, dst, 0, 0);
  EXPECT_EQ(net.get_node(dst)->get_input(0), nullptr);

  Network::set_touch_file_base_path("edge_types_touch");
  net.run();
  EXPECT_EQ(net.get_node(dst)->get_input(0), net.get_node(src));
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
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));
  EXPECT_THROW(net.validate(), TypeMismatchException);
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
  EXPECT_NO_THROW(net.add_connection(src, dst, 0, 0));
  EXPECT_THROW(net.validate(), TypeMismatchException);
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

// Edge type check (#64) across a pass-through argument:
//
//   [0: Derived] --e_up--> [1: setup(Base &)] --e_down--> [2: consume(const Derived &)]
//
// setup's pass-through output is declared as Base but carries the Derived
// object bound upstream, so e_down is valid (on main the graph runs and
// consume returns 42 whatever the edge order). Network::validate() resolves
// the pass-through on the complete graph, so the edge order does not matter.

namespace edge_types::passthrough
{
  struct Base
  {
    virtual ~Base() = default;
    int v           = 0;
  };
  struct Derived : Base
  {
    int extra = 40;
  };

  void
  setup(Base &b)
  {
    b.v = 2;
  }

  void
  increment(Base &b)
  {
    b.v += 1;
  }

  int
  consume(const Derived &d)
  {
    return d.extra + d.v;
  }

  void
  register_all()
  {
    NodeObject::register_elementary_type<int>();
    NodeObject::register_type<Derived>();
    NodeObject::register_base<Derived, Base>();
    NodeObject::register_function(setup, {"edge_types_pt_setup", "obj"});
    NodeObject::register_function(increment,
                                  {"edge_types_pt_increment", "obj"});
    NodeObject::register_function(consume,
                                  {"edge_types_pt_consume", "value", "obj"});
  }

  // Graph above as JSON. Edge keys are compared as strings, so "10" (e_down)
  // is bound before "9" (e_up).
  json
  graph_json()
  {
    json j;
    j["workflow"]["nodes"]["0"]["type"] = detail::hash<Derived>();
    j["workflow"]["nodes"]["1"]["type"] = "edge_types_pt_setup";
    j["workflow"]["nodes"]["2"]["type"] = "edge_types_pt_consume";
    j["workflow"]["edges"]["9"]         = {{"source", 0},
                                           {"source_output", 0},
                                           {"target", 1},
                                           {"target_input", 0}};
    j["workflow"]["edges"]["10"]        = {{"source", 1},
                                           {"source_output", 0},
                                           {"target", 2},
                                           {"target_input", 0}};
    return j;
  }

  // Chain of pass-through arguments:
  //
  //   [0: Derived] --e0--> [1: increment(Base &)] --e1--> [2: increment]
  //                --e2--> [3: increment] --e3--> [4: consume(const Derived &)]
  //
  // Every link carries the same Derived object, so consume returns 40 + 3.
  // Edge keys are compared as strings ("10" < "11" < "8" < "9"), so the edges
  // are bound in reverse order: e3, e2, e1, e0.
  json
  chain_graph_json()
  {
    json j;
    j["workflow"]["nodes"]["0"]["type"] = detail::hash<Derived>();
    j["workflow"]["nodes"]["1"]["type"] = "edge_types_pt_increment";
    j["workflow"]["nodes"]["2"]["type"] = "edge_types_pt_increment";
    j["workflow"]["nodes"]["3"]["type"] = "edge_types_pt_increment";
    j["workflow"]["nodes"]["4"]["type"] = "edge_types_pt_consume";
    j["workflow"]["edges"]["9"]         = {{"source", 0},
                                           {"source_output", 0},
                                           {"target", 1},
                                           {"target_input", 0}};
    j["workflow"]["edges"]["8"]         = {{"source", 1},
                                           {"source_output", 0},
                                           {"target", 2},
                                           {"target_input", 0}};
    j["workflow"]["edges"]["11"]        = {{"source", 2},
                                           {"source_output", 0},
                                           {"target", 3},
                                           {"target_input", 0}};
    j["workflow"]["edges"]["10"]        = {{"source", 3},
                                           {"source_output", 0},
                                           {"target", 4},
                                           {"target_input", 0}};
    return j;
  }
} // namespace edge_types::passthrough

TEST(EdgeTypes, PassThroughUpstreamEdgeFirst)
{
  using namespace edge_types::passthrough;
  register_all();
  Network    net;
  const auto d = net.add_node(make_node<Derived>());
  const auto s = net.add_node(make_method_node("edge_types_pt_setup", setup));
  const auto c =
    net.add_node(make_method_node("edge_types_pt_consume", consume));
  net.add_connection(d, s, 0, 0); // e_up
  net.add_connection(s, c, 0, 0); // e_down
  net.run();
  EXPECT_EQ(net.get_node(c)->get_output(0)->get<int>(), 42);
}

TEST(EdgeTypes, PassThroughDownstreamEdgeFirst)
{
  using namespace edge_types::passthrough;
  register_all();
  Network    net;
  const auto d = net.add_node(make_node<Derived>());
  const auto s = net.add_node(make_method_node("edge_types_pt_setup", setup));
  const auto c =
    net.add_node(make_method_node("edge_types_pt_consume", consume));
  net.add_connection(s, c, 0, 0); // e_down
  net.add_connection(d, s, 0, 0); // e_up
  net.run();
  EXPECT_EQ(net.get_node(c)->get_output(0)->get<int>(), 42);
}

TEST(EdgeTypes, PassThroughJsonLoadEdgeKeyOrder)
{
  using namespace edge_types::passthrough;
  register_all();
  const json j = graph_json();
  Network net;
  net.from_json(j);
  net.run();
  EXPECT_EQ(net.get_node(2)->get_output(0)->get<int>(), 42);
}

TEST(EdgeTypes, PassThroughChainUpstreamEdgesFirst)
{
  using namespace edge_types::passthrough;
  register_all();
  Network    net;
  const auto d  = net.add_node(make_node<Derived>());
  const auto i1 =
    net.add_node(make_method_node("edge_types_pt_increment", increment));
  const auto i2 =
    net.add_node(make_method_node("edge_types_pt_increment", increment));
  const auto i3 =
    net.add_node(make_method_node("edge_types_pt_increment", increment));
  const auto c =
    net.add_node(make_method_node("edge_types_pt_consume", consume));
  net.add_connection(d, i1, 0, 0);  // e0
  net.add_connection(i1, i2, 0, 0); // e1
  net.add_connection(i2, i3, 0, 0); // e2
  net.add_connection(i3, c, 0, 0);  // e3
  net.run();
  EXPECT_EQ(net.get_node(c)->get_output(0)->get<int>(), 43);
}

TEST(EdgeTypes, PassThroughChainDownstreamEdgesFirst)
{
  using namespace edge_types::passthrough;
  register_all();
  Network    net;
  const auto d  = net.add_node(make_node<Derived>());
  const auto i1 =
    net.add_node(make_method_node("edge_types_pt_increment", increment));
  const auto i2 =
    net.add_node(make_method_node("edge_types_pt_increment", increment));
  const auto i3 =
    net.add_node(make_method_node("edge_types_pt_increment", increment));
  const auto c =
    net.add_node(make_method_node("edge_types_pt_consume", consume));
  net.add_connection(i3, c, 0, 0);  // e3
  net.add_connection(i2, i3, 0, 0); // e2
  net.add_connection(i1, i2, 0, 0); // e1
  net.add_connection(d, i1, 0, 0);  // e0
  net.run();
  EXPECT_EQ(net.get_node(c)->get_output(0)->get<int>(), 43);
}

TEST(EdgeTypes, PassThroughChainJsonLoadEdgeKeyOrder)
{
  using namespace edge_types::passthrough;
  register_all();
  const json j = chain_graph_json();
  Network    net;
  net.from_json(j);
  net.run();
  EXPECT_EQ(net.get_node(4)->get_output(0)->get<int>(), 43);
}

// R2 d. connect() binds the object currently at the source output, so it is
// order-dependent (D3): connected downstream first, setup's pass-through
// output still holds its declared type (Base) and is refused; connected
// upstream first, it carries the Derived object and is accepted.
TEST(EdgeTypes, ConnectPassThroughOrderDependent)
{
  using namespace edge_types::passthrough;
  register_all();
  auto d = make_node<Derived>();
  auto s = make_method_node("edge_types_pt_setup", setup);
  auto c = make_method_node("edge_types_pt_consume", consume);
  EXPECT_THROW(connect(c, {{s, 0}}), TypeMismatchException);

  connect(s, {{d, 0}});
  EXPECT_NO_THROW(connect(c, {{s, 0}}));
}
