/*
 * Copyright (C) 2012-2016  Politecnico di Milano
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

#include "rtlib/bbque_exc.h"

#include "bbque/utils/utility.h"

#include <sys/prctl.h>
#include <string.h>
#include <assert.h>

#ifdef BBQUE_DEBUG
#warning Debugging is enabled
#endif

#ifndef BBQUE_RTLIB_DEFAULT_CYCLES
#define BBQUE_RTLIB_DEFAULT_CYCLES 8
#endif

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "exc"

namespace bbque
{
namespace rtlib
{

BbqueEXC::BbqueEXC(std::string const & name,
				   std::string const & recipe,
				   RTLIB_Services_t * const rtl) :
	exc_name(name), rpc_name(recipe), rtlib(rtl),
	config(*(rtlib->config)), cycles_count(0)
{
	// Note: EXC with the same recipe name are not allowed
	RTLIB_EXCParameters_t exc_parameters = {
		{RTLIB_VERSION_MAJOR, RTLIB_VERSION_MINOR},
		RTLIB_LANG_CPP,
		recipe.c_str()
	};

	// Check RTLIB initialization
	if (! rtlib) {
		assert(rtlib);
		return;
	}

	// Get a Logger module
	logger = bu::Logger::GetLogger(BBQUE_LOG_MODULE);
	logger->Info("Initializing a new EXC [%s]...", name.c_str());
	// Register to the resource manager
	assert(rtlib->Register);
	exc_handler = rtlib->Register(name.c_str(), &exc_parameters);

	if (! exc_handler) {
		logger->Error("Registering EXC [%s] FAILED", name.c_str());
		assert(exc_handler);
		return;
	}

	// Set EXC as correctly registered to the resource manager
	exc_status.is_registered = true;
	// Keep track of our UID
	exc_unique_id = rtlib->Utils.GetUniqueID(exc_handler);
	// Set up control groups
	assert(rtlib->SetupCGroups);

	if (rtlib->SetupCGroups(exc_handler) != RTLIB_OK)
		logger->Error("No CGroup support!");

	//--- Spawn the control thread
	control_thread = std::thread(&BbqueEXC::ControlLoop, this);
}

BbqueEXC::~ BbqueEXC()
{
	// Disable the EXC and the Control-Loop
	Disable();
	// Unregister the EXC and Terminate the control loop thread
	Terminate();
}

/*******************************************************************************
 *    Execution Context Management
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::_Enable()
{
	// Check if the EXC is already enabled
	if (exc_status.is_enabled)
		return RTLIB_OK;

	logger->Info("Enabling EXC [%s] (@%p)...",
				 exc_name.c_str(), (void *) exc_handler);
	assert(rtlib->EnableEXC);
	RTLIB_ExitCode_t result = rtlib->EnableEXC(exc_handler);

	if (result != RTLIB_OK) {
		logger->Error("Enabling EXC [%s] (@%p) FAILED",
					  exc_name.c_str(), (void *) exc_handler);
		return result;
	}

	// Mark the is_enabled flag for the Control Loop's sake
	exc_status.is_enabled = true;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Enable()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);

	// Cannot enable an EXC if it has not already started
	if (! exc_status.has_started)
		return RTLIB_EXC_NOT_STARTED;

	return _Enable();
}

RTLIB_ExitCode_t BbqueEXC::Disable()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	RTLIB_ExitCode_t result;
	assert(exc_status.is_registered == true);

	// Check if the EXC is already disable (which, btw, shouldn't happen)
	if (! exc_status.is_enabled)
		return RTLIB_OK;

	logger->Info("Disabling control loop for EXC [%s] (@%p)...",
				 exc_name.c_str(), (void *) exc_handler);
	// Disable the EXC
	logger->Info("Disabling EXC [%s] (@%p)...",
				 exc_name.c_str(), (void *) exc_handler);
	assert(rtlib->Disable);
	result = rtlib->Disable(exc_handler);

	// Notify the control-thread we are STOPPED
	exc_status.is_enabled = false;
	control_cond_variable.notify_all();

	return result;
}

RTLIB_ExitCode_t BbqueEXC::Start()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	RTLIB_ExitCode_t result;
	assert(exc_status.is_registered == true);
	// Enable the working mode to get resources
	result = _Enable();

	if (result != RTLIB_OK)
		return result;

	// Notify the control-thread we are STARTED
	exc_status.has_started = true;
	control_cond_variable.notify_all();
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Terminate()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);

	// Check if we are already terminating
	if (! exc_status.is_registered)
		return RTLIB_OK;

	// Unregister the EXC
	logger->Info("Unregistering EXC [%s] (@%p)...",
				 exc_name.c_str(), (void *) exc_handler);
	assert(rtlib->Unregister);
	rtlib->Unregister(exc_handler);

	// Check if the control loop has already terminated
	if (exc_status.has_finished_processing) {
		// Notify the WaitCompletion before exiting
		control_thread.join();
		//waitid(P_PID, control_thread, &infop, WEXITED);
		// Joining the terminated thread (for a clean exit)
		control_cond_variable.notify_all();
		return RTLIB_OK;
	}

	logger->Info("Terminating control loop for EXC [%s] (@%p)...",
				 exc_name.c_str(), (void *) exc_handler);
	// Notify the control thread we are done
	exc_status.has_finished_processing = true;
	control_cond_variable.notify_all();
	control_u_lock.unlock();
	// Wait for the control thread to finish
	control_thread.join();
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::WaitCompletion()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	logger->Info("Waiting for EXC [%s] control loop termination...",
				 exc_name.c_str());

	while (! exc_status.is_terminated)
		control_cond_variable.wait(control_u_lock);

	return RTLIB_OK;
}

/*******************************************************************************
 *    Default Events Handler
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::onSetup()
{
	logger->Warn("<< Default setup of EXC [%s]  >>", exc_name.c_str());
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onConfigure(int8_t awm_id)
{
	logger->Warn("<< Default switching of EXC [%s] into AWM [%d], latency 10[ms] >>",
				 exc_name.c_str(), awm_id);
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onSuspend()
{
	logger->Warn("<< Default suspending of EXC [%s], latency 10[ms] >>",
				 exc_name.c_str());
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onResume()
{
	logger->Debug("<< Default resume of EXC [%s], latency 10[ms] >>",
				  exc_name.c_str());
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onRun()
{
	// By default return after a pre-defined number of cycles
	if (cycles_count >= BBQUE_RTLIB_DEFAULT_CYCLES)
		return RTLIB_EXC_WORKLOAD_NONE;

	logger->Warn("<< Default onRun: EXC [%s], AWM[%02d], cycle [%d/%d], latency %d[ms] >>",
				 exc_name.c_str(), wmp.awm_id, cycles_count + 1,
				 BBQUE_RTLIB_DEFAULT_CYCLES, 100 * (wmp.awm_id + 1));
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onMonitor()
{
	logger->Warn("<< Default monitoring of EXC [%s], latency 1[ms] >>",
				 exc_name.c_str());
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onRelease()
{
	logger->Warn("<< Default release of EXC [%s]  >>", exc_name.c_str());
	return RTLIB_OK;
}

/*******************************************************************************
 *    Utility Functions Support
 ******************************************************************************/

