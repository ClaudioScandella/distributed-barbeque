#include "rtlib_types_wrappers.h"

void init_wrappers(py::module &m) {
   // wrapper class for RTLIB_Services
   py::class_<RTLIB_Services_Wrapper>(m, "RTLIB_Services_Wrapper")
      .def(py::init<>())
      .def_readwrite("services", &RTLIB_Services_Wrapper::services);

   py::class_<RTLIB_Resources_Amount_Wrapper>(m, "RTLIB_Resources_Amount_Wrapper")
      .def(py::init<>())
      .def_readwrite("amount", &RTLIB_Resources_Amount_Wrapper::amount);

   py::class_<RTLIB_Resources_Systems_Wrapper>(m, "RTLIB_Resources_Systems_Wrapper")
      .def(py::init<uint16_t>())
      .def("systems", &RTLIB_Resources_Systems_Wrapper::systems);

   py::class_<RTLIB_AffinityMasks_Wrapper>(m, "RTLIB_AffinityMasks_Wrapper")
      .def(py::init<int>())
      .def("masks", &RTLIB_AffinityMasks_Wrapper::masks);

   py::class_<RTLIB_Logger_Wrapper>(m, "RTLIB_Logger_Wrapper")
      .def("Debug", &RTLIB_Logger_Wrapper::Debug)
      .def("Info", &RTLIB_Logger_Wrapper::Info)
      .def("Notice", &RTLIB_Logger_Wrapper::Notice)
      .def("Warn", &RTLIB_Logger_Wrapper::Warn)
      .def("Erro", &RTLIB_Logger_Wrapper::Error)
      .def("Crit", &RTLIB_Logger_Wrapper::Crit)
      .def("Alert", &RTLIB_Logger_Wrapper::Alert)
      .def("Fatal", &RTLIB_Logger_Wrapper::Fatal);

}
