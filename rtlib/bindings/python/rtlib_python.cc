#include <pybind11/pybind11.h>
#include "rtlib_enums.h"
#include "rtlib_types_wrappers.h"
#include "rtlib_bbqueexc.h"

namespace py = pybind11;

PYBIND11_PLUGIN(barbeque) {
   // python module declaration
   py::module m("barbeque", R"pbdoc(
      Barbeque bindings
      -----------------

      .. currentmodule:: barbeque

      .. autosummary::
         :toctree: _generate

   )pbdoc");

   // RTLIB_Services struct
   py::class_<RTLIB_Services>(m, "RTLIB_Services")
      .def(py::init<>());

   py::class_<RTLIB_Constraint>(m, "RTLIB_Constraint")
      .def_readwrite("awm", &RTLIB_Constraint::awm)
      .def_readwrite("operation", &RTLIB_Constraint::operation)
      .def_readwrite("type", &RTLIB_Constraint::type);

   py::class_<RTLIB_WorkingModeParams>(m, "RTLIB_WorkingModeParams")
      .def_readwrite("awm_id", &RTLIB_WorkingModeParams::awm_id)
      .def_readwrite("services", &RTLIB_WorkingModeParams::services)
      .def_readwrite("nr_sys", &RTLIB_WorkingModeParams::nr_sys)
      .def_readwrite("systems", &RTLIB_WorkingModeParams::systems);

   m.def("RTLIB_Init",
         [](const char *name, RTLIB_Services_Wrapper &services) {
            return RTLIB_Init(name, &services.services);
         }, R"pbdoc(
      Initialize Execution Context
   )pbdoc");

   init_enums(m);
   init_wrappers(m);
   init_BbqueEXC(m);

   m.attr("__version__") = py::str("dev");

   return m.ptr();
}
