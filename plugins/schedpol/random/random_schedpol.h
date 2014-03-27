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

#ifndef BBQUE_RANDOM_SCHEDPOL_H_
#define BBQUE_RANDOM_SCHEDPOL_H_

#include "bbque/configuration_manager.h"
#include "bbque/app/application.h"
#include "bbque/plugins/scheduler_policy.h"
#include "bbque/plugins/plugin.h"

#include <cstdint>
#include <random>

#define SCHEDULER_POLICY_NAME "random"
#define MODULE_NAMESPACE SCHEDULER_POLICY_NAMESPACE "." SCHEDULER_POLICY_NAME
#define MODULE_CONFIG SCHEDULER_POLICY_CONFIG "." SCHEDULER_POLICY_NAME

// These are the parameters received by the PluginManager on create calls
struct PF_ObjectParams;

using bbque::app::AppCPtr_t;
using bbque::res::RViewToken_t;

namespace bbque { namespace plugins {

// Forward declaration
class Logger;

/**
 * @brief The Random resource scheduler heuristic plugin.
 *
 * A dynamic C++ plugin which implements the Random resource
 * scheduler heuristic.
 */
class RandomSchedPol : public SchedulerPolicyIF {

public:

//----- static plugin interface

	/**
	 *
	 */
	static void * Create(PF_ObjectParams *);

	/**
	 *
	 */
	static int32_t Destroy(void *);

	virtual ~RandomSchedPol();

//----- Scheduler Policy module interface

	char const * Name();

	SchedulerPolicyIF::ExitCode_t
		Schedule(bbque::System & sv, RViewToken_t & rav);

private:

	/**
	 * @brief System logger instance
	 */
	Logger *logger;

	/** Configuration manager instance */
	ConfigurationManager & cm;

	/** Token for accessing a resources view */
	RViewToken_t ra_view = 0;

	/** A counter used for getting always a new clean resources view */
	uint32_t ra_view_count = 0;

	/** The base resource path for the binding step */
	std::string binding_domain;

	/** The type of resource for the binding step */
	Resource::Type_t binding_type;
	
	/**
	 * @brief Random Number Generator engine used for AWM selection
	 */
	std::uniform_int_distribution<uint16_t> dist;

	/**
	 * @brief   The plugins constructor
	 * Plugins objects could be build only by using the "create" method.
	 * Usually the PluginManager acts as object
	 * @param   
	 * @return  
	 */
	RandomSchedPol();

	/**
	 * @brief Scheduler run intialization
	 *
	 * This method is used each time a new random scheduling is tirggered
	 * in order to properly setup the system view to be used.
	 *
	 * @return SCHED_DONE on success, SCHED_ERROR on falilure
	 */
	SchedulerPolicyIF::ExitCode_t Init();

	/**
	 * @brief Randonly select an AWM for the application
	 */
	void ScheduleApp(AppCPtr_t papp);
};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_RANDOM_SCHEDPOL_H_
