#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_out.h>

#include <pybind11/pybind11.h>

#include <sstream>

namespace py = pybind11;

PYBIND11_MODULE(coral, m, py::mod_gil_not_used())
{
    using namespace dealii;

    py::class_<Triangulation<2>>(m, "Triangulation_2")
        .def(py::init<>(), py::call_guard<py::gil_scoped_release_simple>())
        .def("refine_global", &Triangulation<2>::refine_global, py::call_guard<py::gil_scoped_release_simple>());

    m.def("hyper_cube_2_2", &GridGenerator::hyper_cube<2 ,2>,
        py::arg("triangulation"),
        py::arg("left") = 0.0,
        py::arg("right") = 1.0,
        py::arg("colorize") = false,
        py::call_guard<py::gil_scoped_release_simple>()
    );

    m.def("generate_from_name_and_arguments_2_2", &GridGenerator::generate_from_name_and_arguments<2, 2>,
        py::arg("triangulation"),
        py::arg("grid_generator_function_name"),
        py::arg("grid_generator_function_arguments"),
        py::call_guard<py::gil_scoped_release_simple>()
    );

    py::class_<GridOut>(m, "GridOut")
        .def(py::init<>())
        .def("write_svg_2", [](GridOut& self, const Triangulation<2>& tria, py::object file){
            if (!py::hasattr(file, "write"))
                throw std::runtime_error("Expected a file-like object with a 'write' method");

            py::object write_method = file.attr("write");

            std::ostringstream buffer;
            self.write_svg(tria, buffer);
            
            write_method(buffer.str());
        },
        py::arg("triangulation"),
        py::arg("file")
    );
}