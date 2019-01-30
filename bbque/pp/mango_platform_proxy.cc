#include <utility>

#include "bbque/pp/mango_platform_proxy.h"
#include "bbque/config.h"
#include "bbque/res/resource_path.h"
#include "bbque/utils/assert.h"
#include "bbque/pp/mango_platform_description.h"
#include "bbque/resource_partition_validator.h"

#ifdef CONFIG_BBQUE_WM
#include "bbque/power_monitor.h"
#endif

#include <libhn/hn.h>

#define BBQUE_MANGOPP_PLATFORM_ID    "org.mango"
#define MANGO_PEAKOS_FILE_SIZE       256*1024*1024

namespace bb = bbque;
namespace br = bbque::res;
namespace po = boost::program_options;

namespace bbque {
namespace pp {

std::unique_ptr<bu::Logger> logger;

MangoPlatformProxy * MangoPlatformProxy::GetInstance() {
	static MangoPlatformProxy * instance;
	if (instance == nullptr)
		instance = new MangoPlatformProxy();
	return instance;
}

MangoPlatformProxy::MangoPlatformProxy() :
	refreshMode(false)
{
	//---------- Get a logger module
	logger = bu::Logger::GetLogger(MANGO_PP_NAMESPACE);
	bbque_assert(logger);

	// In order to initialize the communication with the HN library we have to
	// prepare the filter to enable access to registers and statistics
	// TODO: this was a copy-and-paste of the example, we have to ask to UPV
	//       how to properly tune these parameters, in particular the meaning of
	//	 UPV_PARTITION_STRATEGY
	hn_filter_t filter;
	filter.target = HN_FILTER_TARGET_MANGO;
	filter.mode = HN_FILTER_APPL_MODE_SYNC_READS;
	filter.tile = 999;
	filter.core = 999;

	logger->Info("MangoPlatformProxy: initializing communication with HN daemon...");
	int hn_init_err = hn_initialize(filter, UPV_PARTITION_STRATEGY, 1, 0, 0);
	if (hn_init_err == HN_SUCCEEDED) {
		logger->Info("MangoPlatformProxy: HN daemon connection established");
	} else {
		logger->Fatal("MangoPlatformProxy: unable to establish HN daemon connection"
			"[error=%d]", hn_init_err);
	}

	bbque_assert ( 0 == hn_init_err );

	// Get the number of clusters
	int err = hn_get_num_clusters(&this->num_clusters);
	if ( HN_SUCCEEDED != err ) {
		logger->Fatal("MangoPlatformProxy: unable to get the number of clusters "
			"[error=%d]", err);
	}
	logger->Info("MangoPlatformProxy: nr. of clusters: %d", num_clusters);

	// Reset the platform (cluster by cluster)
	for (uint32_t cluster_id=0; cluster_id < this->num_clusters; ++cluster_id) {
		logger->Debug("MangoPlatformProxy: resetting cluster=<%d>...", cluster_id);
		// This function call may take several seconds to conclude
		int hn_reset_err = hn_reset(0, cluster_id);
		if (hn_reset_err == HN_SUCCEEDED) {
			logger->Info("MangoPlatformProxy: HN cluster=<%d> successfully initialized",
				cluster_id);
		} else {
			logger->Crit("MangoPlatformProxy: unable to reset the HN cluster=%d"
				" [error= %d]", cluster_id, hn_reset_err);
			// We consider this error non critical and we try to continue
		}
	}

	// Register our skimmer for the incoming partitions (it actually fills the partition list,
	// not skim it)
	// Priority of 100 means the maximum priority, this is the first skimmer to be executed
	ResourcePartitionValidator &rmv = ResourcePartitionValidator::GetInstance();
	rmv.RegisterSkimmer(std::make_shared<MangoPartitionSkimmer>() , 100);
	logger->Info("MangoPlatformProxy: partition skimmer registered");
}

MangoPlatformProxy::~MangoPlatformProxy() {
	logger->Info("MangoPlatformProxy: nothing left to be done");
}


const char* MangoPlatformProxy::GetPlatformID(int16_t system_id) const noexcept {
	UNUSED(system_id);
	return BBQUE_MANGOPP_PLATFORM_ID;
}

const char* MangoPlatformProxy::GetHardwareID(int16_t system_id) const noexcept {
	UNUSED(system_id);
	return BBQUE_TARGET_HARDWARE_ID;    // Defined in bbque.conf
}

bool MangoPlatformProxy::IsHighPerformance(
		bbque::res::ResourcePathPtr_t const & path) const {
	UNUSED(path);
	return false;
}

MangoPlatformProxy::ExitCode_t MangoPlatformProxy::Setup(SchedPtr_t papp) noexcept {
	ExitCode_t result = PLATFORM_OK;
	UNUSED(papp);
	return result;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::Release(SchedPtr_t papp) noexcept {
	logger->Info("Release: application [%s]...", papp->StrId());
	return PLATFORM_OK;
}


MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::ReclaimResources(SchedPtr_t sched) noexcept {
	ba::AppCPtr_t papp = std::dynamic_pointer_cast<ba::Application>(sched);
	auto partition = papp->GetPartition();
	if (partition != nullptr) {
		ResourcePartitionValidator &rmv(ResourcePartitionValidator::GetInstance());
		auto ret = rmv.RemovePartition(*papp->GetTaskGraph(), *partition);
		bbque_assert(ResourcePartitionValidator::PMV_OK == ret);
		if (ret != ResourcePartitionValidator::PMV_OK)
			logger->Warn("ReclaimResources: [%s] hw partition release failed", papp->StrId());
		else {
			papp->SetPartition(nullptr);
			logger->Info("ReclaimResources: [%s] hw partition released", papp->StrId());
		}
	}
	else
		logger->Warn("ReclaimResources: [%s] no partition to release", papp->StrId());
	return PLATFORM_OK;
}


MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::MapResources(SchedPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl) noexcept {
	UNUSED(papp);
	UNUSED(pres);
	UNUSED(excl);

	// This method is empty and this is unusual in Barbeque, however in Mango we use Skimmers
	// to get the list of partitions and set them

	return PLATFORM_OK;
}

void MangoPlatformProxy::Exit() {
	logger->Info("Exit: Termination...");

	// Stop HW counter monitors
	for (uint32_t cluster_id=0; cluster_id < this->num_clusters; ++cluster_id) {
		hn_tile_info_t tile_info;
		for (uint_fast32_t tile_id=0; tile_id < num_tiles; tile_id++) {
			int err = hn_get_tile_info(tile_id, &tile_info, cluster_id);
			if (HN_SUCCEEDED != err) {
				logger->Fatal("Exit: unable to get the info for cluster=<%d> tile=<%d>",
					cluster_id, tile_id);
				continue;
			}
	#ifdef CONFIG_BBQUE_PM_MANGO
			logger->Debug("Exit: disabling monitors...");
			if (tile_info.unit_family == HN_TILE_FAMILY_PEAK) {
				err = hn_stats_monitor_configure_tile(tile_id, 0, cluster_id);
				if (err == 0)
					logger->Info("Exit: stopping monitor for cluster=<%d> tile=<%d>",
						cluster_id, tile_id);
				else
					logger->Error("Error while stopping monitor for cluster=<%d> tile=<%d>",
						cluster_id, tile_id);
			}
	#endif
		}
	}

	// First release occupied resources, i.e., allocated memory for peakOS
	// ...hope partitions are correctly unset too
	for (uint32_t cluster_id=0; cluster_id < this->num_clusters; ++cluster_id) {
		for (auto rsc : allocated_resources_peakos) {
			uint32_t tile_mem = rsc.first;
			uint32_t addr     = rsc.second;
			hn_release_memory(tile_mem, addr, MANGO_PEAKOS_FILE_SIZE, cluster_id);
			logger->Info("Exit: cluster=<%d> "
				"released PEAK OS memory %d address 0x%08x", cluster_id, tile_mem, addr);
		}
	}

	// Just clean up stuffs...
	int hn_err_ret = hn_end();
	if (hn_err_ret != 0) {
		logger->Warn("Exit: Error occurred while terminating: %d", hn_err_ret);
	}
}

MangoPlatformProxy::ExitCode_t MangoPlatformProxy::Refresh() noexcept {
	refreshMode = true;
	// TODO (we really need this method?)
	return PLATFORM_OK;
}


MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::LoadPlatformData() noexcept {
	int err = -1;

	// Load platform data for each available HN cluster
	for (uint32_t cluster_id=0; cluster_id < this->num_clusters; ++cluster_id) {
		// Get the number of tiles
		err = hn_get_num_tiles(&this->num_tiles, &this->num_tiles_x, &this->num_tiles_y, cluster_id);
		if ( HN_SUCCEEDED != err ) {
			logger->Fatal("LoadPlatformData: unable to get the number of tiles [error=%d]", err);
			return PLATFORM_INIT_FAILED;
		}

		// Get the number of VNs
		err = hn_get_num_vns(&this->num_vns, cluster_id);
		if ( HN_SUCCEEDED != err ) {
			logger->Fatal("LoadPlatformData: unable to get the number of VNs [error=%d]", err);
			return PLATFORM_INIT_FAILED;
		}

		logger->Info("LoadPlatformData: cluster=<%d>: num_tiles=%d (%dx%d) num_vns=%d.",
				cluster_id,
				this->num_tiles, this->num_tiles_x, this->num_tiles_y,
				this->num_vns);

		// Now we have to register the tiles to the PlatformDescription and ResourceAccounter
		ExitCode_t pp_err = RegisterTiles(cluster_id);
		if (PLATFORM_OK != pp_err) {
			return pp_err;
		}

		// The tiles should now be booted by Barbeque using the PeakOS image
		pp_err = BootTiles(cluster_id);
		if (PLATFORM_OK != pp_err) {
			return pp_err;
		}
	}

	if (err < 0) {
		logger->Info("LoadPlatformData: some error occurred [error=%d]", err);
		return PLATFORM_INIT_FAILED;
	}

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::BootTiles_PEAK(uint32_t cluster_id, int tile_id) noexcept {
	uint32_t req_size = MANGO_PEAKOS_FILE_SIZE;
	uint32_t tile_memory;	// This will be filled with the memory id
	uint32_t base_addr;	    // This is the starting address selected

	// TODO: This is currently managed by the internal HN find memory, however, we have
	//	 to replace this with an hook to the MemoryManager here.
	int err = hn_find_memory(tile_id, req_size, &tile_memory, &base_addr, cluster_id);
	if (HN_SUCCEEDED != err) {
		logger->Error("BootTiles_PEAK: unable to get memory for tile=%d", tile_id);
		return PLATFORM_LOADING_FAILED;
	}

	// Allocate memory for PEAK OS
	logger->Debug("BootTiles_PEAK: cluster=<%d> tile=<%d> allocating memory [BASE_ADDR=0x%x SIZE=%d]...",
		cluster_id, tile_id, base_addr, req_size);
	err = hn_allocate_memory(tile_memory, base_addr, req_size, cluster_id);
	if (HN_SUCCEEDED != err) {
		logger->Error("BootTiles_PEAK: unable to allocate memory for tile=%d", tile_id);
		return PLATFORM_LOADING_FAILED;
	}

	// Load PEAK OK
	logger->Debug("BootTiles_PEAK: loading PEAK OS in memory id=%d [address=0x%x]...",
		tile_memory, base_addr);
	allocated_resources_peakos.push_back(std::make_pair(tile_memory, base_addr));

	err = hn_boot_unit(tile_id, tile_memory, base_addr, MANGO_PEAK_PROTOCOL, MANGO_PEAK_OS, cluster_id);
	if (HN_SUCCEEDED != err) {
		logger->Error("BootTiles_PEAK: unable to boot PEAK tile=%d", tile_id);
		return PLATFORM_LOADING_FAILED;
	}
	logger->Info("BootTiles_PEAK: cluster=<%d> tile=<%d> [PEAK_OS:%s] [PEAK_PROT:%s] booted",
		cluster_id, tile_id, MANGO_PEAK_OS, MANGO_PEAK_PROTOCOL);

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::BootTiles(uint32_t cluster_id) noexcept {
	hn_tile_info_t tile_info;
	for (uint_fast32_t tile_id=0; tile_id < num_tiles; tile_id++) {
		int err = hn_get_tile_info(tile_id, &tile_info, cluster_id);
		if (HN_SUCCEEDED != err) {
			logger->Fatal("BootTiles: unable to get info from tile=<%d> [error=%d].",
				tile_id, err);
			return PLATFORM_INIT_FAILED;
		}

		if (tile_info.unit_family == HN_TILE_FAMILY_PEAK) {
			err = BootTiles_PEAK(cluster_id, tile_id);
			if (PLATFORM_OK != err) {
				logger->Error("BootTiles: unable to boot cluster=<%d> tile=<%d>",
					cluster_id, tile_id);
				return PLATFORM_INIT_FAILED;
			}
#ifdef CONFIG_BBQUE_PM_MANGO
			// Enable monitoring stuff
			logger->Debug("BootTiles: cluster=<%d> tile=<%d> configuring monitors...",
				cluster_id, tile_id);
			err = hn_stats_monitor_configure_tile(tile_id, 1, cluster_id);
			if (err == 0) {
				err = hn_stats_monitor_set_polling_period(monitor_period_len);
				if (err == 0)
					logger->Info("BootTiles: cluster=<%d> tile=<%d> "
						"set monitoring period=%dms",
						cluster_id, tile_id, monitor_period_len);
				else
					logger->Error("BootTiles: cluster=<%d> tile=<%d> "
						"set monitoring period failed",
						cluster_id, tile_id);
			}
			else
				logger->Error("BootTiles: cluster=<%d> tile=<%d> unable to enable "
					"profiling", cluster_id, tile_id);
#endif
		}
		logger->Info("BootTiles: cluster=<%d> tile=<%d> initialized", cluster_id, tile_id);
	}
	logger->Info("BootTiles: cluster=<%d> all tiles successfully booted", cluster_id);

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::RegisterTiles(uint32_t cluster_id) noexcept {
	typedef PlatformDescription pd_t;	// Just for convenience

	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	pd_t &pd = pli->getPlatformInfo();
	pd_t::System &sys = pd.GetLocalSystem();

#ifdef CONFIG_BBQUE_WM
	PowerMonitor & wm(PowerMonitor::GetInstance());
	monitor_period_len = wm.GetPeriodLengthMs();
#endif

	for (uint_fast32_t tile_id=0; tile_id < num_tiles; tile_id++) {
		// First of all get the information of tiles from HN library
		hn_tile_info_t tile_info;
		int err = hn_get_tile_info(tile_id, &tile_info, cluster_id);
		if (HN_SUCCEEDED != err) {
			logger->Fatal("RegisterTiles: unable to get info about "
				"cluster=<%d> tile=<%d> [error=%d]",
				cluster_id, tile_id, err);
			return PLATFORM_INIT_FAILED;
		}

		logger->Info("RegisterTiles: cluster=<%d> tile={id=%d family=%s model=%s}",
			cluster_id, tile_id,
			hn_to_str_unit_family(tile_info.unit_family),
			hn_to_str_unit_model(tile_info.unit_model));

		MangoTile mt(tile_id,
			(MangoTile::MangoUnitFamily_t)(tile_info.unit_family),
			(MangoTile::MangoUnitModel_t)(tile_info.unit_model));
		sys.AddAccelerator(mt);

		// Map the HN cluster to resource of type "GROUP"
		std::string group_id(".");
		group_id += std::string(GetResourceTypeString(br::ResourceType::GROUP));
		group_id += std::to_string(cluster_id);
		std::string group_prefix(sys.GetPath() + group_id);
		mt.SetPrefix(group_prefix);

		// For each tile, we register how many PE how may cores the accelerator has, this
		// will simplify the tracking of ResourceAccounter resources
		for (int i=0; i < MangoTile::GetCoreNr(mt.GetFamily(), mt.GetModel()); i++) {
			pd_t::ProcessingElement pe(i , 0, 100, pd_t::PartitionType_t::MDEV);
			pe.SetPrefix(mt.GetPath());
			logger->Debug("RegisterTiles: cluster=<%d> tile=<%d> core=<%d>: path=%s",
				cluster_id, tile_id, i, pe.GetPath().c_str());
			mt.AddProcessingElement(pe);
			auto rsrc_ptr = ra.RegisterResource(pe.GetPath(), "", 100);
			rsrc_ptr->SetModel(hn_to_str_unit_family(tile_info.unit_family));
		}

#ifdef CONFIG_BBQUE_WM
		// Register only one processing element per tile (representing a reference
		// to the entire tile), since we do not  expect to have per-core status
		// information
		std::string acc_pe_path(mt.GetPath() + ".pe0");
		wm.Register(acc_pe_path);
		logger->Debug("RegisterTiles: [%s] registered for power monitoring",
			acc_pe_path.c_str());
#endif

		// Let now register the memories. Unfortunately, memories are not easy to be
		// retrieved, we have to iterate over all tiles and search memories
		unsigned int mem_attached = tile_info.memory_attached;
		if (mem_attached != 0) {
			logger->Debug("RegisterTiles: cluster=<%d> tile=<%d>: mem_attached=%d",
				cluster_id, tile_id, mem_attached);
			// Register the memory attached if present and if not already registered
			// Fixed mem_id to tile_id, it can be only a memory
			// controller attached to a mango tile. So, the mem_id
			// equals to the tile_id, since the HN does not set IDs
			// for memories
			ExitCode_t reg_err = RegisterMemoryBank(
						group_prefix, cluster_id, tile_id, tile_id);
			if (reg_err) {
				return reg_err;
			}

			// And now assign in PD the correct memory pointer
			auto mem = sys.GetMemoryById(mem_attached);
			mt.SetMemory(mem);
		}
	}

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::RegisterMemoryBank(
		std::string const & group_prefix,
		uint32_t cluster_id,
		int tile_id,
		int mem_id) noexcept {
	typedef PlatformDescription pd_t;	// Just for convenience
	ResourceAccounter &ra(ResourceAccounter::GetInstance());

	logger->Debug("RegisterMemoryBank: cluster=<%d> tile=<%d> memory=<%d>",
		cluster_id, tile_id, mem_id);
	bbque_assert(mem_id > -1 && mem_id < MANGO_MAX_MEMORIES);
	if (found_memory_banks.test(mem_id)) {
		// We have already registered this memory, nothing to do
		return PLATFORM_OK;
	}

	// A bitset is used to keep track the already registered memory (we could also use the
	// ResourceAccounter methods, but this is faster)
	found_memory_banks.set(mem_id);
	uint32_t memory_size;
	int err = hn_get_memory_size(tile_id, &memory_size, cluster_id);
	if (HN_SUCCEEDED != err) {
		logger->Fatal("RegisterMemoryBank: cluster=<%d> tile=<%d> memory=<%d>: "
			"missing information on memory node [error=%d]",
			cluster_id, tile_id, mem_id, err);
		return PLATFORM_INIT_FAILED;
	}

	pd_t &pd = pli->getPlatformInfo();
	pd_t::System &sys = pd.GetLocalSystem();

	// HN memory banks are under the "sys.grp" scope (group_prefix), so
	// they are distinguished w.r.t to the memory banks accessed by CPU
	// code
	pd_t::Memory mem(mem_id, memory_size);
	mem.SetPrefix(group_prefix);
	logger->Debug("RegisterMemoryBank: memory id=<%d> path=<%s>", tile_id, mem.GetPath().c_str());

	sys.AddMemory(std::make_shared<pd_t::Memory>(mem));
	ra.RegisterResource(mem.GetPath(), "", memory_size);
	logger->Info("RegisterMemoryBank: memory id=<%d> size=%u", tile_id, memory_size);

	return PLATFORM_OK;
}


/* It follows some static local function not member of any class: we selected this approach in order
   to avoid to add the hn.h in the header file mango_platform_proxy.h. This was done in order to
   avoid the pollution of the global namespace of Barbeque with all the mess of the HN library.
   This is definetely not elegant, so maybe we want to FIXME this in the future. */

/**
 * @brief From the TG we have the information of threads number and the architecture type, but
 *	  we have to pass in aggregated form to the HN library, so this function will convert
 *	  to HN library type
 */
static uint32_t ArchTypeToMangoType(ArchType type, unsigned int nr_thread) {
	UNUSED(nr_thread);

	switch (type) {
		case ArchType::PEAK:
			return HN_TILE_FAMILY_PEAK;
		case ArchType::NUPLUS:
			return HN_TILE_FAMILY_NUPLUS;
		case ArchType::DCT:
			return HN_TILE_FAMILY_DCT;
		case ArchType::TETRAPOD:
			return HN_TILE_FAMILY_TETRAPOD;

		case ArchType::GN:
			return HN_TILE_FAMILY_GN;	// In GN emulation case we are not interested in the real time
		// TODO add other architectures
		default:
			throw std::runtime_error("Unsupported architecture");
	}
}

/**
 * @brief This function basically converts a TaskGraph object in the request struct of HN library
 *
 * \deprecated
 */
/*
static hn_st_request_t FillReq(const TaskGraph &tg) __attribute__((unused)) {

	ApplicationManager &am = ApplicationManager::GetInstance();

	hn_st_request_t req;
	req.num_comp_rsc    = tg.TaskCount();

	// We request the number of shared buffer + the number of task, since we have to load also
	// the kernels in memory
	req.num_mem_buffers = tg.BufferCount() + tg.TaskCount();

	// We need the application to get the application bandwidth requirements
	AppPtr_t app = am.GetApplication(tg.GetApplicationId());

	unsigned int i;


	// Fill the computing resources requested
	i=0;
	for (auto t : tg.Tasks()) {
		req.comp_rsc_types[i++] = ArchTypeToMangoType(t.second->GetAssignedArch(),
							      t.second->GetThreadCount());
	}

	// Fill the buffer requested (note: we will add other buffers next)
	i=0;
	for (auto b : tg.Buffers()) {
		bbque_assert(i < req.num_mem_buffers);
		req.mem_buffers_size[i++] = b.second->Size();
	}

	unsigned int filled_buffers = i;

	// We now register the mapping task-buffer and we allocate a per-task buffer to allocate the
	// kernel image.
	// The bandwitdth is not currently managed by the HN library, so we use only a boolean value
	// to indicate that the buffer uses that task and vice versa.
	i=0;
	for (auto t : tg.Tasks()) {
		auto tg_req = app->GetTaskRequirements(t.second->Id());
		unsigned int j=0;
		for (auto b : tg.Buffers()) {
			auto first = t.second->InputBuffers().cbegin();
			auto last = t.second->InputBuffers().cend();

			if ( std::find(first, last, b.second->Id()) != last) {
				req.bw_read_req[i][j]  = 1; // TODO tg_req.InBandwidth();
			} else {
				req.bw_read_req[i][j]  = 0;
			}

			first = t.second->OutputBuffers().cbegin();
			last = t.second->OutputBuffers().cend();

			if ( std::find(first, last, b.second->Id()) != last) {
				req.bw_write_req[i][j]  = 1; // TODO  tg_req.OutBandwidth();
			} else {
				req.bw_write_req[i][j]  = 0;
			}

			j++;
		}

		// Extra buffer for kernel and stack memory
		auto arch =  t.second->GetAssignedArch();
		auto ksize = t.second->Targets()[arch]->BinarySize();
		auto ssize = t.second->Targets()[arch]->StackSize();
		int kimage_index = filled_buffers + i;
		req.mem_buffers_size[kimage_index] = ksize + ssize;
		req.bw_read_req[kimage_index] [j+i]  = 1;	// TODO
		req.bw_write_req[kimage_index][j+i]  = 1;	// TODO

		i++;
	}

	return req;
}
*/


/****************************************************************************
 *  MANGO Partition Skimmer                                                 *
 ****************************************************************************/


/**
 * @brief This function gets sets of units for running the tasks in the TG
 */
static void FindUnitsSets(
		const TaskGraph &tg,
		uint32_t hw_cluster_id,
		unsigned int ***tiles,
		unsigned int ***families_order,
		unsigned int *nsets) {

	int num_tiles = tg.TaskCount();
	unsigned int *tiles_family = new unsigned int[num_tiles];  // FIXME UPV -> POLIMI static memory better ?

	// Fill the computing resources requested
	int i=0;
	for (auto t : tg.Tasks()) {
#ifndef CONFIG_LIBMANGO_GN
		if (t.second->GetAssignedArch() == ArchType::GN) {
			logger->Error("Tile %i is of type GN but BarbequeRTRM is not compiled in GN emulation mode."
			"This will probably lead to an allocation failure.", i);
		}
#endif
		tiles_family[i++] = ArchTypeToMangoType(
			t.second->GetAssignedArch(), t.second->GetThreadCount());
	}

	int res = hn_find_units_sets(0, num_tiles, tiles_family, tiles, families_order, nsets,
					hw_cluster_id);

	if (tiles_family == nullptr) {
		logger->Warn("FindUnitsSets: unexpected null pointer (tiles_family)"
			" [libhn suspect corruption]");
		return;
	}

	delete[] tiles_family;
	if (res != HN_SUCCEEDED)
		throw std::runtime_error("Unable to find units sets");
}

/**
 * @brief This function gets a set of memories for the buffers in the TG
 */
static void FindAndAllocateMemory(
		const TaskGraph &tg,
		uint32_t hw_cluster_id,
		unsigned int *tiles_set,
        unsigned int *mem_buffers_tiles,
        unsigned int *mem_buffers_addr) {

	// We request the number of shared buffer + the number of task, since we have to load also
	// the kernels in memory
	int num_mem_buffers = tg.BufferCount() + tg.TaskCount();
	unsigned int *mem_buffers_size = new unsigned int[num_mem_buffers]; // FIXME UPV -> POLIMI, static mem better?

	int i=0;
	for (auto b : tg.Buffers()) {
		bbque_assert(i < num_mem_buffers);
		mem_buffers_size[i++] = b.second->Size();
	}

	int filled_buffers = i;

	// We now register the mapping task-buffer and we allocate a per-task buffer to allocate the
	// kernel image.
	// The bandwitdth is not currently managed by the HN library, so we use only a boolean value
	// to indicate that the buffer uses that task and vice versa.
	i=0;
	for (auto t : tg.Tasks()) {
		// Extra buffer for kernel and stack memory
		auto arch =  t.second->GetAssignedArch();
		auto ksize = t.second->Targets()[arch]->BinarySize();
		auto ssize = t.second->Targets()[arch]->StackSize();
		int kimage_index = filled_buffers + i;
		mem_buffers_size[kimage_index] = ksize + ssize;
		i++;
	}

	for (i = 0; i < num_mem_buffers; i++) {
		unsigned int tile = 0;
		if (i >= filled_buffers) {
			// Let's set the kernel buffer close to the tile where the task will run on
			tile = tiles_set[i - filled_buffers];
		} else {
			// for input and output buffers.
			// FIXME Better to allocate the buffer close to the tiles the unit that are using it will be mapped on
			tile = tiles_set[0];
		}
		int res = hn_find_memory(tile, mem_buffers_size[i], &mem_buffers_tiles[i], &mem_buffers_addr[i],
					hw_cluster_id);
		if (res != HN_SUCCEEDED) {
			delete[] mem_buffers_size;
			throw std::runtime_error("Unable to find memory");
		}

		res = hn_allocate_memory(mem_buffers_tiles[i], mem_buffers_addr[i], mem_buffers_size[i],
					hw_cluster_id);
		if (res != HN_SUCCEEDED) {
			delete[] mem_buffers_size;
			throw std::runtime_error("Unable to allocate memory");
		}
	}

	if (mem_buffers_size == nullptr) {
		logger->Warn("FindAndAllocateMemory: unexpected null pointer: mem_buffers_size"
			" [libhn suspect corruption]");
		return;
	}

	delete[] mem_buffers_size;
}

/**
 * @brief This function convert a TG + HN response to a Partition object used in Barbeque
 */
/*
static Partition GetPartition(const TaskGraph &tg, hn_st_response_t *res, int partition_id) noexcept {

	auto it_task  = tg.Tasks().begin();
	int tasks_size = tg.Tasks().size();
	auto it_buff  = tg.Buffers().begin();
	int buff_size = tg.Buffers().size();

	Partition part(partition_id);

	for (int j=0; j < tasks_size; j++) {
		part.MapTask(it_task->second, res->comp_rsc_tiles[j],
			res->mem_buffers_tiles[buff_size +j], res->mem_buffers_addr[buff_size + j]);
		it_task++;
	}
	bbque_assert(it_task == tg.Tasks().end());

	for (int j=0; j < buff_size; j++) {
		part.MapBuffer(it_buff->second, res->mem_buffers_tiles[j], res->mem_buffers_addr[j]);
		it_buff++;
	}
	bbque_assert(it_buff == tg.Buffers().end());

	return part;
}
*/
static Partition GetPartition(
		const TaskGraph &tg,
		unsigned int hw_cluster_id,
		unsigned int *tiles,
		unsigned int *families_order,
		unsigned int *mem_buffers_tiles,
		unsigned int *mem_buffers_addr,
        int partition_id) noexcept {
	auto it_task      = tg.Tasks().begin();
	int tasks_size    = tg.Tasks().size();
	auto it_buff      = tg.Buffers().begin();
	int buff_size     = tg.Buffers().size();
	bool *tile_mapped = new bool[tasks_size];
	std::fill_n(tile_mapped, tasks_size, false);

	// The partition has a cluster scope
	Partition part(partition_id, hw_cluster_id);

	// FIXME UPV -> POLIMI do it in a more efficient way if required
	//       We have to map the task to a tile according to its family type
	for (int j=0; j < tasks_size; j++) {
		uint32_t family = ArchTypeToMangoType(
			it_task->second->GetAssignedArch(),
			it_task->second->GetThreadCount());

		// look for the family type of the task
		int k = 0;
		for (k = 0; k < tasks_size; k++) {
			if ((families_order[k] == family) && (!tile_mapped[k])) {
				tile_mapped[k] = true;
				break;
			}
		}
		// we are always going to find an unmapped tile as the sets provided by
		// hn_find_units_sets hnlib function return sets of task_size tiles
		part.MapTask(it_task->second, tiles[k],
				mem_buffers_tiles[buff_size + j],
				mem_buffers_addr[buff_size + j]);
		it_task++;
	}

	delete[] tile_mapped;
	bbque_assert(it_task == tg.Tasks().end());
	for (int j=0; j < buff_size; j++) {
		part.MapBuffer(it_buff->second, mem_buffers_tiles[j], mem_buffers_addr[j]);
		it_buff++;
	}
	bbque_assert(it_buff == tg.Buffers().end());

	return part;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::SetAddresses(
		const TaskGraph &tg,
		const Partition &partition) noexcept {

	uint32_t hw_cluster_id = partition.GetClusterId();

	// Assign a memory area to buffers
	for ( auto buffer : tg.Buffers()) {
		uint32_t memory_bank = partition.GetMemoryBank(buffer.second);
		uint32_t phy_addr    = partition.GetBufferAddress(buffer.second);
		logger->Debug("Buffer %d is located at memory id %d [address=0x%x]",
				buffer.second->Id(), memory_bank, phy_addr);
		buffer.second->SetMemoryBank(memory_bank);
		buffer.second->SetPhysicalAddress(phy_addr);

		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		if (hn_allocate_memory(
			memory_bank, phy_addr, buffer.second->Size(), hw_cluster_id) != HN_SUCCEEDED) {
			return SK_GENERIC_ERROR;
		}
	}

	// Assign a memory area to kernels (executable and stack)
	for ( auto task : tg.Tasks()) {
		auto arch = task.second->GetAssignedArch();
		uint32_t phy_addr    = partition.GetKernelAddress(task.second);
		uint32_t mem_tile    = partition.GetKernelBank(task.second);
		auto     ksize       = task.second->Targets()[arch]->BinarySize();
		auto     ssize       = task.second->Targets()[arch]->StackSize();
		logger->Debug("Task %d allocated space for kernel %s [mem_id=%u address=0x%x size=%lu]",
				task.second->Id(), GetStringFromArchType(arch), mem_tile, phy_addr, ksize + ssize);
		task.second->Targets()[arch]->SetMemoryBank(mem_tile);
		task.second->Targets()[arch]->SetAddress(phy_addr);

		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		if (hn_allocate_memory(mem_tile, phy_addr, ksize + ssize, hw_cluster_id) != HN_SUCCEEDED) {
			return SK_GENERIC_ERROR;
		}
	}

	return SK_OK;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::Skim(
		const TaskGraph &tg,
		std::list<Partition>&part_list,
		uint32_t hw_cluster_id) noexcept {
	unsigned int num_mem_buffers = tg.BufferCount() + tg.TaskCount();
	ExitCode_t res               = SK_OK;
	auto it_task                 = tg.Tasks().begin();
	size_t tasks_size            = tg.Tasks().size();
	auto it_buff                 = tg.Buffers().begin();
	size_t buff_size             = tg.Buffers().size();
	uint32_t **tiles             = NULL;
	uint32_t **families_order    = NULL;
	uint32_t **mem_buffers_tiles = NULL;
	uint32_t **mem_buffers_addr  = NULL;
	uint32_t *mem_buffers_size   = new uint32_t[buff_size+tasks_size];
	uint32_t num_sets            = 0;

	bbque_assert(part_list.empty());

	// Let's get the required memory size per kernel (executable + stack)
	logger->Debug("MangoPartitionSkimmer: request summary: ");
	for (size_t i = 0; i < tasks_size; i++) {
		auto arch = it_task->second->GetAssignedArch();
		uint32_t unit_family = ArchTypeToMangoType(arch,
				               it_task->second->GetThreadCount());
		auto ksize = it_task->second->Targets()[arch]->BinarySize();
		auto ssize = it_task->second->Targets()[arch]->StackSize();
		mem_buffers_size[buff_size + i] = ksize + ssize;
		it_task++;
		logger->Debug("  -> Computing Resource %d, HN type %s",
			i, hn_to_str_unit_family(unit_family));
	}
	bbque_assert(it_task == tg.Tasks().end());

	// Let's get the required memory size per buffer
	for (size_t i = 0; i < buff_size; i++) {
		uint32_t mem_size = it_buff->second->Size();
		mem_buffers_size[i] = mem_size;
		it_buff++;
		logger->Debug("  -> Memory buffer %d, size %d", i, mem_size);
	}
	bbque_assert(it_buff == tg.Buffers().end());

	try {
		// Let's try to find the partitions that satisfy the task-graph
		// requirements
		// It may generate exception if the architecture is not supported
		// (in that case we should not be here ;-))
		logger->Debug("MangoPartitionSkimmer: looking for HN resources...");

		// - Find different sets of resources (partitions)
		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		FindUnitsSets(tg, hw_cluster_id, &tiles, &families_order, &num_sets);
		logger->Debug("MangoPartitionSkimmer: HN returned %d available sets", num_sets);

		// - Find and reserve memory for every set. We cannot call
		//   hn_find_memory more than once without allocate the memory
		//   returned, since it would return the same bank
		mem_buffers_tiles = new uint32_t*[num_sets];
		mem_buffers_addr  = new uint32_t*[num_sets];
		memset(mem_buffers_tiles, 0, num_sets*sizeof(uint32_t *));
		memset(mem_buffers_addr, 0, num_sets*sizeof(uint32_t *));

		for (unsigned int i = 0; i < num_sets; i++) {
			// Find and allocate memory close the tiles/units in the set
			mem_buffers_tiles[i] = new uint32_t[num_mem_buffers];
			mem_buffers_addr[i]  = new uint32_t[num_mem_buffers];
			FindAndAllocateMemory(tg, hw_cluster_id, tiles[i],
				mem_buffers_tiles[i], mem_buffers_addr[i]);
		}
	}
	catch (const std::runtime_error &err) {
		logger->Error("MangoPartitionSkimmer: %s", err.what());
		res = SK_NO_PARTITION;
	}

	if (res == SK_OK) {
		if ( num_sets == 0 ) {
			// No feasible partition found
			logger->Warn("MangoPartitionSkimmer: no feasible partitions");
			res = SK_NO_PARTITION;
		}
		else {
			// Fill the partition vector with the partitions returned by HN library
			for (unsigned int i=0; i < num_sets; i++) {
				Partition part = GetPartition(
							tg, hw_cluster_id,
							tiles[i], families_order[i],
							mem_buffers_tiles[i],
							mem_buffers_addr[i],
							i); // partition id
				part_list.push_back(part);
			}
		}
	}

	// let's deallocate memory created in the hn_find_units_sets hnlib function
	if (tiles != nullptr) {
		for (unsigned int i = 0; i < num_sets; i++) {
			free(tiles[i]);
		}
		free(tiles);
	}

	if (families_order != nullptr) {
		for (unsigned int i = 0; i < num_sets; i++) {
			free(families_order[i]);
		}
		free(families_order);
	}

	// let's deallocate memory created in this method
	std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
	if (mem_buffers_tiles != nullptr) {
		for (unsigned int i = 0; i < num_sets; i++) {
			if (mem_buffers_tiles[i] != NULL) {
				// TRICK we allocated memory banks when finding them in
				// order to get different banks (see try above), so now
				// it is the right moment to release them.
				// Next, When setting the partition it will be allocated again
				for (unsigned int j = 0; j < num_mem_buffers; j++) {
					hn_release_memory(
							mem_buffers_tiles[i][j],
							mem_buffers_addr[i][j],
							mem_buffers_size[j],
                                                        hw_cluster_id);
				}
				delete[] mem_buffers_tiles[i];
			}
		}
		delete[] mem_buffers_tiles;
	}

	if (mem_buffers_addr != nullptr) {
		for (unsigned int i = 0; i < num_sets; i++) {
			if (mem_buffers_addr[i] != NULL) {
				delete[] mem_buffers_addr[i];
			}
		}
		delete[] mem_buffers_addr;
	}

	if (mem_buffers_size != nullptr) {
		delete[] mem_buffers_size;
	}
	else {
		logger->Warn("MangoPartitionSkimmer: unexpected null pointer: mem_buffers_size"
			" [libhn suspect corruption]");
	}

	return res;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::SetPartition(
		TaskGraph &tg,
		const Partition &partition) noexcept {

	// Set the HW cluster including the mapped resources
	uint32_t hw_cluster_id = partition.GetClusterId();
	tg.SetCluster(hw_cluster_id);

	// We set the mapping of buffers-addresses based on the selected partition
	ExitCode_t err = SetAddresses(tg, partition);

	// Set the assigned processor (for each task)
	for ( auto task : tg.Tasks()) {
		auto tile_id = partition.GetUnit(task.second);
		task.second->SetMappedProcessor(tile_id);
		logger->Debug("SetPartition: task %d mapped to processor (tile) %d",
			task.second->Id(), tile_id);
	}

	// Now, we have to ask for the location in TileReg of events
	// TODO: Manage the UNIZG case
	// TODO: what to do in case of failure?
	// TODO: Policy for tile selection
	for ( auto event : tg.Events()) {
		uint32_t phy_addr;

		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		int err = hn_get_synch_id (&phy_addr, 0, HN_READRESET_INCRWRITE_REG_TYPE,
				hw_cluster_id);
		if (err != HN_SUCCEEDED) {
			logger->Error("SetPartition: cannot find sync register for event %d",
				event.second->Id());

			// TODO we should deallocate the other assigned events?
			return SK_GENERIC_ERROR;
	  	}

		logger->Debug("SetPartition: event %d assigned to ID 0x%x", event.second->Id(), phy_addr);
		event.second->SetPhysicalAddress(phy_addr);
	}

	// Reserve the units (processors)
	unsigned int num_tiles = tg.TaskCount();
	uint32_t *units = new uint32_t[num_tiles];
	int i = 0;
	for ( auto task : tg.Tasks()) {
		auto arch = task.second->GetAssignedArch();
		units[i]  = partition.GetUnit(task.second);
		logger->Debug("SetPartition: task %d reserved a unit at tile %u for kernel %s",
				task.second->Id(), units[i], GetStringFromArchType(arch));
		i++;
	}

	std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
	if (hn_reserve_units_set(num_tiles, units, hw_cluster_id) != HN_SUCCEEDED) {
		err = SK_GENERIC_ERROR;
	}

	if (units == nullptr) {
		logger->Warn("SetPartition: unexpected null pointer: units"
			" [libhn suspect corruption]");
		return SK_GENERIC_ERROR;
	}
	delete[] units;

	return err;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::UnsetPartition(
		const TaskGraph &tg,
		const Partition &partition) noexcept {

	uint32_t part_id = partition.GetId();
	uint32_t hw_cluster_id = partition.GetClusterId();

	ExitCode_t ret = SK_OK;
	logger->Debug("UnsetPartition: deallocating partition [id=%d]...", part_id);

	for ( const auto &event : tg.Events()) {
		bbque_assert(event.second);
		uint32_t phy_addr = event.second->PhysicalAddress();
		logger->Debug("UnsetPartition: releasing event %d (ID 0x%x)",
			event.second->Id(), phy_addr);

		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		int err = hn_release_synch_id (phy_addr, hw_cluster_id);
		if(err != HN_SUCCEEDED) {
			logger->Warn("UnsetPartition: unable to release event %d (ID 0x%x)",
				event.second->Id(), phy_addr);
		}
	}

	// release memory
	for ( auto buffer : tg.Buffers()) {
		uint32_t memory_bank = partition.GetMemoryBank(buffer.second);
		uint32_t phy_addr    = partition.GetBufferAddress(buffer.second);
		uint32_t size        = buffer.second->Size();;
		logger->Debug("UnsetPartition: buffer %d is released at bank %d [address=0x%x]",
				buffer.second->Id(), memory_bank, phy_addr);
		buffer.second->SetMemoryBank(memory_bank);
		buffer.second->SetPhysicalAddress(phy_addr);

		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		if (hn_release_memory(memory_bank, phy_addr, size, hw_cluster_id) != HN_SUCCEEDED) {
			ret = SK_GENERIC_ERROR;
		}
	}

	for ( auto task : tg.Tasks()) {
		auto arch = task.second->GetAssignedArch();
		uint32_t phy_addr    = partition.GetKernelAddress(task.second);
		uint32_t mem_tile    = partition.GetKernelBank(task.second);
		auto     ksize       = task.second->Targets()[arch]->BinarySize();
		auto     ssize       = task.second->Targets()[arch]->StackSize();
		logger->Debug("UnsetPartition: task %d released space for kernel %s"
				" [bank=%d, address=0x%x size=%lu]",
				task.second->Id(), GetStringFromArchType(arch), mem_tile, phy_addr, ksize + ssize);
		task.second->Targets()[arch]->SetMemoryBank(mem_tile);
		task.second->Targets()[arch]->SetAddress(phy_addr);

		std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
		if (hn_release_memory(mem_tile, phy_addr, ksize + ssize, hw_cluster_id) != HN_SUCCEEDED) {
			ret = SK_GENERIC_ERROR;
		}
	}

	// release units
	unsigned int num_tiles = tg.TaskCount();
	uint32_t *units = new uint32_t[num_tiles];
	int i = 0;
	for ( auto task : tg.Tasks()) {
		auto arch = task.second->GetAssignedArch();
		units[i] = partition.GetUnit(task.second);
		logger->Debug("UnsetPartition: task %d released tile %u for kernel %s",
				task.second->Id(), units[i], GetStringFromArchType(arch));
		i++;
	}

	std::unique_lock<std::recursive_mutex> hn_lock(hn_mutex);
	if (hn_release_units_set(num_tiles, units, hw_cluster_id) != HN_SUCCEEDED) {
		ret = SK_GENERIC_ERROR;
	}

	if (units == nullptr) {
		logger->Warn("UnsetPartition: unexpected null pointer: units"
			" [libhn suspect corruption]");
		return SK_GENERIC_ERROR;
	}

	delete[] units;

	return ret;
}

MangoPlatformProxy::MangoPartitionSkimmer::MangoPartitionSkimmer(): PartitionSkimmer(SKT_MANGO_HN)
{
	logger = bu::Logger::GetLogger(MANGO_PP_NAMESPACE ".skm");
	bbque_assert(logger);
}

}	// namespace pp
}	// namespace bbque
