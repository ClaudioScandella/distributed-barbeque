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

#ifndef BBQUE_RPC_H_
#define BBQUE_RPC_H_

#include "rtlib/rtlib.h"
#include "bbque/config.h"
#include "rtlib/rpc_messages.h"
#include "bbque/utils/stats.h"
#include "bbque/utils/utility.h"
#include "bbque/utils/timer.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/cpp11/condition_variable.h"
#include "bbque/cpp11/mutex.h"
#include "bbque/cpp11/thread.h"

#include "sys/times.h"

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
# include "bbque/utils/perf.h"
#endif

#ifdef CONFIG_BBQUE_RTLIB_EXECUTION_ANALYSER
#define STAT_LOG(fmt, ...) do {stat_logger->Info(fmt, ## __VA_ARGS__);} while (0)
#else
#define STAT_LOG(fmt, ...) do {} while (0)
#endif

#ifdef CONFIG_BBQUE_OPENCL
#include "rtlib/bbque_ocl_stats.h"
#endif

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/moment.hpp>
#include <boost/accumulators/statistics/variance.hpp>

// Define module namespace for inlined methods
#undef  MODULE_NAMESPACE
#define MODULE_NAMESPACE "rpc"

#define US_IN_A_SECOND 1e6

using namespace boost::accumulators;
namespace bu = bbque::utils;

namespace bbque
{
namespace rtlib
{

/**
 * @class BbqueRPC
 * @brief The class implementing the RTLib plain API
 * @ingroup rtlib_sec03_plain
 *
 * This RPC mechanism is channel agnostic (i.e. does not care how the actual
 * communication with the resource manager is performed) and defines the
 * procedures that are used to send requests to the Barbeque RTRM.  The actual
 * implementation of the communication channel is provided by class derived by
 * this one. This class provides also a factory method which allows to obtain
 * an instance to the concrete communication channel that can be selected at
 * compile time.
 */
class BbqueRPC
{

public:

	/**
	 * @brief Get a reference to the (singleton) RPC service
	 *
	 * This class is a factory of different RPC communication channels. The
	 * actual instance returned is defined at compile time by selecting the
	 * proper specialization class.
	 *
	 * @return A reference to a BbqueRPC isntance, or nullptr on error.
	 */
	static BbqueRPC * GetInstance();

	/**
	 * @brief Get a reference to the RTLib configuration
	 *
	 * All the run-time tunable and configurable RTLib options are hosted
	 * by the struct RTLib_Conf. This call returns a pointer to this
	 * configuration, which could not be update at run-time.
	 *
	 * @return A reference to the RTLib configuration options.
	 */
	static const RTLIB_Conf_t * Configuration()
	{
		return &rtlib_configuration;
	};

	/**
	 * @brief Release the RPC channel
	 */
	virtual ~BbqueRPC(void);

	/***********************************************************************
	 * Initialization and de-initialization
	 **********************************************************************/

	/**
	 * @brief Notify the BarbequeRTRM about application name and PID
	 * @param name Name to be assigned to the application
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t InitializeApplication(const char * name);

	/**
	 * @brief Register a new EXC to the BarbequeRTRM
	 *
	 * Each application spawns one or more Execution Contexts (EXC).
	 * Each execution context follow the BarbequeRTRM abstract execution
	 * model. In order to receive resources from the BarbequeRTRM, each
	 * EXC has to be registered and activated. This method registers
	 * an EXC.
	 *
	 * @see RTLIB_EXCParameters
	 * @param exc_name Name to be assigned to the EXC
	 * @param exc_params Parameters of the new EXC
	 * @return a handler for the EXC only if the EXC was not already
	 * registered and the registration succeeded, nullptr else.
	 */
	RTLIB_EXCHandler_t Register(const char * exc_name,
			const RTLIB_EXCParameters_t * exc_params);

	/**
	 * @brief Unregister an EXC from the BarbequeRTRM
	 *
	 * This causes the BarbequeRTRM to unregister the EXC, i.e., the EXC
	 * will be removed from the list of known EXCs.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void Unregister(const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Unregister all the application EXCs from the BarbequeRTRM
	 *
	 * This causes the BarbequeRTRM to unregister all the EXC associated
	 * to this application, i.e., the EXCs will be removed from the list
	 * of known EXCs.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void UnregisterAll();

	/**
	 * @brief Notify the BarbequeRTRM that the EXC is starting
	 *
	 * This causes the BarbequeRTRM to start allocating resources to
	 * this EXC
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t Enable(const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Unregister an EXC from the BarbequeRTRM
	 *
	 * This causes the BarbequeRTRM to unregister the EXC: the resources
	 * that are allocated to this EXC will be sized back, and it will not
	 * receive resources anymore. If it usually followed by the EXC
	 * un-registration
	 *
	 * @see Unregister function
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t Disable(const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Initialize CGroup support if enabled.
	 * @return RTLIB_OK on success or if CGroup support is disabled
	 */
	RTLIB_ExitCode_t CGroupCheckInitialization();

	/**
	 * @brief store the PID of a registered EXC (for logging purposes)
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t RegisterControlThreadPID(
			RTLIB_EXCHandler_t exc_handler);

	/***********************************************************************
	 * Resource allocation negotiation with the BarbequeRTRM
	 **********************************************************************/

	/**
	 * @brief Set constraints on the AWM choice for this EXC
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param awm_constraints A vector of constraints to be added
	 * @param number_of_constraints Length of the constraints vector
	 * @see RTLIB_Constraint_t
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetAWMConstraints(
			const RTLIB_EXCHandler_t exc_handler,
			RTLIB_Constraint_t * awm_constraints,
			uint8_t number_of_constraints);

	/**
	 * @brief Remove all constraints on the AWM choice for this EXC
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t ClearAWMConstraints(
			const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Send performance feedback to the BarbequeRTRM
	 *
	 * This method triggers a Runtime Profile forward to the BarbequeRTRM.
	 * The Runtime Profile is a summary of the current application
	 * performance, as opposed to the performance goal declared by the EXC.
	 * This information is used by the BarbequeRTRM to refine the EXC
	 * resource allocation; therefore, the Profile forward will likely
	 * trigger a re-schedule for this EXC.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t ForwardRuntimeProfile(
			const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Compute the ideal allocation for the application, enforce it
	 * if possible, schedule a runtime profile forward if current resources
	 * are not ideal.
	 *
	 * @see ForwardRuntimeProfile
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t UpdateAllocation(
			const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief explicitely notify EXC satisfaction to the BarbequeRTRM.
	 *
	 * Explicit goal gap is a percentage, i.e., goal_gap = +/- X means
	 * that EXC is X% too fast/slow.
	 *
	 * Here, the value is only set; it will be sent to the BarbequeRTRM the
	 * next time the RPC will check if forwarding a new runtime profile
	 * is needed
	 *
	 * @see ForwardRuntimeProfile
	 * @param exc_handler Handler of the target Execution Context
	 * @param goal_gap_percent percent satisfaction
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetExplicitGoalGap(
			const RTLIB_EXCHandler_t exc_handler,
			int goal_gap_percent);

	/**
	 * @brief Get an Application Working Mode from the BarbequeRTRM
	 *
	 * Get a valid Application Working Mode for this EXC. If the EXC already
	 * got one, the method does nothing. Else, it triggers a schedule
	 * request to the BarbequeRTRM.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param working_mode_params Parameters of the current AWM
	 * @param sync_type Type of synchronization. If not sure, use 0
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t GetWorkingMode(
			RTLIB_EXCHandler_t exc_handler,
			RTLIB_WorkingModeParams_t * working_mode_params,
			RTLIB_SyncType_t sync_type);

	/**
	 * @brief Send to BarbequeRTRM the OpenCL runtime profile of this EXC
	 *
	 * This is a passive method. That is, you cannot invoke it explicitely:
	 * it is automatically called by the communication module whenever the
	 * BarbequeRTRM sends a get request. Please, do not invoke this method
	 * explicitely: it will try to send a message to the BarbequeRTRM, but,
	 * given that the BarbequeRTRM is not expecting that message, the
	 * communication will likely result in a timeout.
	 *
	 * @param msg The BarbequeRTRM message that triggered this action
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SendOCLRuntimeProfile(rpc_msg_BBQ_GET_PROFILE_t & msg);

	/**
	 * @brief Get a breakdown of the allocated resources
	 * @param exc_handler Handler of the target Execution Context
	 * @param working_mode_params Parameters of the current AWM
	 * @param r_type Type of resources you are interested in
	 * @param r_amount Amount of allocated resources will be stored here.
	 * "-1" means "error".
	 * @return RTLIB_OK if the EXC is registered and already received
	 * a working mode.
	 */
	RTLIB_ExitCode_t GetAssignedResources(
			RTLIB_EXCHandler_t exc_handler,
			const RTLIB_WorkingModeParams_t * working_mode_params,
			RTLIB_ResourceType_t r_type,
			int32_t & r_amount);

