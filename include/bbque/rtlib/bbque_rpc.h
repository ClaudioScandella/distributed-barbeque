/**
 *       @file  bbque_rpc.h
 *      @brief  The Barbeque RPC interface to support comunicaiton among
 *      applications and the RTRM.
 *
 * This RPC mechanism is channel agnostic and defines a set of procedure that
 * applications could call to send requests to the Barbeque RTRM.  The actual
 * implementation of the communication channel is provided by class derived by
 * this one. This class provides also a factory method which allows to obtain
 * an instance to the concrete communication channel that can be selected at
 * compile time.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  01/11/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =============================================================================
 */

#ifndef BBQUE_RPC_H_
#define BBQUE_RPC_H_

#include "bbque/rtlib.h"
#include "bbque/rtlib/rpc_messages.h"
#include "bbque/utils/utility.h"
#include "bbque/utils/timer.h"

#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <thread>

#define FMT_DBG(fmt) BBQUE_FMT(COLOR_LBLUE, "RTLIB_RPC  [DBG]", fmt)

using bbque::utils::Timer;

namespace bbque { namespace rtlib {

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

	RTLIB_ExitCode_t GetWorkingMode(
			RTLIB_ExecutionContextHandler_t ech,
			RTLIB_WorkingModeParams_t *wm,
			RTLIB_SyncType_t st);

protected:

	/**
	 * @brief Statistics on AWM usage
	 */
	typedef struct AwmStats {
		/** Count of times this AWM has been in used */
		uint32_t count;
		/** Count of cycles done within this AWM */
		uint32_t cycles;
		/** The Cumulative Average (CA) of the synchronization time [ms] */
		uint32_t sync_time_ca;
		/** The time [ms] spent on processing into this AWM */
		uint32_t time_processing;

		AwmStats() : count(0), cycles(0),
			sync_time_ca(0), time_processing(0) {};

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
		pid_t ctrlTrdPid;
#define EXC_FLAGS_AWM_VALID      0x01 ///< The EXC has been assigned a valid AWM
#define EXC_FLAGS_AWM_WAITING    0x02 ///< The EXC is waiting for a valid AWM
#define EXC_FLAGS_EXC_SYNC       0x04 ///< The EXC entered Sync Mode
#define EXC_FLAGS_EXC_SYNC_DONE  0x08 ///< The EXC exited Sync Mode
#define EXC_FLAGS_EXC_REGISTERED 0x10 ///< The EXC is registered
#define EXC_FLAGS_EXC_ENABLED    0x20 ///< The EXC is enabled
#define EXC_FLAGS_EXC_BLOCKED    0x40 ///< The EXC is blocked
		/** A set of flags to define the state of this EXC */
		uint8_t flags;
		/** The last required synchronization action */
		RTLIB_ExitCode_t event;
		/** The ID of the assigned AWM (if valid) */
		uint8_t awm_id;
		/** The mutex protecting access to this structure */
		std::mutex mtx;
		/** The conditional variable to be notified on changes for this EXC */
		std::condition_variable cv;

		/** The High-Resolution timer used for profiling */
		Timer exc_tmr;
		/** The time [ms] spent on waiting for an AWM being assigned */
		uint32_t time_blocked;
		/** The time [ms] spent on reconfigurations */
		uint32_t time_reconf;
		/** The time [ms] spent on processing */
		uint32_t time_processing;

		/** Statistics on AWM's of this EXC */
		AwmStatsMap_t stats;
		/** Statistics of currently selected AWM */
		pAwmStats_t pAwmStats;

		RegisteredExecutionContext(const char *_name) :
			name(_name), ctrlTrdPid(0), flags(0x00),
			time_blocked(0), time_reconf(0), time_processing(0) {
				exc_id = GetNextExcID();
		};

	} RegisteredExecutionContext_t;

	typedef std::shared_ptr<RegisteredExecutionContext_t> pregExCtx_t;

