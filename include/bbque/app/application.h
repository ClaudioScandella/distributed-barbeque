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

#ifndef BBQUE_APPLICATION_H
#define BBQUE_APPLICATION_H

#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#include "bbque/config.h"
#include "bbque/rtlib.h"
#include "bbque/app/application_conf.h"
#include "bbque/app/recipe.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/utility.h"

#include "tg/partition.h"

#define APPLICATION_NAMESPACE "bq.app"
#define APPLICATION_NAME_LEN  16

namespace bu = bbque::utils;

namespace bbque {

namespace res {
class Resource;
class ResourcePath;
class ResourceConstraint;
class ResourceAssignment;
using ResourcePathPtr_t = std::shared_ptr<ResourcePath> ;
using ResourceAssignmentPtr_t = std::shared_ptr<ResourceAssignment>;
using ResourceAssignmentMap_t = std::map<ResourcePathPtr_t, ResourceAssignmentPtr_t, CompareSP<ResourcePath>>;
using ResourceAssignmentMapPtr_t = std::shared_ptr<ResourceAssignmentMap_t>;
}

namespace br = bbque::res;

namespace app {

class Recipe;

/** Shared pointer to Recipe object */
typedef std::shared_ptr<Recipe> RecipePtr_t;
/** Pair contained in the resource constraints map  */
typedef std::pair<br::ResourcePathPtr_t, ConstrPtr_t> ConstrPair_t;

/**
 * @brief Collect the runtime profiling data from the RTLib
 */
struct RuntimeProfiling_t {
	bool is_valid = false;

	/** The current Goal-Gap value, must be in [-100,100] */
	int ggap_percent = 0;
	int ggap_percent_prev = 0;
	int ggap_percent_prediction = 0;

	struct {
		int upper_cpu = -1;
		int upper_gap = -1;
		int upper_age = -1;
		int lower_cpu = -1;
		int lower_gap = -1;
		int lower_age = -1;
	} gap_history;

	/** The current CPU Usage of an application. It should be received as a
	 * feedback from the rtlib. */
	int cpu_usage      = 0;
	int cpu_usage_prev = 0;

	/** Cycle time */
	int ctime_ms = 0;

	/** The maximum CPU Usage allocated to an application. It should be
	 * set by the scheduling policy and optionally compared with the variable
	 * `cpu_usage` */
	int cpu_usage_prediction = 0;
	int cpu_usage_prediction_old = 0;
};

/**
 * @class ApplicationConfIF
 *
 * @brief Such descriptor includes static and dynamic information upon application
 * execution. It embeds usual information about name, priority, user, PID
 * (could be different from the one given by OS) plus a reference to the
 * recipe object, the list of enabled working modes and resource constraints.
 */
class Application: public ApplicationConfIF {

public:

	/**
	 * @brief Constructor with parameters name and priority class
	 * @param name Application name
	 * @param pid Process ID
	 * @param exc_id The ID of the Execution Context (assigned from the application)
	 * @param lang The application language
	 * @param container If true, this is just an "EXC container".
	 */
	Application(std::string const & name,
			AppPid_t pid, uint8_t exc_id,
			RTLIB_ProgrammingLanguage_t lang = RTLIB_LANG_CPP,
			bool container = false);

	/**
	 * @brief Default destructor
	 */
	virtual ~Application();

	/**
	 * @see ApplicationStatusIF
	 */
	inline std::string const & Name() const noexcept { return name; }

	/**
	 * @brief Set the application name
	 * @param app_name The application name
	 */
	inline void SetName(std::string const & app_name) noexcept { name = app_name; }

	/**
	 * @brief Get the process ID of the application
	 * @return PID value
	 */
	inline AppPid_t Pid() const noexcept { return pid; }

	/**
	 * @see ApplicationStatusIF
	 */
	inline uint8_t ExcId() const noexcept { return exc_id; }

	/**
	 * @see ApplicationStatusIF
	 */
	inline RTLIB_ProgrammingLanguage_t Language() const noexcept { return language; }

