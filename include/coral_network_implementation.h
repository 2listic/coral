#ifndef CORAL_NETWORK_IMPLEMENTATION_H
#define CORAL_NETWORK_IMPLEMENTATION_H

#include <fstream>
#include <string>
#include "coral_network.h"

#if defined(CORAL_HEADER_ONLY) && CORAL_HEADER_ONLY
#  define CORAL_IMPL_INLINE inline
#else
#  define CORAL_IMPL_INLINE
#endif

namespace coral
{
  // Static member definition with default value for backward compatibility
  CORAL_IMPL_INLINE std::filesystem::path Network::touch_file_base_path{"./"};

  CORAL_IMPL_INLINE size_t Network::n_threads = std::thread::hardware_concurrency();

  enum class TouchMode {
    Running,
    Succeeded,
    Failed
  };

  CORAL_IMPL_INLINE void
  touch_file(std::filesystem::path base_path,
             const std::string& name,
            TouchMode mode)
  {
    std::string full_name = name;
    switch (mode) {
    case TouchMode::Running:
      full_name += ".running";
      break;
    case TouchMode::Succeeded:
      full_name += ".succeeded";
      break;
    case TouchMode::Failed:
      full_name += ".failed";
      break;
    }

    std::ofstream{base_path / full_name, std::iostream::app};
  }
  
  CORAL_IMPL_INLINE
  Connection::Connection(unsigned int _source_id,
                         unsigned int _target_id,
                         unsigned int _source_output,
                         unsigned int _target_input)
    : source_id(_source_id)
    , target_id(_target_id)
    , source_output(_source_output)
    , target_input(_target_input)
  {}

  CORAL_IMPL_INLINE auto
  Connection::to_json() const -> nlohmann::json
  {
    nlohmann::json json;
    json["source"]        = source_id;
    json["target"]        = target_id;
    json["source_output"] = source_output;
    json["target_input"]  = target_input;
    return json;
  }



  CORAL_IMPL_INLINE auto
  Connection::from_json(const nlohmann::json &json) -> Connection
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

  CORAL_IMPL_INLINE
  Network::Network() = default;

  CORAL_IMPL_INLINE
  Network::Network(const Network &other)
    : nodes(other.nodes)
    , nodes_name(other.nodes_name)
    , connections(other.connections)
  {
    rebuild_taskflow();
  }



  CORAL_IMPL_INLINE Network &
  Network::operator=(const Network &other)
  {
    if (this == &other)
      return *this;
    nodes       = other.nodes;
    nodes_name  = other.nodes_name;
    connections = other.connections;
    rebuild_taskflow();
    return *this;
  }

  CORAL_IMPL_INLINE
  Network::Network(Network &&) noexcept = default;

  CORAL_IMPL_INLINE Network &
  Network::operator=(Network &&) noexcept = default;

  CORAL_IMPL_INLINE void
  Network::refresh_dynamic_inputs(unsigned int target_id)
  {
    for (const auto &[conn_id, conn] : connections)
      {
        (void)conn_id;
        if (conn.target_id != target_id)
          continue;
        nodes.at(conn.target_id)
          ->bind_input(
            conn.target_input,
            nodes.at(conn.source_id)->get_output(conn.source_output));
      }
  }

  CORAL_IMPL_INLINE void
  Network::set_touch_file_base_path(const std::filesystem::path &path)
  {
    touch_file_base_path = path;
    slog_debug("Set touch file base path to: %s", path.c_str());

    // Create directory if it doesn't exist
    if (!std::filesystem::exists(touch_file_base_path))
      {
        std::filesystem::create_directories(touch_file_base_path);
        slog_info("Created touch file directory: %s", path.c_str());
      }
    else
      {
        // Directory exists: clean up existing touch files
        slog_debug("Cleaning up existing touch files in: %s", path.c_str());

        for (const auto &entry : std::filesystem::directory_iterator(touch_file_base_path))
          {
            if (entry.is_regular_file())
              {
                const auto filename = entry.path().filename().string();
                if (filename.ends_with(".running") ||
                    filename.ends_with(".succeeded") ||
                    filename.ends_with(".failed"))
                  {
                    std::filesystem::remove(entry.path());
                    slog_debug("Removed touch file: %s", filename.c_str());
                  }
              }
          }

        slog_info("Cleaned touch file directory: %s", path.c_str());
      }
  }

  CORAL_IMPL_INLINE void
  Network::set_threads_number(size_t nth)
  {
    n_threads = nth;
  }

