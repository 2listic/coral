#include <deal.II/dofs/dof_handler.h>
#include <deal.II/grid/grid_generator.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(coral, m, py::mod_gil_not_used())
{
    using namespace dealii;

    py::class_<Triangulation<2>>(m, "Triangulation2")
        .def(py::init<>());
}