	/**
	 * @brief Get a string ID for this Execution Context
	 * This method build a string ID according to this format:
	 * PID:TASK_NAME:EXC_ID
	 * @return String ID
	 */
	inline const char *StrId() const noexcept { return str_id; }

	/**
	 * @see ApplicationStatusIF
	 */
	AppPrio_t Priority() const noexcept { return priority; }

	/**
	 * @brief Set the priority of the application
	 */
	void SetPriority(AppPrio_t prio);

	/**
	 * @see ApplicationStatusIF
	 */
	inline float Value() const noexcept { return schedule.value; }

	/**
	 * @see ApplicationConfIF
	 */
	inline void SetValue(float sched_metrics) noexcept { schedule.value = sched_metrics; }

	/**
	 * @see ApplicationStatusIF
	 */
	inline uint64_t ScheduleCount() noexcept { return schedule.count; }

	/**
	 * @brief Enable the application for resources assignment
	 *
	 * A newly created application is disabled by default, thus it will not be
	 * considered for resource scheduling until it is enabled.
	 */
	ExitCode_t Enable();

	/**
	 * @brief Disabled the application for resources assignment
	 *
	 * A disabled application will not be considered for resources scheduling.
	 */
	ExitCode_t Disable();

	/**
	 * @brief This returns all the informations loaded from the recipe and
	 * stored in a specific object
	 * @return A shared pointer to the recipe object
	 */
	inline RecipePtr_t GetRecipe() noexcept { return recipe; }

	/**
	 * @brief Set the current recipe used by the application.
	 *
	 * The recipe should be loaded by the application manager using a
	 * specific plugin.
	 *
	 * @param recipe Recipe object shared pointer
	 * @param papp Shared pointer to the current Application.
	 *
	 * @return APP_SUCCESS if success, APP_RECP_NULL if a null Recipe object
	 * is passed
	 *
	 * @note papp should be provided by ApplicationManager, which instances
	 * the new Application descriptor
	 */
	ExitCode_t SetRecipe(RecipePtr_t & recipe, AppPtr_t & papp);

	/**
	 * @brief Reload the set of Working Modes
	 *
	 * This is usually required whenever there has been a change in the status
	 * of the resources (e.g., total availability) due to an HW fault or some
	 * low-level actions of power/thermal management, once a re-validation of
	 * the recipes has been performed.
	 *
	 * @papp Shared pointer to the current Application (provided by the
	 * Application Manager)
	 */
	inline void ReloadWorkingModes(AppPtr_t & papp) { return InitWorkingModes(papp); }

	/**
	 * @brief Set Platform Specific Data initialized
	 *
	 * Mark the Platform Specific Data as initialized for this application
	 */
	inline void SetPlatformData() noexcept { platform_data = true; }

#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	inline void SetCGroupSetupData(
		unsigned long cpu_ids, unsigned long mem_ids,
		unsigned long cpu_ids_isolation)
	{
		cgroup_data.cpu_ids = cpu_ids;
		cgroup_data.cpus_ids_isolation = cpu_ids_isolation;
		cgroup_data.mem_ids = mem_ids;
	}

	CGroupSetupData_t GetCGroupSetupData()
	{
		return cgroup_data;
	}
#endif // CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION

	/**
	 * @brief Check Platform Specific Data initialization
	 *
	 * Return true if this application has already a properly configured
	 * set of Platform Specific Data.
	 */
	inline bool HasPlatformData() const noexcept { return platform_data; }

	/** @brief Set a remote application
	 *
	 * Mark the application as remote or local
	 */
	inline void SetRemote(bool is_remote) noexcept { remotely_scheduled = is_remote; }

	/**
	 * @brief Return true if the application is executing or will be
	 *        executed remotely, false if not.
	 */
	inline bool IsRemote() const noexcept { return remotely_scheduled; }

	/**
	 * @brief Set a remote application
	 *
	 * Mark the application as remote or local
	 */
	inline void SetLocal(bool is_local) noexcept { locally_scheduled = is_local; }

	/**
	 * @brief Return true if the application is executing or will be
	 *        executed locally, false if not.
	 */
	inline bool IsLocal() const noexcept { return locally_scheduled; }

