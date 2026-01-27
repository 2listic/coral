#include "coral_manipulator/manipulator.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <dlfcn.h>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <deal.II/base/exceptions.h>
#include <deal.II/base/numbers.h>
#include <deal.II/base/types.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/grid_out.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>

#include <vtkglad/include/glad/gl.h>

#include <vtkActor.h>
#include <vtkAbstractArray.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCellPicker.h>
#include <vtkCellType.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkDataSetMapper.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkGenericRenderWindowInteractor.h>
#include <vtkGeometryFilter.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkIntArray.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkHardwareSelector.h>
#include <vtkIdTypeArray.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkThreshold.h>
#include <vtkUnstructuredGrid.h>
#include <vtkVariant.h>

namespace coral::manipulator
{
namespace
{
struct ButtonRowLayout
{
  float w = 120.0f;
  float h = 0.0f; // use default frame height (from padding)
};

static ButtonRowLayout
GetButtonRowLayout(const int count, const float min_w = 110.0f)
{
  const float avail = ImGui::GetContentRegionAvail().x;
  const float gap   = ImGui::GetStyle().ItemSpacing.x;
  const float w =
    std::max(min_w, (avail - gap * static_cast<float>(count - 1)) /
                      static_cast<float>(std::max(1, count)));
  return ButtonRowLayout{w, 0.0f};
}

class TallButtonStyle
{
public:
  TallButtonStyle()
  {
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(pad.x, std::max(pad.y, 10.0f)));
  }
  ~TallButtonStyle() { ImGui::PopStyleVar(); }
};

static bool
IsFlatManifoldId(const unsigned int id)
{
  return id == static_cast<unsigned int>(dealii::numbers::flat_manifold_id);
}

static bool
ManifoldIdMatches(const dealii::types::manifold_id id, const int selected)
{
  if (selected < 0)
    return id == dealii::numbers::flat_manifold_id;
  return static_cast<int>(id) == selected;
}

static int
RoundScrollSteps(const double v)
{
  if (v > 0.0)
    return static_cast<int>(v + 0.5);
  if (v < 0.0)
    return static_cast<int>(v - 0.5);
  return 0;
}

static vtkOpenGLRenderWindow::VTKOpenGLAPIProc
GlfwGetProcAddress(void *userptr, const char *name)
{
  (void)userptr;
  if (auto *p = glfwGetProcAddress(name))
    return reinterpret_cast<vtkOpenGLRenderWindow::VTKOpenGLAPIProc>(p);

  if (auto *p = dlsym(RTLD_DEFAULT, name))
    return reinterpret_cast<vtkOpenGLRenderWindow::VTKOpenGLAPIProc>(p);

  return nullptr;
}

static GLADapiproc
GlfwGladGetProcAddress(void *userptr, const char *name)
{
  return reinterpret_cast<GLADapiproc>(GlfwGetProcAddress(userptr, name));
}

struct FilePicker
{
  bool        open                = false;
  bool        pending_open_popup   = false;
  std::string title               = "Open";
  std::string current_dir;
  std::string selected_name;
  std::string filter_extension; // e.g. ".vtk"
  std::string error;

  void open_modal(std::string title_in,
                  std::string initial_dir,
                  std::string filter_extension_in)
  {
    title            = std::move(title_in);
    current_dir      = std::move(initial_dir);
    filter_extension = std::move(filter_extension_in);
    selected_name.clear();
    error.clear();
    open = true;
    pending_open_popup = true;
  }

  std::optional<std::filesystem::path> draw()
  {
    std::optional<std::filesystem::path> result;
    if (!open)
      return result;

    const ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (pending_open_popup)
      {
        ImGui::OpenPopup(title.c_str());
        pending_open_popup = false;
      }
    if (!ImGui::BeginPopupModal(title.c_str(), &open, flags))
      return result;

    if (current_dir.empty())
      current_dir = std::filesystem::current_path().string();

    std::filesystem::path dir(current_dir);
    if (!std::filesystem::exists(dir))
      dir = std::filesystem::current_path();

    ImGui::TextUnformatted("Directory");
    ImGui::SetNextItemWidth(520.0f);
    ImGui::InputText("##dir", &current_dir);

    if (ImGui::Button("Up"))
      {
        const auto parent = dir.parent_path();
        if (!parent.empty())
          current_dir = parent.string();
      }

    ImGui::Separator();

    std::vector<std::filesystem::directory_entry> entries;
    try
      {
        for (const auto &entry : std::filesystem::directory_iterator(dir))
          entries.push_back(entry);
      }
    catch (const std::exception &e)
      {
        error = e.what();
      }

    std::sort(entries.begin(),
              entries.end(),
              [](const auto &a, const auto &b) {
                const bool ad = a.is_directory();
                const bool bd = b.is_directory();
                if (ad != bd)
                  return ad > bd;
                return a.path().filename().string() < b.path().filename().string();
              });

    ImGui::BeginChild("##file_list", ImVec2(560.0f, 320.0f), true);
    for (const auto &entry : entries)
      {
        const auto        name = entry.path().filename().string();
        const bool        is_dir = entry.is_directory();
        const std::string label = is_dir ? (name + "/") : name;

        if (!is_dir && !filter_extension.empty() &&
            entry.path().extension() != filter_extension)
          continue;

        const bool selected = selected_name == name;
        if (ImGui::Selectable(label.c_str(), selected))
          selected_name = name;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
          {
            if (is_dir)
              {
                current_dir   = entry.path().string();
                selected_name = {};
              }
            else
              {
                result = entry.path();
                open   = false;
                ImGui::CloseCurrentPopup();
              }
          }
      }
    ImGui::EndChild();

    if (!error.empty())
      {
        ImGui::Separator();
        ImGui::TextWrapped("%s", error.c_str());
      }

    ImGui::Separator();

    const bool can_open = !selected_name.empty() &&
                          std::filesystem::is_regular_file(dir / selected_name);
    if (can_open)
      {
        if (ImGui::Button("Open"))
          {
            result = dir / selected_name;
            open   = false;
            ImGui::CloseCurrentPopup();
          }
      }
    else
      {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
        ImGui::Button("Open");
        ImGui::PopStyleVar();
      }

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      {
        open = false;
        ImGui::CloseCurrentPopup();
      }

    ImGui::EndPopup();
    return result;
  }
};

struct NewGridDialog
{
  bool        open = false;
  bool        pending_open_popup = false;
  std::string name;
  std::string arguments;
  int         refinements = 0;
  std::string error;

  void open_modal()
  {
    open = true;
    error.clear();
    if (name.empty())
      name = "hyper_cube";
    if (arguments.empty())
      arguments = "0 : 1 : true";
    pending_open_popup = true;
  }
};

struct SaveDialog
{
  bool        open               = false;
  bool        pending_open_popup  = false;
  std::string directory;
  std::string filename = "mesh";
  int         format   = 1; // 0: vtk, 1: vtu
  std::string error;

  void open_modal(std::string initial_dir)
  {
    open      = true;
    error.clear();
    directory = std::move(initial_dir);
    if (directory.empty())
      directory = std::filesystem::current_path().string();
    pending_open_popup = true;
  }

  struct Result
  {
    std::filesystem::path path;
    int                   format;
  };

  std::optional<Result> draw()
  {
    std::optional<Result> result;
    if (!open)
      return result;

    const ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (pending_open_popup)
      {
        ImGui::OpenPopup("Save Mesh");
        pending_open_popup = false;
      }
    if (!ImGui::BeginPopupModal("Save Mesh", &open, flags))
      return result;

    ImGui::TextUnformatted("Directory");
    ImGui::SetNextItemWidth(520.0f);
    ImGui::InputText("##save_dir", &directory);

    ImGui::TextUnformatted("Filename");
    ImGui::SetNextItemWidth(520.0f);
    ImGui::InputText("##save_name", &filename);

    const char *formats[] = {"VTK (.vtk)", "VTU (.vtu)"};
    ImGui::Combo("Format", &format, formats, 2);

    if (!error.empty())
      {
        ImGui::Separator();
        ImGui::TextWrapped("%s", error.c_str());
      }

    const char *ext = (format == 0) ? ".vtk" : ".vtu";
    std::filesystem::path out_dir(directory.empty() ? "." : directory);
    std::filesystem::path out_path = out_dir / filename;
    if (out_path.extension() != ext)
      out_path.replace_extension(ext);

    const bool can_save = !filename.empty();
    if (can_save)
      {
        if (ImGui::Button("Save"))
          {
            result = Result{out_path, format};
            open   = false;
            ImGui::CloseCurrentPopup();
          }
      }
    else
      {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
        ImGui::Button("Save");
        ImGui::PopStyleVar();
      }

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      {
        open = false;
        ImGui::CloseCurrentPopup();
      }

    ImGui::EndPopup();
    return result;
  }
};

struct MeshState
{
  dealii::Triangulation<2, 2> tria;

  std::map<unsigned int, dealii::Triangulation<2, 2>::active_cell_iterator>
    cell_by_index;

  enum class Mode
  {
    boundary = 0,
    cell     = 1,
  };
  Mode mode = Mode::boundary;

  std::map<unsigned int, unsigned int> boundary_counts;
  std::map<unsigned int, unsigned int> material_counts;
  std::map<unsigned int, unsigned int> face_manifold_counts;
  std::map<unsigned int, unsigned int> cell_manifold_counts;

  std::map<unsigned int, ImVec4> boundary_colors;
  std::map<unsigned int, ImVec4> material_colors;

  int selected_boundary_id = -1;
  int selected_material_id = -1;
  int selected_face_manifold_id = -1;
  int selected_cell_manifold_id = -1;

  std::set<std::pair<unsigned int, unsigned int>> selected_boundary_faces;
  std::set<unsigned int>                          selected_cells;

  std::optional<std::pair<unsigned int, unsigned int>> hovered_boundary_face;
  std::optional<unsigned int>                          hovered_cell;

  unsigned int assign_boundary_id = 0;
  unsigned int assign_material_id = 0;
  unsigned int assign_face_manifold_id = 0;
  unsigned int assign_cell_manifold_id = 0;

