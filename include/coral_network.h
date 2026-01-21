#ifndef coral_network_h
#define coral_network_h


#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>

#include "coral.h"
#include "taskflow/core/task.hpp"

namespace coral
{
  class Connection
  {
  public:
    // Basic connection properties
    unsigned int source_id;
    unsigned int target_id;
    unsigned int source_output;
    unsigned int target_input;

    // Default constructor for deserialization
    Connection() = default;

    // Constructor with node IDs and ports
    Connection(unsigned int _source_id,
               unsigned int _target_id,
               unsigned int _source_output = 0,
               unsigned int _target_input  = 0)
      : source_id(_source_id)
      , target_id(_target_id)
      , source_output(_source_output)
      , target_input(_target_input)
    {}

    // Serialize connection to JSON
    [[nodiscard]] auto
    to_json() const -> nlohmann::json
    {
      nlohmann::json json;
      json["source"]        = source_id;
      json["target"]        = target_id;
      json["source_output"] = source_output;
      json["target_input"]  = target_input;
      return json;
    }

    // Create connection from JSON
    [[nodiscard]] static auto
    from_json(const nlohmann::json &json) -> Connection
    {
      // Check for required fields
      if (!json.contains("source") || !json.contains("target"))
        {
          throw std::runtime_error(
            "Connection JSON must contain 'source' and 'target' fields");
        }

      Connection conn;
      conn.source_id = json["source"].get<unsigned int>();
      conn.target_id = json["target"].get<unsigned int>();

      // Require source_output and target_input fields
      if (!json.contains("source_output") || !json.contains("target_input"))
        {
          throw std::runtime_error(
            "Connection JSON must contain 'source_output' and 'target_input' fields");
        }

      conn.source_output = json["source_output"].get<unsigned int>();
      conn.target_input  = json["target_input"].get<unsigned int>();

      return conn;
    }
  };

  class Network
  {
  public:
    struct DanglingPort
    {
      unsigned int   node_id        = 0;
      int            argument_index = -1;
      int            input_index    = -1;
      int            output_index   = -1;
      ConnectionType type           = ConnectionType::none;
    };

    using Interface = NodeInterface;

  private:
    std::map<unsigned int, std::shared_ptr<NodeObject>> nodes;
    std::map<unsigned int, std::string>                 nodes_name;
    std::map<unsigned int, tf::Task>                    node_tasks;

    // Store connections by their ID
    std::map<unsigned int, Connection> connections;

    tf::Taskflow taskflow;

  public:
    Network() = default;

    Network(const Network &other)
      : nodes(other.nodes)
      , nodes_name(other.nodes_name)
      , connections(other.connections)
    {
      rebuild_taskflow();
    }

    Network &
    operator=(const Network &other)
    {
      if (this == &other)
        return *this;
      nodes       = other.nodes;
      nodes_name  = other.nodes_name;
      connections = other.connections;
      rebuild_taskflow();
      return *this;
    }

    Network(Network &&) noexcept = default;
    Network &
    operator=(Network &&) noexcept = default;

    static void
    register_node();

  private:
    void
    rebuild_taskflow()
    {
      taskflow.clear();
      node_tasks.clear();

      for (const auto &entry : nodes)
        {
          const auto node_id = entry.first;
          const auto node    = entry.second;
          const auto name    = get_node_name(node_id);
          node_tasks[node_id] =
            taskflow
              .emplace([node, node_id, name]() {
                std::cout << "Running node " << node_id << ": " << name
                          << " (type = " << node->type_name() << ")"
                          << std::endl;
                (*node)();
              })
              .name(name == "" ? "node_" + std::to_string(node_id) + ": " +
                                   node->type_name() :
                                 name);
        }

      for (const auto &[conn_id, conn] : connections)
        {
          (void)conn_id;
          if (node_tasks.find(conn.source_id) == node_tasks.end() ||
              node_tasks.find(conn.target_id) == node_tasks.end())
            throw std::runtime_error(
              "Taskflow rebuild missing source or target node.");
          node_tasks[conn.source_id].precede(node_tasks[conn.target_id]);
        }
    }

  public:
    void
    add_node(unsigned int                       id,
             const std::shared_ptr<NodeObject> &node,
             const std::string                 &node_name = "")
    {
      nodes[id]      = node;
      nodes_name[id] = node_name;
      node_tasks[id] =
        taskflow
          .emplace([node, id, node_name]() {
            std::cout << "Running node " << id << ": " << node_name
                      << " (type = " << node->type_name() << ")" << std::endl;
            (*node)();
          })
          .name(node_name == "" ?
                  "node_" + std::to_string(id) + ": " + node->type_name() :
                  node_name);
    }

    unsigned int
    add_node(const std::shared_ptr<NodeObject> &node,
             const std::string                 &node_name = "")
    {
      unsigned int id = nodes.empty() ? 0 : nodes.rbegin()->first + 1;
      add_node(id, node, node_name);
      return id;
    }

    void
    set_node_name(unsigned int id, const std::string &name)
    {
      nodes_name[id] = name;
      if (node_tasks.find(id) != node_tasks.end())
        {
          node_tasks[id].name(name == "" ? "node_" + std::to_string(id) + ": " +
                                             nodes[id]->type_name() :
                                           name);
        }
    }