	/**
	 * @see ApplicationStatuIF
	 */
	bool Disabled();

	/**
	 * @see ApplicationStatuIF
	 */
	bool Active();

	/**
	 * @see ApplicationStatuIF
	 */
	bool Running();

	/**
	 * @see ApplicationStatuIF
	 */
	bool Synching();

	/**
	 * @see ApplicationStatuIF
	 */
	bool Starting();

	/**
	 * @see ApplicationStatuIF
	 */
	bool Blocking();

	/**
	 * @see ApplicationStatusIF
	 */
	State_t State();

	/**
	 * @see ApplicationStatusIF
	 */
	State_t PreSyncState();

	/**
	 * @see ApplicationStatusIF
	 */
	SyncState_t SyncState();

	/**
	 * @see ApplicationStatusIF
	 */
	bool IsContainer() const noexcept { return container; }

	/**
	 * @see ApplicationStatusIF
	 */
	AwmPtr_t const & CurrentAWM();

	/**
	 * @see ApplicationStatusIF
	 */
	AwmPtr_t const & NextAWM();

	/**
	 * @see ApplicationStatusIF
	 */
	bool SwitchingAWM();

	/**
	 * @see ApplicationStatusIF
	 */
	inline AwmPtrList_t const & WorkingModes() noexcept { return awms.enabled_list; }

	/**
	 * @see ApplicationStatusIF
	 */
	inline AwmPtr_t const & LowValueAWM() noexcept { return awms.enabled_list.front(); }

	/**
	 * @see ApplicationStatusIF
	 */
	inline AwmPtr_t const & HighValueAWM() noexcept { return awms.enabled_list.back(); }

	/**
	 * @see ApplicationStatusIF
	 */
	uint64_t GetResourceRequestStat(
			std::string const & rsrc_path,
			ResourceUsageStatType_t ru_stat);



	// -------------- Scheduling management ----------------------------- //


	/**
	 * @see ApplicationConfIF
	 */
	ExitCode_t ScheduleRequest(
			AwmPtr_t const & awm,
			br::RViewToken_t status_view,
			size_t b_refn = 0);

	/**
	 * @brief Commit a previously required re-scheduling request
	 *
	 * @return APP_SUCCESS on successful update of internal data
	 * structures, APP_ABORT on errors.
	 */
	ExitCode_t ScheduleCommit();

	/**
	 * @brief Abort a scheduling
	 *
	 * This must be called only if the application is in a SYNC state.
	 */
	void ScheduleAbort();

	/**
	 * @brief The application can continue to run
	 *
	 * This method must be called by ApplicationManager to signal that the
	 * Application (previously RUNNING) doesn't need to be reconfigured or
	 * migrated. This it can continue to run in the same AWM, using the same
	 * resources.
	 *
	 * @return APP_SUCCESS for success, APP_ABORT for failed.
	 */
	ExitCode_t ScheduleContinue();

	/**
	 * @brief Terminate this EXC by releasing all resources.
	 *
	 * This method requires to mark the EXC as terminated and to prepare the
	 * ground for releasing all resources as soon as possible. Due to
	 * asynchronous nature of this event and the activation of Optimized and
	 * Synchronizer, a valid reference to the object is granted to be keept
	 * alive until all of its users have terminated.
	 */
	ExitCode_t Terminate();


	// --------------------- Working Modes ----------------------------- //


	/**
	 * @brief Get a working mode descriptor
	 *
	 * @param wmId Working mode ID
	 * @return The (enabled) working mode descriptor
	 *
	 * @note The working mode must come from the enabled list
	 */
	AwmPtr_t GetWorkingMode(uint8_t wmId);

