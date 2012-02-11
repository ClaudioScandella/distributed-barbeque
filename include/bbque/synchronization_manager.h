/**
 *       @file  synchronization_manager.h
 *      @brief  The module to synchronize applicaitons status
 *
 * This module provides a unified interface to access application status
 * synchronization primitives. Once a new resource scheduling has been
 * computed, the status of registered application should be updated according
 * to the new schedule. This update requires to communicate each Execution
 * Context its new assigned set of resources and to verify that the actual
 * resources usage by each application match the schedule. Some of these operations
 * are delagated to module plugins, while the core glue code for status
 * synchronization is defined by this class
 *
 *     @author  Patrick Bellasi (derkling), derkling@gmail.com
 *
 *   @internal
 *     Created  06/03/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =============================================================================
 */

#ifndef BBQUE_SYNCHRONIZATION_MANAGER_H_
#define BBQUE_SYNCHRONIZATION_MANAGER_H_

#include "bbque/config.h"
#include "bbque/plugin_manager.h"
#include "bbque/application_proxy.h"
#include "bbque/platform_proxy.h"

#include "bbque/utils/timer.h"
#include "bbque/utils/metrics_collector.h"

#include "bbque/plugins/logger.h"
#include "bbque/plugins/synchronization_policy.h"

# define BBQUE_DEFAULT_SYNCHRONIZATION_MANAGER_POLICY "sasb"

#define SYNCHRONIZATION_MANAGER_NAMESPACE "bq.ym"

using bbque::plugins::LoggerIF;
using bbque::plugins::SynchronizationPolicyIF;

using bbque::utils::Timer;
using bbque::utils::MetricsCollector;

namespace bbque {

	/**
	 * @class SynchronizationManager
	 * @brief The class implementing the glue logic for status synchronization.
	 */
class SynchronizationManager {

public:

	/**
	 * @brief Result codes generated by methods of this class
	 */
	typedef enum ExitCode {
		OK = 0,
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
	LoggerIF *logger;

	/**
	 * @brief The synchronizaiton policy plugin to use
	 */
	SynchronizationPolicyIF *policy;

	ApplicationManager & am;
	ApplicationProxy & ap;
	MetricsCollector & mc;
	ResourceAccounter & ra;
	PlatformProxy & pp;
	SystemView & sv;

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
		SM_SYNCP_TIME_PRECHANGE,
		SM_SYNCP_TIME_LATENCY,
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

	/**
	 * @brief   Build a new instance of the synchronization manager
	 */
	SynchronizationManager();

	/**
	 * @brief Synchronize the specified EXCs
	 */
	ExitCode_t SyncApps(ApplicationStatusIF::SyncState_t syncState);

	/**
	 * @brief Synchronize platform resources for the specified EXCs
	 */
	ExitCode_t Sync_Platform(ApplicationStatusIF::SyncState_t syncState);

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

	/**
	 * @brief Perform the synchronized resource acquisition
	 *
	 * @param The App/ExC that have to acquire resources
	 */
	void DoAcquireResources(AppPtr_t);

};

} // namespace bbque

#endif // BBQUE_SYNCHRONIZATION_MANAGER_H_