	/**
	 * @brief Get the list of allocated processing elements IDs.
	 *
	 * Each element of the specified vector will be set to the ID of
	 * an allocated processing element. If the vector size is greater
	 * than the number of allocated processing elements, the surplus
	 * elements are set to -1. On error, all the elements are set to -1.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param ids_vector  Vector to be filled with the proc elements IDs
	 * @param vector_size Size of the vector
	 * @return RTLIB_OK if the EXC is registered.
	 */
	RTLIB_ExitCode_t GetAffinityMask(
			RTLIB_EXCHandler_t exc_handler,
			int32_t * ids_vector,
			int vector_size);

	/**
	 * @brief Get amount of allocated resources
	 *
	 * Get the amount of allocated resources for a given resource
	 * type for each allocated system. The resource amounts are stored
	 * in the sys_array argument (-1 on error).
	 *
	 * @see RTLIB_ResourceType_t
	 * @param exc_handler Handler of the target Execution Context
	 * @param working_mode_params Parameters of the current AWM
	 * @param r_type The type of resource to be reported
	 * @param sys_array Array of integers (one integer per system)
	 * @param array_size Size of the systems array
	 * @return RTLIB_OK if the EXC is registered and got resources
	 */
	RTLIB_ExitCode_t GetAssignedResources(
			RTLIB_EXCHandler_t exc_handler,
			const RTLIB_WorkingModeParams_t * working_mode_params,
			RTLIB_ResourceType_t r_type,
			int32_t * sys_array,
			uint16_t array_size);

	/**
	 * @brief Start monitoring performance counters for this EXC
	 * @param exc_handler Handler of the target Execution Context
	 */
	void StartPCountersMonitoring(RTLIB_EXCHandler_t exc_handler);

	/***********************************************************************
	 *	Utility Functions
	 **********************************************************************/

	/**
	 * @brief Build a Control Group path for this EXC.
	 *
	 * The path is created using application name and execution context
	 * ID/PID. This method only create the path, which is a string.
	 * Control Group will be created using this path.
	 *
	 * @see CGroupCreate
	 * @param exc_handler Handler of the target Execution Context
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetupCGroup(const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Get application UID as a char vector
	 * @return the application UID (char*)
	 */
	inline const char * GetCharUniqueID() const
	{
		return channel_thread_unique_id;
	}

	/**
	 * @brief Get the EXC UID as an integer
	 * @param exc_handler Handler of the target Execution Context
	 * @return the EXC UID
	 */
	AppUid_t GetUniqueID(RTLIB_EXCHandler_t exc_handler);

	/***********************************************************************
	 *	Cycles Per Second (CPS) Control
	 **********************************************************************/

