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

#include "bbque/rtlib.h"
#include "bbque/config.h"
#include "bbque/rtlib/rpc_messages.h"
#include "bbque/utils/utility.h"
#include "bbque/utils/timer.h"
#include "bbque/cpp11/condition_variable.h"
#include "bbque/cpp11/mutex.h"
#include "bbque/cpp11/thread.h"

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
# include "bbque/utils/perf.h"
#endif

#include <map>
#include <memory>
#include <string>

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

using bbque::utils::Timer;
using namespace boost::accumulators;

namespace bbque { namespace rtlib {

/**
 * @brief The class implementing the RTLib plain API
 * @ingroup rtlib_sec03_plain
 *
 * This RPC mechanism is channel agnostic and defines a set of procedure that
 * applications could call to send requests to the Barbeque RTRM.  The actual
 * implementation of the communication channel is provided by class derived by
 * this one. This class provides also a factory method which allows to obtain
 * an instance to the concrete communication channel that can be selected at
 * compile time.
 */
class BbqueRPC {

public:

	/**
	 * @brief Get a reference to the (singleton) RPC service
	 *
	 * This class is a factory of different RPC communication channels. The
	 * actual instance returned is defined at compile time by selecting the
	 * proper specialization class.
	 *
	 * @return A reference to an actual BbqueRPC implementing the compile-time
	 * selected communication channel.
	 */
	static BbqueRPC * GetInstance();

	/**
	 * @brief Release the RPC channel
	 */
	virtual ~BbqueRPC(void);

/******************************************************************************
 * Channel Independant interface
 ******************************************************************************/

	RTLIB_ExitCode_t Init(const char *name);

	RTLIB_ExecutionContextHandler_t Register(
			const char* name,
			const RTLIB_ExecutionContextParams_t* params);

	void Unregister(
			const RTLIB_ExecutionContextHandler_t ech);

	void UnregisterAll();

	RTLIB_ExitCode_t Enable(
			const RTLIB_ExecutionContextHandler_t ech);

	RTLIB_ExitCode_t Disable(
			const RTLIB_ExecutionContextHandler_t ech);

	RTLIB_ExitCode_t Set(
			const RTLIB_ExecutionContextHandler_t ech,
			RTLIB_Constraint_t* constraints,
			uint8_t count);

	RTLIB_ExitCode_t Clear(
			const RTLIB_ExecutionContextHandler_t ech);

	RTLIB_ExitCode_t GGap(
			const RTLIB_ExecutionContextHandler_t ech,
			uint8_t percent);

	RTLIB_ExitCode_t GetWorkingMode(
			RTLIB_ExecutionContextHandler_t ech,
			RTLIB_WorkingModeParams_t *wm,
			RTLIB_SyncType_t st);

/*******************************************************************************
 *    Utility Functions
 ******************************************************************************/

	const std::string GetCGroupPath() const {
		return pathCGroup;
	}

	inline const char *GetChUid() const {
		return chTrdUid;
	}

	AppUid_t GetUid(RTLIB_ExecutionContextHandler_t ech);

/*******************************************************************************
 *    Cycles Per Second (CPS) Control Support
 ******************************************************************************/

	/**
	 * @brief Set the required Cycles Per Second (CPS)
	 *
	 * This allows to define the required and expected cycles rate. If at
	 * run-time the cycles execution should be faster, a properly computed
	 * delay will be inserted by the RTLib in order to get the specified
	 * rate.
	 */
	RTLIB_ExitCode_t SetCPS(RTLIB_ExecutionContextHandler_t ech,
			float cps);

	/**
	 * @brief Set the required Cycle time [us]
	 *
	 * This allows to define the required and expected cycle time. If at
	 * run-time the cycles execution should be faster, a properly computed
	 * delay will be inserted by the RTLib in order to get the specified
	 * duration.
	 */
	RTLIB_ExitCode_t SetCTimeUs(RTLIB_ExecutionContextHandler_t ech,
			uint32_t us) {
		return SetCPS(ech, static_cast<float>(1e6)/us);
	}

/*******************************************************************************
 *    Performance Monitoring Support
 ******************************************************************************/

