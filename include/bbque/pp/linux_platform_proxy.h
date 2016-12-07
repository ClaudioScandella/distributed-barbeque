#ifndef BBQUE_LINUX_PLATFORM_PROXY_H
#define BBQUE_LINUX_PLATFORM_PROXY_H

#include "bbque/config.h"
#include "bbque/platform_proxy.h"
#include "bbque/utils/logging/logger.h"

#define LINUX_PP_NAMESPACE "bq.pp.linux"

// The _types.h file must be included after the
// constants define
#include "bbque/pp/linux_platform_proxy_types.h"

#include <bitset>

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


	bool IsHighPerformance(bbque::res::ResourcePathPtr_t const & path) const override;


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

#ifdef CONFIG_BBQUE_LINUX_CG_NET_BANDWIDTH

	NetworkInfo_t network_info;
	void InitNetworkManagement();

	ExitCode_t MakeQDisk(int if_index);
	ExitCode_t MakeCLS(int if_index);
	ExitCode_t SetCGNetworkBandwidth(AppPtr_t papp, CGroupDataPtr_t pcgd,
					ResourceAssignmentMapPtr_t pres,
					RLinuxBindingsPtr_t prlb);
	ExitCode_t MakeNetClass(AppPid_t handle, unsigned rate, int if_index);

	static ExitCode_t HTBParseOpt(struct nlmsghdr *n);
	static ExitCode_t HTBParseClassOpt(unsigned rate, struct nlmsghdr *n);
	static ExitCode_t CGParseOpt(long handle, struct nlmsghdr *n);
#endif

	uint64_t GetNetIFBandwidth(const std::string &ifname) const;

	std::string memory_ids_all;

//-------------------- METHODS

	LinuxPlatformProxy();


	void InitPowerInfo(const char * resourcePath, BBQUE_RID_TYPE core_id);

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


	ExitCode_t ScanPlatformDescription() noexcept;
	ExitCode_t RegisterCPU(const PlatformDescription::CPU &cpu) noexcept;
	ExitCode_t RegisterMEM(const PlatformDescription::Memory &mem) noexcept;
	ExitCode_t RegisterNET(const PlatformDescription::NetworkIF &net) noexcept;

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