	/**
	 * @brief Set the required Cycles Per Second (CPS)
	 *
	 * This method allows an EXC to define its maximum Cycles per Second
	 * rate. If the runtime cycle rate is faster than that, the Runtime
	 * Library will inject sleeps and make it slower.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param cps the value of the desired maximum CPS
	 * @see SetCPSGoal
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetCPS(RTLIB_EXCHandler_t exc_handler, float cps);

	/**
	 * @brief Get the average Cycles Per Second (CPS) value
	 *
	 * This allows to retrieve the average CPS value for the EXC, that is,
	 * the average number of cycles per second up to now.
	 *
	 * @see SetMinimumCycleTimeUs
	 * @param exc_handler Handler of the target Execution Context
	 * @return the measured CPS value
	 */
	float GetCPS(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Get the average Jobs Per Second (CPS) value
	 *
	 * This allows to retrieve the average JPS value for the EXC, that is,
	 * the average number of jobs per second up to now.
	 *
	 * JPS is equal to CPS multiplied by the number of jobs the application
	 * is executing each cycle.
	 *
	 * @see GetCPS UpdateJPC
	 * @param exc_handler Handler of the target Execution Context
	 * @return the measured CPS value
	 */
	float GetJPS(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Set the required Cycles Per Second rate (CPS)
	 *
	 * This method allows the EXC to define a cycle rate goal.
	 * This information is used by the Runtime Library to negotiate resource
	 * allocation with the BarbequeRTRM, so that the allocated resources
	 * will be enough for the EXC to reach its performance goal.
	 *
	 * Note: with the term "cycle" we refer to an EXC execution cycle,
	 * i.e., onConfigure + onRun + onMonitor
	 *
	 * CPS goal is preferable to JPS goal when more resources means
	 * shorter cycle time rather than more jobs per cycle
	 *
	 * @see SetJPSGoal
	 * @param exc_handler Handler of the target Execution Context
	 * @param cps_min Minimum acceptable cycle rate
	 * @param cps_max Maximum acceptable cycle rate
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetCPSGoal(
			RTLIB_EXCHandler_t exc_handler,
			float cps_min,
			float cps_max);

	/**
	 * @brief Reset performance information for this EXC
	 *
	 * Runtime profiles are sent by the Runtime Library to the BarbequeRTRM
	 * to negotiate resource allocation. This method reset such information,
	 * so that future runtime profiles will not include any performance
	 * statistics up to now.
	 *
	 * If new_user_goal is true, performance statistics will be erased also
	 * from the user profile, that is, from the performance data that is
	 * available to the EXC. This usually makes sense if the EXC just
	 * declared a new performance goal, and it is no more interested of
	 * the past performance statistics.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param new_user_goal Whether user profile must also be re-set
	 */
	void ResetRuntimeProfileStats(
			RTLIB_EXCHandler_t exc_handler,
			bool new_user_goal = false);

	/**
	 * @brief Set the required Cycles Per Second rate (JPS)
	 *
	 * This method allows the EXC to define a jobs rate goal.
	 * This information is used by the Runtime Library to negotiate resource
	 * allocation with the BarbequeRTRM, so that the allocated resources
	 * will be enough for the EXC to reach its performance goal.
	 *
	 * JPS goal is preferable to CPS goal when more resources means
	 * more jobs per cycle rather than shorter cycle time
	 *
	 * @see SetCPSGoal
	 *
	 * Note: If the EXC changes the number of Jobs per Cycle it is
	 * processing, it should notify it to the Runtime Library by using
	 * the UpdateJPC method
	 *
	 * @see UpdateJPC
	 * @param exc_handler Handler of the target Execution Context
	 * @param jps_min Minimum acceptable cycle rate
	 * @param jps_max Maximum acceptable cycle rate
	 * @param Current number of jobs executed per cycle
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetJPSGoal(
			RTLIB_EXCHandler_t exc_handler,
			float jps_min,
			float jps_max,
			int jpc);

	/**
	 * @brief Updates Jobs Per Cycle value, which is used to compute JPS
	 *
	 * The number of processes jobs per cycle is needed by the Runtime
	 * Library to translate JPS goals into CPS goals
	 *
	 * @see SetJPSGoal, SetCPSGoal
	 * @param exc_handler Handler of the target Execution Context
	 * @param jpc New number of jobs per cycle
	 *
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t UpdateJPC(
			RTLIB_EXCHandler_t exc_handler,
			int jpc);

	/**
	 * @brief Set the minimum Cycle time [us]
	 *
	 * This allows the EXC to define the required minimum cycle time. If at
	 * run-time the cycles execution is faster than that, the Runtime
	 * Library will inject a delay to make the EXC comply with the minimum
	 * cycle time.
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param max_cycle_time_us Minimum cycletime value in microseconds
	 *
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetMinimumCycleTimeUs(
			RTLIB_EXCHandler_t exc_handler,
			uint32_t max_cycle_time_us)
	{
		return SetCPS(exc_handler,
			static_cast<float>(US_IN_A_SECOND) / max_cycle_time_us);
	}

	/***********************************************************************
	 *	Performance Monitoring Support
	 **********************************************************************/

	/**
	 * @brief Execute the pre-EXC-termination procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void NotifyExit(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Execute the pre-EXC-configuration procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void NotifyPreConfigure(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Execute the post-EXC-configuration procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void NotifyPostConfigure(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Execute the pre-EXC-run procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void NotifyPreRun(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Execute the post-EXC-run procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void NotifyPostRun(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Execute the pre-EXC-monitor procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 */
	void NotifyPreMonitor(RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Execute the post-EXC-monitor procedure
	 *
	 * Mostly, statistics collection and dump
	 *
	 * @param exc_handler Handler of the target Execution Context
	 * @param is_last_cycle Whether it is the last cycle and performance
	 * feedbacks should not be sent to the BarbequeRTRM anymore
	 */
	void NotifyPostMonitor(
			RTLIB_EXCHandler_t exc_handler,
			bool is_last_cycle);

protected:

	/** Instance of the BarbequeRTRM logger*/
	static std::unique_ptr<bu::Logger> logger;

#ifdef CONFIG_BBQUE_RTLIB_EXECUTION_ANALYSER
	/**
	 * Additional instance of the BarbequeRTRM logger, for stat log
	 * purposes
	 */
	static std::unique_ptr<bu::Logger> stat_logger;
#endif // CONFIG_BBQUE_RTLIB_EXECUTION_ANALYSER

	/**
	 * @brief Performance counter event descriptor
	 * @see pPerfEventAttr_t
	 */
	typedef struct PerfEventAttr {
#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		perf_type_id type;
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		uint64_t config;
	} PerfEventAttr_t;

	/**
	 * @brief Pointer to a performance counter event descriptor
	 * @see PerfEventAttr, PerfRegisteredEventsMap_t
	 */
	typedef PerfEventAttr_t * pPerfEventAttr_t;

	/**
	 * @brief Map of the perf counters events that are to be monitored
	 * @see pPerfEventAttr_t
	 */
	typedef std::map<int, pPerfEventAttr_t> PerfRegisteredEventsMap_t;

	/**
	 * Structure containing collected performance counters values for
	 * a perf event
	 *
	 * @see pPerfEventAttr_t
	 */
	typedef	struct PerfEventStats {
		/** Per AWM perf counter value */
		uint64_t value;
		/** Per AWM perf counter enable time */
		uint64_t time_enabled;
		/** Per AWM perf counter running time */
		uint64_t time_running;
		/** Perf counter attrs */
		pPerfEventAttr_t pattr;
		/** Perf counter ID */
		int id;
		/** The statistics collected for this performance event */
		accumulator_set<
				uint32_t,
				stats<tag::min, tag::max, tag::variance>
			> perf_samples;

	} PerfEventStats_t;

	/**
	 * @brief Shared pointer to the statistics of a performance event
	 * @see PerfEventStats_t
	 */
	typedef std::shared_ptr<PerfEventStats_t> pPerfEventStats_t;

	/**
	 * @brief Map containing all the registered performance events
	 * @see pPerfEventStats_t, PerfEventStats_t
	 */
	typedef std::map<int, pPerfEventStats_t> PerfEventStatsMap_t;

	/**
	 * @brief Map of the registered perf events, ordered by configuration
	 * @see pPerfEventStats_t, PerfEventStatsMapByConfEntry_t
	 */
	typedef std::multimap<
			uint8_t,
			pPerfEventStats_t> PerfEventStatsMapByConf_t;

	/**
	 * Utility pair used to insert items in the config-wise perf events map
	 * @see PerfEventStatsMapByConf_t
	 */
	typedef std::pair<
			uint8_t,
			pPerfEventStats_t> PerfEventStatsMapByConfEntry_t;

	/**
	 * Structure containing CPU usage statistics intialized before and
	 * collected after the EXC run phase
	 *
	 * @see NotifyPreRun, NotifyPostRun
	 */
	struct CpuUsageStats {
		/** Temporary storage for timestamp samples */
		struct tms time_sample;
		/** Total time for previous sample*/
		clock_t previous_time;
		/** System time for previous sample*/
		clock_t previous_tms_stime;
		/** User time for previous sample*/
		clock_t previous_tms_utime;
		/** Total time for this sample*/
		clock_t current_time;
	};

	/**
	 * @brief Statistics on AWM usage
	 *
	 * This struct stores statistics about an Application Working Mode:
	 * number of times it was chosen, timings for configure/run/monitor
	 * phases, performance counters events if counters monitoring is enabled
	 */
	typedef struct AwmStats {
		/** Count of times this AWM has been in used */
		uint32_t number_of_uses;
		/** The time [ms] spent on processing into this AWM */
		uint32_t time_spent_processing;
		/** The time [ms] spent on monitoring this AWM */
		uint32_t time_spent_monitoring;
		/** The time [ms] spent on configuring this AWM */
		uint32_t time_spent_configuring;

		/** Statistics on AWM cycles */
		accumulator_set<
				double,
				stats<tag::min, tag::max, tag::variance>
			> cycle_samples;

		/** Statistics on ReConfiguration Overheads */
		accumulator_set<
				double,
				stats<tag::min, tag::max, tag::variance>
			> config_samples;

		/** Statistics on Monitoring Overheads */
		accumulator_set<
				double,
				stats<tag::min, tag::max, tag::variance>
			> monitor_samples;

		/** Statistics on time spent running */
		accumulator_set<
				double,
				stats<tag::min, tag::max, tag::variance>
			> run_samples;

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		/** Map of registered Perf counters */
		PerfEventStatsMap_t events_map;
		/** Map registered Perf counters to their type */
		PerfEventStatsMapByConf_t events_conf_map;
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT

#ifdef CONFIG_BBQUE_OPENCL
		/** Map of OpenCL profiling info */
		OclEventsStatsMap_t ocl_events_map;
#endif // CONFIG_BBQUE_OPENCL

		/** Mutex protecting concurrent access to statistical data */
		std::mutex stats_mutex;

		AwmStats() :
			number_of_uses(0),
			time_spent_processing(0),
			time_spent_monitoring(0),
			time_spent_configuring(0) {};

	} AwmStats_t;


	/**
	 * @brief A pointer to AWM statistics
	 * @see AwmStats_t
	 */
	typedef std::shared_ptr<AwmStats_t> pAwmStats_t;

	/**
	 * @brief Map AWMs ID into its statistics
	 */
	typedef std::map<uint8_t, pAwmStats_t> AwmStatsMap_t;

	/**
	 * @brief A pointer to the system resources
	 * @see RTLIB_SystemResources_t
	 */
	typedef std::shared_ptr<RTLIB_SystemResources_t> pSystemResources_t;

	/**
	 * @brief Map system id - system resources
	 */
	typedef std::map<uint16_t, pSystemResources_t> SysResMap_t;

	/**
	 * @brief All the information relative to a registered EXC
	 *
	 * This struct stores information regarding one Execution Context:
	 * name, status, timings, performance counters information (if
	 * monitoring is active), current AWM, maximum resource allocation as
	 * chosen by the BarbequeRTRM, current performance and performance
	 * goals.
	 *
	 * In short, this is a running EXC in a nutshell.
	 */
	typedef struct RegisteredExecutionContext {
		/** The Execution Context data */
		RTLIB_EXCParameters_t parameters;
		/** The name of this Execuion Context */
		std::string name;
		/** The RTLIB assigned ID for this Execution Context */
		uint8_t id;
		/** The PID of the control thread managing this EXC */
		pid_t control_thread_pid = 0;

#ifdef CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT
		/** The path of the CGroup for this EXC */
		std::string cgroup_path;
#endif // CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT

#define EXC_FLAGS_AWM_VALID	  0x01 ///< The EXC has been assigned a valid AWM
#define EXC_FLAGS_AWM_WAITING	0x02 ///< The EXC is waiting for a valid AWM
#define EXC_FLAGS_AWM_ASSIGNED   0x04 ///< The EXC is waiting for a valid AWM
#define EXC_FLAGS_EXC_SYNC	   0x08 ///< The EXC entered Sync Mode
#define EXC_FLAGS_EXC_SYNC_DONE  0x10 ///< The EXC exited Sync Mode
#define EXC_FLAGS_EXC_REGISTERED 0x20 ///< The EXC is registered
#define EXC_FLAGS_EXC_ENABLED	0x40 ///< The EXC is enabled
#define EXC_FLAGS_EXC_BLOCKED	0x80 ///< The EXC is blocked

		/** A set of flags to define the state of this EXC */
		uint8_t flags = 0x00;
		/** The last required synchronization action */
		RTLIB_ExitCode_t event = RTLIB_OK;
		/** The ID of the assigned AWM (if valid) */
		int8_t current_awm_id = 0;

		/** Whether EXC must reconfigure to exploit this allocation */
		bool trigger_reconfigure = false;

		/** Resource allocation for each system **/
		SysResMap_t resource_assignment;

		/** The mutex protecting access to this structure */
		std::mutex exc_mutex;
		/** Notified on changes for this EXC */
		std::condition_variable exc_condition_variable;

		/** The High-Resolution timer used for profiling */
		bu::Timer execution_timer;

		/** The time [ms] latency to start the first execution */
		uint32_t starting_time_ms   = 0;
		/** The time [ms] spent on waiting for an AWM being assigned */
		uint32_t blocked_time_ms    = 0;
		/** The time [ms] spent on reconfigurations */
		uint32_t config_time_ms     = 0;
		/** The time [ms] spent on processing */
		uint32_t processing_time_ms = 0;

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		/** Performance counters */
		bu::Perf perf;
		/** Map of registered Perf counter IDs */
		PerfRegisteredEventsMap_t events_map;
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT

		/** Overall cycles for this EXC */
		uint64_t cycles_count = 0;
		/** Statistics on AWM's of this EXC */
		AwmStatsMap_t awm_stats;
		/** Statistics of currently selected AWM */
		pAwmStats_t current_awm_stats;

#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
		/**
		 * Maximum allocation according to the BarbequeRTRM.
		 * Enforced allocation must always be less or equal to this
		 * amount. If more resources are needed, the Runtime Library
		 * will sent a request to the BarbequeRTRM.
		 *
		 * CPU allocation is represented globally, and isolation-wise.
		 * an isolated resource is a resource that has been allocated
		 * only to this EXC, i.e., it can be used in isolation.
		 *
		 * Allocations that are expressed in string format are used
		 * to interact with the libcgroup, which only accepts char*
		 * for CGroup writes.
		 *
		 * @see ForwardRuntimeProfile
		 */
		struct CGroupBudgetInfo {
			/** CPU Bandwidth on exclusively allocated cores */
			float cpu_budget_isolation = 0.0;
			/** Total CPU Bandwidth */
			float cpu_budget_shared = 0.0;
			/** IDs of all the allocated cores */
			std::vector<int32_t> cpu_global_ids;
			/** IDs of the exclusively allocated cores */
			std::vector<int32_t> cpu_isolation_ids;

			// String representations for libcgroup ////////////////

			/** Maximum available memory [byte] */
			std::string memory_limit_bytes;
			/** IDs of the exclusively allocated cores */
			std::string cpuset_cpus_isolation;
			/** IDs of all the allocated cores */
			std::string cpuset_cpus_global;
			/** Momory nodes IDs (0 if system is not not NUMA)*/
			std::string cpuset_mems;
		} cg_budget;

		/**
		 * Max resource usage as enforced by the Runtime Library. It
		 * is always less or equal to the resource budget allocated
		 * by the BarbequeRTRM.
		 *
		 * @see CGroupBudgetInfo
		 */
		struct CGroupAllocationInfo {
			/** Currently allocated CPU bandwidth */
			float cpu_budget = 0.0;
			/** IDs of all the currently allocated cores */
			std::vector<int32_t> cpu_affinity_mask;

			// String representations for libcgroup ////////////////

			/** Maximum available memory [byte] */
			std::string memory_limit_bytes;
			/** IDs of all the currently allocated cores */
			std::string cpuset_cpus;
			/** IDs of all the currently allocated memory nodes */
			std::string cpuset_mems;
		} cg_current_allocation;
#endif // CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION

		/**
		 * Runtime Profile of the application. It contains all the
		 * information needed by the scheduling policy to refine
		 * resource allocation. If you want to extend Runtime Profile
		 * information, the additional data must be put here.
		 */
		struct RT_Profile {
			/** How much more/less CPU quota is needed by this EXC*/
			float cpu_goal_gap = 0.0f;
			/**
			 * Whether the current information must be forwarded to
			 * the BarbequeRTRM. If true, the profile will be
			 * forwarded during the next PostMonitor.
			 *
			 * @see NotifyPostMonitor, ForwardRuntimeProfile
			 */
			bool rtp_forward = false;
		} runtime_profiling;

		/** Start time of the lase EXC configuration phase*/
		double configure_tstart_ms = 0.0;
		/** Start time of the lase EXC monitoring phase*/
		double monitor_tstart_ms   = 0.0;
		/** Start time of the lase EXC run phase*/
		double run_tstart_ms	   = 0.0;

		/**
		 * [Hz] the maximum CPS to be enforced
		 * @see SetCPS, SetMinimumCycleTimeUs, ForceCPS
		 */
		float cps_max_allowed = 0.0;

		/**
		 * [ms] the minimum cycle time to be enforced
		 * @see cps_max_allowed
		 */
		float cycle_time_enforced_ms = 0.0;

		/**
		 * [Hz] the minimum required CPS
		 * @see SetCPSGoal
		 */
		float cps_goal_min = 0.0;

		/**
		 * [Hz] the maximum required CPS
		 * @see SetCPSGoal
		 */
		float cps_goal_max = 0.0;

		/**
		 * Current number of processed Jobs per Cycle
		 * @see UpdateJPC, SetJPSGoal
		 */
		int jpc = 1;

		/**
		 * Moving Statistics for onConfigure method
		 * @see NotifyPostMonitor
		 */
		bu::StatsAnalysis time_analyser_configure;
		/**
		 * Moving Statistics for onMonitor method
		 * @see NotifyPostMonitor
		 */
		bu::StatsAnalysis time_analyser_monitor;
		/**
		 * Moving Statistics for onRun method
		 * @see NotifyPostMonitor
		 */
		bu::StatsAnalysis time_analyser_run;

		/**
		 * Moving Statistics for real cycle time [ms]
		 * It does not include ForceCPS delay, if any
		 * @see NotifyPostMonitor
		 */
		bu::StatsAnalysis time_analyser_cycle;

		/**
		 * Moving Statistics for user cycle time [ms]
		 * It includes ForceCPS delay, if any
		 * @see NotifyPostMonitor
		 */
		bu::StatsAnalysis time_analyser_usercycle;

		/**
		 * The cycletime statistics accumulator may be reset, usually
		 * due to a performance goal change. During the first Run phase
		 * after the reset, cycletime for the EXC is not available.
		 * Should the EXC query for its cycletime, the Runtime Library
		 * would return this value.
		 */
		double average_cycletime_pre_reset_ms = 0.0;

		/**
		 * The Runtime Profile for the EXC is automatically computed
		 * by the Runtime Library; however, the EXC is able to
		 * explicitely define the value for its goal gap (i.e., the
		 * percent statisfaction with the current resource allocation).
		 * In this case, we talk about "explicit goal gap assertion".
		 * Adter each Execution Cycle, the Runtime Library forwards the
		 * EXC Runtime Profile to the BarbequeRTRM either if the current
		 * performance is not satisfactory or if the EXC explicitely
		 * asserted its goal gap.
		 *
		 * @see ForwardRuntimeProfile
		 */
		float 	explicit_ggap_value = 0.0;

		/** Whether a goal gap was explicitely asserted */
		bool 	explicit_ggap_assertion = false;

		/**
		 * Once a Runtime Profile has been forwarded to the
		 * BarbequeRTRM, there is no need to forward additional
		 * Profiles: the Runtime Library should first wait for the
		 * BarbequeRTRM to change the resource allocation.
		 * However, despite having received the Profile information,
		 * the BarbequeRTRM could choose not to change the current
		 * resource allocation (e.g. there are not enough resources
		 * at the moment). This variable defines how many milliseconds
		 * the Runtime Library should wait for a new allocation before
		 * choosing to restart forwarding Porfiles to the BarbequeRTRM.
		 *
		 * @see ForwardRuntimeProfile
		 */
		int waiting_sync_timeout_ms = 0;

		/** Whether Profile forwarding is inhibited due to sync wait*/
		bool is_waiting_for_sync = false;

		/** Stores timestamps. Used to compute CPU usage of the EXC */
		CpuUsageStats cpu_usage_info;
		/** Statistical analyser for CPU usage*/
		bu::StatsAnalysis cpu_usage_analyser;

		RegisteredExecutionContext(const char * _name, uint8_t id) :
			name(_name), id(id) {}

		~RegisteredExecutionContext()
		{
			awm_stats.clear();
			current_awm_stats = pAwmStats_t();
		}

	} RegisteredExecutionContext_t;

	typedef std::shared_ptr<RegisteredExecutionContext_t> pRegisteredEXC_t;

	/**
	 * @brief Perf Statistics toggling
	 *
	 * Performance counter statistics can be monitored either during the
	 * EXC Run phase to analyse the EXC execution, or they can be monitored
	 * during the the rest of the time (Configure, Monitor, communication
	 * with the BarbequeRTRM) to analyse the EXC management flow. Right
	 * before and after the Run phase, performance counters monitoring
	 * is turned on/off accordingly.
	 *
	 * @see TogglePerfCountersPostCycle, NotifyPreRun, NotifyPostRun
	 */
	void TogglePerfCountersPreCycle(pRegisteredEXC_t exc);

	/**
	 * @brief Perf Statistics toggling
	 *
	 * Performance counter statistics can be monitored either during the
	 * EXC Run phase to analyse the EXC execution, or they can be monitored
	 * during the the rest of the time (Configure, Monitor, communication
	 * with the BarbequeRTRM) to analyse the EXC management flow. Right
	 * before and after the Run phase, performance counters monitoring
	 * is turned on/off accordingly.
	 *
	 * @see TogglePerfCountersPreCycle, NotifyPreRun, NotifyPostRun
	 */
	void TogglePerfCountersPostCycle(pRegisteredEXC_t exc);

	/**
	 * @brief  Check if this EXC got a valid AWM
	 * @param exc Pointer to registered EXC data
	 * @return true if the AWM is valid
	 */
	inline bool isAwmValid(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_AWM_VALID);
	}

	/**
	 * @brief Mark this EXC as "having a valid AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void setAwmValid(pRegisteredEXC_t exc) const
	{
		logger->Debug("AWM  <= Valid [%d:%s:%d]",
				exc->id,
				exc->name.c_str(),
				exc->current_awm_id);

		exc->flags |= EXC_FLAGS_AWM_VALID;
	}

	/**
	 * @brief Mark this EXC as "not having a valid AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearAwmValid(pRegisteredEXC_t exc) const
	{
		logger->Debug("AWM  <= Invalid [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_AWM_VALID;
	}

	/**
	 * @brief  Check if EXC is waiting for an AWM
	 * @param exc Pointer to registered EXC data
	 * @return true if the EXC is waiting for an AWM
	 */
	inline bool isAwmWaiting(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_AWM_WAITING);
	}

	/**
	 * @brief Mark this EXC as "currently waiting for an AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void setAwmWaiting(pRegisteredEXC_t exc) const
	{
		logger->Debug("AWM  <= Waiting [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags |= EXC_FLAGS_AWM_WAITING;
	}

	/**
	 * @brief Mark this EXC as "not being waiting for an AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearAwmWaiting(pRegisteredEXC_t exc) const
	{
		logger->Debug("AWM  <= NOT Waiting [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_AWM_WAITING;
	}

	/**
	 * @brief Check if this EXC got an AWM
	 * @param exc Pointer to registered EXC data
	 * @return true if EXC got an AWM
	 */
	inline bool isAwmAssigned(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_AWM_ASSIGNED);
	}

