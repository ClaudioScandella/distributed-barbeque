#ifndef RTLIB_ENUMS_H
#define RTLIB_ENUMS_H

#include <pybind11/pybind11.h>
#include <bbque/bbque_exc.h>

namespace py = pybind11;

void init_enums(py::module &m);

#endif