	void NotifySetup(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyInit(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyExit(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPreConfigure(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPostConfigure(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPreRun(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPostRun(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPreMonitor(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPostMonitor(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPreSuspend(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPostSuspend(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPreResume(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyPostResume(
		RTLIB_ExecutionContextHandler_t ech);

	void NotifyRelease(
		RTLIB_ExecutionContextHandler_t ech);

protected:

	typedef struct PerfEventAttr {
#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		perf_type_id type;
#endif
		uint64_t config;
	} PerfEventAttr_t;

	typedef PerfEventAttr_t *pPerfEventAttr_t;

	typedef std::map<int, pPerfEventAttr_t> PerfRegisteredEventsMap_t;

	typedef std::pair<int, pPerfEventAttr_t> PerfRegisteredEventsMapEntry_t;

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
		/** The statistics collected for this PRE */
		accumulator_set<uint32_t,
			stats<tag::min, tag::max, tag::variance>> samples;
	} PerfEventStats_t;

	typedef std::shared_ptr<PerfEventStats_t> pPerfEventStats_t;

	typedef std::map<int, pPerfEventStats_t> PerfEventStatsMap_t;

	typedef std::pair<int, pPerfEventStats_t> PerfEventStatsMapEntry_t;

	typedef std::multimap<uint8_t, pPerfEventStats_t> PerfEventStatsMapByConf_t;

	typedef std::pair<uint8_t, pPerfEventStats_t> PerfEventStatsMapByConfEntry_t;

	/**
	 * @brief Statistics on AWM usage
	 */
	typedef struct AwmStats {
		/** Count of times this AWM has been in used */
		uint32_t count;
		/** The time [ms] spent on processing into this AWM */
		uint32_t time_processing;

		/** Statistics on synchronization time */
		accumulator_set<double,
			stats<tag::min, tag::max, tag::variance> >
				sync_time_samples;

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		/** Map of registered Perf counters */
		PerfEventStatsMap_t events_map;
		/** Map registered Perf counters to their type */
		PerfEventStatsMapByConf_t events_conf_map;
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT

		/** The mutex protecting concurrent access to statistical data */
		std::mutex stats_mtx;

		AwmStats() : count(0), time_processing(0) {};

	} AwmStats_t;

	/**
	 * @brief A pointer to AWM statistics
	 */
	typedef std::shared_ptr<AwmStats_t> pAwmStats_t;

	/**
	 * @brief Map AWMs ID into its statistics
	 */
	typedef std::map<uint8_t, pAwmStats_t> AwmStatsMap_t;

	typedef struct RegisteredExecutionContext {
		/** The Execution Context data */
		RTLIB_ExecutionContextParams_t exc_params;
		/** The name of this Execuion Context */
		std::string name;
		/** The RTLIB assigned ID for this Execution Context */
		uint8_t exc_id;
		/** The PID of the control thread managing this EXC */
		pid_t ctrlTrdPid = 0;
#define EXC_FLAGS_AWM_VALID      0x01 ///< The EXC has been assigned a valid AWM
#define EXC_FLAGS_AWM_WAITING    0x02 ///< The EXC is waiting for a valid AWM
#define EXC_FLAGS_AWM_ASSIGNED   0x04 ///< The EXC is waiting for a valid AWM
#define EXC_FLAGS_EXC_SYNC       0x08 ///< The EXC entered Sync Mode
#define EXC_FLAGS_EXC_SYNC_DONE  0x10 ///< The EXC exited Sync Mode
#define EXC_FLAGS_EXC_REGISTERED 0x20 ///< The EXC is registered
#define EXC_FLAGS_EXC_ENABLED    0x40 ///< The EXC is enabled
#define EXC_FLAGS_EXC_BLOCKED    0x80 ///< The EXC is blocked
		/** A set of flags to define the state of this EXC */
		uint8_t flags = 0x00;
		/** The last required synchronization action */
		RTLIB_ExitCode_t event = RTLIB_OK;
		/** The ID of the assigned AWM (if valid) */
		uint8_t awm_id = 0;
		/** The mutex protecting access to this structure */
		std::mutex mtx;
		/** The conditional variable to be notified on changes for this EXC */
		std::condition_variable cv;

		/** The High-Resolution timer used for profiling */
		Timer exc_tmr;
		/** The time [ms] spent on waiting for an AWM being assigned */
		uint32_t time_blocked = 0;
		/** The time [ms] spent on reconfigurations */
		uint32_t time_reconf = 0;
		/** The time [ms] spent on processing */
		uint32_t time_processing = 0;

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
		/** Performance counters */
		utils::Perf perf;
		/** Map of registered Perf counter IDs */
		PerfRegisteredEventsMap_t events_map;
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT

		/** Statistics on AWM's of this EXC */
		AwmStatsMap_t stats;
		/** Statistics of currently selected AWM */
		pAwmStats_t pAwmStats;

		double cps_tstart = 0; // [ms] at the last cycle start time
		float cps_max = 0;     // [Hz] the requried maximum CPS
		float cps_expect = 0;  // [ms] the expected cycle time

		RegisteredExecutionContext(const char *_name, uint8_t id) :
			name(_name), exc_id(id) {
		}

		~RegisteredExecutionContext() {
			stats.clear();
			pAwmStats = pAwmStats_t();
		}


	} RegisteredExecutionContext_t;

	typedef std::shared_ptr<RegisteredExecutionContext_t> pregExCtx_t;

	//--- AWM Validity
	inline bool isAwmValid(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_AWM_VALID);
	}
	inline void setAwmValid(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("AWM  <= Valid [%d:%s:%d]\n"),
					prec->exc_id, prec->name.c_str(), prec->awm_id));
		prec->flags |= EXC_FLAGS_AWM_VALID;
	}
	inline void clearAwmValid(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("AWM  <= Invalid [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_AWM_VALID;
	}

	//--- AWM Wait
	inline bool isAwmWaiting(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_AWM_WAITING);
	}
	inline void setAwmWaiting(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("AWM  <= Waiting [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_AWM_WAITING;
	}
	inline void clearAwmWaiting(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("AWM  <= NOT Waiting [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_AWM_WAITING;
	}

	//--- AWM Assignment
	inline bool isAwmAssigned(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_AWM_ASSIGNED);
	}
	inline void setAwmAssigned(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("AWM  <= Assigned [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_AWM_ASSIGNED;
	}
	inline void clearAwmAssigned(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("AWM  <= NOT Assigned [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_AWM_ASSIGNED;
	}

	//--- Sync Mode Status
	inline bool isSyncMode(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_SYNC);
	}
	inline void setSyncMode(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("SYNC <= Enter [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_SYNC;
	}
	inline void clearSyncMode(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("SYNC <= Exit [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_SYNC;
	}

	//--- Sync Done
	inline bool isSyncDone(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_SYNC_DONE);
	}
	inline void setSyncDone(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("SYNC <= Done [%d:%s:%d]\n"),
					prec->exc_id, prec->name.c_str(), prec->awm_id));
		prec->flags |= EXC_FLAGS_EXC_SYNC_DONE;
	}
	inline void clearSyncDone(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("SYNC <= Pending [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_SYNC_DONE;
	}

	//--- EXC Registration status
	inline bool isRegistered(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_REGISTERED);
	}
	inline void setRegistered(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("EXC  <= Registered [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_REGISTERED;
	}
	inline void clearRegistered(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("EXC  <= Unregistered [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_REGISTERED;
	}

	//--- EXC Enable status
	inline bool isEnabled(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_ENABLED);
	}
	inline void setEnabled(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("EXC  <= Enabled [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_ENABLED;
	}
	inline void clearEnabled(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("EXC  <= Disabled [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_ENABLED;
	}

	//--- EXC Blocked status
	inline bool isBlocked(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_BLOCKED);
	}
	inline void setBlocked(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("EXC  <= Blocked [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_BLOCKED;
	}
	inline void clearBlocked(pregExCtx_t prec) const {
		DB(fprintf(stderr, FD("EXC  <= UnBlocked [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_BLOCKED;
	}


/******************************************************************************
 * RTLib Run-Time Configuration
 ******************************************************************************/

	//---- Performance Counters options
	static bool envPerfCount;
	static bool envGlobal;
	static bool envOverheads;
	static int  envDetailedRun;
	static int  envRawPerfCount;
	static bool envNoKernel;
	static bool envCsvOutput;
	static bool envMOSTOutput;
#define BBQUE_RTLIB_OPTS_TAG_MAX 6
	// The array +2 is due to:
	// - the ending namespace separator ":"
	// - the string NULL terminator
	static char envMetricsTag[BBQUE_RTLIB_OPTS_TAG_MAX+2];
	static bool envBigNum;
	static bool envUnmanaged;
	static int  envUnmanagedAWM;
	static const char *envCsvSep;

	/**
	 * @brief Look-up configuration from environment variable BBQUE_RTLIB_OPTS
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
	static uint8_t InsertRAWPerfCounter(const char *perf_str);

/******************************************************************************
 * Channel Dependant interface
 ******************************************************************************/

	virtual RTLIB_ExitCode_t _Init(
			const char *name) = 0;

	virtual RTLIB_ExitCode_t _Register(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Unregister(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Enable(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Disable(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Set(pregExCtx_t prec,
			RTLIB_Constraint_t* constraints, uint8_t count) = 0;

	virtual RTLIB_ExitCode_t _Clear(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _GGap(pregExCtx_t prec, uint8_t percent) = 0;

	virtual RTLIB_ExitCode_t _ScheduleRequest(pregExCtx_t prec) = 0;

	virtual void _Exit() = 0;


/******************************************************************************
 * Synchronization Protocol Messages
 ******************************************************************************/

//----- PreChange

	/**
	 * @brief Send response to a Pre-Change command
	 */
	virtual RTLIB_ExitCode_t _SyncpPreChangeResp(
			rpc_msg_token_t token,
			pregExCtx_t prec,
			uint32_t syncLatency) = 0;

	/**
	 * @brief A synchronization protocol Pre-Change for the EXC with the
	 * specified ID.
	 */
	RTLIB_ExitCode_t SyncP_PreChangeNotify(
			rpc_msg_BBQ_SYNCP_PRECHANGE_t &msg);

//----- SyncChange

	/**
	 * @brief Send response to a Sync-Change command
	 */
	virtual RTLIB_ExitCode_t _SyncpSyncChangeResp(
			rpc_msg_token_t token,
			pregExCtx_t prec, RTLIB_ExitCode_t sync) = 0;

	/**
	 * @brief A synchronization protocol Sync-Change for the EXC with the
	 * specified ID.
	 */
	RTLIB_ExitCode_t SyncP_SyncChangeNotify(
			rpc_msg_BBQ_SYNCP_SYNCCHANGE_t &msg);

//----- DoChange

	/**
	 * @brief A synchronization protocol Do-Change for the EXC with the
	 * specified ID.
	 */
	RTLIB_ExitCode_t SyncP_DoChangeNotify(
			rpc_msg_BBQ_SYNCP_DOCHANGE_t &msg);

//----- PostChange

	/**
	 * @brief Send response to a Post-Change command
	 */
	virtual RTLIB_ExitCode_t _SyncpPostChangeResp(
			rpc_msg_token_t token,
			pregExCtx_t prec,
			RTLIB_ExitCode_t result) = 0;

	/**
	 * @brief A synchronization protocol Post-Change for the EXC with the
	 * specified ID.
	 */
	RTLIB_ExitCode_t SyncP_PostChangeNotify(
			rpc_msg_BBQ_SYNCP_POSTCHANGE_t &msg);


protected:


	/**
	 * @brief The PID of the Channel Thread
	 *
	 * The Channel Thread is the process/thread in charge to manage messages
	 * exchange with the Barbeque RTRM. Usually, this thread is spawned by the
	 * subcluss of this based class which provides the low-level channel
	 * access methods.
	 */
	pid_t chTrdPid = 0;


	/**
	 * @brief The channel thread UID
	 *
	 * The Channel Thread and the corresponding application is uniquely
	 * identified by a UID string which is initialized by a call to
	 * @see setUid()
	 */
	char chTrdUid[20] = "00000:undef ";

	inline void setChId(pid_t id, const char *name) {
		chTrdPid = id;
		snprintf(chTrdUid, 20, "%05d:%-13.13s", chTrdPid, name);
	}

private:

	/**
	 * @brief The PID of the application using the library
	 *
	 * This keep track of the application which initialize the library. This
	 * PID could be exploited by the Barbeque RTRM to directly control
	 * applications accessing its managed resources.
	 */
	pid_t appTrdPid = 0;

	/**
	 * @brief True if the library has been properly initialized
	 */
	bool initialized = false;

	/**
	 * @brief A map of registered Execution Context
	 *
	 * This maps Execution Context ID (exc_id) into pointers to
	 * RegisteredExecutionContext structures.
	 */
	typedef std::map<uint8_t, pregExCtx_t> excMap_t;

	/**
	 * @brief The map of EXC (successfully) registered by this application
	 */
	excMap_t exc_map;

	/**
	 * @brief An entry of the map of registered EXCs
	 */
	typedef std::pair<uint8_t, pregExCtx_t> excMapEntry_t;

	/**
	 * @brief The path of the application CGroup
	 */
	std::string pathCGroup;

	/**
	 * @brief Get the next available (and unique) Execution Context ID
	 */
	uint8_t GetNextExcID();

	/**
	 * @brief Setup statistics for a new selecte AWM
	 */
	RTLIB_ExitCode_t SetupStatistics(pregExCtx_t prec);

	/**
	 * @brief Update statistics for the currently selected AWM
	 */
	RTLIB_ExitCode_t UpdateStatistics(pregExCtx_t prec);

	/**
	 * @brief Log the header for statistics collection
	 */
	void DumpStatsHeader();

	/**
	 * @brief Setup the path of the application CGroup
	 */
	RTLIB_ExitCode_t SetCGroupPath(pregExCtx_t prec);

	/**
	 * @brief Log memory usage report
	 */
	void DumpMemoryReport(pregExCtx_t prec);

	/**
	 * @brief Log execution statistics collected so far
	 */
	inline void DumpStats(pregExCtx_t prec, bool verbose = false);

	/**
	 * @brief Log execution statistics collected so far (Console format)
	 */
	void DumpStatsConsole(pregExCtx_t prec, bool verbose = false);

	/**
	 * @brief Log execution statistics collected so far (MOST format)
	 */
	void DumpStatsMOST(pregExCtx_t prec);

	/**
	 * @brief Update sync time [ms] estimation for the currently AWM.
	 *
	 * @note This method requires statistics being already initialized
	 */
	void _SyncTimeEstimation(pregExCtx_t prec);

	/**
	 * @brief Update sync time [ms] estimation for the currently AWM
	 *
	 * This method ensure statistics update if they have been already
	 * initialized.
	 */
	void SyncTimeEstimation(pregExCtx_t prec);

	/**
	 * @brief Get the assigned AWM (if valid)
	 *
	 * @return RTLIB_OK if a valid AWM has been returned, RTLIB_EXC_GWM_FAILED
	 * if the current AWM is not valid and thus a scheduling should be
	 * required to the RTRM
	 */
	RTLIB_ExitCode_t GetAssignedWorkingMode(pregExCtx_t prec,
			RTLIB_WorkingModeParams_t *wm);

	/**
	 * @brief Suspend caller waiting for an AWM being assigned
	 *
	 * When the EXC has notified a scheduling request to the RTRM, this
	 * method put it to sleep waiting for an assignement.
	 *
	 * @return RTLIB_OK if a valid working mode has been assinged to the EXC,
	 * RTLIB_EXC_GWM_FAILED otherwise
	 */
	RTLIB_ExitCode_t WaitForWorkingMode(pregExCtx_t prec,
			RTLIB_WorkingModeParams *wm);

	/**
	 * @brief Suspend caller waiting for a reconfiguration to complete
	 *
	 * When the EXC has notified to switch into a different AWM by the RTRM,
	 * this method put the RTLIB PostChange to sleep waiting for the
	 * completion of such reconfiguration.
	 *
	 * @param prec the regidstered EXC to wait reconfiguration for
	 *
	 * @return RTLIB_OK if the reconfigutation complete successfully,
	 * RTLIB_EXC_SYNCP_FAILED otherwise
	 */
	RTLIB_ExitCode_t WaitForSyncDone(pregExCtx_t prec);

	/**
	 * @brief Get an extimation of the Synchronization Latency
	 */
	uint32_t GetSyncLatency(pregExCtx_t prec);

/******************************************************************************
 * Synchronization Protocol Messages
 ******************************************************************************/

	/**
	 * @brief A synchronization protocol Pre-Change for the specified EXC.
	 */
	RTLIB_ExitCode_t SyncP_PreChangeNotify(pregExCtx_t prec);

	/**
	 * @brief A synchronization protocol Sync-Change for the specified EXC.
	 */
	RTLIB_ExitCode_t SyncP_SyncChangeNotify(pregExCtx_t prec);

	/**
	 * @brief A synchronization protocol Do-Change for the specified EXC.
	 */
	RTLIB_ExitCode_t SyncP_DoChangeNotify(pregExCtx_t prec);

	/**
	 * @brief A synchronization protocol Post-Change for the specified EXC.
	 */
	RTLIB_ExitCode_t SyncP_PostChangeNotify(pregExCtx_t prec);


/******************************************************************************
 * Application Callbacks Proxies
 ******************************************************************************/

	RTLIB_ExitCode_t StopExecution(
			RTLIB_ExecutionContextHandler_t ech,
			struct timespec timeout);

/******************************************************************************
 * Utility functions
 ******************************************************************************/

	pregExCtx_t getRegistered(
			const RTLIB_ExecutionContextHandler_t ech);

	/**
	 * @brief Get an EXC handler for the give EXC ID
	 */
	pregExCtx_t getRegistered(uint8_t exc_id);


/******************************************************************************
 * Performance Counters
 ******************************************************************************/
#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT

# define BBQUE_RTLIB_PERF_ENABLE true

	/** Default performance attributes to collect for each task */
	static PerfEventAttr_t *raw_events;

	/** Default performance attributes to collect for each task */
	static PerfEventAttr_t default_events[];

	/** Detailed stats (-d), covering the L1 and last level data caches */
	static PerfEventAttr_t detailed_events[];

	/** Very detailed stats (-d -d), covering the instruction cache and the
	 * TLB caches: */
	static PerfEventAttr_t very_detailed_events[];

	/** Very, very detailed stats (-d -d -d), adding prefetch events */
	static PerfEventAttr_t very_very_detailed_events[];

	inline uint8_t PerfRegisteredEvents(pregExCtx_t prec) {
		return prec->events_map.size();
	}

	inline bool PerfEventMatch(pPerfEventAttr_t ppea,
			perf_type_id type, uint64_t config) {
		return (ppea->type == type && ppea->config == config);
	}

	inline void PerfDisable(pregExCtx_t prec) {
		prec->perf.Disable();
	}

	inline void PerfEnable(pregExCtx_t prec) {
		prec->perf.Enable();
	}

	void PerfSetupEvents(pregExCtx_t prec);

	void PerfSetupStats(pregExCtx_t prec, pAwmStats_t pstats);

	void PerfCollectStats(pregExCtx_t prec);

	void PerfPrintStats(pregExCtx_t prec, pAwmStats_t pstats);

	bool IsNsecCounter(pregExCtx_t prec, int fd);

	void PerfPrintNsec(pAwmStats_t pstats, pPerfEventStats_t ppes);

	void PerfPrintAbs(pAwmStats_t pstats, pPerfEventStats_t ppes);

	pPerfEventStats_t PerfGetEventStats(pAwmStats_t pstats, perf_type_id type,
			uint64_t config);

	void PerfPrintMissesRatio(double avg_missed, double tot_branches,
			const char *text);

	void PrintNoisePct(double total, double avg);
#else
# define BBQUE_RTLIB_PERF_ENABLE false
# define PerfRegisteredEvents(prec) 0
# define PerfSetupStats(prec, pstats) {}
# define PerfSetupEvents(prec) {}
# define PerfEnable(prec) {}
# define PerfDisable(prec) {}
# define PerfCollectStats(prec) {}
# define PerfPrintStats(prec, pstats) {}
#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT


/*******************************************************************************
 *    Cycles Per Second (CPS) Control Support
 ******************************************************************************/

	void ForceCPS(pregExCtx_t prec);


};

} // namespace rtlib

} // namespace bbque

// Undefine locally defined module name
#undef MODULE_NAMESPACE

#endif /* end of include guard: BBQUE_RPC_H_ */