  std::string current_label;
};

struct VtkScene
{
  vtkNew<vtkGenericOpenGLRenderWindow> render_window;
  vtkNew<vtkRenderer>                  renderer;
  vtkNew<vtkGenericRenderWindowInteractor> interactor;
  vtkNew<vtkInteractorStyleTrackballCamera> style;

  vtkNew<vtkUnstructuredGrid> grid;
  vtkNew<vtkPolyData>         boundary;

  vtkNew<vtkDataSetMapper> cell_mapper;
  vtkNew<vtkActor>         cell_actor;
  vtkNew<vtkLookupTable>   cell_lut;

  vtkNew<vtkThreshold>     cell_selected_threshold;
  vtkNew<vtkDataSetMapper> cell_selected_mapper;
  vtkNew<vtkActor>         cell_selected_actor;
  vtkNew<vtkDataSetMapper> cell_selected_fill_mapper;
  vtkNew<vtkActor>         cell_selected_fill_actor;

  vtkNew<vtkThreshold>     cell_hover_threshold;
  vtkNew<vtkDataSetMapper> cell_hover_mapper;
  vtkNew<vtkActor>         cell_hover_actor;

  vtkNew<vtkThreshold>      boundary_selected_threshold;
  vtkNew<vtkGeometryFilter> boundary_selected_geom;
  vtkNew<vtkPolyDataMapper> boundary_selected_mapper;
  vtkNew<vtkActor>          boundary_selected_actor;

  vtkNew<vtkThreshold>      boundary_hover_threshold;
  vtkNew<vtkGeometryFilter> boundary_hover_geom;
  vtkNew<vtkPolyDataMapper> boundary_hover_mapper;
  vtkNew<vtkActor>          boundary_hover_actor;

  vtkNew<vtkPolyDataMapper> boundary_mapper;
  vtkNew<vtkActor>          boundary_actor;
  vtkNew<vtkLookupTable>    boundary_lut;

  vtkNew<vtkCellPicker> picker;

  vtkNew<vtkCallbackCommand> make_current_cb;
  vtkNew<vtkCallbackCommand> is_current_cb;
  vtkNew<vtkCallbackCommand> supports_gl_cb;
  vtkNew<vtkCallbackCommand> is_direct_cb;
  vtkNew<vtkCallbackCommand> frame_cb;