const char * BbqueEXC::GetUniqueID_String() const
{
	return rtlib->Utils.GetUniqueID_String();
}

RTLIB_ExitCode_t BbqueEXC::GetAssignedResources(
	RTLIB_ResourceType_t r_type,
	int32_t & r_amount)
{
	return rtlib->Utils.GetResources(exc_handler, &wmp, r_type, r_amount);
}

RTLIB_ExitCode_t BbqueEXC::GetAffinityMask(
	int32_t * ids_vector,
        int vector_size)
{
	return rtlib->Utils.GetAffinityMask(exc_handler, ids_vector,
                vector_size);
}

RTLIB_ExitCode_t BbqueEXC::GetAssignedResources(
	RTLIB_ResourceType_t r_type,
	int32_t * sys_array,
	uint16_t array_size)
{
	return rtlib->Utils.GetResourcesArray(exc_handler, &wmp, r_type, sys_array,
										  array_size);
}

float BbqueEXC::GetTimingMs(RTLIB_ExecPhaseType_t aem_phase,
			RTLIB_ExecTimingType_t timing_type) {
	return rtlib->CPS.GetTimingMs(exc_handler, aem_phase, timing_type);
}

/*******************************************************************************
 *    Cycles Per Second (CPS) and Jobs Per Second (JPS) Control Support
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::SetCPS(float cps)
{
	logger->Debug("Set cycles-rate to [%.3f] [Hz] for EXC [%s] (@%p)...",
				  cps, exc_name.c_str(), (void *) exc_handler);
	return rtlib->CPS.Set(exc_handler, cps);
}

RTLIB_ExitCode_t BbqueEXC::SetJPSGoal(float jps_min, float jps_max, int jpc)
{
	logger->Debug("Set jobs-rate goal to [%.3f - %.3f] [Hz] for EXC [%s] (@%p)...",
				  jps_min, jps_max, exc_name.c_str(), (void *) exc_handler);
	return rtlib->JPS.SetGoal(exc_handler, jps_min, jps_max, jpc);
}

RTLIB_ExitCode_t BbqueEXC::SetCPSGoal(float cps_min, float cps_max)
{
	if (cps_min < 0.0f) {
		logger->Error("SetCPSGoal FAILED (min CPS must be >= 0)");
		return RTLIB_ERROR;
	}

	if (cps_min > cps_max) {
		logger->Error("SetCPSGoal FAILED (min CPS must be <= max_cps)");
		return RTLIB_ERROR;
	}

	logger->Debug("Set cycles-rate goal to [%.3f - %.3f] [Hz] for EXC [%s] (@%p)...",
				  cps_min, cps_max, exc_name.c_str(), (void *) exc_handler);
	return rtlib->CPS.SetGoal(exc_handler, cps_min, cps_max);
}

RTLIB_ExitCode_t BbqueEXC::SetMinimumCycleTimeUs(uint32_t min_cycle_time_us)
{
	logger->Debug("Set cycles-time to [%" PRIu32 "] [us] for EXC [%s] (@%p)...",
				  min_cycle_time_us, exc_name.c_str(), (void *) exc_handler);
	return rtlib->CPS.SetMinCycleTime_us(exc_handler, min_cycle_time_us);
}

RTLIB_ExitCode_t BbqueEXC::UpdateJPC(int jpc)
{
	return rtlib->JPS.UpdateJPC(exc_handler, jpc);
}

float BbqueEXC::GetCPS()
{
	return rtlib->CPS.Get(exc_handler);
}

float BbqueEXC::GetJPS()
{
	return rtlib->JPS.Get(exc_handler);
}

/*******************************************************************************
 *    Constraints Management
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::SetAWMConstraints(
	RTLIB_Constraint_t * constraints,
	uint8_t count)
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	RTLIB_ExitCode_t result = RTLIB_OK;
	assert(exc_status.is_registered == true);
	assert(rtlib->SetAWMConstraints);
	//--- Assert constraints on this EXC
	logger->Info("Set [%d] constraints for EXC [%s] (@%p)...",
				 count, exc_name.c_str(), (void *) exc_handler);
	result = rtlib->SetAWMConstraints(exc_handler, constraints, count);
	return result;
}

RTLIB_ExitCode_t BbqueEXC::ClearAWMConstraints()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	RTLIB_ExitCode_t result = RTLIB_OK;
	assert(exc_status.is_registered == true);
	assert(rtlib->ClearAWMConstraints);
	//--- Clear constraints on this EXC
	logger->Info("Clear ALL constraints for EXC [%s] (@%p)...",
				 exc_name.c_str(), (void *) exc_handler);
	result = rtlib->ClearAWMConstraints(exc_handler);
	return result;
}

RTLIB_ExitCode_t BbqueEXC::SetGoalGap(int percent)
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	RTLIB_ExitCode_t result = RTLIB_OK;
	assert(exc_status.is_registered == true);
	assert(rtlib->SetAWMConstraints);
	//--- Assert a Goal-Gap on this EXC
	logger->Info("Set [%d] Goal-Gap for EXC [%s] (@%p)...",
				 percent, exc_name.c_str(), (void *) exc_handler);
	result = rtlib->SetGoalGap(exc_handler, percent);
	return result;
}

/*******************************************************************************
 *    Control Loop
 ******************************************************************************/

