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

#include "bbque/synchronization_manager.h"

#include "bbque/application_manager.h"
#include "bbque/configuration_manager.h"
#include "bbque/modules_factory.h"
#include "bbque/plugin_manager.h"
#include "bbque/resource_accounter.h"
#include "bbque/system.h"

#include "bbque/app/application.h"
#include "bbque/app/working_mode.h"

#include "bbque/utils/utility.h"

// The prefix for configuration file attributes
#define MODULE_CONFIG "SynchronizationManager"

/** Metrics (class COUNTER) declaration */
#define SM_COUNTER_METRIC(NAME, DESC)\
 {SYNCHRONIZATION_MANAGER_NAMESPACE "." NAME, DESC, \
	 MetricsCollector::COUNTER, 0, NULL, 0}
/** Increase counter for the specified metric */
#define SM_COUNT_EVENT(METRICS, INDEX) \
	mc.Count(METRICS[INDEX].mh);
/** Increase counter for the specified metric */
#define SM_COUNT_EVENT2(METRICS, INDEX, AMOUNT) \
	mc.Count(METRICS[INDEX].mh, AMOUNT);

/** Metrics (class SAMPLE) declaration */
#define SM_SAMPLE_METRIC(NAME, DESC)\
 {SYNCHRONIZATION_MANAGER_NAMESPACE "." NAME, DESC, \
	 MetricsCollector::SAMPLE, 0, NULL, 0}
/** Reset the timer used to evaluate metrics */
#define SM_RESET_TIMING(TIMER) \
	TIMER.start();
/** Acquire a new completion time sample */
#define SM_GET_TIMING(METRICS, INDEX, TIMER) \
	mc.AddSample(METRICS[INDEX].mh, TIMER.getElapsedTimeMs());
/** Acquire a new EXC reconfigured sample */
#define SM_ADD_SAMPLE(METRICS, INDEX, COUNT) \
	mc.AddSample(METRICS[INDEX].mh, COUNT);

/** SyncState Metrics (class SAMPLE) declaration */
#define SM_SAMPLE_METRIC_SYNCSTATE(NAME, DESC)\
 {SYNCHRONIZATION_MANAGER_NAMESPACE "." NAME, DESC, MetricsCollector::SAMPLE, \
	 bbque::app::ApplicationStatusIF::SYNC_STATE_COUNT, \
	 bbque::app::ApplicationStatusIF::syncStateStr, 0}
/** Acquire a new completion time sample for SyncState Metrics*/
#define SM_GET_TIMING_SYNCSTATE(METRICS, INDEX, TIMER, STATE) \
	mc.AddSample(METRICS[INDEX].mh, TIMER.getElapsedTimeMs(), STATE);

namespace bu = bbque::utils;
namespace bp = bbque::plugins;
namespace po = boost::program_options;
namespace br = bbque::res;

