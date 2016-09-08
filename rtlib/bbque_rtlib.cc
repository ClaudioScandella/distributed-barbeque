/*
 * Copyright (C) 2012  Politecnico di Milano
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bbque/rtlib.h"

#include "bbque/version.h"
#include "bbque/utils/timer.h"
#include "bbque/utils/utility.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/logging/console_logger.h"

#include "bbque/rtlib/bbque_rpc.h"
#ifdef CONFIG_BBQUE_OPENCL
  #include "bbque/rtlib/bbque_ocl.h"
#endif

#ifdef CONFIG_TARGET_ANDROID
# include <stdint.h>
#else
# include <cstdint>
#endif

namespace bb = bbque;
namespace bu = bbque::utils;
namespace bl = bbque::rtlib;

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "rtl"

/**
 * The global timer, this can be used to get the time since the RTLib has been
 * initialized */
bu::Timer bbque_tmr(true);

/**
 * A pointer to the Barbeque RPC communication channel
 */
static bl::BbqueRPC *rpc = NULL;

/**
 * The collection of RTLib services accessible from applications.
 */
RTLIB_Services_t rtlib_services;

#ifdef CONFIG_BBQUE_OPENCL
/**
 * The collection of RTLib wrapped OpenCL functions.
 */
RTLIB_OpenCL_t rtlib_ocl;

/**
 * The map contains OpenCL command queues and shared pointers to profiling
 * events data structures
 */
std::map<cl_command_queue, QueueProfPtr_t> ocl_queues_prof;

/**
 * The map contains OpenCL command types and their respective string values
 */
std::map<cl_command_type, std::string> ocl_cmd_str;
std::map<void *, cl_command_type> ocl_addr_cmd;

#endif // CONFIG_BBQUE_OPENCL

static RTLIB_ExecutionContextHandler_t rtlib_register(const char *name,
		const RTLIB_ExecutionContextParams_t *params) {
	return rpc->Register(name, params);
}

static void rtlib_unregister(
		const RTLIB_ExecutionContextHandler_t ech) {
	return rpc->Unregister(ech);
}

static RTLIB_ExitCode_t rtlib_enable(
		const RTLIB_ExecutionContextHandler_t ech) {
	return rpc->Enable(ech);
}

static RTLIB_ExitCode_t rtlib_disable(
		const RTLIB_ExecutionContextHandler_t ech) {
	return rpc->Disable(ech);
}

static RTLIB_ExitCode_t rtlib_getwm(
		const RTLIB_ExecutionContextHandler_t ech,
		RTLIB_WorkingModeParams_t *wm,
		RTLIB_SyncType_t st) {
	return rpc->GetWorkingMode(ech, wm, st);
}

static RTLIB_ExitCode_t rtlib_set(
		RTLIB_ExecutionContextHandler_t ech,
		RTLIB_Constraint_t *constraints, uint8_t count) {
	return rpc->Set(ech, constraints, count);
}

static RTLIB_ExitCode_t rtlib_clear(
		RTLIB_ExecutionContextHandler_t ech) {
	return rpc->Clear(ech);
}

static RTLIB_ExitCode_t rtlib_ggap(
		RTLIB_ExecutionContextHandler_t ech,
		int gap) {
	return rpc->SetExplicitGGap(ech, gap);
}

/*******************************************************************************
 *    Utility Functions
 ******************************************************************************/

static const char *rtlib_utils_getchuid() {
	return rpc->GetChUid();
}

static AppUid_t rtlib_utils_getuid(
		RTLIB_ExecutionContextHandler_t ech) {
	return rpc->GetUid(ech);
}

static RTLIB_ExitCode_t rtlib_utils_get_resources(
		RTLIB_ExecutionContextHandler_t ech,
		const RTLIB_WorkingModeParams_t *wm,
		RTLIB_ResourceType_t r_type,
		int32_t & r_amount) {
	return rpc->GetAssignedResources(ech, wm, r_type, r_amount);
}

static RTLIB_ExitCode_t rtlib_utils_get_resources_array(
        RTLIB_ExecutionContextHandler_t ech,
        const RTLIB_WorkingModeParams_t *wm,
        RTLIB_ResourceType_t r_type,
        int32_t *sys_array,
        uint16_t array_size) {
    return rpc->GetAssignedResources(ech, wm, r_type, sys_array, array_size);
}

/*******************************************************************************
 *    Cycles Per Second (CPS) Control Support
 ******************************************************************************/

static RTLIB_ExitCode_t rtlib_cps_set(
		RTLIB_ExecutionContextHandler_t ech,
		float cps) {
	return rpc->SetCPS(ech, cps);
}

static float rtlib_cps_get(
		RTLIB_ExecutionContextHandler_t ech) {
	return rpc->GetCPS(ech);
}

static RTLIB_ExitCode_t rtlib_cps_goal_set(
		RTLIB_ExecutionContextHandler_t ech,
		float cps_min,
		float cps_max) {
	return rpc->SetCPSGoal(ech, cps_min, cps_max);
}

static RTLIB_ExitCode_t rtlib_cps_set_ctime_us(
		RTLIB_ExecutionContextHandler_t ech,
		uint32_t us) {
	return rpc->SetCTimeUs(ech, us);
}

/*******************************************************************************
 *    Performance Monitoring Support
 ******************************************************************************/

static void rtlib_notify_setup(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifySetup(ech);
}

static void rtlib_notify_init(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyInit(ech);
}

static void rtlib_notify_exit(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyExit(ech);
}

static void rtlib_notify_pre_configure(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPreConfigure(ech);
}

