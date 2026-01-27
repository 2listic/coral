#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if __APPLE__
#  include <OpenGL/gl3.h>
#else
#  include <GL/gl.h>
#endif

#include <imgui.h>
#include <imgui_node_editor.h>
#include <nlohmann/json.hpp>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "coral_editor/node_editor_types.h"

namespace ed = ax::NodeEditor;
using json   = nlohmann::json;

#if CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
#  include "coral.h"
#  include "coral_network.h"
#  include "register_types.h"
#endif

struct PinMeta
{
  std::string label;
  std::string type;
  std::string connection_type;
  bool        is_self = false;
};

struct NodeColors
{
  ImVec4 header;
  ImVec4 body;
  ImVec4 border;
  ImVec4 border_hover;
  ImVec4 border_selected;
};

struct NetworkInterface
{
  RegistryNodeType node_type;
  bool             valid = false;
};

static NetworkInterface
BuildNetworkInterface(const EditorState &state, const json &data);
static bool
GetNodeTypeForNode(const EditorState &state,
                   const EditorNode  &node,
                   RegistryNodeType  &out);
static void
UpdateTabIndex(EditorState &state);
static void
CreateTabsFromNetworkNodes(EditorState &state);
static void
SwitchToTab(EditorState &state, int tab_index);
static void
SyncNetworkNodesFromTabs(EditorState &state);

static void
LogDebugV(EditorState &state, int level, const char *fmt, va_list args)
{
  if (level > state.log_level)
    return;

  std::ofstream file(state.log_path.c_str(), std::ios::app);
  if (!file.is_open())
    return;

  auto    now        = std::chrono::system_clock::now();
  auto    now_time_t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_snapshot{};
#if defined(_WIN32)
  localtime_s(&tm_snapshot, &now_time_t);
#else
  localtime_r(&now_time_t, &tm_snapshot);
#endif

  char time_buf[32];
  std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_snapshot);
  file << "[" << time_buf << "] ";

  char message[1024];
  std::vsnprintf(message, sizeof(message), fmt, args);
  file << message << "\n";
}

static void
LogDebug(EditorState &state, int level, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  LogDebugV(state, level, fmt, args);
  va_end(args);
}

static void
LogDebug(EditorState &state, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  LogDebugV(state, 5, fmt, args);
  va_end(args);
}

static bool
GetPinMeta(const RegistryNodeType *node_type,
           ed::PinKind             kind,
           unsigned int            index,
           PinMeta                &out)
{
  if (!node_type)
    return false;

  const std::vector<int> &pins =
    (kind == ed::PinKind::Input) ? node_type->inputs : node_type->outputs;
  if (index >= pins.size())
    return false;

  const int arg_index = pins[index];
  if (arg_index < 0)
    {
      out.label           = "self";
      out.type            = node_type->type;
      out.connection_type = "self";
      out.is_self         = true;
      return true;
    }

  if (static_cast<size_t>(arg_index) >= node_type->arguments.size())
    {
      out.label           = "arg";
      out.type            = "";
      out.connection_type = "";
      out.is_self         = false;
      return true;
    }

  const auto &arg     = node_type->arguments[static_cast<size_t>(arg_index)];
  out.label           = !arg.name.empty() ? arg.name : "arg";
  out.type            = arg.type;
  out.connection_type = arg.connection_type;
  out.is_self         = false;
  return true;
}

static bool
GetPinMetaForNode(const EditorState &state,
                  unsigned int       node_id,
                  ed::PinKind        kind,
                  unsigned int       index,
                  PinMeta           &out)
{
  auto node_it = state.nodes.find(node_id);
  if (node_it == state.nodes.end())
    return false;

  RegistryNodeType node_type;
  if (!GetNodeTypeForNode(state, node_it->second, node_type))
    return false;

  return GetPinMeta(&node_type, kind, index, out);
}

static uint64_t
MakePinId(unsigned int coral_node_id, ed::PinKind kind, unsigned int index)
{
  const uint64_t editor_node_id = static_cast<uint64_t>(coral_node_id) + 1u;
  const uint64_t kind_bits      = (kind == ed::PinKind::Input) ? 1u : 2u;
  return (editor_node_id << 32) | (kind_bits << 24) |
         (static_cast<uint64_t>(index) & 0xFFFFFFu);
}

static bool
DecodePinId(ed::PinId     pin_id,
            unsigned int &coral_node_id,
            ed::PinKind  &kind,
            unsigned int &index)
{
  const uint64_t raw = pin_id.Get();
  if (raw == 0)
    return false;
  const unsigned int editor_node_id = static_cast<unsigned int>(raw >> 32);
  if (editor_node_id == 0)
    return false;
  coral_node_id            = editor_node_id - 1u;
  const uint64_t kind_bits = (raw >> 24) & 0xFFu;
  if (kind_bits == 1u)
    kind = ed::PinKind::Input;
  else if (kind_bits == 2u)
    kind = ed::PinKind::Output;
  else
    return false;
  index = static_cast<unsigned int>(raw & 0xFFFFFFu);
  return true;
}

static ImVec4
GetPinDotColor(ed::PinKind kind, const PinMeta &meta)
{
  if (meta.is_self)
    return ImVec4(0.2f, 0.9f, 0.3f, 1.0f);
  if (meta.connection_type == "pass_through")
    return ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
  if (kind == ed::PinKind::Input)
    return ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
  return ImVec4(0.95f, 0.5f, 0.2f, 1.0f);
}

static NodeColors
GetNodeColors(const RegistryNodeType *node_type)
{
  const std::string category = node_type ? node_type->node_type : "";
  const bool        is_method_or_function =
    category.find("method") != std::string::npos ||
    category.find("function") != std::string::npos;
  if (category == "constructor" || category == "empty_constructor")
    {
      return {ImVec4(0.20f, 0.50f, 0.65f, 0.95f),
              ImVec4(0.12f, 0.16f, 0.20f, 0.95f),
              ImVec4(0.30f, 0.65f, 0.80f, 0.95f),
              ImVec4(0.40f, 0.75f, 0.90f, 1.0f),
              ImVec4(0.55f, 0.85f, 1.0f, 1.0f)};
    }
  if (category == "elementary_constructor")
    {
      return {ImVec4(0.80f, 0.48f, 0.16f, 0.95f),
              ImVec4(0.18f, 0.12f, 0.08f, 0.95f),
              ImVec4(0.92f, 0.62f, 0.25f, 0.95f),
              ImVec4(0.98f, 0.72f, 0.35f, 1.0f),
              ImVec4(1.0f, 0.80f, 0.50f, 1.0f)};
    }
  if (category == "network")
    {
      return {ImVec4(0.45f, 0.35f, 0.78f, 0.95f),
              ImVec4(0.16f, 0.14f, 0.22f, 0.95f),
              ImVec4(0.62f, 0.52f, 0.90f, 0.95f),
              ImVec4(0.74f, 0.64f, 1.0f, 1.0f),
              ImVec4(0.82f, 0.74f, 1.0f, 1.0f)};
    }
  if (is_method_or_function)
    {
      return {ImVec4(0.25f, 0.60f, 0.30f, 0.95f),
              ImVec4(0.10f, 0.16f, 0.10f, 0.95f),
              ImVec4(0.40f, 0.72f, 0.45f, 0.95f),
              ImVec4(0.52f, 0.82f, 0.56f, 1.0f),
              ImVec4(0.62f, 0.90f, 0.66f, 1.0f)};
    }

  return {ImVec4(0.35f, 0.36f, 0.38f, 0.95f),
          ImVec4(0.14f, 0.15f, 0.16f, 0.95f),
          ImVec4(0.50f, 0.52f, 0.55f, 0.95f),
          ImVec4(0.60f, 0.63f, 0.66f, 1.0f),
          ImVec4(0.70f, 0.73f, 0.76f, 1.0f)};
}

static bool
LoadJsonFile(const std::string &path, json &out, std::string &error)
{
  std::ifstream file(path);
  if (!file.is_open())
    {
      error = "Failed to open " + path;
      return false;
    }

  try
    {
      file >> out;
    }
  catch (const std::exception &e)
    {
      error = std::string("JSON parse error: ") + e.what();
      return false;
    }

  return true;
}

static bool
SaveJsonFile(const std::string &path, const json &data, std::string &error)
{
  std::ofstream file(path);
  if (!file.is_open())
    {
      error = "Failed to open " + path;
      return false;
    }

  try
    {
      file << data.dump(2) << std::endl;
    }
  catch (const std::exception &e)
    {
      error = std::string("JSON write error: ") + e.what();
      return false;
    }

  return true;
}

static void
UpdateNextIds(EditorState &state)
{
  state.next_node_id = 1;
  state.next_link_id = 1;
  if (!state.nodes.empty())
    state.next_node_id = state.nodes.rbegin()->first + 1;
  if (!state.links.empty())
    state.next_link_id = state.links.rbegin()->first + 1;
}

static const RegistryNodeType *
FindNodeType(const EditorState &state, const std::string &type)
{
  auto it = state.registry.find(type);
  if (it != state.registry.end())
    return &it->second;
  return nullptr;
}

static bool
GetNodeTypeForNode(const EditorState &state,
                   const EditorNode  &node,
                   RegistryNodeType  &out)
{
  const RegistryNodeType *base = FindNodeType(state, node.type);
  if (!base)
    return false;

  if (base->node_type != "network")
    {
      out = *base;
      return true;
    }

  if (node.value.empty())
    {
      out = *base;
      return true;
    }

  try
    {
      json             data  = json::parse(node.value);
      NetworkInterface iface = BuildNetworkInterface(state, data);
      if (iface.valid)
        {
          out      = iface.node_type;
          out.type = base->type;
          return true;
        }
    }
  catch (const std::exception &)
    {}

  out = *base;
  return true;
}

static void
SaveActiveTab(EditorState &state)
{
  if (state.tabs.empty() || state.active_tab < 0 ||
      state.active_tab >= static_cast<int>(state.tabs.size()))
    return;

  NetworkTab &tab  = state.tabs[static_cast<size_t>(state.active_tab)];
  tab.nodes        = state.nodes;
  tab.links        = state.links;
  tab.next_node_id = state.next_node_id;
  tab.next_link_id = state.next_link_id;
}

static void
LoadTab(EditorState &state, int tab_index)
{
  if (tab_index < 0 || tab_index >= static_cast<int>(state.tabs.size()))
    return;

  const NetworkTab &tab = state.tabs[static_cast<size_t>(tab_index)];
  state.nodes           = tab.nodes;
  state.links           = tab.links;
  state.next_node_id    = tab.next_node_id;
  state.next_link_id    = tab.next_link_id;
  state.active_tab      = tab_index;
}

static bool
LoadRegistry(EditorState &state)
{
  json        data;
  std::string error;
  if (!LoadJsonFile(state.registry_path, data, error))
    {
      state.status = error;
      return false;
    }

  state.registry.clear();
  for (auto it = data.begin(); it != data.end(); ++it)
    {
      const std::string key   = it.key();
      const auto       &value = it.value();

      RegistryNodeType node_type;
      node_type.type = key;
      if (value.contains("type"))
        node_type.type = value.at("type").get<std::string>();
      node_type.node_type = value.value("node_type", "");

      if (value.contains("arguments") && value.at("arguments").is_array())
        {
          for (const auto &arg : value.at("arguments"))
            {
              RegistryArgument entry;
              entry.name            = arg.value("name", "");
              entry.type            = arg.value("type", "");
              entry.connection_type = arg.value("connection_type", "");
              node_type.arguments.push_back(entry);
            }
        }

      if (value.contains("inputs") && value.at("inputs").is_array())
        {
          for (const auto &input : value.at("inputs"))
            node_type.inputs.push_back(input.get<int>());
        }

      if (value.contains("outputs") && value.at("outputs").is_array())
        {
          for (const auto &output : value.at("outputs"))
            node_type.outputs.push_back(output.get<int>());
        }

      if (value.contains("value"))
        {
          if (value.at("value").is_string())
            node_type.default_value = value.at("value").get<std::string>();
          else
            node_type.default_value = value.at("value").dump();
        }

      state.registry[node_type.type] = node_type;
    }

  state.status =
    "Loaded registry: " + std::to_string(state.registry.size()) + " types";
  return true;
}