  int viewport_w = 1;
  int viewport_h = 1;
};

static vtkSmartPointer<vtkIntArray>
NewIntCellArray(const char *name)
{
  auto arr = vtkSmartPointer<vtkIntArray>::New();
  arr->SetName(name);
  arr->SetNumberOfComponents(1);
  return arr;
}

static ImVec4
DefaultColorForId(const unsigned int id)
{
  // Deterministic HSV palette based on the id.
  const float h = std::fmod(0.6180339887f * static_cast<float>(id), 1.0f);
  const float s = 0.55f;
  const float v = 0.95f;

  const float i = std::floor(h * 6.0f);
  const float f = h * 6.0f - i;
  const float p = v * (1.0f - s);
  const float q = v * (1.0f - f * s);
  const float t = v * (1.0f - (1.0f - f) * s);

  float r = v, g = v, b = v;
  switch (static_cast<int>(i) % 6)
    {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
      default: break;
    }
  return ImVec4(r, g, b, 1.0f);
}

static void
RebuildCounts(MeshState &state)
{
  state.boundary_counts.clear();
  state.material_counts.clear();
  state.face_manifold_counts.clear();
  state.cell_manifold_counts.clear();

  for (auto cell = state.tria.begin_active(); cell != state.tria.end(); ++cell)
    {
      state.material_counts[static_cast<unsigned int>(cell->material_id())] +=
        1u;
      state.cell_manifold_counts[static_cast<unsigned int>(cell->manifold_id())] +=
        1u;

      for (unsigned int f = 0; f < cell->n_faces(); ++f)
        {
          const auto face = cell->face(f);
          if (!face->at_boundary())
            continue;
          state.boundary_counts[static_cast<unsigned int>(face->boundary_id())] +=
            1u;
          state.face_manifold_counts[static_cast<unsigned int>(face->manifold_id())] +=
            1u;
        }
    }

  // Ensure default colors exist for discovered ids.
  for (const auto &[bid, count] : state.boundary_counts)
    {
      (void)count;
      if (state.boundary_colors.count(bid) == 0)
        state.boundary_colors[bid] = DefaultColorForId(bid);
    }
  for (const auto &[mid, count] : state.material_counts)
    {
      (void)count;
      if (state.material_colors.count(mid) == 0)
        state.material_colors[mid] = DefaultColorForId(mid);
    }
}

static void
RebuildCellIndexMap(MeshState &state)
{
  state.cell_by_index.clear();
  for (auto cell = state.tria.begin_active(); cell != state.tria.end(); ++cell)
    state.cell_by_index[cell->active_cell_index()] = cell;
}

static void
BuildVtkMeshesFromTriangulation(const MeshState &state, VtkScene &scene)
{
  scene.grid->Reset();
  scene.boundary->Reset();

  const auto &vertices      = state.tria.get_vertices();
  const auto &used_vertices = state.tria.get_used_vertices();

  std::vector<vtkIdType> vertex_to_vtk(vertices.size(), -1);
  vtkNew<vtkPoints>      points;
  points->SetDataTypeToDouble();

  for (size_t i = 0; i < vertices.size(); ++i)
    {
      if (!used_vertices[i])
        continue;
      const vtkIdType pid =
        points->InsertNextPoint(vertices[i][0], vertices[i][1], 0.0);
      vertex_to_vtk[i] = pid;
    }

  scene.grid->SetPoints(points);

  auto active_cell_index = NewIntCellArray("active_cell_index");
  auto material_id       = NewIntCellArray("material_id");
  auto cell_selected     = NewIntCellArray("selected");
  auto cell_hovered      = NewIntCellArray("hovered");

  for (auto cell = state.tria.begin_active(); cell != state.tria.end(); ++cell)
    {
      const unsigned int cell_index =
        static_cast<unsigned int>(cell->active_cell_index());
      const unsigned int mat =
        static_cast<unsigned int>(cell->material_id());

      const unsigned int n_vertices = cell->reference_cell().n_vertices();
      if (n_vertices == 3)
        {
          vtkIdType ids[3] = {};
          for (unsigned int v = 0; v < 3; ++v)
            ids[v] = vertex_to_vtk[cell->vertex_index(v)];
          scene.grid->InsertNextCell(VTK_TRIANGLE, 3, ids);
        }
      else if (n_vertices == 4)
        {
          vtkIdType ids[4] = {};
          ids[0]           = vertex_to_vtk[cell->vertex_index(0)];
          ids[1]           = vertex_to_vtk[cell->vertex_index(1)];
          ids[2]           = vertex_to_vtk[cell->vertex_index(3)];
          ids[3]           = vertex_to_vtk[cell->vertex_index(2)];
          scene.grid->InsertNextCell(VTK_QUAD, 4, ids);
        }
      else
        {
          continue;
        }

      active_cell_index->InsertNextValue(static_cast<int>(cell_index));
      material_id->InsertNextValue(static_cast<int>(mat));
      cell_selected->InsertNextValue(0);
      cell_hovered->InsertNextValue(0);
    }

  scene.grid->GetCellData()->AddArray(active_cell_index);
  scene.grid->GetCellData()->AddArray(material_id);
  scene.grid->GetCellData()->AddArray(cell_selected);
  scene.grid->GetCellData()->AddArray(cell_hovered);

  vtkNew<vtkCellArray> lines;
  auto                 boundary_cell_index = NewIntCellArray("cell_index");
  auto                 boundary_face_no    = NewIntCellArray("face_no");
  auto                 boundary_id         = NewIntCellArray("boundary_id");
  auto                 boundary_selected   = NewIntCellArray("selected");
  auto                 boundary_hovered    = NewIntCellArray("hovered");

  for (auto cell = state.tria.begin_active(); cell != state.tria.end(); ++cell)
    {
      const unsigned int cell_index =
        static_cast<unsigned int>(cell->active_cell_index());
      for (unsigned int f = 0; f < cell->n_faces(); ++f)
        {
          const auto face = cell->face(f);
          if (!face->at_boundary())
            continue;

          vtkIdType ids[2] = {};
          ids[0] = vertex_to_vtk[face->vertex_index(0)];
          ids[1] = vertex_to_vtk[face->vertex_index(1)];
          lines->InsertNextCell(2, ids);

          boundary_cell_index->InsertNextValue(static_cast<int>(cell_index));
          boundary_face_no->InsertNextValue(static_cast<int>(f));
          boundary_id->InsertNextValue(
            static_cast<int>(static_cast<unsigned int>(face->boundary_id())));
          boundary_selected->InsertNextValue(0);
          boundary_hovered->InsertNextValue(0);
        }
    }

  scene.boundary->SetPoints(points);
  scene.boundary->SetLines(lines);
  scene.boundary->GetCellData()->AddArray(boundary_cell_index);
  scene.boundary->GetCellData()->AddArray(boundary_face_no);
  scene.boundary->GetCellData()->AddArray(boundary_id);
  scene.boundary->GetCellData()->AddArray(boundary_selected);
  scene.boundary->GetCellData()->AddArray(boundary_hovered);
}

static vtkIntArray *
GetCellArray(vtkDataSet *dataset, const char *name)
{
  if (!dataset)
    return nullptr;
  return vtkIntArray::SafeDownCast(dataset->GetCellData()->GetArray(name));
}

static void
UpdateVtkSelectionArrays(const MeshState &state, VtkScene &scene);

static bool
HitOnAnyGeometry(VtkScene &scene, const int x, const int y)
{
  scene.picker->InitializePickList();
  scene.picker->AddPickList(scene.cell_actor);
  scene.picker->AddPickList(scene.boundary_actor);
  if (!scene.picker->Pick(x, y, 0.0, scene.renderer))
    return false;
  vtkDataSet *ds = vtkDataSet::SafeDownCast(scene.picker->GetDataSet());
  if (!ds)
    return false;
  return scene.picker->GetCellId() >= 0;
}

static void
UpdateRendererViewport(VtkScene &scene,
                       const double xmin,
                       const double ymin,
                       const double xmax,
                       const double ymax)
{
  const double clamped_xmin = std::max(0.0, std::min(1.0, xmin));
  const double clamped_ymin = std::max(0.0, std::min(1.0, ymin));
  const double clamped_xmax = std::max(0.0, std::min(1.0, xmax));
  const double clamped_ymax = std::max(0.0, std::min(1.0, ymax));
  scene.renderer->SetViewport(clamped_xmin, clamped_ymin, clamped_xmax, clamped_ymax);
}

static void
ApplyBoxSelection(MeshState &state,
                  VtkScene  &scene,
                  const int  x0,
                  const int  y0,
                  const int  x1,
                  const int  y1,
                  const bool add_mode,
                  const bool subtract_mode)
{
  const bool replace_mode = !add_mode && !subtract_mode;
  const int  xmin         = std::min(x0, x1);
  const int  xmax         = std::max(x0, x1);
  const int  ymin         = std::min(y0, y1);
  const int  ymax         = std::max(y0, y1);

  vtkNew<vtkHardwareSelector> selector;
  selector->SetRenderer(scene.renderer);
  selector->SetArea(xmin, ymin, xmax, ymax);
  selector->SetFieldAssociation(vtkDataObject::FIELD_ASSOCIATION_CELLS);

  // Ensure buffers are up-to-date for selection.
  scene.render_window->Render();

  vtkSmartPointer<vtkSelection> sel;
  try
    {
      sel = selector->Select();
    }
  catch (...)
    {
      return;
    }

  if (!sel)
    return;

  if (state.mode == MeshState::Mode::cell)
    {
      if (replace_mode)
        state.selected_cells.clear();

      vtkIntArray *active_cell_index = GetCellArray(scene.grid, "active_cell_index");
      if (!active_cell_index)
        return;

      for (unsigned int n = 0; n < sel->GetNumberOfNodes(); ++n)
        {
          vtkSelectionNode *node = sel->GetNode(n);
          if (!node)
            continue;

          vtkAbstractArray *ids = node->GetSelectionList();
          if (!ids)
            continue;

          for (vtkIdType i = 0; i < ids->GetNumberOfTuples(); ++i)
            {
              const vtkIdType cid =
                static_cast<vtkIdType>(ids->GetVariantValue(i).ToLongLong());
              if (cid < 0 || cid >= active_cell_index->GetNumberOfTuples())
                continue;

              const unsigned int idx =
                static_cast<unsigned int>(active_cell_index->GetValue(cid));
              if (subtract_mode)
                state.selected_cells.erase(idx);
              else
                state.selected_cells.insert(idx);
            }
        }

      UpdateVtkSelectionArrays(state, scene);
    }
  else
    {
      if (replace_mode)
        state.selected_boundary_faces.clear();

      vtkIntArray *cell_index = vtkIntArray::SafeDownCast(
        scene.boundary->GetCellData()->GetArray("cell_index"));
      vtkIntArray *face_no = vtkIntArray::SafeDownCast(
        scene.boundary->GetCellData()->GetArray("face_no"));
      if (!cell_index || !face_no)
        return;

      for (unsigned int n = 0; n < sel->GetNumberOfNodes(); ++n)
        {
          vtkSelectionNode *node = sel->GetNode(n);
          if (!node)
            continue;

          vtkAbstractArray *ids = node->GetSelectionList();
          if (!ids)
            continue;

          for (vtkIdType i = 0; i < ids->GetNumberOfTuples(); ++i)
            {
              const vtkIdType cid =
                static_cast<vtkIdType>(ids->GetVariantValue(i).ToLongLong());
              if (cid < 0 || cid >= cell_index->GetNumberOfTuples())
                continue;

              const unsigned int c =
                static_cast<unsigned int>(cell_index->GetValue(cid));
              const unsigned int f =
                static_cast<unsigned int>(face_no->GetValue(cid));
              const auto key = std::make_pair(c, f);

              if (subtract_mode)
                state.selected_boundary_faces.erase(key);
              else
                state.selected_boundary_faces.insert(key);
            }
        }

      UpdateVtkSelectionArrays(state, scene);
    }
}

static void
UpdateVtkSelectionArrays(const MeshState &state, VtkScene &scene)
{
  vtkIntArray *cell_selected = GetCellArray(scene.grid, "selected");
  if (cell_selected)
    {
      for (vtkIdType i = 0; i < cell_selected->GetNumberOfTuples(); ++i)
        cell_selected->SetValue(i, 0);

      vtkIntArray *active_cell_index =
        GetCellArray(scene.grid, "active_cell_index");
      if (active_cell_index)
        {
          for (vtkIdType i = 0; i < active_cell_index->GetNumberOfTuples(); ++i)
            {
              const unsigned int idx =
                static_cast<unsigned int>(active_cell_index->GetValue(i));
              if (state.selected_cells.count(idx) > 0)
                cell_selected->SetValue(i, 1);
            }
        }
      cell_selected->Modified();
    }

  vtkIntArray *boundary_selected = vtkIntArray::SafeDownCast(
    scene.boundary->GetCellData()->GetArray("selected"));
  if (boundary_selected)
    {
      for (vtkIdType i = 0; i < boundary_selected->GetNumberOfTuples(); ++i)
        boundary_selected->SetValue(i, 0);

      vtkIntArray *cell_index = vtkIntArray::SafeDownCast(
        scene.boundary->GetCellData()->GetArray("cell_index"));
      vtkIntArray *face_no = vtkIntArray::SafeDownCast(
        scene.boundary->GetCellData()->GetArray("face_no"));

      if (cell_index && face_no)
        {
          for (vtkIdType i = 0; i < boundary_selected->GetNumberOfTuples(); ++i)
            {
              const unsigned int c =
                static_cast<unsigned int>(cell_index->GetValue(i));
              const unsigned int f =
                static_cast<unsigned int>(face_no->GetValue(i));
              if (state.selected_boundary_faces.count({c, f}) > 0)
                boundary_selected->SetValue(i, 1);
            }
        }
      boundary_selected->Modified();
    }

  scene.grid->Modified();
  scene.boundary->Modified();
}

static void
UpdateVtkHoverArrays(const MeshState &state, VtkScene &scene)
{
  vtkIntArray *cell_hovered = GetCellArray(scene.grid, "hovered");
  if (cell_hovered)
    {
      for (vtkIdType i = 0; i < cell_hovered->GetNumberOfTuples(); ++i)
        cell_hovered->SetValue(i, 0);

      vtkIntArray *active_cell_index =
        GetCellArray(scene.grid, "active_cell_index");
      if (active_cell_index && state.hovered_cell)
        {
          for (vtkIdType i = 0; i < active_cell_index->GetNumberOfTuples(); ++i)
            {
              const unsigned int idx =
                static_cast<unsigned int>(active_cell_index->GetValue(i));
              if (idx == *state.hovered_cell)
                {
                  cell_hovered->SetValue(i, 1);
                  break;
                }
            }
        }
      cell_hovered->Modified();
    }

  vtkIntArray *boundary_hovered = vtkIntArray::SafeDownCast(
    scene.boundary->GetCellData()->GetArray("hovered"));
  if (boundary_hovered)
    {
      for (vtkIdType i = 0; i < boundary_hovered->GetNumberOfTuples(); ++i)
        boundary_hovered->SetValue(i, 0);

      vtkIntArray *cell_index = vtkIntArray::SafeDownCast(
        scene.boundary->GetCellData()->GetArray("cell_index"));
      vtkIntArray *face_no = vtkIntArray::SafeDownCast(
        scene.boundary->GetCellData()->GetArray("face_no"));

      if (cell_index && face_no && state.hovered_boundary_face)
        {
          const unsigned int hc = state.hovered_boundary_face->first;
          const unsigned int hf = state.hovered_boundary_face->second;
          for (vtkIdType i = 0; i < boundary_hovered->GetNumberOfTuples(); ++i)
            {
              const unsigned int c =
                static_cast<unsigned int>(cell_index->GetValue(i));
              const unsigned int f =
                static_cast<unsigned int>(face_no->GetValue(i));
              if (c == hc && f == hf)
                {
                  boundary_hovered->SetValue(i, 1);
                  break;
                }
            }
        }
      boundary_hovered->Modified();
    }

  scene.grid->Modified();
  scene.boundary->Modified();
}

static void
UpdateLutForMaterialIds(const MeshState &state, VtkScene &scene)
{
  unsigned int max_id = 0;
  for (const auto &[id, count] : state.material_counts)
    {
      (void)count;
      max_id = std::max(max_id, id);
    }
  const int n = std::max(1, static_cast<int>(max_id + 1));
  scene.cell_lut->SetRange(0.0, static_cast<double>(n - 1));
  scene.cell_lut->SetNumberOfTableValues(n);
  scene.cell_lut->Build();

  for (int i = 0; i < n; ++i)
    {
      ImVec4 c = DefaultColorForId(static_cast<unsigned int>(i));
      if (auto it = state.material_colors.find(static_cast<unsigned int>(i));
          it != state.material_colors.end())
        c = it->second;
      scene.cell_lut->SetTableValue(i, c.x, c.y, c.z, 1.0);
    }
  scene.cell_lut->Modified();
}

static void
UpdateLutForBoundaryIds(const MeshState &state, VtkScene &scene)
{
  unsigned int max_id = 0;
  for (const auto &[id, count] : state.boundary_counts)
    {
      (void)count;
      max_id = std::max(max_id, id);
    }
  const int n = std::max(1, static_cast<int>(max_id + 1));
  scene.boundary_lut->SetRange(0.0, static_cast<double>(n - 1));
  scene.boundary_lut->SetNumberOfTableValues(n);
  scene.boundary_lut->Build();

  for (int i = 0; i < n; ++i)
    {
      ImVec4 c = DefaultColorForId(static_cast<unsigned int>(i));
      if (auto it = state.boundary_colors.find(static_cast<unsigned int>(i));
          it != state.boundary_colors.end())
        c = it->second;
      scene.boundary_lut->SetTableValue(i, c.x, c.y, c.z, 1.0);
    }
  scene.boundary_lut->Modified();
}

static void
InitScene(GLFWwindow *glfw_window, MeshState &state, VtkScene &scene)
{
  scene.render_window->AddRenderer(scene.renderer);
  scene.renderer->SetBackground(0.07, 0.08, 0.10);

  scene.render_window->SetReadyForRendering(true);
  scene.render_window->SetMapped(1);
  scene.render_window->SetOwnContext(0);
  scene.render_window->SetOpenGLSymbolLoader(&GlfwGetProcAddress, glfw_window);

  scene.make_current_cb->SetClientData(glfw_window);
  scene.make_current_cb->SetCallback([](vtkObject *, unsigned long, void *client_data, void *) {
    glfwMakeContextCurrent(static_cast<GLFWwindow *>(client_data));
  });
  scene.render_window->AddObserver(vtkCommand::WindowMakeCurrentEvent,
                                   scene.make_current_cb);

  scene.is_current_cb->SetClientData(glfw_window);
  scene.is_current_cb->SetCallback(
    [](vtkObject *, unsigned long, void *client_data, void *call_data) {
      auto *flag = static_cast<bool *>(call_data);
      if (!flag)
        return;
      *flag = glfwGetCurrentContext() == static_cast<GLFWwindow *>(client_data);
    });
  scene.render_window->AddObserver(vtkCommand::WindowIsCurrentEvent,
                                   scene.is_current_cb);

  scene.supports_gl_cb->SetCallback(
    [](vtkObject *, unsigned long, void *, void *call_data) {
      auto *flag = static_cast<int *>(call_data);
      if (flag)
        *flag = 1;
    });
  scene.render_window->AddObserver(vtkCommand::WindowSupportsOpenGLEvent,
                                   scene.supports_gl_cb);

  scene.is_direct_cb->SetCallback(
    [](vtkObject *, unsigned long, void *, void *call_data) {
      auto *flag = static_cast<int *>(call_data);
      if (flag)
        *flag = 1;
    });
  scene.render_window->AddObserver(vtkCommand::WindowIsDirectEvent,
                                   scene.is_direct_cb);

  scene.frame_cb->SetCallback([](vtkObject *, unsigned long, void *, void *) {
    // Swap buffers is handled by the GLFW loop.
  });
  scene.render_window->AddObserver(vtkCommand::WindowFrameEvent, scene.frame_cb);

  scene.interactor->SetRenderWindow(scene.render_window);
  scene.interactor->SetInteractorStyle(scene.style);
  scene.interactor->Initialize();

  BuildVtkMeshesFromTriangulation(state, scene);
  RebuildCounts(state);
  UpdateLutForMaterialIds(state, scene);
  UpdateLutForBoundaryIds(state, scene);

  scene.cell_mapper->SetInputData(scene.grid);
  scene.cell_mapper->SetScalarModeToUseCellFieldData();
  scene.cell_mapper->SelectColorArray("material_id");
  scene.cell_mapper->SetLookupTable(scene.cell_lut);
  scene.cell_mapper->SetUseLookupTableScalarRange(true);

  scene.cell_actor->SetMapper(scene.cell_mapper);
  scene.cell_actor->GetProperty()->SetEdgeVisibility(true);
  scene.cell_actor->GetProperty()->SetEdgeColor(0.9, 0.9, 0.95);
  scene.cell_actor->GetProperty()->SetLineWidth(1.5);
  scene.cell_actor->GetProperty()->SetRenderLinesAsTubes(true);

  scene.boundary_mapper->SetInputData(scene.boundary);
  scene.boundary_mapper->SetScalarModeToUseCellFieldData();
  scene.boundary_mapper->SelectColorArray("boundary_id");
  scene.boundary_mapper->SetLookupTable(scene.boundary_lut);
  scene.boundary_mapper->SetUseLookupTableScalarRange(true);
  scene.boundary_mapper->ScalarVisibilityOn();
  scene.boundary_mapper->SetColorModeToMapScalars();
  scene.boundary_actor->SetMapper(scene.boundary_mapper);
  scene.boundary_actor->GetProperty()->SetLineWidth(4.0);
  scene.boundary_actor->GetProperty()->SetRenderLinesAsTubes(true);

  scene.cell_selected_threshold->SetInputData(scene.grid);
  scene.cell_selected_threshold->SetInputArrayToProcess(
    0,
    0,
    0,
    vtkDataObject::FIELD_ASSOCIATION_CELLS,
    "selected");
  scene.cell_selected_threshold->SetThresholdFunction(
    vtkThreshold::THRESHOLD_BETWEEN);
  scene.cell_selected_threshold->SetLowerThreshold(0.5);
  scene.cell_selected_threshold->SetUpperThreshold(1.5);

  scene.cell_selected_mapper->SetInputConnection(
    scene.cell_selected_threshold->GetOutputPort());
  scene.cell_selected_actor->SetMapper(scene.cell_selected_mapper);
  scene.cell_selected_actor->GetProperty()->SetRepresentationToWireframe();
  scene.cell_selected_actor->GetProperty()->SetColor(0.10, 0.92, 1.00);
  scene.cell_selected_actor->GetProperty()->SetOpacity(1.0);
  scene.cell_selected_actor->GetProperty()->SetLineWidth(7.0);
  scene.cell_selected_actor->GetProperty()->SetRenderLinesAsTubes(true);
  scene.cell_selected_actor->GetProperty()->LightingOff();
  scene.cell_selected_actor->SetPosition(0.0, 0.0, 0.004);
  scene.cell_selected_actor->PickableOff();

  scene.cell_selected_fill_mapper->SetInputConnection(
    scene.cell_selected_threshold->GetOutputPort());
  scene.cell_selected_fill_actor->SetMapper(scene.cell_selected_fill_mapper);
  scene.cell_selected_fill_actor->GetProperty()->SetRepresentationToSurface();
  scene.cell_selected_fill_actor->GetProperty()->SetColor(0.10, 0.92, 1.00);
  scene.cell_selected_fill_actor->GetProperty()->SetOpacity(0.22);
  scene.cell_selected_fill_actor->GetProperty()->LightingOff();
  scene.cell_selected_fill_actor->SetPosition(0.0, 0.0, 0.002);
  scene.cell_selected_fill_actor->PickableOff();

  scene.cell_hover_threshold->SetInputData(scene.grid);
  scene.cell_hover_threshold->SetInputArrayToProcess(
    0,
    0,
    0,
    vtkDataObject::FIELD_ASSOCIATION_CELLS,
    "hovered");
  scene.cell_hover_threshold->SetThresholdFunction(
    vtkThreshold::THRESHOLD_BETWEEN);
  scene.cell_hover_threshold->SetLowerThreshold(0.5);
  scene.cell_hover_threshold->SetUpperThreshold(1.5);
  scene.cell_hover_mapper->SetInputConnection(
    scene.cell_hover_threshold->GetOutputPort());
  scene.cell_hover_actor->SetMapper(scene.cell_hover_mapper);
  scene.cell_hover_actor->GetProperty()->SetRepresentationToWireframe();
  scene.cell_hover_actor->GetProperty()->SetColor(0.95, 0.85, 0.20);
  scene.cell_hover_actor->GetProperty()->SetOpacity(1.0);
  scene.cell_hover_actor->GetProperty()->SetLineWidth(4.0);
  scene.cell_hover_actor->GetProperty()->SetRenderLinesAsTubes(true);
  scene.cell_hover_actor->GetProperty()->LightingOff();
  scene.cell_hover_actor->SetPosition(0.0, 0.0, 0.006);
  scene.cell_hover_actor->PickableOff();

  scene.boundary_selected_threshold->SetInputData(scene.boundary);
  scene.boundary_selected_threshold->SetInputArrayToProcess(
    0,
    0,
    0,
    vtkDataObject::FIELD_ASSOCIATION_CELLS,
    "selected");
  scene.boundary_selected_threshold->SetThresholdFunction(
    vtkThreshold::THRESHOLD_BETWEEN);
  scene.boundary_selected_threshold->SetLowerThreshold(0.5);
  scene.boundary_selected_threshold->SetUpperThreshold(1.5);

  scene.boundary_selected_geom->SetInputConnection(
    scene.boundary_selected_threshold->GetOutputPort());
  scene.boundary_selected_mapper->SetInputConnection(
    scene.boundary_selected_geom->GetOutputPort());
  scene.boundary_selected_mapper->SetResolveCoincidentTopologyToPolygonOffset();
  scene.boundary_selected_mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(
    1.0, 1.0);
  scene.boundary_selected_actor->SetMapper(scene.boundary_selected_mapper);
  scene.boundary_selected_actor->GetProperty()->SetColor(0.98, 0.30, 0.95);
  scene.boundary_selected_actor->GetProperty()->SetLineWidth(12.0);
  scene.boundary_selected_actor->GetProperty()->SetRenderLinesAsTubes(true);
  scene.boundary_selected_actor->GetProperty()->LightingOff();
  scene.boundary_selected_actor->SetPosition(0.0, 0.0, 0.008);
  scene.boundary_selected_actor->PickableOff();

  scene.boundary_hover_threshold->SetInputData(scene.boundary);
  scene.boundary_hover_threshold->SetInputArrayToProcess(
    0,
    0,
    0,
    vtkDataObject::FIELD_ASSOCIATION_CELLS,
    "hovered");
  scene.boundary_hover_threshold->SetThresholdFunction(
    vtkThreshold::THRESHOLD_BETWEEN);
  scene.boundary_hover_threshold->SetLowerThreshold(0.5);
  scene.boundary_hover_threshold->SetUpperThreshold(1.5);
  scene.boundary_hover_geom->SetInputConnection(
    scene.boundary_hover_threshold->GetOutputPort());
  scene.boundary_hover_mapper->SetInputConnection(
    scene.boundary_hover_geom->GetOutputPort());
  scene.boundary_hover_mapper->SetResolveCoincidentTopologyToPolygonOffset();
  scene.boundary_hover_mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(
    1.0, 1.0);
  scene.boundary_hover_actor->SetMapper(scene.boundary_hover_mapper);
  scene.boundary_hover_actor->GetProperty()->SetColor(0.95, 0.85, 0.20);
  scene.boundary_hover_actor->GetProperty()->SetLineWidth(10.0);
  scene.boundary_hover_actor->GetProperty()->SetRenderLinesAsTubes(true);
  scene.boundary_hover_actor->GetProperty()->LightingOff();
  scene.boundary_hover_actor->SetPosition(0.0, 0.0, 0.010);
  scene.boundary_hover_actor->PickableOff();

  scene.renderer->AddActor(scene.cell_actor);
  scene.renderer->AddActor(scene.cell_selected_fill_actor);
  scene.renderer->AddActor(scene.cell_selected_actor);
  scene.renderer->AddActor(scene.cell_hover_actor);
  scene.renderer->AddActor(scene.boundary_actor);
  scene.renderer->AddActor(scene.boundary_selected_actor);
  scene.renderer->AddActor(scene.boundary_hover_actor);

  scene.picker->SetTolerance(1e-2);
  scene.picker->PickFromListOn();

  scene.renderer->ResetCamera();
  scene.render_window->Render();

  UpdateVtkSelectionArrays(state, scene);
  UpdateVtkHoverArrays(state, scene);
}

static void
RebuildScene(GLFWwindow *glfw_window,
             MeshState &state,
             VtkScene  &scene,
             const bool reset_camera)
{
  (void)glfw_window;
  BuildVtkMeshesFromTriangulation(state, scene);
  RebuildCounts(state);
  UpdateLutForMaterialIds(state, scene);
  UpdateLutForBoundaryIds(state, scene);
  UpdateVtkSelectionArrays(state, scene);
  UpdateVtkHoverArrays(state, scene);
  if (reset_camera)
    scene.renderer->ResetCamera();
  scene.renderer->ResetCameraClippingRange();
  scene.render_window->Render();
}

static bool
PickCellOrBoundary(const MeshState &state,
                   VtkScene       &scene,
                   const int       x,
                   const int       y,
                   unsigned int   &out_cell_index,
                   unsigned int   &out_face_no,
                   unsigned int   &out_boundary_id,
                   unsigned int   &out_material_id)
{
  scene.picker->InitializePickList();
  if (state.mode == MeshState::Mode::cell)
    scene.picker->AddPickList(scene.cell_actor);
  else
    scene.picker->AddPickList(scene.boundary_actor);

  if (!scene.picker->Pick(x, y, 0.0, scene.renderer))
    return false;

  vtkDataSet *ds = vtkDataSet::SafeDownCast(scene.picker->GetDataSet());
  if (!ds)
    return false;

  const vtkIdType cid = scene.picker->GetCellId();
  if (cid < 0)
    return false;

  if (state.mode == MeshState::Mode::cell)
    {
      vtkIntArray *active_cell_index = GetCellArray(ds, "active_cell_index");
      vtkIntArray *material_id       = GetCellArray(ds, "material_id");
      if (!active_cell_index || !material_id)
        return false;
      out_cell_index =
        static_cast<unsigned int>(active_cell_index->GetValue(cid));
      out_material_id =
        static_cast<unsigned int>(material_id->GetValue(cid));
      out_face_no     = 0;
      out_boundary_id = 0;
      return true;
    }
  else
    {
      vtkIntArray *cell_index = GetCellArray(ds, "cell_index");
      vtkIntArray *face_no    = GetCellArray(ds, "face_no");
      vtkIntArray *boundary_id = GetCellArray(ds, "boundary_id");
      if (!cell_index || !face_no || !boundary_id)
        return false;
      out_cell_index =
        static_cast<unsigned int>(cell_index->GetValue(cid));
      out_face_no =
        static_cast<unsigned int>(face_no->GetValue(cid));
      out_boundary_id =
        static_cast<unsigned int>(boundary_id->GetValue(cid));
      out_material_id = 0;
      return true;
    }
}

static void
ApplySelectionFromPick(MeshState &state,
                       const unsigned int cell_index,
                       const unsigned int face_no,
                       const unsigned int boundary_id,
                       const unsigned int material_id,
                       const bool         add_mode,
                       const bool         subtract_mode)
{
  const bool replace_mode = !add_mode && !subtract_mode;

  if (state.mode == MeshState::Mode::cell)
    {
      state.selected_material_id = static_cast<int>(material_id);
      if (replace_mode)
        state.selected_cells.clear();

      if (subtract_mode)
        state.selected_cells.erase(cell_index);
      else if (add_mode && state.selected_cells.count(cell_index) > 0)
        state.selected_cells.erase(cell_index);
      else
        state.selected_cells.insert(cell_index);
    }
  else
    {
      state.selected_boundary_id = static_cast<int>(boundary_id);
      const auto key            = std::make_pair(cell_index, face_no);
      if (replace_mode)
        state.selected_boundary_faces.clear();

      if (subtract_mode)
        state.selected_boundary_faces.erase(key);
      else if (add_mode && state.selected_boundary_faces.count(key) > 0)
        state.selected_boundary_faces.erase(key);
      else
        state.selected_boundary_faces.insert(key);
    }
}

static std::string
ToString(const std::exception &e)
{
  return e.what();
}

static bool
LoadTriangulationFromFile(MeshState &state,
                          const std::filesystem::path &path,
                          std::string &error)
{
  try
    {
      state.tria.clear();
      dealii::GridIn<2, 2> grid_in;
      grid_in.attach_triangulation(state.tria);

      std::ifstream in(path);
      if (!in)
        {
          error = "Could not open file: " + path.string();
          return false;
        }

      const auto ext = path.extension().string();
      if (ext == ".vtk")
        grid_in.read_vtk(in);
      else if (ext == ".vtu")
        grid_in.read_vtu(in);
      else
        {
          error = "Unsupported extension: " + ext + " (supported: .vtk, .vtu)";
          return false;
        }

      state.current_label = path.filename().string();
      RebuildCellIndexMap(state);
      RebuildCounts(state);
      state.selected_boundary_faces.clear();
      state.selected_cells.clear();
      state.selected_boundary_id = -1;
      state.selected_material_id = -1;
      return true;
    }
  catch (const std::exception &e)
    {
      error = ToString(e);
      return false;
    }
}

static bool
GenerateTriangulation(MeshState &state,
                      const std::string &name,
                      const std::string &args,
                      const int          refinements,
                      std::string       &error)
{
  try
    {
      state.tria.clear();
      dealii::GridGenerator::generate_from_name_and_arguments(state.tria,
                                                              name,
                                                              args);
      if (refinements > 0)
        state.tria.refine_global(static_cast<unsigned int>(refinements));

      state.current_label = name;
      RebuildCellIndexMap(state);
      RebuildCounts(state);
      state.selected_boundary_faces.clear();
      state.selected_cells.clear();
      state.selected_boundary_id = -1;
      state.selected_material_id = -1;
      return true;
    }
  catch (const std::exception &e)
    {
      error = ToString(e);
      return false;
    }
}

static bool
SaveTriangulationToFile(const MeshState &state,
                        const std::filesystem::path &path,
                        const int                   format,
                        std::string                &error)
{
  try
    {
      std::ofstream out(path);
      if (!out)
        {
          error = "Could not open file for writing: " + path.string();
          return false;
        }

      dealii::GridOut grid_out;
      if (format == 0)
        grid_out.write_vtk(state.tria, out);
      else
        grid_out.write_vtu(state.tria, out);
      return true;
    }
  catch (const std::exception &e)
    {
      error = ToString(e);
      return false;
    }
}

static void
DrawSelectionPanel(MeshState &state, VtkScene &scene)
{
  TallButtonStyle tall_buttons;
  constexpr float id_input_width = 90.0f; // ~4 digits

  ImGui::TextUnformatted("Boundary IDs");
  ImGui::Separator();
  bool boundary_colors_changed = false;
  for (const auto &[bid, count] : state.boundary_counts)
    {
      const bool selected = state.selected_boundary_id == static_cast<int>(bid);
      ImGui::PushID(static_cast<int>(bid));
      ImVec4 &col = state.boundary_colors[bid];
      if (ImGui::ColorEdit3("##c", &col.x, ImGuiColorEditFlags_NoInputs))
        boundary_colors_changed = true;
      ImGui::SameLine();
      char label[128];
      std::snprintf(label, sizeof(label), "%u (%u)", bid, count);
      if (ImGui::Selectable(label, selected))
        {
          state.selected_boundary_id = static_cast<int>(bid);
          state.mode                = MeshState::Mode::boundary;
        }
      ImGui::PopID();
    }
  if (boundary_colors_changed)
    {
      UpdateLutForBoundaryIds(state, scene);
      scene.render_window->Render();
    }

  ImGui::SetNextItemWidth(id_input_width);
  ImGui::InputScalar("Assign boundary id",
                     ImGuiDataType_U32,
                     &state.assign_boundary_id);
  {
    ImGui::PushID("boundary_actions");
    const auto lay = GetButtonRowLayout(3);
    if (ImGui::Button("Select all", ImVec2(lay.w, lay.h)) &&
        state.selected_boundary_id >= 0)
      {
        state.mode = MeshState::Mode::boundary;
        state.selected_boundary_faces.clear();
        for (auto cell = state.tria.begin_active(); cell != state.tria.end();
             ++cell)
          {
            const unsigned int cell_index =
              static_cast<unsigned int>(cell->active_cell_index());
            for (unsigned int f = 0; f < cell->n_faces(); ++f)
              {
                const auto face = cell->face(f);
                if (!face->at_boundary())
                  continue;
                if (static_cast<int>(face->boundary_id()) !=
                    state.selected_boundary_id)
                  continue;
                state.selected_boundary_faces.insert({cell_index, f});
              }
          }
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(lay.w, lay.h)))
      {
        state.selected_boundary_faces.clear();
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(lay.w, lay.h)))
      {
        for (const auto &pair : state.selected_boundary_faces)
          {
            const unsigned int cell_index = pair.first;
            const unsigned int face_no    = pair.second;
            auto it = state.cell_by_index.find(cell_index);
            if (it == state.cell_by_index.end())
              continue;
            auto face = it->second->face(face_no);
            if (face->at_boundary())
              face->set_boundary_id(state.assign_boundary_id);
          }
        RebuildCounts(state);
        RebuildScene(nullptr, state, scene, false);
      }
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Material IDs");
  ImGui::Separator();
  bool material_colors_changed = false;
  for (const auto &[mid, count] : state.material_counts)
    {
      const bool selected = state.selected_material_id == static_cast<int>(mid);
      ImGui::PushID(static_cast<int>(mid) + 100000);
      ImVec4 &col = state.material_colors[mid];
      if (ImGui::ColorEdit3("##c", &col.x, ImGuiColorEditFlags_NoInputs))
        material_colors_changed = true;
      ImGui::SameLine();
      char label[128];
      std::snprintf(label, sizeof(label), "%u (%u)", mid, count);
      if (ImGui::Selectable(label, selected))
        {
          state.selected_material_id = static_cast<int>(mid);
          state.mode                = MeshState::Mode::cell;
        }
      ImGui::PopID();
    }
  if (material_colors_changed)
    {
      UpdateLutForMaterialIds(state, scene);
      scene.render_window->Render();
    }