namespace bbque {

/* Definition of metrics used by this module */
MetricsCollector::MetricsCollection_t
SynchronizationManager::metrics[SM_METRICS_COUNT] = {
	//----- Event counting metrics
	SM_COUNTER_METRIC("runs", "SyncP executions count"),
	SM_COUNTER_METRIC("comp", "SyncP completion count"),
	SM_COUNTER_METRIC("excs", "Total EXC reconf count"),
	SM_COUNTER_METRIC("sync_hit",  "Syncs HIT count"),
	SM_COUNTER_METRIC("sync_miss", "Syncs MISS count"),
	//----- Timing metrics
	SM_SAMPLE_METRIC("sp.a.time",  "Avg SyncP execution t[ms]"),
	SM_SAMPLE_METRIC("sp.a.lat",   " Pre-Sync Lat   t[ms]"),
	SM_SAMPLE_METRIC_SYNCSTATE("sp.a.pre",   " PreChange  exe t[ms]"),
	SM_SAMPLE_METRIC_SYNCSTATE("sp.a.sync",  " SyncChange exe t[ms]"),
	SM_SAMPLE_METRIC_SYNCSTATE("sp.a.synp",  " SyncPlatform exe t[ms]"),
	SM_SAMPLE_METRIC_SYNCSTATE("sp.a.do",    " DoChange   exe t[ms]"),
	SM_SAMPLE_METRIC_SYNCSTATE("sp.a.post",  " PostChange exe t[ms]"),
	//----- Couting statistics
	SM_SAMPLE_METRIC("avge", "Average EXCs reconf"),
	SM_SAMPLE_METRIC("app.SyncLat", "Average SyncLatency declared"),

};

SynchronizationManager & SynchronizationManager::GetInstance() {
	static SynchronizationManager ym;
	return ym;
}

SynchronizationManager::SynchronizationManager() :
	am(ApplicationManager::GetInstance()),
	ap(ApplicationProxy::GetInstance()),
	mc(bu::MetricsCollector::GetInstance()),
	ra(ResourceAccounter::GetInstance()),
	plm(PlatformManager::GetInstance()),
	prm(ProcessManager::GetInstance()),
	sv(System::GetInstance()),
	sync_count(0) {
	std::string sync_policy;

	//---------- Get a logger module
	logger = bu::Logger::GetLogger(SYNCHRONIZATION_MANAGER_NAMESPACE);
	assert(logger);

	logger->Debug("Starting synchronization manager...");

	//---------- Loading module configuration
	ConfigurationManager & cm = ConfigurationManager::GetInstance();
	po::options_description opts_desc("Synchronization Manager Options");
	opts_desc.add_options()
		(MODULE_CONFIG".policy",
		 po::value<std::string>
		 (&sync_policy)->default_value(
			 BBQUE_DEFAULT_SYNCHRONIZATION_MANAGER_POLICY),
		 "The name of the optimization policy to use")
		;
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	//---------- Load the required optimization plugin
	std::string sync_namespace(SYNCHRONIZATION_POLICY_NAMESPACE".");
	logger->Debug("Loading synchronization policy [%s%s]...",
			sync_namespace.c_str(), sync_policy.c_str());
	policy = ModulesFactory::GetModule<bp::SynchronizationPolicyIF>(
			sync_namespace + sync_policy);
	if (!policy) {
		logger->Fatal("Synchronization policy load FAILED "
			"(Error: missing plugin for [%s%s])",
			sync_namespace.c_str(), sync_policy.c_str());
		assert(policy);
	}

	//---------- Setup all the module metrics
	mc.Register(metrics, SM_METRICS_COUNT);

}

SynchronizationManager::~SynchronizationManager() {
}

bool
SynchronizationManager::Reshuffling(AppPtr_t papp) {
	if ((papp->SyncState() == ApplicationStatusIF::RECONF))
		return !(papp->SwitchingAWM());
	return false;
}

SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_PreChange(ApplicationStatusIF::SyncState_t syncState) {
	ExitCode_t syncInProgress = NOTHING_TO_SYNC;
	AppsUidMapIt apps_it;

	typedef std::map<AppPtr_t, ApplicationProxy::pPreChangeRsp_t> RspMap_t;
	typedef std::pair<AppPtr_t, ApplicationProxy::pPreChangeRsp_t> RspMapEntry_t;

	ApplicationProxy::pPreChangeRsp_t presp;
	RspMap_t::iterator resp_it;
	RTLIB_ExitCode_t result;
	RspMap_t rsp_map;
	AppPtr_t papp;

	logger->Debug("STEP 1: preChange() START");
	SM_RESET_TIMING(sm_tmr);

	papp = am.GetFirst(syncState, apps_it);
	for ( ; papp; papp = am.GetNext(syncState, apps_it)) {

		if (!policy->DoSync(papp))
			continue;

		if (Reshuffling(papp) ||
			papp->IsContainer()) {
			syncInProgress = OK;
			continue;
		}

		logger->Info("STEP 1: preChange() ===> [%s]", papp->StrId());

		// Jumping meanwhile disabled applications
		if (papp->Disabled()) {
			logger->Debug("STEP 1: ignoring disabled EXC [%s]",
					papp->StrId());
			continue;
		}

		// Pre-Change (just starting it if asynchronous)
		presp = ApplicationProxy::pPreChangeRsp_t(
				new ApplicationProxy::preChangeRsp_t());
		result = ap.SyncP_PreChange(papp, presp);
		if (result != RTLIB_OK)
			continue;

		// This flag is set if there is at least one sync pending
		syncInProgress = OK;


// Pre-Change completion (just if asynchronous)
#ifdef CONFIG_BBQUE_YP_SASB_ASYNC
		// Mapping the response future for responses collection
		rsp_map.insert(RspMapEntry_t(papp, presp));

	}

	// Collecting EXC responses
	for (resp_it = rsp_map.begin();
			resp_it != rsp_map.end();
			++resp_it) {

		papp  = (*resp_it).first;
		presp = (*resp_it).second;
#endif

		Sync_PreChange_Check_EXC_Response(papp, presp);

// Pre-Change completion (just if asynchronous)
#ifdef CONFIG_BBQUE_YP_SASB_ASYNC
		// Remove the respose future
		rsp_map.erase(resp_it);
#endif // CONFIG_BBQUE_YP_SASB_ASYNC

		// This is required just for clean compilation
		continue;

	}

	// Collecing execution metrics
	SM_GET_TIMING_SYNCSTATE(metrics, SM_SYNCP_TIME_PRECHANGE,
			sm_tmr, syncState);
	logger->Debug("STEP 1: preChange() DONE");

	if (syncInProgress == NOTHING_TO_SYNC)
		return NOTHING_TO_SYNC;

	return OK;
}

void SynchronizationManager::Sync_PreChange_Check_EXC_Response(
		AppPtr_t papp,
		ApplicationProxy::pPreChangeRsp_t presp) const {

	SynchronizationPolicyIF::ExitCode_t syncp_result;

	// Jumping meanwhile disabled applications
	if (papp->Disabled()) {
		logger->Debug("STEP 1: ignoring disabled EXC [%s]",
				papp->StrId());
		return;
	}

// Pre-Change completion (just if asynchronous)
#ifdef CONFIG_BBQUE_YP_SASB_ASYNC
	RTLIB_ExitCode_t result;
	logger->Debug("STEP 1: .... (wait) .... [%s]", papp->StrId());
	result = ap.SyncP_PreChange_GetResult(presp);


	if (result == RTLIB_BBQUE_CHANNEL_TIMEOUT) {
		logger->Warn("STEP 1: <---- TIMEOUT -- [%s]",
				papp->StrId());
		// Disabling not responding applications
		am.DisableEXC(papp, true);
		return;
	}

	if (result == RTLIB_BBQUE_CHANNEL_WRITE_FAILED) {
		logger->Warn("STEP 1: <------ WERROR -- [%s]",
				papp->StrId());
		am.DisableEXC(papp, true);
		return;
	}

	if (result != RTLIB_OK) {
		logger->Warn("STEP 1: <----- FAILED -- [%s]", papp->StrId());
		// FIXME This case should be handled
		assert(false);
	}

#endif

	logger->Info("STEP 1: <--------- OK -- [%s]", papp->StrId());
	logger->Info("STEP 1: [%s] declared syncLatency %d[ms]",
			papp->StrId(), presp->syncLatency);

	// Collect stats on declared sync latency
	SM_ADD_SAMPLE(metrics, SM_SYNCP_APP_SYNCLAT, presp->syncLatency);

	syncp_result = policy->CheckLatency(papp, presp->syncLatency);
    UNUSED(syncp_result); // TODO: check the POLICY required action

}

SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_SyncChange(
		ApplicationStatusIF::SyncState_t syncState) {
	AppsUidMapIt apps_it;

	typedef std::map<AppPtr_t, ApplicationProxy::pSyncChangeRsp_t> RspMap_t;
	typedef std::pair<AppPtr_t, ApplicationProxy::pSyncChangeRsp_t> RspMapEntry_t;

	ApplicationProxy::pSyncChangeRsp_t presp;
	RspMap_t::iterator resp_it;
	RTLIB_ExitCode_t result;
	RspMap_t rsp_map;
	AppPtr_t papp;

	logger->Debug("STEP 2: syncChange() START");
	SM_RESET_TIMING(sm_tmr);

	papp = am.GetFirst(syncState, apps_it);
	for ( ; papp; papp = am.GetNext(syncState, apps_it)) {

		if (!policy->DoSync(papp))
			continue;

		if (Reshuffling(papp) ||
			papp->IsContainer())
			continue;

		logger->Info("STEP 2: syncChange() ===> [%s]", papp->StrId());

		// Jumping meanwhile disabled applications
		if (papp->Disabled()) {
			logger->Debug("STEP 2: ignoring disabled EXC [%s]",
					papp->StrId());
			continue;
		}

		// Sync-Change (just starting it if asynchronous)
		presp = ApplicationProxy::pSyncChangeRsp_t(
				new ApplicationProxy::syncChangeRsp_t());
		result = ap.SyncP_SyncChange(papp, presp);
		if (result != RTLIB_OK)
			continue;

// Sync-Change completion (just if asynchronous)
#ifdef CONFIG_BBQUE_YP_SASB_ASYNC
		// Mapping the response future for responses collection
		rsp_map.insert(RspMapEntry_t(papp, presp));

	}

	// Collecting EXC responses
	resp_it = rsp_map.begin();
	for (; resp_it != rsp_map.end(); ++resp_it) {
		papp  = (*resp_it).first;
		presp = (*resp_it).second;
#endif

	Sync_SyncChange_Check_EXC_Response(papp, presp);

// Sync-Change completion (just if asynchronous)
#ifdef CONFIG_BBQUE_YP_SASB_ASYNC
		// Remove the respose future
		rsp_map.erase(resp_it);
#endif // CONFIG_BBQUE_YP_SASB_ASYNC

		// This is required just for clean compilation
		continue;

	}

	// Collecing execution metrics
	SM_GET_TIMING_SYNCSTATE(metrics, SM_SYNCP_TIME_SYNCCHANGE,
			sm_tmr, syncState);
	logger->Debug("STEP 2: syncChange() DONE");

	return OK;
}

void SynchronizationManager::Sync_SyncChange_Check_EXC_Response(AppPtr_t papp,
								ApplicationProxy::pSyncChangeRsp_t presp) const {

	// Jumping meanwhile disabled applications
	if (papp->Disabled()) {
		logger->Debug("STEP 2: ignoring disabled EXC [%s]",
				papp->StrId());
		return;
	}

// Sync-Change completion (just if asynchronous)
#ifdef CONFIG_BBQUE_YP_SASB_ASYNC
	RTLIB_ExitCode_t result;

	logger->Debug("STEP 2: .... (wait) .... [%s]", papp->StrId());
	result = ap.SyncP_SyncChange_GetResult(presp);


	if (result == RTLIB_BBQUE_CHANNEL_TIMEOUT) {
		logger->Warn("STEP 2: <---- TIMEOUT -- [%s]",
				papp->StrId());
		// Disabling not responding applications
		am.DisableEXC(papp, true);
		// Accounting for syncpoints missed
		SM_COUNT_EVENT(metrics, SM_SYNCP_SYNC_MISS);
		return;
	}

	if (result == RTLIB_BBQUE_CHANNEL_WRITE_FAILED) {
		logger->Warn("STEP 1: <------ WERROR -- [%s]",
				papp->StrId());
		// Disabling not responding applications
		am.DisableEXC(papp, true);
		// Accounting for syncpoints missed
		SM_COUNT_EVENT(metrics, SM_SYNCP_SYNC_MISS);
		return;
	}


	if (result != RTLIB_OK) {
		logger->Warn("STEP 2: <----- FAILED -- [%s]", papp->StrId());
		// TODO Here the synchronization policy should be queryed to
		// decide if the synchronization latency is compliant with the
		// RTRM optimization goals.
		//
		DB(logger->Warn("TODO: Check sync policy for sync miss reaction"));

		// FIXME This case should be handled
		assert(false);
	}
#endif
	// Accounting for syncpoints missed
	SM_COUNT_EVENT(metrics, SM_SYNCP_SYNC_HIT);

	logger->Info("STEP 2: <--------- OK -- [%s]", papp->StrId());

}

SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_DoChange(ApplicationStatusIF::SyncState_t syncState) {
	AppsUidMapIt apps_it;

	RTLIB_ExitCode_t result;
	AppPtr_t papp;

	logger->Debug("STEP 3: doChange() START");
	SM_RESET_TIMING(sm_tmr);

	papp = am.GetFirst(syncState, apps_it);
	for ( ; papp; papp = am.GetNext(syncState, apps_it)) {

		if (!policy->DoSync(papp))
			continue;

		if (Reshuffling(papp) ||
			papp->IsContainer())
			continue;

		logger->Info("STEP 3: doChange() ===> [%s]", papp->StrId());

		// Jumping meanwhile disabled applications
		if (papp->Disabled()) {
			logger->Debug("STEP 3: ignoring disabled EXC [%s]",
					papp->StrId());
			continue;
		}

		// Send a Do-Change
		result = ap.SyncP_DoChange(papp);
		if (result != RTLIB_OK)
			continue;

		logger->Info("STEP 3: <--------- OK -- [%s]", papp->StrId());
	}

	// Collecing execution metrics
	SM_GET_TIMING_SYNCSTATE(metrics, SM_SYNCP_TIME_DOCHANGE,
			sm_tmr, syncState);
	logger->Debug("STEP 3: doChange() DONE");

	return OK;
}

SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_PostChange(ApplicationStatusIF::SyncState_t syncState) {
	ApplicationProxy::pPostChangeRsp_t presp;
	AppsUidMapIt apps_it;
	AppPtr_t papp;
	uint8_t excs = 0;

	logger->Debug("STEP 4: postChange() START");
	SM_RESET_TIMING(sm_tmr);

	papp = am.GetFirst(syncState, apps_it);
	for ( ; papp; papp = am.GetNext(syncState, apps_it)) {

		if (!policy->DoSync(papp))
			continue;

		if (! (Reshuffling(papp) || papp->IsContainer()) ) {

			logger->Info("STEP 4: postChange() ===> [%s]", papp->StrId());

			// Jumping meanwhile disabled applications
			if (papp->Disabled()) {
				logger->Debug("STEP 4: ignoring disabled EXC [%s]",
						papp->StrId());
				continue;
			}

			logger->Info("STEP 4: <--------- OK -- [%s]", papp->StrId());
		}

		// Disregarding commit for EXC disabled meanwhile
		if (papp->Disabled())
			continue;

		// Perform resource acquisition for RUNNING App/ExC
		SyncCommit(papp);
		excs++;
	}

	// Collecing execution metrics
	SM_GET_TIMING_SYNCSTATE(metrics, SM_SYNCP_TIME_POSTCHANGE,
			sm_tmr, syncState);
	logger->Debug("STEP 4: postChange() DONE");

	// Account for total reconfigured EXCs
	SM_COUNT_EVENT2(metrics, SM_SYNCP_EXCS, excs);

	// Collect statistics on average EXCSs reconfigured.
	SM_ADD_SAMPLE(metrics, SM_SYNCP_AVGE, excs);

	return OK;
}

SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_PostChangeForProcesses() {
	logger->Debug("STEP 4.2: postChange() START: processes");

	// Commit SYNC -> RUNNING
	ProcessMapIterator procs_it;
	ProcPtr_t proc = prm.GetFirst(Schedulable::SYNC, procs_it);
	for ( ; proc; proc = prm.GetNext(Schedulable::SYNC, procs_it)) {
		SyncCommit(proc);
		logger->Info("STEP 4.2: <--------- OK -- [%s]", proc->StrId());
	}

	// Commit FINISHED -> <removed>
	proc = prm.GetFirst(Schedulable::FINISHED, procs_it);
	for ( ; proc; proc = prm.GetNext(Schedulable::FINISHED, procs_it)) {
		logger->Info("STEP 4.2: <---- RELEASED -- [%s]", proc->StrId());
		SyncCommit(proc);
	}

	logger->Debug("STEP 4.2: postChange() DONE: processes");
	return OK;
}


void SynchronizationManager::SyncCommit(AppPtr_t papp) {
	ResourceAccounter::ExitCode_t ra_result;

	// Acquiring the resources for RUNNING Applications
	if (!papp->Blocking()) {
		logger->Debug("SyncCommit: [%s] is in %s/%s", papp->StrId(),
				papp->StateStr(papp->State()),
				papp->SyncStateStr(papp->SyncState()));

		// Resource acquisition
		ra_result = ra.SyncAcquireResources(papp);
		if (ra_result != ResourceAccounter::RA_SUCCESS) {
			logger->Error("SyncCommit: failed for [%s] (ret=%d)",
					papp->StrId(), ra_result);
			am.SyncAbort(papp);
		}
	}

	// Committing change to the manager (to update queues)
	logger->Debug("SyncCommit: [%s] (adaptive) commit...", papp->StrId());
	am.SyncCommit(papp);
}

void SynchronizationManager::SyncCommit(ProcPtr_t proc) {
	// Acquiring the resources for RUNNING Applications
	if (!proc->Blocking()) {
		logger->Debug("SyncCommit: [%s] is in %s/%s", proc->StrId(),
				proc->StateStr(proc->State()),
				proc->SyncStateStr(proc->SyncState()));
		// Resource acquisition
		auto ra_result = ra.SyncAcquireResources(proc);
		if (ra_result != ResourceAccounter::RA_SUCCESS) {
			logger->Error("SyncCommit: failed for [%s] (ret=%d)",
					proc->StrId(), ra_result);
			prm.SyncAbort(proc);
		}
	}

	// Committing change to the manager (to update queues)
	logger->Debug("SyncCommit: [%s] (process) commit...", proc->StrId());
	prm.SyncCommit(proc);
}


SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_Platform(ApplicationStatusIF::SyncState_t syncState) {
	ExitCode_t result;
	bool at_least_one_success = false;

	logger->Debug("STEP M.1: SyncPlatform(%s) START: adaptive applications",
		Schedulable::SyncStateStr(syncState));
	SM_RESET_TIMING(sm_tmr);

	AppsUidMapIt apps_it;
	AppPtr_t papp;
	papp = am.GetFirst(syncState, apps_it);
	for ( ; papp; papp = am.GetNext(syncState, apps_it)) {
		logger->Info("STEP M.1: SyncPlatform(%s)", papp->SyncStateStr(syncState));
		if (!policy->DoSync(papp))
			continue;
		logger->Info("STEP M.1: SyncPlatform(%s) ===> [%s]",
			papp->SyncStateStr(syncState), papp->StrId());

		result = MapResources(papp);
		if (result != SynchronizationManager::OK) {
			logger->Error("STEP M.1: cannot synchronize application [%s]", papp->StrId());
			sync_fails_apps.push_back(papp);
			continue;
		}
		else
			if (!at_least_one_success)
				at_least_one_success = true;
		logger->Info("STEP M.1: <--------- OK -- [%s]", papp->StrId());
	}

	// Collecting execution metrics
	SM_GET_TIMING_SYNCSTATE(metrics, SM_SYNCP_TIME_SYNCPLAT, sm_tmr, syncState);
	logger->Debug("STEP M.1: SyncPlatform(%s) DONE: adaptive applications",
		papp->SyncStateStr(syncState));

	if (at_least_one_success)
		return OK;
	return PLATFORM_SYNC_FAILED;
}

SynchronizationManager::ExitCode_t
SynchronizationManager::Sync_PlatformForProcesses() {
	ExitCode_t result;
	bool at_least_one_success = false;

	logger->Debug("STEP M.2: SyncPlatform() START: processes");
	SM_RESET_TIMING(sm_tmr);

	if (!prm.HasProcesses(Schedulable::SYNC)) {
		logger->Debug("STEP M.2: SyncPlatform() NONE: no processes");
		return NOTHING_TO_SYNC;
	}

	ProcessMapIterator procs_it;
	ProcPtr_t proc = prm.GetFirst(Schedulable::SYNC, procs_it);
	for ( ; proc; proc = prm.GetNext(Schedulable::SYNC, procs_it)) {
		result = MapResources(proc);
		if (result != SynchronizationManager::OK) {
			logger->Error("STEP M.2: cannot synchronize application [%s]", proc->StrId());
			sync_fails_procs.push_back(proc);
			continue;
		}
		else
			if (!at_least_one_success)
				at_least_one_success = true;
		logger->Info("STEP M.2: <--------- OK -- [%s]", proc->StrId());
	}

	// Collecting execution metrics
	logger->Debug("STEP M.2: SyncPlatform() DONE: processes");
	if (at_least_one_success)
		return OK;
	return PLATFORM_SYNC_FAILED;
}


SynchronizationManager::ExitCode_t
SynchronizationManager::MapResources(SchedPtr_t papp) {
	PlatformManager::ExitCode_t result = PlatformManager::PLATFORM_OK;
	logger->Info("STEP M: SyncPlatform() ===> [%s]", papp->StrId());

	switch (papp->SyncState()) {
	case Schedulable::STARTING:
		logger->Debug("STEP M: allocating resources to [%s]", papp->StrId());
		result = plm.MapResources(papp,
				papp->NextAWM()->GetResourceBinding());
		break;
	case Schedulable::RECONF:
	case Schedulable::MIGREC:
	case Schedulable::MIGRATE:
		logger->Debug("STEP M: re-mapping resources for [%s]", papp->StrId());
		result = plm.MapResources(papp,
				papp->NextAWM()->GetResourceBinding());
		break;
	case Schedulable::BLOCKED:
		logger->Debug("STEP M: reclaiming resources from [%s]", papp->StrId());
		result = plm.ReclaimResources(papp);
		break;
	default:
		break;
	}

	if (result != PlatformManager::PLATFORM_OK)
		return PLATFORM_SYNC_FAILED;
	return OK;
}


SynchronizationManager::ExitCode_t
SynchronizationManager::SyncApps(ApplicationStatusIF::SyncState_t syncState) {
	ExitCode_t result;

	if (syncState == ApplicationStatusIF::SYNC_NONE) {
		logger->Warn("Synchronization FAILED (Error: empty EXCs list)");
		assert(syncState != ApplicationStatusIF::SYNC_NONE);
		return OK;
	}

#ifdef CONFIG_BBQUE_YM_SYNC_FORCE
	SynchronizationPolicyIF::SyncLatency_t syncLatency;

	result = Sync_PreChange(syncState);
	if (result != OK)
		return result;

	syncLatency = policy->EstimatedSyncTime();
	SM_ADD_SAMPLE(metrics, SM_SYNCP_TIME_LATENCY, syncLatency);

	// Wait for the policy specified sync point
	logger->Debug("Wait sync point for %d[ms]", syncLatency);
	std::this_thread::sleep_for(
			std::chrono::milliseconds(syncLatency));

	result = Sync_SyncChange(syncState);
	if (result != OK)
		return result;

	result = Sync_Platform(syncState);
	if (result != OK)
		return result;

	result = Sync_DoChange(syncState);
	if (result != OK)
		return result;

#else
	// Platform is synched before to:
	// 1. speed-up resources assignement
	// 2. properly setup platform specific data for the time the
	// application reconfigure it-self
	// e.g. CGroups should be already properly initialized
	result = Sync_Platform(syncState);
	if (result != OK)
		return result;

	result = Sync_PreChange(syncState);
	if (result != OK)
		return result;

#endif // CONFIG_BBQUE_YM_SYNC_FORCE

	result = Sync_PostChange(syncState);
	if (result != OK)
		return result;

	return OK;
}

SynchronizationManager::ExitCode_t
SynchronizationManager::SyncProcesses() {
	// Perform resource mapping
	logger->Debug("SyncProcesses: platform mapping...");
	ExitCode_t result = Sync_PlatformForProcesses();
	if (result != OK)
		return result;
	// Commit changes
	logger->Debug("SyncProcesses: post-change commit...");
	return Sync_PostChangeForProcesses();
}

SynchronizationManager::ExitCode_t
SynchronizationManager::SyncSchedule() {
	ApplicationStatusIF::SyncState_t syncState;
	ResourceAccounter::ExitCode_t raResult;
	bu::Timer syncp_tmr;
	ExitCode_t result;
	// TODO add here proper tracing/monitoring events for statistics
	// collection

	// Update session count
	++sync_count;
	logger->Notice("SyncSchedule: synchronization [%d] START, policy [%s]",
			sync_count, policy->Name());
	am.ReportStatusQ();
	am.ReportSyncQ();
	SM_COUNT_EVENT(metrics, SM_SYNCP_RUNS); // Account for SyncP runs
	SM_RESET_TIMING(syncp_tmr);             // Reset the SyncP overall timer

	// TODO here a synchronization decision policy is used
	// to decide if a synchronization should be run or not, e.g. based on
	// the kind of applications in SYNC state or considering stability
	// problems and synchronization overheads.
	// The role of the SynchronizationManager it quite simple: it calls a
	// policy provided method which should decide what applications must be
	// synched. As soon as the queue of apps to sync returned is empty, the
	// syncronization is considered terminated and will start again only at
	// the next synchronization event.
	syncState = policy->GetApplicationsQueue(sv, true);
	if (syncState == ApplicationStatusIF::SYNC_NONE) {
		logger->Info("SyncSchedule: session=%d ABORTED", sync_count);
		// Possibly this should never happens
		assert(syncState != ApplicationStatusIF::SYNC_NONE);
		return OK;
	}

	// Start the resource accounter synchronized session
	raResult = ra.SyncStart();
	if (raResult != ResourceAccounter::RA_SUCCESS) {
		logger->Fatal("SyncSchedule: session=%d unable to start resource accounting ",
			sync_count);
		return ABORTED;
	}

	// Synchronize the adaptive applications/EXC in order of status as
	// returned by the policy
	while (syncState != ApplicationStatusIF::SYNC_NONE) {
		result = SyncApps(syncState);
		if ((result != NOTHING_TO_SYNC) && (result != OK)) {
			logger->Warn("SyncSchedule: session=%d FAILED, aborting "
				"during adaptive applications synchronization...", sync_count);
			ra.SyncAbort();
			DisableFailedApps();
			return result;
		}

		// Next set of applications to synchronize (if any)
		syncState = policy->GetApplicationsQueue(sv);
	}

	// Synchronization of generic processes
	result = SyncProcesses();
	if ((result != NOTHING_TO_SYNC) && (result != OK)) {
		logger->Warn("SyncSchedule: session=%d FAILED, aborting "
			"during processes synchronization...", sync_count);
		ra.SyncAbort();
		DisableFailedApps();
		return result;
	}

	// FIXME at this point ALL apps must be committed and the sync queues
	// empty, this should be checked probably here before to commit the
	// system view

	// Commit the resource accounter synchronized session
	raResult = ra.SyncCommit();
	if (raResult != ResourceAccounter::RA_SUCCESS) {
		logger->Fatal("SyncSchedule: session=%d resource accounting commit failed", sync_count);
		DisableFailedApps();
		return ABORTED;
	}
	SM_GET_TIMING(metrics, SM_SYNCP_TIME, syncp_tmr); // Overall SyncP execution time
	SM_COUNT_EVENT(metrics, SM_SYNCP_COMP);           // Account for SyncP completed

	DisableFailedApps();
	logger->Notice("SyncSchedule: session=%d DONE", sync_count);
	am.ReportStatusQ();
	am.ReportSyncQ();
	return OK;
}

void SynchronizationManager::DisableFailedApps() {
	for (auto papp: sync_fails_apps) {
		logger->Warn("DisableFailedApps: disabling [%s] due to failure",
				papp->StrId());
		am.DisableEXC(papp);
	}
}

void SynchronizationManager::DisableFailedProcesses() {
	for (auto proc: sync_fails_procs) {
		logger->Warn("DisableFailedProcesses: disabling [%s] due to failure",
				proc->StrId());
	//	prm.NotifyStop(proc);
		// TODO what to do for disabling?
	}
}


} // namespace bbque

