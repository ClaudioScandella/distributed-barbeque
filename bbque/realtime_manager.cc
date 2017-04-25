#include "bbque/app/application.h"
#include "bbque/configuration_manager.h"
#include "bbque/realtime_manager.h"
#include "bbque/pp/linux_platform_proxy.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <linux/version.h>
#include <linux/sched.h>
#include <sys/resource.h>

#define REALTIME_MANAGER_NAMESPACE "bq.rtm"

#define FILE_PROC_SCHED_RR_Q    "/proc/sys/kernel/sched_rr_timeslice_ms"
#define FILE_PROC_SCHED_PERIOD  "/proc/sys/kernel/sched_rt_period_us"
#define FILE_PROC_SCHED_RUNTIME "/proc/sys/kernel/sched_rt_runtime_us"

#define DEFAULT_SCHED_PERIOD 1000000	// 1s

#if defined(CONFIG_BBQUE_RT_SCHED_FIFO)
	#define SCHED_POLICY SCHED_FIFO
#elif defined(CONFIG_BBQUE_RT_SCHED_RR)
	#define SCHED_POLICY SCHED_RR
#else
	#error "Unknown scheduling policy"
#endif

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

#ifdef CONFIG_BBQUE_RT_SCHED_RR
	// Now read the default quantum of RR scheduler, this can be useful in
	// future for the policies
	std::ifstream  quantum_file(FILE_PROC_SCHED_RR_Q);

	if (unlikely(! quantum_file.good() )) {
		logger->Crit("Unable to open RR quantum file [%s] default RR quantum "
					 "will be set", FILE_PROC_SCHED_RR_Q);
		sched_rr_interval_ms = 30;
		return;
	}

	quantum_file >> sched_rr_interval_ms;
	quantum_file.close();
#endif

	// Now set the RLimit
	unsigned long int linux_max = sched_get_priority_max(SCHED_POLICY);
	const rlimit rl = {linux_max, linux_max};
	int err = setrlimit(RLIMIT_RTPRIO, &rl);
	if (unlikely(0 != err)) {
		logger->Crit("Unable to set rlimit [%d: %s]", 
						errno, strerror(errno));
		return;
	}

	logger->Debug("RT Kernel reservation OK");
}


RealTimeManager::~RealTimeManager() {


}

RealTimeManager::ExitCode_t RealTimeManager::SetupApp(app::AppPtr_t papp) {

	int err;

	if (unlikely(papp->RTLevel() == RT_NONE)) {
		throw std::runtime_error("The application is not RT.");
	}

	std::vector<int> pids;

#ifndef CONFIG_BBQUE_TEST_PLATFORM_DATA
	pp::LinuxPlatformProxy *lpp = pp::LinuxPlatformProxy::GetInstance();

	lpp->GetRegisteredTasks(papp, pids);
#else
	pids.push_back(papp->Pid());
#endif

	// The BBQ priority has a positive range between 0 (highest) and
	// BBQUE_APP_PRIO_LEVELS (lower priority). Instead the RT scheduler wants a
	// priority from 1 (lower) to  N-1 (highest).

	app::AppPrio_t prio      = papp->Priority();

	int linux_min = sched_get_priority_min(SCHED_POLICY);
	int linux_max = sched_get_priority_max(SCHED_POLICY);

	assert (linux_min > 0 && linux_max > 0);

	int      linux_prio = (BBQUE_APP_PRIO_LEVELS - prio + linux_min) // from 5..0 to 1..6
                          * (linux_max-1)	// proportion to linux value
                          / (BBQUE_APP_PRIO_LEVELS + linux_min);


	const struct sched_param rt_sched = { linux_prio };

	for (const auto pid : pids){
		err = sched_setscheduler(pid, SCHED_POLICY, &rt_sched);
		if (unlikely(0 != err)) {
			logger->Error("Unable to setup application [%s] [%d: %s]",
							papp->StrId(), errno, strerror(errno));
			return RTM_SYSCALL_FAILED;
		}
	}

	logger->Debug("Set application RT priority successful [%s] [%d]", 
					papp->StrId(), linux_prio);

	return RTM_OK;
}


}	// namespace bbque