  ImGui::SetNextItemWidth(id_input_width);
  ImGui::InputScalar("Assign material id",
                     ImGuiDataType_U32,
                     &state.assign_material_id);
  {
    ImGui::PushID("material_actions");
    const auto lay = GetButtonRowLayout(3);
    if (ImGui::Button("Select all", ImVec2(lay.w, lay.h)) &&
        state.selected_material_id >= 0)
      {
        state.mode = MeshState::Mode::cell;
        state.selected_cells.clear();
        for (auto cell = state.tria.begin_active(); cell != state.tria.end();
             ++cell)
          {
            if (static_cast<int>(cell->material_id()) != state.selected_material_id)
              continue;
            state.selected_cells.insert(
              static_cast<unsigned int>(cell->active_cell_index()));
          }
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(lay.w, lay.h)))
      {
        state.selected_cells.clear();
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(lay.w, lay.h)))
      {
        for (const auto cell_index : state.selected_cells)
          {
            auto it = state.cell_by_index.find(cell_index);
            if (it == state.cell_by_index.end())
              continue;
            it->second->set_material_id(state.assign_material_id);
          }
        RebuildCounts(state);
        RebuildScene(nullptr, state, scene, false);
      }
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Face manifold ids");
  ImGui::Separator();
  for (const auto &[mid, count] : state.face_manifold_counts)
    {
      const int shown_mid = IsFlatManifoldId(mid) ? -1 : static_cast<int>(mid);
      const bool selected = state.selected_face_manifold_id == shown_mid;
      char label[128];
      if (shown_mid < 0)
        std::snprintf(label, sizeof(label), "-1 (%u)", count);
      else
        std::snprintf(label, sizeof(label), "%u (%u)", mid, count);
      if (ImGui::Selectable(label, selected))
        {
          state.selected_face_manifold_id = shown_mid;
          state.mode                     = MeshState::Mode::boundary;
        }
    }
  ImGui::SetNextItemWidth(id_input_width);
  ImGui::InputScalar("Assign face manifold id",
                     ImGuiDataType_U32,
                     &state.assign_face_manifold_id);
  {
    ImGui::PushID("face_manifold_actions");
    const auto lay = GetButtonRowLayout(3);
    if (ImGui::Button("Select all", ImVec2(lay.w, lay.h)) &&
        state.selected_face_manifold_id >= 0)
      {
        state.mode = MeshState::Mode::boundary;
        state.selected_boundary_faces.clear();
        for (auto cell = state.tria.begin_active(); cell != state.tria.end(); ++cell)
          {
            const unsigned int cell_index =
              static_cast<unsigned int>(cell->active_cell_index());
            for (unsigned int f = 0; f < cell->n_faces(); ++f)
              {
                const auto face = cell->face(f);
                if (!face->at_boundary())
                  continue;
                if (!ManifoldIdMatches(face->manifold_id(),
                                       state.selected_face_manifold_id))
                  continue;
                state.selected_boundary_faces.insert({cell_index, f});
              }
          }
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(lay.w, lay.h)))
      {
        state.selected_boundary_faces.clear();
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(lay.w, lay.h)))
      {
        for (const auto &pair : state.selected_boundary_faces)
          {
            const unsigned int cell_index = pair.first;
            const unsigned int face_no    = pair.second;
            auto it = state.cell_by_index.find(cell_index);
            if (it == state.cell_by_index.end())
              continue;
            auto face = it->second->face(face_no);
            if (face->at_boundary())
              face->set_manifold_id(
                static_cast<dealii::types::manifold_id>(state.assign_face_manifold_id));
          }
        RebuildCounts(state);
        RebuildScene(nullptr, state, scene, false);
      }
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Cell manifold ids");
  ImGui::Separator();
  for (const auto &[mid, count] : state.cell_manifold_counts)
    {
      const int shown_mid = IsFlatManifoldId(mid) ? -1 : static_cast<int>(mid);
      const bool selected = state.selected_cell_manifold_id == shown_mid;
      char label[128];
      if (shown_mid < 0)
        std::snprintf(label, sizeof(label), "-1 (%u)", count);
      else
        std::snprintf(label, sizeof(label), "%u (%u)", mid, count);
      if (ImGui::Selectable(label, selected))
        {
          state.selected_cell_manifold_id = shown_mid;
          state.mode                     = MeshState::Mode::cell;
        }
    }
  ImGui::SetNextItemWidth(id_input_width);
  ImGui::InputScalar("Assign cell manifold id",
                     ImGuiDataType_U32,
                     &state.assign_cell_manifold_id);
  {
    ImGui::PushID("cell_manifold_actions");
    const auto lay = GetButtonRowLayout(3);
    if (ImGui::Button("Select all", ImVec2(lay.w, lay.h)) &&
        state.selected_cell_manifold_id >= 0)
      {
        state.mode = MeshState::Mode::cell;
        state.selected_cells.clear();
        for (auto cell = state.tria.begin_active(); cell != state.tria.end(); ++cell)
          {
            if (!ManifoldIdMatches(cell->manifold_id(),
                                   state.selected_cell_manifold_id))
              continue;
            state.selected_cells.insert(
              static_cast<unsigned int>(cell->active_cell_index()));
          }
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(lay.w, lay.h)))
      {
        state.selected_cells.clear();
        UpdateVtkSelectionArrays(state, scene);
      }
    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(lay.w, lay.h)))
      {
        for (const auto cell_index : state.selected_cells)
          {
            auto it = state.cell_by_index.find(cell_index);
            if (it == state.cell_by_index.end())
              continue;
            it->second->set_manifold_id(
              static_cast<dealii::types::manifold_id>(state.assign_cell_manifold_id));
          }
        RebuildCounts(state);
        RebuildScene(nullptr, state, scene, false);
      }
    ImGui::PopID();
  }
}

} // namespace