    std::string
    get_node_name(unsigned int id) const
    {
      auto it = nodes_name.find(id);
      return it == nodes_name.end() ? std::string() : it->second;
    }


    void
    add_connection(unsigned int id, const Connection &conn)
    {
      connections[id] = conn;
      // Ensure both source and target nodes exist
      if (nodes.find(conn.source_id) == nodes.end())
        {
          throw std::runtime_error("Source target node not found: " +
                                   std::to_string(conn.source_id));
        }

      if (nodes.find(conn.target_id) == nodes.end())
        {
          throw std::runtime_error("Target node not found: " +
                                   std::to_string(conn.target_id));
        }

      try
        {
          const auto &source_node = nodes.at(conn.source_id);
          const auto &target_node = nodes.at(conn.target_id);
          auto        output_ptr  = source_node->output(conn.source_output);

          if (output_ptr == source_node &&
              get_node_name(conn.source_id).empty())
            {
              const auto &info = target_node->get_info();
              if (info.contains("inputs") && info.contains("arguments") &&
                  conn.target_input < info["inputs"].size())
                {
                  const auto arg_index =
                    info["inputs"][conn.target_input].get<unsigned int>();
                  if (arg_index < info["arguments"].size() &&
                      info["arguments"][arg_index].contains("name"))
                    {
                      set_node_name(conn.source_id,
                                    info["arguments"][arg_index]["name"]
                                      .get<std::string>());
                    }
                }
            }
        }
      catch (const std::exception &)
        {
          // Naming is best-effort; ignore errors
        }

      // Set the input of the target node to the output of the source node
      nodes[conn.target_id]->set_input(
        conn.target_input, nodes[conn.source_id]->output(conn.source_output));

      // Update taskflow dependencies
      if (node_tasks.find(conn.source_id) != node_tasks.end() &&
          node_tasks.find(conn.target_id) != node_tasks.end())
        {
          auto source_task = node_tasks[conn.source_id];
          auto target_task = node_tasks[conn.target_id];

          std::cerr << "[TASKFLOW] Establishing dependency: node " << conn.source_id
                    << " precedes node " << conn.target_id << "\n";

          // Connect the source and target tasks
          source_task.precede(target_task);
        }
      else
        {
          std::cerr << "[TASKFLOW] WARNING: Cannot establish dependency " << conn.source_id
                    << " -> " << conn.target_id << " (tasks not found: "
                    << "source=" << (node_tasks.find(conn.source_id) != node_tasks.end())
                    << ", target=" << (node_tasks.find(conn.target_id) != node_tasks.end())
                    << ")\n";
        }
    }

    void
    add_connection(unsigned int id,
                   unsigned int source_id,
                   unsigned int target_id,
                   unsigned int source_output,
                   unsigned int target_input)
    {
      Connection conn(source_id, target_id, source_output, target_input);
      add_connection(id, conn);
    }


    unsigned int
    add_connection(unsigned int source_id,
                   unsigned int target_id,
                   unsigned int source_output,
                   unsigned int target_input)
    {
      Connection conn(source_id, target_id, source_output, target_input);
      return add_connection(conn);
    }

    unsigned int
    add_connection(const Connection &conn)
    {
      unsigned int id =
        connections.empty() ? 0 : connections.rbegin()->first + 1;
      add_connection(id, conn);
      return id;
    }

    void
    remove_nodes_and_connections(
      const std::vector<unsigned int> &node_ids,
      const std::vector<unsigned int> &connection_ids)
    {
      for (const auto id : connection_ids)
        connections.erase(id);

      for (const auto id : node_ids)
        {
          nodes.erase(id);
          nodes_name.erase(id);
        }

      rebuild_taskflow();
    }

    unsigned int
    n_connections() const
    {
      return connections.size();
    }


    unsigned int
    n_nodes() const
    {
      return nodes.size();
    }

    /**
     * Return a registry containing only the node types used in this network.
     */
    json
    get_registry() const
    {
      std::set<std::string>                              active_types;
      std::map<std::string, std::shared_ptr<NodeObject>> type_to_node;
      for (const auto &[id, node] : nodes)
        {
          (void)id;
          active_types.insert(node->hash());
          if (type_to_node.find(node->hash()) == type_to_node.end())
            type_to_node[node->hash()] = node;
        }

      json full   = NodeObject::get_registry();
      json result = json::object();
      for (const auto &type_hash : active_types)
        {
          if (full.contains(type_hash))
            result[type_hash] = full[type_hash];
          else if (auto it = type_to_node.find(type_hash);
                   it != type_to_node.end())
            {
              // Fallback to the node's own info if not present in the global
              // registry
              result[type_hash] = it->second->get_info();
            }
        }
      return result;
    }

