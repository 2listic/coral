#ifndef CORAL_BACKENDS_UTILITIES_H
#define CORAL_BACKENDS_UTILITIES_H

#include <deal.II/base/types.h>

#include <deal.II/grid/cell_data.h>
#include <deal.II/grid/grid_in.h>
#include <deal.II/grid/tria.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef DEAL_II_WITH_VTK
#  include <vtkCell.h>
#  include <vtkCellData.h>
#  include <vtkDataArray.h>
#  include <vtkPoints.h>
#  include <vtkSmartPointer.h>
#  include <vtkUnstructuredGrid.h>
#  include <vtkUnstructuredGridReader.h>
#  include <vtkXMLUnstructuredGridReader.h>
#endif

namespace dealii
{
  namespace internal
  {
    inline std::string
    lowercase_extension(const std::string &file_name)
    {
      std::string extension =
        std::filesystem::path(file_name).extension().string();
      std::transform(extension.begin(),
                     extension.end(),
                     extension.begin(),
                     [](const unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                     });
      return extension;
    }

#ifdef DEAL_II_WITH_VTK
    inline auto
    get_optional_vtk_array(vtkCellData *cell_data, const char *name)
      -> vtkDataArray *
    {
      return cell_data != nullptr ? cell_data->GetArray(name) : nullptr;
    }



    inline long long
    get_vtk_id(vtkDataArray *array, const vtkIdType cell_index)
    {
      return array != nullptr ?
               static_cast<long long>(array->GetComponent(cell_index, 0)) :
               0LL;
    }



    template <int structdim>
    inline void
    fill_vertices(CellData<structdim> &cell_data, vtkCell *cell)
    {
      for (unsigned int j = 0; j < cell_data.vertices.size(); ++j)
        cell_data.vertices[j] = static_cast<unsigned int>(cell->GetPointId(j));
    }



    template <int dim>
    inline void
    reorder_vtk_vertices(CellData<dim> &cell_data, const int vtk_cell_type)
    {
      if constexpr (dim == 2)
        {
          if (vtk_cell_type == VTK_QUAD)
            std::swap(cell_data.vertices[2], cell_data.vertices[3]);
        }
      else if constexpr (dim == 3)
        {
          if (vtk_cell_type == VTK_HEXAHEDRON)
            {
              std::swap(cell_data.vertices[2], cell_data.vertices[3]);
              std::swap(cell_data.vertices[6], cell_data.vertices[7]);
            }
        }
    }



    template <int structdim>
    inline void
    set_subcell_metadata(CellData<structdim> &subcell,
                         vtkDataArray        *boundary_or_material_ids,
                         vtkDataArray        *manifold_ids,
                         const vtkIdType      cell_index)
    {
      subcell.boundary_id =
        boundary_or_material_ids != nullptr ?
          static_cast<types::boundary_id>(
            get_vtk_id(boundary_or_material_ids, cell_index)) :
          types::boundary_id{0};
      subcell.manifold_id = manifold_ids != nullptr ?
                              static_cast<types::manifold_id>(
                                get_vtk_id(manifold_ids, cell_index)) :
                              numbers::flat_manifold_id;
    }



