#ifndef BBQUE_PLATFORM_DESCRIPTION_H_
#define BBQUE_PLATFORM_DESCRIPTION_H_

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef UINT64_MAX
#define BBQUE_PP_ARCH_SUPPORTS_INT64 1
#else
#define BBQUE_PP_ARCH_SUPPORTS_INT64 0
#endif

namespace bbque {
namespace pp {

/**
 * @class PlatformDescription
 *
 * @brief A PlatformDescription object includes the description of the
 * underlying platform, provided through the systems.xml file.
 *
 * @warning Non thread safe.
 */
class PlatformDescription {

public:
	typedef enum PartitionType {
	        HOST,
	        MDEV,
	        SHARED
	} PartitionType_t;


	class Resource {
	public:
		Resource() {}

		Resource(uint16_t id):
			id(id)
		{}

		inline uint16_t GetId() const {
			return this->id;
		}

		inline void SetId(uint16_t id) {
			this->id = id;
		}
	protected:
		uint16_t id = 0;

	};

	class ProcessingElement : public Resource {
	public:

		ProcessingElement() = default;

		ProcessingElement(
		        uint16_t id,
		        uint16_t core_id,
		        uint8_t share,
		        PartitionType_t ptype)
			: Resource(id), core_id(core_id), share(share), ptype(ptype)
		{}

		inline uint16_t GetCoreId() const {
			return this->core_id;
		}

		inline void SetCoreId(uint16_t core_id) {
			this->core_id = core_id;
		}

		inline uint32_t GetQuantity() const {
			return this->quantity;
		}

		inline void SetQuantity(uint32_t quantity) {
			this->quantity = quantity;
		}


		inline uint8_t GetShare() const {
			return this->share;
		}

		inline void SetShare(uint8_t share) {
			this->share = share;
		}

		inline PartitionType_t GetPartitionType() const {
			return this->ptype;
		}

		inline void SetPartitionType(PartitionType_t ptype) {
			this->ptype = ptype;
		}

	private:
		uint16_t core_id;
		uint32_t quantity;
		uint8_t share;
		PartitionType_t ptype;

	};

	class Memory : public Resource {

	public:

		Memory() = default;

#if BBQUE_PP_ARCH_SUPPORTS_INT64
		Memory(uint16_t id, uint64_t quantity)
			: Resource(id), quantity(quantity)
		{}
#else
		Memory(uint16_t id, uint32_t quantity_hi, uint32_t quantity_lo)
			: Resource(id), quantity_hi(quantity_hi), quantity_lo(quantity_lo)
		{}
#endif


#if BBQUE_PP_ARCH_SUPPORTS_INT64
		inline uint64_t GetQuantity() const {
			return this->quantity;
		}

		inline void SetQuantity(uint64_t quantity) {
			this->quantity = quantity;
		}
#else
		inline uint32_t GetQuantityLO() const {
			return this->quantity_lo;
		}

		inline void SetQuantityLO(uint32_t quantity) {
			this->quantity_lo = quantity;
		}
		inline uint32_t GetQuantityHI() const {
			return this->quantity_hi;
		}

		inline void SetQuantityHI(uint32_t quantity) {
			this->quantity_hi = quantity;
		}

#endif

	private:
#if BBQUE_PP_ARCH_SUPPORTS_INT64
		uint64_t quantity;
#else
		uint32_t quantity_lo;
		uint32_t quantity_hi;
#endif
	};

	typedef std::shared_ptr<Memory> MemoryPtr_t;

	class MulticoreProcessor : public Resource {
	public:
		inline const std::string & GetArchitecture() const {
			return this->architecture;
		}

		inline void SetArchitecture(const std::string & arch) {
			this->architecture = arch;
		}

		inline const std::vector<ProcessingElement> & GetProcessingElementsAll() const {
			return this->pes;
		}

		inline std::vector<ProcessingElement> & GetProcessingElementsAll() {
			return this->pes;
		}