/** Wait for the EXC to be enabled by the resource manager*/
void BbqueEXC::WaitEnabling()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);

	while (! exc_status.is_enabled)
		control_cond_variable.wait(control_u_lock);
}

/** Get a new resource allocation for the EXC by the resource manager*/
RTLIB_ExitCode_t BbqueEXC::CheckConfigure()
{
	// Call the RPC pre-configuration procedure
	rtlib->Notify.PreConfigure(exc_handler);

	RTLIB_ExitCode_t checkconfigure_result =
		rtlib->GetWorkingMode(exc_handler, &wmp, RTLIB_SYNC_STATELESS);

	// Checking that the return code actually makes sense
	if (unlikely(checkconfigure_result != RTLIB_OK &&
		(checkconfigure_result < RTLIB_EXC_GWM_START ||
		 checkconfigure_result > RTLIB_EXC_GWM_BLOCKED))) {
	    logger->Debug("[%s] CheckConfigure: invalid return code for "
				"GetWorkingMode()", exc_name.c_str());
	    checkconfigure_result = RTLIB_EXC_GWM_FAILED;
	}

	switch (checkconfigure_result) {
	    case RTLIB_OK:
		logger->Debug("[%s] CheckConfigure: Allocation did not change",
							    exc_name.c_str());
		break;
	    case RTLIB_EXC_GWM_FAILED:
		break;
	    case RTLIB_EXC_GWM_BLOCKED:
		checkconfigure_result = Suspend();
		break;
	    default:
		if (exc_status.is_suspended ||
			    (checkconfigure_result == RTLIB_EXC_GWM_START)) {
		    // Call the user-defined resuming procedure
		    logger->Debug("[%s] CheckConfigure: Executing onResume()",
							    exc_name.c_str());
		    onResume();
		    // Set this EXC as NOT SUSPENDED
		    exc_status.is_suspended = false;
		}
		// Call the user-defined configuration procedure
		logger->Debug("[%s] CheckConfigure: Executing onConfigure()",
							    exc_name.c_str());
		checkconfigure_result = onConfigure(wmp.awm_id);
	}

	// Call the RPC post-configuration procedure
	rtlib->Notify.PostConfigure(exc_handler);

	return checkconfigure_result;
}

