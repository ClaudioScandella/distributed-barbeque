#ifndef RTLIB_TYPES_WRAPPERS_H
#define RTLIB_TYPES_WRAPPERS_H

#include <pybind11/pybind11.h>
#include <bbque/bbque_exc.h>

namespace py = pybind11;

using bbque::rtlib::BbqueEXC;

/*
 * We wrap RTLIB_Services inside another object since pybind11 doesn't support
 * functions with double pointer arguments
 */
struct RTLIB_Services_Wrapper {
   RTLIB_Services_t *services;
};

/*
 * In python int objects are immutable so they can't be changed by a function
 * when passed by pointer, need a wrapper for them
 */
struct RTLIB_Resources_Amount_Wrapper {
   int32_t amount;
};

class RTLIB_Resources_Systems_Wrapper {
   public:
      RTLIB_Resources_Systems_Wrapper(uint16_t number_of_systems) :
         _number_of_systems(number_of_systems) {
         _systems.reserve(number_of_systems);
      }
      int32_t * systems() {
         return &_systems[0];
      }
      uint16_t number_of_systems() {
         return _number_of_systems;
      }
   private:
      uint16_t _number_of_systems;
      std::vector<int32_t> _systems;
};

class RTLIB_AffinityMasks_Wrapper {
   public:
      RTLIB_AffinityMasks_Wrapper(int number_of_masks) :
         _number_of_masks(number_of_masks) {
         _masks.reserve(number_of_masks);
      }
      int32_t * masks() {
         return &_masks[0];
      }
      int number_of_masks() {
         return _number_of_masks;
      }
   private:
      int _number_of_masks;
      std::vector<int32_t> _masks;
};

/*
 * Simple wrapper for the logger, the formatting is done on the python side
 * so we just pass a string as argument
 */
class RTLIB_Logger_Wrapper {
   public:
      RTLIB_Logger_Wrapper(std::unique_ptr<bu::Logger> &logger) :
         w_logger(logger){}
      void Debug(std::string msg) { w_logger->Debug("%s", msg.c_str()); };
      void Info(std::string msg) { w_logger->Info("%s", msg.c_str()); };
      void Notice(std::string msg) { w_logger->Notice("%s", msg.c_str()); };
      void Warn(std::string msg) { w_logger->Warn("%s", msg.c_str()); };
      void Error(std::string msg) { w_logger->Error("%s", msg.c_str()); };
      void Crit(std::string msg) { w_logger->Crit("%s", msg.c_str()); };
      void Alert(std::string msg) { w_logger->Alert("%s", msg.c_str()); };
      void Fatal(std::string msg) { w_logger->Fatal("%s", msg.c_str()); };

   private:
      std::unique_ptr<bu::Logger> &w_logger;
};

void init_wrappers(py::module &m);

#endif