static void rtlib_notify_post_configure(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPostConfigure(ech);
}

static void rtlib_notify_pre_run(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPreRun(ech);
}

static void rtlib_notify_post_run(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPostRun(ech);
}

static void rtlib_notify_pre_monitor(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPreMonitor(ech);
}

static void rtlib_notify_post_monitor(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPostMonitor(ech);
}

static void rtlib_notify_pre_suspend(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPreSuspend(ech);
}

static void rtlib_notify_post_suspend(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPostSuspend(ech);
}

static void rtlib_notify_pre_resume(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPreResume(ech);
}

static void rtlib_notify_post_resume(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyPostResume(ech);
}

static void rtlib_notify_release(
		RTLIB_ExecutionContextHandler_t ech) {
	rpc->NotifyRelease(ech);
}

const char *rtlib_app_name;
static uint8_t rtlib_initialized = 0;
static std::unique_ptr<bu::Logger> logger;

#include "bbque_errors.cc"

RTLIB_ExitCode_t RTLIB_Init(const char *name, RTLIB_Services_t **rtlib) {
	RTLIB_ExitCode_t result;
	(*rtlib) = NULL;

	// Checking error string array consistency
	static_assert(ARRAY_SIZE(RTLIB_errorStr) == RTLIB_EXIT_CODE_COUNT,
			"RTLIB error strings not matching errors count");

	assert(rtlib_initialized==0);

	// Get a Logger module
	logger = bu::Logger::GetLogger(BBQUE_LOG_MODULE);

	// Welcome screen
	logger->Info("Barbeque RTLIB (ver. %s)\n", g_git_version);
	logger->Info("Built: " __DATE__  " " __TIME__ "\n");

	// Data structure initialization
	rtlib_services.version.major = RTLIB_VERSION_MAJOR;
	rtlib_services.version.minor = RTLIB_VERSION_MINOR;

	rtlib_services.Register = rtlib_register;
	rtlib_services.Enable = rtlib_enable;
	rtlib_services.GetWorkingMode = rtlib_getwm;
	rtlib_services.SetConstraints = rtlib_set;
	rtlib_services.ClearConstraints = rtlib_clear;
	rtlib_services.SetGoalGap = rtlib_ggap;
	rtlib_services.Disable = rtlib_disable;
	rtlib_services.Unregister = rtlib_unregister;

	// Utility functions interface
	rtlib_services.Utils.GetChUid = rtlib_utils_getchuid;
	rtlib_services.Utils.GetUid = rtlib_utils_getuid;
	rtlib_services.Utils.GetResources = rtlib_utils_get_resources;
	rtlib_services.Utils.GetResourcesArray = rtlib_utils_get_resources_array;

	// Cycles Time Control interface
	rtlib_services.CPS.Set = rtlib_cps_set;
	rtlib_services.CPS.Get = rtlib_cps_get;
	rtlib_services.CPS.SetGoal = rtlib_cps_goal_set;
	rtlib_services.CPS.SetCTimeUs = rtlib_cps_set_ctime_us;

	// Performance monitoring notifiers
	rtlib_services.Notify.Setup = rtlib_notify_setup;
	rtlib_services.Notify.Init = rtlib_notify_init;
	rtlib_services.Notify.Exit = rtlib_notify_exit;
	rtlib_services.Notify.PreConfigure = rtlib_notify_pre_configure;
	rtlib_services.Notify.PostConfigure = rtlib_notify_post_configure;
	rtlib_services.Notify.PreRun = rtlib_notify_pre_run;
	rtlib_services.Notify.PostRun = rtlib_notify_post_run;
	rtlib_services.Notify.PreMonitor = rtlib_notify_pre_monitor;
	rtlib_services.Notify.PostMonitor = rtlib_notify_post_monitor;
	rtlib_services.Notify.PreSuspend = rtlib_notify_pre_suspend;
	rtlib_services.Notify.PostSuspend = rtlib_notify_post_suspend;
	rtlib_services.Notify.PreResume = rtlib_notify_pre_resume;
	rtlib_services.Notify.PostResume = rtlib_notify_post_resume;
	rtlib_services.Notify.Release = rtlib_notify_release;

#ifdef CONFIG_BBQUE_OPENCL
	// OpenCL support initialization
	rtlib_ocl_init();
#endif

	// Building a communication channel
	rpc = bl::BbqueRPC::GetInstance();
	if (!rpc) {
		logger->Error("RPC communication channel build FAILED");
		return RTLIB_BBQUE_CHANNEL_SETUP_FAILED;
	}

	// Initializing the RPC communication channel
	result = rpc->Init(name);
	if (result!=RTLIB_OK) {
		logger->Error("RPC communication channel initialization FAILED");
		return RTLIB_BBQUE_UNREACHABLE;
	}

	// Setup configuration descriptor
	rtlib_services.conf = bl::BbqueRPC::Configuration();

	// Marking library as intialized
	rtlib_initialized = 1;
	rtlib_app_name = name;

	(*rtlib) = &rtlib_services;
	return RTLIB_OK;
}

__attribute__((destructor))
static void RTLIB_Exit(void) {

	logger = bu::ConsoleLogger::GetInstance(BBQUE_LOG_MODULE);
	logger->Debug("Barbeque RTLIB, Cleanup and release");

	if (!rtlib_initialized)
		return;

	// Close the RPC FIFO channel thus releasing all BBQUE resource used by
	// this application
	assert(rpc);

	// Ensure all the EXCs are unregistered
	rpc->UnregisterAll();
	delete rpc;

	if (0) RTLIB_Exit(); // Fix compilation warning

}


