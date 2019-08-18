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

#include "bbque/resource_partition_validator.h"
#include "bbque/utils/assert.h"

#define MODULE_NAMESPACE   "bq.rmv"
#define MODULE_CONFIG      "ResourcePartitionValidator"


namespace bbque {


ResourcePartitionValidator & ResourcePartitionValidator::GetInstance() {
	static ResourcePartitionValidator instance;
	return instance;
}

ResourcePartitionValidator::ResourcePartitionValidator():
	logger(bbque::utils::Logger::GetLogger(MODULE_NAMESPACE)) {
}

void ResourcePartitionValidator::RegisterSkimmer(
		PartitionSkimmerPtr_t skimmer, int priority) noexcept {
	std::lock_guard<std::mutex> curr_lock(skimmers_lock);
	logger->Notice("Registered skimmer with priority=%i", priority);
	skimmers.insert(
		std::pair<int,PartitionSkimmerPtr_t> (priority, skimmer));
}


ResourcePartitionValidator::ExitCode_t
ResourcePartitionValidator::LoadPartitions(
		const TaskGraph &tg,
		std::list<Partition> &partitions,
		uint32_t hw_cluster_id) {

	logger->Info("Initial partitions nr. %d", partitions.size());
	PartitionSkimmer::SkimmerType_t skimmer_type = PartitionSkimmer::SkimmerType_t::SKT_NONE;

	skimmers_lock.lock();
	if ( skimmers.empty() ) {
		skimmers_lock.unlock();
		logger->Warn("No skimmers registered, no action performed.");
		return PMV_OK;
	}

	// I get the skimmers in the reverse order, in order to execute the one with highest
	// priority
	for (auto s = skimmers.rbegin(); s != skimmers.rend(); ++s) {
		int priority = s->first;
		PartitionSkimmerPtr_t skimmer = s->second; 
		skimmer_type = skimmer->GetType(); 
		logger->Debug("Executing skimmer [type=%d] [priority=%d]",
			(int)skimmer_type, priority);

		PartitionSkimmer::ExitCode_t ret = skimmer->Skim(tg, partitions, hw_cluster_id);
		if (ret != PartitionSkimmer::SK_OK) {
			logger->Error("Skimmer %d [priority=%d] FAILED [err=%d]", 
				(int)skimmer_type, priority, ret);
			this->failed_skimmer = skimmer_type;
			skimmers_lock.unlock();
			return PMV_SKIMMER_FAIL;
		}

		if ( partitions.empty() ) {
			break;
		}
	}

	skimmers_lock.unlock();

	this->failed_skimmer = PartitionSkimmer::SKT_NONE;
	if ( partitions.empty() ) {
		logger->Warn("Skimmer %d: no feasible partitions", (int)skimmer_type);
		return PMV_NO_PARTITION;  // No feasible solution found
	}

	return PMV_OK;
}

ResourcePartitionValidator::ExitCode_t
ResourcePartitionValidator::PropagatePartition(
		TaskGraph &tg, const Partition &partition) const noexcept {

	logger->Info("Propagating partition id=%d", partition.GetId());
	// We have to ensure that no skimmer failed for any reasons before this call.
	bbque_assert(failed_skimmer == PartitionSkimmer::SKT_NONE);
	std::lock_guard<std::mutex> curr_lock(skimmers_lock);

	// Just propagate the selected partition to all registered partition skimmer. They should
	// not fail for any reason, since they have already skimmed the partitions.
	for (auto s = skimmers.rbegin(); s != skimmers.rend(); ++s) {
		PartitionSkimmerPtr_t skimmer = s->second; 
		PartitionSkimmer::ExitCode_t err = skimmer->SetPartition(tg, partition);
		if ( PartitionSkimmer::SK_OK != err ) {
			logger->Error("Skimmer failed to set partition [type=%d] [priority=%d] "
				      "[err=%d]", skimmer->GetType(), s->first, err);
			return PMV_GENERIC_ERROR;
		}
	}
	return PMV_OK;
}

ResourcePartitionValidator::ExitCode_t
ResourcePartitionValidator::RemovePartition(
		const TaskGraph &tg,
		const Partition &partition) const noexcept {

	logger->Info("Removing partition id=%d", partition.GetId());
	// We have to ensure that no skimmer failed for any reasons before this call.
//	bbque_assert(failed_skimmer == PartitionSkimmer::SKT_NONE);
	if (failed_skimmer != PartitionSkimmer::SKT_NONE) {
		logger->Error("Skimmer [%d] failure reported", failed_skimmer);
	}

	std::lock_guard<std::mutex> curr_lock(skimmers_lock);

	// Just propagate the selected partition to all registered partition skimmer. They should
	// not fail for any reason, since they have already skimmed the partitions.
	for (auto s = skimmers.rbegin(); s != skimmers.rend(); ++s) {
		PartitionSkimmerPtr_t skimmer = s->second;
		PartitionSkimmer::ExitCode_t err = skimmer->UnsetPartition(tg, partition);
		if ( PartitionSkimmer::SK_OK != err ) {
			logger->Error("Skimmer failed to unset partition [type=%d] [priority=%d] "
				      "[err=%d]", skimmer->GetType(), s->first, err);
			return PMV_GENERIC_ERROR;
		}
	}
	return PMV_OK;
}

} // namespace bbque

