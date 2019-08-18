#ifndef RTLIB_PYBBQUEEXC_H
#define RTLIB_PYBBQUEEXC_H

#include <pybind11/pybind11.h>
#include <bbque/bbque_exc.h>
#include "rtlib_types_wrappers.h"

namespace py = pybind11;

/*
 * This is the trampoline class that will be implemented from the python side
 *
 * NOTES:
 * 1- python doesn't have class attribute accessor, every field and functions
 *    are public.
 * 2- private and protected methods must be redefined in the trampoline class if
 *    we want to be able to access them from python
 */
class PyBbqueEXC : public BbqueEXC {
   public:
      using BbqueEXC::BbqueEXC;

      /*
       * onSetup, onConfigure, onSuspend, onResume, onRun, onMonitor and onSetup
       * are the virtual methods that can be reimplemented on the python side.
       * the py::gil_scoped_release at the start of each function is neede to python
       * GIL, the lock mechanism for python thread safety. Since RTLIB is already
       * thread safe, we let it deal with that. If we don't release the lock, we risk
       * a deadlock between the futexes and mutexs used by the two locking mechanisms
       */
      RTLIB_ExitCode_t onSetup() override {
         py::gil_scoped_release release;

         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onSetup,
               );
      };
      RTLIB_ExitCode_t onConfigure(int8_t awm_id) override {
         py::gil_scoped_release release;
         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onConfigure,
               awm_id
               );
      };
      RTLIB_ExitCode_t onSuspend() override {
         py::gil_scoped_release release;
         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onSuspend,
               );
      };
      RTLIB_ExitCode_t onResume() override {
         py::gil_scoped_release release;
         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onResume,
               );
      };
      RTLIB_ExitCode_t onRun() override {
         py::gil_scoped_release release;
         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onRun,
               );
      };
      RTLIB_ExitCode_t onMonitor() override {
         py::gil_scoped_release release;
         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onMonitor,
               );
      };
      RTLIB_ExitCode_t onRelease() override {
         py::gil_scoped_release release;
         PYBIND11_OVERLOAD(
               RTLIB_ExitCode,
               BbqueEXC,
               onRelease,
               );
      };

      std::unique_ptr<bu::Logger>& get_logger() {
         return logger;
      };
      std::string const get_exc_name() {
         return exc_name;
      };
      std::string const get_rpc_name() {
         return rpc_name;
      };
};

void init_BbqueEXC(py::module &m);

#endif