struct ManipulatorApp::Impl
{
  MeshState    mesh;
  VtkScene     vtk;
  FilePicker   open_picker;
  NewGridDialog new_grid;
  SaveDialog   save_dialog;
  GLFWwindow  *glfw_window = nullptr;

  double last_cursor_x       = 0.0;
  double last_cursor_y       = 0.0;
  bool   camera_button_down  = false; // RMB mapped to VTK LMB

  bool   lmb_pan_down        = false; // LMB mapped to VTK MMB when off-grid
  bool   box_select_active   = false;
  bool   mouse_over_grid_any = false;
  int    last_mods           = 0;

  ImVec2 box_start_screen = ImVec2(0.0f, 0.0f);
  ImVec2 box_end_screen   = ImVec2(0.0f, 0.0f);
  int    box_start_x = 0; // VTK coords (origin bottom-left)
  int    box_start_y = 0;
  int    box_end_x   = 0;
  int    box_end_y   = 0;

  float pick_tolerance = 1e-2f;
  float cell_edge_width = 1.5f;

  int fb_width  = 1;
  int fb_height = 1;

  std::string status;

  bool initialized = false;
  float left_panel_width = 380.0f;

  bool create_default_grid()
  {
    std::string error;
    if (!GenerateTriangulation(mesh, "hyper_cube", "0 : 1 : true", 3, error))
      {
        status = "Default grid failed: " + error;
        return false;
      }
    status.clear();
    return true;
  }