		inline void AddProcessingElement(const ProcessingElement & pe) {
			this->pes.push_back(pe);
		}

	private:
		std::string architecture;
		std::vector<ProcessingElement> pes;
	};

	class CPU : public MulticoreProcessor {
	public:
		inline uint16_t GetSocketId() const {
			return this->socket_id;
		}

		inline void SetSocketId(uint16_t socket_id) {
			this->socket_id = socket_id;
		}

		inline std::shared_ptr<Memory> GetMemory() const {
			return this->memory;
		}

		inline void SetMemory(std::shared_ptr<Memory> memory) {
			this->memory = memory;
		}

	private:
		uint16_t socket_id;
		std::shared_ptr<Memory> memory;

	};


	class System :  public Resource {
	public:
		inline bool IsLocal() const {
			return this->local;
		}

		inline const std::string & GetHostname() const {
			return this->hostname;
		}

		inline const std::string & GetNetAddress() const {
			return this->net_address;
		}

		inline void SetLocal(bool local) {
			this->local = local;
		}

		inline void SetHostname(const std::string & hostname) {
			this->hostname = hostname;
		}

		inline void SetNetAddress(const std::string & net_address) {
			this->net_address = net_address;
		}

		inline const std::vector<CPU> & GetCPUsAll() const {
			return this->cpus;
		}

		inline std::vector<CPU> & GetCPUsAll() {
			return this->cpus;
		}

		inline void AddCPU(const CPU & cpu) {
			this->cpus.push_back(cpu);
		}

		inline const std::vector<MulticoreProcessor> & GetGPUsAll() const {
			return this->gpus;
		}

		inline std::vector<MulticoreProcessor> & GetGPUsAll() {
			return this->gpus;
		}

		inline void AddGPU(const MulticoreProcessor & gpu) {
			this->gpus.push_back(gpu);
		}

		inline const std::vector<MulticoreProcessor> & GetAcceleratorsAll() const {
			return this->accelerators;
		}

		inline std::vector<MulticoreProcessor> & GetAcceleratorsAll() {
			return this->accelerators;
		}

		inline void AddAccelerator(const MulticoreProcessor & accelerator) {
			this->accelerators.push_back(accelerator);
		}

		inline const std::vector<MemoryPtr_t> & GetMemories() const {
			return this->memories;
		}

		inline std::vector<MemoryPtr_t> & GetMemoryAll() {
			return this->memories;
		}

		MemoryPtr_t GetMemoryById(short id) const noexcept {
			for (auto x : memories) {
				if ( x->GetId() == id )
					return x;
			}

			return nullptr;
		}

		inline void AddMemory(const MemoryPtr_t & memory) {
			this->memories.push_back(memory);
		}

	private:

		bool local;
		std::string hostname;
		std::string net_address;

		std::vector <CPU> cpus;
		std::vector <MulticoreProcessor> gpus;
		std::vector <MulticoreProcessor> accelerators;
		std::vector <MemoryPtr_t> memories;
	};

	inline const System & GetLocalSystem() const {
		static std::shared_ptr<System> sys(nullptr);
		if (!sys) {
			for (auto s : this->GetSystemsAll()) {
				if (s.IsLocal()) {
					sys = std::make_shared<System>(s);
				}
			}
			assert(sys);
		}
		return *sys;
	}

	inline const std::vector<System> & GetSystemsAll() const {
		return this->systems;
	}

	inline std::vector<System> & GetSystemsAll() {
		return this->systems;
	}

	inline void AddSystem(const System & sys) {
		this->systems.push_back(sys);
	}

	inline const System & GetSystem(int id) const {
		return this->systems.at(id);
	}

private:
	std::vector <System> systems;

}; // class PlatformDescription

}   // namespace pp
}   // namespace bbque

#endif // BBQUE_PLATFORM_DESCRIPTION_H_