static NetworkInterface
BuildNetworkInterface(const EditorState &state, const json &data)
{
  NetworkInterface iface;
  iface.node_type.type      = "coral::Network";
  iface.node_type.node_type = "network";

  if (!data.contains("workflow"))
    return iface;
  const auto &workflow = data.at("workflow");
  if (!workflow.contains("nodes") || !workflow.contains("edges"))
    return iface;

  struct NodeInfo
  {
    std::string type;
  };
  struct EdgeInfo
  {
    unsigned int source        = 0;
    unsigned int target        = 0;
    unsigned int source_output = 0;
    unsigned int target_input  = 0;
  };

  std::map<unsigned int, NodeInfo> nodes;
  for (auto it = workflow.at("nodes").begin(); it != workflow.at("nodes").end();
       ++it)
    {
      const unsigned int id = static_cast<unsigned int>(std::stoul(it.key()));
      const auto        &value = it.value();
      NodeInfo           info;
      info.type = value.value("type", "");
      nodes[id] = info;
    }

  std::vector<EdgeInfo> edges;
  for (auto it = workflow.at("edges").begin(); it != workflow.at("edges").end();
       ++it)
    {
      const auto &value = it.value();
      EdgeInfo    edge;
      edge.source        = value.value("source", 0u);
      edge.target        = value.value("target", 0u);
      edge.source_output = value.value("source_output", 0u);
      edge.target_input  = value.value("target_input", 0u);
      edges.push_back(edge);
    }

  struct Dangling
  {
    int         input_index  = -1;
    int         output_index = -1;
    std::string type;
    std::string name;
  };

  for (const auto &[node_id, info] : nodes)
    {
      auto reg_it = state.registry.find(info.type);
      if (reg_it == state.registry.end())
        continue;
      const RegistryNodeType &node_type = reg_it->second;
      std::map<int, Dangling> by_argument;

      for (unsigned int i = 0; i < node_type.inputs.size(); ++i)
        {
          const int arg_index = node_type.inputs[i];
          bool      connected = false;
          for (const auto &edge : edges)
            {
              if (edge.target == node_id && edge.target_input == i)
                {
                  connected = true;
                  break;
                }
            }
          if (!connected)
            {
              auto &entry       = by_argument[arg_index];
              entry.input_index = static_cast<int>(i);
              if (arg_index >= 0 &&
                  static_cast<size_t>(arg_index) < node_type.arguments.size())
                {
                  entry.type = node_type.arguments[arg_index].type;
                  entry.name = node_type.arguments[arg_index].name;
                }
            }
        }

      for (unsigned int i = 0; i < node_type.outputs.size(); ++i)
        {
          const int arg_index = node_type.outputs[i];
          if (arg_index < 0)
            continue;
          bool connected = false;
          for (const auto &edge : edges)
            {
              if (edge.source == node_id && edge.source_output == i)
                {
                  connected = true;
                  break;
                }
            }
          if (!connected)
            {
              auto &entry        = by_argument[arg_index];
              entry.output_index = static_cast<int>(i);
              if (static_cast<size_t>(arg_index) < node_type.arguments.size())
                {
                  entry.type = node_type.arguments[arg_index].type;
                  entry.name = node_type.arguments[arg_index].name;
                }
            }
        }

      for (auto &pair : by_argument)
        {
          const int  arg_index  = pair.first;
          Dangling  &entry      = pair.second;
          const bool has_input  = entry.input_index >= 0;
          const bool has_output = entry.output_index >= 0;
          if (arg_index < 0)
            continue;
          if (entry.type.empty())
            continue;

          RegistryArgument arg;
          arg.name = entry.name;
          arg.type = entry.type;
          if (has_input && has_output)
            arg.connection_type = "pass_through";
          else if (has_input)
            arg.connection_type = "input";
          else if (has_output)
            arg.connection_type = "output";
          else
            continue;

          const unsigned int arg_pos =
            static_cast<unsigned int>(iface.node_type.arguments.size());
          iface.node_type.arguments.push_back(arg);
          if (has_input)
            iface.node_type.inputs.push_back(static_cast<int>(arg_pos));
          if (has_output)
            iface.node_type.outputs.push_back(static_cast<int>(arg_pos));
        }
    }

  iface.valid = true;
  return iface;
}

static ImVec2
ComputeNodePosition(int node_index)
{
  const float x = 40.0f + static_cast<float>(node_index % 4) * 280.0f;
  const float y = 40.0f + static_cast<float>(node_index / 4) * 180.0f;
  return ImVec2(x, y);
}

static bool
LoadNetworkFromJson(EditorState                        &state,
                    const json                         &data,
                    std::map<unsigned int, EditorNode> &nodes,
                    std::map<unsigned int, EditorLink> &links,
                    unsigned int                       &next_node_id,
                    unsigned int                       &next_link_id,
                    std::string                        &error)
{
  if (!data.contains("workflow"))
    {
      error = "Network JSON missing workflow";
      return false;
    }

  const auto &workflow = data.at("workflow");
  if (!workflow.contains("nodes"))
    {
      error = "Network JSON missing workflow.nodes";
      return false;
    }

  nodes.clear();
  links.clear();

  int node_index = 0;
  for (auto it = workflow.at("nodes").begin(); it != workflow.at("nodes").end();
       ++it)
    {
      const std::string key   = it.key();
      const auto       &value = it.value();

      const unsigned int id = static_cast<unsigned int>(std::stoul(key));
      EditorNode         node;
      node.id               = id;
      node.type             = value.value("type", "");
      node.name             = value.value("name", "");
      node.name_buffer_init = false;
      if (value.contains("value"))
        {
          if (value.at("value").is_string())
            node.value = value.at("value").get<std::string>();
          else
            node.value = value.at("value").dump();
        }
      else
        {
          auto reg_it = state.registry.find(node.type);
          if (reg_it != state.registry.end())
            node.value = reg_it->second.default_value;
        }
      if (!node.value.empty())
        {
          std::snprintf(node.value_buffer,
                        sizeof(node.value_buffer),
                        "%s",
                        node.value.c_str());
          node.value_buffer_init = true;
        }

      bool   has_position = false;
      ImVec2 position(0.0f, 0.0f);
      if (value.contains("position"))
        {
          const auto &pos = value.at("position");
          if (pos.contains("x") && pos.contains("y"))
            {
              position.x   = pos.value("x", 0.0f);
              position.y   = pos.value("y", 0.0f);
              has_position = true;
            }
        }

      nodes[id] = node;
      nodes[id].desired_position =
        has_position ? position : ComputeNodePosition(node_index++);
      nodes[id].needs_position = true;
    }

  if (workflow.contains("edges"))
    {
      for (auto it = workflow.at("edges").begin();
           it != workflow.at("edges").end();
           ++it)
        {
          const std::string key   = it.key();
          const auto       &value = it.value();

          const unsigned int id = static_cast<unsigned int>(std::stoul(key));
          EditorLink         link;
          link.id            = id;
          link.source_node   = value.value("source", 0u);
          link.target_node   = value.value("target", 0u);
          link.source_output = value.value("source_output", 0u);
          link.target_input  = value.value("target_input", 0u);
          links[id]          = link;
        }
    }

  next_node_id = 1;
  next_link_id = 1;
  if (!nodes.empty())
    next_node_id = nodes.rbegin()->first + 1;
  if (!links.empty())
    next_link_id = links.rbegin()->first + 1;

  return true;
}

static bool
LoadNetwork(EditorState &state)
{
  json        data;
  std::string error;
  if (!LoadJsonFile(state.network_path, data, error))
    {
      state.status = error;
      return false;
    }

  if (state.tabs.empty())
    {
      NetworkTab main_tab;
      main_tab.name = "Main";
      state.tabs.push_back(main_tab);
      state.active_tab = 0;
    }
  if (state.active_tab < 0 ||
      state.active_tab >= static_cast<int>(state.tabs.size()))
    state.active_tab = 0;

  std::string load_error;
  if (!LoadNetworkFromJson(
        state,
        data,
        state.tabs[static_cast<size_t>(state.active_tab)].nodes,
        state.tabs[static_cast<size_t>(state.active_tab)].links,
        state.tabs[static_cast<size_t>(state.active_tab)].next_node_id,
        state.tabs[static_cast<size_t>(state.active_tab)].next_link_id,
        load_error))
    {
      state.status = load_error;
      return false;
    }

  LoadTab(state, state.active_tab);
  state.force_tab_select = true;
  ++state.tab_bar_uid;
  UpdateTabIndex(state);
  if (state.active_tab == 0)
    {
      CreateTabsFromNetworkNodes(state);
      UpdateTabIndex(state);
    }
  state.status = "Loaded network into " +
                 state.tabs[static_cast<size_t>(state.active_tab)].name + ": " +
                 std::to_string(state.nodes.size()) + " nodes";
  return true;
}

static json
BuildNetworkJsonFrom(const std::map<unsigned int, EditorNode>      &nodes,
                     const std::map<unsigned int, EditorLink>      &links,
                     const std::map<std::string, RegistryNodeType> &registry)
{
  json result;
  json nodes_json = json::object();
  json edges_json = json::object();

  for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
      const unsigned int id   = it->first;
      const EditorNode  &node = it->second;

      json node_json;
      node_json["type"] = node.type;
      if (!node.name.empty() && node.name != node.type)
        node_json["name"] = node.name;
      node_json["position"]["x"] = node.desired_position.x;
      node_json["position"]["y"] = node.desired_position.y;

      const RegistryNodeType *node_type = nullptr;
      auto                    reg_it    = registry.find(node.type);
      if (reg_it != registry.end())
        node_type = &reg_it->second;

      if (node_type && (node_type->node_type == "elementary_constructor" ||
                        node_type->node_type == "network"))
        {
          node_json["value"] = node.value;
        }

      nodes_json[std::to_string(id)] = node_json;
    }

  for (auto it = links.begin(); it != links.end(); ++it)
    {
      const unsigned int id   = it->first;
      const EditorLink  &link = it->second;

      json link_json;
      link_json["source"]            = link.source_node;
      link_json["target"]            = link.target_node;
      link_json["source_output"]     = link.source_output;
      link_json["target_input"]      = link.target_input;
      edges_json[std::to_string(id)] = link_json;
    }

  result["workflow"]["nodes"] = nodes_json;
  result["workflow"]["edges"] = edges_json;
  result["version"]           = 1;
  result["author"]            = "coral-editor";

  auto              now        = std::chrono::system_clock::now();
  auto              now_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&now_time_t), "%FT%T.000Z");
  result["date_time_utc"] = ss.str();

  return result;
}

#if CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
static void
EnsureCoralTypesRegistered()
{
  static bool registered = false;
  if (registered)
    return;
  coral::register_all_types();
  registered = true;
}

static bool
LoadTriangulationForEditorNode(
  EditorState                        &state,
  const NetworkTab                   &tab,
  const unsigned int                  node_id,
  dealii::Triangulation<2, 2>        &out,
  std::map<unsigned int, dealii::Triangulation<2, 2>::active_cell_iterator>
    &cell_by_index,
  std::string &error)
{
  try
    {
      EnsureCoralTypesRegistered();
      json data = BuildNetworkJsonFrom(tab.nodes, tab.links, state.registry);
      coral::Network net;
      net.from_json(data);
      net.run();

      auto node = net.get_node(node_id);
      if (!node)
        {
          error = "Node not found in executed network.";
          return false;
        }

      const auto &tria_ref = node->get<dealii::Triangulation<2, 2>>();
      out.clear();
      out.copy_triangulation(tria_ref);

      cell_by_index.clear();
      for (auto cell = out.begin_active(); cell != out.end(); ++cell)
        {
          cell_by_index[cell->active_cell_index()] = cell;
        }

      return true;
    }
  catch (const std::exception &e)
    {
      error = e.what();
      return false;
    }
}

static void
RebuildMeshToolCaches(EditorState::MeshToolState &tool)
{
  tool.cell_tris.clear();
  tool.boundary_segs.clear();
  tool.boundary_counts.clear();
  tool.material_counts.clear();
  tool.selected_boundary_faces.clear();
  tool.selected_cells.clear();
  tool.selected_boundary_id = -1;
  tool.selected_material_id = -1;

  for (auto cell = tool.tria.begin_active(); cell != tool.tria.end(); ++cell)
    {
      const unsigned int cell_index =
        static_cast<unsigned int>(cell->active_cell_index());
      const unsigned int material_id =
        static_cast<unsigned int>(cell->material_id());
      tool.material_counts[material_id] += 1u;

      const unsigned int n_vertices = cell->reference_cell().n_vertices();
      auto               get_v      = [&](unsigned int v) -> ImVec2 {
        const auto p = cell->vertex(v);
        return ImVec2(static_cast<float>(p[0]), static_cast<float>(p[1]));
      };

      if (n_vertices == 3)
        {
          tool.cell_tris.push_back(
            {get_v(0), get_v(1), get_v(2), cell_index, material_id});
        }
      else if (n_vertices == 4)
        {
          // deal.II quadrilateral vertices are typically stored in lexicographic
          // order (0:(0,0), 1:(1,0), 2:(0,1), 3:(1,1)), which is not a cyclic
          // polygon order. Reorder to a consistent loop: 0-1-3-2.
          const ImVec2 v0 = get_v(0);
          const ImVec2 v1 = get_v(1);
          const ImVec2 v2 = get_v(3);
          const ImVec2 v3 = get_v(2);
          tool.cell_tris.push_back({v0, v1, v2, cell_index, material_id});
          tool.cell_tris.push_back({v0, v2, v3, cell_index, material_id});
        }

      for (unsigned int f = 0; f < cell->n_faces(); ++f)
        {
          const auto face = cell->face(f);
          if (!face->at_boundary())
            continue;

          const unsigned int boundary_id =
            static_cast<unsigned int>(face->boundary_id());
          tool.boundary_counts[boundary_id] += 1u;

          const auto p0 = face->vertex(0);
          const auto p1 = face->vertex(1);
          tool.boundary_segs.push_back(
            {ImVec2(static_cast<float>(p0[0]), static_cast<float>(p0[1])),
             ImVec2(static_cast<float>(p1[0]), static_cast<float>(p1[1])),
             cell_index,
             f,
             boundary_id});
        }
    }

  tool.view_initialized = false;
}

static ImVec2
WorldToScreen(const EditorState::MeshToolState &tool,
              const ImVec2                     canvas_min,
              const ImVec2                     canvas_size,
              const ImVec2                     world)
{
  const ImVec2 canvas_center(canvas_min.x + 0.5f * canvas_size.x,
                             canvas_min.y + 0.5f * canvas_size.y);
  const ImVec2 delta(world.x - tool.view_center.x,
                     world.y - tool.view_center.y);
  return ImVec2(canvas_center.x + delta.x * tool.view_scale,
                canvas_center.y - delta.y * tool.view_scale);
}

static ImVec2
ScreenToWorld(const EditorState::MeshToolState &tool,
              const ImVec2                     canvas_min,
              const ImVec2                     canvas_size,
              const ImVec2                     screen)
{
  const ImVec2 canvas_center(canvas_min.x + 0.5f * canvas_size.x,
                             canvas_min.y + 0.5f * canvas_size.y);
  const ImVec2 delta(screen.x - canvas_center.x,
                     screen.y - canvas_center.y);
  return ImVec2(tool.view_center.x + delta.x / tool.view_scale,
                tool.view_center.y - delta.y / tool.view_scale);
}

