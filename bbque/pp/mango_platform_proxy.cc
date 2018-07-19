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

#define BBQUE_MANGOPP_PLATFORM_ID		"org.mango"
#define MANGO_MEMORY_COUNT_START 10
#define MANGO_PEAKOS_FILE_SIZE 256*1024*1024

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

	logger->Debug("Initializing communication with MANGO platform...");
	int hn_init_err = hn_initialize(filter, UPV_PARTITION_STRATEGY, 1, 0, 0);
	if(hn_init_err == HN_SUCCEEDED) {
		logger->Info("HN Daemon connection established.");
	} else {
		logger->Fatal("Unable to establish HN daemon connection. Error code: %d", hn_init_err);
	}

	bbque_assert ( 0 == hn_init_err );

	// We have now to reset the platform
	// This function call may take several seconds to conclude
	logger->Debug("Resetting MANGO platform...");

	int hn_reset_err = hn_reset(0);
	if(hn_reset_err == HN_SUCCEEDED) {
		logger->Info("HN Library successfully initialized");
	} else {
		logger->Crit("Unable to reset the HN board. Error code: %d", hn_reset_err);
		// We consider this error non critical and we try to continue
	}

	// Register our skimmer for the incoming partitions (it actually fills the partition list,
	// not skim it)
	// Priority of 100 means the maximum priority, this is the first skimmer to be executed
	ResourcePartitionValidator &rmv = ResourcePartitionValidator::GetInstance();
	rmv.RegisterSkimmer(std::make_shared<MangoPartitionSkimmer>() , 100);
}