	/**
	 * @brief Set or clear a constraint on the working modes
	 *
	 * It's important to clearly explain the behavior of the following API.
	 * To set a lower bound means that all the AWMs having an ID lesser than
	 * the one specified in the method call will be disabled. Thus they will
	 * not considered by the scheduler. Similarly, but accordingly to the
	 * reverse logics, if an upper bound is set: all the working modes with ID
	 * greater to the one specified will be disabled.
	 * An "exact value" constraint acts in addictive way. This means that the
	 * working mode specified is added to the constraint range (if not
	 * included yet).
	 *
	 * Whenever a constraint of the same type occurs, the previous constraint
	 * is replaced. This means that if the application sets a lower bound, a
	 * second lower bound assertion will "overwrite" the firsts. The same for
	 * upper bound case.
	 *
	 * @param constraint @see RTLIB_Constraint
	 * @return APP_WM_ENAB_CHANGED if the set of enabled working modes has
	 * been successfully changed, APP_WM_ENAB_UNCHANGED otherwise.
	 * Then APP_WM_NOT_FOUND is returned if the working mode ID is not
	 * correct.
	 */
	ExitCode_t SetWorkingModeConstraint(RTLIB_Constraint & constraint);

	/**
	 * @brief Remove all the working mode constraints
	 *
	 * Reset the list of enabled working modes by included the whole set of
	 * working modes loaded from the recipe.
	 */
	void ClearWorkingModeConstraints();

	/**
	 * @brief Check the validity of the AWM scheduled
	 *
	 * This call check if the current scheduled AWM has been invalidated by a
	 * constraint assertion.
	 *
	 * @return true is the AWM is no more valid, false otherwise.
	 */
	inline bool CurrentAWMNotValid() const noexcept { return awms.curr_inv; }

	/**
	 * @brief Dump on logger a list of valid AWMs
	 *
	 * The list of AWMs which are currently valid is dumped on log with the
	 * Info loglevel.
	 */
	void DumpValidAWMs() const;


	// ---------------------- Profiling management ---------------------- //

	/**
	 * @brief Get the profiling data collected at runtime by the RTLib
	 *
	 * @param mark_acknowledged
	 * @return A data structure of type RuntimeProfiling_t
	 */
	inline RuntimeProfiling_t GetRuntimeProfile(bool mark_acknowledged = false) {
		std::unique_lock<std::mutex> rtp_lock(rt_prof_mtx);
		if (mark_acknowledged)
			rt_prof.is_valid = false;
		return rt_prof;
	}

	/**
	 * @brief Save the profiling data collected at runtime by the RTLib
	 *
	 * @param rt_profile The structure containing the profiling data
	 */
	inline void SetRuntimeProfile(struct RuntimeProfiling_t rt_profile) {
		std::unique_lock<std::mutex> rtp_lock(rt_prof_mtx);
		rt_prof = rt_profile;
	}

	/**
	 * @brief Save predictions about profiling data
	 *
	 * @param cpu_usage_prediction The expected amount of CPU usage
	 * @param goal_gap_prediction The expected goal gap
	 */
	inline void SetAllocationInfo(int cpu_usage_prediction, int goal_gap_prediction = 0) {
		std::unique_lock<std::mutex> rtp_lock(rt_prof_mtx);
		rt_prof.cpu_usage_prediction_old = rt_prof.cpu_usage_prediction;
		rt_prof.cpu_usage_prediction     = cpu_usage_prediction;
		rt_prof.ggap_percent_prediction  = goal_gap_prediction;
	}

	// -------------------------- Task-graph management ------------------------------------- //

#ifdef CONFIG_BBQUE_TG_PROG_MODEL

	/**
	 * @brief Restore/retrieve the task-graph description
	 */
	ExitCode_t LoadTaskGraph();

	/**
	 * @brief Return the current task-graph description
	 */
	inline std::shared_ptr<TaskGraph> GetTaskGraph() { return task_graph; }

	/**
	 * @brief Set a new task-graph description
	 * @note Typically used by the scheduling policy for resource mapping purpose
	 * @param tg Shared pointer to task-graph descriptor
	 * @param write_through If set to true (default) update also the file or memory
	 * region storing the copy shared with the RTLib
	 */
	inline void SetTaskGraph(std::shared_ptr<TaskGraph> tg, bool write_through=true) {
		task_graph = tg;
		if (write_through) UpdateTaskGraph();
	}

	/**
	 * @brief Update the task-graph description shared with the RTLib
	 */
	 void UpdateTaskGraph();