static float
DistPointToSegSq(const ImVec2 p, const ImVec2 a, const ImVec2 b)
{
  const ImVec2 ab(b.x - a.x, b.y - a.y);
  const ImVec2 ap(p.x - a.x, p.y - a.y);
  const float  ab_len_sq = ab.x * ab.x + ab.y * ab.y;
  if (ab_len_sq <= 1e-12f)
    {
      const float dx = p.x - a.x;
      const float dy = p.y - a.y;
      return dx * dx + dy * dy;
    }
  float t = (ap.x * ab.x + ap.y * ab.y) / ab_len_sq;
  t       = std::max(0.0f, std::min(1.0f, t));
  const ImVec2 proj(a.x + t * ab.x, a.y + t * ab.y);
  const float  dx = p.x - proj.x;
  const float  dy = p.y - proj.y;
  return dx * dx + dy * dy;
}

struct Box2f
{
  ImVec2 min;
  ImVec2 max;
};

static Box2f
MakeBoxFromScreen(const ImVec2 a, const ImVec2 b)
{
  return {ImVec2(std::min(a.x, b.x), std::min(a.y, b.y)),
          ImVec2(std::max(a.x, b.x), std::max(a.y, b.y))};
}

static bool
PointInBox(const ImVec2 p, const Box2f &box)
{
  return p.x >= box.min.x && p.x <= box.max.x && p.y >= box.min.y &&
         p.y <= box.max.y;
}

