#ifndef LINUX_PLATFORM_PROXY_TYPES_H
#define LINUX_PLATFORM_PROXY_TYPES_H

#ifndef LINUX_PP_NAMESPACE
#error Please do not include directly this file, but you have to\
include linux_platform_proxy.h!
#endif

#include "bbque/utils/extra_data_container.h"
#include "bbque/app/application.h"

#ifdef CONFIG_BBQUE_LINUX_CG_NET_BANDWIDTH
#include <netlink/libnetlink.h>
#endif

#include <cstdint>
#include <memory>
#include <libcgroup.h>

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

namespace bbque {
namespace pp {

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
	uint_fast16_t amount_cpus = 0;
	/** The bytes amount of Socket MEMORY assigned */
	int_fast64_t amount_memb = 0;
	/** The bytes/s amount of network bandwidth assigned */
	int_fast64_t amount_net_bw = 0;

	RLinuxBindings(const uint_fast8_t MaxCpusCount, const uint_fast8_t MaxMemsCount) {
		// 3 chars are required for each CPU/MEM resource if formatted
		// with syntax: "nn,". This allows for up-to 99 resources per
		// cluster
		if (MaxCpusCount) {
			cpus = new char[3*MaxCpusCount]();
			cpus[0] = 0;
		}
		if (MaxMemsCount) {
			mems = new char[3*MaxMemsCount]();
			mems[0] = 0; 
		}
	}
	~RLinuxBindings() {
		delete [] cpus;
		delete [] mems;
		if (memb != NULL)
			delete memb;
	}
} RLinuxBindings_t;

typedef std::shared_ptr<RLinuxBindings_t> RLinuxBindingsPtr_t;

typedef struct CGroupData : public bbque::utils::PluginData_t {
	bbque::app::AppPtr_t papp; /** The controlled application */
#define BBQUE_LINUXPP_CGROUP_PATH_MAX 128 // "user.slice/res/12345:ABCDEF:00";
	char cgpath[BBQUE_LINUXPP_CGROUP_PATH_MAX];
	struct cgroup *pcg;
	struct cgroup_controller *pc_cpu;
	struct cgroup_controller *pc_cpuset;
	struct cgroup_controller *pc_memory;
	struct cgroup_controller *pc_net_cls;

	bool cfs_quota_available = false; /**< True if the target system supports 
										   CFS quota management */

	CGroupData(bbque::app::AppPtr_t pa) :
		bu::PluginData_t(LINUX_PP_NAMESPACE, "cgroup"),
		papp(pa), pcg(NULL), pc_cpu(NULL),
		pc_cpuset(NULL), pc_memory(NULL) {
		snprintf(cgpath, BBQUE_LINUXPP_CGROUP_PATH_MAX,
		         BBQUE_LINUXPP_RESOURCES"/%s",
		         papp->StrId());
	}

	CGroupData(const char *cgp) :
		bu::PluginData_t(LINUX_PP_NAMESPACE, "cgroup"),
		pcg(NULL), pc_cpu(NULL),
		pc_cpuset(NULL), pc_memory(NULL) {
		snprintf(cgpath, BBQUE_LINUXPP_CGROUP_PATH_MAX,
		         "%s", cgp);
	}

	~CGroupData() {
		if (pcg != NULL) {
			// Removing Kernel Control Group
			cgroup_delete_cgroup(pcg, 1);
			// Releasing libcgroup resources
			cgroup_free(&pcg);
		}
	}

} CGroupData_t;

typedef std::shared_ptr<CGroupData_t> CGroupDataPtr_t;


#ifdef CONFIG_BBQUE_LINUX_CG_NET_BANDWIDTH
// TODO check this size (it seems too high)
#define MAX_MSG 16384	// 2^14
/**
 * @brief The netlink communication structure
 *	  It contains the file descriptors used in communication with the kernel
 *	  and the socket address
 */
typedef struct NetworkInfo {
	struct rtnl_handle rth_1;
	struct rtnl_handle rth_2;
	struct sockaddr_nl kernel_addr;
} NetworkInfo_t;

typedef struct Requests{
	struct nlmsghdr 	n;
	struct tcmsg 		t;
	char   			buf[MAX_MSG];
} NetworkKernelRequest_t;

#endif

}   // namespace pp
}   // namespace bbque

#endif // LINUX_PLATFORM_PROXY_TYPES_H
