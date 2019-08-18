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

#ifndef BBQUE_RESOURCE_MAPPING_VALIDATOR_H_
#define BBQUE_RESOURCE_MAPPING_VALIDATOR_H_

#include <functional>
#include <map>
#include <memory>
#include <mutex>


#include "bbque/utils/logging/logger.h"
#include "tg/task_graph.h"
#include "tg/partition.h"

namespace bu = bbque::utils;

namespace bbque {

/**
 * @class PartitionSkimmer
 *
 * @brief The interface for declaration of a PartitionSkimmer object.
 */
class PartitionSkimmer {

public:

	typedef enum SkimmerType_t {
		SKT_NONE = 0,
		SKT_MANGO_HN,
		SKT_MANGO_MEMORY_MANAGER,
		SKT_MANGO_POWER_MANAGER
	} SkimmerType_t;

	typedef enum ExitCode_t {
		SK_OK = 0,
		SK_GENERIC_ERROR,
		SK_NO_PARTITION
	} ExitCode_t;

	virtual ~PartitionSkimmer() { }

	/**
	 * @brief This abstract method should remove a subset of partitions (eventually all) from
	 * 	  the list passed. The method can also eventually add partitions.
	 */
	virtual ExitCode_t Skim(const TaskGraph &tg, std::list<Partition>& partitions, uint32_t hw_cluster_id) noexcept = 0;

	/**
	 * @brief This abstract method is called when the policy finally decides which partition is
	 *	  the best to be selected.
	 */
	virtual ExitCode_t SetPartition(TaskGraph &tg, const Partition &partition) noexcept = 0;

	/**
	 * @brief This abstract method is called when a partition is released
	 */
	virtual ExitCode_t UnsetPartition(const TaskGraph &tg, const Partition &partition) noexcept = 0;

	/**
	 * @brief The constructor that initializes the type of the skimmer.
	 */
	PartitionSkimmer(SkimmerType_t type) : type(type) {}

	/**
	 * @brief Return the type of the skimmer
	 */
	SkimmerType_t GetType() { return type; }

private:

	SkimmerType_t type;
};

typedef std::shared_ptr<PartitionSkimmer> PartitionSkimmerPtr_t;

/**
 * @class ResourcePartitionValidator
 *
 * @brief Provides the list of available partitions, validating them across the registered callback
 *	  function.
 */
class ResourcePartitionValidator {

public:

	typedef enum ExitCode_t {
		PMV_OK = 0,
		PMV_GENERIC_ERROR,
		PMV_NO_PARTITION,
		PMV_SKIMMER_FAIL
	} ExitCode_t;


	/**
	 * @brief Return the singleton instance
	 */
	static ResourcePartitionValidator & GetInstance();


	virtual ~ResourcePartitionValidator()  {};

	/**
	 * @brief Load the feasible partitions according to registered callbacks. 
	 *
	 */
	ExitCode_t LoadPartitions(
			const TaskGraph &tg, std::list<Partition> &partitions,
			uint32_t hw_cluster_id);


	/**
	 * @brief Register a new skimmer with the given priority (high numbers mean high priority)
	 * @note  Thread-safe
	 */
	void RegisterSkimmer(PartitionSkimmerPtr_t skimmer, int priority) noexcept;

	/**
	 * @brief If LoadPartitions failed with `PMV_NO_PARTITION` or `PMV_SKIMMER_FAIL`, this
	 * 	  method returns the skimmer that have not found a list of feasible partitions or
	 *	  fails to skim it. 
	 * @note  If the LoadPartitions is never called or returned with other
	 *	  value the return of this method is undefined.
	 */
	inline PartitionSkimmer::SkimmerType_t GetLastFailed() const noexcept { 
		return failed_skimmer;
	}

	/**
	 * The policy should call this method when it finally decides the final selected partitions.
	 * The partition is then propagated to all registered PartitionSkimmer with the SetPartition
	 * method.
	 */
	ExitCode_t PropagatePartition(TaskGraph &tg, const Partition &partition) const noexcept;

	/**
	 * Propagates the partition removal request to all skimmers
	 */
	ExitCode_t RemovePartition(const TaskGraph &tg, const Partition &partition) const noexcept;


private:

	/* ******* ATTRIBUTES ******* */
	std::unique_ptr<bu::Logger> logger;

	mutable std::mutex skimmers_lock;
	std::multimap<int, PartitionSkimmerPtr_t> skimmers;

	PartitionSkimmer::SkimmerType_t failed_skimmer;

	/* ******* METHODS ******* */
	ResourcePartitionValidator();

};

}

#endif // BBQUE_RESOURCE_MAPPING_VALIDATOR_H_