static bool
SegmentIntersectsSegment(const ImVec2 a,
                         const ImVec2 b,
                         const ImVec2 c,
                         const ImVec2 d)
{
  auto cross = [](const ImVec2 u, const ImVec2 v) {
    return u.x * v.y - u.y * v.x;
  };
  const ImVec2 r(b.x - a.x, b.y - a.y);
  const ImVec2 s(d.x - c.x, d.y - c.y);
  const ImVec2 cma(a.x - c.x, a.y - c.y);

  const float rxs = cross(r, s);
  const float qpxr = cross(ImVec2(c.x - a.x, c.y - a.y), r);

  if (std::fabs(rxs) < 1e-12f && std::fabs(qpxr) < 1e-12f)
    {
      auto dot = [](const ImVec2 u, const ImVec2 v) { return u.x * v.x + u.y * v.y; };
      const float rr = dot(r, r);
      if (rr < 1e-12f)
        return false;
      const float t0 = dot(ImVec2(c.x - a.x, c.y - a.y), r) / rr;
      const float t1 = dot(ImVec2(d.x - a.x, d.y - a.y), r) / rr;
      const float tmin = std::min(t0, t1);
      const float tmax = std::max(t0, t1);
      return tmax >= 0.0f && tmin <= 1.0f;
    }

  if (std::fabs(rxs) < 1e-12f)
    return false;

  const float t = cross(ImVec2(c.x - a.x, c.y - a.y), s) / rxs;
  const float u = cross(ImVec2(c.x - a.x, c.y - a.y), r) / rxs;
  return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

static bool
SegmentIntersectsBox(const ImVec2 a, const ImVec2 b, const Box2f &box)
{
  if (PointInBox(a, box) || PointInBox(b, box))
    return true;
  const ImVec2 r0(box.min.x, box.min.y);
  const ImVec2 r1(box.max.x, box.min.y);
  const ImVec2 r2(box.max.x, box.max.y);
  const ImVec2 r3(box.min.x, box.max.y);
  if (SegmentIntersectsSegment(a, b, r0, r1))
    return true;
  if (SegmentIntersectsSegment(a, b, r1, r2))
    return true;
  if (SegmentIntersectsSegment(a, b, r2, r3))
    return true;
  if (SegmentIntersectsSegment(a, b, r3, r0))
    return true;
  return false;
}

static bool
PointInTri(const ImVec2 p, const ImVec2 a, const ImVec2 b, const ImVec2 c);

static bool
TriIntersectsBox(const ImVec2 a, const ImVec2 b, const ImVec2 c, const Box2f &box)
{
  if (PointInBox(a, box) || PointInBox(b, box) || PointInBox(c, box))
    return true;

  const ImVec2 r0(box.min.x, box.min.y);
  const ImVec2 r1(box.max.x, box.min.y);
  const ImVec2 r2(box.max.x, box.max.y);
  const ImVec2 r3(box.min.x, box.max.y);

  if (PointInTri(r0, a, b, c) || PointInTri(r1, a, b, c) ||
      PointInTri(r2, a, b, c) || PointInTri(r3, a, b, c))
    return true;

  if (SegmentIntersectsBox(a, b, box))
    return true;
  if (SegmentIntersectsBox(b, c, box))
    return true;
  if (SegmentIntersectsBox(c, a, box))
    return true;

  return false;
}

static bool
PointInTri(const ImVec2 p, const ImVec2 a, const ImVec2 b, const ImVec2 c)
{
  const auto sign = [](const ImVec2 p1, const ImVec2 p2, const ImVec2 p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
  };
  const float d1 = sign(p, a, b);
  const float d2 = sign(p, b, c);
  const float d3 = sign(p, c, a);
  const bool  has_neg = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
  const bool  has_pos = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
  return !(has_neg && has_pos);
}

static void
InitMeshToolView(EditorState::MeshToolState &tool, const ImVec2 canvas_size)
{
  if (tool.cell_tris.empty() && tool.boundary_segs.empty())
    return;

  float min_x = std::numeric_limits<float>::infinity();
  float min_y = std::numeric_limits<float>::infinity();
  float max_x = -std::numeric_limits<float>::infinity();
  float max_y = -std::numeric_limits<float>::infinity();

  auto consider = [&](const ImVec2 p) {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  };

  for (const auto &tri : tool.cell_tris)
    {
      consider(tri.p0);
      consider(tri.p1);
      consider(tri.p2);
    }
  for (const auto &seg : tool.boundary_segs)
    {
      consider(seg.p0);
      consider(seg.p1);
    }

  tool.view_center = ImVec2(0.5f * (min_x + max_x), 0.5f * (min_y + max_y));
  const float span_x  = std::max(1e-6f, max_x - min_x);
  const float span_y  = std::max(1e-6f, max_y - min_y);
  const float scale_x = (canvas_size.x * 0.85f) / span_x;
  const float scale_y = (canvas_size.y * 0.85f) / span_y;
  tool.view_scale     = std::max(10.0f, std::min(scale_x, scale_y));
  tool.view_initialized = true;
}

static void
FitMeshToolView(EditorState::MeshToolState &tool, const ImVec2 canvas_size)
{
  tool.view_initialized = false;
  InitMeshToolView(tool, canvas_size);
}

static void
DrawMeshTool(EditorState &state)
{
  auto &tool = state.mesh_tool;
  if (!tool.open)
    return;

  ImGui::SetNextWindowSize(ImVec2(1040.0f, 720.0f), ImGuiCond_FirstUseEver);
  if (!ImGui::BeginPopupModal("Mesh Tool", &tool.open))
    return;

  if (tool.pending_load)
    {
      tool.pending_load = false;
      tool.load_error.clear();
      tool.has_tria = false;
      tool.cell_by_index.clear();

      if (tool.target_tab_index < 0 ||
          tool.target_tab_index >= static_cast<int>(state.tabs.size()))
        {
          tool.load_error = "Invalid tab index.";
        }
      else
        {
          auto &tab = state.tabs[static_cast<size_t>(tool.target_tab_index)];
          if (LoadTriangulationForEditorNode(state,
                                             tab,
                                             tool.target_node_id,
                                             tool.tria,
                                             tool.cell_by_index,
                                             tool.load_error))
            {
              if (tool.tria.n_active_cells() == 0)
                {
                  state.console_output +=
                    "Mesh Tool: triangulation is empty; nothing to select.\n";
                  state.console_scroll_to_bottom = true;
                  tool.open                      = false;
                  ImGui::CloseCurrentPopup();
                  ImGui::EndPopup();
                  return;
                }
              tool.has_tria = true;
              RebuildMeshToolCaches(tool);
            }
        }
    }

  if (!tool.load_error.empty())
    {
      ImGui::TextUnformatted("Could not load triangulation:");
      ImGui::Separator();
      ImGui::TextWrapped("%s", tool.load_error.c_str());
      if (ImGui::Button("Close"))
        tool.open = false;
      ImGui::EndPopup();
      return;
    }

  const float splitter_width = 6.0f;
  const float min_left       = 240.0f;
  const float min_canvas     = 420.0f;
  const float full_width     = ImGui::GetContentRegionAvail().x;
  const float max_left =
    std::max(min_left, full_width - splitter_width - min_canvas);
  tool.left_panel_width =
    std::max(min_left, std::min(max_left, tool.left_panel_width));

  ImGui::BeginChild("##mesh_tool_left",
                    ImVec2(tool.left_panel_width, 0.0f),
                    true);
  ImGui::TextUnformatted("Selection");
  ImGui::Separator();

  ImGui::TextUnformatted("View");
  ImGui::Separator();
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::SliderFloat("Cell edge thickness",
                     &tool.cell_edge_thickness,
                     0.0f,
                     6.0f,
                     "%.1f");
  tool.cell_edge_thickness = std::max(0.0f, tool.cell_edge_thickness);
  ImGui::Spacing();

  int mode_int = static_cast<int>(tool.mode);
  if (ImGui::RadioButton("Boundary faces", mode_int == 0))
    tool.mode = EditorState::MeshToolState::Mode::boundary;
  if (ImGui::RadioButton("Cells", mode_int == 1))
    tool.mode = EditorState::MeshToolState::Mode::cell;

  ImGui::Spacing();
  if (tool.mode == EditorState::MeshToolState::Mode::boundary)
    {
      ImGui::TextUnformatted("Boundary IDs");
      ImGui::Separator();
      for (const auto &[bid, count] : tool.boundary_counts)
        {
          const bool selected =
            tool.selected_boundary_id == static_cast<int>(bid);
          std::string label =
            std::to_string(bid) + " (" + std::to_string(count) + ")";
          if (ImGui::Selectable(label.c_str(), selected))
            {
              tool.selected_boundary_id = static_cast<int>(bid);
              tool.selected_boundary_faces.clear();
              for (const auto &seg : tool.boundary_segs)
                {
                  if (seg.boundary_id == bid)
                    tool.selected_boundary_faces.insert(
                      {seg.cell_index, seg.face_no});
                }
            }
        }

      ImGui::Spacing();
      if (ImGui::Button("Select all faces with active ID"))
        {
          if (tool.selected_boundary_id >= 0)
            {
              tool.selected_boundary_faces.clear();
              for (const auto &seg : tool.boundary_segs)
                {
                  if (seg.boundary_id ==
                      static_cast<unsigned int>(tool.selected_boundary_id))
                    tool.selected_boundary_faces.insert(
                      {seg.cell_index, seg.face_no});
                }
            }
        }
      ImGui::SameLine();
      if (ImGui::Button("Clear selection"))
        tool.selected_boundary_faces.clear();

      ImGui::InputScalar("Assign boundary id",
                         ImGuiDataType_U32,
                         &tool.assign_boundary_id);
      if (ImGui::Button("Apply to selection"))
        {
          if (!tool.selected_boundary_faces.empty())
            {
              for (const auto &[cell_idx, face_no] :
                   tool.selected_boundary_faces)
                {
                  auto it = tool.cell_by_index.find(cell_idx);
                  if (it == tool.cell_by_index.end())
                    continue;
                  it->second->face(face_no)->set_boundary_id(
                    static_cast<dealii::types::boundary_id>(
                      tool.assign_boundary_id));
                }
              RebuildMeshToolCaches(tool);
            }
        }
      if (ImGui::Button("Export selection to console"))
        {
          json out = json::array();
          for (const auto &[cell_idx, face_no] :
               tool.selected_boundary_faces)
            out.push_back({{"cell_index", cell_idx}, {"face_no", face_no}});
          state.console_output +=
            "MeshTool boundary selection: " + out.dump() + "\n";
          state.console_scroll_to_bottom = true;
        }
    }
  else
    {
      ImGui::TextUnformatted("Material IDs");
      ImGui::Separator();
      for (const auto &[mid, count] : tool.material_counts)
        {
          const bool selected =
            tool.selected_material_id == static_cast<int>(mid);
          std::string label =
            std::to_string(mid) + " (" + std::to_string(count) + ")";
          if (ImGui::Selectable(label.c_str(), selected))
            {
              tool.selected_material_id = static_cast<int>(mid);
              tool.selected_cells.clear();
              for (const auto &tri : tool.cell_tris)
                {
                  if (tri.material_id == mid)
                    tool.selected_cells.insert(tri.cell_index);
                }
            }
        }

      ImGui::Spacing();
      if (ImGui::Button("Select all cells with active ID"))
        {
          if (tool.selected_material_id >= 0)
            {
              tool.selected_cells.clear();
              for (const auto &tri : tool.cell_tris)
                {
                  if (tri.material_id ==
                      static_cast<unsigned int>(tool.selected_material_id))
                    tool.selected_cells.insert(tri.cell_index);
                }
            }
        }
      ImGui::SameLine();
      if (ImGui::Button("Clear selection"))
        tool.selected_cells.clear();

      ImGui::InputScalar("Assign material id",
                         ImGuiDataType_U32,
                         &tool.assign_material_id);
      if (ImGui::Button("Apply to selection"))
        {
          if (!tool.selected_cells.empty())
            {
              for (const auto cell_idx : tool.selected_cells)
                {
                  auto it = tool.cell_by_index.find(cell_idx);
                  if (it == tool.cell_by_index.end())
                    continue;
                  it->second->set_material_id(
                    static_cast<dealii::types::material_id>(
                      tool.assign_material_id));
                }
              RebuildMeshToolCaches(tool);
            }
        }
      if (ImGui::Button("Export selection to console"))
        {
          json out = json::array();
          for (const auto cell_idx : tool.selected_cells)
            out.push_back(cell_idx);
          state.console_output +=
            "MeshTool cell selection: " + out.dump() + "\n";
          state.console_scroll_to_bottom = true;
        }
    }

  ImGui::EndChild();
  ImGui::SameLine(0.0f, 0.0f);

  ImGui::BeginChild("##mesh_tool_splitter",
                    ImVec2(splitter_width, 0.0f),
                    false);
  const float splitter_height = ImGui::GetContentRegionAvail().y;
  ImGui::InvisibleButton("##mesh_tool_splitter_btn",
                         ImVec2(splitter_width, splitter_height));
  if (ImGui::IsItemHovered())
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  if (ImGui::IsItemActive())
    {
      tool.left_panel_width += ImGui::GetIO().MouseDelta.x;
      tool.left_panel_width =
        std::max(min_left, std::min(max_left, tool.left_panel_width));
    }
  ImGui::EndChild();

  ImGui::SameLine(0.0f, 0.0f);

  ImGui::BeginChild("##mesh_tool_canvas",
                    ImVec2(0.0f, 0.0f),
                    true,
                    ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse);
  const ImVec2 canvas_min  = ImGui::GetCursorScreenPos();
  const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
  const ImVec2 canvas_max(canvas_min.x + canvas_size.x,
                          canvas_min.y + canvas_size.y);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const bool   mouse_in_canvas =
    mouse.x >= canvas_min.x && mouse.x <= canvas_max.x &&
    mouse.y >= canvas_min.y && mouse.y <= canvas_max.y;

  ImDrawList *dl = ImGui::GetWindowDrawList();
  // Filled triangles/quads look "striped" with ImGui's AA fill because the
  // feathered edges remain visible along internal triangulation diagonals.
  // Disable AA fill for the mesh canvas to make cell coloring uniform.
  const ImDrawListFlags old_flags = dl->Flags;
  dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
  dl->AddRectFilled(canvas_min,
                    canvas_max,
                    ImGui::GetColorU32(ImVec4(0.10f, 0.10f, 0.12f, 1.0f)));

  if (!tool.view_initialized)
    InitMeshToolView(tool, canvas_size);

  const bool hovered = ImGui::IsWindowHovered(
    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  if (!state.text_input_active && ImGui::IsKeyPressed(ImGuiKey_F) &&
      mouse_in_canvas)
    FitMeshToolView(tool, canvas_size);

  if (hovered && !tool.box_select_active)
    {
      const float wheel = ImGui::GetIO().MouseWheel;
      if (wheel != 0.0f)
        {
          const float zoom_factor = (wheel > 0.0f) ? 1.15f : 1.0f / 1.15f;
          const ImVec2 mouse      = ImGui::GetIO().MousePos;
          const ImVec2 before =
            ScreenToWorld(tool, canvas_min, canvas_size, mouse);
          tool.view_scale = std::max(
            5.0f,
            std::min(5000.0f, tool.view_scale * zoom_factor));
          const ImVec2 after =
            ScreenToWorld(tool, canvas_min, canvas_size, mouse);
          tool.view_center.x += (before.x - after.x);
          tool.view_center.y += (before.y - after.y);
        }

      if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
        {
          const ImVec2 delta = ImGui::GetIO().MouseDelta;
          tool.view_center.x -= delta.x / tool.view_scale;
          tool.view_center.y += delta.y / tool.view_scale;
        }
    }

  // Draw cells. Prefer quads as quads to avoid diagonal seams.
  for (auto cell = tool.tria.begin_active(); cell != tool.tria.end(); ++cell)
    {
      const unsigned int cell_index =
        static_cast<unsigned int>(cell->active_cell_index());
      const bool is_selected = tool.selected_cells.count(cell_index) > 0;
      const ImU32 col =
        is_selected ? ImGui::GetColorU32(ImVec4(0.10f, 0.75f, 0.25f, 0.45f))
                    : ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.28f, 0.20f));
      const ImU32 edge_col =
        ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.90f, 0.35f));
      const float edge_thickness = tool.cell_edge_thickness;
      const bool  draw_edges     = edge_thickness > 0.0f;

      const unsigned int n_vertices = cell->reference_cell().n_vertices();
      auto               get_v      = [&](unsigned int v) -> ImVec2 {
        const auto p = cell->vertex(v);
        return ImVec2(static_cast<float>(p[0]), static_cast<float>(p[1]));
      };

      if (n_vertices == 3)
        {
          const ImVec2 a =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(0));
          const ImVec2 b =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(1));
          const ImVec2 c =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(2));
          dl->AddTriangleFilled(a, b, c, col);
          if (draw_edges)
            {
              dl->AddLine(a, b, edge_col, edge_thickness);
              dl->AddLine(b, c, edge_col, edge_thickness);
              dl->AddLine(c, a, edge_col, edge_thickness);
            }
        }
      else if (n_vertices == 4)
        {
          // Reorder quad vertices from lexicographic to cyclic order 0-1-3-2.
          const ImVec2 a =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(0));
          const ImVec2 b =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(1));
          const ImVec2 c =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(3));
          const ImVec2 d =
            WorldToScreen(tool, canvas_min, canvas_size, get_v(2));
          ImVec2 pts[4] = {a, b, c, d};
          dl->AddConvexPolyFilled(pts, 4, col);
          if (draw_edges)
            {
              dl->AddLine(a, b, edge_col, edge_thickness);
              dl->AddLine(b, c, edge_col, edge_thickness);
              dl->AddLine(c, d, edge_col, edge_thickness);
              dl->AddLine(d, a, edge_col, edge_thickness);
            }
        }
    }

  for (const auto &seg : tool.boundary_segs)
    {
      const bool matches_group =
        tool.selected_boundary_id >= 0 &&
        static_cast<unsigned int>(tool.selected_boundary_id) == seg.boundary_id;
      const bool is_selected =
        tool.selected_boundary_faces.count({seg.cell_index, seg.face_no}) > 0;
      ImVec4 base = matches_group ? ImVec4(0.95f, 0.75f, 0.15f, 1.0f)
                                  : ImVec4(0.80f, 0.80f, 0.85f, 0.70f);
      if (is_selected)
        base = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
      const ImU32 col = ImGui::GetColorU32(base);
      const ImVec2 a  = WorldToScreen(tool, canvas_min, canvas_size, seg.p0);
      const ImVec2 b  = WorldToScreen(tool, canvas_min, canvas_size, seg.p1);
      dl->AddLine(a, b, col, is_selected ? 3.0f : 2.0f);
    }

  if (hovered && mouse_in_canvas &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !state.text_input_active)
    {
      tool.box_select_active = true;
      tool.box_start_screen  = mouse;
      tool.box_end_screen    = mouse;
    }

  if (tool.box_select_active && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    tool.box_end_screen = mouse;

  if (tool.box_select_active && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
      const ImVec2 delta(tool.box_end_screen.x - tool.box_start_screen.x,
                         tool.box_end_screen.y - tool.box_start_screen.y);
      const float drag_dist_sq = delta.x * delta.x + delta.y * delta.y;
      const bool  is_box       = drag_dist_sq > 16.0f; // >4px

      const bool add_mode      = ImGui::GetIO().KeyShift;
      const bool subtract_mode = ImGui::GetIO().KeyAlt;
      const bool replace_mode  = !add_mode && !subtract_mode;

      if (is_box)
        {
          const ImVec2 w0 =
            ScreenToWorld(tool, canvas_min, canvas_size, tool.box_start_screen);
          const ImVec2 w1 =
            ScreenToWorld(tool, canvas_min, canvas_size, tool.box_end_screen);
          const Box2f world_box = MakeBoxFromScreen(w0, w1);

          if (tool.mode == EditorState::MeshToolState::Mode::boundary)
            {
              if (replace_mode)
                tool.selected_boundary_faces.clear();

              for (const auto &seg : tool.boundary_segs)
                {
                  if (!SegmentIntersectsBox(seg.p0, seg.p1, world_box))
                    continue;
                  const auto key = std::make_pair(seg.cell_index, seg.face_no);
                  if (subtract_mode)
                    tool.selected_boundary_faces.erase(key);
                  else
                    tool.selected_boundary_faces.insert(key);
                }
            }
          else
            {
              if (replace_mode)
                tool.selected_cells.clear();

              for (const auto &tri : tool.cell_tris)
                {
                  if (!TriIntersectsBox(tri.p0, tri.p1, tri.p2, world_box))
                    continue;
                  if (subtract_mode)
                    tool.selected_cells.erase(tri.cell_index);
                  else
                    tool.selected_cells.insert(tri.cell_index);
                }
            }
        }
      else
        {
          // Click selection.
          const ImVec2 world =
            ScreenToWorld(tool, canvas_min, canvas_size, tool.box_start_screen);

          if (tool.mode == EditorState::MeshToolState::Mode::boundary)
            {
              const float pick_radius_px = 6.0f;
              const float pick_radius_w  = pick_radius_px / tool.view_scale;
              const float pick_radius_sq = pick_radius_w * pick_radius_w;
              int         best           = -1;
              float       best_d = std::numeric_limits<float>::infinity();
              for (int i = 0;
                   i < static_cast<int>(tool.boundary_segs.size());
                   ++i)
                {
                  const auto &seg =
                    tool.boundary_segs[static_cast<size_t>(i)];
                  const float d = DistPointToSegSq(world, seg.p0, seg.p1);
                  if (d < best_d)
                    {
                      best_d = d;
                      best   = i;
                    }
                }
              if (best >= 0 && best_d <= pick_radius_sq)
                {
                  const auto &seg =
                    tool.boundary_segs[static_cast<size_t>(best)];
                  tool.selected_boundary_id = static_cast<int>(seg.boundary_id);
                  const auto key = std::make_pair(seg.cell_index, seg.face_no);
                  if (subtract_mode)
                    tool.selected_boundary_faces.erase(key);
                  else if (!add_mode)
                    {
                      tool.selected_boundary_faces.clear();
                      tool.selected_boundary_faces.insert(key);
                    }
                  else if (tool.selected_boundary_faces.count(key) > 0)
                    tool.selected_boundary_faces.erase(key);
                  else
                    tool.selected_boundary_faces.insert(key);
                }
            }
          else
            {
              int          best_cell = -1;
              unsigned int best_mat  = 0;
              for (const auto &tri : tool.cell_tris)
                {
                  if (PointInTri(world, tri.p0, tri.p1, tri.p2))
                    {
                      best_cell = static_cast<int>(tri.cell_index);
                      best_mat  = tri.material_id;
                      break;
                    }
                }
              if (best_cell >= 0)
                {
                  const unsigned int cell_idx =
                    static_cast<unsigned int>(best_cell);
                  tool.selected_material_id = static_cast<int>(best_mat);
                  if (subtract_mode)
                    tool.selected_cells.erase(cell_idx);
                  else if (!add_mode)
                    {
                      tool.selected_cells.clear();
                      tool.selected_cells.insert(cell_idx);
                    }
                  else if (tool.selected_cells.count(cell_idx) > 0)
                    tool.selected_cells.erase(cell_idx);
                  else
                    tool.selected_cells.insert(cell_idx);
                }
            }
        }

      tool.box_select_active = false;
    }

  if (tool.box_select_active)
    {
      const Box2f rect =
        MakeBoxFromScreen(tool.box_start_screen, tool.box_end_screen);
      dl->AddRectFilled(rect.min,
                        rect.max,
                        ImGui::GetColorU32(ImVec4(0.20f, 0.60f, 0.95f, 0.12f)));
      dl->AddRect(rect.min,
                  rect.max,
                  ImGui::GetColorU32(ImVec4(0.20f, 0.60f, 0.95f, 0.85f)),
                  0.0f,
                  0,
                  1.5f);
    }

  dl->AddText(ImVec2(canvas_min.x + 8.0f, canvas_min.y + 8.0f),
              ImGui::GetColorU32(ImVec4(1, 1, 1, 0.85f)),
              "F fit view  |  LMB select/box  |  Shift add  |  Alt subtract  |  RMB pan  |  wheel zoom");

  dl->Flags = old_flags;
  ImGui::EndChild();

  ImGui::EndPopup();
}

#endif // CORAL_NODE_EDITOR_ENABLE_MANIPULATOR

static json
BuildNetworkJson(const EditorState &state)
{
  return BuildNetworkJsonFrom(state.nodes, state.links, state.registry);
}

static void
UpdateTabIndex(EditorState &state)
{
  state.tab_name_index.clear();
  for (size_t i = 0; i < state.tabs.size(); ++i)
    state.tab_name_index[state.tabs[i].name] = static_cast<int>(i);
}

static int
FindTabByName(const EditorState &state, const std::string &name)
{
  auto it = state.tab_name_index.find(name);
  if (it == state.tab_name_index.end())
    return -1;
  return it->second;
}