	/**
	 * @brief Clear the task-graph description (it optionally forces a reload)
	 */
	inline void ClearTaskGraph() { task_graph = nullptr; }


	/**
	 * @brief Performance requirements of a task
	 * @param task_id Task identification number
	 * @return A reference to TaskRequirements stored in the recipe object
	 */
	inline const TaskRequirements & GetTaskRequirements(uint32_t task_id) {
		return recipe->GetTaskRequirements(task_id);
	};

	/**
	 * @brief Return the current partition
	 */
	inline std::shared_ptr<Partition> GetPartition() { return assigned_partition; }

	/**
	 * @brief Set the allocated partition
	 * @note Typically used by the scheduling policy for resource mapping purpose
	 * @param partition the allocated partition object
	 */
	inline void SetPartition(std::shared_ptr<Partition> partition) {
		assigned_partition = partition;
	}

#endif // CONFIG_BBQUE_TG_PROG_MODEL

private:

	/** The logger used by the application */
	std::unique_ptr<bu::Logger> logger;

	/** The application name */
	std::string name;

	/** The user who has launched the application */
	std::string user;

	/** The PID assigned from the OS */
	AppPid_t pid;

	/** The ID of this Execution Context */
	uint8_t exc_id;

	/** The programming language */
	RTLIB_ProgrammingLanguage_t language;

	/** The application string ID */
	char str_id[APPLICATION_NAME_LEN];

	/** True if this is an application container */
	bool container;

	/** A numeric priority value */
	AppPrio_t priority = BBQUE_APP_PRIO_LEVELS - 1;

	/** Current scheduling informations */
	SchedulingInfo_t schedule;
#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	CGroupSetupData_t cgroup_data;
#endif
	/**
	 * @brief Store profiling information collected at runtime
	 */
	RuntimeProfiling_t rt_prof;

	std::mutex rt_prof_mtx;

	/**
	 * Task-graph serialization file path
	 */
	std::string tg_path;

	/**
	 * Task-graph named semaphore
	 */
	std::string tg_sem_name;

	sem_t * tg_sem = nullptr;

	/**
	 * Task-graph descriptor (shared pointer to)
	 */
	std::shared_ptr<TaskGraph> task_graph;

	/**
	 * Assigned partition
	 */
	std::shared_ptr<Partition> assigned_partition = nullptr;

	/**
	 * Platform Specifica Data properly initialized
	 */
	bool platform_data = false;

	/**
	 * Remotely scheduled
	 */
	bool remotely_scheduled = false;

	/**
	 * Locally scheduled
	 */
	bool locally_scheduled = false;

	/**
	 * Recipe pointer for the current application instance.
	 * At runtime we could manage many instances of the same application using
	 * different recipes.
	 */
	RecipePtr_t recipe;

	/** @struct WorkingModesInfo
	 *
	 * Store the information needed to support the management of the
	 * application working modes.
	 */
	struct WorkingModesInfo {
		/** Vector of all the working modes */
		AwmPtrVect_t recipe_vect;
		/** List of pointers to enabled working modes */
		AwmPtrList_t enabled_list;
		/** A bitset to keep track of the enabled working modes */
		std::bitset<MAX_NUM_AWM> enabled_bset;
		/** Lower bound AWM ID*/
		uint8_t low_id;
		/** Upper bound AWM ID*/
		uint8_t upp_id;
		/** The number of working modes - 1*/
		uint8_t max_id;
		/** Current AWM invalidated by a constraint assertion */
		bool curr_inv;
	} awms;

	/**
	 * The following map keeps track of the constraints on resources.
	 * It is used by function UsageOutOfBounds() (see below) to check if a working
	 * mode includes a resource assignments that violates a bounds contained in this
	 * map.
	 **/
	ConstrMap_t rsrc_constraints;

	/**
	 * @brief Init working modes by reading the recipe
	 *
	 * @param papp Pointer to the current Application, allocated by
	 * ApplicationManager
	 */
	void InitWorkingModes(AppPtr_t & papp);

