#pragma once

#include <imgui.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
#  define CORAL_NODE_EDITOR_ENABLE_MANIPULATOR 0
#endif

#ifndef CORAL_NODE_EDITOR_ENABLE_BACKEND_RUNNER
#  define CORAL_NODE_EDITOR_ENABLE_BACKEND_RUNNER 0
#endif

#if CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
#  include <deal.II/grid/tria.h>
#endif

struct RegistryArgument
{
  std::string name;
  std::string type;
  std::string connection_type;
};

struct RegistryNodeType
{
  std::string                   type;
  std::string                   node_type;
  std::vector<RegistryArgument> arguments;
  std::vector<int>              inputs;
  std::vector<int>              outputs;
  std::string                   default_value;
};

struct EditorNode
{
  unsigned int id = 0;
  std::string  type;
  std::string  name;
  std::string  value;
  char         value_buffer[256] = {};
  bool         value_buffer_init = false;
  char         name_buffer[256]  = {};
  bool         name_buffer_init  = false;
  bool         needs_position    = false;
  ImVec2       desired_position  = ImVec2(0.0f, 0.0f);
};

struct EditorLink
{
  unsigned int id            = 0;
  unsigned int source_node   = 0;
  unsigned int target_node   = 0;
  unsigned int source_output = 0;
  unsigned int target_input  = 0;
};

struct NetworkTab
{
  std::string                        name;
  std::map<unsigned int, EditorNode> nodes;
  std::map<unsigned int, EditorLink> links;
  unsigned int                       next_node_id = 1;
  unsigned int                       next_link_id = 1;
};

struct EditorState
{
  std::map<std::string, RegistryNodeType> registry;
  std::vector<NetworkTab>                 tabs;
  int                                     active_tab         = 0;
  int                                     pending_tab_switch = -1;
  std::map<std::string, int>              tab_name_index;
  std::map<unsigned int, EditorNode>      nodes;
  std::map<unsigned int, EditorLink>      links;
  unsigned int                            next_node_id       = 1;
  unsigned int                            next_link_id       = 1;
  char                                    registry_path[256] = {};
  char                                    network_path[256]  = {};
  char                                    new_tab_name[128]  = {};
  std::string                             status;
  std::string                             log_path  = "coral_node_editor.log";
  int                                     log_level = 3;
  bool                                    request_delete     = false;
  bool                                    delete_key_down    = false;
  bool                                    backspace_key_down = false;
  bool                                    text_input_active  = false;

#if CORAL_NODE_EDITOR_ENABLE_BACKEND_RUNNER
  char backend_path[256] = {};
#endif

  std::string console_output;
  bool        console_scroll_to_bottom = false;
  bool        show_error_popup         = false;
  std::string error_message;
  bool        force_tab_select  = false;
  int         tab_bar_uid       = 0;
  bool        left_collapsed    = false;
  bool        right_collapsed   = false;
  float       left_width        = 300.0f;
  float       right_width       = 360.0f;
  bool        request_focus     = false;
  float       node_types_height = 260.0f;
  bool        show_module_delete_popup = false;
  std::string module_delete_candidate;
  std::string pending_module_delete_name;
  bool        show_module_rename_popup = false;
  std::string module_rename_candidate;
  std::string pending_module_rename_from;
  std::string pending_module_rename_to;
  char        module_rename_buffer[128] = {};

#if CORAL_NODE_EDITOR_ENABLE_MANIPULATOR
  struct MeshToolState
  {
    bool         open            = false;
    bool         pending_load     = false;
    unsigned int target_node_id   = 0;
    int          target_tab_index = -1;
    std::string  load_error;

    dealii::Triangulation<2, 2> tria;
    bool                         has_tria = false;

    struct CellTri
    {
      ImVec2       p0;
      ImVec2       p1;
      ImVec2       p2;
      unsigned int cell_index  = 0;
      unsigned int material_id = 0;
    };

    struct BoundarySeg
    {
      ImVec2       p0;
      ImVec2       p1;
      unsigned int cell_index  = 0;
      unsigned int face_no     = 0;
      unsigned int boundary_id = 0;
    };

    std::vector<CellTri>     cell_tris;
    std::vector<BoundarySeg> boundary_segs;
    std::map<unsigned int, unsigned int> boundary_counts;
    std::map<unsigned int, unsigned int> material_counts;
    std::map<unsigned int, dealii::Triangulation<2, 2>::active_cell_iterator>
      cell_by_index;

    bool   view_initialized = false;
    ImVec2 view_center      = ImVec2(0.0f, 0.0f);
    float  view_scale       = 100.0f;

    enum class Mode
    {
      boundary = 0,
      cell     = 1,
    };
    Mode mode = Mode::boundary;

    int selected_boundary_id = -1;
    int selected_material_id = -1;
    std::set<std::pair<unsigned int, unsigned int>> selected_boundary_faces;
    std::set<unsigned int>                          selected_cells;

    unsigned int assign_boundary_id = 0;
    unsigned int assign_material_id = 0;

    float left_panel_width = 320.0f;

    bool   box_select_active = false;
    ImVec2 box_start_screen  = ImVec2(0.0f, 0.0f);
    ImVec2 box_end_screen    = ImVec2(0.0f, 0.0f);

    float cell_edge_thickness = 1.0f;
  } mesh_tool;
#endif
};