static void
DeleteModule(EditorState &state, const std::string &module_name)
{
  if (module_name.empty() || state.tabs.size() <= 1)
    return;

  const int tab_index = FindTabByName(state, module_name);
  if (tab_index <= 0)
    return;

  // Remove all module nodes from the main tab.
  NetworkTab &main_tab = state.tabs[0];
  std::vector<unsigned int> nodes_to_remove;
  for (const auto &pair : main_tab.nodes)
    {
      const EditorNode &node = pair.second;
      if (node.type == "coral::Network" && node.name == module_name)
        nodes_to_remove.push_back(pair.first);
    }
  for (unsigned int id : nodes_to_remove)
    {
      main_tab.nodes.erase(id);
      for (auto it = main_tab.links.begin(); it != main_tab.links.end();)
        {
          const EditorLink &link = it->second;
          if (link.source_node == id || link.target_node == id)
            it = main_tab.links.erase(it);
          else
            ++it;
        }
    }
  if (!nodes_to_remove.empty())
    {
      if (!main_tab.nodes.empty())
        main_tab.next_node_id = main_tab.nodes.rbegin()->first + 1;
      else
        main_tab.next_node_id = 1;
      if (!main_tab.links.empty())
        main_tab.next_link_id = main_tab.links.rbegin()->first + 1;
      else
        main_tab.next_link_id = 1;
    }

  // Switch away if we are deleting the active tab (or any tab before it).
  const int old_active = state.active_tab;
  if (old_active == tab_index)
    LoadTab(state, 0);

  state.tabs.erase(state.tabs.begin() + tab_index);
  UpdateTabIndex(state);

  if (old_active > tab_index)
    {
      state.active_tab = old_active - 1;
      LoadTab(state, state.active_tab);
    }

  if (state.active_tab == 0)
    state.nodes = main_tab.nodes;

  state.force_tab_select = true;
  state.request_focus    = true;
  state.status           = "Deleted module: " + module_name;
}

static void
RenameModule(EditorState &state,
             const std::string &old_name,
             const std::string &new_name)
{
  if (old_name.empty() || new_name.empty() || old_name == new_name)
    return;
  if (new_name == "Main")
    {
      state.status = "Cannot rename module to Main";
      return;
    }

  const int tab_index = FindTabByName(state, old_name);
  if (tab_index <= 0)
    return;
  if (FindTabByName(state, new_name) >= 0)
    {
      state.status = "Module name already exists: " + new_name;
      return;
    }

  state.tabs[static_cast<size_t>(tab_index)].name = new_name;

  NetworkTab &main_tab = state.tabs[0];
  for (auto &pair : main_tab.nodes)
    {
      EditorNode &node = pair.second;
      if (node.type == "coral::Network" && node.name == old_name)
        {
          node.name = new_name;
          std::snprintf(node.name_buffer,
                        sizeof(node.name_buffer),
                        "%s",
                        node.name.c_str());
          node.name_buffer_init = true;
        }
    }

  UpdateTabIndex(state);
  state.force_tab_select = true;
  state.request_focus    = true;
  state.status           = "Renamed module: " + old_name + " -> " + new_name;
}

static void
SwitchToTab(EditorState &state, int tab_index)
{
  if (tab_index < 0 || tab_index >= static_cast<int>(state.tabs.size()))
    return;
  if (tab_index == state.active_tab)
    return;

  SaveActiveTab(state);
  LoadTab(state, tab_index);
  SyncNetworkNodesFromTabs(state);
  state.force_tab_select = true;
  state.request_focus    = true;
}

static std::string
SerializeTab(const NetworkTab &tab, const EditorState &state)
{
  return BuildNetworkJsonFrom(tab.nodes, tab.links, state.registry).dump();
}

static void
SyncNetworkNodesFromTabs(EditorState &state)
{
  if (state.tabs.empty())
    return;

  NetworkTab &main_tab = state.tabs[0];
  for (size_t i = 1; i < state.tabs.size(); ++i)
    {
      const NetworkTab &tab   = state.tabs[i];
      const std::string value = SerializeTab(tab, state);
      for (auto &pair : main_tab.nodes)
        {
          EditorNode &node = pair.second;
          if (node.type == "coral::Network" && node.name == tab.name)
            {
              node.value = value;
              std::snprintf(node.value_buffer,
                            sizeof(node.value_buffer),
                            "%s",
                            node.value.c_str());
              node.value_buffer_init = true;
            }
        }
    }

  if (state.active_tab == 0)
    state.nodes = main_tab.nodes;
}

static void
CreateTabsFromNetworkNodes(EditorState &state)
{
  if (state.tabs.empty())
    return;

  std::vector<EditorNode> network_nodes;
  network_nodes.reserve(state.tabs[0].nodes.size());
  for (const auto &pair : state.tabs[0].nodes)
    {
      const EditorNode &node = pair.second;
      if (node.type != "coral::Network")
        continue;
      if (node.name.empty())
        continue;
      if (state.tab_name_index.find(node.name) != state.tab_name_index.end())
        continue;
      network_nodes.push_back(node);
    }

  for (const auto &node : network_nodes)
    {
      NetworkTab tab;
      tab.name = node.name;
      if (!node.value.empty())
        {
          try
            {
              json        data = json::parse(node.value);
              std::string error;
              if (LoadNetworkFromJson(state,
                                      data,
                                      tab.nodes,
                                      tab.links,
                                      tab.next_node_id,
                                      tab.next_link_id,
                                      error))
                state.tabs.push_back(tab);
            }
          catch (const std::exception &)
            {
              state.tabs.push_back(tab);
            }
        }
      else
        {
          state.tabs.push_back(tab);
        }
    }
}

static unsigned int
AddNodeToTab(EditorState       &state,
             NetworkTab        &tab,
             const std::string &type,
             const std::string &display_name,
             const std::string &value_override)
{
  const unsigned int id = tab.next_node_id++;
  EditorNode         node;
  node.id               = id;
  node.type             = type;
  node.name_buffer_init = false;
  if (!display_name.empty())
    node.name = display_name;

  if (!value_override.empty())
    node.value = value_override;
  else
    {
      const RegistryNodeType *node_type = FindNodeType(state, type);
      if (node_type && !node_type->default_value.empty())
        node.value = node_type->default_value;
    }

  if (!node.value.empty())
    {
      std::snprintf(node.value_buffer,
                    sizeof(node.value_buffer),
                    "%s",
                    node.value.c_str());
      node.value_buffer_init = true;
    }

  tab.nodes[id] = node;
  tab.nodes[id].desired_position =
    ComputeNodePosition(static_cast<int>(tab.nodes.size()) - 1);
  tab.nodes[id].needs_position = true;
  return id;
}

static unsigned int
AddNodeFromType(EditorState       &state,
                const std::string &type,
                const std::string &display_name   = std::string(),
                const std::string &value_override = std::string())
{
  const unsigned int id = state.next_node_id++;
  EditorNode         node;
  node.id               = id;
  node.type             = type;
  node.name_buffer_init = false;
  if (!display_name.empty())
    node.name = display_name;

  if (!value_override.empty())
    node.value = value_override;
  else
    {
      const RegistryNodeType *node_type = FindNodeType(state, type);
      if (node_type && !node_type->default_value.empty())
        node.value = node_type->default_value;
    }

  if (!node.value.empty())
    {
      std::snprintf(node.value_buffer,
                    sizeof(node.value_buffer),
                    "%s",
                    node.value.c_str());
      node.value_buffer_init = true;
    }

  state.nodes[id] = node;
  state.nodes[id].desired_position =
    ComputeNodePosition(static_cast<int>(state.nodes.size()) - 1);
  state.nodes[id].needs_position = true;
  return id;
}

static bool
IsInputConnected(const EditorState &state,
                 unsigned int       node_id,
                 unsigned int       input_index)
{
  for (auto it = state.links.begin(); it != state.links.end(); ++it)
    {
      const EditorLink &link = it->second;
      if (link.target_node == node_id && link.target_input == input_index)
        return true;
    }
  return false;
}

static bool
FindSelfPin(const RegistryNodeType *node_type,
            ed::PinKind            &kind,
            unsigned int           &index)
{
  if (!node_type)
    return false;

  for (unsigned int i = 0; i < node_type->outputs.size(); ++i)
    {
      if (node_type->outputs[i] < 0)
        {
          kind  = ed::PinKind::Output;
          index = i;
          return true;
        }
    }

  for (unsigned int i = 0; i < node_type->inputs.size(); ++i)
    {
      if (node_type->inputs[i] < 0)
        {
          kind  = ed::PinKind::Input;
          index = i;
          return true;
        }
    }

  return false;
}

static void
DrawPin(const RegistryNodeType *node_type,
        unsigned int            node_id,
        ed::PinKind             kind,
        unsigned int            index,
        bool                    align_right)
{
  PinMeta meta;
  if (!GetPinMeta(node_type, kind, index, meta))
    return;

  const float dot_size = 8.0f;

  ed::BeginPin(ed::PinId(MakePinId(node_id, kind, index)), kind);
  if (kind == ed::PinKind::Input)
    ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
  else
    ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
  ed::PinPivotSize(ImVec2(0.0f, 0.0f));

  ImGuiColorEditFlags color_flags =
    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
    ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs |
    ImGuiColorEditFlags_NoLabel;
  const ImVec4 color = GetPinDotColor(kind, meta);
  char         color_id[64];
  std::snprintf(color_id,
                sizeof(color_id),
                "##pin_dot_%u_%u_%u",
                node_id,
                kind == ed::PinKind::Input ? 0u : 1u,
                index);

  if (kind == ed::PinKind::Input)
    {
      ImGui::ColorButton(color_id,
                         color,
                         color_flags,
                         ImVec2(dot_size, dot_size));
      ImGui::SameLine();
      ImGui::TextUnformatted(meta.label.c_str());
    }
  else
    {
      ImGui::TextUnformatted(meta.label.c_str());
      ImGui::SameLine();
      ImGui::ColorButton(color_id,
                         color,
                         color_flags,
                         ImVec2(dot_size, dot_size));
    }

  ed::EndPin();
}

static void
DrawSelfPinInline(const RegistryNodeType *node_type,
                  unsigned int            node_id,
                  ed::PinKind             kind,
                  unsigned int            index)
{
  PinMeta meta;
  if (!GetPinMeta(node_type, kind, index, meta))
    return;

  const float dot_size = 8.0f;
  ed::BeginPin(ed::PinId(MakePinId(node_id, kind, index)), kind);
  if (kind == ed::PinKind::Input)
    ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
  else
    ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
  ed::PinPivotSize(ImVec2(0.0f, 0.0f));
  ImGuiColorEditFlags color_flags =
    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
    ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs |
    ImGuiColorEditFlags_NoLabel;
  const ImVec4 color = GetPinDotColor(kind, meta);
  char         color_id[64];
  std::snprintf(color_id,
                sizeof(color_id),
                "##pin_self_%u_%u",
                node_id,
                kind == ed::PinKind::Input ? 0u : 1u);

  if (kind == ed::PinKind::Input)
    {
      ImGui::ColorButton(color_id,
                         color,
                         color_flags,
                         ImVec2(dot_size, dot_size));
    }
  else
    {
      ImGui::ColorButton(color_id,
                         color,
                         color_flags,
                         ImVec2(dot_size, dot_size));
    }
  ed::EndPin();
}

static void
DrawHoverTooltips(const EditorState &state)
{
  const ed::PinId hovered_pin = ed::GetHoveredPin();
  if (hovered_pin)
    {
      unsigned int coral_id = 0;
      unsigned int index    = 0;
      ed::PinKind  kind     = ed::PinKind::Input;
      if (DecodePinId(hovered_pin, coral_id, kind, index))
        {
          PinMeta meta;
          if (GetPinMetaForNode(state, coral_id, kind, index, meta))
            {
              ImGui::BeginTooltip();
              ImGui::Text("Type: %s",
                          meta.type.empty() ? "<unknown>" : meta.type.c_str());
              ImGui::EndTooltip();
              return;
            }
        }
    }

  const ed::NodeId hovered_node = ed::GetHoveredNode();
  if (hovered_node)
    {
      const unsigned int editor_id =
        static_cast<unsigned int>(hovered_node.Get());
      if (editor_id > 0)
        {
          const unsigned int coral_id = editor_id - 1u;
          auto               it       = state.nodes.find(coral_id);
          if (it != state.nodes.end())
            {
              ImGui::BeginTooltip();
              ImGui::Text("Type: %s", it->second.type.c_str());
              ImGui::EndTooltip();
            }
        }
    }
}

