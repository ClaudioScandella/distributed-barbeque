/*
 * Copyright (C) 2019  Politecnico di Milano
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

#ifndef BBQUE_SCHEDULABLE_H_
#define BBQUE_SCHEDULABLE_H_

#include <memory>

#include "bbque/config.h"
#include "bbque/cpp11/mutex.h"

namespace bbque { namespace app {

/** The application identifier type */
typedef uint32_t AppPid_t;

/** The application priority type */
typedef uint16_t AppPrio_t;

// Forward declarations
class WorkingMode;
typedef std::shared_ptr<WorkingMode> AwmPtr_t;


class Schedulable {

public:
	/**
	 * @enum ExitCode_t
	 * @brief Error codes returned by methods
	 */
	enum ExitCode_t {
		APP_SUCCESS = 0,	/** Success */
		APP_DISABLED,   	/** Application being DISABLED */
		APP_FINISHED,   	/** Application being FINISHED */
		APP_STATUS_NOT_EXP,   	/** Application in unexpected status */
		APP_RECP_NULL,  	/** Null recipe object passed */
		APP_WM_NOT_FOUND,	/** Application working mode not found */
		APP_RSRC_NOT_FOUND,	/** Resource not found */
		APP_CONS_NOT_FOUND,	/** Constraint not found */
		APP_WM_REJECTED,	/** The working mode is not schedulable */
		APP_WM_ENAB_CHANGED,	/** Enabled working modes list has changed */
		APP_WM_ENAB_UNCHANGED,	/** Enabled working modes list has not changed */
		APP_TG_SEM_ERROR,	/** Error while accessing task-graph semaphore */
		APP_TG_FILE_ERROR,	/** Error while accessing task-graph serial file */
		APP_ABORT         	/** Unexpected error */
	};

	/**
	 * @enum State_t
	 * @brief A possible application state.
	 *
	 * This is the set of possible state an application could be.
	 */
	typedef enum State {
		DISABLED = 0, 	/** Registered within Barbeque but currently disabled */
		READY,      	/** Registered within Barbeque and waiting to start */
		SYNC,     	/** (Re-)scheduled but not reconfigured yet */
		RUNNING,  	/** Running */
		FINISHED, 	/** Regular termination */

		STATE_COUNT	/** This must alwasy be the last entry */
	} State_t;

	/**
	 * @enum SyncState_t
	 * @brief Required synchronization action
	 *
	 * Once a reconfiguration for an schedulable entity has been requested, one of these
	 * synchronization state is entered. Vice versa, if no reconfiguration
	 * are required, the SYNC_NONE (alias SYNC_STATE_COUNT) is assigned to
	 * the schedulable entity.
	 */
	typedef enum SyncState {
	// NOTE These values should be reported to match (in number and order)
	//      those defined by the RTLIB::RTLIB_ExitCode.
		STARTING = 0, 	/** The application is entering the system */
		RECONF ,      	/** Must change working mode */
		MIGREC,       	/** Must migrate and change working mode */
		MIGRATE,    	/** Must migrate into another cluster */
		BLOCKED,    	/** Must be blocked because of resource are not more available */

		SYNC_STATE_COUNT /** This must alwasy be the last entry */
	} SyncState_t;


#define SYNC_NONE SYNC_STATE_COUNT

	/**
	 * @struct SchedulingInfo_t
	 *
	 * Application scheduling informations.
	 * The scheduling of an application is characterized by a pair of
	 * information: the state (@see State_t), and the working mode
	 * choosed by the scheduler/optimizer module.
	 */
	struct SchedulingInfo_t {
		mutable std::recursive_mutex mtx; /** The mutex to serialize access to scheduling info */
		State_t state;            /** The current scheduled state */
		State_t preSyncState;     /** The state before a sync has been required */
		SyncState_t syncState;    /** The current synchronization state */
		AwmPtr_t awm;             /** The current application working mode */
		AwmPtr_t next_awm;        /** The next scheduled application working mode */
		uint64_t count = 0;       /** How many times the application has been scheduled */
		/**
		 * The metrics value set by the scheduling policy. The purpose of this
		 * attribute is to provide a support for the evaluation of the schedule
		 * results.
		 */
		float value;

