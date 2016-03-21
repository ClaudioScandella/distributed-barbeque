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

#include "bbque/rtlib/bbque_exc.h"

#include "bbque/utils/utility.h"

#include <sys/prctl.h>
#include <string.h>
#include <assert.h>

#ifdef BBQUE_DEBUG
# warning Debugging is enabled
#endif

#ifndef BBQUE_RTLIB_DEFAULT_CYCLES
# define BBQUE_RTLIB_DEFAULT_CYCLES 8
#endif

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "exc"

namespace bbque { namespace rtlib {

BbqueEXC::BbqueEXC(std::string const & name,
		std::string const & recipe,
		RTLIB_Services_t * const rtl) :
	exc_name(name), rpc_name(recipe), rtlib(rtl),
	conf(*(rtlib->conf)), cycles_count(0),
	registered(false), started(false), enabled(false),
	suspended(false), done(false), terminated(false) {

	RTLIB_ExecutionContextParams_t exc_params = {
		{RTLIB_VERSION_MAJOR, RTLIB_VERSION_MINOR},
		RTLIB_LANG_CPP,
		recipe.c_str()
	};

	assert(rtlib);

	// Get a Logger module
	logger = bu::Logger::GetLogger(BBQUE_LOG_MODULE);

	logger->Info("Initializing a new EXC [%s]...", name.c_str());

	//--- Register
	assert(rtlib->Register);
	exc_hdl = rtlib->Register(name.c_str(), &exc_params);
	if (!exc_hdl) {
		logger->Error("Registering EXC [%s] FAILED", name.c_str());
		// TODO set initialization not completed
		return;
	}
	registered = true;

	//--- Keep track of our UID
	uid = rtlib->Utils.GetUid(exc_hdl);

	//--- Spawn the control thread
	ctrl_trd = std::thread(&BbqueEXC::ControlLoop, this);

}


BbqueEXC::~BbqueEXC() {
	//--- Disable the EXC and the Control-Loop
	Disable();
	//--- Unregister the EXC and Terminate the control loop thread
	Terminate();
}


/*******************************************************************************
 *    Execution Context Management
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::_Enable() {
	RTLIB_ExitCode_t result;

	assert(registered == true);

	if (enabled)
		return RTLIB_OK;

	logger->Info("Enabling EXC [%s] (@%p)...",
			exc_name.c_str(), (void*)exc_hdl);

	assert(rtlib->Enable);
	result = rtlib->Enable(exc_hdl);
	if (result != RTLIB_OK) {
		logger->Error("Enabling EXC [%s] (@%p) FAILED",
				exc_name.c_str(), (void*)exc_hdl);
		return result;
	}

	//--- Mark the control loop as Enabled
	// NOTE however, the thread should be started only with an actual call to
	// the Start() method
	enabled = true;

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Enable() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	if (!started)
		return RTLIB_EXC_NOT_STARTED;
	return _Enable();
}

RTLIB_ExitCode_t BbqueEXC::Disable() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	RTLIB_ExitCode_t result;

	assert(registered == true);

	if (!enabled)
		return RTLIB_OK;

	logger->Info("Disabling control loop for EXC [%s] (@%p)...",
			exc_name.c_str(), (void*)exc_hdl);

	//--- Notify the control-thread we are STOPPED
	enabled = false;
	ctrl_cv.notify_all();

	//--- Disable the EXC
	logger->Info("Disabling EXC [%s] (@%p)...",
			exc_name.c_str(), (void*)exc_hdl);

	assert(rtlib->Disable);
	result = rtlib->Disable(exc_hdl);

	return result;

}

RTLIB_ExitCode_t BbqueEXC::Start() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	RTLIB_ExitCode_t result;

	assert(registered == true);

	//--- Enable the working mode to get resources
	result = _Enable();
	if (result != RTLIB_OK)
		return result;

	//--- Notify the control-thread we are STARTED
	started = true;
	ctrl_cv.notify_all();

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Terminate() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);

	// Check if we are already terminating
	if (!registered)
		return RTLIB_OK;

	// Unregister the EXC
	logger->Info("Unregistering EXC [%s] (@%p)...",
			exc_name.c_str(), (void*)exc_hdl);

	assert(rtlib->Unregister);
	rtlib->Unregister(exc_hdl);

	// Check if the control loop has already terminated
	if (done) {
		// Notify the WaitCompletion before exiting
		ctrl_trd.join();
		//waitid(P_PID, ctrl_trd, &infop, WEXITED);
		// Joining the terminated thread (for a clean exit)
		ctrl_cv.notify_all();
		return RTLIB_OK;
	}

	logger->Info("Terminating control loop for EXC [%s] (@%p)...",
			exc_name.c_str(), (void*)exc_hdl);

	// Notify the control thread we are done
	done = true;
	ctrl_cv.notify_all();
	ctrl_ul.unlock();

	// Wait for the control thread to finish
	ctrl_trd.join();

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::WaitCompletion() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);

	logger->Info("Waiting for EXC [%s] control loop termination...",
			exc_name.c_str());

	while (!terminated)
		ctrl_cv.wait(ctrl_ul);

	return RTLIB_OK;
}

/*******************************************************************************
 *    Default Events Handler
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::onSetup() {

	DB(logger->Warn("<< Default setup of EXC [%s]  >>", exc_name.c_str()));
	DB(::usleep(10000));

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onConfigure(int8_t awm_id) {

	DB(logger->Warn("<< Default switching of EXC [%s] into AWM [%d], latency 10[ms] >>",
			exc_name.c_str(), awm_id));
	DB(::usleep(10000));

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onSuspend() {

	DB(logger->Warn("<< Default suspending of EXC [%s], latency 10[ms] >>",
			exc_name.c_str()));
	DB(::usleep(10000));

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onResume() {

	DB(logger->Warn("<< Default resume of EXC [%s], latency 10[ms] >>",
			exc_name.c_str()));
	DB(::usleep(10000));

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onRun() {

	// By default return after a pre-defined number of cycles
	if (cycles_count >= BBQUE_RTLIB_DEFAULT_CYCLES)
		return RTLIB_EXC_WORKLOAD_NONE;

	DB(logger->Warn("<< Default onRun: EXC [%s], AWM[%02d], cycle [%d/%d], latency %d[ms] >>",
				exc_name.c_str(), wmp.awm_id, cycles_count+1,
				BBQUE_RTLIB_DEFAULT_CYCLES, 100*(wmp.awm_id+1)));
	DB(::usleep((wmp.awm_id+1)*100000));

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onMonitor() {

	DB(logger->Warn("<< Default monitoring of EXC [%s], latency 1[ms] >>",
			exc_name.c_str()));
	DB(::usleep(1000));

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::onRelease() {

	DB(logger->Warn("<< Default release of EXC [%s]  >>", exc_name.c_str()));
	DB(::usleep(10000));

	return RTLIB_OK;
}

/*******************************************************************************
 *    Utility Functions Support
 ******************************************************************************/

const char *BbqueEXC::GetChUid() const {
	return rtlib->Utils.GetChUid();
}

RTLIB_ExitCode_t BbqueEXC::GetAssignedResources(
		RTLIB_ResourceType_t r_type,
		int32_t & r_amount) {
	return rtlib->Utils.GetResources(exc_hdl, &wmp, r_type, r_amount);
}

RTLIB_ExitCode_t BbqueEXC::GetAssignedResources(
        RTLIB_ResourceType_t r_type,
        int32_t * sys_array,
        uint16_t array_size) {
    return rtlib->Utils.GetResourcesArray(exc_hdl, &wmp, r_type, sys_array, array_size);
}

/*******************************************************************************
 *    Cycles Per Second (CPS) and Jobs Per Second (JPS) Control Support
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::SetCPS(float cps) {
	DB(logger->Debug("Set cycles-rate to [%.3f] [Hz] for EXC [%s] (@%p)...",
			cps, exc_name.c_str(), (void*)exc_hdl));
	return rtlib->CPS.Set(exc_hdl, cps);
}

RTLIB_ExitCode_t BbqueEXC::SetJPSGoal(float jps_min, float jps_max, int jpc) {
	DB(logger->Debug("Set jobs-rate goal to [%.3f - %.3f] [Hz] for EXC [%s] (@%p)...",
			jps_min, jps_max, exc_name.c_str(), (void*)exc_hdl));
	return rtlib->JPS.SetGoal(exc_hdl, jps_min, jps_max, jpc);
}

RTLIB_ExitCode_t BbqueEXC::SetCPSGoal(float cps_min, float cps_max) {
	DB(logger->Debug("Set cycles-rate goal to [%.3f - %.3f] [Hz] for EXC [%s] (@%p)...",
			cps_min, cps_max, exc_name.c_str(), (void*)exc_hdl));
	return rtlib->CPS.SetGoal(exc_hdl, cps_min, cps_max);
}

RTLIB_ExitCode_t BbqueEXC::SetCTimeUs(uint32_t us) {
	DB(logger->Debug("Set cycles-time to [%" PRIu32 "] [us] for EXC [%s] (@%p)...",
			us, exc_name.c_str(), (void*)exc_hdl));
	return rtlib->CPS.SetCTimeUs(exc_hdl, us);
}

RTLIB_ExitCode_t BbqueEXC::UpdateJPC(int jpc) {
	DB(fprintf(stderr, FD("Updating JPC [%d] [Hz] for EXC [%s] (@%p)...\n"),
			jpc, exc_name.c_str(), (void*)exc_hdl));
	return rtlib->JPS.UpdateJPC(exc_hdl, jpc);
}

float BbqueEXC::GetCPS() {
	float cps = rtlib->CPS.Get(exc_hdl);
	DB(fprintf(stderr, FD("Get cycles-rate [%.3f] [Hz] for EXC [%s] (@%p)...\n"),
			cps, exc_name.c_str(), (void*)exc_hdl));
	return cps;
}

float BbqueEXC::GetJPS() {
	float jps = rtlib->JPS.Get(exc_hdl);
	DB(fprintf(stderr, FD("Get jobs-rate [%.3f] [Hz] for EXC [%s] (@%p)...\n"),
			jps, exc_name.c_str(), (void*)exc_hdl));
	return jps;
}

/*******************************************************************************
 *    Constraints Management
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::SetConstraints(
		RTLIB_Constraint_t *constraints,
		uint8_t count) {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	RTLIB_ExitCode_t result = RTLIB_OK;

	assert(registered == true);
	assert(rtlib->SetConstraints);

	//--- Assert constraints on this EXC
	logger->Info("Set [%d] constraints for EXC [%s] (@%p)...",
			count, exc_name.c_str(), (void*)exc_hdl);
	result = rtlib->SetConstraints(exc_hdl, constraints, count);

	return result;
}

RTLIB_ExitCode_t BbqueEXC::ClearConstraints() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	RTLIB_ExitCode_t result = RTLIB_OK;

	assert(registered == true);
	assert(rtlib->ClearConstraints);

	//--- Clear constraints on this EXC
	logger->Info("Clear ALL constraints for EXC [%s] (@%p)...",
			exc_name.c_str(), (void*)exc_hdl);
	result = rtlib->ClearConstraints(exc_hdl);

	return result;
}

RTLIB_ExitCode_t BbqueEXC::SetGoalGap(int percent) {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	RTLIB_ExitCode_t result = RTLIB_OK;

	assert(registered == true);
	assert(rtlib->SetConstraints);

	//--- Assert a Goal-Gap on this EXC
	logger->Info("Set [%d] Goal-Gap for EXC [%s] (@%p)...",
			percent, exc_name.c_str(), (void*)exc_hdl);
	result = rtlib->SetGoalGap(exc_hdl, percent);

	return result;
}

/*******************************************************************************
 *    Control Loop
 ******************************************************************************/

RTLIB_ExitCode_t BbqueEXC::Setup() {
	RTLIB_ExitCode_t result;

	DB(logger->Debug("CL 0. Setup EXC [%s]...", exc_name.c_str()));

	result = onSetup();
	return result;
}

bool BbqueEXC::WaitEnable() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);

