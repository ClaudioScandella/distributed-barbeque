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
#include <ctype.h>

#define MODULE_NAMESPACE "bq.prm"
#define MODULE_CONFIG    "ProcessManager"

#define PRM_MAX_ARG_LENGTH 15

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
			MODULE_NAMESPACE CMD_REMOVE_PROCESS,
			static_cast<CommandHandler*>(this),
			"Remove a managed process (by executable name)");
#define CMD_SETSCHED_PROCESS ".setsched"
	cm.RegisterCommand(
			MODULE_NAMESPACE CMD_SETSCHED_PROCESS,
			static_cast<CommandHandler*>(this),
			"Set a resource allocation request for a process/program");

	// Status vector size equal to numbe of possible process states
	state_procs.resize(app::Schedulable::STATE_COUNT);
}


int ProcessManager::CommandsCb(int argc, char *argv[]) {
	std::string command_name(argv[0]);
	std::string arg1;
	logger->Debug("CommandsCb: processing command <%s>", command_name.c_str());

	// bq.prm.add <process_name>
	if (command_name.compare(MODULE_NAMESPACE CMD_ADD_PROCESS) == 0) {
		if (argc < 1) {
		    logger->Error("CommandsCb: <%s> : missing argument", command_name.c_str());
		    return -1;
		}
		arg1.assign(argv[1]);
		logger->Info("CommandsCb: adding <%s> to managed processes", argv[1]);
		Add(arg1);
		return 0;
	}

	// bq.prm.remove <process_name>
	if (command_name.compare(MODULE_NAMESPACE CMD_REMOVE_PROCESS) == 0) {
		if (argc < 1) {
		    logger->Error("CommandsCb: <%s> : missing argument", command_name.c_str());
		    return -1;
		}

		arg1.assign(argv[1]);
		logger->Info("CommandsCb: removing <%s> from managed processes", argv[1]);
		Remove(arg1);
		return 0;
	}

	// bq.prm.setsched <process_name> ...
	if (command_name.compare(MODULE_NAMESPACE CMD_SETSCHED_PROCESS) == 0) {
		CommandManageSetSchedule(argc, argv);
		return 0;
	}

	logger->Error("CommandsCb: <%s> not supported by this module", command_name.c_str());
	return -1;
}


void ProcessManager::CommandManageSetSchedule(int argc, char * argv[]) {
	app::Process::ScheduleRequest sched_req;
	app::AppPid_t pid = 0;
	std::string name;
	char c;
	while ((c = getopt(argc, argv, "n:p:c:a:m:h")) != -1) {
		switch (c) {
		case 'n':
			if (optarg != nullptr) {
				name.assign(optarg);
				Add(name);
			}
			break;
		case 'p':
			if (optarg != nullptr)
				pid = atoi(optarg);
			break;
		case 'c':
			if (optarg != nullptr)
				sched_req.cpu_cores = atoi(optarg);
			break;
		case 'a':
			if (optarg != nullptr)
				sched_req.acc_cores = atoi(optarg);
			break;
		case 'm':
			if (optarg != nullptr)
				sched_req.memory_mb = atoi(optarg);
			break;
		case 'h':
		default :
			CommandManageSetScheduleHelp();
		}
	}

	if (name.empty()) {
		logger->Error("CommandsCb: wrong arguments specification");
		CommandManageSetScheduleHelp();
		return;
	}

	std::unique_lock<std::mutex> u_lock(proc_mutex);
	*(managed_procs[name].sched_req) = sched_req;
	logger->Notice("CommandsCb: <%s> schedule request: cpus=%d accs=%d mem=%d",
		name.c_str(),
		managed_procs[name].sched_req->cpu_cores,
		managed_procs[name].sched_req->acc_cores,
		managed_procs[name].sched_req->memory_mb);
}


void ProcessManager::CommandManageSetScheduleHelp() const {
	logger->Notice("%s -n=<process_name> [-p=<pid>] -c=<cpu_cores> "
		"[-a=<accelerator_cores>] [-m=<memory_MB>]",
		MODULE_NAMESPACE CMD_SETSCHED_PROCESS);
}


void ProcessManager::Add(std::string const & name) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	auto it = managed_procs.find(name);
	if (it == managed_procs.end()) {
		managed_procs.emplace(name, ProcessManager::ProcessInstancesInfo());
		logger->Debug("Add: processes with name '%s' in the managed map", name.c_str());
	}
	else
		logger->Debug("Add: processes with name '%s' already n the managed map", name.c_str());
}


void ProcessManager::Remove(std::string const & name) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	managed_procs.erase(name);
	logger->Debug("Remove: processes with name '%s' no longer in the managed map",
		name.c_str());
}


bool ProcessManager::IsToManage(std::string const & name) const {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	if (managed_procs.find(name) == managed_procs.end())
		return false;
	return true;
}


void ProcessManager::NotifyStart(std::string const & name, app::AppPid_t pid) {
	if (!IsToManage(name)) {
//		logger->Debug("NotifyStart: %s not managed", name.c_str());
		return;
	}
	logger->Info("NotifyStart: scheduling required for [%s: %d]", name.c_str(), pid);
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	managed_procs[name].pid_set->emplace(pid);
	state_procs[app::Schedulable::READY].emplace(
		pid, std::make_shared<Process>(name, pid));
}


