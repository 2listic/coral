#ifndef coral_network_h
#define coral_network_h

#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>

#include "coral.h"

namespace coral
{
  class Connection
  {
  public:
    // Basic connection properties
    int         source_id;
    int         target_id;
    std::string source_output;
    std::string target_input;

    // Optional reference to the actual node objects
    std::weak_ptr<NodeObject> source_node;
    std::weak_ptr<NodeObject> target_node;

    // Default constructor for deserialization
    Connection() = default;

    // Constructor with node IDs and ports
    Connection(int                _source_id,
               int                _target_id,
               const std::string &_source_output = "self",
               const std::string &_target_input  = "0")
      : source_id(_source_id)
      , target_id(_target_id)
      , source_output(_source_output)
      , target_input(_target_input)
    {}

    // Constructor with actual node objects and IDs
    Connection(int                                _source_id,
               int                                _target_id,
               const std::shared_ptr<NodeObject> &_source,
               const std::shared_ptr<NodeObject> &_target,
               const std::string                 &_source_output = "self",
               const std::string                 &_target_input  = "0")
      : source_id(_source_id)
      , target_id(_target_id)
      , source_output(_source_output)
      , target_input(_target_input)
      , source_node(_source)
      , target_node(_target)
    {}

    // Serialize connection to JSON
    [[nodiscard]] auto
    to_json() const -> nlohmann::json
    {
      nlohmann::json json;
      json["source"]        = std::to_string(source_id);
      json["target"]        = std::to_string(target_id);
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
      conn.source_id = std::stoi(json["source"].get<std::string>());
      conn.target_id = std::stoi(json["target"].get<std::string>());

      // Require source_output and target_input fields
      if (!json.contains("source_output") || !json.contains("target_input"))
        {
          throw std::runtime_error(
            "Connection JSON must contain 'source_output' and 'target_input' fields");
        }

      conn.source_output = json["source_output"].get<std::string>();
      conn.target_input  = json["target_input"].get<std::string>();

      return conn;
    }
  };

  class Network
  {
  private:
    std::unordered_map<int, std::shared_ptr<NodeObject>> nodes;
    std::unordered_map<int, tf::Task>                    node_tasks;

    // Store connections by their ID
    std::unordered_map<int, Connection> connections;

    tf::Executor executor;
    tf::Taskflow taskflow;

  public:
    void
    addNode(int id, const std::shared_ptr<NodeObject> &node)
    {
      nodes[id] = node;
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

          // Prepare node data - ensure type_hash exists for proper
          // deserialization
          nlohmann::json node_data = value;
          try
            {
              // Simple direct deserialization using the json conversion
              // operator
              nodes[id] = node_data.get<NodeObjectPtr>();
            }
          catch (const std::exception &e)
            {
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

              // Connect the nodes and store the connection with its metadata
              connect_nodes(conn.source_id,
                            conn.target_id,
                            conn.source_output,
                            conn.target_input,
                            conn_id);
            }
        }
    }

    void
    run()
    {
      executor.run(taskflow).wait();
    }

    void
    run_network()
    {
      executor.run(taskflow).wait();
    }

    void
    clear_network()
    {
      taskflow.clear();
      node_tasks.clear();
      connections.clear();
    }

    auto
    add_task(const std::function<void()> &task_function,
             const std::string           &name) -> tf::Task
    {
      return taskflow.emplace(task_function).name(name);
    }

    [[nodiscard]] auto
    get_node(int id) const -> std::shared_ptr<NodeObject>
    {
      auto it = nodes.find(id);
      return it != nodes.end() ? it->second : nullptr;
    }

    auto
    connect_nodes(int                inId,
                  int                outId,
                  const std::string &sourceOutput = "self",
                  const std::string &targetInput  = "0",
                  int                conn_id      = -1) -> bool
    {
      auto in  = nodes[inId];
      auto out = nodes[outId];
      if (!in || !out)
        return false;

      // Create or retrieve task for input node
      tf::Task in_task;
      if (node_tasks.count(inId) == 0)
        {
          in_task = taskflow.emplace([in]() { (*in)(); })
                      .name("node_" + std::to_string(inId));
          node_tasks[inId] = in_task;
        }
      else
        {
          in_task = node_tasks[inId];
        }

      // Create or retrieve task for output node
      tf::Task out_task;
      if (node_tasks.count(outId) == 0)
        {
          out_task = taskflow.emplace([out]() { (*out)(); })
                       .name("node_" + std::to_string(outId));
          node_tasks[outId] = out_task;
        }
      else
        {
          out_task = node_tasks[outId];
        }

      // Add dependency: input node must execute before the output node
      in_task.precede(out_task);

      // Create and store the connection object
      Connection conn(inId, outId, in, out, sourceOutput, targetInput);

      // If no connection ID was provided, generate one based on source and
      // target
      if (conn_id == -1)
        {
          // Simple generation of a connection ID
          conn_id = inId * 1000 + outId;
        }

      connections[conn_id] = conn;

      return true;
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
    output_dot(const std::string &filename) const
    {
      std::ofstream dot_file(filename);
      taskflow.dump(dot_file);
      dot_file.close();
    }

    // Get all connections in the network
    [[nodiscard]] auto
    get_connections() const -> const std::unordered_map<int, Connection> &
    {
      return connections;
    }

    // Get all nodes that are connected to a specific node (outgoing
    // connections)
    [[nodiscard]] auto
    get_connected_nodes(int nodeId) const -> std::vector<int>
    {
      std::vector<int> result;

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

    // Get all connection objects for a specific source node
    [[nodiscard]] auto
    get_node_connections(int nodeId) const -> std::vector<Connection>
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

    // Count the total number of connections in the network
    [[nodiscard]] auto
    connection_count() const -> size_t
    {
      return connections.size();
    }

    // Check if two nodes are connected (direct connection from inId to outId)
    [[nodiscard]] auto
    is_connected(int inId, int outId) const -> bool
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
        json[std::to_string(node_id)] = node;

      return json;
    }


    // Serialize the entire network to JSON
    [[nodiscard]] auto
    to_json() const -> nlohmann::json
    {
      nlohmann::json json;
      json["workflow"]["nodes"] = nlohmann::json::object();

      // Serialize nodes
      for (const auto &[id, node] : nodes)
        {
          // Direct serialization using json conversion
          nlohmann::json node_json = node;

          // Add workflow-specific fields
          node_json["id"] = id;
          if (node_json.contains("type_hash"))
            {
              node_json["typeHash"] = node_json["type_hash"];
            }
          node_json["outputs"] = {"self"};
          node_json["isValid"] = true;

          // Add to nodes object using ID as key
          json["workflow"]["nodes"][std::to_string(id)] = node_json;
        }

      // Serialize edges
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
} // namespace coral
#endif