    template <int dim>
    inline void
    append_vtk_cell(vtkCell                    *cell,
                    const vtkIdType             cell_index,
                    vtkDataArray               *boundary_or_material_ids,
                    vtkDataArray               *manifold_ids,
                    std::vector<CellData<dim>> &cells,
                    SubCellData                &subcell_data)
    {
      const int vtk_cell_type = cell->GetCellType();

      if constexpr (dim == 1)
        {
          AssertThrow(vtk_cell_type == VTK_LINE,
                      ExcMessage("Unsupported cell type in 1D VTK file: only "
                                 "VTK_LINE is supported."));
          AssertThrow(cell->GetNumberOfPoints() == 2,
                      ExcMessage(
                        "Only line cells with 2 points are supported."));

          CellData<1> cell_data(2);
          fill_vertices(cell_data, cell);
          cell_data.material_id = static_cast<types::material_id>(
            get_vtk_id(boundary_or_material_ids, cell_index));
          cell_data.manifold_id = manifold_ids != nullptr ?
                                    static_cast<types::manifold_id>(
                                      get_vtk_id(manifold_ids, cell_index)) :
                                    numbers::flat_manifold_id;
          cells.push_back(cell_data);
        }
      else if constexpr (dim == 2)
        {
          if (vtk_cell_type == VTK_QUAD || vtk_cell_type == VTK_TRIANGLE)
            {
              const unsigned int n_vertices =
                vtk_cell_type == VTK_QUAD ? 4U : 3U;
              AssertThrow(static_cast<unsigned int>(
                            cell->GetNumberOfPoints()) == n_vertices,
                          ExcMessage("Unexpected number of vertices for a 2D "
                                     "VTK cell."));

              CellData<2> cell_data(n_vertices);
              fill_vertices(cell_data, cell);
              reorder_vtk_vertices(cell_data, vtk_cell_type);
              cell_data.material_id = static_cast<types::material_id>(
                get_vtk_id(boundary_or_material_ids, cell_index));
              cell_data.manifold_id =
                manifold_ids != nullptr ?
                  static_cast<types::manifold_id>(
                    get_vtk_id(manifold_ids, cell_index)) :
                  numbers::flat_manifold_id;
              cells.push_back(cell_data);
              return;
            }

          AssertThrow(vtk_cell_type == VTK_LINE,
                      ExcMessage("Unsupported cell type in 2D VTK file: only "
                                 "VTK_QUAD, VTK_TRIANGLE, and VTK_LINE are "
                                 "supported."));
          AssertThrow(cell->GetNumberOfPoints() == 2,
                      ExcMessage("Only line subcells with 2 points are "
                                 "supported."));

          CellData<1> line_data(2);
          fill_vertices(line_data, cell);
          set_subcell_metadata(line_data,
                               boundary_or_material_ids,
                               manifold_ids,
                               cell_index);
          subcell_data.boundary_lines.push_back(line_data);
        }
      else if constexpr (dim == 3)
        {
          switch (vtk_cell_type)
            {
              case VTK_HEXAHEDRON:
              case VTK_TETRA:
              case VTK_WEDGE:
              case VTK_PYRAMID:
                {
                  const unsigned int n_vertices =
                    vtk_cell_type == VTK_HEXAHEDRON ? 8U :
                    vtk_cell_type == VTK_TETRA      ? 4U :
                    vtk_cell_type == VTK_WEDGE      ? 6U :
                                                      5U;

                  AssertThrow(static_cast<unsigned int>(
                                cell->GetNumberOfPoints()) == n_vertices,
                              ExcMessage(
                                "Unexpected number of vertices for a 3D VTK "
                                "cell."));

                  CellData<3> cell_data(n_vertices);
                  fill_vertices(cell_data, cell);
                  reorder_vtk_vertices(cell_data, vtk_cell_type);
                  cell_data.material_id = static_cast<types::material_id>(
                    get_vtk_id(boundary_or_material_ids, cell_index));
                  cell_data.manifold_id =
                    manifold_ids != nullptr ?
                      static_cast<types::manifold_id>(
                        get_vtk_id(manifold_ids, cell_index)) :
                      numbers::flat_manifold_id;
                  cells.push_back(cell_data);
                  return;
                }

              case VTK_QUAD:
              case VTK_TRIANGLE:
                {
                  const unsigned int n_vertices =
                    vtk_cell_type == VTK_QUAD ? 4U : 3U;
                  AssertThrow(
                    static_cast<unsigned int>(cell->GetNumberOfPoints()) ==
                      n_vertices,
                    ExcMessage("Unexpected number of vertices for a 3D face."));

                  CellData<2> face_data(n_vertices);
                  fill_vertices(face_data, cell);
                  set_subcell_metadata(face_data,
                                       boundary_or_material_ids,
                                       manifold_ids,
                                       cell_index);
                  subcell_data.boundary_quads.push_back(face_data);
                  return;
                }

              case VTK_LINE:
                {
                  AssertThrow(cell->GetNumberOfPoints() == 2,
                              ExcMessage("Only line subcells with 2 points are "
                                         "supported."));

                  CellData<1> line_data(2);
                  fill_vertices(line_data, cell);
                  set_subcell_metadata(line_data,
                                       boundary_or_material_ids,
                                       manifold_ids,
                                       cell_index);
                  subcell_data.boundary_lines.push_back(line_data);
                  return;
                }

              default:
                AssertThrow(false,
                            ExcMessage("Unsupported cell type in 3D VTK file: "
                                       "only VTK_HEXAHEDRON, VTK_TETRA, "
                                       "VTK_WEDGE, VTK_PYRAMID, VTK_QUAD, "
                                       "VTK_TRIANGLE, and VTK_LINE are "
                                       "supported."));
            }
        }
      else
        {
          AssertThrow(false, ExcMessage("Unsupported dimension."));
        }
    }
#endif
  } // namespace internal



