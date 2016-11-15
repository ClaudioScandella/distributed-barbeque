#include "bbque/configuration_manager.h"
#include "bbque/realtime_manager.h"

#define REALTIME_MANAGER_NAMESPACE "bq.rtm"

namespace bbque {

RealTimeManager::RealTimeManager() noexcept {
	ConfigurationManager cfm(ConfigurationManager::GetInstance());

	// Get a logger
	logger = bu::Logger::GetLogger(REALTIME_MANAGER_NAMESPACE);
	assert(logger);

	short int sys_rt_level = cfm.GetRTLevel();

	switch ( sys_rt_level ) {
		case -1:
			logger->Error("No information about preemption model in the "
						 "kernel.");
	
			this->is_soft = true;
#ifdef CONFIG_BBQUE_RT_HARD
			this->is_hard = true;
			logger->Warn("I will continue assuming the kernel full "
						 "preemptive!");
#else
			logger->Warn("I will continue assuming the kernel partially "
						 "preemptive!");
#endif
			
			
			break;

		case 0:
			logger->Warn("Barbeque compiled with Real-Time support but the "
						 "kernel does not support it.");
			logger->Error("RT support disabled.");
			break;

		case 1:
			logger->Info("Kernel supports voluntary preemption (Soft RT).");
			this->is_soft = true;
			break;
		
		case 2:
		case 3:
			logger->Info("Kernel supports low-latency desktop preemption "
						 "(Soft RT).");
			this->is_soft = true;
			break;

		case 4:
			logger->Info("Kernel supports full preemption (Hard RT).");
			this->is_soft = true;
#ifdef CONFIG_BBQUE_RT_HARD
			this->is_hard = true;
#endif
			break;

		default:
			logger->Crit("Unexpected Real-Time level [%d]", sys_rt_level);
		break;
	}

}

RealTimeManager::~RealTimeManager() {


}


}

