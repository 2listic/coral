#ifndef coral_network_h
#define coral_network_h

#include <nlohmann/json.hpp>
#include <taskflow/taskflow.hpp>

#include <fstream>
#include <memory>
#include <unordered_map>

#include "coral.h"

namespace coral
{
  class Connection
  {
  public:
    std::weak_ptr<NodeObject> in;
    std::weak_ptr<NodeObject> out;

    Connection(const std::shared_ptr<NodeObject> &_in,
               const std::shared_ptr<NodeObject> &_out)
      : in(_in)
      , out(_out)
    {}
  };

  class Network
  {
  private:
    std::unordered_map<int, std::shared_ptr<NodeObject>> nodes;

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
      for (const auto &[key, value] : json_data["nodes"].items())
        {
          int         id   = value["id"];
          std::string name = value["name"];
          auto        node = make_node(name);

          // Skip setting data if NodeObject does not support it

          nodes[id] = node;
        }

      for (const auto &[key, value] : json_data["nodes"].items())
        {
          int id = value["id"];
          if (value.contains("inputs"))
            {
              for (const auto &[input_key, input_value] :
                   value["inputs"].items())
                {
                  for (const auto &connection : input_value["connections"])
                    {
                      int source_id = connection["node"];
                      connect_nodes(source_id, id);
                    }
                }
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
    connect_nodes(int inId, int outId) -> bool
    {
      auto in  = nodes[inId];
      auto out = nodes[outId];
      if (!in || !out)
        return false;

      // Add dependency: input node must execute before the current node
      auto in_task  = taskflow.emplace([in]() { (*in)(); });
      auto out_task = taskflow.emplace([out]() { (*out)(); });
      in_task.precede(out_task);

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
  };
} // namespace coral
#endif