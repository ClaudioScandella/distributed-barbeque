#ifndef BBQUE_LINUX_PLATFORM_PROXY_H
#define BBQUE_LINUX_PLATFORM_PROXY_H

#include "bbque/platform_proxy.h"
#include "bbque/utils/logging/logger.h"

#define LINUX_PP_NAMESPACE "bq.pp.linux"

// The _types.h file must be included after the
// constants define
#include "bbque/pp/linux_platform_proxy_types.h"


namespace bbque {
namespace pp {

class LinuxPlatformProxy : public PlatformProxy
{
public:

	static LinuxPlatformProxy * GetInstance();

	virtual ~LinuxPlatformProxy();

	/**
	 * @brief Return the Platform specific string identifier
	 */
	const char* GetPlatformID(int16_t system_id=-1) const noexcept override final;

	/**
	 * @brief Return the Hardware identifier string
	 */
	const char* GetHardwareID(int16_t system_id=-1) const noexcept override final;

	/**
	 * @brief Platform specific resource setup interface.
	 */
	ExitCode_t Setup(AppPtr_t papp) noexcept override final;

	/**
	 * @brief Platform specific resources enumeration
	 *
	 * The default implementation of this method loads the TPD, is such a
	 * function has been enabled
	 */
	ExitCode_t LoadPlatformData() noexcept override final;

	/**
	 * @brief Platform specific resources refresh
	 */
	ExitCode_t Refresh() noexcept override final;

	/**
	 * @brief Platform specific resources release interface.
	 */
	ExitCode_t Release(AppPtr_t papp) noexcept override final;

	/**
	 * @brief Platform specific resource claiming interface.
	 */
	ExitCode_t ReclaimResources(AppPtr_t papp) noexcept override final;

	/**
	 * @brief Platform specific resource binding interface.
	 */
	ExitCode_t MapResources(
	        AppPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl) noexcept override final;

private:
//-------------------- CONSTS
	/**
	 * @brief Default MAX number of CPUs per socket
	 */
	const int MaxCpusCount = BBQUE_MAX_R_ID_NUM+1;

	/**
	 * @brief Default MAX number of MEMs node per host
	 */
	const int MaxMemsCount = BBQUE_MAX_R_ID_NUM+1;

//-------------------- ATTRIBUTES
	/**
	 * @brief the control group controller
	 *
	 * This is a reference to the controller used on a generic Linux host.
	 * So far we use the "cpuset" controller.
	 */
	const char *controller;

	bool cfsQuotaSupported; /**< True if the target system supports CFS quota management */

	bool refreshMode;

	int cfs_margin_pct    = 0;  /**< CFS bandwidth enforcement safety margin (default: 0%) */
	int cfs_threshold_pct = 100;/**< CFS bandwidth enforcement threshold (default: 100%)   */

	std::unique_ptr<bu::Logger> logger;

	/**
	 * @brief The "silos" CGroup
	 *
	 * The "silos" is a control group where are placed processe which have
	 * been scheduled. This CGroup is indended to be a resource constrained
	 * group which grants a bare minimun of resources for the controlling
	 * application to run the RTLib
	 */
	CGroupDataPtr_t psilos;

#ifdef CONFIG_TARGET_ARM_BIG_LITTLE
	/**
	 * @brief ARM big.LITTLE support: type of each CPU core
	 *
	 * If true, indicates that the related CPU cores is an
	 * high-performance one.
	 */
	std::array<bool, BBQUE_TARGET_CPU_CORES_NUM> high_perf_cores = { {false} };

	void InitCoresType();
#endif

	std::string core_ids_all;

	std::string memory_ids_all;

//-------------------- METHODS

	LinuxPlatformProxy();

	/**
	 * @brief Load values from the configuration file
	 */
	void LoadConfiguration() noexcept;


	/**
	 * @brief Resources Mapping and Assigment to Applications
	 */
	ExitCode_t GetResourceMapping(
	        AppPtr_t papp,ResourceAssignmentMapPtr_t assign_map,
	        RLinuxBindingsPtr_t prlb,
	        BBQUE_RID_TYPE node_id,
	        br::RViewToken_t rvt) noexcept;

	ExitCode_t ParseNode(struct cgroup_file_info &entry) noexcept;
	ExitCode_t ParseNodeAttributes(struct cgroup_file_info &entry,
	                               RLinuxBindingsPtr_t prlb) noexcept;
	ExitCode_t RegisterCluster(RLinuxBindingsPtr_t prlb) noexcept;
	ExitCode_t RegisterClusterMEMs(RLinuxBindingsPtr_t prlb) noexcept;
	ExitCode_t RegisterClusterCPUs(RLinuxBindingsPtr_t prlb) noexcept;
	ExitCode_t GetSysMemoryTotal(uint64_t & mem_kb_tot) noexcept;

	// --- CGroup-releated methods
	ExitCode_t InitCGroups() noexcept;                          /**< Load the libcgroup and initialize the internal representation */
	ExitCode_t BuildSilosCG(CGroupDataPtr_t &pcgd) noexcept;    /**< Load the silos */
	ExitCode_t BuildCGroup(CGroupDataPtr_t &pcgd) noexcept;
	ExitCode_t GetCGroupData(ba::AppPtr_t papp, CGroupDataPtr_t &pcgd) noexcept;
	ExitCode_t SetupCGroup(CGroupDataPtr_t &pcgd, RLinuxBindingsPtr_t prlb,
	                       bool excl = false, bool move = true) noexcept;
	ExitCode_t BuildAppCG(AppPtr_t papp, CGroupDataPtr_t &pcgd) noexcept;
};

}   // namespace pp
}   // namespace bbque

#endif // LINUX_PLATFORM_PROXY_H