    void
    from_json(const nlohmann::json &json_data)
    {
      // Ensure the input JSON has the expected "workflow" structure
      if (!json_data.contains("workflow"))
        {
          throw std::runtime_error("JSON data must contain 'workflow' object");
        }

      const auto &workflow = json_data["workflow"];

      // Check for required fields
      if (!workflow.contains("nodes"))
        {
          throw std::runtime_error("Workflow must contain 'nodes' object");
        }

      // Clear any existing data
      clear_network();

      const auto &nodes_data = workflow["nodes"];

      // First pass: create all nodes
      for (const auto &[key, value] : nodes_data.items())
        {
          int id = std::stoi(key); // Convert string key to int

          // Prepare node data - ensure type exists for proper
          // deserialization
          nlohmann::json node_data = value;
          if (!node_data.contains("type"))
            {
              throw std::runtime_error(
                "Node " + key +
                " does not contain a 'type' field: " + node_data.dump(2));
            }
          std::string node_name = node_data.value("name", "");
          try
            {
              add_node(id, node_data.get<NodeObjectPtr>(), node_name);
            }
          catch (const std::exception &e)
            {
              std::cout << "Error with node " << key << ": "
                        << node_data.dump(2) << std::endl;
              throw std::runtime_error("Failed to create node " + key + ": " +
                                       e.what());
            }
        }

      // Second pass: connect nodes based on edges
      if (workflow.contains("edges"))
        {
          for (const auto &[edge_key, edge_value] : workflow["edges"].items())
            {
              // Create a Connection object from JSON
              Connection conn = Connection::from_json(edge_value);

              // Use the edge_key as the connection ID (converted to int)
              int conn_id = std::stoi(edge_key);

              add_connection(conn_id, conn);
            }
        }
    }

    void
    run()
    {
      tf::Executor executor;
      executor.run(taskflow).wait();
    }

    void
    clear_network()
    {
      nodes.clear();
      node_tasks.clear();
      connections.clear();
      taskflow.clear();
    }


    [[nodiscard]] auto
    get_node(unsigned int id) const -> std::shared_ptr<NodeObject>
    {
      auto it = nodes.find(id);
      return it != nodes.end() ? it->second : nullptr;
    }

    [[nodiscard]] auto
    get_task(unsigned int id) const -> tf::Task
    {
      auto it = node_tasks.find(id);
      if (it == node_tasks.end())
        {
          throw std::runtime_error("Task with ID " + std::to_string(id) +
                                   " not found.");
        }
      return it->second;
    }

    [[nodiscard]] auto
    size() const -> size_t
    {
      return nodes.size();
    }

    // Added a method to expose the Taskflow instance
    [[nodiscard]] auto
    get_taskflow() -> tf::Taskflow &
    {
      return taskflow;
    }

    // Added a method to output the network as a DOT file
    void
    output_dot(const std::filesystem::path &filepath) const
    {
      std::ofstream dot_file(filepath);
      taskflow.dump(dot_file);
      dot_file.close();
    }

    // Get all nodes that are connected to a specific node (outgoing
    // connections)
    [[nodiscard]] auto
    get_connected_nodes(unsigned int nodeId) const -> std::vector<unsigned int>
    {
      std::vector<unsigned int> result;

      // Iterate through all connections and find those with matching source
      // ID
      for (const auto &[conn_id, conn] : connections)
        {
          if (conn.source_id == nodeId)
            {
              result.push_back(conn.target_id);
            }
        }
      return result;
    }

    // Get all connection objects for a specific source node
    [[nodiscard]] auto
    get_node_connections(unsigned int nodeId) const -> std::vector<Connection>
    {
      std::vector<Connection> result;

      // Iterate through all connections and find those that have this source
      // ID
      for (const auto &[conn_id, conn] : connections)
        {
          if (conn.source_id == nodeId)
            {
              result.push_back(conn);
            }
        }
      return result;
    }

    [[nodiscard]] auto
    get_inputs() const -> std::vector<std::pair<unsigned int, unsigned int>>
    {
      std::vector<std::pair<unsigned int, unsigned int>> result;

      // Ordering is stable by node id (ascending), then by input index.
      for (const auto &[node_id, node] : nodes)
        {
          if (!node)
            continue;

          const auto        n_inputs = node->n_inputs();
          std::vector<bool> connected(n_inputs, false);

          for (const auto &[conn_id, conn] : connections)
            {
              (void)conn_id;
              if (conn.target_id == node_id && conn.target_input < n_inputs)
                connected[conn.target_input] = true;
            }

          for (unsigned int i = 0; i < n_inputs; ++i)
            if (!connected[i])
              result.emplace_back(node_id, i);
        }

      return result;
    }

    [[nodiscard]] auto
    n_inputs() const -> size_t
    {
      return get_inputs().size();
    }

    [[nodiscard]] auto
    get_outputs() const -> std::vector<std::pair<unsigned int, unsigned int>>
    {
      std::vector<std::pair<unsigned int, unsigned int>> result;

      // Ordering is stable by node id (ascending), then by output index.
      for (const auto &[node_id, node] : nodes)
        {
          if (!node)
            continue;

          const auto        n_outputs = node->n_outputs();
          std::vector<bool> connected(n_outputs, false);

          for (const auto &[conn_id, conn] : connections)
            {
              (void)conn_id;
              if (conn.source_id == node_id && conn.source_output < n_outputs)
                connected[conn.source_output] = true;
            }

          for (unsigned int i = 0; i < n_outputs; ++i)
            if (!connected[i])
              result.emplace_back(node_id, i);
        }

      return result;
    }

    [[nodiscard]] auto
    n_outputs() const -> size_t
    {
      return get_outputs().size();
    }

