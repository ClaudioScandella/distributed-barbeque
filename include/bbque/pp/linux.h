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

#ifndef BBQUE_LINUX_PP_H_
#define BBQUE_LINUX_PP_H_

#include <array>

#include "bbque/config.h"
#include "bbque/platform_proxy.h"
#include "bbque/command_manager.h"
#include "bbque/res/bitset.h"
#include "bbque/utils/extra_data_container.h"

#include <libcgroup.h>

#ifdef CONFIG_BBQUE_OPENCL
#include "bbque/pp/opencl.h"
#endif

/**
 * @brief Default MAX number of CPUs per socket
 */
#define DEFAULT_MAX_CPUS BBQUE_MAX_R_ID_NUM+1

/**
 * @brief Default MAX number of MEMs node per host
 */
#define DEFAULT_MAX_MEMS BBQUE_MAX_R_ID_NUM+1

/**
 * @brief The CGroup expected to assigne resources to BBQ
 *
 * Resources which are assigned to Barbeque for Run-Time Management
 * are expected to be define under this control group.
 * @note this CGroup should be given both "task" and "admin" permissions to
 * the UID used to run the BarbequeRTRM
 */
#define BBQUE_LINUXPP_CGROUP "user.slice"

/**
 * @brief The CGroup expected to define resources clusterization
 *
 * Resources assigned to Barbeque can be grouped into clusters, in a NUMA
 * machine a cluster usually correspond to a "node". The BarbequeRTRM will
 * consider this clusterization when scheduling applications by trying to keep
 * each application within a single cluster.
 */
#define BBQUE_LINUXPP_RESOURCES BBQUE_LINUXPP_CGROUP"/res"

/**
 * @brief The CGroup expected to define Clusters
 *
 * Resources managed by Barbeque are clusterized by grouping available
 * platform resources into CGroups which name start with this radix.
 */
#define BBQUE_LINUXPP_CLUSTER "node"

/**
 * @brief The namespace of the Linux platform integration module
 */
#define PLAT_LNX_ATTRIBUTE PLATFORM_PROXY_NAMESPACE".lnx"

namespace bb = bbque;
namespace ba = bbque::app;
namespace br = bbque::res;
namespace bu = bbque::utils;

namespace bbque {

/**
 * @brief The Linux Platform Proxy module
 * @ingroup sec20_pp_linux
 */
class LinuxPP : public PlatformProxy, public bb::CommandHandler {

public:


	struct CGroupPtrDlt {
		CGroupPtrDlt(void) {}
		void operator()(struct cgroup *ptr) const {
			cgroup_free(&ptr);
		}
	};


	LinuxPP();

	virtual ~LinuxPP();

private:

	/**
	 * @brief CFS bandwidth enforcement safety margin (default: 0%)
	 */
	int cfs_margin_pct    = 0;

	/**
	 * @brief CFS bandwidth enforcement threshold (default: 100%)
	 */
	int cfs_threshold_pct = 100;

#ifdef CONFIG_BBQUE_OPENCL
	OpenCLProxy & oclProxy;
#endif
	/**
	 * @brief Resource assignement bindings on a Linux machine
	 */
	typedef struct RLinuxBindings {
		/** Computing node, e.g. processor */
		unsigned short node_id = 0;
		/** Processing elements / CPU cores assigned */
		char *cpus = NULL;
		/** Memory nodes assigned */
		char *mems = NULL;
		/** Memory limits in bytes */
		char *memb = NULL;
		/** The percentage of CPUs time assigned */
		uint16_t amount_cpus = 0;
		/** The bytes amount of Socket MEMORY assigned */
		uint64_t amount_memb = 0;
		/** The CPU time quota assigned */
		uint32_t amount_cpuq = 0;
		/** The CPU time period considered for quota assignement */
		uint32_t amount_cpup = 0;
		RLinuxBindings(const uint8_t MaxCpusCount, const uint8_t MaxMemsCount) {
			// 3 chars are required for each CPU/MEM resource if formatted
			// with syntax: "nn,". This allows for up-to 99 resources per
			// cluster
			if (MaxCpusCount)
				cpus = (char*)calloc(3*MaxCpusCount, sizeof(char));
			if (MaxMemsCount)
				mems = (char*)calloc(3*MaxMemsCount, sizeof(char));
		}
		~RLinuxBindings() {
			free(cpus);
			free(mems);
			free(memb);
		}
	} RLinuxBindings_t;

	typedef std::shared_ptr<RLinuxBindings_t> RLinuxBindingsPtr_t;

	typedef struct CGroupData : public bu::PluginData_t {
		ba::AppPtr_t papp; /** The controlled application */
#define BBQUE_LINUXPP_CGROUP_PATH_MAX 128 // "user.slice/res/12345:ABCDEF:00";
		char cgpath[BBQUE_LINUXPP_CGROUP_PATH_MAX];
		struct cgroup *pcg;
		struct cgroup_controller *pc_cpu;
		struct cgroup_controller *pc_cpuset;
		struct cgroup_controller *pc_memory;

		CGroupData(ba::AppPtr_t pa) :
			bu::PluginData_t(PLAT_LNX_ATTRIBUTE, "cgroup"),
			papp(pa), pcg(NULL), pc_cpu(NULL),
			pc_cpuset(NULL), pc_memory(NULL) {
			snprintf(cgpath, BBQUE_LINUXPP_CGROUP_PATH_MAX,
					BBQUE_LINUXPP_RESOURCES"/%s",
					papp->StrId());
		}

		CGroupData(const char *cgp) :
			bu::PluginData_t(PLAT_LNX_ATTRIBUTE, "cgroup"),
			pcg(NULL), pc_cpu(NULL),
			pc_cpuset(NULL), pc_memory(NULL) {
			snprintf(cgpath, BBQUE_LINUXPP_CGROUP_PATH_MAX,
					"%s", cgp);
		}

		~CGroupData() {
			// Removing Kernel Control Group
			cgroup_delete_cgroup(pcg, 1);
			// Releasing libcgroup resources
			cgroup_free(&pcg);
		}

	} CGroupData_t;

