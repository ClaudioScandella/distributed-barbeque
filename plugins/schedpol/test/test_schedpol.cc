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

#include "test_schedpol.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "bbque/modules_factory.h"
#include "bbque/binding_manager.h"
#include "bbque/utils/logging/logger.h"

#include "bbque/app/working_mode.h"
#include "bbque/res/binder.h"
#include "bbque/res/resource_path.h"
#include "tg/task_graph.h"

#define MODULE_CONFIG SCHEDULER_POLICY_CONFIG "." SCHEDULER_POLICY_NAME

namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

// :::::::::::::::::::::: Static plugin interface ::::::::::::::::::::::::::::

void * TestSchedPol::Create(PF_ObjectParams *) {
	return new TestSchedPol();
}

int32_t TestSchedPol::Destroy(void * plugin) {
	if (!plugin)
		return -1;
	delete (TestSchedPol *)plugin;
	return 0;
}

// ::::::::::::::::::::: Scheduler policy module interface :::::::::::::::::::

char const * TestSchedPol::Name() {
	return SCHEDULER_POLICY_NAME;
}

TestSchedPol::TestSchedPol():
		cm(ConfigurationManager::GetInstance()),
		ra(ResourceAccounter::GetInstance()) {
	logger = bu::Logger::GetLogger("bq.sp.test");
	assert(logger);
	if (logger)
		logger->Info("test: Built a new dynamic object[%p]", this);
	else
		fprintf(stderr,
			FI("test: Built new dynamic object [%p]\n"), (void *)this);
}


TestSchedPol::~TestSchedPol() {

}


SchedulerPolicyIF::ExitCode_t TestSchedPol::Init() {
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

	// Processing elements IDs
	auto & resource_types = sys->ResourceTypes();
	auto const & r_ids_entry = resource_types.find(br::ResourceType::PROC_ELEMENT);
	pe_ids = r_ids_entry->second;
	logger->Debug("Init: %d processing elements available", pe_ids.size());
	if (pe_ids.empty()) {
		logger->Crit("Init: not available CPU cores!");
		return SCHED_R_UNAVAILABLE;
	}

	// Load all the applications task graphs
	logger->Debug("Init: loading the applications task graphs");
	fut_tg = std::async(std::launch::async, &System::LoadTaskGraphs, sys);

	return SCHED_OK;
}


SchedulerPolicyIF::ExitCode_t
TestSchedPol::Schedule(
		System & system,
		RViewToken_t & status_view) {
	SchedulerPolicyIF::ExitCode_t result = SCHED_DONE;

	// Class providing query functions for applications and resources
	sys = &system;
	result = Init();
	if (result != SCHED_OK)
		return result;

	/** INSERT YOUR CODE HERE **/
	bbque::app::AppCPtr_t papp;
	AppsUidMapIt app_it;

	fut_tg.get();

	// Ready applications
	papp = sys->GetFirstReady(app_it);
	for (; papp; papp = sys->GetNextReady(app_it)) {
		AssignWorkingMode(papp);
	}

	// Running applications
	papp = sys->GetFirstRunning(app_it);
	for (; papp; papp = sys->GetNextRunning(app_it)) {
		papp->CurrentAWM()->ClearResourceRequests();
		AssignWorkingMode(papp);
	}

	// Return the new resource status view according to the new resource
	// allocation performed
	status_view = sched_status_view;
	return result;
}


#define CPU_QUOTA_TO_ALLOCATE 100
#define CPU_ASSIGNED_ID         1

