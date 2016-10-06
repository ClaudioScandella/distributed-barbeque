#include "bbque/config.h"

#include "bbque/power_monitor.h"
#include "bbque/pp/linux_platform_proxy.h"
#include "bbque/res/binder.h"

#include <boost/program_options.hpp>
#include <fstream>
#include <libcgroup.h>
#include <linux/version.h>

#define BBQUE_LINUXPP_PLATFORM_ID		"org.linux.cgroup"

#define BBQUE_LINUXPP_SILOS 			BBQUE_LINUXPP_CGROUP"/silos"

#define BBQUE_LINUXPP_CPUS_PARAM 		"cpuset.cpus"
#define BBQUE_LINUXPP_CPUP_PARAM 		"cpu.cfs_period_us"
#define BBQUE_LINUXPP_CPUQ_PARAM 		"cpu.cfs_quota_us"
#define BBQUE_LINUXPP_MEMN_PARAM 		"cpuset.mems"
#define BBQUE_LINUXPP_MEMB_PARAM 		"memory.limit_in_bytes"
#define BBQUE_LINUXPP_CPU_EXCLUSIVE_PARAM 	"cpuset.cpu_exclusive"
#define BBQUE_LINUXPP_MEM_EXCLUSIVE_PARAM 	"cpuset.mem_exclusive"
#define BBQUE_LINUXPP_PROCS_PARAM		"cgroup.procs"

#define BBQUE_LINUXPP_SYS_MEMINFO		"/proc/meminfo"

// The default CFS bandwidth period [us]
#define BBQUE_LINUXPP_CPUP_DEFAULT		100000
#define BBQUE_LINUXPP_CPUP_MAX			1000000

// Checking for kernel version requirements

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
#error Linux kernel >= 2.6.34 required by the Platform Integration Layer
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
#warning CPU Quota management disabled, Linux kernel >= 3.2 required
#endif

#define MODULE_CONFIG "LinuxPlatformProxy"

namespace bb = bbque;
namespace br = bbque::res;
namespace po = boost::program_options;


namespace bbque {
namespace pp {


LinuxPlatformProxy * LinuxPlatformProxy::GetInstance() {
	static LinuxPlatformProxy * instance;
	if (instance == nullptr)
		instance = new LinuxPlatformProxy();
	return instance;
}


LinuxPlatformProxy::LinuxPlatformProxy() :
	controller("cpuset"),
	cfsQuotaSupported(true),
	refreshMode(false) {

	//---------- Get a logger module
	logger = bu::Logger::GetLogger(LINUX_PP_NAMESPACE);
	assert(logger);

	//---------- Loading configuration
	this->LoadConfiguration();

	//---------- Init the CGroups
	if (this->InitCGroups() != PLATFORM_OK) {
		// We have to throw the exception: without cgroups the object should not
		// be created!
		throw new std::runtime_error("Unable to construct LinuxPlatformProxy, "
		                             "InitCGroups() failed.");
	}

#ifdef CONFIG_TARGET_ARM_BIG_LITTLE
	// Initialize the set of ARM "big" cores
	InitCoresType();
#endif

}

LinuxPlatformProxy::~LinuxPlatformProxy() {

}

const char* LinuxPlatformProxy::GetPlatformID(int16_t system_id) const noexcept {
	(void) system_id;
	return BBQUE_LINUXPP_PLATFORM_ID;
}

const char* LinuxPlatformProxy::GetHardwareID(int16_t system_id) const noexcept {
	(void) system_id;
	return BBQUE_TARGET_HARDWARE_ID;    // Defined in bbque.conf
}

LinuxPlatformProxy::ExitCode_t LinuxPlatformProxy::Setup(AppPtr_t papp) noexcept {
	ExitCode_t result = PLATFORM_OK;
	CGroupDataPtr_t pcgd;

	RLinuxBindingsPtr_t prlb = std::make_shared<RLinuxBindings_t>(
	        MaxCpusCount, MaxMemsCount);

	// Setup a new CGroup data for this application
	result = GetCGroupData(papp, pcgd);
	if (result != PLATFORM_OK) {
		logger->Error("PLAT LNX: [%s] CGroup initialization FAILED "
		"(Error: CGroupData setup)");
		return result;
	}

	// Setup the kernel CGroup with an empty resources assignement
	SetupCGroup(pcgd, prlb, false, false);

	// Reclaim application resource, thus moving this app into the silos
	result = this->ReclaimResources(papp);
	if (result != PLATFORM_OK) {
		logger->Error("PLAT LNX: [%s] CGroup initialization FAILED "
		"(Error: failed moving app into silos)");
		return result;
	}

	return result;
}


void LinuxPlatformProxy::LoadConfiguration() noexcept {
	po::options_description opts_desc("Linux Platform Proxy Options");
	opts_desc.add_options()
	(MODULE_CONFIG ".cfs_bandwidth.margin_pct",
	po::value<int>
	(&cfs_margin_pct)->default_value(0),
	"The safety margin [%] to add for CFS bandwidth enforcement");
	opts_desc.add_options()
	(MODULE_CONFIG ".cfs_bandwidth.threshold_pct",
	po::value<int>
	(&cfs_threshold_pct)->default_value(100),
	"The threshold [%] under which we enable CFS bandwidth enforcement");
	po::variables_map opts_vm;
	ConfigurationManager::GetInstance().
	ParseConfigurationFile(opts_desc, opts_vm);

	// Range check
	cfs_margin_pct = std::min(std::max(cfs_margin_pct, 0), 100);
	cfs_threshold_pct = std::min(std::max(cfs_threshold_pct, 0), 100);

	// Force threshold to be NOT lower than (100 - margin)
	if (cfs_threshold_pct < cfs_margin_pct)
		cfs_threshold_pct = 100 - cfs_margin_pct;

	logger->Info("CFS bandwidth control, margin %d, threshold: %d",
	cfs_margin_pct, cfs_threshold_pct);
}


LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::Release(AppPtr_t papp) noexcept {
	// Release CGroup plugin data
	// ... thus releasing the corresponding control group
	papp->ClearPluginData(LINUX_PP_NAMESPACE);
	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::ReclaimResources(AppPtr_t papp) noexcept {
	RLinuxBindingsPtr_t prlb(new RLinuxBindings_t(MaxCpusCount, MaxMemsCount));
	// FIXME: update once a better SetAttributes support is available
	//CGroupDataPtr_t pcgd(new CGroupData_t);
	CGroupDataPtr_t pcgd;
	int error;

	logger->Debug("PLAT LNX: CGroup resource claiming START");

	// Move this app into "silos" CGroup
	cgroup_set_value_uint64(psilos->pc_cpuset,
	BBQUE_LINUXPP_PROCS_PARAM,
	papp->Pid());

	// Configure the CGroup based on resource bindings
	logger->Notice("PLAT LNX: [%s] => SILOS[%s]",
	papp->StrId(), psilos->cgpath);
	error = cgroup_modify_cgroup(psilos->pcg);
	if (error) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, kernel cgroup update "
		"[%d: %s]", errno, strerror(errno));
		return PLATFORM_MAPPING_FAILED;
	}