	typedef std::shared_ptr<CGroupData_t> CGroupDataPtr_t;

	/**
	 * @brief the control group controller
	 *
	 * This is a reference to the controller used on a generic Linux host.
	 * So far we use the "cpuset" controller.
	 */
	const char *controller;

	/**
	 * @brief True if the target system supports CFS quota management
	 */
	bool cfsQuotaSupported;

	/**
	 * @brief The maximum number of CPUs per socket
	 */
	uint8_t MaxCpusCount;

	/**
	 * @brief The maximum number of MEMs per node
	 */
	uint8_t MaxMemsCount;

	/**
	 * @brief The "silos" CGroup
	 *
	 * The "silos" is a control group where are placed processe which have
	 * been scheduled. This CGroup is indended to be a resource constrained
	 * group which grants a bare minimun of resources for the controlling
	 * application to run the RTLib
	 */
	CGroupDataPtr_t psilos;


	/**
	 * @brief Set true if resources status should be refreshed
	 *
	 * This value is false just the first time this platform proxy is
	 * running. If false parsed resources produces a corresponding
	 * registration into the ResourceManager.
	 *
	 * After initialization its value is updated to true, and this
	 * platform proxy switch to refresh mode. In this mode, each new
	 * resources scanning is intended to notice changes on resources
	 * availability and thus trigger reservation or online/offline
	 * requests to the ResourceManager.
	 */
	bool refreshMode;

#ifdef CONFIG_TARGET_ARM_BIG_LITTLE
	/**
	 * @brief ARM big.LITTLE support: type of each CPU core
	 *
	 * If true, indicates that the related CPU cores is an
	 * high-performance one.
	 */
	std::array<bool, BBQUE_TARGET_CPU_CORES_NUM> highPerfCores = { {false} };

	void InitCoresType();
#endif

/**
 * @defgroup group_plt_prx Platform Proxy
 * @{
 * @name Linux Platform Proxy
 *
 * The linux platform proxy is provided to control resources of a genric
 * host machine running on top of a Linux kernel, version 2.6.24 or higher.
 *
 * For a comprensive introduction to resopurce management using Control Groups
 * please refers to the RedHat documentation available at this link:
 * http://docs.redhat.com/docs/en-US/Red_Hat_Enterprise_Linux/6/html/Resource_Management_Guide/index.html
 *
 * @name Basic Infrastructure and Initialization
 * @{
 */

	ExitCode_t RegisterCluster(RLinuxBindingsPtr_t prlb);
	ExitCode_t RegisterClusterCPUs(RLinuxBindingsPtr_t prlb);
	ExitCode_t RegisterClusterMEMs(RLinuxBindingsPtr_t prlb);
	ExitCode_t ParseNode(struct cgroup_file_info &entry);
	ExitCode_t ParseNodeAttributes(struct cgroup_file_info &entry,
			RLinuxBindingsPtr_t prlb);

	/**
	 * @brief Get total amount of memory (in KB) reading procfs attribute
	 */
	ExitCode_t GetSysMemoryTotal(uint64_t & mem_kb_tot);

	const char* _GetPlatformID();
	const char* _GetHardwareID();

	/**
	 * @brief Return true if the id is related to a "big" CPU core
	 */
	bool _isHighPerformance(uint16_t core_id) {
#ifdef CONFIG_TARGET_ARM_BIG_LITTLE
		return highPerfCores[core_id];
#else
		(void) core_id;
		return true;
#endif
	}

	/**
	 * @brief Parse the resources assigned to BarbequeRTRM by CGroup
	 *
	 * This method allows to parse a set of resources, assinged to Barbeque for
	 * run-time management, which are defined with a properly configure
	 * control group.
	 */
	ExitCode_t _LoadPlatformData();
	ExitCode_t _RefreshPlatformData();
	ExitCode_t _Setup(ba::AppPtr_t papp);
	ExitCode_t _Release(ba::AppPtr_t papp);

	ExitCode_t _ReclaimResources(ba::AppPtr_t papp);

	ExitCode_t _MapResources(ba::AppPtr_t papp, br::ResourceAssignmentMapPtr_t pres,
		br::RViewToken_t rvt, bool excl);

/**
 * @}
 * @}
 */

	ExitCode_t GetResourceMapping(
			ba::AppPtr_t papp,
			br::ResourceAssignmentMapPtr_t assign_map,
			RLinuxBindingsPtr_t prlb,
			BBQUE_RID_TYPE node_id,
			br::RViewToken_t rvt);

	ExitCode_t BuildCGroup(CGroupDataPtr_t &pcgd);

	ExitCode_t BuildSilosCG(CGroupDataPtr_t &pcgd);
	ExitCode_t BuildAppCG(ba::AppPtr_t papp, CGroupDataPtr_t &pcgd);

	ExitCode_t GetCGroupData(ba::AppPtr_t papp, CGroupDataPtr_t &pcgd);
	ExitCode_t SetupCGroup(CGroupDataPtr_t &pcgd, RLinuxBindingsPtr_t prlb,
			bool excl = false, bool move = true);

	int Unregister(const char *uid);
	int CommandsCb(int argc, char *argv[]);

};

} // namespace bbque

#endif // BBQUE_PP_LINUX_H_
