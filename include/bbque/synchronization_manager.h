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

#ifndef BBQUE_SYNCHRONIZATION_MANAGER_H_
#define BBQUE_SYNCHRONIZATION_MANAGER_H_

#include "bbque/config.h"
#include "bbque/plugin_manager.h"
#include "bbque/application_proxy.h"
#include "bbque/platform_manager.h"
#include "bbque/process_manager.h"
#include "bbque/utils/timer.h"
#include "bbque/utils/metrics_collector.h"

#include "bbque/utils/logging/logger.h"
#include "bbque/plugins/synchronization_policy.h"

# define BBQUE_DEFAULT_SYNCHRONIZATION_MANAGER_POLICY "sasb"

#define SYNCHRONIZATION_MANAGER_NAMESPACE "bq.ym"

namespace bu = bbque::utils;

using bbque::plugins::SynchronizationPolicyIF;

using bbque::utils::Timer;
using bbque::utils::MetricsCollector;

namespace bbque {

/**
 * @class SynchronizationManager
 * @brief The class implementing the glue logic for status synchronization.
 *
 * This module provides a unified interface to access application status
 * synchronization primitives. Once a new resource scheduling has been
 * computed, the status of registered application should be updated according
 * to the new schedule. This update requires to communicate each Execution
 * Context its new assigned set of resources and to verify that the actual
 * resources usage by each application match the schedule. Some of these
 * operations are delagated to module plugins, while the core glue code for
 * status synchronization is defined by this class

 * @ingroup sec06_ym
 */
class SynchronizationManager {

public:

	/**
	 * @brief Result codes generated by methods of this class
	 */
	typedef enum ExitCode {
		OK = 0,
		NOTHING_TO_SYNC,
		ABORTED,
		PLATFORM_SYNC_FAILED
	} ExitCode_t;


	/**
	 * @brief Get a reference to the synchronization manager
	 * The SynchronizationManager is a singleton class providing the glue logic for
	 * the Barbeque status synchronization.
	 *
	 * @return  a reference to the SynchronizationManager singleton instance
	 */
	static SynchronizationManager & GetInstance();

	/**
	 * @brief  Clean-up the grill by releasing current resource manager
	 * resources and modules.
	 */
	~SynchronizationManager();

	/**
	 * @brief Synchronize the application status
	 */
	ExitCode_t SyncSchedule();

private:

	/**
	 * @brief The logger to use.
	 */
	std::unique_ptr<bu::Logger> logger;

	/**
	 * @brief The synchronizaiton policy plugin to use
	 */
	SynchronizationPolicyIF *policy;

	ApplicationManager & am;
	ApplicationProxy & ap;
	MetricsCollector & mc;
	ResourceAccounter & ra;
	PlatformManager & plm;
	ProcessManager & prm;
	System & sv;

	/**
	 * @brief The number of synchronization cycles
	 */
	uint32_t sync_count;

	typedef enum SyncMgrMetrics {
		//----- Event counting metrics
		SM_SYNCP_RUNS = 0,
		SM_SYNCP_COMP,
		SM_SYNCP_EXCS,
		SM_SYNCP_SYNC_HIT,
		SM_SYNCP_SYNC_MISS,
		//----- Timing metrics
		SM_SYNCP_TIME,
		SM_SYNCP_TIME_LATENCY,
		SM_SYNCP_TIME_PRECHANGE,
		SM_SYNCP_TIME_SYNCCHANGE,
		SM_SYNCP_TIME_SYNCPLAT,
		SM_SYNCP_TIME_DOCHANGE,
		SM_SYNCP_TIME_POSTCHANGE,
		//----- Couting statistics
		SM_SYNCP_AVGE,
		SM_SYNCP_APP_SYNCLAT,

		SM_METRICS_COUNT
	} SyncMgrMetrics_t;

	/** The High-Resolution timer used for profiling */
	Timer sm_tmr;

	static MetricsCollector::MetricsCollection_t metrics[SM_METRICS_COUNT];

	std::list<AppPtr_t> sync_fails_apps;

	std::list<ProcPtr_t> sync_fails_procs;

	/**
	 * @brief   Build a new instance of the synchronization manager
	 */
	SynchronizationManager();

	/**
	 * @brief Synchronize the specified EXCs
	 */
	ExitCode_t SyncApps(ApplicationStatusIF::SyncState_t syncState);

	ExitCode_t SyncProcesses();

	/**
	 * @brief Synchronize platform resources for the specified EXCs
	 */
	ExitCode_t Sync_Platform(ApplicationStatusIF::SyncState_t syncState);

	ExitCode_t Sync_PlatformForProcesses();

	ExitCode_t MapResources(SchedPtr_t papp);

	/**
	 * @brief Notify a Pre-Change to the specified EXCs
	 */
	ExitCode_t Sync_PreChange(ApplicationStatusIF::SyncState_t syncState);

	/**
	 * @brief Notify a Sync-Change to the specified EXCs
	 */
	ExitCode_t Sync_SyncChange(ApplicationStatusIF::SyncState_t syncState);

	/**
	 * @brief Notify a Do-Change to the specified EXCs
	 */
	ExitCode_t Sync_DoChange(ApplicationStatusIF::SyncState_t syncState);

	/**
	 * @brief Notify a Post-Change to the specified EXCs
	 */
	ExitCode_t Sync_PostChange(ApplicationStatusIF::SyncState_t syncState);

	ExitCode_t Sync_PostChangeForProcesses();

	/**
	 * @brief Perform the synchronized resource acquisition
	 *
	 * @param The application or process that has to acquire resources
	 */
	void SyncCommit(AppPtr_t);

	void SyncCommit(ProcPtr_t);

	/**
	 * @brief Check fo reshuffling reconfigurations
	 *
	 * A resuhffling reconfiguration happens when an application is not
	 * changing its working mode or not being migrated but just assigned a
	 * different set of resources within the same node.
	 *
	 * @param papp The App/ExC to verify
	 *
	 * @return true if this is a reshuffling reconfiguration
	 */
	bool Reshuffling(AppPtr_t papp);

	/**
	 * @brief Collects result from EXCs during PreChange
	 */
	void Sync_PreChange_Check_EXC_Response(AppPtr_t papp, 
                                 ApplicationProxy::pPreChangeRsp_t presp) const;

	/**
	 * @brief Collects result from EXCs during SyncChange
	 */
	void Sync_SyncChange_Check_EXC_Response(AppPtr_t papp, 
                                 ApplicationProxy::pSyncChangeRsp_t presp) const;

	/**
	 * @brief Disable EXCs for which the synchronization has not been
	 * successfully performed
	 */
	void DisableFailedApps();

	void DisableFailedProcesses();

};

} // namespace bbque

#endif // BBQUE_SYNCHRONIZATION_MANAGER_H_