void ProcessManager::NotifyStop(std::string const & name, app::AppPid_t pid) {
	if (!IsToManage(name)) {
	//	logger->Debug("NotifyStop: %s not managed", name.c_str());
		return;
	}
	logger->Debug("NotifyStop: process [%s: %d] terminated", name.c_str(), pid);
	std::unique_lock<std::mutex> u_lock(proc_mutex);

	// Remove from the status maps...
	for (auto state_it = state_procs.begin(); state_it != state_procs.end(); ++state_it) {
		auto & state_map(*state_it);
		auto proc_it = state_map.find(pid);
		if (proc_it != state_map.end()) {
			state_map.erase(proc_it);
			logger->Debug("NotifyStop: [%s: %d] removed from map",
				name.c_str(), pid);
		}
	}
}


bool ProcessManager::HasProcesses(app::Schedulable::State_t state) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	return !state_procs[state].empty();
}

ProcPtr_t ProcessManager::GetFirst(app::Schedulable::State_t state, ProcessMapIterator & it) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	auto & state_map(state_procs[state]);
	it = state_map.begin();
	if (it == state_map.end())
		return nullptr;
	return it->second;
}

ProcPtr_t ProcessManager::GetNext(app::Schedulable::State_t state, ProcessMapIterator & it) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	auto & state_map(state_procs[state]);
	it++;
	if (it == state_map.end())
		return nullptr;
	return it->second;
}


uint32_t ProcessManager::ProcessesCount(app::Schedulable::State_t state) {
	if (state >= app::Schedulable::STATE_COUNT)
		return -1;
	std::unique_lock<std::mutex> u_lock(proc_mutex);
	return state_procs[state].size();
}

/*******************************************************************************
 *     Synchronization functions
 ******************************************************************************/

ProcessManager::ExitCode_t ProcessManager::SyncCommit(ProcPtr_t proc) {
	logger->Debug("SyncCommit: [%s] changing to RUNNING...", proc->StrId());
	auto ret = ChangeState(proc, Schedulable::RUNNING);
	if (ret != SUCCESS) {
		logger->Error("SyncCommit: [%s] failed (state=%s)",
			proc->StrId(), proc->StateStr(proc->State()));
	}
	return ret;
}

ProcessManager::ExitCode_t ProcessManager::SyncAbort(ProcPtr_t proc) {
	logger->Debug("SyncAbort: [%s] changing to DISABLED...", proc->StrId());
	auto ret = ChangeState(proc, Schedulable::DISABLED);
	if (ret != SUCCESS) {
		logger->Error("SyncAbort: [%s] failed (state=%s)",
			proc->StrId(), proc->StateStr(proc->State()));
	}
	return ret;
}

ProcessManager::ExitCode_t ProcessManager::SyncContinue(ProcPtr_t proc) {
	logger->Debug("SyncContinue: [%s] continuing with RUNNING...", proc->StrId());
	if (proc->State() != Schedulable::RUNNING) {
		logger->Error("SyncContinue: [%s] wrong status (state=%s)",
			proc->StrId(), proc->StateStr(proc->State()));
		return PROCESS_NOT_SCHEDULABLE;
	}
	auto ret = ChangeState(proc, Schedulable::RUNNING);
	if (ret != SUCCESS) {
		logger->Error("SyncContinue: [%s] failed (state=%s)",
			proc->StrId(), proc->StateStr(proc->State()));
	}
	return ret;
}

ProcessManager::ExitCode_t ProcessManager::ChangeState(
		ProcPtr_t proc,
		Schedulable::State_t to_state,
		Schedulable::SyncState_t next_sync) {
	std::unique_lock<std::mutex> u_lock(proc_mutex);

	auto & from_map(state_procs[proc->State()]);
	auto proc_it = from_map.find(proc->Pid());
	if (proc_it == from_map.end()) {
		logger->Warn("ChangeState: process PID=%d not found in state=%s)",
			proc->Pid(), proc->StateStr(proc->State()));
		return PROCESS_NOT_FOUND;
	}

	if (proc->State() == to_state) {
		logger->Debug("ChangeState: process PID=%d already in state=%s",
			proc->Pid(), proc->StateStr(proc->State()));
		return SUCCESS;
	}
	else {
		auto & to_map(state_procs[to_state]);
		to_map.emplace(proc->Pid(), proc);
		from_map.erase(proc_it);
	}

	logger->Debug("ChangeState: [%s] state=%s sync=%s",
		proc->StrId(),
		proc->StateStr(proc->State()),
		proc->SyncStateStr(proc->SyncState()));

	proc->SetState(to_state, next_sync);
	logger->Debug("ChangeState: [%s] state=%s sync=%s",
		proc->StrId(),
		proc->StateStr(proc->State()),
		proc->SyncStateStr(proc->SyncState()));

	return SUCCESS;
}

} // namespace bbque

