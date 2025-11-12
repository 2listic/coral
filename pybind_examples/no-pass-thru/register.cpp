#include <iostream>
#include <pybind11/pybind11.h>

namespace py = pybind11;

#define PRINT_ME std::cout << __PRETTY_FUNCTION__ << std::endl

class Instrumented {
    int m_value;

public:
    explicit Instrumented(int value)
        : m_value{value}
    {
        PRINT_ME;
    }

    Instrumented(const Instrumented& other) noexcept
        : m_value{other.m_value}
    {
        PRINT_ME;
    }

    Instrumented(Instrumented&& other) noexcept
        : m_value{other.m_value}
    {
        PRINT_ME;
    }

    Instrumented& operator=(const Instrumented& other) noexcept
    {
        PRINT_ME;

        m_value = other.m_value;
        return *this;
    }

    Instrumented& operator=(Instrumented&& other) noexcept
    {
        PRINT_ME;

        m_value = other.m_value;
        return *this;
    }

    ~Instrumented() noexcept
    {
        PRINT_ME;
    }

    int get_value() const noexcept
    {
        return m_value;
    }

    void set_value(int value) noexcept
    {
        m_value = value;
    }
};

int get_and_set(Instrumented& inst, int new_value)
{
    int old_value = inst.get_value();
    inst.set_value(new_value);
    return old_value;
}

PYBIND11_MODULE(no_pass_thru, m, py::mod_gil_not_used())
{
    using ReleaseGIL =  py::call_guard<py::gil_scoped_release>;

    py::class_<Instrumented>(m, "Instrumented")
        .def(py::init<int>(), ReleaseGIL())
        .def("get_value", &Instrumented::get_value, ReleaseGIL())
        .def("set_value", &Instrumented::set_value, ReleaseGIL());

    m.def("get_and_set", [](Instrumented& inst, int new_val) -> std::pair<int, Instrumented&> {
        // Well defined since P0145R3.
        // otherwise return simply a pointer
        return {get_and_set(inst, new_val), std::ref(inst)};
    }, py::return_value_policy::reference, ReleaseGIL());
}