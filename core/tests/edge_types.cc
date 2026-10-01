#include <gtest/gtest.h>

#include <string>

#include "coral.h"
#include "coral_network.h"

using namespace coral;
using json = nlohmann::json;

// Edge type check (#64) across a pass-through argument:
//
//   [0: Derived] --e_up--> [1: setup(Base &)] --e_down--> [2: consume(const Derived &)]
//
// setup's pass-through output is declared as Base but carries the Derived
// object bound upstream, so e_down is valid (on main the graph runs and
// consume returns 42 whatever the edge order). With the bind-time check, an
// unbound pass-through output still holds a placeholder of its declared type
// (Base), so e_down is refused when it is bound before e_up.

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
    NodeObject::register_derived_type<Base, Derived>();
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
