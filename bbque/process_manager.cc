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

#include "bbque/process_manager.h"

#include <cstring>

#define MODULE_NAMESPACE "bq.prm"
#define MODULE_CONFIG    "ProcessManager"

using namespace bbque::app;


namespace bbque {


ProcessManager & ProcessManager::GetInstance() {
	static ProcessManager instance;
	return instance;
}

ProcessManager::ProcessManager():
		cm(CommandManager::GetInstance()) {
	// Get a logger
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	assert(logger);

	// Register commands
#define CMD_ADD_PROCESS ".add"
	cm.RegisterCommand(
			MODULE_NAMESPACE CMD_ADD_PROCESS,
			static_cast<CommandHandler*>(this),
			"Add a process to manage (by executable name)");
#define CMD_REMOVE_PROCESS ".remove"
	cm.RegisterCommand(
			MODULE_NAMESPACE CMD_ADD_PROCESS,
			static_cast<CommandHandler*>(this),
			"Remove a managed process (by executable name)");
}


int ProcessManager::CommandsCb(int argc, char *argv[]) {
	uint8_t cmd_offset = ::strlen(MODULE_NAMESPACE) + 1;
	std::string command_name(argv[0]);
	logger->Debug("CommandsCb: processing command <%s>", command_name.c_str());

	if (command_name.compare(MODULE_NAMESPACE CMD_ADD_PROCESS) == 0) {
		if (argc < 1) {
		    logger->Error("CommandsCb: <%s> : missing argument", command_name.c_str());
		    return -1;
		}

		logger->Info("CommandsCb: adding <%s> to managed processes", argv[1]);
		Add(argv[1]);
		return 0;
	}
/*
	cmd_offset = ::strlen(MODULE_NAMESPACE) + sizeof(""); // ...
	switch (argv[0][cmd_offset]) {
	case 'w':
		logger->Debug("Commands: # recipes = %d", recipes.size());
		logger->Info("Commands: wiping out all the recipes...");
		recipes.clear();
		logger->Debug("Commands: # recipes = %d", recipes.size());
		return 0;
	}
*/
	logger->Error("CommandsCb: <%s> not supported by this module", command_name.c_str());
	return -1;
}


void ProcessManager::Add(std::string const & name) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	managed_proc_names.emplace(name);
	logger->Debug("Add: processes with name '%s' in the managed list", name.c_str());
}


void ProcessManager::Remove(std::string const & name) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	managed_proc_names.erase(name);
	logger->Debug("Remove: processes with name '%s' no longer in the managed list",
		name.c_str());
}


bool ProcessManager::IsToManage(std::string const & name) const {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	if (managed_proc_names.find(name) == managed_proc_names.end())
		return false;
	return true;
}


void ProcessManager::NotifyStart(std::string const & name, app::AppPid_t pid) {
	if (!IsToManage(name)) {
		logger->Debug("NotifyStart: %s not managed", name.c_str());
		return;
	}
	logger->Debug("NotifyStart: scheduling required for '%s'", name.c_str());
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	proc_to_schedule.emplace(pid, std::make_shared<Process>(pid));
}


void ProcessManager::NotifyStop(std::string const & name, app::AppPid_t pid) {
	if (!IsToManage(name)) {
		logger->Debug("NotifyStop: %s not managed", name.c_str());
		return;
	}
	logger->Debug("NotifyStop: scheduling required for '%s'", name.c_str());
	//std::unique_lock<std::mutex> u_lock(proc_mutex);
	//proc_to_schedule.emplace(pid, std::make_shared<Process>(pid));
}

} // namespace bbque
