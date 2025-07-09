#ifndef coral_network_h
#define coral_network_h

#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>

#include <chrono>
#include <fstream>
#include <iomanip>
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
    int source_id;
    int target_id;
    int source_output;
    int target_input;

    // Default constructor for deserialization
    Connection() = default;

    // Constructor with node IDs and ports
    Connection(int _source_id,
               int _target_id,
               int _source_output = 0,
               int _target_input  = 0)
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
      conn.source_id = json["source"].get<int>();
      conn.target_id = json["target"].get<int>();

      // Require source_output and target_input fields
      if (!json.contains("source_output") || !json.contains("target_input"))
        {
          throw std::runtime_error(
            "Connection JSON must contain 'source_output' and 'target_input' fields");
        }

      conn.source_output = json["source_output"].get<int>();
      conn.target_input  = json["target_input"].get<int>();

      return conn;
    }
  };

  class Network
  {
  private:
    std::map<int, std::shared_ptr<NodeObject>> nodes;
    std::map<int, tf::Task>                    node_tasks;

    // Store connections by their ID
    std::map<int, Connection> connections;

    tf::Executor executor;
    tf::Taskflow taskflow;

  public:
    void
    add_node(int id, const std::shared_ptr<NodeObject> &node)
    {
      nodes[id]      = node;
      node_tasks[id] = taskflow.emplace([node]() { (*node)(); })
                         .name("node_" + std::to_string(id));
    }

    int
    add_node(const std::shared_ptr<NodeObject> &node)
    {
      int id = nodes.empty() ? 1 : nodes.rbegin()->first + 1;
      add_node(id, node);
      return id;
    }


    void
    add_connection(int id, const Connection &conn)
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

      auto source_task = node_tasks[conn.source_id];
      auto target_task = node_tasks[conn.target_id];

      // Connect the source and target tasks
      source_task.precede(target_task);
    }

    void
    add_connection(int id,
                   int source_id,
                   int target_id,
                   int source_output,
                   int target_input)
    {
      Connection conn(source_id, target_id, source_output, target_input);
      add_connection(id, conn);
    }


    int
    add_connection(int source_id,
                   int target_id,
                   int source_output,
                   int target_input)
    {
      Connection conn(source_id, target_id, source_output, target_input);
      return add_connection(conn);
    }

    int
    add_connection(const Connection &conn)
    {
      int id = connections.empty() ? 1 : connections.rbegin()->first + 1;
      add_connection(id, conn);
      return id;
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
              add_node(id, node_data.get<NodeObjectPtr>());
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
      nodes.clear();
      node_tasks.clear();
      connections.clear();
      taskflow.clear();
    }


    [[nodiscard]] auto
    get_node(int id) const -> std::shared_ptr<NodeObject>
    {
      auto it = nodes.find(id);
      return it != nodes.end() ? it->second : nullptr;
    }

    [[nodiscard]] auto
    get_task(int id) const -> tf::Task
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
    output_dot(const std::string &filename) const
    {
      std::ofstream dot_file(filename);
      taskflow.dump(dot_file);
      dot_file.close();
    }

    // Get all nodes that are connected to a specific node (outgoing
    // connections)
    [[nodiscard]] auto
    get_connected_nodes(int nodeId) const -> std::vector<int>
    {
      std::vector<int> result;

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
    get_node_connections(int nodeId) const -> std::vector<Connection>
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
} // namespace coral
#endif