	logger->Debug("PLAT LNX: CGroup resource claiming DONE!");

	return PLATFORM_OK;
}


LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::MapResources(AppPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl) noexcept {
	ResourceAccounter &ra = ResourceAccounter::GetInstance();
	RViewToken_t rvt = ra.GetScheduledView();

	// FIXME: update once a better SetAttributes support is available
	CGroupDataPtr_t pcgd;
	ExitCode_t result;

	logger->Debug("PLAT LNX: CGroup resource mapping START");

	// Get a reference to the CGroup data
	result = GetCGroupData(papp, pcgd);
	if (result != PLATFORM_OK)
		return result;

	// Get the set of assigned (bound) computing nodes (e.g., CPUs)
	br::ResourceBitset nodes(
	        br::ResourceBinder::GetMask(pres, br::ResourceType::CPU));
	BBQUE_RID_TYPE node_id = nodes.FirstSet();
	if (node_id < 0) {
		logger->Fatal("PLAT LNX: Missing binding to nodes/CPUs");
		return PLATFORM_MAPPING_FAILED;
	}

	// Map resources for each node (e.g., CPU)
	RLinuxBindingsPtr_t prlb = std::make_shared<RLinuxBindings_t>
	(MaxCpusCount, MaxMemsCount);
	for (; node_id <= nodes.LastSet(); ++node_id) {
		logger->Debug("PLAT LNX: CGroup resource mapping node [%d]", node_id);
		if (!nodes.Test(node_id)) continue;

		// Node resource mapping
		result = GetResourceMapping(papp, pres, prlb, node_id, rvt);
		if (result != PLATFORM_OK) {
			logger->Error("PLAT LNX: binding parsing FAILED");
			return PLATFORM_MAPPING_FAILED;
		}

		// Configure the CGroup based on resource bindings
		SetupCGroup(pcgd, prlb, excl, true);
	}
	logger->Debug("PLAT LNX: CGroup resource mapping DONE!");

#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	logger->Debug("PLAT LNX: Distributed actuation: retrieving masks and ranking");

	br::ResourceBitset proc_elements =
	        br::ResourceBinder::GetMask(
		pres,
		br::ResourceType::PROC_ELEMENT,
		br::ResourceType::CPU,
		R_ID_ANY,
		papp,
		rvt);

	br::ResourceBitset mem_nodes =
	        br::ResourceBinder::GetMask(
		pres,
		br::ResourceType::MEMORY,
		br::ResourceType::CPU,
		R_ID_ANY,
		papp,
		rvt);


	// Processing elements that have been allocated exclusively
	br::ResourceBitset proc_elements_exclusive = proc_elements;
	proc_elements_exclusive.Reset();

	for (BBQUE_RID_TYPE pe_id = proc_elements.FirstSet();
		 pe_id <= proc_elements.LastSet(); pe_id++) {

		// Skip if this processing element is not allocated to this app
		if (! proc_elements.Test(pe_id))
			continue;

		// Getting a reference to the resource status
		std::string path = "sys.cpu.pe" + std::to_string(pe_id);
		br::ResourcePtr_t resource = *(ra.GetResources(path).begin());

		// If allocation is exclusive, set the bit
		if(resource->ApplicationsCount(rvt) == 1)
			proc_elements_exclusive.Set(pe_id);

	}

	logger->Debug("[%d] pes %s (isolated %s), mems %s",
			papp->Pid(),
			proc_elements.ToString().c_str(),
			proc_elements_exclusive.ToString().c_str(),
			mem_nodes.ToString().c_str());

	papp->SetCGroupSetupData(proc_elements.ToULong(), mem_nodes.ToULong(), proc_elements_exclusive.ToULong());
#endif // CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION

	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::GetResourceMapping(
        AppPtr_t papp,
        ResourceAssignmentMapPtr_t assign_map,
        RLinuxBindingsPtr_t prlb,
        BBQUE_RID_TYPE node_id,
        br::RViewToken_t rvt) noexcept {
	ResourceAccounter & ra(ResourceAccounter::GetInstance());

	// CPU core set
	br::ResourceBitset core_ids(
	        br::ResourceBinder::GetMask(assign_map,
	br::ResourceType::PROC_ELEMENT,
	br::ResourceType::CPU, node_id, papp, rvt));
	if (strlen(prlb->cpus) > 0)
		strcat(prlb->cpus, ",");
	strncat(prlb->cpus, + core_ids.ToStringCG().c_str(), 3*MaxCpusCount);
	logger->Debug("PLAT LNX: Node [%d] cores: { %s }", node_id, prlb->cpus);

	// Memory nodes
	br::ResourceBitset mem_ids(
		br::ResourceBinder::GetMask(assign_map,
		br::ResourceType::PROC_ELEMENT,
		br::ResourceType::MEMORY, node_id, papp, rvt));
	if (mem_ids.Count() == 0)
		strncpy(prlb->mems, "0", 1);
	else
		strncpy(prlb->mems, mem_ids.ToStringCG().c_str(), 3*MaxMemsCount);
	logger->Debug("PLAT LNX: Node [%d] mems : { %s }", node_id, prlb->mems);

	// CPU quota
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
	prlb->amount_cpus += ra.GetAssignedAmount(
	        assign_map, papp, rvt,
	        br::ResourceType::PROC_ELEMENT, br::ResourceType::CPU, node_id);
#else
	prlb->amount_cpus = -1;
#endif
	logger->Debug("PLAT LNX: Node [%d] quota: { %ld }",
	node_id, prlb->amount_cpus);

	// Memory amount
	prlb->amount_memb = -1;
#ifdef CONFIG_BBQUE_LINUX_CG_MEMORY
	uint64_t memb = ra.GetAssignedAmount(
	        assign_map, papp, rvt, br::ResourceType::MEMORY, br::ResourceType::CPU);
	if (memb > 0)
		prlb->amount_memb = memb;
#endif
	logger->Debug("PLAT LNX: Node [%d] memb : { %ld }",
	node_id, prlb->amount_memb);

	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t LinuxPlatformProxy::Refresh() noexcept {
	logger->Notice("Refreshing CGroups resources description...");
	refreshMode = true;
	return LoadPlatformData();
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::LoadPlatformData() noexcept {
	struct cgroup *bbq_resources = NULL;
	struct cgroup_file_info entry;
	ExitCode_t pp_result = PLATFORM_OK;
	void *node_it = NULL;
	int cg_result;
	int level;

	logger->Info("PLAT LNX: CGROUP based resources enumeration...");

	// Lookup for a "bbque/res" cgroup
	bbq_resources = cgroup_new_cgroup(BBQUE_LINUXPP_RESOURCES);
	cg_result = cgroup_get_cgroup(bbq_resources);
	if (cg_result) {
		logger->Error("PLAT LNX: [" BBQUE_LINUXPP_RESOURCES "] lookup FAILED! "
		"(Error: No resources assignment)");
		return PLATFORM_ENUMERATION_FAILED;
	}

	// Scan  subfolders to map "clusters"
	cg_result = cgroup_walk_tree_begin("cpuset", BBQUE_LINUXPP_RESOURCES,
	1, &node_it, &entry, &level);
	if ((cg_result != 0) || (node_it == NULL)) {
		logger->Error("PLAT LNX: [" BBQUE_LINUXPP_RESOURCES "] lookup FAILED! "
		"(Error: No resources assignment)");
		return PLATFORM_ENUMERATION_FAILED;
	}

	// Scan all "nodeN" assignment
	while (!cg_result && (pp_result == PLATFORM_OK)) {
		// That's fine here, since we want also to skip the root group [bbq_resources]
		cg_result = cgroup_walk_tree_next(1, &node_it, &entry, level);
		pp_result = ParseNode(entry);
	}

	// Release the iterator
	cgroup_walk_tree_end(&node_it);

	return pp_result;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::ParseNode(struct cgroup_file_info &entry) noexcept {
	RLinuxBindingsPtr_t prlb(new RLinuxBindings_t(0,0));
	ExitCode_t pp_result = PLATFORM_OK;

	// Jump all entries deeper than first-level subdirs
	if (entry.depth > 1)
		return PLATFORM_OK;

	// Skip parsing of all NON directory, if not required to parse an attribute
	if (entry.type != CGROUP_FILE_TYPE_DIR)
		return PLATFORM_OK;

	logger->Info("PLAT LNX: scanning [%d:%s]...",
	entry.depth, entry.full_path);

	// Consistency check for required folder names
	if (strncmp(BBQUE_LINUXPP_CLUSTER, entry.path,
	STRLEN(BBQUE_LINUXPP_CLUSTER))) {
		logger->Warn("PLAT LNX: Resources enumeration, "
		"ignoring unexpected CGroup [%s]",
		entry.full_path);
		return PLATFORM_OK;
	}

	pp_result = ParseNodeAttributes(entry, prlb);
	if (pp_result != PLATFORM_OK)
		return pp_result;

	// Scan "cpus" and "mems" attributes for each cluster
	logger->Debug("PLAT LNX: Setup resources from [%s]...",
	entry.full_path);

	// Register CPUs for this Node
	pp_result = RegisterCluster(prlb);
	return pp_result;

}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::RegisterCluster(RLinuxBindingsPtr_t prlb) noexcept {
	ExitCode_t pp_result = PLATFORM_OK;

	logger->Debug("PLAT LNX: %s resources for Node [%d], "
	"CPUs [%s], MEMs [%s]",
	refreshMode ? "Check" : "Setup",
	prlb->node_id, prlb->cpus, prlb->mems);

	// The CPUs are generally represented with a syntax like this:
	// 1-3,4,5-7
	pp_result = RegisterClusterCPUs(prlb);
	if (pp_result != PLATFORM_OK)
		return pp_result;

	// The MEMORY amount is represented in Bytes
	pp_result = RegisterClusterMEMs(prlb);
	if (pp_result != PLATFORM_OK)
		return pp_result;

	return pp_result;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::RegisterClusterMEMs(RLinuxBindingsPtr_t prlb) noexcept {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	char resourcePath[] = "sys0.cpu256.mem256";
	unsigned short first_mem_id;
	unsigned short last_mem_id;
	const char *p = prlb->mems;
	uint64_t limit_in_bytes;

#ifdef CONFIG_BBQUE_LINUX_CG_MEMORY
	limit_in_bytes = atol(prlb->memb);
#else
	GetSysMemoryTotal(limit_in_bytes);
	limit_in_bytes *= 1024;
#endif

	// NOTE: The Memory limit in bytes is used to assign the SAME quota to
	// each memory node within the same cluster. This is not the intended
	// behavior of the limit_in_bytes, but simplifies a lot the
	// configuration and should be just enough for our purposes.

	while (*p) {

		// Get a Memory NODE id, and register the corresponding resource path
		sscanf(p, "%hu", &first_mem_id);
		snprintf(resourcePath+8, 11, "%hu.mem%d",
		prlb->node_id, first_mem_id);
		logger->Debug("PLAT LNX: %s [%s]...",
		refreshMode ? "Refreshing" : "Registering",
		resourcePath);
		if (refreshMode)
			ra.UpdateResource(resourcePath, "", limit_in_bytes);
		else
			ra.RegisterResource(resourcePath, "", limit_in_bytes);

		// Look-up for next NODE id
		while (*p && (*p != ',') && (*p != '-')) {
			++p;
		}

		if (!*p)
			return PLATFORM_OK;

		if (*p == ',') {
			++p;
			continue;
		}
		// Otherwise: we have stopped on a "-"

		// Get last Memory NODE id of this range
		sscanf(++p, "%hu", &last_mem_id);
		// Register all the other Memory NODEs of this range
		while (++first_mem_id <= last_mem_id) {
			snprintf(resourcePath+8, 11, "%hu.mem%d",
			         prlb->node_id, first_mem_id);
			logger->Debug("PLAT LNX: %s [%s]...",
			              refreshMode ? "Refreshing" : "Registering",
			              resourcePath);

			if (refreshMode)
				ra.UpdateResource(resourcePath, "", limit_in_bytes);
			else
				ra.RegisterResource(resourcePath, "", limit_in_bytes);
		}

		// Look-up for next CPU id
		while (*p && (*p != ',')) {
			++p;
		}

		if (*p == ',')
			++p;
	}

	return PLATFORM_OK;
}


LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::RegisterClusterCPUs(RLinuxBindingsPtr_t prlb) noexcept {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	char resourcePath[] = "sys0.cpu256.pe256";
	unsigned short first_cpu_id;
	unsigned short last_cpu_id;
	const char *p = prlb->cpus;
	uint32_t cpu_quota = 100;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)

	// NOTE: The CPU bandwidth is used to assign the SAME quota to each
	// processor within the same node/cluster. This is not the intended
	// behavior of the cfs_quota_us, but simplifies a lot the
	// configuration and should be just enough for our purposes.
	// Thus, each CPU will receive a % of CPU time defined by:
	//   QUOTA = CPU_QUOTA * 100 / CPU_PERIOD
	if (prlb->amount_cpup) {
		cpu_quota = (prlb->amount_cpuq * 100) / prlb->amount_cpup;
		logger->Debug("Configuring CPUs of node [%d] with CPU quota of [%lu]%",
		prlb->node_id, cpu_quota);
	}

	// Because of CGroups interface, we cannot assign an empty CPU quota.
	// The minimum allowed value is 1%, since this value is also quite
	// un-useful on a real configuration, we assume a CPU being offline
	// when a CPU quota <= 1% is required.
	if (cpu_quota <= 1) {
		logger->Warn("Quota < 1%, Offlining CPUs of node [%d]...",
		prlb->node_id);
		cpu_quota = 0;
	}


#endif

	while (*p) {

		// Get a CPU id, and register the corresponding resource path
		sscanf(p, "%hu", &first_cpu_id);
		snprintf(resourcePath+8, 10, "%hu.pe%d",
		prlb->node_id, first_cpu_id);
		logger->Debug("PLAT LNX: Registering [%s]...",
		resourcePath);
		if (refreshMode)
			ra.UpdateResource(resourcePath, "", cpu_quota);
		else {
			ra.RegisterResource(resourcePath, "", cpu_quota);
#ifdef CONFIG_TARGET_ARM_BIG_LITTLE
			br::ResourcePtr_t rsrc(ra.GetResource(resourcePath));
			if (highPerfCores[first_cpu_id])
				rsrc->SetModel("ARM Cortex A15");
			else
				rsrc->SetModel("ARM Cortex A7");
			logger->Info("PLAT LNX: [%s] CPU model = %s",
			rsrc->Path().c_str(), rsrc->Model().c_str());
#endif
#ifdef CONFIG_BBQUE_WM
			PowerMonitor & wm(PowerMonitor::GetInstance());
			wm.Register(resourcePath);
#endif
		}

		// Look-up for next CPU id
		while (*p && (*p != ',') && (*p != '-')) {
			++p;
		}

		if (!*p)
			return PLATFORM_OK;

		if (*p == ',') {
			++p;
			continue;
		}
		// Otherwise: we have stopped on a "-"

		// Get last CPU of this range
		sscanf(++p, "%hu", &last_cpu_id);
		// Register all the other CPUs of this range
		while (++first_cpu_id <= last_cpu_id) {
			snprintf(resourcePath+8, 10, "%hu.pe%d",
			         prlb->node_id, first_cpu_id);
			logger->Debug("PLAT LNX: %s [%s]...",
			              refreshMode ? "Refreshing" : "Registering",
			              resourcePath);

			if (refreshMode)
				ra.UpdateResource(resourcePath, "", cpu_quota);
			else {
				ra.RegisterResource(resourcePath, "", cpu_quota);
#ifdef CONFIG_BBQUE_WM
				PowerMonitor & wm(PowerMonitor::GetInstance());
				wm.Register(resourcePath);
#endif
			}
		}

		// Look-up for next CPU id
		while (*p && (*p != ',')) {
			++p;
		}

		if (*p == ',')
			++p;
	}

	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::GetSysMemoryTotal(uint64_t & mem_kb_tot) noexcept {
	std::ifstream meminfo_fd;
	std::string line;
	char s[10], k[2];

	meminfo_fd.open(BBQUE_LINUXPP_SYS_MEMINFO);
	if (!meminfo_fd.is_open()) {
		logger->Error("PLAT LNX: Cannot read total amount of system memory");
		return PLATFORM_DATA_NOT_FOUND;
	}

	while (!meminfo_fd.eof()) {
		getline(meminfo_fd, line);
		if (line.compare("MemTotal")) {
			sscanf(line.data(), "%s %" PRIu64 " %s", s, &mem_kb_tot, k);
			break;
		}
	}
	meminfo_fd.close();

	return PLATFORM_OK;
}


//---------------------------------------------------------------------------//
//----------------------- GROUPS-related method -----------------------------//
//---------------------------------------------------------------------------//

LinuxPlatformProxy::ExitCode_t LinuxPlatformProxy::InitCGroups() noexcept {
	ExitCode_t pp_result;
	char *mount_path = NULL;
	int cg_result;

	// Init the Control Group Library
	cg_result = cgroup_init();
	if (cg_result) {
		logger->Error("PLAT LNX: CGroup Library initializaton FAILED! "
		"(Error: %d - %s)", cg_result, cgroup_strerror(cg_result));
		return PLATFORM_INIT_FAILED;
	}

	cg_result = cgroup_get_subsys_mount_point(controller, &mount_path);
	if (cg_result) {
		logger->Error("PLAT LNX: CGroup Library mountpoint lookup FAILED! "
		"(Error: %d - %s)", cg_result, cgroup_strerror(cg_result));
		return PLATFORM_GENERIC_ERROR;
	}
	logger->Info("PLAT LNX: controller [%s] mounted at [%s]",
	controller, mount_path);


	// TODO: check that the "bbq" cgroup already existis
	// TODO: check that the "bbq" cgroup has CPUS and MEMS
	// TODO: keep track of overall associated resources => this should be done
	// by exploting the LoadPlatformData low-level method
	// TODO: update the MaxCpusCount and MaxMemsCount

	// Build "silos" CGroup to host blocked applications
	pp_result = BuildSilosCG(psilos);
	if (pp_result) {
		logger->Error("PLAT LNX: Silos CGroup setup FAILED!");
		return PLATFORM_GENERIC_ERROR;
	}

	free(mount_path);

	return PLATFORM_OK;


}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::BuildSilosCG(CGroupDataPtr_t &pcgd) noexcept {
	RLinuxBindingsPtr_t prlb(new RLinuxBindings_t(
	        MaxCpusCount, MaxMemsCount));
	ExitCode_t result;
	int error;

	logger->Debug("PLAT LNX: Building SILOS CGroup...");

	// Build new CGroup data
	pcgd = CGroupDataPtr_t(new CGroupData_t(BBQUE_LINUXPP_SILOS));
	result = BuildCGroup(pcgd);
	if (result != PLATFORM_OK)
		return result;

	// Setting up silos (limited) resources, just to run the RTLib
	sprintf(prlb->cpus, "0");
	sprintf(prlb->mems, "0");

	// Configuring silos constraints
	cgroup_set_value_string(pcgd->pc_cpuset,
	BBQUE_LINUXPP_CPUS_PARAM, prlb->cpus);
	cgroup_set_value_string(pcgd->pc_cpuset,
	BBQUE_LINUXPP_MEMN_PARAM, prlb->mems);

	// Updating silos constraints
	logger->Notice("PLAT LNX: Updating kernel CGroup [%s]", pcgd->cgpath);
	error = cgroup_modify_cgroup(pcgd->pcg);
	if (error) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, kernel cgroup update "
		"[%d: %s]", errno, strerror(errno));
		return PLATFORM_MAPPING_FAILED;
	}

	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::BuildCGroup(CGroupDataPtr_t &pcgd) noexcept {
	int result;

	logger->Debug("PLAT LNX: Building CGroup [%s]...", pcgd->cgpath);

	// Setup CGroup path for this application
	pcgd->pcg = cgroup_new_cgroup(pcgd->cgpath);
	if (!pcgd->pcg) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, \"cgroup\" creation)");
		return PLATFORM_MAPPING_FAILED;
	}

	// Add "cpuset" controller
	pcgd->pc_cpuset = cgroup_add_controller(pcgd->pcg, "cpuset");
	if (!pcgd->pc_cpuset) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, [cpuset] \"controller\" "
		"creation failed)");
		return PLATFORM_MAPPING_FAILED;
	}

#ifdef CONFIG_BBQUE_LINUX_CG_MEMORY
	// Add "memory" controller
	pcgd->pc_memory = cgroup_add_controller(pcgd->pcg, "memory");
	if (!pcgd->pc_memory) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, [memory] \"controller\" "
		"creation failed)");
		return PLATFORM_MAPPING_FAILED;
	}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)

	// Add "cpu" controller
	pcgd->pc_cpu = cgroup_add_controller(pcgd->pcg, "cpu");
	if (!pcgd->pc_cpu) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, [cpu] \"controller\" "
		"creation failed)");
		return PLATFORM_MAPPING_FAILED;
	}