		/** Overloading of operator != for structure comparisons */
		inline bool operator!=(SchedulingInfo_t const &other) const {
			return ((this->state != other.state) ||
					(this->preSyncState != other.preSyncState) ||
					(this->syncState != other.syncState) ||
					(this->awm != other.awm));
		};
	};

#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	struct CGroupSetupData_t {
		unsigned long cpu_ids;
		unsigned long cpus_ids_isolation;
		unsigned long mem_ids;
	};

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
#endif

	/**
	 * @brief Get the name of the application
	 * @return The name string
	 */
	virtual std::string const & Name() const noexcept { return name; }

	/**
	 * @brief Get the process ID of the application
	 * @return PID value
	 */
	virtual AppPid_t Pid() const noexcept { return pid; }

	/**
	 * @brief Get the priority associated to
	 * @return The priority value
	 */
	virtual	AppPrio_t Priority() const noexcept { return priority; }


	/**
	 * @brief Get the schedule state
	 * @return The current scheduled state
	 */
	virtual State_t State() const;

	/**
	 * @brief Get the pre-synchronization state
	 */
	virtual State_t PreSyncState() const;

	/**
	 * @brief Get the synchronization state
	 */
	virtual SyncState_t SyncState() const;

	/**
	 * @brief Verbose application state names
	 */
	static char const *stateStr[STATE_COUNT];

	/**
	 * @brief Verbose synchronization state names
	 */
	static char const *syncStateStr[SYNC_STATE_COUNT+1];


	/**
	 * @brief Check if this EXC is currently DISABLED
	 */
	virtual bool Disabled() const;

	/**
	 * @brief Check if this EXC is currently READY of RUNNING
	 */
	virtual bool Active() const;

	/**
	 * @brief Check if this EXC is currently RUNNING
	 */
	virtual bool Running() const;

	/**
	 * @brief Check if this EXC is currently in SYNC state
	 */
	virtual bool Synching() const;

	/**
	 * @brief Check if this EXC is currently STARTING
	 */
	virtual bool Starting() const;

	/**
	 * @brief Check if this EXC is being BLOCKED
	 */
	virtual bool Blocking() const;


	/**
	 * @brief Get the current working mode
	 * @return A shared pointer to the current application working mode
	 */
	virtual AwmPtr_t const & CurrentAWM() const;

	/**
	 * @brief Get next working mode to switch in when the application is
	 * re-scheduld
	 * @return A shared pointer to working mode descriptor (optimizer
	 * interface)
	 */
	virtual AwmPtr_t const & NextAWM() const;

	/**
	 * @brief Check if the current AWM is going to be changed
	 * @return true if the application is going to be reconfigured with
	 * 	also a change of the current AWM
	 */
	virtual bool SwitchingAWM() const noexcept;

	/**
	 * @brief Number of schedulations
	 */
	virtual uint64_t ScheduleCount() const noexcept;

// Scheduling


// Synchronization


protected:

	/** The application name */
	std::string name;

	/** The PID assigned from the OS */
	AppPid_t pid;

	/** A numeric priority value */
	AppPrio_t priority;

	/** Current scheduling informations */
	SchedulingInfo_t schedule;

#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	CGroupSetupData_t cgroup_data;
#endif


	virtual bool _Disabled() const;

	virtual bool _Active() const;

	virtual  bool _Running() const;

	virtual bool _Synching() const;

	virtual bool _Starting() const;

	virtual bool _Blocking() const;

	virtual State_t _State() const;

	virtual State_t _PreSyncState() const;

	virtual SyncState_t _SyncState() const;

	virtual AwmPtr_t const & _CurrentAWM() const;

	virtual AwmPtr_t const & _NextAWM() const;

	virtual bool _SwitchingAWM() const;

};


} // namespace app

} // namespace bbque

#endif // BBQUE_SCHEDULABLE_H_