MangoPlatformProxy::~MangoPlatformProxy() {

	// first release occupied resources, p.e. allocated memory for peakOS
	// Hope Partitions are unset correctly too
	for (auto rsc : allocated_resources_peakos) {
		uint32_t tile_mem = rsc.first;
		uint32_t addr     = rsc.second;
		hn_release_memory(tile_mem, addr, MANGO_PEAKOS_FILE_SIZE);
		logger->Info("Released peakOS memory %d address 0x%08x", tile_mem, addr);
	}

	// Just clean up stuffs...
	int hn_err_ret = hn_end();
	bbque_assert(0 == hn_err_ret);

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

MangoPlatformProxy::ExitCode_t MangoPlatformProxy::Setup(AppPtr_t papp) noexcept {
	ExitCode_t result = PLATFORM_OK;
	UNUSED(papp);
	return result;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::Release(AppPtr_t papp) noexcept {
	logger->Info("Release: application [%s]...", papp->StrId());
	ReclaimResources(papp);
	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::ReclaimResources(AppPtr_t papp) noexcept {
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

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::MapResources(AppPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl) noexcept {
	UNUSED(papp);
	UNUSED(pres);
	UNUSED(excl);

	// This method is empty and this is unusual in Barbeque, however in Mango we use Skimmers
	// to get the list of partitions and set them

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t MangoPlatformProxy::Refresh() noexcept {
	refreshMode = true;
	// TODO (we really need this method?)
	return PLATFORM_OK;
}


MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::LoadPlatformData() noexcept {
	// Get the number of tiles
	int err = hn_get_num_tiles(&this->num_tiles, &this->num_tiles_x, &this->num_tiles_y);
	if ( HN_SUCCEEDED != err ) {
		logger->Fatal("Unable to get the number of tiles [err=%d]", err);
		return PLATFORM_INIT_FAILED;
	}

	// Get the number of VNs
	err = hn_get_num_vns(&this->num_vns);
	if ( HN_SUCCEEDED != err ) {
		logger->Fatal("Unable to get the number of VNs [err=%d]", err);
		return PLATFORM_INIT_FAILED;
	}
	logger->Info("Found a total of %d tiles (%dx%d) and %d VNs.", this->num_tiles,
			this->num_tiles_x, this->num_tiles_y, this->num_vns);

	// Now we have to register the tiles to the PlatformDescription and ResourceAccounter
	ExitCode_t pp_err = RegisterTiles();
	if (PLATFORM_OK != pp_err) {
		return pp_err;
	}

	// The tiles should now be booted by Barbeque using the PeakOS image
	pp_err = BootTiles();
	if (PLATFORM_OK != pp_err) {
		return pp_err;
	}

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::BootTiles_PEAK(int tile) noexcept {
	uint32_t req_size = MANGO_PEAKOS_FILE_SIZE;
	uint32_t tile_memory;	// This will be filled with the memory id
	uint32_t base_addr;	    // This is the starting address selected

	// TODO: This is currently managed by the internal HN find memory, however, we have
	//	 to replace this with an hook to the MemoryManager here.
	int err = hn_find_memory(tile, req_size, &tile_memory, &base_addr);
	if (HN_SUCCEEDED != err) {
		logger->Error("BootTiles_PEAK: Unable to get memory for tile nr=%d", tile);
		return PLATFORM_LOADING_FAILED;
	}

	err = hn_allocate_memory(tile_memory, base_addr, req_size);
	if (HN_SUCCEEDED != err) {
		logger->Error("BootTiles_PEAK: Unable to allocate memory for tile nr=%d", tile);
		return PLATFORM_LOADING_FAILED;
	}
	logger->Debug("Loading PEAK OS in memory id %d [address=0x%x]", tile_memory, base_addr);
	allocated_resources_peakos[tile_memory] = base_addr;

	logger->Debug("Booting PEAK tile nr=%d [PEAK_OS:%s] [PEAK_PROT:%s]", tile, MANGO_PEAK_OS,
			MANGO_PEAK_PROTOCOL);
	err = hn_boot_unit(tile, tile_memory, base_addr, MANGO_PEAK_PROTOCOL, MANGO_PEAK_OS);
	if (HN_SUCCEEDED != err) {
		logger->Error("Unable to boot PEAK tile nr=%d", tile);
		return PLATFORM_LOADING_FAILED;
	}

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::BootTiles() noexcept {
	hn_tile_info_t tile_info;
	for (uint_fast32_t i=0; i < num_tiles; i++) {
		int err = hn_get_tile_info(i, &tile_info);
		if (HN_SUCCEEDED != err) {
			logger->Fatal("Unable to get the tile nr.%d [error=%d].", i, err);
			return PLATFORM_INIT_FAILED;
		}

		if (tile_info.unit_family == HN_TILE_FAMILY_PEAK) {
			err = BootTiles_PEAK(i);
			if (PLATFORM_OK != err) {
				logger->Error("Unable to boot tile nr=%d", i);
				return PLATFORM_INIT_FAILED;
			}
		}
	}

	logger->Info("All tiles successfully booted.");

	return PLATFORM_OK;
}

MangoPlatformProxy::ExitCode_t
MangoPlatformProxy::RegisterTiles() noexcept {
	typedef PlatformDescription pd_t;	// Just for convenience

	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	pd_t &pd = pli->getPlatformInfo();
	pd_t::System &sys = pd.GetLocalSystem();

#ifdef CONFIG_BBQUE_WM
	PowerMonitor & wm(PowerMonitor::GetInstance());
#endif

	for (uint_fast32_t i=0; i < num_tiles; i++) {
		// First of all get the information of tiles from HN library
		hn_tile_info_t tile_info;
		int err = hn_get_tile_info(i, &tile_info);
		if (HN_SUCCEEDED != err) {
			logger->Fatal("Unable to get the tile nr.%d [error=%d].", i, err);
			return PLATFORM_INIT_FAILED;
		}

		logger->Info("Loading tile %d of family %s and model %s...", i,
			hn_to_str_unit_family(tile_info.unit_family),
			hn_to_str_unit_model(tile_info.unit_model));

		MangoTile mt(i,
			(MangoTile::MangoUnitFamily_t)(tile_info.unit_family),
			(MangoTile::MangoUnitModel_t)(tile_info.unit_model));
		sys.AddAccelerator(mt);
		mt.SetPrefix(sys.GetPath());

		// For each tile, we register how many PE how may cores the accelerator has, this
		// will simplify the tracking of ResourceAccounter resources
		for (int i=0; i < MangoTile::GetCoreNr(mt.GetFamily(), mt.GetModel()); i++) {
			pd_t::ProcessingElement pe(i , 0, 100, pd_t::PartitionType_t::MDEV);
			pe.SetPrefix(mt.GetPath());
			mt.AddProcessingElement(pe);
			ra.RegisterResource(pe.GetPath(), "", 100);
		}

#ifdef CONFIG_BBQUE_WM
		// Register only one processing element per tile (representing a reference
		// to the entire tile), since we do not  expect to have per-core status
		// information
		std::string acc_pe_path(mt.GetPath() + ".pe0");
		wm.Register(acc_pe_path);
		logger->Debug("InitPowerInfo: [%s] registered for monitoring",
			acc_pe_path.c_str());
#endif

		// Let now register the memories. Unfortunately, memories are not easy to be
		// retrieved, we have to iterate over all tiles and search memories
		unsigned int mem_attached = tile_info.memory_attached;
		if (mem_attached != 0) {
			// Register the memory attached if present and if not already registered
			// Fixed mem_id to tile_id, it can be only a memory controller attached to a mango tile
			// So, the mem_id equals to the tile_id, since the HN does not set IDs for memories
			ExitCode_t reg_err = RegisterMemoryBank(i, i);
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
MangoPlatformProxy::RegisterMemoryBank(int tile_id, int mem_id) noexcept {
	typedef PlatformDescription pd_t;	// Just for convenience
	ResourceAccounter &ra(ResourceAccounter::GetInstance());

	logger->Debug("Registering tile %d, memory id %d", tile_id, mem_id);
	bbque_assert(mem_id > -1 && mem_id < MANGO_MAX_MEMORIES);
	if (found_memory_banks.test(mem_id)) {
		// We have already registered this memory, nothing to do
		return PLATFORM_OK;
	}

	// A bitset is used to keep track the already registered memory (we could also use the
	// ResourceAccounter methods, but this is faster)
	found_memory_banks.set(mem_id);
	uint32_t memory_size;
	int err = hn_get_memory_size(tile_id, &memory_size);
	if (HN_SUCCEEDED != err) {
		logger->Fatal("Unable to get memory information of tile nr.%d, memory id %d [error=%d].",
			      tile_id, mem_id, err);
		return PLATFORM_INIT_FAILED;
	}

	pd_t &pd = pli->getPlatformInfo();
	pd_t::System &sys = pd.GetLocalSystem();

	// In order to distinguish the system memory with the accelerator memory we start counting
	// the memory from MANGO_MEMORY_COUNT_START (10).
	// TODO: this is very ugly
	pd_t::Memory mem(MANGO_MEMORY_COUNT_START + mem_id, memory_size);
	mem.SetPrefix(sys.GetPath());
	sys.AddMemory(std::make_shared<pd_t::Memory>(mem));
	ra.RegisterResource(mem.GetPath(), "", memory_size);
	logger->Info("Registered memory id=%d size=%u", tile_id, memory_size);

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

/**
 * @brief This function gets sets of units for running the tasks in the TG
 */
static void FindUnitsSets(
		const TaskGraph &tg,
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
			logger->Error("Tile %i is of type GN but Barbeque is not compiled in GN emulation mode. This will probably lead to an allocation failure.", i);
		}
		#endif

		tiles_family[i++] = ArchTypeToMangoType(
			t.second->GetAssignedArch(), t.second->GetThreadCount());
	}

	int res = hn_find_units_sets(0, num_tiles, tiles_family, tiles, families_order, nsets);
	delete tiles_family;
	if (res != HN_SUCCEEDED)
		throw std::runtime_error("Unable to find units sets");
}

/**
 * @brief This function gets a set of memories for the buffers in the TG
 */
static void FindAndAllocateMemory(
		const TaskGraph &tg,
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
		int res = hn_find_memory(tile, mem_buffers_size[i], &mem_buffers_tiles[i], &mem_buffers_addr[i]);
		if (res != HN_SUCCEEDED) {
			delete mem_buffers_size;
			throw std::runtime_error("Unable to find memory");
		}

		res = hn_allocate_memory(mem_buffers_tiles[i], mem_buffers_addr[i], mem_buffers_size[i]);
		if (res != HN_SUCCEEDED) {
			delete mem_buffers_size;
			throw std::runtime_error("Unable to allocate memory");
		}
	}

	delete mem_buffers_size;
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

	Partition part(partition_id);
	// FIXME UPV -> POLIMI do it in a more efficient way if required
	//       We have to map the task to a tile according to its family type
	for (int j=0; j < tasks_size; j++) {
		uint32_t family = ArchTypeToMangoType(
			it_task->second->GetAssignedArch(), it_task->second->GetThreadCount());

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
				mem_buffers_tiles[buff_size + j], mem_buffers_addr[buff_size + j]);
		it_task++;
	}

	delete tile_mapped;
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
  for ( auto buffer : tg.Buffers()) {
		uint32_t memory_bank = partition.GetMemoryBank(buffer.second);
		uint32_t phy_addr    = partition.GetBufferAddress(buffer.second);
		logger->Debug("Buffer %d is located at memory id %d [address=0x%x]",
				buffer.second->Id(), memory_bank, phy_addr);
		buffer.second->SetMemoryBank(memory_bank);
		buffer.second->SetPhysicalAddress(phy_addr);

		if (hn_allocate_memory(memory_bank, phy_addr, buffer.second->Size()) != HN_SUCCEEDED) {
		  return SK_GENERIC_ERROR;
		}
	}

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

		if (hn_allocate_memory(mem_tile, phy_addr, ksize + ssize) != HN_SUCCEEDED) {
		  return SK_GENERIC_ERROR;
		}
	}

	return SK_OK;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::Skim(
		const TaskGraph &tg,
		std::list<Partition>&part_list) noexcept {
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
	uint32_t *mem_buffers_size   = new uint32_t[buff_size];
	uint32_t num_sets            = 0;

	bbque_assert(part_list.empty());

	// Let's get the required memory size per kernel
	logger->Debug("Request summary:");
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
		// Find resources for TG
		// It may generate exception if the architecture is not supported
		// (in that case we should not be here ;-))
		logger->Debug("Finding resources for TG");

		/*
		* Let's try to find the partitions that satisfy the TG.
		* First, find different sets of units
		* Next, find and reserve memory for every set. We cannot call hn_find_memory more than once
		* without allocate the memory returned, since it would return the same bank.
		*/
		FindUnitsSets(tg, &tiles, &families_order, &num_sets);

		// TODO: Implement the management of the multiple sets returned
		num_sets = 1;

		mem_buffers_tiles = new uint32_t*[num_sets];
		mem_buffers_addr  = new uint32_t*[num_sets];
		memset(mem_buffers_tiles, 0, num_sets*sizeof(uint32_t *));
		memset(mem_buffers_addr, 0, num_sets*sizeof(uint32_t *));

		for (unsigned int i = 0; i < num_sets; i++) {
			// let's find and allocate memory close the tiles obtained for the set
			mem_buffers_tiles[i] = new uint32_t[num_mem_buffers];
			mem_buffers_addr[i]  = new uint32_t[num_mem_buffers];
			FindAndAllocateMemory(tg, tiles[i], mem_buffers_tiles[i], mem_buffers_addr[i]);
		}
	}
	catch (const std::runtime_error &err) {
		logger->Error("MangoPartitionSkimmer: %s", err.what());
		res = SK_NO_PARTITION;
	}

	if (res == SK_OK) {
		logger->Debug("Found %d partitions", num_sets);

		// No feasible partition found
		if ( num_sets == 0 ) {
			res = SK_NO_PARTITION;
		} else {
			// Fill the partition vector with the partitions returned by HN library
			for (unsigned int i=0; i < num_sets; i++) {
				Partition part = GetPartition(tg, tiles[i], families_order[i],
						      mem_buffers_tiles[i], mem_buffers_addr[i], i);
				part_list.push_back(part);
			}
		}
	}

	// let's deallocate memory created in the hn_find_units_sets hnlib function
	if (tiles != NULL) {
		for (unsigned int i = 0; i < num_sets; i++)
			free(tiles[i]);
		free(tiles);
	}

	if (families_order != NULL) {
		for (unsigned int i = 0; i < num_sets; i++)
			free(families_order[i]);
		free(families_order);
	}

	// let's deallocate memory created in this method
	if (mem_buffers_tiles != NULL) {
		for (unsigned int i = 0; i < num_sets; i++)
			if (mem_buffers_tiles[i] != NULL) {
				// TRICK we allocated memory banks when finding them in
				// order to get different banks (see try above), so now
				// it is the right moment to release them.
				// Next, When setting the partition it will be allocated again
				for (unsigned int j = 0; j < num_mem_buffers; j++)
					hn_release_memory(
							mem_buffers_tiles[i][j],
							mem_buffers_addr[i][j],
							mem_buffers_size[j]);
				delete mem_buffers_tiles[i];
			}
			delete mem_buffers_tiles;
	}

	if (mem_buffers_addr != NULL) {
		for (unsigned int i = 0; i < num_sets; i++)
			if (mem_buffers_addr[i] != NULL)
				delete mem_buffers_addr[i];
			delete mem_buffers_addr;
	}

	delete mem_buffers_size;

	return res;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::SetPartition(
		TaskGraph &tg,
		const Partition &partition) noexcept {
	UNUSED(tg);

	// We set the mapping of buffers-addresses based on the selected partition
	ExitCode_t err = SetAddresses(tg, partition);

	for ( auto task : tg.Tasks()) {
		task.second->SetMappedProcessor( partition.GetUnit(task.second) );
	}

	// Now, we have to ask for the location in TileReg of events
	// TODO: Manage the UNIZG case
	// TODO: what to do in case of failure?
	// TODO: Policy for tile selection
	for ( auto event : tg.Events()) {
		uint32_t phy_addr;

		int err = hn_get_synch_id (&phy_addr, 0, HN_READRESET_INCRWRITE_REG_TYPE);
		if (err != HN_SUCCEEDED) {
			logger->Error("Cannot find sync register for event %d", event.second->Id());

			// TODO we should deallocate the other assigned events?
			return SK_GENERIC_ERROR;
	  	}

		logger->Debug("Event %d assigned to ID 0x%x", event.second->Id(), phy_addr);
		event.second->SetPhysicalAddress(phy_addr);
	}

	// allocate units
	unsigned int num_tiles = tg.TaskCount();
	uint32_t *units = new uint32_t[num_tiles];
	int i = 0;
	for ( auto task : tg.Tasks()) {
		auto arch = task.second->GetAssignedArch();
		units[i]  = partition.GetUnit(task.second);
		logger->Debug("Task %d reserved a unit at tile %u for kernel %s",
				task.second->Id(), units[i], GetStringFromArchType(arch));
		i++;
	}

	if (hn_reserve_units_set(num_tiles, units) != HN_SUCCEEDED) {
		err = SK_GENERIC_ERROR;
	}
	delete units;

	return err;
}

MangoPlatformProxy::MangoPartitionSkimmer::ExitCode_t
MangoPlatformProxy::MangoPartitionSkimmer::UnsetPartition(
		const TaskGraph &tg,
		const Partition &partition) noexcept {
	UNUSED(tg);

	uint32_t part_id = partition.GetId();
	ExitCode_t ret = SK_OK;
	logger->Debug("Deallocating partition [id=%d]...", part_id);

	for ( const auto &event : tg.Events()) {
		bbque_assert(event.second);
		uint32_t phy_addr = event.second->PhysicalAddress();
		logger->Debug("Releasing event %d (ID 0x%x)",
			event.second->Id(), phy_addr);
		int err = hn_release_synch_id (phy_addr);

		if(err != HN_SUCCEEDED) {
			logger->Warn("Unable to release event %d (ID 0x%x)", event.second->Id(), phy_addr);
		}
	}

	// release memory
	for ( auto buffer : tg.Buffers()) {
		uint32_t memory_bank = partition.GetMemoryBank(buffer.second);
		uint32_t phy_addr    = partition.GetBufferAddress(buffer.second);
		uint32_t size        = buffer.second->Size();;
		logger->Debug("Buffer %d is released at bank %d [address=0x%x]",
				buffer.second->Id(), memory_bank, phy_addr);
		buffer.second->SetMemoryBank(memory_bank);
		buffer.second->SetPhysicalAddress(phy_addr);

		if (hn_release_memory(memory_bank, phy_addr, size) != HN_SUCCEEDED) {
			ret = SK_GENERIC_ERROR;
		}
	}

	for ( auto task : tg.Tasks()) {
		auto arch = task.second->GetAssignedArch();
		uint32_t phy_addr    = partition.GetKernelAddress(task.second);
		uint32_t mem_tile    = partition.GetKernelBank(task.second);
		auto     ksize       = task.second->Targets()[arch]->BinarySize();
		auto     ssize       = task.second->Targets()[arch]->StackSize();
		logger->Debug("Task %d released space for kernel %s [bank=%d, address=0x%x size=%lu]",
				task.second->Id(), GetStringFromArchType(arch), mem_tile, phy_addr, ksize + ssize);
		task.second->Targets()[arch]->SetMemoryBank(mem_tile);
		task.second->Targets()[arch]->SetAddress(phy_addr);

		if (hn_release_memory(mem_tile, phy_addr, ksize + ssize) != HN_SUCCEEDED) {
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
		logger->Debug("Task %d released tile %u for kernel %s",
				task.second->Id(), units[i], GetStringFromArchType(arch));
		i++;
	}

	if (hn_release_units_set(num_tiles, units) != HN_SUCCEEDED) {
		ret = SK_GENERIC_ERROR;
	}
	delete units;

	return ret;
}

MangoPlatformProxy::MangoPartitionSkimmer::MangoPartitionSkimmer(): PartitionSkimmer(SKT_MANGO_HN)
{
	logger = bu::Logger::GetLogger(MANGO_PP_NAMESPACE ".skm");
	bbque_assert(logger);
}

}	// namespace pp
}	// namespace bbque