	while (!done && !enabled)
		ctrl_cv.wait(ctrl_ul);

	return done;
}

RTLIB_ExitCode_t BbqueEXC::GetWorkingMode() {
	RTLIB_ExitCode_t result;

	DB(logger->Debug("CL 1. Get AWM for EXC [%s]...", exc_name.c_str()));

	assert(rtlib->GetWorkingMode);
	result = rtlib->GetWorkingMode(exc_hdl, &wmp, RTLIB_SYNC_STATELESS);

	return result;
}

RTLIB_ExitCode_t BbqueEXC::Configure(int8_t awm_id, RTLIB_ExitCode_t event) {

	if (suspended || (event == RTLIB_EXC_GWM_START)) {
		// Call the user-defined resuming procedure
		rtlib->Notify.PreResume(exc_hdl);
		onResume();
		rtlib->Notify.PostResume(exc_hdl);

		// Set this EXC as NOT SUSPENDED
		suspended = false;
	}

	// Call the user-defined configuration procedure
	rtlib->Notify.PreConfigure(exc_hdl);
	onConfigure(awm_id);
	rtlib->Notify.PostConfigure(exc_hdl);

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Suspend() {

	// Call the user-defined suspension procedure
	rtlib->Notify.PreSuspend(exc_hdl);
	onSuspend();
	rtlib->Notify.PostSuspend(exc_hdl);

	// Set this EXC as SUSPENDED
	suspended = true;

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueEXC::Reconfigure(RTLIB_ExitCode_t result) {

	DB(logger->Debug("CL 2. Reconfigure check for EXC [%s]...",
			exc_name.c_str()));

	switch (result) {
	case RTLIB_OK:
		DB(logger->Debug("CL 2-1. Continue to run on the assigned AWM [%d] for EXC [%s]",
				wmp.awm_id, exc_name.c_str()));
		return result;

	case RTLIB_EXC_GWM_START:
	case RTLIB_EXC_GWM_RECONF:
	case RTLIB_EXC_GWM_MIGREC:
	case RTLIB_EXC_GWM_MIGRATE:
		DB(logger->Debug("CL 2-2. Switching EXC [%s] to AWM [%02d]...",
				exc_name.c_str(), wmp.awm_id));
		Configure(wmp.awm_id, result);
		return result;

	case RTLIB_EXC_GWM_BLOCKED:
		DB(logger->Debug("CL 2-3. Suspending EXC [%s]...",
				exc_name.c_str()));
		Suspend();
		return result;

	default:
		DB(logger->Error("GetWorkingMode for EXC [%s] FAILED (Error: Invalid event [%d])",
					exc_name.c_str(), result));
		assert(result >= RTLIB_EXC_GWM_START);
		assert(result <= RTLIB_EXC_GWM_BLOCKED);
	}

	return RTLIB_EXC_GWM_FAILED;
}


RTLIB_ExitCode_t BbqueEXC::Run() {
	RTLIB_ExitCode_t result;

	DB(logger->Debug("CL 3. Run EXC [%s], cycle [%010d], AWM[%02d]...",
			exc_name.c_str(), cycles_count+1, wmp.awm_id));

	rtlib->Notify.PreRun(exc_hdl);

	result = onRun();
	if (result == RTLIB_EXC_WORKLOAD_NONE) {
		done = true;
	} else {
		rtlib->Notify.PostRun(exc_hdl);
	}

	return result;
}

RTLIB_ExitCode_t BbqueEXC::Monitor() {
	RTLIB_ExitCode_t result;

	// Account executed cycles
	cycles_count++;

	DB(logger->Debug("CL 4. Monitor EXC [%s]...", exc_name.c_str()));

	rtlib->Notify.PreMonitor(exc_hdl);
	result = onMonitor();
	rtlib->Notify.PostMonitor(exc_hdl);
	if (result == RTLIB_EXC_WORKLOAD_NONE)
		done = true;

	if (likely(!conf.duration.enabled))
		return result;

	// Duration control checks
	if (conf.duration.time_limit) {
		if (conf.duration.millis != 0)
			return result;
	} else {
		if (conf.duration.cycles > cycles_count)
			return result;
	}

	done = true;
	logger->Warn("Application termination due to DURATION ENFORCING");
	return RTLIB_EXC_WORKLOAD_NONE;
}

RTLIB_ExitCode_t BbqueEXC::Release() {
	RTLIB_ExitCode_t result;

	DB(logger->Debug("CL 5. Release EXC [%s]...", exc_name.c_str()));

	result = onRelease();
	return result;
}

void BbqueEXC::ControlLoop() {
	std::unique_lock<std::mutex> ctrl_ul(ctrl_mtx);
	RTLIB_ExitCode_t result;

	assert(rtlib);
	assert(registered == true);

	// Set the thread name
	if (unlikely(prctl(PR_SET_NAME, (long unsigned int)"bq.cloop", 0, 0, 0)))
		logger->Error("Set name FAILED! (Error: %s)", strerror(errno));

	// Wait for the EXC being STARTED
	while (!started)
		ctrl_cv.wait(ctrl_ul);
	ctrl_ul.unlock();

	DB(logger->Debug("EXC [%s] control thread [%d] started...",
				exc_name.c_str(), gettid()));

	assert(enabled == true);

	// Setup notification
	rtlib->Notify.Setup(exc_hdl);

	// Setup the EXC
	if (Setup() != RTLIB_OK) {
		logger->Error("Setup EXC [%s] FAILED!", exc_name.c_str());
		goto exit_setup;
	}

	// Initialize notification
	rtlib->Notify.Init(exc_hdl);

	// Endless loop
	while (!done) {

		// Wait for the EXC being enabled
		if (WaitEnable())
			break;

		// Get the assigned AWM
		result = GetWorkingMode();
		if (result == RTLIB_EXC_GWM_FAILED)
			continue;

		// Check for reconfiguration required
		result = Reconfigure(result);
		if (result != RTLIB_OK)
			continue;

		// Run the workload
		result = Run();
		if (done || result != RTLIB_OK)
			continue;

		// Monitor Quality-of-Services
		result = Monitor();
		if (done || result != RTLIB_OK)
			continue;

	};

exit_setup:

	// Disable the EXC (thus notifying waiters)
	Disable();

	// Releasing all EXC resources
	Release();

	// Exit notification
	rtlib->Notify.Exit(exc_hdl);

	DB(logger->Info("Control-loop for EXC [%s] TERMINATED", exc_name.c_str()));

	//--- Notify the control-thread is TERMINATED
	ctrl_ul.lock();
	terminated = true;
	ctrl_cv.notify_all();

}

} // namespace rtlib

} // namespace bbque
