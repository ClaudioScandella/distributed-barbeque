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

#include "bbque/app/process.h"
#include "bbque/command_manager.h"
#include "bbque/utils/logging/logger.h"


namespace bbque {

using ProcPtr_t    = std::shared_ptr<app::Process>;
using ProcessMap_t = std::map<app::AppPid_t, ProcPtr_t>;

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


	/**
	 * @brief Get the first process in the map
	 * @return A pointer to a process descriptor
	 */
	ProcPtr_t GetFirst() { return nullptr; }

	/**
	 * @brief Get the next process in the map
	 * @return A pointer to a process descriptor
	 */
	ProcPtr_t GetNext() { return nullptr; }


private:

	using PidSet_t    = std::set<app::AppPid_t>;
	using PidSetPtr_t = std::shared_ptr<PidSet_t>;

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

	/**  Command manager instance */
	CommandManager & cm;

	mutable std::mutex proc_mutex;

	/** The set containing the names of the managed processes */
	std::map<std::string, ProcessInstancesInfo> managed_procs;

	/** Processes to schedule */
	ProcessMap_t proc_ready;

	/** Processes scheduled but not synchronized yet */
	ProcessMap_t proc_to_sync;

	/** Processes currently running */
	ProcessMap_t proc_running;


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
	void CommandManageSetScheduleHelp() const;

};


} // namespace bbque

#endif // BBQUE_PROCESS_MANAGER_H_