  CORAL_IMPL_INLINE void
  Network::execute_node_task(unsigned int         node_id,
                             const NodeObjectPtr &node,
                             const std::string   &node_name)
  {
    slog_info("Start running node %u: %s (type = %s)",
              node_id,
              node_name.c_str(),
              node->type_name().c_str());
    if (!node_name.empty())
      touch_file(touch_file_base_path, node_name, TouchMode::Running);
    try
      {
        refresh_dynamic_inputs(node_id);
        (*node)();
      }
    catch (const std::exception &e)
      {
        if (!node_name.empty())
          touch_file(touch_file_base_path, node_name, TouchMode::Failed);
        throw std::runtime_error("Node " + std::to_string(node_id) +
                                 " failed: " + e.what());
      }
    slog_info("Node %u: %s (type = %s) run",
              node_id,
              node_name.c_str(),
              node->type_name().c_str());
    if (!node_name.empty())
      touch_file(touch_file_base_path, node_name, TouchMode::Succeeded);
  }



  CORAL_IMPL_INLINE void
  Network::rebuild_taskflow()
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
            .emplace([this, node, node_id, name]() {
              this->execute_node_task(node_id, node, name);
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



  CORAL_IMPL_INLINE void
  Network::add_node(unsigned int         id,
                    const NodeObjectPtr &node,
                    const std::string   &node_name)
  {
    if (nodes.find(id) != nodes.end())
      slog_warn("Overwriting existing node id %u", id);
    if (!node)
      slog_error("Adding null node id %u", id);

    nodes[id]      = node;
    nodes_name[id] = node_name;
    node_tasks[id] = taskflow
                       .emplace([this, node, id, node_name]() {
                         this->execute_node_task(id, node, node_name);
                       })
                       .name(node_name == "" ? "node_" + std::to_string(id) +
                                                 ": " + node->type_name() :
                                               node_name);

    slog_debug("Added node %u name='%s' type='%s'",
               id,
               node_name.c_str(),
               node ? node->type_name().c_str() : "<null>");
  }



  CORAL_IMPL_INLINE unsigned int
  Network::add_node(const NodeObjectPtr &node, const std::string &node_name)
  {
    unsigned int id = nodes.empty() ? 0 : nodes.rbegin()->first + 1;
    add_node(id, node, node_name);
    return id;
  }



  CORAL_IMPL_INLINE void
  Network::set_node_name(unsigned int id, const std::string &name)
  {
    nodes_name[id] = name;
    if (node_tasks.find(id) != node_tasks.end())
      {
        node_tasks[id].name(name == "" ? "node_" + std::to_string(id) + ": " +
                                           nodes[id]->type_name() :
                                         name);
      }
  }



  CORAL_IMPL_INLINE std::string
                    Network::get_node_name(unsigned int id) const
  {
    auto it = nodes_name.find(id);
    return it == nodes_name.end() ? std::string() : it->second;
  }



  CORAL_IMPL_INLINE const std::map<unsigned int, std::string>&
  Network::get_nodes_name() const
  {
    return nodes_name;
  }



  CORAL_IMPL_INLINE void
  Network::add_connection(unsigned int id, const Connection &conn)
  {
    if (connections.find(id) != connections.end())
      slog_warn("Overwriting existing connection id %u", id);
    slog_debug("Adding connection %u: %u[%u] -> %u[%u]",
               id,
               conn.source_id,
               conn.source_output,
               conn.target_id,
               conn.target_input);
    connections[id] = conn;
    // Ensure both source and target nodes exist
    if (nodes.find(conn.source_id) == nodes.end())
      {
        throw std::runtime_error("Source node not found: " +
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
        auto        output_ptr  = source_node->get_output(conn.source_output);

        if (output_ptr == source_node && get_node_name(conn.source_id).empty())
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
                    set_node_name(
                      conn.source_id,
                      info["arguments"][arg_index]["name"].get<std::string>());
                  }
              }
          }
      }
    catch (const std::exception &)
      {
        // Naming is best-effort; ignore errors
      }

    // Set the input of the target node to the output of the source node
    nodes[conn.target_id]->bind_input(
      conn.target_input, nodes[conn.source_id]->get_output(conn.source_output));

    const auto source_it = node_tasks.find(conn.source_id);
    if (source_it == node_tasks.end())
      throw std::runtime_error("Source task not found: " +
                               std::to_string(conn.source_id));
    const auto target_it = node_tasks.find(conn.target_id);
    if (target_it == node_tasks.end())
      throw std::runtime_error("Target task not found: " +
                               std::to_string(conn.target_id));
    auto source_task = source_it->second;
    auto target_task = target_it->second;

    // Connect the source and target tasks
    source_task.precede(target_task);
    slog_debug("Connected task precedence: %u -> %u",
               conn.source_id,
               conn.target_id);
  }