	/**
	 * @brief Mark this EXC as "having been assigned an AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void setAwmAssigned(pRegisteredEXC_t exc) const
	{
		logger->Debug("AWM  <= Assigned [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags |= EXC_FLAGS_AWM_ASSIGNED;
	}

	/**
	 * @brief Mark this EXC as "not having been assigned an AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearAwmAssigned(pRegisteredEXC_t exc) const
	{
		logger->Debug("AWM  <= NOT Assigned [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_AWM_ASSIGNED;
	}

	/**
	 * @brief Check of this EXC is reconfiguring to use a new AWM
	 * @param exc Pointer to registered EXC data
	 * @return true if EXC is reconfiguraing (aka syncing)
	 */
	inline bool isSyncMode(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_EXC_SYNC);
	}

	/**
	 * @brief Mark this EXC as "reconfiguring to use a new AWM"
	 * @param exc Pointer to registered EXC data
	 */
	inline void setSyncMode(pRegisteredEXC_t exc) const
	{
		logger->Debug("SYNC <= Enter [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags |= EXC_FLAGS_EXC_SYNC;
	}

	/**
	 * @brief Mark this EXC as "not reconfiguring"
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearSyncMode(pRegisteredEXC_t exc) const
	{
		logger->Debug("SYNC <= Exit [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_EXC_SYNC;
	}

	/**
	 * @brief Check if this EXC has finished reconfiguring (syncing)
	 * @param exc Pointer to registered EXC data
	 * @return true if reconfiguration is done
	 */
	inline bool isSyncDone(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_EXC_SYNC_DONE);
	}