	//--- AWM Validity
	inline bool isAwmValid(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_AWM_VALID);
	}
	inline void setAwmValid(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("AWM  <= Valid [%d:%s:%d]\n"),
					prec->exc_id, prec->name.c_str(), prec->awm_id));
		prec->flags |= EXC_FLAGS_AWM_VALID;
	}
	inline void setAwmInvalid(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("AWM  <= Inalid [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_AWM_VALID;
	}

	//--- AWM Wait
	inline bool isAwmWaiting(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_AWM_WAITING);
	}
	inline void setAwmWaiting(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("AWM  <= Waiting [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_AWM_WAITING;
	}
	inline void clearAwmWaiting(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("AWM  <= NOT Waiting [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_AWM_WAITING;
	}

	//--- Sync Mode Status
	inline bool isSyncMode(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_SYNC);
	}
	inline void setSyncMode(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("SYNC <= Enter [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_SYNC;
	}
	inline void clearSyncMode(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("SYNC <= Exit [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_SYNC;
	}

	//--- Sync Done
	inline bool isSyncDone(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_SYNC_DONE);
	}
	inline void setSyncDone(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("SYNC <= Done [%d:%s:%d]\n"),
					prec->exc_id, prec->name.c_str(), prec->awm_id));
		prec->flags |= EXC_FLAGS_EXC_SYNC_DONE;
	}
	inline void clearSyncDone(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("SYNC <= Pending [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_SYNC_DONE;
	}

	//--- EXC Registration status
	inline bool isRegistered(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_REGISTERED);
	}
	inline void setRegistered(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("EXC  <= Registered [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_REGISTERED;
	}
	inline void clearRegistered(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("EXC  <= Unregistered [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_REGISTERED;
	}

	//--- EXC Enable status
	inline bool isEnabled(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_ENABLED);
	}
	inline void setEnabled(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("EXC  <= Enabled [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_ENABLED;
	}
	inline void clearEnabled(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("EXC  <= Disabled [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_ENABLED;
	}

	//--- EXC Blocked status
	inline bool isBlocked(pregExCtx_t prec) const {
		return (prec->flags & EXC_FLAGS_EXC_BLOCKED);
	}
	inline void setBlocked(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("EXC  <= Blocked [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags |= EXC_FLAGS_EXC_BLOCKED;
	}
	inline void clearBlocked(pregExCtx_t prec) const {
		DB(fprintf(stderr, FMT_DBG("EXC  <= UnBlocked [%d:%s]\n"),
					prec->exc_id, prec->name.c_str()));
		prec->flags &= ~EXC_FLAGS_EXC_BLOCKED;
	}

	/**
	 * @brief Build a new RTLib
	 */
	BbqueRPC(void);

/******************************************************************************
 * Channel Dependant interface
 ******************************************************************************/

	virtual RTLIB_ExitCode_t _Init(
			const char *name) = 0;

	virtual RTLIB_ExitCode_t _Register(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Unregister(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Enable(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Disable(pregExCtx_t prec) = 0;

	virtual RTLIB_ExitCode_t _Set(
		const RTLIB_ExecutionContextHandler_t ech,
			RTLIB_Constraint_t* constraints,
			uint8_t count) = 0;

	virtual RTLIB_ExitCode_t _Clear(
			const RTLIB_ExecutionContextHandler_t ech) = 0;

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
	pid_t chTrdPid;

private:

	/**
	 * @brief The PID of the application using the library
	 *
	 * This keep track of the application which initialize the library. This
	 * PID could be exploited by the Barbeque RTRM to directly control
	 * applications accessing its managed resources.
	 */
	pid_t appTrdPid;

	/**
	 * @brief True if the library has been properly initialized
	 */
	bool initialized;

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
	static excMap_t exc_map;

	/**
	 * @brief An entry of the map of registered EXCs
	 */
	typedef std::pair<uint8_t, pregExCtx_t> excMapEntry_t;

	/**
	 * @brief Get the next available (and unique) Execution Context ID
	 */
	static uint8_t GetNextExcID();

	/**
	 * @brief Setup statistics for a new selecte AWM
	 */
	RTLIB_ExitCode_t SetupStatistics(pregExCtx_t prec);

	/**
	 * @brief Update statistics for the currently selected AWM
	 */
	RTLIB_ExitCode_t UpdateStatistics(pregExCtx_t prec);

	/**
	 * @brief Log execution statistics collected so far
	 */
	void DumpStats(pregExCtx_t prec, bool verbose = false);

	/**
	 * @brief Update sync time [ms] estimation for the currently AWM
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

};

} // namespace rtlib

} // namespace bbque

// Clean-up locally used definitions
#undef FMT_DBG

#endif /* end of include guard: BBQUE_RPC_H_ */