    NodeObjectPtr
    input(const unsigned int index)
    {
      // Indexing follows get_inputs(): ascending node id, then input index.
      const auto inputs = get_inputs();
      if (index >= inputs.size())
        throw std::runtime_error("Network input index out of bounds.");
      const auto &[node_id, port_index] = inputs[index];
      auto it                           = nodes.find(node_id);
      if (it == nodes.end() || !it->second)
        throw std::runtime_error("Network input node not found.");
      return it->second->input(port_index);
    }

    NodeObjectPtr
    output(const unsigned int index)
    {
      // Indexing follows get_outputs(): ascending node id, then output index.
      const auto outputs = get_outputs();
      if (index >= outputs.size())
        throw std::runtime_error("Network output index out of bounds.");
      const auto &[node_id, port_index] = outputs[index];
      auto it                           = nodes.find(node_id);
      if (it == nodes.end() || !it->second)
        throw std::runtime_error("Network output node not found.");
      return it->second->output(port_index);
    }

    [[nodiscard]] auto
    get_arguments(const std::map<unsigned int, std::map<int, int>> &remap_table =
                    {}) const -> std::vector<DanglingPort>
    {
      std::vector<DanglingPort> result;

      for (const auto &[node_id, node] : nodes)
        {
          if (!node)
            continue;

          const auto                  info = node->get_info();
          std::map<int, DanglingPort> by_argument;

          if (info.contains("inputs"))
            {
              const auto &inputs = info["inputs"];
              for (unsigned int i = 0; i < inputs.size(); ++i)
                {
                  int arg_index = inputs[i].get<int>();

                  // Apply remapping if available for this node
                  auto node_remap_it = remap_table.find(node_id);
                  if (node_remap_it != remap_table.end())
                    {
                      auto index_remap_it =
                        node_remap_it->second.find(arg_index);
                      if (index_remap_it != node_remap_it->second.end())
                        {
                          arg_index = index_remap_it->second;
                        }
                    }

                  bool connected = false;
                  for (const auto &[conn_id, conn] : connections)
                    {
                      (void)conn_id;
                      if (conn.target_id == node_id && conn.target_input == i)
                        {
                          connected = true;
                          break;
                        }
                    }
                  if (!connected)
                    {
                      auto &entry          = by_argument[arg_index];
                      entry.node_id        = node_id;
                      entry.argument_index = arg_index;
                      entry.input_index    = static_cast<int>(i);
                    }
                }
            }

          if (info.contains("outputs"))
            {
              const auto &outputs = info["outputs"];
              for (unsigned int i = 0; i < outputs.size(); ++i)
                {
                  int arg_index = outputs[i].get<int>();
                  if (arg_index < 0)
                    continue;

                  // Apply remapping if available for this node
                  auto node_remap_it = remap_table.find(node_id);
                  if (node_remap_it != remap_table.end())
                    {
                      auto index_remap_it =
                        node_remap_it->second.find(arg_index);
                      if (index_remap_it != node_remap_it->second.end())
                        {
                          arg_index = index_remap_it->second;
                        }
                    }

                  bool connected = false;
                  for (const auto &[conn_id, conn] : connections)
                    {
                      (void)conn_id;
                      if (conn.source_id == node_id && conn.source_output == i)
                        {
                          connected = true;
                          break;
                        }
                    }
                  if (!connected)
                    {
                      auto &entry          = by_argument[arg_index];
                      entry.node_id        = node_id;
                      entry.argument_index = arg_index;
                      entry.output_index   = static_cast<int>(i);
                    }
                }
            }

          for (auto &[arg_index, entry] : by_argument)
            {
              (void)arg_index;
              const bool has_input  = entry.input_index >= 0;
              const bool has_output = entry.output_index >= 0;
              if (has_input && has_output)
                entry.type = ConnectionType::pass_through;
              else if (has_input)
                entry.type = ConnectionType::input;
              else if (has_output)
                entry.type = ConnectionType::output;
              else
                entry.type = ConnectionType::none;
              result.push_back(entry);
            }
        }

      return result;
    }