#endif

	// Create the kernel-space CGroup
	// NOTE: the current libcg API is quite confuse and unclear
	// regarding the "ignore_ownership" second parameter
	logger->Info("PLAT LNX: Create kernel CGroup [%s]", pcgd->cgpath);
	result = cgroup_create_cgroup(pcgd->pcg, 0);
	if (result && errno) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, kernel cgroup creation "
		"[%d: %s]", errno, strerror(errno));
		return PLATFORM_MAPPING_FAILED;
	}

	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::GetCGroupData(AppPtr_t papp, CGroupDataPtr_t &pcgd) noexcept {
	ExitCode_t result;
#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	logger->Warn("Distributed cgroup actuation: cgroup will be written by "
				 "the EXC itself.");
	return PLATFORM_OK;
#endif
	// Loop-up for application control group data
	pcgd = std::static_pointer_cast<CGroupData_t>(
	        papp->GetPluginData(LINUX_PP_NAMESPACE, "cgroup")
	);
	if (pcgd)
		return PLATFORM_OK;

	// A new CGroupData must be setup for this app
	result = BuildAppCG(papp, pcgd);
	if (result != PLATFORM_OK)
		return result;

	// Keep track of this control group
	// FIXME check return value otherwise multiple BuildCGroup could be
	// called for the same application
	papp->SetPluginData(pcgd);

	return PLATFORM_OK;

}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::SetupCGroup(CGroupDataPtr_t & pcgd,
								RLinuxBindingsPtr_t prlb,
								bool excl, bool move) noexcept
{
#ifdef CONFIG_BBQUE_CGROUPS_DISTRIBUTED_ACTUATION
	logger->Warn("Distributed cgroup actuation: cgroup will be setup by "
				 "the EXC itself.");
	return PLATFORM_OK;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
	int64_t cpus_quota = -1; // NOTE: use "-1" for no quota assignement
#endif
	int result;

	/**********************************************************************
	 *    CPUSET Controller
	 **********************************************************************/

#if 0
	// Setting CPUs as EXCLUSIVE if required
	if (excl) {
		cgroup_set_value_string(pcgd->pc_cpuset,
		BBQUE_LINUXPP_CPU_EXCLUSIVE_PARAM, "1");
	}
#else
	excl = false;
#endif

	// Set the assigned CPUs
	cgroup_set_value_string(pcgd->pc_cpuset,
	BBQUE_LINUXPP_CPUS_PARAM,
	prlb->cpus ? prlb->cpus : "");

	// Set the assigned memory NODE (only if we have at least one CPUS)
	if (prlb->cpus[0]) {
		cgroup_set_value_string(pcgd->pc_cpuset,
		BBQUE_LINUXPP_MEMN_PARAM, prlb->mems);

		logger->Debug("PLAT LNX: Setup CPUSET for [%s]: "
		"{cpus [%c: %s], mems[%s]}",
		pcgd->papp->StrId(),
		excl ? 'E' : 'S',
		prlb->cpus ,
		prlb->mems);
	} else {

		logger->Debug("PLAT LNX: Setup CPUSET for [%s]: "
		"{cpus [NONE], mems[NONE]}",
		pcgd->papp->StrId());
	}


	/**********************************************************************
	 *    MEMORY Controller
	 **********************************************************************/

#ifdef CONFIG_BBQUE_LINUX_CG_MEMORY
	char quota[] = "9223372036854775807";
	// Set the assigned MEMORY amount
	sprintf(quota, "%lu", prlb->amount_memb);
	cgroup_set_value_string(pcgd->pc_memory,
	BBQUE_LINUXPP_MEMB_PARAM, quota);

	logger->Debug("PLAT LNX: Setup MEMORY for [%s]: "
	"{bytes_limit [%lu]}",
	pcgd->papp->StrId(), prlb->amount_memb);
#endif

	/**********************************************************************
	 *    CPU Quota Controller
	 **********************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)

	app::RuntimeProfiling_t runtime_profile =
			pcgd->papp->GetRuntimeProfile();

	uint32_t cfs_quota_us = std::min(
			1000 * runtime_profile.ctime_ms, BBQUE_LINUXPP_CPUP_MAX);
	if (cfs_quota_us == 0)
		cfs_quota_us = BBQUE_LINUXPP_CPUP_DEFAULT;

	char const *cfs_c = std::to_string(cfs_quota_us).c_str();

	if (unlikely(!cfsQuotaSupported))
		goto jump_quota_management;

	// Set the default CPU bandwidth period
	cgroup_set_value_string(pcgd->pc_cpu,
			BBQUE_LINUXPP_CPUP_PARAM,
			cfs_c);

	// Set the assigned CPU bandwidth amount
	// NOTE: if a quota is NOT assigned we have amount_cpus="0", but this
	// is not acceptable by the CFS controller, which requires a negative
	// number to remove any constraint.
	assert(cpus_quota == -1);
	if (!prlb->amount_cpus)
		goto quota_enforcing_disabled;

	// CFS quota to enforced is
	// assigned + (margin * #PEs)
	cpus_quota = prlb->amount_cpus;
	cpus_quota += ((cpus_quota / 100) + 1) * cfs_margin_pct;
	if ((cpus_quota % 100) > cfs_threshold_pct) {
		logger->Warn("CFS (quota+margin) %d > %d threshold, enforcing disabled",
		cpus_quota, cfs_threshold_pct);
		goto quota_enforcing_disabled;
	}

	cpus_quota = (cfs_quota_us / 100) *
			prlb->amount_cpus;
	cgroup_set_value_int64(pcgd->pc_cpu,
	BBQUE_LINUXPP_CPUQ_PARAM, cpus_quota);

	logger->Debug("PLAT LNX: Setup CPU for [%s]: "
			"{period [%s], quota [%lu]",
			pcgd->papp->StrId(),
			cfs_c,
			cpus_quota);

quota_enforcing_disabled:

	logger->Debug("PLAT LNX: Setup CPU for [%s]: "
			"{period [%s], quota [-]}",
			pcgd->papp->StrId(),
			cfs_c);

jump_quota_management:
	// Here we jump, if CFS Quota management is not enabled on the target

#endif

	/**********************************************************************
	 *    CGroup Configuraiton
	 **********************************************************************/

	logger->Debug("PLAT LNX: Updating kernel CGroup [%s]", pcgd->cgpath);
	result = cgroup_modify_cgroup(pcgd->pcg);
	if (result) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, kernel cgroup update "
		"[%d: %s])", errno, strerror(errno));
		return PLATFORM_MAPPING_FAILED;
	}

	/* If a task has not beed assigned, we are done */
	if (!move)
		return PLATFORM_OK;

	/**********************************************************************
	 *    CGroup Task Assignement
	 **********************************************************************/
	// NOTE: task assignement must be done AFTER CGroup configuration, to
	// ensure all the controller have been properly setup to manage the
	// task. Otherwise a task could be killed if being assigned to a
	// CGroup not yet configure.

	logger->Notice("PLAT LNX: [%s] => "
	"{cpus [%s: %ld], mems[%s: %ld B]}",
	pcgd->papp->StrId(),
	prlb->cpus, prlb->amount_cpus,
	prlb->mems, prlb->amount_memb);
	cgroup_set_value_uint64(pcgd->pc_cpuset,
	BBQUE_LINUXPP_PROCS_PARAM,
	pcgd->papp->Pid());

	logger->Debug("PLAT LNX: Updating kernel CGroup [%s]", pcgd->cgpath);
	result = cgroup_modify_cgroup(pcgd->pcg);
	if (result) {
		logger->Error("PLAT LNX: CGroup resource mapping FAILED "
		"(Error: libcgroup, kernel cgroup update "
		"[%d: %s])", errno, strerror(errno));
		return PLATFORM_MAPPING_FAILED;
	}

	return PLATFORM_OK;
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::BuildAppCG(AppPtr_t papp, CGroupDataPtr_t &pcgd) noexcept {
	// Build new CGroup data for the specified application
	pcgd = CGroupDataPtr_t(new CGroupData_t(papp));
	return BuildCGroup(pcgd);
}

