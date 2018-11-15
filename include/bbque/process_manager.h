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

#ifndef BBQUE_PROCESS_MANAGER_H_
#define BBQUE_PROCESS_MANAGER_H_

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "bbque/app/process.h"
#include "bbque/command_manager.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/map_iterator.h"


namespace bbque {

using ProcPtr_t    = std::shared_ptr<app::Process>;
using ProcessMap_t = std::map<app::AppPid_t, ProcPtr_t>;
//using ProcessMapIterator = MapIterator<ProcPtr_t>;
using ProcessMapIterator = ProcessMap_t::iterator;

using namespace app;


class ProcessManager: public CommandHandler {

public:

	/**
	 * @enum ExitCode_t
	 * @brief Get the ProcessManager instance
	 */
	enum ExitCode_t {
		SUCCESS = 0,
		PROCESS_NOT_FOUND
	};

	/**
	 * @brief Get the ProcessManager instance
	 */
	static ProcessManager & GetInstance();

/*******************************************************************************
 *     Process manipulation
 ******************************************************************************/

	/**
	 * @brief Add a process to the managed map
	 * @param name the binary name
	 */
	void Add(std::string const & name);

	/**
	 * @brief Remove a process from the managed map
	 * @param name the binary name
	 */
	void Remove(std::string const & name);

	/**
	 * @brief Check if a program/process is in the management list
	 * @param name the binary name
	 */
	bool IsToManage(std::string const & name) const;

	/**
	 * @brief Notify the start of a process/program
	 * @param name the binary name
	 * @param pid the process id
	 */
	void NotifyStart(std::string const & name, app::AppPid_t pid);

	/**
	 * @brief Notify the start of a process/program
	 * @param name the binary name
	 * @param pid the process id
	 */
	void NotifyStop(std::string const & name, app::AppPid_t pid);

/*******************************************************************************
 *     Map iterations
 ******************************************************************************/

	/**
	 * @brief Check if there are processes in a given state
	 * @return true if yes, fals for not
	 */
	bool HasProcesses(app::Schedulable::State_t state);

	/**
	 * @brief Get the first process in the map
	 * @return A pointer to a process descriptor
	 */
	ProcPtr_t GetFirst(app::Schedulable::State_t state, ProcessMapIterator & it);

	/**
	 * @brief Get the next process in the map
	 * @return A pointer to a process descriptor
	 */
	ProcPtr_t GetNext(app::Schedulable::State_t state, ProcessMapIterator & it);


/*******************************************************************************
 *     Synchronization functions
 ******************************************************************************/

	/**
	 * @brief Commit the synchronization for the specified application
	 *
	 * @param proc the application which has been synchronized
	 *
	 * @return AM_SUCCESS on commit succesfull, AM_ABORT on errors.
	 */
	ExitCode_t SyncCommit(ProcPtr_t proc) {}

	/**
	 * @brief Abort the synchronization for the specified application
	 *
	 * @param proc the application which has been synchronized
	 */
	void SyncAbort(ProcPtr_t proc) {}

	/**
	 * @brief Commit the "continue to run" for the specified application
	 *
	 * @param proc a pointer to the interested application
	 * @return AM_SUCCESS on success, AM_ABORT on failure
	 */
	ExitCode_t SyncContinue(ProcPtr_t proc) {}


private:

	using PidSet_t    = std::set<app::AppPid_t>;
	using PidSetPtr_t = std::shared_ptr<PidSet_t>;

	/**
	 * @brief Runtime information about the instances of managed processes
	 */
	class ProcessInstancesInfo {
	public:
		ProcessInstancesInfo() {
			sched_req = std::make_shared<app::Process::ScheduleRequest>();
			pid_set   = std::make_shared<PidSet_t>();
		}
		app::Process::ScheduleRequestPtr_t sched_req; // Scheduling request
		PidSetPtr_t pid_set;                          // Set of PIDs of active process instances
	};

	/** The logger used by the application manager */
	std::unique_ptr<bu::Logger> logger;

	/** Command manager instance */
	CommandManager & cm;

	/** Mutex to protect processes maps */
	mutable std::mutex proc_mutex;

	/** The set containing the names of the managed processes */
	std::map<std::string, ProcessInstancesInfo> managed_procs;

	/** State vectors of the managed processes */
	std::vector<ProcessMap_t> state_procs;

	/**
	 * @brief Constructor
	 */
	ProcessManager();

	/**
	 * @brief The handler for commands defined by this module
	 */
	int CommandsCb(int argc, char *argv[]);

	/**
	 * @brief The handler for the command used to set a scheduling request
	 */
	void CommandManageSetSchedule(int argc, char * argv[]);

	/**
	 * @brief Help for the schedule request command
	 */
	void CommandManageSetScheduleHelp() const;

};


} // namespace bbque

#endif // BBQUE_PROCESS_MANAGER_H_