static void
DrawNode(EditorState &state, EditorNode &node)
{
  RegistryNodeType        resolved_type;
  const RegistryNodeType *node_type = nullptr;
  if (GetNodeTypeForNode(state, node, resolved_type))
    node_type = &resolved_type;

  LogDebug(
    state, 10, "BeginNode coral_id=%u editor_id=%u", node.id, node.id + 1u);
  const NodeColors node_colors = GetNodeColors(node_type);
  ed::PushStyleColor(ed::StyleColor_NodeBg, node_colors.body);
  ed::PushStyleColor(ed::StyleColor_NodeBorder, node_colors.border);
  ed::PushStyleColor(ed::StyleColor_HovNodeBorder, node_colors.border_hover);
  ed::PushStyleColor(ed::StyleColor_SelNodeBorder, node_colors.border_selected);
  ed::BeginNode(ed::NodeId(node.id + 1u));
  if (!node.name_buffer_init)
    {
      const std::string &initial_name =
        node.name.empty() ? node.type : node.name;
      std::snprintf(node.name_buffer,
                    sizeof(node.name_buffer),
                    "%s",
                    initial_name.c_str());
      node.name_buffer_init = true;
    }

  const bool has_value_field =
    node_type && node_type->node_type == "elementary_constructor";
  if (has_value_field && !node.value_buffer_init)
    {
      std::snprintf(node.value_buffer,
                    sizeof(node.value_buffer),
                    "%s",
                    node.value.c_str());
      node.value_buffer_init = true;
    }

  std::vector<unsigned int> input_indices;
  std::vector<unsigned int> output_indices;
  if (node_type)
    {
      input_indices.reserve(node_type->inputs.size());
      output_indices.reserve(node_type->outputs.size());
      for (unsigned int i = 0; i < node_type->inputs.size(); ++i)
        {
          PinMeta meta;
          if (GetPinMeta(node_type, ed::PinKind::Input, i, meta) &&
              meta.is_self)
            continue;
          input_indices.push_back(i);
        }
      for (unsigned int i = 0; i < node_type->outputs.size(); ++i)
        {
          PinMeta meta;
          if (GetPinMeta(node_type, ed::PinKind::Output, i, meta) &&
              meta.is_self)
            continue;
          output_indices.push_back(i);
        }
    }

  const ImVec4 header_color   = node_colors.header;
  const float  min_name_width = 120.0f;
  const float name_text_width = ImGui::CalcTextSize(node.name_buffer).x + 16.0f;
  const float name_width      = std::max(min_name_width, name_text_width);
  ed::PinKind self_kind       = ed::PinKind::Output;
  unsigned int self_index     = 0;
  PinMeta      self_meta;
  const bool   has_self_pin =
    FindSelfPin(node_type, self_kind, self_index) &&
    GetPinMeta(node_type, self_kind, self_index, self_meta);
  const float dot_size   = 8.0f;
  const float self_width = has_self_pin ?
                             ImGui::CalcTextSize(self_meta.label.c_str()).x +
                               ImGui::GetStyle().ItemSpacing.x + dot_size :
                             0.0f;

  float max_input_width  = 0.0f;
  float max_output_width = 0.0f;
  if (node_type)
    {
      const float spacing = ImGui::GetStyle().ItemSpacing.x;
      for (unsigned int idx : input_indices)
        {
          PinMeta meta;
          if (GetPinMeta(node_type, ed::PinKind::Input, idx, meta))
            {
              const float width =
                ImGui::CalcTextSize(meta.label.c_str()).x + dot_size + spacing;
              if (width > max_input_width)
                max_input_width = width;
            }
        }
      for (unsigned int idx : output_indices)
        {
          PinMeta meta;
          if (GetPinMeta(node_type, ed::PinKind::Output, idx, meta))
            {
              const float width =
                ImGui::CalcTextSize(meta.label.c_str()).x + dot_size + spacing;
              if (width > max_output_width)
                max_output_width = width;
            }
        }
    }
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float pins_width =
    max_input_width + max_output_width +
    (max_input_width > 0.0f && max_output_width > 0.0f ? spacing : 0.0f);
  float value_width = 0.0f;
  if (has_value_field)
    {
      const float text_width = ImGui::CalcTextSize(node.value_buffer).x + 16.0f;
      value_width            = std::max(min_name_width, text_width);
    }
  const float header_width =
    std::max(name_width + (has_self_pin ? spacing + self_width : 0.0f),
             std::max(pins_width, value_width));
  const ImVec2 header_min    = ImGui::GetCursorScreenPos();
  const float  header_height = ImGui::GetFrameHeight();
  ImDrawList  *draw_list     = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(header_min,
                           ImVec2(header_min.x + header_width,
                                  header_min.y + header_height),
                           ImGui::ColorConvertFloat4ToU32(header_color),
                           6.0f,
                           ImDrawFlags_RoundCornersTop);

  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  const float name_input_width =
    header_width - (has_self_pin ? spacing + dot_size : 0.0f);
  ImGui::SetNextItemWidth(name_input_width);
  std::string name_label = "##name_" + std::to_string(node.id);
  if (ImGui::InputText(name_label.c_str(),
                       node.name_buffer,
                       sizeof(node.name_buffer)))
    {
      const std::string new_name = node.name_buffer;
      if (new_name.empty() || new_name == node.type)
        node.name.clear();
      else
        node.name = new_name;
    }
  if (ImGui::IsItemActive())
    state.text_input_active = true;
  if (has_self_pin)
    {
      ImGui::SameLine(0.0f, spacing);
      DrawSelfPinInline(node_type, node.id, self_kind, self_index);
    }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
  ImGui::Spacing();

  if (has_value_field)
    {
      std::string value_label = "##value" + std::to_string(node.id);
      ImGui::SetNextItemWidth(header_width);
      if (ImGui::InputText(value_label.c_str(),
                           node.value_buffer,
                           sizeof(node.value_buffer)))
        node.value = node.value_buffer;
      if (ImGui::IsItemActive())
        state.text_input_active = true;
    }

#if CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
  const bool is_triangulation_2d =
    node.type.find("dealii::Triangulation<2") != std::string::npos;
  if (is_triangulation_2d)
    {
      std::string button_id = "Mesh Tool##mesh_tool_" + std::to_string(node.id);
      if (ImGui::Button(button_id.c_str()))
        {
          state.mesh_tool.open             = true;
          state.mesh_tool.pending_load     = true;
          state.mesh_tool.target_node_id   = node.id;
          state.mesh_tool.target_tab_index = state.active_tab;
        }
    }
#endif

  if (node_type)
    {
      const size_t row_count =
        std::max(input_indices.size(), output_indices.size());
      if (row_count > 0)
        {
          std::string table_id = "##pins_" + std::to_string(node.id);
          if (ImGui::BeginTable(table_id.c_str(),
                                2,
                                ImGuiTableFlags_SizingFixedFit |
                                  ImGuiTableFlags_NoPadOuterX))
            {
              ImGui::TableSetupColumn("Inputs",
                                      ImGuiTableColumnFlags_WidthFixed);
              ImGui::TableSetupColumn("Outputs",
                                      ImGuiTableColumnFlags_WidthFixed);
              for (size_t row = 0; row < row_count; ++row)
                {
                  ImGui::TableNextRow();
                  ImGui::TableSetColumnIndex(0);
                  if (row < input_indices.size())
                    DrawPin(node_type,
                            node.id,
                            ed::PinKind::Input,
                            input_indices[row],
                            false);
                  ImGui::TableSetColumnIndex(1);
                  if (row < output_indices.size())
                    DrawPin(node_type,
                            node.id,
                            ed::PinKind::Output,
                            output_indices[row],
                            true);
                }
              ImGui::EndTable();
            }
        }
    }

  ed::EndNode();
  ed::PopStyleColor(4);
  LogDebug(state, 10, "EndNode coral_id=%u", node.id);
}

static void
HandleLinkCreation(EditorState &state)
{
  LogDebug(state, 10, "BeginCreate()");
  if (ed::BeginCreate())
    {
      ed::PinId start_pin;
      ed::PinId end_pin;
      if (ed::QueryNewLink(&start_pin, &end_pin))
        {
          LogDebug(state,
                   "QueryNewLink start=%llu end=%llu",
                   static_cast<unsigned long long>(start_pin.Get()),
                   static_cast<unsigned long long>(end_pin.Get()));
          unsigned int start_node  = 0;
          unsigned int start_index = 0;
          ed::PinKind  start_kind  = ed::PinKind::Input;
          unsigned int end_node    = 0;
          unsigned int end_index   = 0;
          ed::PinKind  end_kind    = ed::PinKind::Input;

          if (DecodePinId(start_pin, start_node, start_kind, start_index) &&
              DecodePinId(end_pin, end_node, end_kind, end_index))
            {
              unsigned int source_node   = 0;
              unsigned int source_output = 0;
              unsigned int target_node   = 0;
              unsigned int target_input  = 0;

              if (start_kind == ed::PinKind::Output &&
                  end_kind == ed::PinKind::Input)
                {
                  source_node   = start_node;
                  source_output = start_index;
                  target_node   = end_node;
                  target_input  = end_index;
                }
              else if (start_kind == ed::PinKind::Input &&
                       end_kind == ed::PinKind::Output)
                {
                  source_node   = end_node;
                  source_output = end_index;
                  target_node   = start_node;
                  target_input  = start_index;
                }

              const bool can_create =
                (start_kind != end_kind) &&
                !IsInputConnected(state, target_node, target_input);

              bool    types_match = false;
              PinMeta source_meta;
              PinMeta target_meta;
              if (start_kind != end_kind)
                {
                  const bool have_types = GetPinMetaForNode(state,
                                                            source_node,
                                                            ed::PinKind::Output,
                                                            source_output,
                                                            source_meta) &&
                                          GetPinMetaForNode(state,
                                                            target_node,
                                                            ed::PinKind::Input,
                                                            target_input,
                                                            target_meta);
                  types_match = have_types && !source_meta.type.empty() &&
                                (source_meta.type == target_meta.type);
                }

              LogDebug(
                state,
                "Link candidate source=%u out=%u target=%u in=%u can_create=%d",
                source_node,
                source_output,
                target_node,
                target_input,
                can_create ? 1 : 0);

              if (start_kind != end_kind && !types_match)
                {
                  state.status = "Link rejected: type mismatch (" +
                                 source_meta.type + " -> " + target_meta.type +
                                 ")";
                  ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
                }

              if (can_create && types_match && ed::AcceptNewItem())
                {
                  EditorLink link;
                  link.id              = state.next_link_id++;
                  link.source_node     = source_node;
                  link.target_node     = target_node;
                  link.source_output   = source_output;
                  link.target_input    = target_input;
                  state.links[link.id] = link;
                  LogDebug(state, "Link created id=%u", link.id);

                  auto source_it = state.nodes.find(source_node);
                  if (source_it != state.nodes.end())
                    {
                      EditorNode &source = source_it->second;
                      const bool  is_default_name =
                        source.name.empty() || source.name == source.type;
                      if (is_default_name)
                        {
                          PinMeta target_meta;
                          if (GetPinMetaForNode(state,
                                                target_node,
                                                ed::PinKind::Input,
                                                target_input,
                                                target_meta) &&
                              !target_meta.label.empty() &&
                              target_meta.label != "self")
                            {
                              source.name = target_meta.label;
                              std::snprintf(source.name_buffer,
                                            sizeof(source.name_buffer),
                                            "%s",
                                            source.name.c_str());
                              source.name_buffer_init = true;
                            }
                        }
                    }
                }
            }
        }
    }

  ed::EndCreate();
  LogDebug(state, 10, "EndCreate()");
}

static void
RemoveNode(EditorState &state, unsigned int coral_node_id)
{
  std::string maybe_module_name;
  {
    auto it = state.nodes.find(coral_node_id);
    if (it != state.nodes.end())
      {
        const EditorNode &node = it->second;
        if (state.active_tab == 0 && node.type == "coral::Network" &&
            !node.name.empty() && FindTabByName(state, node.name) > 0)
          maybe_module_name = node.name;
      }
  }

  for (auto it = state.links.begin(); it != state.links.end();)
    {
      const EditorLink &link = it->second;
      if (link.source_node == coral_node_id ||
          link.target_node == coral_node_id)
        it = state.links.erase(it);
      else
        ++it;
    }
  state.nodes.erase(coral_node_id);
  UpdateNextIds(state);

  if (!maybe_module_name.empty())
    {
      bool any_remaining = false;
      for (const auto &pair : state.nodes)
        {
          const EditorNode &node = pair.second;
          if (node.type == "coral::Network" && node.name == maybe_module_name)
            {
              any_remaining = true;
              break;
            }
        }
      if (!any_remaining)
        {
          state.module_delete_candidate  = maybe_module_name;
          state.show_module_delete_popup = true;
        }
    }
}

static void
HandleDeletion(EditorState &state)
{
  LogDebug(state, 10, "BeginDelete()");
  if (ed::BeginDelete())
    {
      ed::NodeId node_id;
      while (ed::QueryDeletedNode(&node_id))
        {
          if (ed::AcceptDeletedItem())
            {
              const unsigned int editor_id =
                static_cast<unsigned int>(node_id.Get());
              if (editor_id > 0)
                {
                  const unsigned int coral_id = editor_id - 1u;
                  RemoveNode(state, coral_id);
                  LogDebug(state, "Node deleted editor_id=%u", editor_id);
                }
            }
        }

      ed::LinkId link_id;
      while (ed::QueryDeletedLink(&link_id))
        {
          if (ed::AcceptDeletedItem())
            {
              const unsigned int editor_id =
                static_cast<unsigned int>(link_id.Get());
              if (editor_id > 0)
                state.links.erase(editor_id - 1u);
              LogDebug(state, "Link deleted editor_id=%u", editor_id);
            }
        }
    }

  ed::EndDelete();
  LogDebug(state, 10, "EndDelete()");
}