  CORAL_IMPL_INLINE void
  Network::add_connection(unsigned int id,
                          unsigned int source_id,
                          unsigned int target_id,
                          unsigned int source_output,
                          unsigned int target_input)
  {
    Connection conn(source_id, target_id, source_output, target_input);
    add_connection(id, conn);
  }



  CORAL_IMPL_INLINE unsigned int
  Network::add_connection(unsigned int source_id,
                          unsigned int target_id,
                          unsigned int source_output,
                          unsigned int target_input)
  {
    Connection conn(source_id, target_id, source_output, target_input);
    return add_connection(conn);
  }



  CORAL_IMPL_INLINE unsigned int
  Network::add_connection(const Connection &conn)
  {
    unsigned int id = connections.empty() ? 0 : connections.rbegin()->first + 1;
    add_connection(id, conn);
    return id;
  }



  CORAL_IMPL_INLINE void
  Network::remove_nodes_and_connections(
    const std::vector<unsigned int> &node_ids,
    const std::vector<unsigned int> &connection_ids)
  {
    slog_info("Removing %zu nodes and %zu connections",
              node_ids.size(),
              connection_ids.size());
    for (const auto id : connection_ids)
      connections.erase(id);

    for (const auto id : node_ids)
      {
        nodes.erase(id);
        nodes_name.erase(id);
      }

    rebuild_taskflow();
  }



  CORAL_IMPL_INLINE unsigned int
  Network::n_connections() const
  {
    return connections.size();
  }



  CORAL_IMPL_INLINE unsigned int
  Network::n_nodes() const
  {
    return nodes.size();
  }