  void ensure_glad_ready()
  {
    // VTK's vtkOpenGLState::Reset() can run before vtkOpenGLRenderWindow::OpenGLInit()
    // so ensure VTK's bundled GLAD is initialized for the current context.
    if (vtk_gladLoadGLUserPtr(&GlfwGladGetProcAddress, glfw_window) == 0)
      {
        status = "VTK GLAD init failed (gl function pointers missing)";
      }
  }

  std::pair<int, int>
  to_vtk_xy(GLFWwindow *win, const double xpos, const double ypos) const
  {
    int win_w = 1;
    int win_h = 1;
    glfwGetWindowSize(win, &win_w, &win_h);
    win_w = std::max(1, win_w);
    win_h = std::max(1, win_h);

    const double sx =
      static_cast<double>(fb_width) / static_cast<double>(win_w);
    const double sy =
      static_cast<double>(fb_height) / static_cast<double>(win_h);

    const int px = static_cast<int>(xpos * sx);
    const int py = static_cast<int>(ypos * sy);
    return {px, py};
  }
};

ManipulatorApp::ManipulatorApp() : impl(std::make_unique<Impl>()) {}
ManipulatorApp::~ManipulatorApp() = default;

void
ManipulatorApp::initialize(GLFWwindow *glfw_window)
{
  impl->glfw_window = glfw_window;
  glfwMakeContextCurrent(glfw_window);
  impl->ensure_glad_ready();
  if (!impl->create_default_grid())
    return;
  InitScene(glfw_window, impl->mesh, impl->vtk);
  impl->vtk.cell_actor->GetProperty()->SetLineWidth(impl->cell_edge_width);
  impl->vtk.picker->SetTolerance(impl->pick_tolerance);
  impl->initialized = true;
}

