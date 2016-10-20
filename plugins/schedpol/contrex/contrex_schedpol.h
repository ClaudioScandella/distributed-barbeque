/*
 * Copyright (C) 2016  Politecnico di Milano
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

#ifndef BBQUE_CONTREX_SCHEDPOL_H_
#define BBQUE_CONTREX_SCHEDPOL_H_

#include <cstdint>
#include <list>
#include <memory>

#include "bbque/configuration_manager.h"
#include "bbque/scheduler_manager.h"

#include "bbque/res/resource_path.h"
#include "bbque/plugins/plugin.h"
#include "bbque/plugins/scheduler_policy.h"


#define SCHEDULER_POLICY_NAME "contrex"

#define MODULE_NAMESPACE SCHEDULER_POLICY_NAMESPACE "." SCHEDULER_POLICY_NAME

using bbque::res::RViewToken_t;
using bbque::utils::MetricsCollector;
using bbque::utils::Timer;

// These are the parameters received by the PluginManager on create calls
struct PF_ObjectParams;

namespace bbque { namespace plugins {

class LoggerIF;

/**
 * @class ContrexSchedPol
 *
 * Contrex scheduler policy registered as a dynamic C++ plugin.
 */
class ContrexSchedPol: public SchedulerPolicyIF {

public:

	// :::::::::::::::::::::: Static plugin interface :::::::::::::::::::::::::

	static void * Create(PF_ObjectParams *);

	static int32_t Destroy(void *);


	// :::::::::::::::::: Scheduler policy module interface :::::::::::::::::::

	virtual ~ContrexSchedPol() {}

	char const * Name();


	/**
	 * @brief The member function called by the SchedulerManager to perform a
	 * new scheduling / resource allocation
	 */
	ExitCode_t Schedule(System & system, RViewToken_t & status_view);

private:

	/**
	 * @brief Specific internal exit code of the class
	 */
	enum ExitCode_t {
		OK,
		ERROR,
		ERROR_VIEW
	};

	/** Shared pointer to a scheduling entity */
	typedef std::shared_ptr<SchedEntity_t> SchedEntityPtr_t;

	/** List of scheduling entities */
	typedef std::list<SchedEntityPtr_t> SchedEntityList_t;


	/** Configuration manager instance */
	ConfigurationManager & cm;

	/** Resource accounter instance */
	ResourceAccounter & ra;

	/** System logger instance */
	std::unique_ptr<bu::Logger> logger;

	/** System view:
	 *  This points to the class providing the functions to query information
	 *  about applications and resources
	 */
	System * sys;


	/** Reference to the current scheduling status view of the resources */
	RViewToken_t sched_status_view;

	/** A counter used for getting always a new clean resources view */
	uint32_t status_view_count = 0;

	/** List of scheduling entities  */
	SchedEntityList_t entities;


	/** An High-Resolution timer */
	Timer timer;


	uint32_t proc_total;

	uint32_t nr_critical;

	uint32_t nr_non_critical;

	uint32_t nr_ready;

	uint32_t nr_running;


	/**
	 * @brief Constructor
	 *
	 * Plugins objects could be build only by using the "create" method.
	 * Usually the PluginManager acts as object
	 */
	ContrexSchedPol();

	/**
	 * @brief Optional initialization member function
	 */
	ExitCode_t Init();

	void SetPowerConfiguration();

	uint32_t ScheduleCritical();

	uint32_t ScheduleNonCritical(uint32_t proc_available, uint32_t proc_quota);

	uint32_t SchedulePriority(
		bbque::app::AppPrio_t prio, uint32_t proc_available, uint32_t proc_quota);

	SchedulerPolicyIF::ExitCode_t ScheduleApplication(
		bbque::app::AppCPtr_t papp, uint32_t proc_quota);

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_CONTREX_SCHEDPOL_H_