SchedulerPolicyIF::ExitCode_t
TestSchedPol::AssignWorkingMode(bbque::app::AppCPtr_t papp) {

	// Build a new working mode featuring assigned resources
	ba::AwmPtr_t pawm = papp->CurrentAWM();
	if (pawm == nullptr) {
		pawm = std::make_shared<ba::WorkingMode>(
				papp->WorkingModes().size(),"Run-time", 1, papp);
	}

	// Resource request addition
	pawm->AddResourceRequest(
		"sys0.cpu.pe", CPU_QUOTA_TO_ALLOCATE,
		br::ResourceAssignment::Policy::BALANCED);

	BindingManager & bdm(BindingManager::GetInstance());
	BindingMap_t & bindings(bdm.GetBindingDomains());
	auto & cpu_ids(bindings[br::ResourceType::CPU]->r_ids);
	if (cpu_ids.empty()) {
		logger->Crit("AssingWorkingMode: not available CPUs!");
		return SCHED_ERROR;
	}

	// Look for the first available CPU
	for (BBQUE_RID_TYPE cpu_id: cpu_ids) {
		logger->Info("AssingWorkingMode: binding attempt CPU id = %d", cpu_id);

		// CPU binding
		auto ref_num = DoCPUBinding(pawm, cpu_id);
		if (ref_num < 0) {
			logger->Error("AssingWorkingMode: CPU binding to [%d] failed", cpu_id);
			continue;
		}

		// Schedule request
		ApplicationManager & am(ApplicationManager::GetInstance());
		ApplicationManager::ExitCode_t am_ret;
		am_ret = am.ScheduleRequest(papp, pawm, sched_status_view, ref_num);
		if (am_ret != ApplicationManager::AM_SUCCESS) {
			logger->Error("AssignWorkingMode: schedule request failed for [%d]",
				papp->StrId());
			continue;
		}

#ifdef CONFIG_BBQUE_TG_PROG_MODEL
		MapTaskGraph(papp); // Task level mapping
#endif // CONFIG_BBQUE_TG_PROG_MODEL
		return SCHED_OK;
	}

	return SCHED_ERROR;
}


int32_t TestSchedPol::DoCPUBinding(
		bbque::app::AwmPtr_t pawm,
		BBQUE_RID_TYPE cpu_id) {
	// CPU-level binding: the processing elements are in the scope of the CPU 'cpu_id'
	int32_t ref_num = -1;
	ref_num = pawm->BindResource(br::ResourceType::CPU, R_ID_ANY, cpu_id, ref_num);
	auto resource_path = ra.GetPath("sys0.cpu" + std::to_string(cpu_id) + ".pe");

	// The ResourceBitset object is used for the processing elements binding
	// (CPU core mapping)
	br::ResourceBitset pes;
	uint16_t pe_count = 0;
	for (auto & pe_id: pe_ids) {
		pes.Set(pe_id);
		ref_num = pawm->BindResource(resource_path, pes, ref_num);
		logger->Info("AssignWorkingMode: binding refn: %d", ref_num);
		++pe_count;
		if (pe_count == CPU_QUOTA_TO_ALLOCATE / 100) break;
	}

	return ref_num;
}

#ifdef CONFIG_BBQUE_TG_PROG_MODEL

void TestSchedPol::MapTaskGraph(bbque::app::AppCPtr_t papp) {
	auto task_graph = papp->GetTaskGraph();
	if (task_graph == nullptr) {
		logger->Warn("[%s] No task-graph to map", papp->StrId());
		return;
	}

	uint16_t throughput;
	uint32_t c_time;
	int unit_id = 3; // An arbitrary device id number

	for (auto t_entry: task_graph->Tasks()) {
		unit_id++;
		auto & task(t_entry.second);
		auto const & tr(papp->GetTaskRequirements(task->Id()));

		task->SetMappedProcessor(unit_id);
		task->GetProfiling(throughput, c_time);
		logger->Info("[%s] <T %d> throughput: %.2f/%.2f  ctime: %d/%d [ms]",
			papp->StrId(), t_entry.first,
			static_cast<float>(throughput)/100.0, tr.Throughput(),
			c_time, tr.CompletionTime());
	}

	task_graph->GetProfiling(throughput, c_time);
	logger->Info("[%s] Task-graph throughput: %d  ctime: %d [app=%p]",
		papp->StrId(), throughput, c_time, papp.get());

	papp->SetTaskGraph(task_graph);
	logger->Info("[%s] Task-graph updated", papp->StrId());
}

#endif // CONFIG_BBQUE_TG_PROG_MODEL

} // namespace plugins

} // namespace bbque