	/**
	 * @brief Mark this EXC as "finished reconfiguring"
	 * @param exc Pointer to registered EXC data
	 */
	inline void setSyncDone(pRegisteredEXC_t exc) const
	{
		logger->Debug("SYNC <= Done [%d:%s:%d]",
				exc->id,
				exc->name.c_str(),
				exc->current_awm_id);

		exc->flags |= EXC_FLAGS_EXC_SYNC_DONE;
	}

	/**
	 * @brief Mark this EXC as "under reconfiguration"
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearSyncDone(pRegisteredEXC_t exc) const
	{
		logger->Debug("SYNC <= Pending [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_EXC_SYNC_DONE;
	}

	/**
	 * @brief Check if this EXC is registered
	 *
	 * EXC registered means that the BarbequeRTRM actually knows that the
	 * EXC exsists. The EXC will not receive any resources until it is also
	 * enabled.
	 *
	 * @param exc Pointer to registered EXC data
	 * @return  true if EXC is registered
	 */
	inline bool isRegistered(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_EXC_REGISTERED);
	}

	/**
	 * @brief Mark this EXC as registered
	 *
	 * This method only sets the flag. Obviously, to register the EXC,
	 * the Runtime Library must communicate with the BarbequeRTRM
	 *
	 * @see Register function
	 * @param exc Pointer to registered EXC data
	 */
	inline void setRegistered(pRegisteredEXC_t exc) const
	{
		logger->Debug("EXC  <= Registered [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags |= EXC_FLAGS_EXC_REGISTERED;
	}

	/**
	 * @brief Mark this EXC as not registered
	 *
	 * This method only unsets the flag. Obviously, to unregister the EXC,
	 * the Runtime Library must communicate with the BarbequeRTRM
	 *
	 * @see Unregister function
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearRegistered(pRegisteredEXC_t exc) const
	{
		logger->Debug("EXC  <= Unregistered [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_EXC_REGISTERED;
	}

	/**
	 * @brief Check if this EXC is enabled
	 *
	 * EXC enabled means that the BarbequeRTRM actually knows that it must
	 * allocate resources to this EXC.
	 *
	 * @param exc Pointer to registered EXC data
	 * @return  true if EXC is enabled
	 */
	inline bool isEnabled(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_EXC_ENABLED);
	}

