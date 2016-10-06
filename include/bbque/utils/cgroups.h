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

#ifndef BBQUE_CGROUPS_H_
#define BBQUE_CGROUPS_H_

#include "bbque/config.h"
#include "bbque/utils/utility.h"
#include "bbque/utils/logging/logger.h"

#ifdef CONFIG_EXTERNAL_LIBCG
# include <libcgroup.h>
#endif

namespace bu = bbque::utils;

namespace bbque { namespace utils {

/**
 * @class CGroups
 * @brief The CGroup Support
 */
class CGroups {

public:

	enum class CGResult {
		OK = 0,
		ERROR,
		INIT_FAILED,
		MOUNT_FAILED,
		NEW_FAILED,
		CREATE_FAILED,
		DELETE_FAILED,
		CLONE_FAILED,
		ADD_FAILED,
		GET_FAILED,
		READ_FAILED,
		WRITE_FAILED,
		ATTACH_FAILED,
	};


	enum CGC {
		CPUSET = 0,
		CPU,
		CPUACCT,
		MEMORY,
		DEVICES,
		FREEZER,
		NET_CLS,
		BLKIO,
		PERF_EVENT,
		HUGETLB,

		// This must be the last entry
		COUNT
	};

	struct CGData {
		struct cgroup *cgroup_handler;
		struct cgroup_controller *pc_cpuset;
		struct cgroup_controller *pc_cpu;
		struct cgroup_controller *pc_cpuacct;
		struct cgroup_controller *pc_memory;
	};

	struct CGSetup {
		// CPUSET controller
		struct {
			std::string cpus;
			std::string mems;
		} cpuset;
		// CPU controller
		struct {
			std::string cfs_period_us;
			std::string cfs_quota_us;
#define CGSETUP_CPU_CFS_QUOTA_NOLIMITS "-1"
		} cpu;
		// MEMORY controller
		struct {
#define CGSETUP_MEMORY_NOLIMITS "18446744073709551615"
			std::string limit_in_bytes;
		} memory;
	};

	static CGResult Init(const char *logname);

	static bool Exists(const char *cgpath);

	static CGResult Read(const char *cgpath, CGSetup &cgroup_data);
	static CGResult CloneFromParent(const char *cgpath);
	static CGResult WriteCgroup(
		const char *cgpath,
		const CGSetup &cgroup_data,
		int pid);
	static CGResult Delete(const char *cgpath);

private:

	// This class has just static methods
	CGroups() {}

	/**
	 * @brief The logger used by this module
	 */
	static std::unique_ptr<bu::Logger> logger;

	static const char *controller[];

	static char *mounts[];

};

} // namespace utils

} // namespace bbque

#endif // BBQUE_CGROUPS_H_