RTLIB_ExitCode_t BbqueEXC::Suspend()
{
	// Call the user-defined suspension procedure
	logger->Debug("[%s] Suspend: Executing onSuspend()", exc_name.c_str());
	onSuspend();
	// Set this EXC as SUSPENDED
	exc_status.is_suspended = true;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Setup()
{
	logger->Debug("[%s] Setup: Executing onSetup()", exc_name.c_str());
	RTLIB_ExitCode_t result = onSetup();

	if (result == RTLIB_OK) {
		logger->Debug("[%s] Setup: Checking if perf-counters "
					"must be monitored", exc_name.c_str());
		// Start monitoring performance counters
		assert(rtlib->Utils.MonitorPerfCounters);
		rtlib->Utils.MonitorPerfCounters(exc_handler);

		logger->Debug("[%s] Setup: Registering control thread PID",
					exc_name.c_str());
		assert(rtlib->RegisterCtrlThreadPID);
		result = rtlib->RegisterCtrlThreadPID(exc_handler);
	}

	return result;
}

RTLIB_ExitCode_t BbqueEXC::Run()
{
	// Call the RPC pre-execution procedure
	rtlib->Notify.PreRun(exc_handler);

	// Call the user-defined execution procedure
	RTLIB_ExitCode_t run_result = onRun();

	// Check if it was the last execution burst
	if (run_result == RTLIB_EXC_WORKLOAD_NONE)
		exc_status.has_finished_processing = true;

	// Call the RPC post-execution procedure
	rtlib->Notify.PostRun(exc_handler);

	return run_result;
}

RTLIB_ExitCode_t BbqueEXC::Monitor()
{
	// Call the RPC pre-monitor procedure
	rtlib->Notify.PreMonitor(exc_handler);

	// Call the user-defined monitor procedure
	RTLIB_ExitCode_t monitor_result = onMonitor();

	bool max_cycles_reached = config.duration.enabled &&
		cycles_count >= config.duration.max_cycles_before_termination;
	bool max_duration_reached = config.duration.enabled &&
		config.duration.time_limit &&
		config.duration.max_ms_before_termination == 0;

	// Check if it was the last execution burst
	if (monitor_result == RTLIB_EXC_WORKLOAD_NONE ||
		max_cycles_reached || max_duration_reached)
		exc_status.has_finished_processing = true;

	// Call the RPC post-monitor procedure
	rtlib->Notify.PostMonitor(exc_handler, exc_status.has_finished_processing);

	return monitor_result;
}

RTLIB_ExitCode_t BbqueEXC::Release()
{
	// Disable the EXC (thus notifying waiters)
	logger->Debug("[%s] Release: Disabling EXC", exc_name.c_str());
	Disable();

	logger->Debug("[%s] Release: Executing onRelease()", exc_name.c_str());
	return onRelease();
}

void BbqueEXC::WaitEXCInitCompletion()
{
	std::unique_lock<std::mutex> control_u_lock(control_mutex);
	assert(rtlib);
	assert(exc_status.is_registered == true);

	while (! exc_status.has_started)
		control_cond_variable.wait(control_u_lock);

	assert(exc_status.is_enabled == true);
}

void BbqueEXC::ControlLoop()
{
	// Set the thread name
	if (unlikely(prctl(PR_SET_NAME, (long unsigned int) "bq.cloop", 0, 0, 0)))
		logger->Error("Set name FAILED! (Error: %s)", strerror(errno));

	// Wait for EXC to be registered and enabled
	WaitEXCInitCompletion();

	RTLIB_ExitCode_t result;

	// Setup the EXC
	if (Setup() == RTLIB_OK) {

		// Endless loop
		while (! exc_status.has_finished_processing) {

			cycles_count ++;

			// Check if EXC is temporarily disabled
			WaitEnabling();

			if (! exc_status.has_finished_processing) {
				// Check for changes in resource allocation.
				// (Reconfigure or Suspend EXC if needed)
				if (CheckConfigure() != RTLIB_OK)
					continue;

				// Run the workload
				result = Run();

				if (result != RTLIB_OK &&
					result != RTLIB_EXC_WORKLOAD_NONE)
					continue;

				// Monitor Quality-of-Service
				result = Monitor();
				if (result != RTLIB_OK &&
					result != RTLIB_EXC_WORKLOAD_NONE)
					continue;
			}
		}
	} else {
		logger->Error("Setup EXC [%s] FAILED!", exc_name.c_str());
	}

	// Releasing all EXC resources
	Release();

	logger->Info("Control-loop for EXC [%s] TERMINATED", exc_name.c_str());

	// Exit notification
	rtlib->Notify.Exit(exc_handler);
	//--- Notify the control-thread is TERMINATED
	exc_status.is_terminated = true;
	// Wake up whoever is waiting for control loop termination
	control_cond_variable.notify_all();
}

} // namespace rtlib

} // namespace bbque