static void
DrawEditor(EditorState &state)
{
  LogDebug(state, 10, "Editor Begin");
  ed::Begin("Node Editor");
  state.text_input_active = false;

  static ed::NodeId context_node_id;
  static ed::LinkId context_link_id;
  ed::Suspend();
  if (ed::ShowNodeContextMenu(&context_node_id))
    ImGui::OpenPopup("Node Context");
  else if (ed::ShowLinkContextMenu(&context_link_id))
    ImGui::OpenPopup("Link Context");
  else if (ed::ShowBackgroundContextMenu())
    ImGui::OpenPopup("Background Context");
  ed::Resume();

  ed::Suspend();
  if (ImGui::BeginPopup("Node Context"))
    {
      if (ImGui::MenuItem("Delete Node"))
        {
          ed::DeleteNode(context_node_id);
          const unsigned int editor_id =
            static_cast<unsigned int>(context_node_id.Get());
          if (editor_id > 0)
            RemoveNode(state, editor_id - 1u);
        }
      ImGui::EndPopup();
    }

  if (ImGui::BeginPopup("Link Context"))
    {
      if (ImGui::MenuItem("Delete Link"))
        {
          ed::DeleteLink(context_link_id);
          const unsigned int editor_id =
            static_cast<unsigned int>(context_link_id.Get());
          if (editor_id > 0)
            state.links.erase(editor_id - 1u);
        }
      ImGui::EndPopup();
    }

  if (ImGui::BeginPopup("Background Context"))
    {
      if (ImGui::MenuItem("Delete Selected"))
        state.request_delete = true;
      ImGui::EndPopup();
    }
  ed::Resume();

  for (auto it = state.nodes.begin(); it != state.nodes.end(); ++it)
    {
      EditorNode &node = it->second;
      if (node.needs_position)
        {
          ed::SetNodePosition(ed::NodeId(node.id + 1u), node.desired_position);
          node.needs_position = false;
        }
    }

  for (auto it = state.nodes.begin(); it != state.nodes.end(); ++it)
    DrawNode(state, it->second);

  for (auto it = state.links.begin(); it != state.links.end(); ++it)
    {
      const unsigned int id         = it->first;
      const EditorLink  &link       = it->second;
      const auto         output_pin = ed::PinId(
        MakePinId(link.source_node, ed::PinKind::Output, link.source_output));
      const auto input_pin = ed::PinId(
        MakePinId(link.target_node, ed::PinKind::Input, link.target_input));
      ed::Link(ed::LinkId(id + 1u), output_pin, input_pin);
    }

  HandleLinkCreation(state);
  HandleDeletion(state);
  DrawHoverTooltips(state);
  if (state.request_focus)
    {
      ed::NavigateToContent();
      state.request_focus = false;
    }

  const bool delete_requested = state.request_delete;
  state.request_delete        = false;
  if (delete_requested && !state.text_input_active)
    {
      bool      deleted_any    = false;
      const int selected_nodes = ed::GetSelectedNodes(nullptr, 0);
      if (selected_nodes > 0)
        {
          std::vector<ed::NodeId> nodes(static_cast<size_t>(selected_nodes));
          ed::GetSelectedNodes(nodes.data(), selected_nodes);
          for (const ed::NodeId &node_id : nodes)
            {
              ed::DeleteNode(node_id);
              const unsigned int editor_id =
                static_cast<unsigned int>(node_id.Get());
              if (editor_id > 0)
                RemoveNode(state, editor_id - 1u);
              deleted_any = true;
            }
        }

      const int selected_links = ed::GetSelectedLinks(nullptr, 0);
      if (selected_links > 0)
        {
          std::vector<ed::LinkId> links(static_cast<size_t>(selected_links));
          ed::GetSelectedLinks(links.data(), selected_links);
          for (const ed::LinkId &link_id : links)
            {
              ed::DeleteLink(link_id);
              const unsigned int editor_id =
                static_cast<unsigned int>(link_id.Get());
              if (editor_id > 0)
                state.links.erase(editor_id - 1u);
              deleted_any = true;
            }
        }
      if (!deleted_any)
        state.status = "Nothing selected to delete";
    }

  if (ed::HasSelectionChanged())
    {
      const int selected_nodes = ed::GetSelectedNodes(nullptr, 0);
      const int selected_links = ed::GetSelectedLinks(nullptr, 0);
      LogDebug(state,
               "Selection changed nodes=%d links=%d",
               selected_nodes,
               selected_links);
      if (selected_nodes > 0)
        {
          ed::NodeId nodes[8];
          const int  count = ed::GetSelectedNodes(nodes, 8);
          for (int i = 0; i < count; ++i)
            LogDebug(state,
                     "  selected node editor_id=%llu",
                     static_cast<unsigned long long>(nodes[i].Get()));
        }
      if (selected_links > 0)
        {
          ed::LinkId links[8];
          const int  count = ed::GetSelectedLinks(links, 8);
          for (int i = 0; i < count; ++i)
            LogDebug(state,
                     "  selected link editor_id=%llu",
                     static_cast<unsigned long long>(links[i].Get()));
        }
    }

  for (auto it = state.nodes.begin(); it != state.nodes.end(); ++it)
    {
      const EditorNode &node = it->second;
      it->second.desired_position =
        ed::GetNodePosition(ed::NodeId(node.id + 1u));
    }

  ed::End();
  LogDebug(state, 10, "Editor End");
}