void
ManipulatorApp::set_framebuffer_size(int width, int height)
{
  impl->fb_width  = std::max(1, width);
  impl->fb_height = std::max(1, height);
  impl->vtk.viewport_w = impl->fb_width;
  impl->vtk.viewport_h = impl->fb_height;
  impl->vtk.render_window->SetSize(impl->fb_width, impl->fb_height);
}

void
ManipulatorApp::on_glfw_cursor_pos(GLFWwindow *glfw_window,
                                   double      xpos,
                                   double      ypos)
{
  if (!impl->initialized)
    return;

  impl->last_cursor_x = xpos;
  impl->last_cursor_y = ypos;

  const ImGuiIO &imgui_io = ImGui::GetIO();
  if (imgui_io.WantCaptureMouse)
    return;

  const auto [px, py] = impl->to_vtk_xy(glfw_window, xpos, ypos);
  const bool ctrl     = false;
  const bool shift    = false;
  impl->vtk.interactor->SetEventInformationFlipY(px, py, ctrl, shift);
  impl->vtk.interactor->InvokeEvent(vtkCommand::MouseMoveEvent, nullptr);

  const int vtk_x = px;
  const int vtk_y = impl->fb_height - py; // bottom-left
  impl->mouse_over_grid_any = HitOnAnyGeometry(impl->vtk, vtk_x, vtk_y);

  if (impl->box_select_active)
    {
      impl->box_end_screen = ImGui::GetIO().MousePos;
      impl->box_end_x      = vtk_x;
      impl->box_end_y      = vtk_y;
      return;
    }

  if (impl->camera_button_down || impl->lmb_pan_down || impl->box_select_active)
    return;

  unsigned int cell_index  = 0;
  unsigned int face_no     = 0;
  unsigned int boundary_id = 0;
  unsigned int material_id = 0;
  const bool hit = PickCellOrBoundary(impl->mesh,
                                      impl->vtk,
                                      vtk_x,
                                      vtk_y,
                                      cell_index,
                                      face_no,
                                      boundary_id,
                                      material_id);

  bool changed = false;
  if (hit)
    {
      if (impl->mesh.mode == MeshState::Mode::cell)
        {
          if (!impl->mesh.hovered_cell || *impl->mesh.hovered_cell != cell_index)
            changed = true;
          impl->mesh.hovered_cell = cell_index;
          if (impl->mesh.hovered_boundary_face)
            changed = true;
          impl->mesh.hovered_boundary_face.reset();
        }
      else
        {
          const std::pair<unsigned int, unsigned int> key{cell_index, face_no};
          if (!impl->mesh.hovered_boundary_face ||
              *impl->mesh.hovered_boundary_face != key)
            changed = true;
          impl->mesh.hovered_boundary_face = key;
          if (impl->mesh.hovered_cell)
            changed = true;
          impl->mesh.hovered_cell.reset();
        }
    }
  else
    {
      if (impl->mesh.hovered_cell || impl->mesh.hovered_boundary_face)
        changed = true;
      impl->mesh.hovered_cell.reset();
      impl->mesh.hovered_boundary_face.reset();
    }

  if (changed)
    UpdateVtkHoverArrays(impl->mesh, impl->vtk);
}

void
ManipulatorApp::on_glfw_mouse_button(GLFWwindow *glfw_window,
                                     int        button,
                                     int        action,
                                     int        mods)
{
  if (!impl->initialized)
    return;

  const ImGuiIO &imgui_io = ImGui::GetIO();
  if (imgui_io.WantCaptureMouse)
    return;

  impl->last_mods = mods;

  const auto [px_top, py_top] =
    impl->to_vtk_xy(glfw_window, impl->last_cursor_x, impl->last_cursor_y);
  const int vtk_x = px_top;
  const int vtk_y = impl->fb_height - py_top; // bottom-left

  // RMB -> VTK LMB for camera rotate.
  if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
      const bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
      const bool shift = (mods & GLFW_MOD_SHIFT) != 0;
      impl->vtk.interactor->SetEventInformationFlipY(vtk_x,
                                                     py_top,
                                                     ctrl,
                                                     shift);

      if (action == GLFW_PRESS)
        {
          impl->camera_button_down = true;
          impl->vtk.interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent,
                                            nullptr);
        }
      else if (action == GLFW_RELEASE)
        {
          impl->camera_button_down = false;
          impl->vtk.interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent,
                                            nullptr);
        }
      return;
    }

  // LMB: drag is either box-select (over grid) or pan camera (off grid).
  if (button != GLFW_MOUSE_BUTTON_LEFT)
    return;

  if (action == GLFW_PRESS)
    {
      impl->box_start_screen = ImGui::GetIO().MousePos;
      impl->box_end_screen   = impl->box_start_screen;
      impl->box_start_x      = vtk_x;
      impl->box_start_y      = vtk_y;
      impl->box_end_x        = vtk_x;
      impl->box_end_y        = vtk_y;

      // Decide mode at press time.
      const bool over_grid = HitOnAnyGeometry(impl->vtk, vtk_x, vtk_y);
      impl->mouse_over_grid_any = over_grid;
      if (over_grid)
        {
          impl->box_select_active = true;
          impl->lmb_pan_down      = false;
        }
      else
        {
          impl->box_select_active = false;
          impl->lmb_pan_down      = true;

          const bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
          const bool shift = (mods & GLFW_MOD_SHIFT) != 0;
          impl->vtk.interactor->SetEventInformationFlipY(vtk_x,
                                                         py_top,
                                                         ctrl,
                                                         shift);
          impl->vtk.interactor->InvokeEvent(vtkCommand::MiddleButtonPressEvent,
                                            nullptr);
        }
      return;
    }

  if (action == GLFW_RELEASE)
    {
      if (impl->lmb_pan_down)
        {
          impl->lmb_pan_down = false;
          const bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
          const bool shift = (mods & GLFW_MOD_SHIFT) != 0;
          impl->vtk.interactor->SetEventInformationFlipY(vtk_x,
                                                         py_top,
                                                         ctrl,
                                                         shift);
          impl->vtk.interactor->InvokeEvent(
            vtkCommand::MiddleButtonReleaseEvent, nullptr);
          return;
        }

      if (impl->box_select_active)
        {
          impl->box_end_screen = ImGui::GetIO().MousePos;
          impl->box_end_x      = vtk_x;
          impl->box_end_y      = vtk_y;

          const float dx =
            std::abs(impl->box_end_screen.x - impl->box_start_screen.x);
          const float dy =
            std::abs(impl->box_end_screen.y - impl->box_start_screen.y);

          const bool add_mode      = (mods & GLFW_MOD_SHIFT) != 0;
          const bool subtract_mode = (mods & GLFW_MOD_ALT) != 0;

          if (dx < 3.0f && dy < 3.0f)
            {
              unsigned int cell_index  = 0;
              unsigned int face_no     = 0;
              unsigned int boundary_id = 0;
              unsigned int material_id = 0;
              if (PickCellOrBoundary(impl->mesh,
                                     impl->vtk,
                                     vtk_x,
                                     vtk_y,
                                     cell_index,
                                     face_no,
                                     boundary_id,
                                     material_id))
                {
                  ApplySelectionFromPick(impl->mesh,
                                         cell_index,
                                         face_no,
                                         boundary_id,
                                         material_id,
                                         add_mode,
                                         subtract_mode);
                  UpdateVtkSelectionArrays(impl->mesh, impl->vtk);
                }
            }
          else
            {
              ApplyBoxSelection(impl->mesh,
                                impl->vtk,
                                impl->box_start_x,
                                impl->box_start_y,
                                impl->box_end_x,
                                impl->box_end_y,
                                add_mode,
                                subtract_mode);
            }

          impl->box_select_active = false;
          return;
        }
    }

  // Other buttons are ignored.
}

void
ManipulatorApp::on_glfw_scroll(GLFWwindow *glfw_window,
                               double      xoffset,
                               double      yoffset)
{
  (void)xoffset;
  if (!impl->initialized)
    return;

  const ImGuiIO &imgui_io = ImGui::GetIO();
  if (imgui_io.WantCaptureMouse)
    return;

  const auto [px, py] =
    impl->to_vtk_xy(glfw_window, impl->last_cursor_x, impl->last_cursor_y);
  const bool ctrl  = false;
  const bool shift = false;
  impl->vtk.interactor->SetEventInformationFlipY(px, py, ctrl, shift);

  const int steps = RoundScrollSteps(yoffset);
  if (steps > 0)
    for (int i = 0; i < steps; ++i)
      impl->vtk.interactor->InvokeEvent(vtkCommand::MouseWheelForwardEvent,
                                        nullptr);
  else if (steps < 0)
    for (int i = 0; i < -steps; ++i)
      impl->vtk.interactor->InvokeEvent(vtkCommand::MouseWheelBackwardEvent,
                                        nullptr);
}

void
ManipulatorApp::process_input(GLFWwindow *glfw_window)
{
  if (!impl->initialized)
    return;

  (void)glfw_window;
  // Selection/camera interactions are handled via GLFW callbacks.
}

void
ManipulatorApp::render_frame()
{
  if (!impl->initialized)
    return;
  impl->vtk.render_window->Render();
}

