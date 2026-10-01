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