static void
DrawSidebar(EditorState &state)
{
  auto draw_section_header = [](const char *label) {
    ImDrawList  *draw_list = ImGui::GetWindowDrawList();
    const ImVec2 pos       = ImGui::GetCursorScreenPos();
    const float  width     = ImGui::GetContentRegionAvail().x;
    const float  height    = ImGui::GetFrameHeight();
    const ImU32  bg = ImGui::GetColorU32(ImVec4(0.18f, 0.20f, 0.24f, 1.0f));

    draw_list->AddRectFilled(pos,
                             ImVec2(pos.x + width, pos.y + height),
                             bg,
                             4.0f);
    draw_list->AddText(ImVec2(pos.x + ImGui::GetStyle().FramePadding.x,
                              pos.y + ImGui::GetStyle().FramePadding.y),
                       ImGui::GetColorU32(ImGuiCol_Text),
                       label);
    ImGui::Dummy(ImVec2(width, height));
    ImGui::Spacing();
  };

  draw_section_header("Registry");
  ImGui::InputText("Registry Path",
                   state.registry_path,
                   sizeof(state.registry_path));
  if (ImGui::Button("Load Registry"))
    LoadRegistry(state);

  ImGui::Separator();
  draw_section_header("Network");
  ImGui::InputText("Network Path",
                   state.network_path,
                   sizeof(state.network_path));
  if (ImGui::Button("Load Network"))
    LoadNetwork(state);
  ImGui::SameLine();
  if (ImGui::Button("Save Network"))
    {
      std::string error;
      SaveActiveTab(state);
      SyncNetworkNodesFromTabs(state);
      if (state.tabs.empty())
        {
          state.status = "No network to save";
        }
      else
        {
          json data = BuildNetworkJsonFrom(state.tabs[0].nodes,
                                           state.tabs[0].links,
                                           state.registry);
          if (!SaveJsonFile(state.network_path, data, error))
            state.status = error;
          else
            state.status = "Saved network";
        }
    }

  ImGui::Separator();
  draw_section_header("Node Types");
  static char filter[128] = {0};
  ImGui::InputText("Filter", filter, sizeof(filter));

  const float node_types_min      = 120.0f;
  const float node_types_rest_min = 280.0f;
  const float node_types_max =
    std::max(node_types_min,
             ImGui::GetContentRegionAvail().y - node_types_rest_min);
  if (state.node_types_height < node_types_min)
    state.node_types_height = node_types_min;
  if (state.node_types_height > node_types_max)
    state.node_types_height = node_types_max;

  if (ImGui::BeginChild("##node_types_list",
                        ImVec2(0.0f, state.node_types_height),
                        true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
      for (auto it = state.registry.begin(); it != state.registry.end(); ++it)
        {
          const std::string &type = it->first;
          if (filter[0] != '\0')
            {
              if (type.find(filter) == std::string::npos)
                continue;
            }
          if (ImGui::Selectable(type.c_str()))
            AddNodeFromType(state, type);
        }
    }
  ImGui::EndChild();

  ImGui::InvisibleButton("##node_types_splitter",
                         ImVec2(ImGui::GetContentRegionAvail().x, 6.0f));
  if (ImGui::IsItemHovered())
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  if (ImGui::IsItemActive())
    {
      state.node_types_height += ImGui::GetIO().MouseDelta.y;
      if (state.node_types_height < node_types_min)
        state.node_types_height = node_types_min;
      if (state.node_types_height > node_types_max)
        state.node_types_height = node_types_max;
    }

  if (state.tabs.size() > 1)
    {
      ImGui::Separator();
      draw_section_header("Modules");
      for (size_t i = 1; i < state.tabs.size(); ++i)
        {
          const std::string &tab_name = state.tabs[i].name;
          ImGui::PushID(static_cast<int>(i));
          if (ImGui::Selectable(tab_name.c_str()))
            AddNodeFromType(state,
                            "coral::Network",
                            tab_name,
                            SerializeTab(state.tabs[i], state));
          if (ImGui::BeginPopupContextItem("##module_ctx"))
            {
              if (ImGui::MenuItem("Delete Module"))
                {
                  state.module_delete_candidate  = tab_name;
                  state.show_module_delete_popup = true;
                }
              if (ImGui::MenuItem("Rename Module"))
                {
                  state.module_rename_candidate  = tab_name;
                  state.show_module_rename_popup = true;
                  std::snprintf(state.module_rename_buffer,
                                sizeof(state.module_rename_buffer),
                                "%s",
                                tab_name.c_str());
                }
              ImGui::EndPopup();
            }
          ImGui::PopID();
        }
    }

  ImGui::Separator();
  draw_section_header("Networks");
  ImGui::InputText("New Network",
                   state.new_tab_name,
                   sizeof(state.new_tab_name));
  if (ImGui::Button("Add Network"))
    {
      if (state.new_tab_name[0] != '\0')
        {
          NetworkTab tab;
          tab.name = state.new_tab_name;
          state.tabs.push_back(tab);
          state.new_tab_name[0]    = '\0';
          state.pending_tab_switch = static_cast<int>(state.tabs.size()) - 1;
          state.force_tab_select   = true;
          UpdateTabIndex(state);
          if (!state.tabs.empty())
            {
              NetworkTab       &main_tab = state.tabs[0];
              const std::string value    = SerializeTab(tab, state);
              AddNodeToTab(state,
                           main_tab,
                           "coral::Network",
                           state.tabs.back().name,
                           value);
              if (state.active_tab == 0)
                LoadTab(state, 0);
            }
        }
    }

  ImGui::Separator();
  if (!state.status.empty())
    ImGui::TextWrapped("%s", state.status.c_str());

  ImGui::Separator();
  draw_section_header("Logging");
  static const char *log_levels[] = {"0 - Off",
                                     "1 - Error",
                                     "2 - Warn",
                                     "3 - Info",
                                     "4 - Debug",
                                     "5 - Trace",
                                     "6 - Verbose",
                                     "7 - Verbose+",
                                     "8 - Verbose++",
                                     "9 - Verbose+++",
                                     "10 - All"};
  int                log_level    = state.log_level;
  if (ImGui::Combo(
        "Log Level", &log_level, log_levels, IM_ARRAYSIZE(log_levels)))
    state.log_level = log_level;
}

static void
ApplyPendingEditorStateUpdates(EditorState &state)
{
  SaveActiveTab(state);
  SyncNetworkNodesFromTabs(state);
  UpdateTabIndex(state);
  if (!state.pending_module_rename_from.empty() &&
      !state.pending_module_rename_to.empty())
    {
      const std::string from = state.pending_module_rename_from;
      const std::string to   = state.pending_module_rename_to;
      state.pending_module_rename_from.clear();
      state.pending_module_rename_to.clear();
      RenameModule(state, from, to);
      SyncNetworkNodesFromTabs(state);
      UpdateTabIndex(state);
    }
  if (!state.pending_module_delete_name.empty())
    {
      const std::string name = state.pending_module_delete_name;
      state.pending_module_delete_name.clear();
      DeleteModule(state, name);
      SyncNetworkNodesFromTabs(state);
      UpdateTabIndex(state);
    }
  if (state.pending_tab_switch >= 0 &&
      state.pending_tab_switch < static_cast<int>(state.tabs.size()))
    {
      SwitchToTab(state, state.pending_tab_switch);
      state.pending_tab_switch = -1;
    }
}

static void
DrawCanvasTabs(EditorState &state, ed::EditorContext *editor, float canvas_width)
{
  ImGui::BeginChild("##canvas_panel", ImVec2(canvas_width, 0.0f), false);
  const std::string tab_bar_id =
    "##network_tabs_" + std::to_string(state.tab_bar_uid);
  bool did_switch = false;
  if (ImGui::BeginTabBar(tab_bar_id.c_str(),
                         ImGuiTabBarFlags_AutoSelectNewTabs |
                           ImGuiTabBarFlags_Reorderable))
    {
      for (size_t i = 0; i < state.tabs.size(); ++i)
        {
          const std::string &tab_name = state.tabs[i].name;
          ImGuiTabItemFlags  flags    = 0;
          if (state.force_tab_select && static_cast<int>(i) == state.active_tab)
            flags |= ImGuiTabItemFlags_SetSelected;
          if (ImGui::BeginTabItem(tab_name.c_str(), nullptr, flags))
            {
              if (static_cast<int>(i) != state.active_tab)
                SwitchToTab(state, static_cast<int>(i));

              if (i > 0)
                {
                  if (ImGui::Button("Delete Module"))
                    {
                      state.module_delete_candidate  = tab_name;
                      state.show_module_delete_popup = true;
                    }
                  ImGui::SameLine();
                  if (ImGui::Button("Rename Module"))
                    {
                      state.module_rename_candidate  = tab_name;
                      state.show_module_rename_popup = true;
                      std::snprintf(state.module_rename_buffer,
                                    sizeof(state.module_rename_buffer),
                                    "%s",
                                    tab_name.c_str());
                    }
                  ImGui::Separator();
                }

              ed::SetCurrentEditor(editor);
              DrawEditor(state);
              ed::SetCurrentEditor(nullptr);
              ImGui::EndTabItem();
            }
        }
      ImGui::EndTabBar();
    }
  if (state.pending_tab_switch >= 0 &&
      state.pending_tab_switch < static_cast<int>(state.tabs.size()))
    {
      const int target_tab     = state.pending_tab_switch;
      state.pending_tab_switch = -1;
      SwitchToTab(state, target_tab);
      did_switch = true;
    }
  if (!did_switch)
    state.force_tab_select = false;
  ImGui::EndChild();
}

static void
DrawBackendPanel(EditorState &state)
{
  if (!state.right_collapsed)
    ImGui::BeginChild("##backend_panel", ImVec2(0.0f, 0.0f), true);

  ImGui::TextUnformatted("Backend");
#if CORAL_NODE_EDITOR_ENABLE_BACKEND_RUNNER
  ImGui::InputText("Path", state.backend_path, sizeof(state.backend_path));
  if (ImGui::Button("Save and Execute"))
    {
      SaveActiveTab(state);
      SyncNetworkNodesFromTabs(state);
      std::string error;
      if (state.tabs.empty())
        {
          state.console_output += "No network to save.\n";
        }
      else
        {
          json data = BuildNetworkJsonFrom(state.tabs[0].nodes,
                                           state.tabs[0].links,
                                           state.registry);
          if (!SaveJsonFile(state.network_path, data, error))
            {
              state.console_output += "Save failed: " + error + "\n";
            }
          else
            {
              const std::string cmd = std::string(state.backend_path) +
                                      " run " +
                                      std::string(state.network_path) + " 2>&1";
              state.console_output += "$ " + cmd + "\n";
              std::array<char, 256> buffer{};
              FILE                 *pipe = popen(cmd.c_str(), "r");
              if (!pipe)
                {
                  state.console_output += "Failed to start backend.\n";
                }
              else
                {
                  while (
                    fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
                    state.console_output += buffer.data();
                  pclose(pipe);
                }
              state.console_scroll_to_bottom = true;
            }
        }
    }
#else
  ImGui::TextWrapped(
    "Backend execution is disabled in this build. "
    "Use the separate runner/manipulator application to execute a network.");
#endif

  ImGui::SameLine();
  if (ImGui::Button("Clear Console"))
    state.console_output.clear();
  ImGui::Separator();
  ImGui::BeginChild("##console_panel", ImVec2(0.0f, 0.0f), true);
  ImGui::TextUnformatted(state.console_output.c_str());
  if (state.console_scroll_to_bottom)
    {
      ImGui::SetScrollHereY(1.0f);
      state.console_scroll_to_bottom = false;
    }
  ImGui::EndChild();

  if (!state.right_collapsed)
    ImGui::EndChild();
}

static void
DrawModalPopups(EditorState &state)
{
#if CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
  if (state.mesh_tool.open)
    ImGui::OpenPopup("Mesh Tool");
  DrawMeshTool(state);
#endif

  if (state.show_module_delete_popup && !state.module_delete_candidate.empty())
    ImGui::OpenPopup("Delete Module?");
  if (ImGui::BeginPopupModal("Delete Module?",
                             nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
    {
      ImGui::Text("Do you want to delete the module \"%s\"?",
                  state.module_delete_candidate.c_str());
      ImGui::Spacing();
      if (ImGui::Button("Delete"))
        {
          state.pending_module_delete_name = state.module_delete_candidate;
          state.module_delete_candidate.clear();
          state.show_module_delete_popup = false;
          ImGui::CloseCurrentPopup();
        }
      ImGui::SameLine();
      if (ImGui::Button("Keep"))
        {
          state.module_delete_candidate.clear();
          state.show_module_delete_popup = false;
          ImGui::CloseCurrentPopup();
        }
      ImGui::EndPopup();
    }

  if (state.show_module_rename_popup && !state.module_rename_candidate.empty())
    ImGui::OpenPopup("Rename Module?");
  if (ImGui::BeginPopupModal("Rename Module?",
                             nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize))
    {
      ImGui::Text("Rename module \"%s\" to:",
                  state.module_rename_candidate.c_str());
      ImGui::Spacing();
      ImGui::InputText("##new_module_name",
                       state.module_rename_buffer,
                       sizeof(state.module_rename_buffer));
      ImGui::Spacing();
      if (ImGui::Button("Rename"))
        {
          state.pending_module_rename_from = state.module_rename_candidate;
          state.pending_module_rename_to   = state.module_rename_buffer;
          state.module_rename_candidate.clear();
          state.module_rename_buffer[0] = '\0';
          state.show_module_rename_popup = false;
          ImGui::CloseCurrentPopup();
        }
      ImGui::SameLine();
      if (ImGui::Button("Cancel"))
        {
          state.module_rename_candidate.clear();
          state.module_rename_buffer[0] = '\0';
          state.show_module_rename_popup = false;
          ImGui::CloseCurrentPopup();
        }
      ImGui::EndPopup();
    }
}

static void
DrawLayout(EditorState &state, ed::EditorContext *editor)
{
  ApplyPendingEditorStateUpdates(state);

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::Begin("##CoralNodeEditor",
               nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
  const float avail_width = ImGui::GetContentRegionAvail().x;
  const float min_left    = 220.0f;
  const float max_left    = std::max(min_left, avail_width - 220.0f);
  if (state.left_width < min_left)
    state.left_width = min_left;
  if (state.left_width > max_left)
    state.left_width = max_left;

  const float splitter_width   = 8.0f;
  const float left_panel_width = state.left_collapsed ? 0.0f : state.left_width;
  if (!state.left_collapsed)
    {
      ImGui::BeginChild("##left_panel",
                        ImVec2(left_panel_width, 0.0f),
                        true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar);
      DrawSidebar(state);
      ImGui::EndChild();
      ImGui::SameLine(0.0f, 0.0f);
    }

  ImGui::BeginChild("##left_splitter", ImVec2(splitter_width, 0.0f), false);
  const float splitter_height = ImGui::GetContentRegionAvail().y;
  ImGui::InvisibleButton("##left_splitter_btn",
                         ImVec2(splitter_width, splitter_height));
  if (ImGui::IsItemHovered())
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  if (ImGui::IsItemActive() && !state.left_collapsed)
    state.left_width += ImGui::GetIO().MouseDelta.x;
  ImDrawList  *splitter_draw = ImGui::GetWindowDrawList();
  ImVec2       split_min     = ImGui::GetItemRectMin();
  ImVec2       split_max     = ImGui::GetItemRectMax();
  const ImVec2 arrow_center  = ImVec2((split_min.x + split_max.x) * 0.5f,
                                     (split_min.y + split_max.y) * 0.5f);
  const char  *left_arrow    = state.left_collapsed ? ">" : "<";
  ImVec2       left_arrow_pos(arrow_center.x - 6.0f, arrow_center.y - 8.0f);
  ImVec2       left_arrow_size(12.0f, 16.0f);
  ImGui::SetCursorScreenPos(left_arrow_pos);
  if (ImGui::InvisibleButton("##left_toggle", left_arrow_size))
    state.left_collapsed = !state.left_collapsed;
  splitter_draw->AddText(left_arrow_pos,
                         ImGui::GetColorU32(ImGuiCol_Text),
                         left_arrow);
  ImGui::EndChild();
  ImGui::SameLine(0.0f, 0.0f);

  ImGui::BeginChild("##right_panel", ImVec2(0.0f, 0.0f), false);
  const float right_width = ImGui::GetContentRegionAvail().x;
  const float min_backend = 260.0f;
  const float max_backend = std::max(min_backend, right_width - 260.0f);
  if (state.right_width < min_backend)
    state.right_width = min_backend;
  if (state.right_width > max_backend)
    state.right_width = max_backend;
  const float backend_width = state.right_collapsed ? 0.0f : state.right_width;
  const float canvas_width =
    std::max(0.0f, right_width - backend_width - splitter_width);
  DrawCanvasTabs(state, editor, canvas_width);

  ImGui::SameLine(0.0f, 0.0f);
  ImGui::BeginChild("##right_splitter", ImVec2(splitter_width, 0.0f), false);
  const float right_splitter_height = ImGui::GetContentRegionAvail().y;
  ImGui::InvisibleButton("##right_splitter_btn",
                         ImVec2(splitter_width, right_splitter_height));
  if (ImGui::IsItemHovered())
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  if (ImGui::IsItemActive() && !state.right_collapsed)
    state.right_width -= ImGui::GetIO().MouseDelta.x;
  ImVec2      right_min   = ImGui::GetItemRectMin();
  ImVec2      right_max   = ImGui::GetItemRectMax();
  const char *right_arrow = state.right_collapsed ? "<" : ">";
  ImVec2      right_arrow_pos((right_min.x + right_max.x) * 0.5f - 6.0f,
                         (right_min.y + right_max.y) * 0.5f - 8.0f);
  ImVec2      right_arrow_size(12.0f, 16.0f);
  ImGui::SetCursorScreenPos(right_arrow_pos);
  if (ImGui::InvisibleButton("##right_toggle", right_arrow_size))
    state.right_collapsed = !state.right_collapsed;
  splitter_draw->AddText(right_arrow_pos,
                         ImGui::GetColorU32(ImGuiCol_Text),
                         right_arrow);
  ImGui::EndChild();
  ImGui::SameLine(0.0f, 0.0f);
  DrawBackendPanel(state);
  ImGui::EndChild();
  ImGui::End();
  DrawModalPopups(state);
}

static void
SetupTrueTypeFonts(ImGuiIO &io)
{
  io.FontGlobalScale = 1.0f;

  io.Fonts->Clear();

  auto file_exists = [](const char *path) -> bool {
    if (!path || path[0] == '\0')
      return false;
    std::ifstream f(path);
    return f.good();
  };

  struct FontCandidate
  {
    const char *path;
    float       size_px;
  };

  const FontCandidate candidates[] = {
    {"bin/data/Play-Regular.ttf", 19.0f},
    {"bin/data/Oswald-Regular.ttf", 19.0f},
    {"bin/data/Cuprum-Bold.ttf", 19.0f},
    {"gui/imgui-node-editor/examples/data/Play-Regular.ttf", 19.0f},
    {"gui/imgui-node-editor/examples/data/Oswald-Regular.ttf", 19.0f},
    {"gui/imgui-node-editor/examples/data/Cuprum-Bold.ttf", 19.0f},
    {"../gui/imgui-node-editor/examples/data/Play-Regular.ttf", 19.0f},
    {"../gui/imgui-node-editor/examples/data/Oswald-Regular.ttf", 19.0f},
    {"../gui/imgui-node-editor/examples/data/Cuprum-Bold.ttf", 19.0f},
  };

  ImFont *font = nullptr;
  for (const auto &candidate : candidates)
    {
      if (!file_exists(candidate.path))
        continue;
      font = io.Fonts->AddFontFromFileTTF(candidate.path, candidate.size_px);
      if (font)
        break;
    }

  if (!font)
    {
      std::fprintf(stderr,
                   "Warning: failed to load .ttf fonts, using default font.\n");
      font = io.Fonts->AddFontDefault();
    }

  io.FontDefault = font;
}

int
main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  if (!glfwInit())
    return 1;

#if __APPLE__
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  GLFWwindow *window =
    glfwCreateWindow(1280, 720, "Coral Node Editor", nullptr, nullptr);
  if (!window)
    {
      glfwTerminate();
      return 1;
    }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigWindowsMoveFromTitleBarOnly = true;

  SetupTrueTypeFonts(io);

  ImGui::StyleColorsDark();
  ImGuiStyle &style    = ImGui::GetStyle();
  style.WindowRounding = 6.0f;
  style.FrameRounding  = 4.0f;
  style.ScrollbarSize  = 14.0f;
  style.FramePadding   = ImVec2(6.0f, 4.0f);
  style.ItemSpacing    = ImVec2(6.0f, 6.0f);

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  ed::Config config;
  config.SettingsFile       = "coral_node_editor.json";
  ed::EditorContext *editor = ed::CreateEditor(&config);
  ed::SetCurrentEditor(editor);
  ed::Style &editor_style                        = ed::GetStyle();
  editor_style.NodeRounding                      = 8.0f;
  editor_style.NodeBorderWidth                   = 2.0f;
  editor_style.HoveredNodeBorderWidth            = 3.0f;
  editor_style.SelectedNodeBorderWidth           = 3.0f;
  editor_style.Colors[ed::StyleColor_NodeBg]     = ImColor(30, 34, 40, 230);
  editor_style.Colors[ed::StyleColor_NodeBorder] = ImColor(200, 140, 60, 200);
  editor_style.Colors[ed::StyleColor_HovNodeBorder] =
    ImColor(240, 180, 90, 255);
  editor_style.Colors[ed::StyleColor_SelNodeBorder] =
    ImColor(255, 210, 120, 255);
  ed::EnableShortcuts(true);
  ed::SetCurrentEditor(nullptr);

  EditorState state;
  NetworkTab  main_tab;
  main_tab.name = "Main";
  state.tabs.push_back(main_tab);
  state.active_tab = 0;
  std::snprintf(state.registry_path,
                sizeof(state.registry_path),
                "%s",
                "node_types.json");
  std::snprintf(state.network_path,
                sizeof(state.network_path),
                "%s",
                "mwe.json");
#if CORAL_NODE_EDITOR_ENABLE_BACKEND_RUNNER
  std::snprintf(state.backend_path,
                sizeof(state.backend_path),
                "%s",
                "./dealii_backend.g");
#endif
  {
    std::ofstream(state.log_path.c_str(), std::ios::trunc);
  }
  LogDebug(state, 10, "Editor start");
  LoadRegistry(state);

  while (!glfwWindowShouldClose(window))
    {
      glfwPollEvents();

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      try
        {
          const int delete_down =
            glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
          const int backspace_down =
            glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
          const bool delete_pressed = delete_down && !state.delete_key_down;
          const bool backspace_pressed =
            backspace_down && !state.backspace_key_down;
          state.delete_key_down    = delete_down;
          state.backspace_key_down = backspace_down;

          if (delete_pressed || backspace_pressed)
            state.request_delete = true;

          if (!state.show_error_popup)
            DrawLayout(state, editor);
        }
      catch (const std::exception &e)
        {
          state.error_message    = std::string("Unhandled error: ") + e.what();
          state.show_error_popup = true;
        }
      catch (...)
        {
          state.error_message    = "Unhandled error: unknown exception.";
          state.show_error_popup = true;
        }

      if (state.show_error_popup)
        {
          ImGui::OpenPopup("Fatal Error");
          if (ImGui::BeginPopupModal("Fatal Error",
                                     nullptr,
                                     ImGuiWindowFlags_AlwaysAutoResize))
            {
              ImGui::TextWrapped("%s", state.error_message.c_str());
              if (ImGui::Button("Close"))
                {
                  ImGui::CloseCurrentPopup();
                  glfwSetWindowShouldClose(window, GL_TRUE);
                }
              ImGui::EndPopup();
            }
        }

      ImGui::Render();
      int display_w = 0;
      int display_h = 0;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
    }

  ed::DestroyEditor(editor);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
