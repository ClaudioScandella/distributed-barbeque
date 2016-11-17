#include "bbque/configuration_manager.h"
#include "bbque/realtime_manager.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <linux/version.h>

#define REALTIME_MANAGER_NAMESPACE "bq.rtm"

#define FILE_PROC_SCHED_RR_Q    "/proc/sys/kernel/sched_rr_timeslice_ms"
#define FILE_PROC_SCHED_PERIOD  "/proc/sys/kernel/sched_rt_period_us"
#define FILE_PROC_SCHED_RUNTIME "/proc/sys/kernel/sched_rt_runtime_us"

#define DEFAULT_SCHED_PERIOD 1000000	// 1s


#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,9,0)
	#error "Real-Time capabilities of Barbeque require Linux kernel 3.9"
#endif



namespace bbque {

RealTimeManager::RealTimeManager() noexcept {


	// Get a logger
	logger = bu::Logger::GetLogger(REALTIME_MANAGER_NAMESPACE);
	assert(logger);

	SetRTLevel();

	SetKernelReservation();

}

void RealTimeManager::SetRTLevel() noexcept {
	ConfigurationManager cfm(ConfigurationManager::GetInstance());

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

void RealTimeManager::SetKernelReservation() noexcept {

	// The problem of threads with SCHED_FIFO, SCHED_RR or SCHED_DEADLINE
	// scheduling policies is that a nonblocking infinite loop in one of these
	// threads will block all threads with lower priority forever.

	// Thus, we have to set the limit of maximum cpu time available for RT
	// tasks.

	std::ofstream  period_file(FILE_PROC_SCHED_PERIOD);
	std::ofstream runtime_file(FILE_PROC_SCHED_RUNTIME);

	if (! period_file.good() ) {
		logger->Crit("Unable to open RT period file [%s] RT processes may not "
					 "work as expected.", FILE_PROC_SCHED_PERIOD);
		return;
	}

	if (! runtime_file.good() ) {
		logger->Crit("Unable to open RT runtime file  [%s] RT processes may "
					 "not work as expected", FILE_PROC_SCHED_RUNTIME);
		return;
	}

	std::ostringstream ss_period, ss_runtime;
	ss_period << DEFAULT_SCHED_PERIOD;
	ss_runtime << BBQUE_RT_MAX_CPU * 1000;	// selected in menuconfig

	period_file.write(ss_period.str().c_str(), ss_period.str().size());
	runtime_file.write(ss_runtime.str().c_str(), ss_runtime.str().size());

	// FLush is needed to check  if the writes have success.
	period_file.flush();
	runtime_file.flush();

	if (! period_file.good() || ! runtime_file.good() ) {
		logger->Crit("Unexpected error writing to /proc files. RT processes may "
					 "not work as expected");
		return;
	}

	period_file.close();
	runtime_file.close();

	logger->Info("Max system RT time configured [%d per-mille]",
				BBQUE_RT_MAX_CPU);

	// Now read the default quantum of RR scheduler, this can be useful in
	// future for the policies
	std::ifstream  quantum_file(FILE_PROC_SCHED_RR_Q);

	if (! quantum_file.good() ) {
		logger->Crit("Unable to open RR quantum file [%s] default RR quantum "
					 "will be set", FILE_PROC_SCHED_RR_Q);
		sched_rr_interval_ms = 30;
		return;
	}

	quantum_file >> sched_rr_interval_ms;
	quantum_file.close();

}


RealTimeManager::~RealTimeManager() {


}

RealTimeManager::ExitCode_t RealTimeManager::SetupApp(app::AppPtr_t app) {
	

}


}	// namespace bbque