  CORAL_IMPL_INLINE json
  Network::get_registry() const
  {
    std::set<std::string>                active_types;
    std::map<std::string, NodeObjectPtr> type_to_node;
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



  CORAL_IMPL_INLINE void
  Network::from_json(const nlohmann::json &json_data)
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
    slog_info("Loading network from json: %zu nodes",
              nodes_data.size());

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
            const auto dump = node_data.dump(2);
            slog_error("Error with node %s: %s", key.c_str(), dump.c_str());
            throw std::runtime_error("Failed to create node " + key + ": " +
                                     e.what());
          }
      }

    // Second pass: connect nodes based on edges
    if (workflow.contains("edges"))
      {
        slog_info("Loading network edges: %zu edges",
                  workflow["edges"].size());
        for (const auto &[edge_key, edge_value] : workflow["edges"].items())
          {
            // Create a Connection object from JSON
            Connection conn = Connection::from_json(edge_value);

            // Use the edge_key as the connection ID (converted to int)
            int conn_id = std::stoi(edge_key);

            add_connection(conn_id, conn);
          }
      }
    else
      {
        slog_warn("Network JSON has no 'edges' section");
      }
  }



  CORAL_IMPL_INLINE void
  Network::run()
  {
    tf::Executor executor{n_threads};
    slog_info("Running network (%zu nodes, %zu connections) with %zu worker(s).",
              nodes.size(),
              connections.size(),
              n_threads);
    try
      {
        executor.run(taskflow).get();
      }
    catch (const std::exception &e)
      {
        slog_error("Network run failed: %s", e.what());
        throw;
      }
    slog_info("Network run finished");
  }



  CORAL_IMPL_INLINE void
  Network::clear_network()
  {
    if (!nodes.empty() || !connections.empty())
      slog_debug("Clearing network (%zu nodes, %zu connections)",
                 nodes.size(),
                 connections.size());
    nodes.clear();
    node_tasks.clear();
    connections.clear();
    taskflow.clear();
  }



  CORAL_IMPL_INLINE auto
  Network::get_node(unsigned int id) const -> NodeObjectPtr
  {
    auto it = nodes.find(id);
    return it != nodes.end() ? it->second : nullptr;
  }



  CORAL_IMPL_INLINE auto
  Network::size() const -> size_t
  {
    return nodes.size();
  }



  CORAL_IMPL_INLINE auto
  Network::get_taskflow() -> tf::Taskflow &
  {
    return taskflow;
  }



  CORAL_IMPL_INLINE void
  Network::output_dot(const std::filesystem::path &filepath) const
  {
    std::ofstream dot_file(filepath);
    taskflow.dump(dot_file);
    dot_file.close();
  }



  CORAL_IMPL_INLINE auto
  Network::get_connected_nodes(unsigned int nodeId) const
    -> std::vector<unsigned int>
  {
    std::vector<unsigned int> result;

    // Iterate through all connections and find those with matching source ID
    for (const auto &[conn_id, conn] : connections)
      {
        if (conn.source_id == nodeId)
          {
            result.push_back(conn.target_id);
          }
      }
    return result;
  }



  CORAL_IMPL_INLINE auto
  Network::get_node_connections(unsigned int nodeId) const
    -> std::vector<Connection>
  {
    std::vector<Connection> result;

    // Iterate through all connections and find those that have this source ID
    for (const auto &[conn_id, conn] : connections)
      {
        if (conn.source_id == nodeId)
          {
            result.push_back(conn);
          }
      }
    return result;
  }



  CORAL_IMPL_INLINE auto
  Network::get_inputs() const
    -> std::vector<std::pair<unsigned int, unsigned int>>
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



  CORAL_IMPL_INLINE auto
  Network::n_inputs() const -> size_t
  {
    return get_inputs().size();
  }



  CORAL_IMPL_INLINE auto
  Network::get_outputs() const
    -> std::vector<std::pair<unsigned int, unsigned int>>
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



  CORAL_IMPL_INLINE auto
  Network::n_outputs() const -> size_t
  {
    return get_outputs().size();
  }



  CORAL_IMPL_INLINE NodeObjectPtr
  Network::get_input(const unsigned int index)
  {
    // Indexing follows get_inputs(): ascending node id, then input index.
    const auto inputs = get_inputs();
    if (index >= inputs.size())
      throw std::runtime_error("Network input index out of bounds.");
    const auto &[node_id, port_index] = inputs[index];
    auto it                           = nodes.find(node_id);
    if (it == nodes.end() || !it->second)
      throw std::runtime_error("Network input node not found.");
    return it->second->get_input(port_index);
  }



  CORAL_IMPL_INLINE NodeObjectPtr
  Network::get_output(const unsigned int index)
  {
    // Indexing follows get_outputs(): ascending node id, then output index.
    const auto outputs = get_outputs();
    if (index >= outputs.size())
      throw std::runtime_error("Network output index out of bounds.");
    const auto &[node_id, port_index] = outputs[index];
    auto it                           = nodes.find(node_id);
    if (it == nodes.end() || !it->second)
      throw std::runtime_error("Network output node not found.");
    return it->second->get_output(port_index);
  }



  CORAL_IMPL_INLINE auto
  Network::build_interface() const -> Interface
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
            entry.argument_index >= static_cast<int>(info["arguments"].size()))
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



  CORAL_IMPL_INLINE auto
  Network::get_task(unsigned int id) const -> tf::Task
  {
    auto it = node_tasks.find(id);
    if (it == node_tasks.end())
      {
        throw std::runtime_error("Task with ID " + std::to_string(id) +
                                 " not found.");
      }
    return it->second;
  }



  CORAL_IMPL_INLINE auto
  Network::get_arguments() const -> std::vector<DanglingPort>
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
                const int arg_index = inputs[i].get<int>();
                bool      connected = false;
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
                const int arg_index = outputs[i].get<int>();
                if (arg_index < 0)
                  continue;
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



  CORAL_IMPL_INLINE auto
  Network::connections_to_json() const -> nlohmann::json
  {
    nlohmann::json json;

    for (const auto &[connection_id, connection] : connections)
      json[std::to_string(connection_id)] = connection.to_json();

    return json;
  }



  CORAL_IMPL_INLINE auto
  Network::nodes_to_json() const -> nlohmann::json
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



  CORAL_IMPL_INLINE auto
  Network::is_connected(unsigned int inId, unsigned int outId) const -> bool
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



  CORAL_IMPL_INLINE auto
  Network::to_json() const -> nlohmann::json
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



  CORAL_IMPL_INLINE void
  from_json(const json &j, Network &net)
  {
    net.from_json(j);
  }



  CORAL_IMPL_INLINE void
  to_json(json &j, const Network &net)
  {
    j = net.to_json();
  }



  CORAL_IMPL_INLINE void
  to_json(json &j, const Connection &conn)
  {
    j = conn.to_json();
  }



  CORAL_IMPL_INLINE void
  from_json(const json &j, Connection &conn)
  {
    conn = Connection::from_json(j);
  }



  CORAL_IMPL_INLINE void
  Network::register_node()
  {
    auto &initializer = NodeObject::register_json_header<Network>();
    initializer.json_serializer["node_type"] = "network";
    initializer.node_type                    = NodeType::network;
    initializer.json_serializer["value"]     = json::object();

    NodeObject::network_type_hash = coral::detail::hash<Network>();
    NodeObject::network_interface_builder =
      [](
        const std::shared_ptr<entt::meta_any> &value) -> detail::NodeInterface {
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
      if (!j.contains("workflow") && j.contains("value"))
        {
          const auto &inner = j.at("value");
          if (inner.is_string())
            j = json::parse(inner.get<std::string>());
          else
            j = inner;
        }
      t->from_json(j);
      return std::make_shared<entt::meta_any>(t);
    };

    initializer.executor =
      [](const NodeObjectPtr       &node,
         std::vector<NodeObjectPtr> args) -> std::shared_ptr<entt::meta_any> {
      if (!node)
        throw std::runtime_error("Network executor received null node.");
      if (!node->ready())
        {
          const auto info = node->get_info();
          if (info.contains("value"))
            {
              const auto &value = info.at("value");
              if (value.is_string())
                node->parse_string(value.get<std::string>());
              else
                node->parse_string(value.dump());
            }
          else
            throw std::runtime_error(
              "Network node has no value to initialize.");
        }

      const auto iface =
        NodeObject::build_network_interface(detail::meta_any_ref(*node));
      if (!iface.arguments.empty() || !iface.inputs.empty() ||
          !iface.outputs.empty())
        node->override_interface(iface.arguments, iface.inputs, iface.outputs);

      auto      &value = detail::meta_any_ref(*node);
      const auto ptr   = value->template try_cast<std::shared_ptr<Network>>();
      if (ptr == nullptr)
        throw std::runtime_error(
          "Could not cast meta_any to shared_ptr<Network>.");
      if (!*ptr)
        throw std::runtime_error("Network object is not initialized.");

      auto      &network  = **ptr;
      const auto args_map = network.get_arguments();

      if (args.size() != args_map.size())
        throw std::runtime_error("Wrong number of arguments.");

      const auto                info = node->get_info();
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
                  target_node->bind_input(entry.input_index, entry.previous);
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
          for (size_t i = 0; i < args_map.size(); ++i)
            {
              const auto &entry = args_map[i];
              const auto &arg   = args[i];
              if (!arg || !arg->ready())
                continue;

              if ((entry.type & ConnectionType::input) == ConnectionType::none)
                continue;

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

              auto node_id = network.add_node(arg);
              added_node_ids.push_back(node_id);
              restore_inputs.push_back({entry.node_id,
                                        input_index,
                                        target_node->get_input(input_index)});

              auto conn_id =
                network.add_connection(node_id, entry.node_id, 0, input_index);
              added_connection_ids.push_back(conn_id);

              if (entry.type == ConnectionType::pass_through)
                {
                  for (size_t out_index = 0;
                       out_index < output_to_argument.size();
                       ++out_index)
                    {
                      if (output_to_argument[out_index] ==
                          static_cast<unsigned int>(entry.argument_index))
                        {
                          node->bind_output(static_cast<unsigned int>(
                                              out_index),
                                            arg->get_output(0));
                          break;
                        }
                    }
                }
            }

          if (!network.get_inputs().empty())
            throw std::runtime_error(
              "Network has unconnected inputs after binding arguments.");

          network.run();
          const auto outputs = network.get_outputs();
          for (size_t out_index = 0; out_index < outputs.size(); ++out_index)
            node->bind_output(static_cast<unsigned int>(out_index),
                              network.get_output(out_index));
          cleanup();
        }
      catch (...)
        {
          cleanup();
          throw;
        }

      return value;
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
  }



  CORAL_IMPL_INLINE auto
  build_network_interface(const std::shared_ptr<Network> &net)
    -> detail::NodeInterface
  {
    if (!net)
      throw std::runtime_error("Network interface builder received null.");
    return net->build_interface();
  }

} // namespace coral

#undef CORAL_IMPL_INLINE

#endif