	/**
	 * @brief Init constraints by reading the recipe
	 *
	 * The method reads the "static" constraints on resources.
	 */
	void InitResourceConstraints();


	bool _Disabled() const;

	bool _Active() const;

	bool _Running() const;

	bool _Synching() const;

	bool _Starting() const;

	bool _Blocking() const;

	State_t _State() const;

	State_t _PreSyncState() const;

	SyncState_t _SyncState() const;

	AwmPtr_t const & _CurrentAWM() const;

	AwmPtr_t const & _NextAWM() const;

	bool _SwitchingAWM() const;

	/**
	 * @brief Assert a specific resource constraint.
	 *
	 * If exists yet it is overwritten. This could disable some working modes.
	 *
	 * @param res_path Resource path
	 * @param type The constraint type (@see ContraintType)
	 * @param value The constraint value
	 *
	 * @return APP_SUCCESS if success, APP_RSRC_NOT_FOUND if the resource
	 * specified does not exist.
	 */
	ExitCode_t SetResourceConstraint(br::ResourcePathPtr_t r_path,
			br::ResourceConstraint::BoundType_t b_type, uint64_t value);

	/**
	 * @brief Remove a constraint upon a specific resource.
	 *
	 * This could re-enable some working modes.
	 *
	 * @param res_path A pointer to the resource object
	 * @param type The constraint type (@see ContraintType)
	 *
	 * @return APP_SUCCESS if success, APP_CONS_NOT_FOUND if cannot be found
	 * a previous constraint on the resource
	 */
	ExitCode_t ClearResourceConstraint(br::ResourcePathPtr_t r_path,
			br::ResourceConstraint::BoundType_t b_type);


	/**
	 * @brief Set a constraint on the working modes list
	 *
	 * Three types of constraints are provided, according to @see
	 * RTLIB_Constraint: lower bound, upper bound, exact value.
	 *
	 * @param constraint @see RTLIB_Constraint
	 * @return APP_WM_ENAB_CHANGED if the enabled AWM list has changed,
	 * APP_WM_ENAB_UNCHANGED otherwise.
	 */
	ExitCode_t AddWorkingModeConstraint(RTLIB_Constraint & constraint);

	/**
	 * @brief Set lower bound info into the awm_range struct
	 *
	 * The structure awm_range is properly filled with the info needed to
	 * rebuild the list of enabled working modes, according to the asserted
	 * lower bound.
	 *
	 * @param constraint @see RTLIB_Constraint
	 */
	void SetWorkingModesLowerBound(RTLIB_Constraint & constraint);

	/**
	 * @brief Set the upper bound info into the awm_range struct
	 *
	 * The structure awm_range is properly filled with the info needed to
	 * rebuild the list of enabled working modes, according to the asserted
	 * upper bound.
	 *
	 * @param constraint @see RTLIB_Constraint
	 */
	void SetWorkingModesUpperBound(RTLIB_Constraint & constraint);

	/**
	 * @brief Remove a constraint on a working modes list
	 *
	 * If the constraint is a lower bound, this is placed equal to 0. If it
	 * is an upper bound it is set to the max AWM ID. The bitset is then
	 * restored accordingly.
	 */
	ExitCode_t RemoveWorkingModeConstraint(RTLIB_Constraint & constraint);

	/**
	 * @brief Check if an AWM violates resource constraints
	 *
	 * @param awm Shared pointer to the AWM object
	 *
	 * @return true if it violates, false otherwise
	 */
	bool UsageOutOfBounds(AwmPtr_t & awm);

	/**
	 * @brief Rebuild the list of enabled working modes
	 *
	 * This is needed whenever a constraint has been asserted or removed.
	 * The bitmap of the enabled is scanned, each set bit is related to the ID
	 * of the AWM to consider enabled. If the resources required by the AWM
	 * do not violate any resource constraint, the AWM is inserted into the
	 * list. Finally the list is ordered by "AWM value".
	 */
	void RebuildEnabledWorkingModes();

