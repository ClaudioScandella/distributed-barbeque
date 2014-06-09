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

char *CGroups::mounts[CGC::COUNT] = { nullptr, };

CGroups::CGResult CGroups::Init(const char *logname, bool mount) {
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

	// Mounting all available controllers
	for (int id = CGC::CPUSET; id < CGC::COUNT; ++id) {
		result = cgroup_get_subsys_mount_point(controller[id], &mounts[id]);
		if (result) {
			logger->Error("CGroup controller [%s] mountpoint lookup FAILED! (Error: %d - %s)",
					controller[id], result, cgroup_strerror(result));
			mounts[id] = nullptr;
		} else {
			logger->Notice("CGroup controller [%s] available at [%s]",
				controller[id], mounts[id]);
		}

		// Mark initialized if at leat one Controller is available
		initialized = true;
	}

	return CGResult::OK;
}

bool CGroups::Exists(const char *cgpath) {
	struct cgroup *pcg;
	int result;

	// Get required CGroup path
	pcg = cgroup_new_cgroup(cgpath);
	if (!pcg) {
		logger->Error("CGroup [%s] creation FAILED", cgpath);
		return false;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(pcg);
	if (result != 0) {
		logger->Debug("CGroup [%s] read FAILED (Error: %d, %s)",
				cgpath, result, cgroup_strerror(result));
		return false;
	}

	cgroup_free(&pcg);
	return true;
}

CGroups::CGResult CGroups::Read(const char *cgpath, CGSetup &cgsetup) {
	struct cgroup_controller *pc_cpuset;
	struct cgroup_controller *pc_memory;
	struct cgroup_controller *pc_cpu;
	struct cgroup *pcg;
	char *attribute;
	int result;

	// Get required CGroup path
	pcg = cgroup_new_cgroup(cgpath);
	if (!pcg) {
		logger->Error("CGroup [%s] creation FAILED", cgpath);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(pcg);
	if (result != 0) {
		logger->Error("CGroup [%s] read FAILED (Error: %d, %s)",
				cgpath, result, cgroup_strerror(result));
		return CGResult::READ_FAILED;
	}


	// Get CPUSET configuration (if available)
	cgsetup.cpuset.cpus = nullptr;
	cgsetup.cpuset.mems = nullptr;
	if (!mounts[CGC::CPUSET])
		goto get_cpus;

	pc_cpuset = cgroup_get_controller(pcg, "cpuset");
	if (!pc_cpuset) {
		logger->Error("CGroup [%s]: get CPUSET controller FAILED", cgpath);
		return CGResult::GET_FAILED;
	}

	result = cgroup_get_value_string(pc_cpuset, "cpuset.cpus",
			&cgsetup.cpuset.cpus);
	if (result)
		logger->Error("CGroup [%s]: read [cpuset.cpus] FAILED", cgpath);
	result = cgroup_get_value_string(pc_cpuset, "cpuset.cpu_exclusive",
			&attribute);
	if (result)
		logger->Error("CGroup [%s]: read [cpuset.cpu_exclusive] FAILED", cgpath);
	if (attribute[0] == '1')
		cgsetup.cpuset.cpu_exclusive = true;

	result = cgroup_get_value_string(pc_cpuset, "cpuset.mems",
			&cgsetup.cpuset.mems);
	if (result)
		logger->Error("CGroup [%s]: read [cpuset.mems] FAILED", cgpath);
	result = cgroup_get_value_string(pc_cpuset, "cpuset.mem_exclusive",
			&attribute);
	if (result)
		logger->Error("CGroup [%s]: read [cpuset.mem_exclusive] FAILED", cgpath);
	if (attribute[0] == '1')
		cgsetup.cpuset.mem_exclusive = true;

	logger->Debug("CGroup [%s] => cpus: %s [%s], mems: %s [%s]",
			cgpath,
			cgsetup.cpuset.cpus, cgsetup.cpuset.cpu_exclusive ? "exc" : "shr",
			cgsetup.cpuset.mems, cgsetup.cpuset.mem_exclusive ? "exc" : "shr");
get_cpus:

	// Get CPU configuration (if available)
	cgsetup.cpu.cfs_period_us = nullptr;
	cgsetup.cpu.cfs_quota_us  = nullptr;
	if (!mounts[CGC::CPU])
		goto get_memory;

	pc_cpu = cgroup_get_controller(pcg, "cpu");
	if (!pc_cpu) {
		logger->Error("CGroup [%s]: get CPU controller FAILED", cgpath);
		return CGResult::GET_FAILED;
	}

	result = cgroup_get_value_string(pc_cpu, "cpu.cfs_period_us",
			&cgsetup.cpu.cfs_period_us);
	if (result)
		logger->Error("CGroup [%s]: read [cpu.cfs_pediod_us] FAILED", cgpath);
	result = cgroup_get_value_string(pc_cpu, "cpu.cfs_quota_us",
			&cgsetup.cpu.cfs_quota_us);
	if (result)
		logger->Error("CGroup [%s]: read [cpu.cfs_quota_us] FAILED", cgpath);

	logger->Debug("CGroup [%s] => cfs_period_us: %s, cfs_quota_us: %s",
			cgpath, cgsetup.cpu.cfs_period_us, cgsetup.cpu.cfs_quota_us);
get_memory:

	// Get MEMORY configuration (if available)
	cgsetup.memory.limit_in_bytes = nullptr;
	if (!mounts[CGC::MEMORY])
		goto done;

	pc_memory = cgroup_get_controller(pcg, "memory");
	if (!pc_memory) {
		logger->Error("CGroup [%s]: get MEMORY controller FAILED", cgpath);
		return CGResult::GET_FAILED;
	}

	result = cgroup_get_value_string(pc_memory, "memory.limit_in_bytes",
			&cgsetup.memory.limit_in_bytes);
	if (result)
		logger->Error("CGroup [%s]: read [memory.limit_in_bytes] FAILED", cgpath);

	logger->Debug("CGroup [%s] => limit_in_bytes: %s",
			cgpath, cgsetup.memory.limit_in_bytes);

done:

	cgroup_free(&pcg);
	return CGResult::OK;

}

CGroups::CGResult CGroups::CloneFromParent(const char *cgpath) {
	CGResult result = CGResult::OK;
	struct cgroup *pcg;
	int error;

	// Get required CGroup path
	pcg = cgroup_new_cgroup(cgpath);
	if (!pcg) {
		logger->Error("CGroup [%s] creation FAILED", cgpath);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	error = cgroup_create_cgroup_from_parent(pcg, 0);
	if (error != 0) {
		logger->Debug("CGroup [%s] clone FAILED (Error: %d, %s)",
				cgpath, error, cgroup_strerror(error));
		result = CGResult::CLONE_FAILED;
		goto exit_error;
	}

exit_error:
	cgroup_free(&pcg);
	return result; 
}

CGroups::CGResult CGroups::Create(const char *cgpath,
		const CGSetup &cgsetup) {
	struct cgroup_controller *pc_cpuset;
	struct cgroup_controller *pc_memory;
	struct cgroup_controller *pc_cpu;
	struct cgroup *pcg;
	int result;


	// Setup CGroup path for this application
	pcg = cgroup_new_cgroup(cgpath);
	if (!pcg) {
		logger->Error("CGroup resource mapping FAILED "
				"(Error: libcgroup, \"cgroup\" creation)");
		return CGResult::NEW_FAILED;
	}

	// Set CPUSET configuration (if available)
	if (!mounts[CGC::CPUSET]) {
		logger->Debug("CGroup [%s]: CPUSET controller not configured", cgpath);
		goto set_cpus;
	}

	pc_cpuset = cgroup_add_controller(pcg, "cpuset");
	if (!pc_cpuset) {
		logger->Error("CGroup [%s]: set CPUSET controller FAILED", cgpath);
		return CGResult::ADD_FAILED;
	}

	result = cgroup_set_value_string(pc_cpuset, "cpuset.cpus",
			cgsetup.cpuset.cpus);
	if (result)
		logger->Error("CGroup [%s]: write [cpuset.cpus] FAILED", cgpath);
	result = cgroup_set_value_string(pc_cpuset, "cpuset.cpu_exclusive",
			cgsetup.cpuset.cpu_exclusive ? "1" : "0");
	if (result)
		logger->Error("CGroup [%s]: write [cpuset.cpu_exclusive] FAILED", cgpath);

	result = cgroup_set_value_string(pc_cpuset, "cpuset.mems",
			cgsetup.cpuset.mems);
	if (result)
		logger->Error("CGroup [%s]: write [cpuset.mems] FAILED", cgpath);
	result = cgroup_set_value_string(pc_cpuset, "cpuset.mem_exclusive",
			cgsetup.cpuset.mem_exclusive ? "1" : "0");
	if (result)
		logger->Error("CGroup [%s]: write [cpuset.mem_exclusive] FAILED", cgpath);
set_cpus:

	// Set CPU configuration (if available)
	if (!mounts[CGC::CPU]) {
		logger->Debug("CGroup [%s]: CPU controller not configured", cgpath);
		goto set_memory;
	}

	pc_cpu = cgroup_add_controller(pcg, "cpu");
	if (!pc_cpu) {
		logger->Error("CGroup [%s]: set CPU controller FAILED", cgpath);
		return CGResult::ADD_FAILED;
	}

	result = cgroup_set_value_string(pc_cpu, "cpu.cfs_period_us",
			cgsetup.cpu.cfs_period_us);
	if (result)
		logger->Error("CGroup [%s]: write [cpu.cfs_pediod_us] FAILED", cgpath);

	result = cgroup_set_value_string(pc_cpu, "cpu.cfs_quota_us",
			cgsetup.cpu.cfs_quota_us);
	if (result)
		logger->Error("CGroup [%s]: write [cpu.cfs_quota_us] FAILED", cgpath);

set_memory:

	// Set MEMORY configuration (if available)
	if (!mounts[CGC::MEMORY]) {
		logger->Debug("CGroup [%s]: MEMORY controller not configured", cgpath);
		goto done;
	}

	pc_memory = cgroup_add_controller(pcg, "memory");
	if (!pc_memory) {
		logger->Error("CGroup [%s]: set MEMORY controller FAILED", cgpath);
		return CGResult::ADD_FAILED;
	}

	result = cgroup_set_value_string(pc_memory, "memory.limit_in_bytes",
			cgsetup.memory.limit_in_bytes);
	if (result)
		logger->Error("CGroup [%s]: write [memory.limit_in_bytes] FAILED", cgpath);

done:

	// Create the kernel-space CGroup
	// NOTE: the current libcg API is quite confuse and unclear
	// regarding the "ignore_ownership" second parameter
	logger->Notice("Create kernel CGroup [%s]", cgpath);
	result = cgroup_create_cgroup(pcg, 0);
	if (result && errno) {
		logger->Error("CGroup [%s] write FAILED (Error: %d, %s)",
				cgpath, result, cgroup_strerror(result));
		return CGResult::CREATE_FAILED;
	}

	cgroup_free(&pcg);
	return CGResult::OK;
}

CGroups::CGResult CGroups::Delete(const char *cgpath) {
	struct cgroup *pcg;
	int result;

	// Get required CGroup path
	pcg = cgroup_new_cgroup(cgpath);
	if (!pcg) {
		logger->Error("CGroup [%s] new FAILED", cgpath);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(pcg);
	if (result != 0) {
		logger->Error("CGroup [%s] get FAILED (Error: %d, %s)",
				cgpath, result, cgroup_strerror(result));
		return CGResult::READ_FAILED;
	}

	// Delete the kernel cgroup
	if (cgroup_delete_cgroup(pcg, 1) != 0) {
		logger->Error("CGroup [%s] delete FAILED", cgpath);
		return CGResult::DELETE_FAILED;
	}

	cgroup_free(&pcg);
	return CGResult::OK;
}

CGroups::CGResult CGroups::AttachMe(const char *cgpath) {
	struct cgroup *pcg;
	int result;

	// Get required CGroup path
	pcg = cgroup_new_cgroup(cgpath);
	if (!pcg) {
		logger->Error("CGroup [%s] new FAILED", cgpath);
		return CGResult::NEW_FAILED;
	}

	// Update the CGroup variable with kernel info
	result = cgroup_get_cgroup(pcg);
	if (result != 0) {
		logger->Error("CGroup [%s] get FAILED (Error: %d, %s)",
				cgpath, result, cgroup_strerror(result));
		return CGResult::READ_FAILED;
	}

	// Update the kernel cgroup
	result = cgroup_attach_task(pcg);
	if (result != 0) {
		logger->Error("CGroup [%s] attach FAILED", cgpath);
		return CGResult::ATTACH_FAILED;
	}

	cgroup_free(&pcg);
	return CGResult::OK;
}

} /* utils */

} /* bbque */
