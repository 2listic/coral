#ifndef coral_network_h
#define coral_network_h

#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "coral.h"

namespace coral
{
  /**
   * Directed edge between two nodes and their ports.
   */
  class Connection
  {
  public:
    unsigned int source_id     = 0;
    unsigned int target_id     = 0;
    unsigned int source_output = 0;
    unsigned int target_input  = 0;

    Connection() = default;

    Connection(unsigned int _source_id,
               unsigned int _target_id,
               unsigned int _source_output = 0,
               unsigned int _target_input  = 0);

    [[nodiscard]] auto
    to_json() const -> nlohmann::json;

    [[nodiscard]] static auto
    from_json(const nlohmann::json &json) -> Connection;
  };

  /**
   * Graph of nodes and connections with Taskflow execution.
   */
  class Network
  {
  public:
    using Interface = detail::NodeInterface;

  private:
    struct DanglingPort
    {
      unsigned int   node_id        = 0;
      int            argument_index = -1;
      int            input_index    = -1;
      int            output_index   = -1;
      ConnectionType type           = ConnectionType::none;
    };

    std::map<unsigned int, NodeObjectPtr> nodes;
    std::map<unsigned int, std::string>   nodes_name;
    std::map<unsigned int, tf::Task>      node_tasks;
    std::map<unsigned int, Connection>    connections;
    tf::Taskflow                          taskflow;
    std::string                           name;

    void
    rebuild_taskflow();

    auto
    get_arguments() const -> std::vector<DanglingPort>;

    auto
    connections_to_json() const -> nlohmann::json;

    auto
    nodes_to_json() const -> nlohmann::json;

    void
    refresh_dynamic_inputs(unsigned int target_id);

    void
    execute_node_task(unsigned int         node_id,
                      const NodeObjectPtr &node,
                      const std::string   &node_name);

  public:
    Network();
    Network(const Network &other);
    Network &
    operator=(const Network &other);
    Network(Network &&) noexcept;
    Network &
    operator=(Network &&) noexcept;

    static void
    register_node();

    void
    add_node(unsigned int         id,
             const NodeObjectPtr &node,
             const std::string   &node_name = "");

    unsigned int
    add_node(const NodeObjectPtr &node, const std::string &node_name = "");

    void
    set_node_name(unsigned int id, const std::string &name);

    std::string
    get_node_name(unsigned int id) const;

    void
    add_connection(unsigned int id, const Connection &conn);

    void
    add_connection(unsigned int id,
                   unsigned int source_id,
                   unsigned int target_id,
                   unsigned int source_output,
                   unsigned int target_input);

    unsigned int
    add_connection(unsigned int source_id,
                   unsigned int target_id,
                   unsigned int source_output,
                   unsigned int target_input);

    unsigned int
    add_connection(const Connection &conn);

    void
    remove_nodes_and_connections(
      const std::vector<unsigned int> &node_ids,
      const std::vector<unsigned int> &connection_ids);

    unsigned int
    n_connections() const;

    unsigned int
    n_nodes() const;

    nlohmann::json
    get_registry() const;

    void
    from_json(const nlohmann::json &json_data);

    [[nodiscard]] auto
    to_json() const -> nlohmann::json;

    void
    run();

    void
    clear_network();

    auto
    get_node(unsigned int id) const -> NodeObjectPtr;

    auto
    size() const -> size_t;

    auto
    get_taskflow() -> tf::Taskflow &;

    void
    output_dot(const std::filesystem::path &filepath) const;

    auto
    get_connected_nodes(unsigned int nodeId) const -> std::vector<unsigned int>;

    auto
    get_node_connections(unsigned int nodeId) const -> std::vector<Connection>;

    auto
    get_inputs() const -> std::vector<std::pair<unsigned int, unsigned int>>;

    auto
    n_inputs() const -> size_t;

    auto
    get_outputs() const -> std::vector<std::pair<unsigned int, unsigned int>>;

    auto
    n_outputs() const -> size_t;

    NodeObjectPtr
    get_input(const unsigned int index);

    NodeObjectPtr
    get_output(const unsigned int index);

    auto
    build_interface() const -> Interface;

    auto
    get_task(unsigned int id) const -> tf::Task;

    auto
    is_connected(unsigned int inId, unsigned int outId) const -> bool;
  };

  void
  from_json(const json &j, Network &net);

  void
  to_json(json &j, const Network &net);

  void
  to_json(json &j, const Connection &conn);

  void
  from_json(const json &j, Connection &conn);

  auto
  build_network_interface(const std::shared_ptr<Network> &net)
    -> detail::NodeInterface;

} // namespace coral

#if defined(CORAL_HEADER_ONLY) && CORAL_HEADER_ONLY
#  include "coral_network_implementation.h"
#endif

#endif