	/**
	 * @brief Finalize the changes in the enabled working modes list
	 *
	 * When the list of enabled working modes changes, due to constraint
	 * assertions, a couple of operations are required:
	 * 1) we must signal if the currently scheduled AWM has been invalidated.
	 * 2) the list must be re-ordered by AWM value
	 * This is what this method does.
	 */
	void FinalizeEnabledWorkingModes();

	/**
	 * @brief Clear the lower bound info from awm_range struct
	 */
	void ClearWorkingModesLowerBound();

	/**
	 * @brief Clear the lower bound info from awm_range struct
	 */
	void ClearWorkingModesUpperBound();

	/**
	 * @brief Update enabled working modes list
	 *
	 * Whenever a resource constraint is set or removed, the method is called
	 * in order to check if there are some working mode to disable or
	 * re-enable.  The method rebuilds the list of enabled working modes by
	 * checking if there are some working modes requiring a resource usage
	 * which is out of the bounds set by a resource constraint assertion.
	 */
	void UpdateEnabledWorkingModes();

	/**
	 * @brief Update the application state and sync state
	 *
	 * @param state the new application state
	 * @param sync the new synchronization state (SYNC_NONE by default)
	 */
	void SetState(State_t state, SyncState_t sync = SYNC_NONE);

	/**
	 * @brief Update the application synchronization state
	 */
	void SetSyncState(SyncState_t sync);

	/**
	 * @brief Request a synchronization of this application into the specied
	 * state.
	 *
	 * @param sync the new synchronization state (SYNC_NONE by default)
	 * @return @see ExitCode_t
	 */
	ExitCode_t RequestSync(SyncState_t sync);

	/**
	 * @brief Configure this application to switch to the specified AWM
	 * @return @see ExitCode_t
	 */
	ExitCode_t Reschedule(AwmPtr_t const & awm);

	/**
	 * @brief Configure this application to release resources.
	 * @return @see ExitCode_t
	 */
	ExitCode_t Unschedule();

	/**
	 * @brief Verify if a synchronization is required to move into the
	 * specified AWM.
	 *
	 * The method is called only if the Application is currently RUNNING.
	 * Compare the WorkingMode specified with the currently used by the
	 * Application. Compare the Resources set with the one binding the
	 * resources of the current WorkingMode, in order to check if the
	 * Application is going to run using processing elements from the same
	 * clusters used in the previous execution step.
	 *
	 * @param awm the target working mode
 	 *
	 * @return One of the following values:
	 * - RECONF: Application is being scheduled for using PEs from the same
	 *   clusters, but with a different WorkingMode.
	 * - MIGRATE: Application is going to run in the same WorkingMode, but
	 *   it’s going to be moved onto a different clusters set.
	 * - MIGREC: Application changes completely its execution profile. Both
	 *   WorkingMode and clusters set are different from the previous run.
	 * - SYNC_NONE: Nothing changes. Application is going to run in the same
	 *   operating point.
	 *
	 */
	SyncState_t SyncRequired(AwmPtr_t const & awm);

	/**
	 * @brief Check if this is a reshuffling
	 *
	 * Resources reshuffling happens when two resources bindings are not
	 * the same, i.e. different kind or amount of resources, while the
	 * application is not being reconfigured or migrated.
	 *
	 * This method check if the specified AWM will produce a reshuffling.
	 *
	 * @param next_awm the AWM to compare with current
	 * @return true if the specified next_awm will produce a resources
	 * reshuffling
	 */
	bool Reshuffling(AwmPtr_t const & next_awm);

	/**
	 * @brief Notify the EXC being run
	 *
	 * This method is called by ScheduleCommit once an EXC has been
	 * synchronized to the RUNNING state. It update the current EXC state to
	 * RUNNING
	 */
	ExitCode_t SetRunning();

	/**
	 * @brief Notify the EXC being blocked
	 *
	 * This method is called by ScheduleCommit once an EXC has been
	 * synchronized to the BLOCKED state. It update the current EXC state to
	 * either READY or FINISHED according to what happened meanwhile. If the
	 * EXC has been simply disabled, it becomes READY, while insted
	 */
	ExitCode_t SetBlocked();

};

} // namespace app

} // namespace bbque

#endif // BBQUE_APPLICATION_H
