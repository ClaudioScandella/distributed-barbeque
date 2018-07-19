#ifndef BBQUE_MANGO_PLATFORM_PROXY_H
#define BBQUE_MANGO_PLATFORM_PROXY_H

#include "bbque/config.h"
#include "bbque/platform_proxy.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/pp/mango_platform_description.h"
#include "tg/task_graph.h"
#include "bbque/resource_partition_validator.h"

#define MANGO_PP_NAMESPACE "bq.pp.mango"

// UPV -> POLIMI there could be as much memories as tiles in MANGO
#define MANGO_MAX_MEMORIES 256

namespace bbque {
namespace pp {



class MangoPlatformProxy : public PlatformProxy
{
public:

	static MangoPlatformProxy * GetInstance();

	virtual ~MangoPlatformProxy();

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

	/**
	 * @brief Platform specific resource claiming interface.
	 */
	ExitCode_t LoadPartitions(AppPtr_t papp) noexcept;
	

private:
//-------------------- CONSTS

//-------------------- ATTRIBUTES

	bool refreshMode;

	uint32_t num_tiles;
	uint32_t num_tiles_x;
	uint32_t num_tiles_y;
	uint32_t num_vns;

	uint32_t alloc_nr_req_cores;
	uint32_t alloc_nr_req_buffers;

	std::bitset<MANGO_MAX_MEMORIES> found_memory_banks;

	// the next keeps track of the memory tiles & addresses where peakos has been uploaded
	std::map<uint32_t, uint32_t> allocated_resources_peakos;

//-------------------- METHODS

	MangoPlatformProxy();

	ExitCode_t BootTiles() noexcept;
	ExitCode_t BootTiles_PEAK(int tile) noexcept;
	ExitCode_t RegisterTiles() noexcept;
	ExitCode_t RegisterMemoryBank (int tile_id, int mem_id) noexcept;


	class MangoPartitionSkimmer : public PartitionSkimmer {

	public:
		MangoPartitionSkimmer();
		virtual ExitCode_t Skim(const TaskGraph &tg, std::list<Partition>&) noexcept override final;
		virtual ExitCode_t SetPartition(TaskGraph &tg,
						const Partition &partition) noexcept override final;
		virtual ExitCode_t UnsetPartition(const TaskGraph &tg,
						  const Partition &partition) noexcept override final;

		ExitCode_t SetAddresses(const TaskGraph &tg, const Partition &partition) noexcept;
	private:
		std::unique_ptr<bu::Logger> logger;

	};

};

}   // namespace pp
}   // namespace bbque

#endif // MANGO_PLATFORM_PROXY_H
