/*
 * Copyright (C) 2015  Politecnico di Milano
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

#ifndef BBQUE_TEMPURA_SCHEDPOL_H_
#define BBQUE_TEMPURA_SCHEDPOL_H_

#include <cstdint>
#include <list>
#include <map>
#include <memory>

#include "bbque/configuration_manager.h"
#include "bbque/plugins/plugin.h"
#include "bbque/plugins/scheduler_policy.h"
#include "bbque/pm/model_manager.h"
#include "bbque/scheduler_manager.h"
#include "bbque/resource_manager.h"

#define SCHEDULER_POLICY_NAME "tempura"

#define MODULE_NAMESPACE SCHEDULER_POLICY_NAMESPACE "." SCHEDULER_POLICY_NAME

using bbque::pm::ModelManager;
using bbque::pm::ModelPtr_t;
using bbque::res::RViewToken_t;
using bbque::utils::MetricsCollector;
using bbque::utils::Timer;

namespace br = bbque::res;

// These are the parameters received by the PluginManager on create calls
struct PF_ObjectParams;

namespace bbque { namespace plugins {

class LoggerIF;

/**
 * @class TempuraSchedPol
 *
 * Tempura scheduler policy registered as a dynamic C++ plugin.
 */
class TempuraSchedPol: public SchedulerPolicyIF {

public:

	// :::::::::::::::::::::: Static plugin interface :::::::::::::::::::::::::

	/**
	 * @brief Create the tempura plugin
	 */
	static void * Create(PF_ObjectParams *);

	/**
	 * @brief Destroy the tempura plugin
	 */
	static int32_t Destroy(void *);


	// :::::::::::::::::: Scheduler policy module interface :::::::::::::::::::

	/**
	 * @brief Destructor
	 */
	virtual ~TempuraSchedPol();

	/**
	 * @brief Return the name of the policy plugin
	 */
	char const * Name();


	/**
	 * @brief The member function called by the SchedulerManager to perform a
	 * new scheduling / resource allocation
	 */
	ExitCode_t Schedule(System & system, RViewToken_t & status_view);

private:

	/** Shared pointer to a scheduling entity */
	typedef std::shared_ptr<SchedEntity_t> SchedEntityPtr_t;

	/** List of scheduling entities */
	typedef std::list<SchedEntityPtr_t> SchedEntityList_t;


	/** Configuration manager instance */
	ConfigurationManager & cm;

	/** Resource accounter instance */
	ResourceAccounter & ra;

	/** P/T model manager */
	ModelManager & mm;

	/** System logger instance */
	std::unique_ptr<bu::Logger> logger;

	/** System view:
	 *  This points to the class providing the functions to query information
	 *  about applications and resources
	 */
	System * sys;


	/** Reference to the current scheduling status view of the resources */
	RViewToken_t sched_status_view;

	/** Scheduler counter */
	uint32_t sched_count = 0;

	/** String for requiring new resource status views */
	char status_view_id[30];

	/** A counter used for getting always a new clean resources view */
	uint32_t status_view_count = 0;

	/** List of scheduling entities  */
	SchedEntityList_t entities;

	/** Allocatable resource slots */
	uint32_t slots ;


	/** Power budgets due to thermal or energy constraints */
	br::UsagesMap_t power_budgets;

	/** Resource budgets according to the power budgets */
	br::UsagesMap_t resource_budgets;

	/** Power-thermal models for each resource to bind  */
	std::map<br::ResourcePathPtr_t, std::string> model_ids;


	/** An High-Resolution timer */
	Timer timer;

	/**
	 * @brief Constructor
	 *
	 * Plugins objects could be build only by using the "create" method.
	 * Usually the PluginManager acts as object
	 */
	TempuraSchedPol();

	/**
	 * @brief Policy initialization
	 */
	ExitCode_t Init();

	/**
	 * @brief Initialize the new resource status view
	 */
	ExitCode_t InitResourceStateView();

	/**
	 * @brief Initialize the power and resource budgets
	 */
	ExitCode_t InitBudgets();

	/**
	 * @brief Initialize the slots to allocate
	 *
	 * The slot is a resource unity of allocation. The idea is that a resource
	 * budget can be seen as a number slots. The slots assigned to each
	 * application are proportional to its priority
	 */
	ExitCode_t InitSlots();

	/**
	 * @brief Compute power and resource budgets
	 *
	 * This step is performed according to the constraints on the critical
	 * termal threshold and (optionally) the energy budget
	 */
	ExitCode_t ComputeBudgets();

	/**
	 * @brief Define the power budget of a specific resource to allocate
	 * (power capping)
	 *
	 * @param rp The resource path
	 * @param pmodel The resource powert-thermal model
	 *
	 * @return The power value to cap (in milliwatts)
	 */
	uint32_t GetPowerBudget(
			br::ResourcePathPtr_t const & rp, ModelPtr_t pmodel);

	/**
	 * @brief Define the power budget given specified thermal constraints
	 *
	 * Consider for instance critical thermal threshold
	 *
	 * @param rp The resource path
	 * @param pmodel The resource powert-thermal model
	 *
	 * @return The power value to cap (in milliwatts)
	 */
	uint32_t GetPowerBudgetFromThermalConstraints(
			br::ResourcePathPtr_t const & rp, ModelPtr_t pmodel);

	/**
	 * @brief Define the power budget given specified energy constraints
	 *
	 * Consider for instance battery lifetime goals
	 *
	 * @param rp The resource path
	 * @param pmodel The resource powert-thermal model
	 *
	 * @return The power value to cap (in milliwatts)
	 */
	uint32_t GetPowerBudgetFromEnergyConstraints(
			br::ResourcePathPtr_t const & rp, ModelPtr_t pmodel);

	/**
	 * @brief Define the resource budget to allocate according to the power
	 * budget
	 *
	 * The function is in charge of computing the amount of resource to
	 * allocate, for instance by capping the CPU total bandwith and/or setting
	 * the CPU cores frequencies.
	 *
	 * @param rp The resource path
	 * @param pmodel The resource powert-thermal model
	 *
	 * @return The total amount of allocatable resource
	 */
	int64_t GetResourceBudget(
			br::ResourcePathPtr_t const & rp, ModelPtr_t pmodel);

	/**
	 * @brief Perform the resource partitioning among active applications
	 *
	 * @return SCHED_OK for success
	 */
	ExitCode_t DoResourcePartitioning();

	/**
	 * @brief Build the working mode of assigned resources
	 *
	 * @param papp Pointer to the application to schedule
	 *
	 * @return SCHED_OK for success
	 */
	ExitCode_t AssignWorkingMode(ba::AppCPtr_t papp);

	/**
	 * @brief Check if the application does not need to be re-scheduled
	 *
	 * @param papp The pointer to the application descriptor
	 *
	 * @return true if the application must be skipped, false otherwise
	 */
	bool CheckSkip(ba::AppCPtr_t const & papp);

	/**
	 * @brief Do resource binding and send scheduling requests
	 *
	 * @return SCHED_OK for success
	 */
	ExitCode_t DoScheduling();

	/**
	 * @brief Bind the working mode to platform resources
	 *
	 * @param papp Pointer to the scheduling entity (application and working
	 * mode)
	 *
	 * @return SCHED_OK for success
	 */
	ExitCode_t DoBinding(SchedEntityPtr_t psched);
};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_TEMPURA_SCHEDPOL_H_