	/**
	 * @brief Mark this EXC as enabled
	 *
	 * This method only sets the flag. Obviously, to enable the EXC,
	 * the Runtime Library must communicate with the BarbequeRTRM
	 *
	 * @see Enable function
	 * @param exc Pointer to registered EXC data
	 */
	inline void setEnabled(pRegisteredEXC_t exc) const
	{
		logger->Debug("EXC  <= Enabled [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags |= EXC_FLAGS_EXC_ENABLED;
	}

	/**
	 * @brief Mark this EXC as disabled
	 *
	 * This method only unsets the flag. Obviously, to disable the EXC,
	 * the Runtime Library must communicate with the BarbequeRTRM
	 *
	 * @see Disable function
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearEnabled(pRegisteredEXC_t exc) const
	{
		logger->Debug("EXC  <= Disabled [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_EXC_ENABLED;
	}

	/**
	 * @brief Check if this EXC is blocked
	 *
	 * EXC blocked means that the EXC is currently stopped, and it is
	 * waiting for the BarbequeRTRM to signal that the EXC can resume its
	 * execution.
	 *
	 * @param exc Pointer to registered EXC data
	 * @return  true if EXC is blocked
	 */
	inline bool isBlocked(pRegisteredEXC_t exc) const
	{
		return (exc->flags & EXC_FLAGS_EXC_BLOCKED);
	}

	/**
	 * @brief Mark this EXC as blocked
	 *
	 * This method only sets the flag. Obviously, to block the EXC,
	 * the BarbequeRTRM must communicate with the Runtime Library
	 *
	 * @param exc Pointer to registered EXC data
	 */
	inline void setBlocked(pRegisteredEXC_t exc) const
	{
		logger->Debug("EXC  <= Blocked [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags |= EXC_FLAGS_EXC_BLOCKED;
	}

	/**
	 * @brief Mark this EXC as not blocked
	 *
	 * This method only unsets the flag. Obviously, to unblock the EXC,
	 * the BarbequeRTRM must communicate with the Runtime Library
	 *
	 * @param exc Pointer to registered EXC data
	 */
	inline void clearBlocked(pRegisteredEXC_t exc) const
	{
		logger->Debug("EXC  <= UnBlocked [%d:%s]",
				exc->id,
				exc->name.c_str());

		exc->flags &= ~EXC_FLAGS_EXC_BLOCKED;
	}

	/***********************************************************************
	 * OpenCL support
	 **********************************************************************/
#ifdef CONFIG_BBQUE_OPENCL
	void OclSetDevice(uint8_t device_id, RTLIB_ExitCode_t status);
	void OclClearStats();
	void OclCollectStats(
		int8_t current_awm_id, OclEventsStatsMap_t & ocl_events_map);
	void OclPrintCmdStats(QueueProfPtr_t, cl_command_queue);
	void OclPrintAddrStats(QueueProfPtr_t, cl_command_queue);
	void OclDumpStats(pRegisteredEXC_t exc);
	void OclDumpCmdStats(QueueProfPtr_t stPtr, cl_command_queue cmd_queue);
	void OclDumpAddrStats(QueueProfPtr_t stPtr, cl_command_queue cmd_queue);
	void OclGetRuntimeProfile(
		pRegisteredEXC_t exc, uint32_t & exec_time, uint32_t & mem_time);

#endif // CONFIG_BBQUE_OPENCL

	/***********************************************************************
	 * RTLib Run-Time Configuration
	 **********************************************************************/

	static RTLIB_Conf_t rtlib_configuration;

	/**
	 * @brief Look-up config for the BBQUE_RTLIB_OPTS environment variable
	 */
	static RTLIB_ExitCode_t ParseOptions();

	/**
	 * @brief Insert a raw performance counter into the events array
	 *
	 * @param perf_str The string containing label and event code of the
	 * performance counter
	 *
	 * @return The index of the performance counter in the events array
	 */
	static uint8_t InsertRAWPerfCounter(const char * perf_str);

	/***********************************************************************
	 * Communication module-related functions
	 **********************************************************************/

	// EXC registration ////////////////////////////////////////////////////
	/**
	 * @brief Initialize communication with the BarbequeRTRM
	 *
	 * This is the first step to be taken by a new EXC, so that it becomes
	 * able to communicate with the BarbequeRTRM
	 *
	 * @param name Name of the EXC
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _Init(const char * name) = 0;

	/**
	 * @brief Notify the BarbequeRTRM that the EXC exists
	 *
	 * This is the second step to be taken by a new EXC, right after
	 * communication initialization, so that the BarbequeRTRM knows
	 * that the EXC is starting
	 *
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _Register(pRegisteredEXC_t exc) = 0;

	/**
	 * @brief Notify the BarbequeRTRM that the EXC needs resources
	 *
	 * This is the third step to be taken by a new EXC, right after
	 * being registered, so that the BarbequeRTRM knows that the EXC is
	 * ready to receive resources
	 *
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _Enable(pRegisteredEXC_t exc) = 0;

	/**
	 * @brief Notify the BarbequeRTRM that the EXC has terminated
	 *
	 * Before terminating, the EXC must notify the BarbequeRTRM that:
	 * 1) the EXC does not resources anymore
	 * 2) the EXC can be removed from the list of registered EXC
	 *
	 * This is the first step
	 *
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _Disable(pRegisteredEXC_t exc) = 0;

	/**
	 * @brief Notify the BarbequeRTRM that the EXC does not need resources
	 *
	 * Before terminating, the EXC must notify the BarbequeRTRM that:
	 * 1) the EXC does not resources anymore
	 * 2) the EXC can be removed from the list of registered EXC
	 *
	 * This is the second step
	 *
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _Unregister(pRegisteredEXC_t exc) = 0;

	/**
	 * @brief Close communication with the BarbequeRTRM
	 *
	 * This is the last step to be taken by an EXC, so that communication
	 * with the BarbequeRTRM actually terminates
	 */
	virtual void _Exit() = 0;

	// EXC resource allocation negotiation /////////////////////////////////

	/**
	 * @brief Send the Runtime Profile of this EXC to the BarbequeRTRM
	 *
	 * Runtime Profiles of an EXC are used by the BarbequeRTRM to understand
	 * if the EXC is content with its current resource allocation
	 *
	 * @param exc Pointer to registered EXC data
	 * @param percent Suggest a percent variation on assigned CPU bandwidth
	 * @param cusage Current, effective CPU usage of the EXC
	 * @param ctime_ms Current execution-cycle time of the EXC [ms]
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _RTNotify(
			pRegisteredEXC_t exc,
			int percent,
			int cusage,
			int ctime_ms) = 0;

	/**
	 * @brief Send the OpenCL Runtime Profile of this EXC to the BarbequeRTRM
	 *
	 * OpenCL runtime profile are sent to the BarbequeRTRM only if it asked
	 * for them in the first place. Hence, this is effectively a response.
	 * Do not try to invoke this method if the BarbequeRTRM did not request
	 * OpenCL Runtime Profiles: it would result in a communication timeout.
	 *
	 * @see rpc_msg_header struct
	 * @param token ID of the message that prompted for this answer
	 * @param exc Pointer to registered EXC data
	 * @param exc_time Kernel execution time
	 * @param mem_time Kernel memory transfer time
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _GetRuntimeProfileResp(
			rpc_msg_token_t token,
			pRegisteredEXC_t exc,
			uint32_t exc_time,
			uint32_t mem_time) = 0;

	/**
	 * @brief Ask the BarbequeRTRM to allocate resources to this EXC
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _ScheduleRequest(pRegisteredEXC_t exc) = 0;

	/**
	 * @brief Put contraints on the BarbequeRTRM AWM selection
	 * @param exc Pointer to registered EXC data
	 * @param constraints List of AWM selection constraints
	 * @param count Number of constraints in the list
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _SetAWMConstraints(
			pRegisteredEXC_t exc,
			RTLIB_Constraint_t * constraints,
			uint8_t count) = 0;

	/**
	 * @brief Clear contraints of the BarbequeRTRM AWM selection
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _ClearAWMConstraints(pRegisteredEXC_t exc) = 0;

	// Synchronization /////////////////////////////////////////////////////
	/**
	 * @brief Store new allocation information
	 *
	 * When the BarbequeRTRM changes the resource allocation of an EXC,
	 * it sends the EXC a message containing the allocation description,
	 * e.g. AWM id and resource amounts for each resource type. This method
	 * stores this information, then notifies to the BarbequeRTRM that
	 * the message has been successifully received and taken into account.
	 *
	 * This method is called my the communication module after the new
	 * configuration has been received. This method eventually invokes
	 * the communication module to notify to the BarbequeRTRM that the
	 * allocation information has been correclty received.
	 *
	 * @see _SyncpPreChangeResp
	 * @param msg The message that triggered the reconfiguration
	 * @param systems Resource allocation will be stored here
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SyncP_PreChangeNotify(
			rpc_msg_BBQ_SYNCP_PRECHANGE_t msg,
			std::vector<rpc_msg_BBQ_SYNCP_PRECHANGE_SYSTEM_t> &
				systems);

	/**
	 * @brief Notify the BarbequeRTRM that new allocation has been received
	 *
	 * After the EXC receives the new allocation information from the
	 * BarbequeRTRM, it must ACK the reception.
	 *
	 * @see SyncP_PreChangeNotify
	 * @param token ID of the message that triggered this ACK
	 * @param exc Pointer to registered EXC data
	 * @param syncLatency Latency delay for the reconfiguration
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _SyncpPreChangeResp(
			rpc_msg_token_t token,
			pRegisteredEXC_t exc,
			uint32_t syncLatency) = 0;

	/**
	 * @brief Notify the BarbequeRTRM that the EXC finished reconfiguring
	 *
	 * This method is called my the communication module after the Runtime
	 * Library acknowledges to have received a new configuration.
	 * This method eventually invokes the communication module to notify to
	 * the BarbequeRTRM that the reconfiguration was effectively carried out.
	 *
	 * @see _SyncpPostChangeResp
	 * @param msg Message that triggered this response
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SyncP_PostChangeNotify(
			rpc_msg_BBQ_SYNCP_POSTCHANGE_t & msg);

	/**
	 * @brief Notify the BarbequeRTRM that the EXC is reconfiguring
	 *
	 * After the EXC reconfigures, it notifies it to the BarbequeRTRM
	 *
	 * @see SyncP_PostChangeNotify
	 * @param token ID of the BarbequeRTRM message that triggered this action
	 * @param exc Pointer to registered EXC data
	 * @param result Result of the reconfiguration
	 * @return RTLIB_OK on success
	 */
	virtual RTLIB_ExitCode_t _SyncpPostChangeResp(
			rpc_msg_token_t token,
			pRegisteredEXC_t exc,
			RTLIB_ExitCode_t result) = 0;

	// TODO understand whether these sync-point-enforcing
	// methods should be deprecated or not
	RTLIB_ExitCode_t SyncP_SyncChangeNotify(
			rpc_msg_BBQ_SYNCP_SYNCCHANGE_t & msg);
	virtual RTLIB_ExitCode_t _SyncpSyncChangeResp(
			rpc_msg_token_t token,
			pRegisteredEXC_t exc,
			RTLIB_ExitCode_t sync) = 0;
	RTLIB_ExitCode_t SyncP_DoChangeNotify(
			rpc_msg_BBQ_SYNCP_DOCHANGE_t & msg);

protected:

	/**
	 * @brief The name of this application
	 */
	const char * application_name;


	/**
	 * @brief The PID of the Channel Thread
	 *
	 * The Channel Thread is the process/thread in charge to manage messages
	 * exchange with the Barbeque RTRM. Usually, this thread is spawned by
	 * the subclass of this based class which provides the low-level channel
	 * access methods.
	 */
	pid_t channel_thread_pid = 0;

	/**
	 * @brief The channel thread UID
	 *
	 * The Channel Thread and the corresponding application is uniquely
	 * identified by a UID string which is initialized by a call to
	 * @see setUid()
	 */
	char channel_thread_unique_id[20] = "00000:undef";

	inline void SetChannelThreadID(pid_t id, const char * name)
	{
		channel_thread_pid = id;
		snprintf(channel_thread_unique_id, 20,
				"%05d:%-.13s",
				channel_thread_pid,
				name);
	}

private:

	/**
	 * @brief The PID of the application using the library
	 *
	 * This keep track of the application which initialize the library. This
	 * PID could be exploited by the Barbeque RTRM to directly control
	 * applications accessing its managed resources.
	 */
	pid_t application_pid = 0;

	/**
	 * @brief True if the library has been properly initialized
	 */
	bool rtlib_is_initialized = false;

	/**
	 * @brief A map of registered Execution Context
	 *
	 * This maps Execution Context ID (exc_id) into pointers to
	 * RegisteredExecutionContext structures.
	 */
	typedef std::map<uint8_t, pRegisteredEXC_t> excMap_t;

	/**
	 * @brief The map of EXC (successfully) registered by this application
	 */
	excMap_t exc_map;

	/**
	 * @brief An entry of the map of registered EXCs
	 */
	typedef std::pair<uint8_t, pRegisteredEXC_t> excMapEntry_t;

	/**
	 * @brief The path of the application CGroup
	 */
	std::string pathCGroup;

	/**
	 * @brief Get the next available (and unique) Execution Context ID
	 */
	uint8_t NextExcID();

	// EXC metrics monitoring and report ///////////////////////////////////

	/**
	 * @brief Setup statistics for a new selected AWM
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SetupAWMStatistics(pRegisteredEXC_t exc);

	/**
	 * @brief Update statistics for the currently selected AWM
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t UpdateExecutionCycleStatistics(pRegisteredEXC_t exc);

	/**
	 * @brief Init CPU usage statistics for this Run phase
	 * @see NotifyPreRun
	 * @param exc Pointer to registered EXC data
	 */
	void InitCPUBandwidthStats(pRegisteredEXC_t exc);

	/**
	 * @brief Update CPU usage statistics for this Run phase
	 * @see NotifyPostRun
	 * @param exc Pointer to registered EXC data
	 */
	RTLIB_ExitCode_t UpdateCPUBandwidthStats(pRegisteredEXC_t exc);

	/**
	 * @brief Log the header for statistics report
	 */
	void DumpStatsHeader();

	/**
	 * @brief Log memory usage report
		 * @param exc Pointer to registered EXC data
	 */
	void DumpMemoryReport(pRegisteredEXC_t exc);

	/**
	 * @brief Log execution statistics collected so far
	 * @param exc Pointer to registered EXC data
	 * @param verbose Whether the report should show all the information
	 */
	inline void DumpStats(pRegisteredEXC_t exc, bool verbose = false);

	/**
	 * @brief Log execution statistics collected so far (console format)
	 * @param exc Pointer to registered EXC data
	 * @param verbose Whether the report should show all the information
	 */
	void DumpStatsConsole(pRegisteredEXC_t exc, bool verbose = false);

	// Linux Control Groups handling ///////////////////////////////////////

	/**
	 * @brief Create a CGroup for the specifed EXC
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t CGroupPathSetup(pRegisteredEXC_t exc);

	/**
	 * @brief Delete the CGroup of the specified EXC
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t CGroupDelete(pRegisteredEXC_t exc);

	/**
	 * @brief Create a CGroup of the specified EXC
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t CGroupCreate(pRegisteredEXC_t exc);

	/**
	 * @brief Updates the CGroup of the specified EXC
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t CGroupCommitAllocation(pRegisteredEXC_t exc);

	// Synchronization handling ////////////////////////////////////////////

	/**
	 * @brief Suspend caller waiting for an AWM being assigned
	 *
	 * When the EXC has notified a scheduling request to the RTRM, this
	 * method put it to sleep waiting for an assignement.
	 *
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK if a valid working mode has been assinged to the
	 * EXC, RTLIB_EXC_GWM_FAILED otherwise
	 */
	RTLIB_ExitCode_t WaitForWorkingMode(pRegisteredEXC_t exc);

	/**
	 * @brief setup AWM change and extract new AWM information
	 * @param exc Pointer to registered EXC data
	 * @param working_mode_params Parameters of the current AWM
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t GetWorkingModeParams(pRegisteredEXC_t exc,
			RTLIB_WorkingModeParams * working_mode_params);

	/**
	 * @brief Suspend caller waiting for a reconfiguration to complete
	 *
	 * When the EXC has notified to switch into a different AWM by the RTRM,
	 * this method put the RTLIB PostChange to sleep waiting for the
	 * completion of such reconfiguration.
	 *
	 * @param exc the regidstered EXC to wait reconfiguration for
	 * @return RTLIB_OK if the reconfigutation complete successfully,
	 * RTLIB_EXC_SYNCP_FAILED otherwise
	 */
	RTLIB_ExitCode_t WaitForSyncDone(pRegisteredEXC_t exc);

	/**
	 * @brief Get an extimation of the Synchronization Latency
	 * @param exc Pointer to registered EXC data
	 * @return The sync latency [ms]
	 */
	uint32_t GetSyncLatency(pRegisteredEXC_t exc);

	/***********************************************************************
	 * Synchronization Protocol Messages
	 **********************************************************************/

	/**
	 * @brief A synchronization protocol Pre-Change for the specified EXC.
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SyncP_PreChangeNotify(pRegisteredEXC_t exc);

	/**
	 * @brief A synchronization protocol Sync-Change for the specified EXC.
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SyncP_SyncChangeNotify(pRegisteredEXC_t exc);

	/**
	 * @brief A synchronization protocol Do-Change for the specified EXC.
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SyncP_DoChangeNotify(pRegisteredEXC_t exc);

	/**
	 * @brief A synchronization protocol Post-Change for the specified EXC.
	 * @param exc Pointer to registered EXC data
	 * @return RTLIB_OK on success
	 */
	RTLIB_ExitCode_t SyncP_PostChangeNotify(pRegisteredEXC_t exc);


	/***********************************************************************
	 * Application Callbacks Proxies
	 **********************************************************************/

	// TODO remove this old function. See app proxy test plugin
	RTLIB_ExitCode_t StopExecution(
			RTLIB_EXCHandler_t exc_handler,
			struct timespec timeout);

	/***********************************************************************
	 * Utility functions
	 **********************************************************************/
	/**
	 * @brief Retrive registered EXC data
	 * @param exc_handler Handler of the target Execution Context
	 * @return The registered EXC data, or nullptr in case of error
	 */
	pRegisteredEXC_t getRegistered(const RTLIB_EXCHandler_t exc_handler);

	/**
	 * @brief Retrive registered EXC data
	 * @param exc_id ID of the target Execution Context
	 * @return The registered EXC data, or nullptr in case of error
	 */
	pRegisteredEXC_t getRegistered(uint8_t exc_id);

	/***********************************************************************
	 * Performance Counters
	 **********************************************************************/
#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
#define BBQUE_RTLIB_PERF_ENABLE true

	/** Default performance attributes to collect for each task */
	static PerfEventAttr_t * raw_events;

	/** Default performance attributes to collect for each task */
	static PerfEventAttr_t default_events[];

	/** Detailed stats (-d), covering the L1 and last level data caches */
	static PerfEventAttr_t detailed_events[];

	/** Very detailed stats (-d -d), covering the instruction cache and the
	 * TLB caches: */
	static PerfEventAttr_t very_detailed_events[];

	/** Very, very detailed stats (-d -d -d), adding prefetch events */
	static PerfEventAttr_t very_very_detailed_events[];

	/**
	 * @brief Get number of registered performance events
	 * @param exc Pointer to registered EXC data
	 * @return Number of registered performance events
	 */
	inline uint8_t PerfRegisteredEvents(pRegisteredEXC_t exc)
	{
		return exc->events_map.size();
	}

	/**
	 * @brief Check if an event description matches a given type and config
	 * @param ppea Pointer to a performance counter event descriptor
	 * @param type Type of event
	 * @param config Config for the event
	 * @return True on match
	 */
	inline bool PerfEventMatch(
			pPerfEventAttr_t ppea,
			perf_type_id type,
			uint64_t config)
	{
		return (ppea->type == type && ppea->config == config);
	}

	/**
	 * @brief Disable perf events monitoring
	 * @param exc Pointer to registered EXC data
	 */
	inline void PerfDisable(pRegisteredEXC_t exc)
	{
		exc->perf.Disable();
	}

	/**
	 * @brief Enable perf events monitoring
	 * @param exc Pointer to registered EXC data
	 */
	inline void PerfEnable(pRegisteredEXC_t exc)
	{
		exc->perf.Enable();
	}

	/**
	 * @brief Register events to be monitored
	 * @param exc Pointer to registered EXC data
	 */
	void PerfSetupEvents(pRegisteredEXC_t exc);

	/**
	 * @brief Setup statistics for registered perf events
	 * @param exc Pointer to registered EXC data
	 */
	void PerfSetupStats(pRegisteredEXC_t exc, pAwmStats_t awm_stats);

	/**
	 * @brief Collect counters for registered events
	 * @param exc Pointer to registered EXC data
	 */
	void PerfCollectStats(pRegisteredEXC_t exc);


	/**
	 * @brief Collect counters for registered events
	 * @param exc Pointer to registered EXC data
	 */
	void PerfPrintStats(pRegisteredEXC_t exc, pAwmStats_t awm_stats);

	pPerfEventStats_t PerfGetEventStats(
			pAwmStats_t awm_stats,
			perf_type_id type,
			uint64_t config);

	/**
	 * @brief Check if the counter value is expressed in milliseconds
	 * @param exc Pointer to registered EXC data
	 * @param fd event ID
	 * @return true if value expressed in milliseconds
	 */
	bool IsNsecCounter(pRegisteredEXC_t exc, int fd);

	/**
	 * @brief Print event stats [ms]
	 * @param awm_stats AWM statistics
	 * @param perf_event_stats Perf statics
	 */
	void PerfPrintNsec(pAwmStats_t awm_stats, pPerfEventStats_t perf_event_stats);

	/**
	 * @brief Print event stats
	 * @param awm_stats AWM statistics
	 * @param perf_event_stats Perf statics
	 */
	void PerfPrintAbs(pAwmStats_t awm_stats, pPerfEventStats_t perf_event_stats);

	/**
	 * @brief Print noise on perf counters measurements
	 * @param total Standard value of counter
	 * @param avg Average value of counter
	 */
	void PrintNoisePct(double total, double avg);

#else // CONFIG_BBQUE_RTLIB_PERF_SUPPORT
# define BBQUE_RTLIB_PERF_ENABLE false
# define PerfRegisteredEvents(exc) 0
# define PerfSetupStats(exc, awm_stats) {}
# define PerfSetupEvents(exc) {}
# define PerfEnable(exc) {}
# define PerfDisable(exc) {}
# define PerfCollectStats(exc) {}
# define PerfPrintStats(exc, awm_stats) {}
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT


	/***********************************************************************
	 *	Cycles Per Second (CPS) Control Support
	 **********************************************************************/

	/**
	 * Used at the end of an excution cycle (Configure + Run + Monitor)
	 * to guarantee that the execution cycle duration is greater than the
	 * one required by the EXC (if the EXC explicitely defined such a
	 * duration). If this cycle time execution was too fast, the Runtime
	 * Library will insert a sleep call and make the duration stick with the
	 * required one.
	 *
	 * @see SetCPS, NotifyPostMonitor
	 * @param exc Pointer to registered EXC data
	 */
	void ForceCPS(pRegisteredEXC_t exc);

};

} // namespace rtlib

} // namespace bbque

// Undefine locally defined module name
#undef MODULE_NAMESPACE

#endif /* end of include guard: BBQUE_RPC_H_ */
