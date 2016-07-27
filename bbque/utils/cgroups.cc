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
std::vector<int> cid = {
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
	for (int id : cid) {
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
	if (!cgroup_handler) {
		logger->Error("CGroup [%s] creation FAILED", cgroup_path);
		return false;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(cgroup_handler);
	if (result != 0) {
		logger->Debug("CGroup [%s] read FAILED (Error: %d, %s)",
				cgroup_path, result, cgroup_strerror(result));
		return false;
	}

	cgroup_free(&cgroup_handler);
	return true;
}

CGroups::CGResult CGroups::Read(const char *cgroup_path, CGSetup &cgroup_data) {
	struct cgroup_controller *cpuset_controller;
	struct cgroup_controller *memory_controller;
	struct cgroup_controller *cpu_controller;
	struct cgroup *cgroup_handler;
	int result;

	// Get required CGroup path
	cgroup_handler = cgroup_new_cgroup(cgroup_path);
	if (!cgroup_handler) {
		logger->Error("CGroup [%s] creation FAILED", cgroup_path);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(cgroup_handler);
	if (result != 0) {
		logger->Error("CGroup [%s] read FAILED (Error: %d, %s)",
				cgroup_path, result, cgroup_strerror(result));
		return CGResult::READ_FAILED;
	}


	// Get CPUSET configuration (if available)
	cgroup_data.cpuset.cpus = nullptr;
	cgroup_data.cpuset.mems = nullptr;
	if (!mounts[CGC::CPUSET])
		goto get_cpus;

	cpuset_controller = cgroup_get_controller(cgroup_handler, "cpuset");
	if (!cpuset_controller) {
		logger->Error("CGroup [%s]: get CPUSET controller FAILED", cgroup_path);
		return CGResult::GET_FAILED;
	}

	result = cgroup_get_value_string(cpuset_controller, "cpuset.cpus",
			&cgroup_data.cpuset.cpus);
	if (result)
		logger->Error("CGroup [%s]: read [cpuset.cpus] FAILED", cgroup_path);
	result = cgroup_get_value_string(cpuset_controller, "cpuset.mems",
			&cgroup_data.cpuset.mems);
	if (result)
		logger->Error("CGroup [%s]: read [cpuset.mems] FAILED", cgroup_path);

	logger->Debug("CGroup [%s] => cpus: %s, mems: %s",
			cgroup_path, cgroup_data.cpuset.cpus, cgroup_data.cpuset.mems);
get_cpus:

	// Get CPU configuration (if available)
	cgroup_data.cpu.cfs_period_us = nullptr;
	cgroup_data.cpu.cfs_quota_us  = nullptr;
	if (!mounts[CGC::CPU])
		goto get_memory;

	cpu_controller = cgroup_get_controller(cgroup_handler, "cpu");
	if (!cpu_controller) {
		logger->Error("CGroup [%s]: get CPU controller FAILED", cgroup_path);
		return CGResult::GET_FAILED;
	}

	result = cgroup_get_value_string(cpu_controller, "cpu.cfs_period_us",
			&cgroup_data.cpu.cfs_period_us);
	if (result)
		logger->Error("CGroup [%s]: read [cpu.cfs_pediod_us] FAILED", cgroup_path);
	result = cgroup_get_value_string(cpu_controller, "cpu.cfs_quota_us",
			&cgroup_data.cpu.cfs_quota_us);
	if (result)
		logger->Error("CGroup [%s]: read [cpu.cfs_quota_us] FAILED", cgroup_path);

	logger->Debug("CGroup [%s] => cfs_period_us: %s, cfs_quota_us: %s",
			cgroup_path, cgroup_data.cpu.cfs_period_us, cgroup_data.cpu.cfs_quota_us);
get_memory:

	// Get MEMORY configuration (if available)
	cgroup_data.memory.limit_in_bytes = nullptr;
	if (!mounts[CGC::MEMORY])
		goto done;

	memory_controller = cgroup_get_controller(cgroup_handler, "memory");
	if (!memory_controller) {
		logger->Error("CGroup [%s]: get MEMORY controller FAILED", cgroup_path);
		return CGResult::GET_FAILED;
	}

	result = cgroup_get_value_string(memory_controller, "memory.limit_in_bytes",
			&cgroup_data.memory.limit_in_bytes);
	if (result)
		logger->Error("CGroup [%s]: read [memory.limit_in_bytes] FAILED", cgroup_path);

	logger->Debug("CGroup [%s] => limit_in_bytes: %s",
			cgroup_path, cgroup_data.memory.limit_in_bytes);

done:

	cgroup_free(&cgroup_handler);
	return CGResult::OK;

}

CGroups::CGResult CGroups::CloneFromParent(const char *cgroup_path) {
	struct cgroup *cgroup_handler;
	int result;

	// Get required CGroup path
	cgroup_handler = cgroup_new_cgroup(cgroup_path);
	if (!cgroup_handler) {
		logger->Error("CGroup [%s] creation FAILED", cgroup_path);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_create_cgroup_from_parent(cgroup_handler, 0);
	if (result != 0) {
		logger->Debug("CGroup [%s] clone FAILED (Error: %d, %s)",
				cgroup_path, result, cgroup_strerror(result));
		return CGResult::CLONE_FAILED;
	}

	cgroup_free(&cgroup_handler);
	return CGResult::OK;
}

CGroups::CGResult CGroups::Create(const char *cgroup_path,
		const CGSetup &cgroup_data) {
	struct cgroup_controller *cpuset_controller;
	struct cgroup_controller *memory_controller;
	struct cgroup_controller *cpu_controller;
	struct cgroup *cgroup_handler;
	int result;


	// Setup CGroup path for this application
	cgroup_handler = cgroup_new_cgroup(cgroup_path);
	if (!cgroup_handler) {
		logger->Error("CGroup resource mapping FAILED "
				"(Error: libcgroup, \"cgroup\" creation)");
		return CGResult::NEW_FAILED;
	}

	// Set CPUSET configuration (if available)
	if (!mounts[CGC::CPUSET]) {
		logger->Debug("CGroup [%s]: CPUSET controller not configured", cgroup_path);
		goto set_cpus;
	}

	cpuset_controller = cgroup_add_controller(cgroup_handler, "cpuset");
	if (!cpuset_controller) {
		logger->Error("CGroup [%s]: set CPUSET controller FAILED", cgroup_path);
		return CGResult::ADD_FAILED;
	}

	result = cgroup_set_value_string(cpuset_controller, "cpuset.cpus",
			cgroup_data.cpuset.cpus);
	if (result)
		logger->Error("CGroup [%s]: write [cpuset.cpus] FAILED", cgroup_path);

	result = cgroup_set_value_string(cpuset_controller, "cpuset.mems",
			cgroup_data.cpuset.mems);
	if (result)
		logger->Error("CGroup [%s]: write [cpuset.mems] FAILED", cgroup_path);

set_cpus:

	// Set CPU configuration (if available)
	if (!mounts[CGC::CPU]) {
		logger->Debug("CGroup [%s]: CPU controller not configured", cgroup_path);
		goto set_memory;
	}

	cpu_controller = cgroup_add_controller(cgroup_handler, "cpu");
	if (!cpu_controller) {
		logger->Error("CGroup [%s]: set CPU controller FAILED", cgroup_path);
		return CGResult::ADD_FAILED;
	}

	result = cgroup_set_value_string(cpu_controller, "cpu.cfs_period_us",
			cgroup_data.cpu.cfs_period_us);
	if (result)
		logger->Error("CGroup [%s]: write [cpu.cfs_pediod_us] FAILED", cgroup_path);

	result = cgroup_set_value_string(cpu_controller, "cpu.cfs_quota_us",
			cgroup_data.cpu.cfs_quota_us);
	if (result)
		logger->Error("CGroup [%s]: write [cpu.cfs_quota_us] FAILED", cgroup_path);

set_memory:

	// Set MEMORY configuration (if available)
	if (!mounts[CGC::MEMORY]) {
		logger->Debug("CGroup [%s]: MEMORY controller not configured", cgroup_path);
		goto done;
	}

	memory_controller = cgroup_add_controller(cgroup_handler, "memory");
	if (!memory_controller) {
		logger->Error("CGroup [%s]: set MEMORY controller FAILED", cgroup_path);
		return CGResult::ADD_FAILED;
	}

	result = cgroup_set_value_string(memory_controller, "memory.limit_in_bytes",
			cgroup_data.memory.limit_in_bytes);
	if (result)
		logger->Error("CGroup [%s]: write [memory.limit_in_bytes] FAILED", cgroup_path);

done:

	// Create the kernel-space CGroup
	// NOTE: the current libcg API is quite confuse and unclear
	// regarding the "ignore_ownership" second parameter
	logger->Notice("Create kernel CGroup [%s]", cgroup_path);
	result = cgroup_create_cgroup(cgroup_handler, 0);
	if (result && errno) {
		logger->Error("CGroup [%s] write FAILED (Error: %d, %s)",
				cgroup_path, result, cgroup_strerror(result));
		return CGResult::CREATE_FAILED;
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

CGroups::CGResult CGroups::AttachMe(const char *cgroup_path) {
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

	// Update the kernel cgroup
	result = cgroup_attach_task(cgroup_handler);
	if (result != 0) {
		logger->Error("CGroup [%s] attach FAILED", cgroup_path);
		return CGResult::ATTACH_FAILED;
	}

	cgroup_free(&cgroup_handler);
	return CGResult::OK;
}

} /* utils */

} /* bbque */
