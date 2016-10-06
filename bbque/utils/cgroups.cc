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

#include "bbque/utils/cgroups.h"

#include <cstring>
#include <linux/version.h>
#include <vector>

namespace bbque { namespace utils {

std::unique_ptr<bu::Logger> CGroups::logger;

// NOTE: this should match the CgcID enum
const char *CGroups::controller[] = {
	"cpuset",
	"cpu",
	"cpuacct",
	"memory",
	"devices",
	"freezer",
	"net_cls",
	"blkio",
	"perf_event",
	"hugetlb",
};

// Needed controllers
std::vector<int> controllers_IDs = {
	CGroups::CGC::CPUSET,
	CGroups::CGC::CPU,
	CGroups::CGC::CPUACCT,
	CGroups::CGC::MEMORY,
	// CGroups::CGC::DEVICES,
	// CGroups::CGC::FREEZER,
	// CGroups::CGC::NET_CLS,
	// CGroups::CGC::BLKIO,
	// CGroups::CGC::PERF_EVENT,
	// CGroups::CGC::HUGETLB
};

char *CGroups::mounts[CGC::COUNT] = { nullptr, };

CGroups::CGResult CGroups::Init(const char *logname) {
	static bool initialized = false;
	int result;

	if (initialized)
		return CGResult::OK;

	//---------- Get a logger module
	logger = Logger::GetLogger(logname);
	assert(logger);
	// Init the Control Group Library
	result = cgroup_init();
	if (result) {
		logger->Error("CGroup Library initializaton FAILED! "
				"(Error: %d - %s)", result, cgroup_strerror(result));
		return CGResult::INIT_FAILED;
	}

	// Mounting all needed controllers
	for (int id : controllers_IDs) {
		result = cgroup_get_subsys_mount_point(controller[id], &mounts[id]);
		if (result) {
			logger->Error("CGroup controller [%s] mountpoint lookup FAILED! (Error: %d - %s)",
					controller[id], result, cgroup_strerror(result));
			mounts[id] = nullptr;
		} else {
			logger->Debug("CGroup controller [%s] available at [%s]",
				controller[id], mounts[id]);
		}

		// Mark initialized if at leat one Controller is available
		initialized = true;
	}

	return CGResult::OK;
}

bool CGroups::Exists(const char *cgroup_path) {
	struct cgroup *cgroup_handler;
	int result;

	// Get required CGroup path
	cgroup_handler = cgroup_new_cgroup(cgroup_path);

	if (! cgroup_handler) {
		logger->Error("CGroups::Exists::cgroup_new_cgroup [%s] FAILED", cgroup_path);
		return false;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(cgroup_handler);

	if (result != 0) {
		logger->Error("CGroups::Exists::cgroup_get_cgroup [%s] FAILED (Error: %d, %s)",
					  cgroup_path, result, cgroup_strerror(result));
		return false;
	}

	cgroup_free(&cgroup_handler);
	return true;
}

CGroups::CGResult CGroups::Read(const char * cgroup_path, CGSetup & cgroup_data)
{
	int result = 0;
	struct cgroup * cgroup_handler = cgroup_new_cgroup(cgroup_path);

	if (! cgroup_handler) {
		logger->Error("CGroups::Read::cgroup_new_cgroup [%s] FAILED", cgroup_path);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(cgroup_handler);

	if (result != 0) {
		logger->Error("CGroups::Read::cgroup_get_cgroup [%s] FAILED (Error: %d, %s)",
					  cgroup_path, result, cgroup_strerror(result));
		return CGResult::READ_FAILED;
	}

	struct cgroup_controller * cpuset_controller;

	struct cgroup_controller * memory_controller;

	struct cgroup_controller * cpu_controller;

	if (mounts[CGC::CPUSET]) {
		cpuset_controller = cgroup_get_controller(cgroup_handler, "cpuset");

		if (! cpuset_controller) {
			logger->Error("CGroups::Read::cgroup_get_controller CPUSET [%s] FAILED",
						  cgroup_path);
			return CGResult::GET_FAILED;
		}

		char *cpuset_cpus;
		char *cpuset_mems;

		cgroup_get_value_string(cpuset_controller, "cpuset.cpus",
						&cpuset_cpus);
		cgroup_get_value_string(cpuset_controller, "cpuset.mems",
						&cpuset_mems);

		cgroup_data.cpuset.cpus = std::string(cpuset_cpus);
		cgroup_data.cpuset.mems = std::string(cpuset_mems);
	}

	if (mounts[CGC::CPU]) {
		cpu_controller = cgroup_get_controller(cgroup_handler, "cpu");

		if (! cpu_controller) {
			logger->Error("CGroups::Read::cgroup_get_controller CPU [%s] FAILED",
						  cgroup_path);
			return CGResult::GET_FAILED;
		}

		char *cpu_cfs_period_us;
		char *cpu_cfs_quota_us;

		cgroup_get_value_string(cpu_controller, "cpu.cfs_period_us",
						&cpu_cfs_period_us);
		cgroup_get_value_string(cpu_controller, "cpu.cfs_quota_us",
						&cpu_cfs_quota_us);

		cgroup_data.cpu.cfs_period_us = std::string(cpu_cfs_period_us);
		cgroup_data.cpu.cfs_quota_us = std::string(cpu_cfs_quota_us);
	}

	if (mounts[CGC::MEMORY]) {
		memory_controller = cgroup_get_controller(cgroup_handler, "memory");

		if (! memory_controller) {
			logger->Error("CGroups::Read::cgroup_get_controller MEMORY [%s] FAILED",
						  cgroup_path);
			return CGResult::GET_FAILED;
		}

		char *mem_limit_in_bytes;
		cgroup_get_value_string(memory_controller, "memory.limit_in_bytes",
						&mem_limit_in_bytes);

		cgroup_data.memory.limit_in_bytes = std::string(mem_limit_in_bytes);
	}

	cgroup_free(&cgroup_handler);
	return CGResult::OK;
}

CGroups::CGResult CGroups::CloneFromParent(const char * cgroup_path)
{
	struct cgroup * cgroup_handler = cgroup_new_cgroup(cgroup_path);
	cgroup_create_cgroup_from_parent(cgroup_handler, 1);
	cgroup_free(&cgroup_handler);
	return CGResult::OK;
}

CGroups::CGResult CGroups::WriteCgroup(
	const char * cgroup_path,
	const CGSetup & cgroup_data,
	int pid)
{
	int result = 0;
	struct cgroup * cgroup_handler = cgroup_new_cgroup(cgroup_path);

	if (! cgroup_handler) {
		logger->Error("CGroups::WriteCgroup::cgroup_new_cgroup [%s] FAILED",
					  cgroup_path);
		return CGResult::NEW_FAILED;
	}

	struct cgroup_controller * cpuset_controller;

	struct cgroup_controller * memory_controller;

	struct cgroup_controller * cpu_controller;

	cpuset_controller = cgroup_add_controller(cgroup_handler, "cpuset");

	if (mounts[CGC::CPUSET]) {

		if (! cpuset_controller) {
			logger->Error("CGroups::WriteCgroup::cgroup_get_controller CPUSET [%s] FAILED",
						  cgroup_path);
			return CGResult::GET_FAILED;
		}

		cgroup_set_value_string(cpuset_controller, "cpuset.cpus",
						cgroup_data.cpuset.cpus.c_str());
		cgroup_set_value_string(cpuset_controller, "cpuset.mems",
						cgroup_data.cpuset.mems.c_str());
	} else {
		cpuset_controller = cgroup_get_controller(cgroup_handler, "cpuset");
	}

	if (mounts[CGC::CPU]) {
		cpu_controller = cgroup_add_controller(cgroup_handler, "cpu");

		if (! cpu_controller) {
			logger->Error("CGroups::WriteCgroup::cgroup_get_controller CPU [%s] FAILED",
						  cgroup_path);
			return CGResult::GET_FAILED;
		}

		cgroup_set_value_string(cpu_controller, "cpu.cfs_period_us",
						cgroup_data.cpu.cfs_period_us.c_str());
		cgroup_set_value_string(cpu_controller, "cpu.cfs_quota_us",
						cgroup_data.cpu.cfs_quota_us.c_str());
	}

	if (mounts[CGC::MEMORY]) {
		memory_controller = cgroup_add_controller(cgroup_handler, "memory");

		if (! memory_controller) {
			logger->Error("CGroups::WriteCgroup::cgroup_get_controller MEMORY [%s] FAILED",
						  cgroup_path);
			return CGResult::GET_FAILED;
		}

		cgroup_set_value_string(memory_controller, "memory.limit_in_bytes",
						cgroup_data.memory.limit_in_bytes.c_str());
	}

	if (pid == 0) {
		// Create the kernel-space CGroup
		// NOTE: the current libcg API is quite confuse and unclear
		// regarding the "ignore_ownership" second parameter
		logger->Debug("Create kernel CGroup [%s]", cgroup_path);
		result = cgroup_create_cgroup(cgroup_handler, 1);

		if (result && errno) {
			logger->Error("CGroups::WriteCgroup::cgroup_create_cgroup [%s] FAILED (Error: %d, %s)",
						  cgroup_path, result, cgroup_strerror(result));
			return CGResult::CREATE_FAILED;
		}
	}
	else {
		// Create the kernel-space CGroup
		logger->Debug("Update kernel CGroup [%s]", cgroup_path);
		result = cgroup_modify_cgroup(cgroup_handler);

		if (result && errno) {
			logger->Error("CGroups::WriteCgroup::cgroup_modify_cgroup [%s] FAILED during update (Error: %d, %s)",
						  cgroup_path, result, cgroup_strerror(result));
			return CGResult::CREATE_FAILED;
		}

		cgroup_set_value_uint64(cpuset_controller,
								"cgroup.procs",
								pid);
		// Create the kernel-space CGroup
		logger->Debug("Update kernel CGroup [%s]", cgroup_path);
		result = cgroup_modify_cgroup(cgroup_handler);

		if (result && errno) {
			logger->Error("CGroups::WriteCgroup::cgroup_modify_cgroup [%s] FAILED during task attach (Error: %d, %s)",
						  cgroup_path, result, cgroup_strerror(result));
			return CGResult::CREATE_FAILED;
		}
	}

	cgroup_free(&cgroup_handler);
	return CGResult::OK;
}

CGroups::CGResult CGroups::Delete(const char *cgroup_path) {
	struct cgroup *cgroup_handler;
	int result;

	// Get required CGroup path
	cgroup_handler = cgroup_new_cgroup(cgroup_path);
	if (!cgroup_handler) {
		logger->Error("CGroup [%s] new FAILED", cgroup_path);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(cgroup_handler);

	if (result != 0) {
		logger->Error("CGroup [%s] get FAILED (Error: %d, %s)",
				cgroup_path, result, cgroup_strerror(result));
		return CGResult::READ_FAILED;
	}

	// Delete the kernel cgroup
	if (cgroup_delete_cgroup(cgroup_handler, 1) != 0) {
		logger->Error("CGroup [%s] delete FAILED", cgroup_path);
		return CGResult::DELETE_FAILED;
	}

	cgroup_free(&cgroup_handler);
	return CGResult::OK;
}

} /* utils */

} /* bbque */
