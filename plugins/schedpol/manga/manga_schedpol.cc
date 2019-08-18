/*
 * Copyright (C) 2016  Politecnico di Milano
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

#include "manga_schedpol.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "bbque/binding_manager.h"
#include "bbque/modules_factory.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/assert.h"
#include "bbque/app/working_mode.h"
#include "bbque/res/binder.h"
#include "tg/task_graph.h"

#define MODULE_CONFIG SCHEDULER_POLICY_CONFIG "." SCHEDULER_POLICY_NAME

#ifndef CONFIG_TARGET_LINUX_MANGO
#error "MangA policy must be compiled only for Linux Mango target"
#endif

namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

// :::::::::::::::::::::: Static plugin interface ::::::::::::::::::::::::::::

void * MangASchedPol::Create(PF_ObjectParams *) {
	return new MangASchedPol();
}

int32_t MangASchedPol::Destroy(void * plugin) {
	if (!plugin)
		return -1;
	delete (MangASchedPol *)plugin;
	return 0;
}

// ::::::::::::::::::::: Scheduler policy module interface :::::::::::::::::::

char const * MangASchedPol::Name() {
	return SCHEDULER_POLICY_NAME;
}

MangASchedPol::MangASchedPol():
		cm(ConfigurationManager::GetInstance()),
		ra(ResourceAccounter::GetInstance()),
		rmv(ResourcePartitionValidator::GetInstance()) {
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	assert(logger);
	if (logger)
		logger->Info("manga: Built a new dynamic object[%p]", this);
	else
		fprintf(stderr,
			FI("manga: Built new dynamic object [%p]\n"), (void *)this);
}


MangASchedPol::~MangASchedPol() {

}


SchedulerPolicyIF::ExitCode_t MangASchedPol::Init() {
	// Build a string path for the resource state view
	std::string token_path(MODULE_NAMESPACE);
	++status_view_count;
	token_path.append(std::to_string(status_view_count));
	logger->Debug("Init: Require a new resource state view [%s]",
		token_path.c_str());

	// Get a fresh resource status view
	ResourceAccounterStatusIF::ExitCode_t ra_result =
		ra.GetView(token_path, sched_status_view);
	if (ra_result != ResourceAccounterStatusIF::RA_SUCCESS) {
		logger->Fatal("Init: cannot get a resource state view");
		return SCHED_ERROR_VIEW;
	}
	logger->Debug("Init: resources state view token: %ld", sched_status_view);


	// Get the amount of processing elements from each accelerator
	if (pe_per_acc.empty()) {

		// Get the number of HN clusters (groups)
		auto groups = sys->GetResources("sys.grp");
		logger->Debug("Init: # clusters: %d", groups.size());
		for (auto & hn_cluster: groups) {
			// Cluster info
			auto cluster_id = hn_cluster->ID();
			std::string cluster_path(
				std::string("sys.grp") + std::to_string(cluster_id));

			// Nr. of accelerators
			std::string acc_path(cluster_path + std::string(".acc"));
			logger->Debug("Init: accelerators path: <%s>", acc_path.c_str());
			auto accs = sys->GetResources(acc_path);
			pe_per_acc.emplace(cluster_id, std::vector<uint32_t>(accs.size()));
			logger->Debug("Init: cluster=<%d>: # accelerators %d",
				cluster_id, accs.size());

			// Nr. of cores per accelerator
			for (br::ResourcePtr_t const & acc: accs) {
				std::string pe_binding_path(
					acc_path + std::to_string(acc->ID()) + std::string(".pe"));
				logger->Debug("Init: binding resource path: <%s>", pe_binding_path.c_str());

				pe_per_acc[cluster_id][acc->ID()] = sys->ResourceTotal(pe_binding_path) / 100 ;
				logger->Debug("Init: <%s> #proc_element=%d",
					pe_binding_path.c_str(), pe_per_acc[cluster_id][acc->ID()]);
			}
		}
	}

	// Load the application task graphs
	logger->Debug("Init: loading the applications task graphs");
	fut_tg = std::async(std::launch::async, &System::LoadTaskGraphs, sys);

	return SCHED_OK;
}


SchedulerPolicyIF::ExitCode_t
MangASchedPol::Schedule(
		System & system,
		RViewToken_t & status_view) {
	SchedulerPolicyIF::ExitCode_t result = SCHED_DONE;

	// Class providing query functions for applications and resources
	sys = &system;
	Init();
	fut_tg.get();

	for (AppPrio_t priority = 0; priority <= sys->ApplicationLowestPriority(); priority++) {
		// Checking if there are applications at this priority
		if (!sys->HasApplications(priority))
			continue;

		logger->Debug("Schedule: serving applications with priority %d", priority);
		ExitCode_t err = ServeApplicationsWithPriority(priority);
		if (err == SCHED_R_UNAVAILABLE) {
			// We have finished the resources, suspend all other
			// apps and returns gracefully
			result = SCHED_DONE;
			break;
		} else if (err != SCHED_OK) {
			logger->Error("Schedule: unexpected error: %d", err);
			result = err;
			break;
		}
	}

	// Return the new resource status view according to the new resource
	// allocation performed
	status_view = sched_status_view;
	return result;
}

SchedulerPolicyIF::ExitCode_t
MangASchedPol::ServeApplicationsWithPriority(int priority) noexcept {
	ExitCode_t err, err_relax = SCHED_OK;
	ApplicationManager & am(ApplicationManager::GetInstance());
//	do {
		ba::AppCPtr_t  papp;
		AppsUidMapIt app_iterator;
		// Get all the applications @ this priority
		papp = sys->GetFirstWithPrio(priority, app_iterator);
		for (; papp; papp = sys->GetNextWithPrio(priority, app_iterator)) {
			logger->Debug("ServeApplicationsWithPriority: [%s]: looking for resources...",
					papp->StrId());

			if (papp->Disabled()) {
				logger->Debug("ServeApplicationsWithPriority: [%s] disabled. Skip...",
					papp->StrId());
				continue;
			}

			err = ServeApp(papp);
			if (err == SCHED_SKIP_APP) {
				// In this case we have no sufficient memory to start it, the only
				// one thing to do is to ignore it
				logger->Warn("ServeApplicationsWithPriority: [%s]: missing information",
					papp->StrId());
				am.NoSchedule(papp);
				continue;
			}
			else if (err == SCHED_R_UNAVAILABLE) {
				// In this case we have no bandwidth feasibility, so we can try to
				// fairly reduce the bandwidth for non strict applications.
				logger->Warn("ServeApplicationsWithPriority: [%s]: unable to allocate required resources",
					papp->StrId());
				err_relax = RelaxRequirements(priority);
				am.NoSchedule(papp);
				continue;
			}
			else if (err != SCHED_OK) {
				// It returns  error exit code in case of error.
				return err;
			}

			logger->Info("ServeApplicationsWithPriority: [%s] successfully scheduled",
				     papp->StrId());
		}

//	} while (err == SCHED_R_UNAVAILABLE && err_relax == SCHED_OK);

	return err_relax != SCHED_OK ? err_relax : err;
}

SchedulerPolicyIF::ExitCode_t MangASchedPol::RelaxRequirements(int priority) noexcept {
	//TODO: smart policy to reduce the requirements
	UNUSED(priority);

	return SCHED_R_UNAVAILABLE;
}

SchedulerPolicyIF::ExitCode_t MangASchedPol::ServeApp(ba::AppCPtr_t papp) noexcept {

	// Try to allocate resourced for the application
	if (papp->Running()) {
		logger->Debug("ServeApp: [%s] is RUNNING -> rescheduling", papp->StrId());
		return ReassignWorkingMode(papp);
	}

	// First of all we have to decide which processor type to assign to each task
	auto err = CheckHWRequirements(papp);
	if (err != SCHED_OK) {
		logger->Warn("ServeApp: [%s] not schedulable", papp->StrId());
		return err;
	}

	uint32_t curr_cluster_id = 0;
	uint32_t nr_clusters = pe_per_acc.size();
	logger->Debug("ServeApp: [%s] HN clusters available = %d",
		papp->StrId(), nr_clusters);
	assert (nr_clusters > 0);
	if (nr_clusters <= 0) {
		logger->Fatal("ServeApp: no HW clusters available: platform not initialized?",
			papp->StrId());
		return SCHED_ERROR;
	}

	// HN clusters
	std::list<Partition> partitions;
	for (; curr_cluster_id < nr_clusters; curr_cluster_id++) {
		logger->Debug("ServeApp: [%s] looking for a partition in HN cluster %d",
			papp->StrId(), curr_cluster_id);

		// Look for resource allocation partitions
		auto rmv_err = rmv.LoadPartitions(*papp->GetTaskGraph(), partitions, curr_cluster_id);
		switch(rmv_err) {
			case ResourcePartitionValidator::PMV_OK:
				logger->Debug("ServeApp: [%s] HW partitions found: %d "
					"[HN cluster %d]",
					papp->StrId(), partitions.size(), curr_cluster_id);
				err = ScheduleApplication(papp, partitions);
				break;
			case ResourcePartitionValidator::PMV_SKIMMER_FAIL:
				logger->Warn("ServeApp: [%s] at least one skimmer failed"
					"[HN cluster %d]",
					papp->StrId(), curr_cluster_id);
			case ResourcePartitionValidator::PMV_NO_PARTITION:
				logger->Warn("ServeApp: [%s] no HW partitions available"
					"[HN cluster %d]",
					papp->StrId(), curr_cluster_id);
				err = DealWithNoPartitionFound(papp);
				break;
			default:
				// Ehi what's happened here?
				logger->Fatal("ServeApp: [%s] unexpected error while retrieving"
					" partitions", papp->StrId());
				return SCHED_ERROR;
		}

		char okstr[]="OK";
		char errstr[]="!";
		char * estr;
		err != SCHED_OK ? estr=errstr : estr=okstr;
		logger->Debug("ServeApp: [%s] error=%d [%s]", papp->StrId(), err, estr);
		if (err == SCHED_SKIP_APP || err == SCHED_OK) {
			return err;
		}
	}

	return err;
}

SchedulerPolicyIF::ExitCode_t MangASchedPol::DealWithNoPartitionFound(ba::AppCPtr_t papp) noexcept {
	UNUSED(papp);
	switch(rmv.GetLastFailed()) {
		case PartitionSkimmer::SKT_MANGO_HN:
			// In this case we can try to reduce the allocated
			// bandwidth
		case PartitionSkimmer::SKT_MANGO_MEMORY_MANAGER:
			// We have no sufficient memory to run the application
			return SCHED_R_UNAVAILABLE;
		case PartitionSkimmer::SKT_MANGO_POWER_MANAGER:	
			// Strict thermal constraints must not violated - do
			// not schedule the application
		default:
			return SCHED_SKIP_APP;
	}
}

SchedulerPolicyIF::ExitCode_t MangASchedPol::CheckHWRequirements(ba::AppCPtr_t papp) noexcept {
	// Trivial allocation policy: we select always the best one for the receipe
	// TODO: a smart one
	if (nullptr == papp->GetTaskGraph()) {
		logger->Error("CheckHWRequirements: [%s] task-graph not available", papp->StrId());
		return SCHED_SKIP_APP;
	}

	for (auto task_pair : papp->GetTaskGraph()->Tasks()) {
		auto task = task_pair.second;
		const auto requirements = papp->GetTaskRequirements(task->Id());
	
		uint_fast8_t i=0; 
		ArchType_t preferred_type;
		const auto targets = task->Targets();

		for ( auto targ : task->Targets() ) {
			logger->Debug("CheckHRequirements: [%s] task %d available [arch=%s (%d)]",
				papp->StrId(), task->Id(),
				GetStringFromArchType(targ.first), targ.first);
		}

		do {	// Select every time the best preferred available architecture
			if ( i > 0 ) {
				logger->Warn("CheckHWRequirements: [%s] architecture %s (%d) available in "
					"recipe but task %i does not support it",
					papp->StrId(),
					GetStringFromArchType(preferred_type), preferred_type, task->Id());
			}
			if ( i > requirements.NumArchPreferences() ) {
				break;
			}
			preferred_type = requirements.ArchPreference(i);
			i++;
		} while(targets.find(preferred_type) == targets.end());

		if (preferred_type == ArchType_t::NONE) {
			logger->Error("CheckHWRequirements: [%s] no architecture available for task %d",
				papp->StrId(), task->Id());
			return SCHED_R_UNAVAILABLE;
		}

		// TODO We have to select also the number of cores!

		logger->Info("CheckHWRequirements: [%s] task %d preliminary assignment "
			"[arch=%s (%d), in_bw=%d, out_bw=%d]",
			papp->StrId(),
			task->Id(), GetStringFromArchType(preferred_type), preferred_type,
			requirements.GetAssignedBandwidth().in_kbps,
			requirements.GetAssignedBandwidth().out_kbps);
		task->SetAssignedArch( preferred_type );
		task->SetAssignedBandwidth( requirements.GetAssignedBandwidth() );
	}
	return SCHED_OK;

}


SchedulerPolicyIF::ExitCode_t
MangASchedPol::ScheduleApplication(
		ba::AppCPtr_t papp, const std::list<Partition> &partitions) noexcept {
	// Order the HW resources partition
	auto ret = SortPartitions(papp, partitions);
	if (ret != SCHED_OK) {
		logger->Error("ScheduleApplication: [%s] partition ordering failed",
			papp->StrId());
		return ret;
	}

	// Select/build a working mode
	auto tg = papp->GetTaskGraph();
	for (auto & selected_partition: partitions) {
		logger->Debug("ScheduleApplication: [%s] selecting AWM for partition %d",
			papp->StrId(), selected_partition.GetId());
		auto ret = SelectWorkingMode(papp, selected_partition);
		if (ret == SCHED_OK) {
			logger->Debug("ScheduleApplication: [%s] selected partition %d",
				papp->StrId(), selected_partition.GetId());
			papp->SetPartition(std::make_shared<Partition>(selected_partition));
			auto pret = rmv.PropagatePartition(*tg, selected_partition);
			if (pret != ResourcePartitionValidator::ExitCode_t::PMV_OK) {
				logger->Debug("ScheduleApplication: partition propagation"
					" failed [error=%d]", pret);
				return SCHED_SKIP_APP;
			}
			break;
		}
	}

	// Update task-graph info
	logger->Debug("ScheduleApplication: [%s] updating task-graph mapping...",
		papp->StrId());
	papp->SetTaskGraph(tg);
	return ret;
}


SchedulerPolicyIF::ExitCode_t
MangASchedPol::SortPartitions(ba::AppCPtr_t papp, const std::list<Partition> &partitions) noexcept {
	UNUSED(papp);
	bbque_assert(partitions.size() > 0);
	logger->Debug("SortPartitions: sorting the list of partitions... (TODO)");
	// TODO: Intelligent policy. For the demo just select the first
	// partition
	logger->Warn("SortPartitions: just pick the first available partition");
	return SCHED_OK;
}


SchedulerPolicyIF::ExitCode_t
MangASchedPol::SelectWorkingMode(ba::AppCPtr_t papp, const Partition & selected_partition) noexcept {
	// Build a new working mode featuring assigned resources
	ba::AwmPtr_t pawm = papp->CurrentAWM();
	if (pawm == nullptr) {
		pawm = std::make_shared<ba::WorkingMode>(
				papp->WorkingModes().size(),"Run-time", 1, papp);
	}
	logger->Info("SelectWorkingMode: [%s] partition mapping...", papp->StrId());
	int32_t ref_num = -1;

	// Now I will update the Resource Accounter in order to trace the resource allocation
	// This has no effect in the platform resource assignment, since the effective 
	// assignment was performed (by the platform proxy) during the PropagatePartition
	uint32_t nr_cores = 1;
	auto tg = papp->GetTaskGraph();
	for (auto task : tg->Tasks()) {
		// if thread count not specified, assign all the available cores
		logger->Debug("SelectWorkingMode: task=%d thread_count=%d",
			task.first, task.second->GetThreadCount());
		if (task.second->GetThreadCount() <= 0) {
			nr_cores = pe_per_acc
				[selected_partition.GetClusterId()]
				[selected_partition.GetUnit(task.second)];
			logger->Debug("SelectWorkingMode: task=%d nr_cores=%d",
				task.first, nr_cores);
		}

		// Resource request of processing resources
		pawm->AddResourceRequest("sys.grp.acc.pe", 100 * nr_cores,
				br::ResourceAssignment::Policy::BALANCED);

		// Resource Binding: HN cluster (group)
		ref_num = pawm->BindResource(
				br::ResourceType::GROUP, R_ID_ANY,
				selected_partition.GetClusterId(),
				ref_num);

		// Resource binding: HN tile (accelerator)
		ref_num = pawm->BindResource(
				br::ResourceType::ACCELERATOR, R_ID_ANY,
				selected_partition.GetUnit(task.second),
				ref_num);
		logger->Debug("SelectedWorkingMode: task=%d -> cluster=<%d> tile=<%d>",
				task.first,
				selected_partition.GetClusterId(),
				selected_partition.GetUnit(task.second));
	}

	// Get all the assigned memory banks and amount
	br::ResourceBitset mem_bank_ids;
	uint64_t mem_req_amount = 0;
	for (auto buff : tg->Buffers()) {
		mem_req_amount += buff.second->Size();
		uint32_t mem_target_id = selected_partition.GetMemoryBank(buff.second);
		mem_bank_ids.Set(mem_target_id);
		logger->Debug("SelectWorkingMode: buffer=%d -> memory bank %d (id=%d)",
			buff.first,
			selected_partition.GetMemoryBank(buff.second),
			mem_target_id);
	}

	// Resource request for memory
	pawm->AddResourceRequest("mem", mem_req_amount,
				br::ResourceAssignment::Policy::SEQUENTIAL);

	// Resource binding: buffers to memory banks
	logger->Debug("SelectWorkingMode: binding memory node: %s",
		mem_bank_ids.ToString().c_str());
	ref_num = pawm->BindResource(
			br::ResourceType::MEMORY, R_ID_ANY, R_ID_ANY,
			ref_num,
			br::ResourceType::MEMORY, &mem_bank_ids);

	// Update the accounting of resources
	ApplicationManager & am(ApplicationManager::GetInstance());
	auto ret = am.ScheduleRequest(papp, pawm, sched_status_view, ref_num);
	if (ret != ApplicationManager::AM_SUCCESS) {
		logger->Error("SelectWorkingMode: [%s] schedule request failed", papp->StrId());
		return SCHED_ERROR;
	}

	logger->Info("SelectWorkingMode: [%s] schedule request accepted", papp->StrId());
	return SCHED_OK;
}


SchedulerPolicyIF::ExitCode_t
MangASchedPol::ReassignWorkingMode(ba::AppCPtr_t papp) noexcept {
	ApplicationManager & am(ApplicationManager::GetInstance());
	auto ret = am.ScheduleRequestAsPrev(papp, sched_status_view);
	if (ret != ApplicationManager::AM_SUCCESS) {
		logger->Error("ReassignWorkingMode: [%s] rescheduling failed", papp->StrId());
		return SCHED_SKIP_APP;
	}

	logger->Info("ReassignWorkingMode: [%s] rescheduling done", papp->StrId());
	return SCHED_OK;
}


} // namespace plugins

} // namespace bbque