void
ManipulatorApp::draw_ui()
{
  if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File"))
        {
          if (ImGui::MenuItem("Open..."))
            {
              const auto start_dir = std::filesystem::current_path().string();
              impl->open_picker.open_modal("Open VTK", start_dir, ".vtk");
            }
          if (ImGui::MenuItem("New..."))
            impl->new_grid.open_modal();
          if (ImGui::MenuItem("Save..."))
            {
              const auto start_dir = std::filesystem::current_path().string();
              impl->save_dialog.open_modal(start_dir);
            }
          if (ImGui::MenuItem("Quit"))
            glfwSetWindowShouldClose(impl->glfw_window, GLFW_TRUE);
          ImGui::EndMenu();
        }

      if (!impl->mesh.current_label.empty())
        {
          ImGui::Separator();
          ImGui::TextUnformatted(impl->mesh.current_label.c_str());
        }

      ImGui::EndMainMenuBar();
    }

  // Layout: left tools panel (ImGui window) + splitter (ImGui window) +
  // instructions overlay (ImGui window without inputs/background). The mesh is
  // rendered directly by VTK to the whole framebuffer behind these windows.
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  const ImVec2         root_pos  = vp ? vp->WorkPos : ImVec2(0, 0);
  const ImVec2         root_size = vp ? vp->WorkSize : ImGui::GetIO().DisplaySize;
  const ImVec2         vp_pos    = vp ? vp->Pos : ImVec2(0, 0);
  const ImVec2         vp_size   = vp ? vp->Size : ImVec2(impl->fb_width, impl->fb_height);

  const float splitter_width = 6.0f;
  const float min_left       = 260.0f;
  const float min_right      = 240.0f;
  const float full_width     = root_size.x;
  const float max_left =
    std::max(min_left, full_width - splitter_width - min_right);
  impl->left_panel_width =
    std::max(min_left, std::min(max_left, impl->left_panel_width));

  // Constrain the VTK renderer viewport to the right panel (and exclude the
  // menu bar/work-area offset) so ResetCamera centers in the visible area.
  if (vp && impl->fb_width > 1 && impl->fb_height > 1)
    {
      const double sx =
        static_cast<double>(impl->fb_width) / std::max(1.0f, vp_size.x);
      const double sy =
        static_cast<double>(impl->fb_height) / std::max(1.0f, vp_size.y);

      const double work_off_x = static_cast<double>(root_pos.x - vp_pos.x);
      const double work_off_y = static_cast<double>(root_pos.y - vp_pos.y);

      const double x0_fb = (work_off_x + impl->left_panel_width + splitter_width) * sx;
      const double ymax_fb = static_cast<double>(impl->fb_height) - (work_off_y * sy);

      const double xmin = x0_fb / static_cast<double>(impl->fb_width);
      const double xmax = 1.0;
      const double ymin = 0.0;
      const double ymax = ymax_fb / static_cast<double>(impl->fb_height);
      UpdateRendererViewport(impl->vtk, xmin, ymin, xmax, ymax);
    }

  ImGui::SetNextWindowPos(root_pos);
  ImGui::SetNextWindowSize(ImVec2(impl->left_panel_width, root_size.y));
  const ImGuiWindowFlags left_flags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
  ImGui::Begin("##coral_manipulator_left", nullptr, left_flags);

  if (ImGui::Button("Save..."))
    {
      const auto start_dir = std::filesystem::current_path().string();
      impl->save_dialog.open_modal(start_dir);
    }

  ImGui::SameLine();
  ImGui::TextUnformatted("Pick tolerance");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(140.0f);
  if (ImGui::DragFloat("##pick_tol",
                       &impl->pick_tolerance,
                       0.0005f,
                       0.0001f,
                       0.05f,
                       "%.4f"))
    {
      impl->pick_tolerance =
        std::max(0.0001f, std::min(0.05f, impl->pick_tolerance));
      impl->vtk.picker->SetTolerance(impl->pick_tolerance);
    }

  ImGui::SameLine();
  ImGui::TextUnformatted("Cell edge width");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  if (ImGui::DragFloat("##cell_edge_width",
                       &impl->cell_edge_width,
                       0.1f,
                       0.5f,
                       12.0f,
                       "%.1f"))
    {
      impl->cell_edge_width = std::max(0.5f, std::min(12.0f, impl->cell_edge_width));
      impl->vtk.cell_actor->GetProperty()->SetLineWidth(impl->cell_edge_width);
      impl->vtk.render_window->Render();
    }

  ImGui::TextUnformatted("Selection");
  ImGui::Separator();

  if (auto selected = impl->open_picker.draw())
    {
      std::string error;
      if (!LoadTriangulationFromFile(impl->mesh, *selected, error))
        {
          impl->status = "Open failed: " + error;
        }
      else
        {
          impl->status.clear();
          RebuildScene(impl->glfw_window, impl->mesh, impl->vtk, true);
        }
    }

  if (auto save = impl->save_dialog.draw())
    {
      std::string error;
      if (!SaveTriangulationToFile(impl->mesh, save->path, save->format, error))
        {
          impl->status = "Save failed: " + error;
        }
      else
        {
          impl->status = "Saved: " + save->path.string();
          impl->mesh.current_label = save->path.filename().string();
        }
    }

  if (impl->new_grid.open)
    {
      const ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
      if (impl->new_grid.pending_open_popup)
	        {
	          ImGui::OpenPopup("New Grid");
	          impl->new_grid.pending_open_popup = false;
	        }
	      if (ImGui::BeginPopupModal("New Grid", &impl->new_grid.open, flags))
	        {
	          ImGui::TextUnformatted("GridGenerator::generate_from_name_and_arguments");
	          ImGui::Separator();

          ImGui::InputText("Name", &impl->new_grid.name);
          ImGui::InputText("Arguments", &impl->new_grid.arguments);
          ImGui::InputInt("N refinements", &impl->new_grid.refinements);
          impl->new_grid.refinements = std::max(0, impl->new_grid.refinements);

          ImGui::TextUnformatted("Example:");
          ImGui::TextUnformatted("  name: hyper_ball");
          ImGui::TextUnformatted("  args: 0.0, 0.0 : 1 : false");

          if (!impl->new_grid.error.empty())
            {
              ImGui::Separator();
              ImGui::TextWrapped("%s", impl->new_grid.error.c_str());
            }

          ImGui::Separator();
          if (ImGui::Button("Create"))
            {
              std::string error;
              if (!GenerateTriangulation(impl->mesh,
                                         impl->new_grid.name,
                                         impl->new_grid.arguments,
                                         impl->new_grid.refinements,
                                         error))
                {
                  impl->new_grid.error = error;
                }
              else
                {
                  impl->status.clear();
                  impl->new_grid.open = false;
                  ImGui::CloseCurrentPopup();
                  RebuildScene(impl->glfw_window, impl->mesh, impl->vtk, true);
                }
            }
          ImGui::SameLine();
          if (ImGui::Button("Cancel"))
            {
              impl->new_grid.open = false;
              ImGui::CloseCurrentPopup();
            }

          ImGui::EndPopup();
        }
    }

  int mode_int = static_cast<int>(impl->mesh.mode);
  if (ImGui::RadioButton("Boundary faces", mode_int == 0))
    impl->mesh.mode = MeshState::Mode::boundary;
  ImGui::SameLine();
  if (ImGui::RadioButton("Cells", mode_int == 1))
    impl->mesh.mode = MeshState::Mode::cell;

	  ImGui::Separator();
	  DrawSelectionPanel(impl->mesh, impl->vtk);

	  if (!impl->status.empty())
	    {
	      ImGui::Separator();
      ImGui::TextWrapped("%s", impl->status.c_str());
    }

  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(root_pos.x + impl->left_panel_width, root_pos.y));
  ImGui::SetNextWindowSize(ImVec2(splitter_width, root_size.y));
  const ImGuiWindowFlags splitter_flags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
  ImGui::Begin("##coral_manipulator_splitter", nullptr, splitter_flags);
  const float splitter_height = root_size.y;
  ImGui::InvisibleButton("##coral_manipulator_splitter_btn",
                         ImVec2(splitter_width, splitter_height));
  if (ImGui::IsItemHovered())
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  if (ImGui::IsItemActive())
    {
      impl->left_panel_width += ImGui::GetIO().MouseDelta.x;
      impl->left_panel_width =
        std::max(min_left, std::min(max_left, impl->left_panel_width));
    }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(root_pos.x + impl->left_panel_width + splitter_width,
                                 root_pos.y));
  ImGui::SetNextWindowSize(
    ImVec2(std::max(0.0f, root_size.x - impl->left_panel_width - splitter_width),
           root_size.y));
  const ImGuiWindowFlags overlay_flags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::Begin("##coral_manipulator_view_overlay", nullptr, overlay_flags);

  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 pad(8.0f, 6.0f);
  const char  *help = impl->mouse_over_grid_any ?
                        "LMB drag: box select   |   LMB click: select   |   Shift add   |   Alt subtract   |   RMB drag: rotate   |   Wheel: zoom" :
                        "LMB drag: pan camera   |   RMB drag: rotate   |   Wheel: zoom";
  const ImVec2 text_sz = ImGui::CalcTextSize(help);
  const ImVec2 bg_min(p0.x + pad.x, p0.y + pad.y);
  const ImVec2 bg_max(bg_min.x + text_sz.x + 2.0f * pad.x,
                      bg_min.y + text_sz.y + 2.0f * pad.y);
  dl->AddRectFilled(bg_min,
                    bg_max,
                    ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.06f, 0.65f)),
                    6.0f);
  dl->AddText(ImVec2(bg_min.x + pad.x, bg_min.y + pad.y),
              ImGui::GetColorU32(ImVec4(1, 1, 1, 0.90f)),
              help);

  if (impl->box_select_active)
    {
      ImDrawList *fg = ImGui::GetForegroundDrawList();
      const ImU32 fill_col = ImGui::GetColorU32(ImVec4(0.95f, 0.85f, 0.20f, 0.10f));
      const ImU32 line_col = ImGui::GetColorU32(ImVec4(0.95f, 0.85f, 0.20f, 0.90f));
      const ImVec2 a = impl->box_start_screen;
      const ImVec2 b = impl->box_end_screen;
      const ImVec2 rmin(std::min(a.x, b.x), std::min(a.y, b.y));
      const ImVec2 rmax(std::max(a.x, b.x), std::max(a.y, b.y));
      fg->AddRectFilled(rmin, rmax, fill_col, 0.0f);
      fg->AddRect(rmin, rmax, line_col, 0.0f, 0, 2.0f);
    }

  ImGui::End();
}

std::string
ManipulatorApp::status_text() const
{
  return impl->status;
}

} // namespace coral::manipulator
