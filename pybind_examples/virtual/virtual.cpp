#include <pybind11/pybind11.h>
#include <iostream>

#define PRINT_FUNC std::cout << __PRETTY_FUNCTION__ << std::endl

struct Base {
    explicit Base()
    {
        PRINT_FUNC;
    }

    virtual void method()
    {
        PRINT_FUNC;
    }

    virtual ~Base()
    {
        PRINT_FUNC;
    }
};

struct Derived : public Base {
    using Base::Base;

    virtual void method() override
    {
        PRINT_FUNC;
    }

    virtual ~Derived()
    {
        PRINT_FUNC;
    }
};

void play_method(Base *base)
{
    base->method();
}

namespace py = pybind11;

PYBIND11_MODULE(virtual, m, py::mod_gil_not_used()) {
    py::class_<Base>(m, "Base")
        .def(py::init<>())
        .def("method", &Base::method);

    py::class_<Derived, Base>(m, "Derived")
        .def(py::init<>())
        .def("method", &Derived::method);

    m.def("play_method", &play_method);
}