    [[nodiscard]] auto
    build_argument_remap_table(const nlohmann::json &outer_arguments) const
      -> std::map<unsigned int, std::map<int, int>>
    {
      // Build a mapping from (name, type) -> new index in outer_arguments
      std::map<std::pair<std::string, std::string>, int> name_type_to_index;

      for (size_t i = 0; i < outer_arguments.size(); ++i)
        {
          const auto &arg = outer_arguments[i];
          if (arg.contains("name") && arg.contains("type"))
            {
              std::string name = arg["name"].get<std::string>();
              std::string type = arg["type"].get<std::string>();
              name_type_to_index[{name, type}] = static_cast<int>(i);
            }
        }

      // Build remap table: node_id -> {old_index -> new_index}
      std::map<unsigned int, std::map<int, int>> remap_table;

      // For each node in the embedded network
      for (const auto &[node_id, node] : nodes)
        {
          if (!node)
            continue;

          const auto info = node->get_info();
          if (!info.contains("arguments"))
            continue;

          const auto &arguments = info["arguments"];

          // Build remap for inputs
          if (info.contains("inputs"))
            {
              const auto &inputs = info["inputs"];
              for (size_t i = 0; i < inputs.size(); ++i)
                {
                  int old_index = inputs[i].get<int>();

                  // Skip negative indices (internal nodes)
                  if (old_index < 0)
                    continue;

                  // Get the argument metadata for this input
                  if (i >= arguments.size())
                    continue;

                  const auto &arg = arguments[i];
                  if (!arg.contains("name") || !arg.contains("type"))
                    continue;

                  std::string name = arg["name"].get<std::string>();
                  std::string type = arg["type"].get<std::string>();

                  // Look up the new index by name+type
                  auto it = name_type_to_index.find({name, type});
                  if (it != name_type_to_index.end())
                    {
                      int new_index = it->second;
                      if (new_index != old_index)
                        {
                          std::cerr << "[REMAP] Node " << node_id
                                    << " input " << i << ": " << name << " ("
                                    << type << ") " << old_index << " -> "
                                    << new_index << "\n";
                          remap_table[node_id][old_index] = new_index;
                        }
                    }
                }
            }

          // Build remap for outputs
          if (info.contains("outputs"))
            {
              const auto &outputs = info["outputs"];
              for (size_t i = 0; i < outputs.size(); ++i)
                {
                  int old_index = outputs[i].get<int>();

                  // Skip negative indices (internal outputs)
                  if (old_index < 0)
                    continue;

                  // Find which argument this output corresponds to
                  // by looking for an argument with matching properties
                  for (size_t arg_idx = 0; arg_idx < arguments.size();
                       ++arg_idx)
                    {
                      const auto &arg = arguments[arg_idx];
                      if (!arg.contains("name") || !arg.contains("type"))
                        continue;

                      // Check if this argument is an output or pass_through
                      if (!arg.contains("connection_type"))
                        continue;
                      std::string conn_type =
                        arg["connection_type"].get<std::string>();
                      if (conn_type != "output" &&
                          conn_type != "pass_through")
                        continue;

                      std::string name = arg["name"].get<std::string>();
                      std::string type = arg["type"].get<std::string>();

                      // Look up the new index by name+type
                      auto it = name_type_to_index.find({name, type});
                      if (it != name_type_to_index.end())
                        {
                          int new_index = it->second;
                          if (new_index != old_index)
                            {
                              std::cerr << "[REMAP] Node " << node_id
                                        << " output " << i << ": " << name
                                        << " (" << type << ") " << old_index
                                        << " -> " << new_index << "\n";
                              remap_table[node_id][old_index] = new_index;
                              break;
                            }
                        }
                    }
                }
            }
        }

      return remap_table;
    }

    [[nodiscard]] auto
    build_interface() const -> Interface
    {
      Interface interface;

      for (const auto &entry : get_arguments())
        {
          auto node_it = nodes.find(entry.node_id);
          if (node_it == nodes.end() || !node_it->second)
            throw std::runtime_error("Network interface node not found.");

          const auto info = node_it->second->get_info();
          if (!info.contains("arguments"))
            throw std::runtime_error("Node is missing argument metadata.");

          if (entry.argument_index < 0 ||
              entry.argument_index >=
                static_cast<int>(info["arguments"].size()))
            throw std::runtime_error("Dangling argument index out of range.");

          const auto &arg_info = info["arguments"][entry.argument_index];
          if (!arg_info.contains("type"))
            throw std::runtime_error("Argument metadata missing type.");

          nlohmann::json arg_json;
          arg_json["type"]            = arg_info["type"];
          arg_json["connection_type"] = magic_enum::enum_name(entry.type);
          if (arg_info.contains("name"))
            arg_json["name"] = arg_info["name"];

          const auto arg_index =
            static_cast<unsigned int>(interface.arguments.size());
          interface.arguments.push_back(arg_json);

          if ((entry.type & ConnectionType::input) != ConnectionType::none)
            interface.inputs.push_back(arg_index);
          if ((entry.type & ConnectionType::output) != ConnectionType::none)
            interface.outputs.push_back(arg_index);
        }

      return interface;
    }

    // Check if two nodes are connected (direct connection from inId to outId)
    [[nodiscard]] auto
    is_connected(unsigned int inId, unsigned int outId) const -> bool
    {
      // Loop through all connections to find a match
      for (const auto &[conn_id, conn] : connections)
        {
          // Check if the source and target match the specified IDs
          if (conn.source_id == inId && conn.target_id == outId)
            {
              return true;
            }
        }
      return false;
    }

    // Serialize all connections to JSON
    [[nodiscard]] auto
    connections_to_json() const -> nlohmann::json
    {
      nlohmann::json json;

      for (const auto &[connection_id, connection] : connections)
        json[std::to_string(connection_id)] = connection.to_json();

      return json;
    }

    // Get all connection IDs
    [[nodiscard]] auto
    nodes_to_json() const -> nlohmann::json
    {
      nlohmann::json json;

      for (const auto &[node_id, node] : nodes)
        {
          nlohmann::json node_json;
          node_json["type"] = node->hash();

          const auto name = get_node_name(node_id);
          if (!name.empty())
            node_json["name"] = name;

          if (node->node_type() == NodeType::elementary_constructor)
            {
              const auto &info = node->get_info();
              if (info.contains("value"))
                node_json["value"] = info["value"];
            }

          json[std::to_string(node_id)] = node_json;
        }

      return json;
    }