  template <int dim, int spacedim = dim>
  void
  read_grid(const std::string            &file_name,
            Triangulation<dim, spacedim> &triangulation)
  {
#ifndef DEAL_II_WITH_VTK
    GridIn<dim, spacedim> grid_in(triangulation);
    grid_in.read(file_name);
#else
    std::ifstream file(file_name);
    AssertThrow(file.good(), ExcMessage("VTK file not found: " + file_name));

    vtkSmartPointer<vtkUnstructuredGrid> grid;
    const auto extension = internal::lowercase_extension(file_name);

    if (extension == ".vtk")
      {
        auto reader = vtkSmartPointer<vtkUnstructuredGridReader>::New();
        reader->SetFileName(file_name.c_str());
        reader->Update();
        grid = reader->GetOutput();
      }
    else if (extension == ".vtu")
      {
        auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
        reader->SetFileName(file_name.c_str());
        reader->Update();
        grid = reader->GetOutput();
      }
    else
      {
        AssertThrow(false,
                    ExcMessage("Unsupported VTK grid extension: " + extension));
      }

    AssertThrow(grid != nullptr, ExcMessage("Failed to read VTK grid."));

    vtkPoints *vtk_points = grid->GetPoints();
    AssertThrow(vtk_points != nullptr,
                ExcMessage("VTK grid does not contain point data."));

    const vtkIdType              n_points = vtk_points->GetNumberOfPoints();
    std::vector<Point<spacedim>> points(static_cast<std::size_t>(n_points));
    for (vtkIdType i = 0; i < n_points; ++i)
      {
        std::array<double, 3> coords{{0.0, 0.0, 0.0}};
        vtk_points->GetPoint(i, coords.data());

        for (unsigned int d = 0; d < spacedim; ++d)
          points[static_cast<std::size_t>(i)][d] = coords[d];

        for (unsigned int d = spacedim; d < 3; ++d)
          AssertThrow(coords[d] == 0.0,
                      ExcMessage("VTK grid has non-zero coordinate in an "
                                 "unused dimension."));
      }

    vtkCellData  *vtk_cell_data = grid->GetCellData();
    vtkDataArray *boundary_or_material_ids =
      internal::get_optional_vtk_array(vtk_cell_data, "MaterialID");
    if (boundary_or_material_ids == nullptr)
      boundary_or_material_ids =
        internal::get_optional_vtk_array(vtk_cell_data, "MaterialID");
    vtkDataArray *manifold_ids =
      internal::get_optional_vtk_array(vtk_cell_data, "ManifoldID");

    std::vector<CellData<dim>> cells;
    SubCellData                subcell_data;

    const vtkIdType n_cells = grid->GetNumberOfCells();
    cells.reserve(static_cast<std::size_t>(n_cells));

    for (vtkIdType i = 0; i < n_cells; ++i)
      internal::append_vtk_cell<dim>(grid->GetCell(i),
                                     i,
                                     boundary_or_material_ids,
                                     manifold_ids,
                                     cells,
                                     subcell_data);

    triangulation.create_triangulation(points, cells, subcell_data);
#endif
  }
} // namespace dealii

#endif