LinuxPlatformProxy::ExitCode_t
LinuxPlatformProxy::ParseNodeAttributes(struct cgroup_file_info &entry,
                                        RLinuxBindingsPtr_t prlb) noexcept {
	char group_name[] = BBQUE_LINUXPP_RESOURCES "/" BBQUE_LINUXPP_CLUSTER "123";
	struct cgroup_controller *cg_controller = NULL;
	struct cgroup *bbq_node = NULL;
	ExitCode_t pp_result = PLATFORM_OK;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
	char *buff = NULL;
#endif
	int cg_result;

	// Read "cpuset" attributes from kernel
	logger->Debug("PLAT LNX: Loading kernel info for [%s]...", entry.path);


	// Initialize the CGroup variable
	sscanf(entry.path + STRLEN(BBQUE_LINUXPP_CLUSTER), "%hu",
	&prlb->node_id);
	snprintf(group_name +
	STRLEN(BBQUE_LINUXPP_RESOURCES) +   // e.g. "bbque/res"
	STRLEN(BBQUE_LINUXPP_CLUSTER) + 1,  // e.g. "/" + "node"
	4, "%d",
	prlb->node_id);
	bbq_node = cgroup_new_cgroup(group_name);
	if (bbq_node == NULL) {
		logger->Error("PLAT LNX: Parsing resources FAILED! "
		"(Error: cannot create [%s] group)", entry.path);
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	// Update the CGroup variable with kernel info
	cg_result = cgroup_get_cgroup(bbq_node);
	if (cg_result != 0) {
		logger->Error("PLAT LNX: Reading kernel info FAILED! "
		"(Error: %d, %s)", cg_result, cgroup_strerror(cg_result));
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	/**********************************************************************
	 *    CPUSET Controller
	 **********************************************************************/

	// Get "cpuset" controller info
	cg_controller = cgroup_get_controller(bbq_node, "cpuset");
	if (cg_controller == NULL) {
		logger->Error("PLAT LNX: Getting controller FAILED! "
		"(Error: Cannot find controller \"cpuset\" "
		"in group [%s])", entry.path);
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	// Getting the value for the "cpuset.cpus" attribute
	cg_result = cgroup_get_value_string(cg_controller, BBQUE_LINUXPP_CPUS_PARAM,
	&(prlb->cpus));
	if (cg_result) {
		logger->Error("PLAT LNX: Getting CPUs attribute FAILED! "
		"(Error: 'cpuset.cpus' not configured or not readable)");
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	// Getting the value for the "cpuset.mems" attribute
	cg_result = cgroup_get_value_string(cg_controller, BBQUE_LINUXPP_MEMN_PARAM,
	&(prlb->mems));
	if (cg_result) {
		logger->Error("PLAT LNX: Getting MEMs attribute FAILED! "
		"(Error: 'cpuset.mems' not configured or not readable)");
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	/**********************************************************************
	 *    MEMORY Controller
	 **********************************************************************/

#ifdef CONFIG_BBQUE_LINUX_CG_MEMORY
	// Get "memory" controller info
	cg_controller = cgroup_get_controller(bbq_node, "memory");
	if (cg_controller == NULL) {
		logger->Error("PLAT LNX: Getting controller FAILED! "
		"(Error: Cannot find controller \"memory\" "
		"in group [%s])", entry.path);
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	// Getting the value for the "memory.limit_in_bytes" attribute
	cg_result = cgroup_get_value_string(cg_controller, BBQUE_LINUXPP_MEMB_PARAM,
	&(prlb->memb));
	if (cg_result) {
		logger->Error("PLAT LNX: Getting MEMORY attribute FAILED! "
		"(Error: 'memory.limit_in_bytes' not configured "
		"or not readable)");
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}
#endif

	/**********************************************************************
	 *    CPU Quota Controller
	 **********************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)

	if (unlikely(!cfsQuotaSupported))
		goto jump_quota_parsing;

	// Get "cpu" controller info
	cg_controller = cgroup_get_controller(bbq_node, "cpu");
	if (cg_controller == NULL) {
		logger->Error("PLAT LNX: Getting controller FAILED! "
		"(Error: Cannot find controller \"cpu\" "
		"in group [%s])", entry.path);
		pp_result = PLATFORM_NODE_PARSING_FAILED;
		goto parsing_failed;
	}

	// Getting the value for the "cpu.cfs_quota_us" attribute
	cg_result = cgroup_get_value_string(cg_controller,
	BBQUE_LINUXPP_CPUQ_PARAM, &buff);
	if (cg_result) {
		logger->Error("PLAT LNX: Getting CPU attributes FAILED! "
		"(Error: 'cpu.cfs_quota_us' not configured "
		"or not readable)");
		logger->Warn("PLAT LNX: Disabling CPU Quota management");

		// Disable CFS quota management
		cfsQuotaSupported = false;

		goto jump_quota_parsing;
	}

	// Check if a quota has been assigned (otherwise a "-1" is expected)
	if (buff[0] != '-') {

		// Save the "quota" value
		errno = 0;
		prlb->amount_cpuq = strtoul(buff, NULL, 10);
		if (errno != 0) {
			logger->Error("PLAT LNX: Getting CPU attributes FAILED! "
			"(Error: 'cpu.cfs_quota_us' convertion)");
			pp_result = PLATFORM_NODE_PARSING_FAILED;
			goto parsing_failed;
		}

		// Getting the value for the "cpu.cfs_period_us" attribute
		cg_result = cgroup_get_value_string(cg_controller,
		                                    BBQUE_LINUXPP_CPUP_PARAM,
		                                    &buff);
		if (cg_result) {
			logger->Error("PLAT LNX: Getting CPU attributes FAILED! "
			              "(Error: 'cpu.cfs_period_us' not configured "
			              "or not readable)");
			pp_result = PLATFORM_NODE_PARSING_FAILED;
			goto parsing_failed;
		}

		// Save the "period" value
		errno = 0;
		prlb->amount_cpup = strtoul(buff, NULL, 10);
		if (errno != 0) {
			logger->Error("PLAT LNX: Getting CPU attributes FAILED! "
			              "(Error: 'cpu.cfs_period_us' convertion)");
			pp_result = PLATFORM_NODE_PARSING_FAILED;
			goto parsing_failed;
		}

	}

jump_quota_parsing:
	// Here we jump, if CFS Quota management is not enabled on the target

#endif

parsing_failed:
	cgroup_free (&bbq_node);
	return pp_result;
}



}   // namespace pp
}   // namespace bbque