    // Serialize the entire network to JSON
    [[nodiscard]] auto
    to_json() const -> nlohmann::json
    {
      nlohmann::json json;
      json["workflow"]["nodes"] = nodes_to_json();
      json["workflow"]["edges"] = connections_to_json();

      // Add metadata
      json["version"] = 1;
      json["author"]  = "coral-editor";

      // Add timestamp in ISO format
      auto              now        = std::chrono::system_clock::now();
      auto              now_time_t = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << std::put_time(std::gmtime(&now_time_t), "%FT%T.000Z");
      json["date_time_utc"] = ss.str();

      return json;
    }
  };

  inline void
  from_json(const json &j, Network &net)
  {
    net.from_json(j);
  }

  inline void
  to_json(json &j, const Network &net)
  {
    j = net.to_json();
  }

  inline void
  to_json(json &j, const Connection &conn)
  {
    j = conn.to_json();
  }

  inline void
  from_json(const json &j, Connection &conn)
  {
    conn = Connection::from_json(j);
  }

  inline void
  Network::register_node()
  {
    auto &initializer = NodeObject::register_json_header<Network>();
    initializer.json_serializer["node_type"] = "network";
    initializer.node_type                    = NodeType::network;
    initializer.json_serializer["value"]     = "{}";

    NodeObject::network_type_hash = coral::hash<Network>();
    NodeObject::network_interface_builder =
      [](const std::shared_ptr<entt::meta_any> &value) -> NodeInterface {
      const auto ptr = value->template try_cast<std::shared_ptr<Network>>();
      if (ptr == nullptr)
        throw std::runtime_error(
          "Could not cast meta_any to shared_ptr<Network>.");
      return coral::build_network_interface(*ptr);
    };

    initializer.parse_string =
      [](const std::string &value) -> std::shared_ptr<entt::meta_any> {
      auto        t       = std::make_shared<Network>();
      std::string payload = value;
      bool        is_path = false;
      if (!value.empty())
        {
          try
            {
              is_path = std::filesystem::exists(value);
            }
          catch (const std::filesystem::filesystem_error &)
            {
              is_path = false;
            }
        }
      if (is_path)
        {
          std::ifstream input(value);
          if (!input)
            throw std::runtime_error("Failed to open network file: " + value);
          std::ostringstream buffer;
          buffer << input.rdbuf();
          payload = buffer.str();
        }
      auto j = json::parse(payload);
      if (j.is_string())
        j = json::parse(j.get<std::string>());
      if (!j.contains("workflow") && j.contains("value") &&
          j.at("value").is_string())
        j = json::parse(j.at("value").get<std::string>());
      t->from_json(j);
      return std::make_shared<entt::meta_any>(t);
    };

    initializer.executor =
      [](const std::vector<std::shared_ptr<NodeObject>> &args)
      -> std::shared_ptr<entt::meta_any> {
      if (args.size() != 0)
        throw std::runtime_error("Wrong number of arguments.");
      return std::make_shared<entt::meta_any>(std::make_shared<Network>());
    };

    initializer.to_string =
      [](const std::shared_ptr<entt::meta_any> &value) -> std::string {
      const auto ptr = value->template try_cast<std::shared_ptr<Network>>();
      if (ptr == nullptr)
        throw std::runtime_error(
          "Could not cast meta_any to requested shared_ptr type.");
      const Network &t = **ptr;
      return json(t).dump();
    };

    NodeObject::set_network_executor([](NodeObject &node,
                                        std::vector<std::shared_ptr<NodeObject>>
                                          args)
                                       -> std::shared_ptr<entt::meta_any> {
      auto      &value = static_cast<std::shared_ptr<entt::meta_any> &>(node);
      const auto ptr   = value->template try_cast<std::shared_ptr<Network>>();
      if (ptr == nullptr)
        throw std::runtime_error(
          "Could not cast meta_any to shared_ptr<Network>.");
      if (!*ptr)
        throw std::runtime_error("Network object is not initialized.");

      auto      &network  = **ptr;
      const auto info = node.get_info();

      // Remap embedded network node IDs to avoid conflicts with parent context
      // We need to ensure embedded network's internal nodes don't conflict with
      // the parent network's node IDs or with dynamically added argument nodes
      std::map<unsigned int, unsigned int> node_id_remap;
      unsigned int next_available_id = 0;

      // Find the maximum existing node ID in the embedded network
      for (const auto &[node_id, node_obj] : network.nodes)
        {
          (void)node_obj;
          if (node_id >= next_available_id)
            next_available_id = node_id + 1;
        }

      // Check if any embedded network node IDs might conflict with common IDs
      // We'll remap all nodes to start from a high offset to avoid conflicts
      // Use offset starting at 1000 * (some identifier) to create namespace separation
      const unsigned int ID_OFFSET = 1000;
      bool needs_remapping = false;

      // DISABLE REMAPPING FOR NOW TO TEST IF IT'S CAUSING THE ISSUE
      needs_remapping = false;

      /*
      for (const auto &[node_id, node_obj] : network.nodes)
        {
          (void)node_obj;
          // If any node ID is less than ID_OFFSET, we should remap all nodes
          // to avoid potential conflicts with parent network or argument nodes
          if (node_id < ID_OFFSET)
            {
              needs_remapping = true;
              break;
            }
        }
      */

      if (needs_remapping)
        {
          std::cerr << "[NETWORK_EXECUTOR] Remapping embedded network node IDs to avoid conflicts\n";

          // Build the remap table: old_id -> new_id
          unsigned int new_id = ID_OFFSET;
          for (const auto &[old_id, node_obj] : network.nodes)
            {
              (void)node_obj;
              node_id_remap[old_id] = new_id;
              std::cerr << "[NODE_REMAP] Node " << old_id << " -> " << new_id << "\n";
              new_id++;
            }

          // Create new nodes map with remapped IDs
          std::map<unsigned int, std::shared_ptr<NodeObject>> new_nodes;
          std::map<unsigned int, std::string> new_nodes_name;
          for (const auto &[old_id, node_obj] : network.nodes)
            {
              unsigned int new_node_id = node_id_remap[old_id];
              new_nodes[new_node_id] = node_obj;
              if (auto it = network.nodes_name.find(old_id); it != network.nodes_name.end())
                new_nodes_name[new_node_id] = it->second;
            }

          // Create new connections map with remapped node IDs
          std::map<unsigned int, Connection> new_connections;
          unsigned int conn_id = 0;
          for (const auto &[old_conn_id, conn] : network.connections)
            {
              (void)old_conn_id;
              Connection new_conn;
              new_conn.source_id = node_id_remap[conn.source_id];
              new_conn.target_id = node_id_remap[conn.target_id];
              new_conn.source_output = conn.source_output;
              new_conn.target_input = conn.target_input;
              new_connections[conn_id] = new_conn;
              std::cerr << "[CONN_REMAP] Edge " << conn.source_id << " -> " << conn.target_id
                        << " remapped to " << new_conn.source_id << " -> " << new_conn.target_id << "\n";
              conn_id++;
            }

          // Replace network's internal maps with remapped versions
          network.nodes = new_nodes;
          network.nodes_name = new_nodes_name;
          network.connections = new_connections;

          // Rebuild taskflow with new node IDs
          network.rebuild_taskflow();

          // CRITICAL: Re-establish input/output pointers after remapping
          // The rebuild_taskflow() only updates the task DAG, not the data flow!
          std::cerr << "[NETWORK_EXECUTOR] Re-establishing input/output pointers after remapping\n";
          for (const auto &[conn_id, conn] : network.connections)
            {
              (void)conn_id;
              auto source_node = network.nodes[conn.source_id];
              auto target_node = network.nodes[conn.target_id];

              // Reconnect the data flow: target's input points to source's output
              target_node->set_input(conn.target_input,
                                    source_node->output(conn.source_output));

              std::cerr << "[INPUT_RECONNECT] Node " << conn.target_id
                        << " input " << conn.target_input
                        << " <- Node " << conn.source_id
                        << " output " << conn.source_output << "\n";
            }

          std::cerr << "[NETWORK_EXECUTOR] Node ID remapping completed\n";
        }

      // Build remap table to translate embedded network's argument indices to match outer network's argument order
      std::map<unsigned int, std::map<int, int>> remap_table;
      if (info.contains("arguments"))
        {
          std::cerr << "[NETWORK_EXECUTOR] Building argument remap table based on name+type\n";
          remap_table = network.build_argument_remap_table(info["arguments"]);
        }

      const auto args_map = network.get_arguments(remap_table);

      if (args.size() != args_map.size())
        throw std::runtime_error("Wrong number of arguments.");
      std::vector<unsigned int> output_to_argument;
      if (info.contains("outputs"))
        {
          const auto &outputs = info["outputs"];
          output_to_argument.reserve(outputs.size());
          for (const auto &entry : outputs)
            {
              const auto arg_index = entry.get<int>();
              output_to_argument.push_back(
                arg_index >= 0 ? static_cast<unsigned int>(arg_index) :
                                 std::numeric_limits<unsigned int>::max());
            }
        }

      struct InputRestore
      {
        unsigned int  node_id;
        unsigned int  input_index;
        NodeObjectPtr previous;
      };

      std::vector<unsigned int> added_node_ids;
      std::vector<unsigned int> added_connection_ids;
      std::vector<InputRestore> restore_inputs;

      bool cleaned = false;
      auto cleanup = [&]() {
        if (cleaned)
          return;
        cleaned = true;

        for (const auto &entry : restore_inputs)
          {
            try
              {
                auto target_node = network.get_node(entry.node_id);
                if (target_node)
                  target_node->set_input(entry.input_index, entry.previous);
              }
            catch (const std::exception &)
              {}
          }

        try
          {
            network.remove_nodes_and_connections(added_node_ids,
                                                 added_connection_ids);
          }
        catch (const std::exception &)
          {}
      };

      try
        {
          // Connect ready arguments to their corresponding network inputs.
          std::cerr << "[NETWORK_EXECUTOR] Starting argument binding loop\n";
          std::cerr << "[NETWORK_EXECUTOR] args_map.size() = " << args_map.size() << "\n";
          std::cerr << "[NETWORK_EXECUTOR] args.size() = " << args.size() << "\n";
          std::cerr << "[NETWORK_EXECUTOR] Network inputs before binding: " << network.get_inputs().size() << "\n";

          for (size_t i = 0; i < args_map.size(); ++i)
            {
              const auto &entry = args_map[i];
              const auto &arg   = args[i];

              std::cerr << "[NETWORK_EXECUTOR] Processing argument " << i << ":\n";
              std::cerr << "  argument_index = " << entry.argument_index << "\n";
              std::cerr << "  node_id = " << entry.node_id << "\n";
              std::cerr << "  input_index = " << entry.input_index << "\n";
              std::cerr << "  type = " << static_cast<int>(entry.type) << " (input=" << static_cast<int>(ConnectionType::input)
                        << ", pass_through=" << static_cast<int>(ConnectionType::pass_through) << ")\n";
              std::cerr << "  arg is null: " << (!arg ? "YES" : "NO") << "\n";

              if (arg)
                {
                  std::cerr << "  arg->ready(): " << (arg->ready() ? "YES" : "NO") << "\n";
                  std::cerr << "  arg->n_outputs(): " << arg->n_outputs() << "\n";
                }

              if (!arg || !arg->ready())
                {
                  std::cerr << "  SKIPPING: argument is " << (!arg ? "null" : "not ready") << "\n";
                  continue;
                }

              if ((entry.type & ConnectionType::input) == ConnectionType::none)
                {
                  std::cerr << "  SKIPPING: connection type has no input flag\n";
                  continue;
                }

              if (entry.input_index < 0)
                throw std::runtime_error(
                  "Network input index is invalid for argument " +
                  std::to_string(i) + ".");

              if (arg->n_outputs() == 0)
                throw std::runtime_error("Argument " + std::to_string(i) +
                                         " has no outputs to connect.");

              auto target_node = network.get_node(entry.node_id);
              if (!target_node)
                throw std::runtime_error(
                  "Network input node not found for argument " +
                  std::to_string(i) + ".");

              const auto input_index =
                static_cast<unsigned int>(entry.input_index);
              if (input_index >= target_node->n_inputs())
                throw std::runtime_error(
                  "Network input index out of bounds for argument " +
                  std::to_string(i) + ".");

              std::cerr << "  BINDING: Adding argument as node to network\n";
              auto node_id = network.add_node(arg);
              added_node_ids.push_back(node_id);
              std::cerr << "  Added node_id = " << node_id << "\n";

              restore_inputs.push_back(
                {entry.node_id, input_index, target_node->input(input_index)});

              std::cerr << "  Creating connection: node " << node_id << " (output 0) -> node " << entry.node_id
                        << " (input " << input_index << ")\n";
              auto conn_id =
                network.add_connection(node_id, entry.node_id, 0, input_index);
              added_connection_ids.push_back(conn_id);
              std::cerr << "  Connection created with id = " << conn_id << "\n";

              if (entry.type == ConnectionType::pass_through)
                {
                  std::cerr << "  This is a PASS_THROUGH argument\n";
                  std::cerr << "  output_to_argument.size() = " << output_to_argument.size() << "\n";
                  for (size_t out_index = 0;
                       out_index < output_to_argument.size();
                       ++out_index)
                    {
                      std::cerr << "    Checking output " << out_index << ": maps to argument "
                                << output_to_argument[out_index] << "\n";
                      if (output_to_argument[out_index] ==
                          static_cast<unsigned int>(entry.argument_index))
                        {
                          std::cerr << "    SETTING OUTPUT " << out_index << " to arg->output(0) BEFORE network runs\n";
                          node.set_output(static_cast<unsigned int>(out_index),
                                          arg->output(0));
                          break;
                        }
                    }
                }
            }

          std::cerr << "[NETWORK_EXECUTOR] After binding: network inputs remaining = " << network.get_inputs().size() << "\n";
          if (!network.get_inputs().empty())
            throw std::runtime_error(
              "Network has unconnected inputs after binding arguments.");

          // Dump taskflow for debugging
          std::cerr << "[NETWORK_EXECUTOR] Dumping taskflow DAG to /tmp/embedded_network.dot\n";
          std::ofstream dot_file("/tmp/embedded_network.dot");
          network.get_taskflow().dump(dot_file);
          dot_file.close();

          // List all nodes and their ready status before execution
          std::cerr << "[NETWORK_EXECUTOR] Node inventory before execution:\n";
          for (const auto &[node_id, node_obj] : network.nodes)
            {
              std::cerr << "  Node " << node_id << ": ready=" << node_obj->ready()
                        << ", n_inputs=" << node_obj->n_inputs()
                        << ", n_outputs=" << node_obj->n_outputs() << "\n";
            }

          std::cerr << "[NETWORK_EXECUTOR] Running embedded network...\n";
          network.run();
          std::cerr << "[NETWORK_EXECUTOR] Network execution completed\n";

          const auto outputs = network.get_outputs();
          std::cerr << "[NETWORK_EXECUTOR] Network has " << outputs.size() << " outputs\n";
          for (size_t out_index = 0; out_index < outputs.size(); ++out_index)
            {
              std::cerr << "  Setting output " << out_index << " to network.output(" << out_index << ") AFTER network runs\n";
              node.set_output(static_cast<unsigned int>(out_index),
                              network.output(out_index));
            }
          cleanup();
        }
      catch (...)
        {
          cleanup();
          throw;
        }

      return value;
    });
  }

  inline auto
  build_network_interface(const std::shared_ptr<Network> &net) -> NodeInterface
  {
    if (!net)
      throw std::runtime_error("Network interface builder received null.");
    return net->build_interface();
  }

} // namespace coral
#endif
