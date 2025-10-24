#include <pybind11/pybind11.h>
#include <cmath>


bool is_prime(size_t n)
{
    size_t max_i = std::sqrt(n) + 1;

    for (size_t i = 2; i <= max_i; ++i)
        if (n % i == 0)
            return false;

    return true;
}

size_t count_primes(size_t max_n)
{
    size_t count = 0;
    for (size_t n = 0; n <= max_n; ++n)
        count += is_prime(n);

    return count;
}


namespace py = pybind11;

PYBIND11_MODULE(primes, m, py::mod_gil_not_used())
{
    m.def("count_primes", &count_primes, py::call_guard<py::gil_scoped_release_simple>());
}
