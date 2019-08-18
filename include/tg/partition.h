/*
 * Copyright (C) 2017  Politecnico di Milano
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


#ifndef BBQUE_TG_PARTITION_H
#define BBQUE_TG_PARTITION_H

#include <memory>
#include <map>
#include <cstdint>

#include "bbque/pp/platform_description.h"
#include "bbque/utils/assert.h"
#include "tg/task_graph.h"

namespace bbque {

/**
 * \brief This class represents a partition of available resources that can be assigned to an entire
 *	  TaskGraph. This type of object is generally retrieved from the platform proxy through the
 *	  PlatformProxy
 */
class Partition {

public:

	/**
	 * \brief The constructor of a new partition (the id must be unique for each TaskGraph)
	 */
	Partition(uint32_t id, uint32_t c_id=0) :
		id(id), cluster_id(c_id) { }

	virtual ~Partition() {}

	/**
	 * \brief Getter method for the unique identifier of the partition (unique in each TG)
	 */
	inline uint32_t GetId() const noexcept {
		return this->id;
	}

	/**
	 * \brief Getter method for the unique identifier of the HW cluster
	 * including the partition
	 */
	inline uint32_t GetClusterId() const noexcept {
		return this->cluster_id;
	}

	/**
	 * \brief It returns a score [0;100] representing the memory manager score for the current
	 * 	  partition. A score of 0 means that the partition is not feasible and therefore it
	 *	  should not selected.
	 * \note If the memory manager did not fill this field, the return is undefined
	 */
	inline int_fast8_t GetMMScore() const noexcept {
		return this->mm_score;
	}

	/**
	 * \brief Set a score [0;100] representing the memory manager score for the current
	 * 	  partition. A score of 0 means that the partition is not feasible and therefore it
	 *	  should not selected.
	 */
	inline void SetMMScore(int_fast8_t score) noexcept {
		bbque_assert(score >= 0 && score <= 100);
		this->mm_score = score;
	}

	/**
	 * \brief It returns a score [0;100] representing the power manager score for the current
	 * 	  partition. A score of 0 means that the partition is not feasible and therefore it
	 *	  should not selected.
	 * \note If the power manager did not fill this field, the return is undefined
	 */
	inline int_fast8_t GetPMScore() const noexcept {
		return this->pm_score;
	}

	/**
	 * \brief Set a score [0;100] representing the power manager score for the current
	 * 	  partition. A score of 0 means that the partition is not feasible and therefore it
	 *	  should not selected.
	 */
	inline void SetPMScore(int_fast8_t score) noexcept {
		bbque_assert(score >= 0 && score <= 100);
		this->pm_score = score;
	}

	/**
	 * \brief Map a task in the TaskGraph to a resource.
	 * \param unit     The id of the processor unit
	 * \param mem_bank The memory bank assigned to the kernel image
	 * \param addr     The memory address assigned to the kernel image
	 */
	inline void MapTask(TaskPtr_t task, int unit, uint32_t mem_bank, uint32_t addr) noexcept {
		this->tasks_map.emplace(task->Id(), unit);
		this->kernels_addr_map.emplace(task->Id(), addr);
		this->kernels_bank_map.emplace(task->Id(), mem_bank);
	}

	/**
	 * \brief Map a buffer in the TaskGraph to a resource.
	 * \param unit     The id of the memory bank
	 * \param addr     The memory address assigned to the kernel image
	 */
	inline void MapBuffer(BufferPtr_t buff, int mem_bank, uint32_t addr) noexcept {
		this->buffers_map.emplace(buff->Id(), mem_bank);
		this->buffers_addr_map.emplace(buff->Id(), addr);
	}

	/**
	 * @brief Get the multicore processor assigned to the task
	 * @except std::out_of_range if the task is not present in the mapping
	 */
	int GetUnit(TaskPtr_t task) const;

	/**
	 * @brief Get the id of the memory bank as an identifier number associated by the HN library
	 * @except std::out_of_range if the buffer is not present in the mapping
	 */
	int GetMemoryBank(BufferPtr_t buff) const;

	/**
	 * \brief Get the address of a specific buffer
	 * \param buff the buffer
	 */
	uint32_t GetBufferAddress(BufferPtr_t buff) const;

	/**
	 * \brief Get the address of a kernel image
	 * \param task the task to which the kernel image is associated
	 */
	uint32_t GetKernelAddress(TaskPtr_t task) const;

	/**
	 * \brief Get the bank of a kernel image
	 * \param task the task to which the kernel image is associated
	 */
	uint32_t GetKernelBank(TaskPtr_t task) const;

	/**
	 * \brief Get the TaskGraph
	 */
	inline std::shared_ptr<TaskGraph> GetTask() noexcept {
		return this->tg;
	}

	/**
	 * \brief Set the TaskGraph related
	 * \param the task graph
	 */
	inline void GetTask(std::shared_ptr<TaskGraph> tg) noexcept {
		this->tg = tg;
	}


/* ******************* ITERATORS ******************* */

	typedef std::map<int,int>::const_iterator map_citerator_t;

	inline map_citerator_t Tasks_cbegin() const noexcept {
		return this->tasks_map.cbegin();
	}

	inline map_citerator_t Tasks_cend() const noexcept {
		return this->tasks_map.cend();
	}

	inline map_citerator_t Buffers_cbegin() const noexcept {
		return this->buffers_map.cbegin();
	}

	inline map_citerator_t Buffers_cend() const noexcept {
		return this->buffers_map.cend();
	}

	inline map_citerator_t KernelsAddr_cbegin() const noexcept {
		return this->kernels_addr_map.cbegin();
	}

	inline map_citerator_t KernelsAddr_cend() const noexcept {
		return this->kernels_addr_map.cend();
	}

	inline map_citerator_t KernelsBank_cbegin() const noexcept {
		return this->kernels_bank_map.cbegin();
	}

	inline map_citerator_t KernelsBank_cend() const noexcept {
		return this->kernels_bank_map.cend();
	}

	inline map_citerator_t BuffersAddr_cbegin() const noexcept {
		return this->buffers_addr_map.cbegin();
	}

	inline map_citerator_t BuffersAddr_cend() const noexcept {
		return this->buffers_addr_map.cend();
	}

private:
	const uint32_t id;	/** The internal identifier returned by HN library */
	uint32_t cluster_id;	/** The HW cluster (id) to which the partition belongs */
	int_fast8_t mm_score;	/** The score index [0;100] provided by the MemoryManager */
	int_fast8_t pm_score;	/** The score index [0;100] provided by the PowerManager */

	std::shared_ptr<TaskGraph> tg;	/** The pointer to the associated TaskGraph */
	
	std::map<int, int> tasks_map;
	std::map<int, int> buffers_map;
	std::map<int, int> kernels_addr_map;
	std::map<int, int> kernels_bank_map;
	std::map<int, int> buffers_addr_map;

};

} // namespace bbque

#endif // BBQUE_TG_PARTITION_H
