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

#include "emulsion_schedpol.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>

#include "bbque/app/working_mode.h"
#include "bbque/modules_factory.h"
#include "bbque/res/binder.h"
#include "bbque/res/resource_path.h"
#include "bbque/utils/logging/logger.h"

#define MODULE_CONFIG SCHEDULER_POLICY_CONFIG "." SCHEDULER_POLICY_NAME

namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

// :::::::::::::::::::::: Static plugin interface ::::::::::::::::::::::::::::

void * EmulsionSchedPol::Create(PF_ObjectParams *) {
	return new EmulsionSchedPol();
}

int32_t EmulsionSchedPol::Destroy(void * plugin) {
	if (!plugin)
		return -1;
	delete (EmulsionSchedPol *)plugin;
	return 0;
}

// ::::::::::::::::::::: Scheduler policy module interface :::::::::::::::::::

char const * EmulsionSchedPol::Name() {
	return SCHEDULER_POLICY_NAME;
}

EmulsionSchedPol::EmulsionSchedPol():
		cm(ConfigurationManager::GetInstance()),
		ra(ResourceAccounter::GetInstance()) {
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	assert(logger);
	if (logger)
		logger->Info("emulsion: Built a new dynamic object[%p]", this);
	else
		fprintf(stderr,
			FI("emulsion: Built new dynamic object [%p]\n"), (void *)this);
}


EmulsionSchedPol::~EmulsionSchedPol() {

}


SchedulerPolicyIF::ExitCode_t EmulsionSchedPol::Init() {
	++status_view_count;

	// Get the available PEs: the minimum between all available to Barbeque 
	// and the max assignable to rt tasks

	float perc_avail = BBQUE_RT_MAX_CPU / 1000.0;

	total_rt_cpu_available = ra.Total("sys.cpu.pe") * perc_avail;
	logger->Debug("Total available CPUs for Real-Time tasks [%i]",
			total_rt_cpu_available);

	return SCHED_OK;
}


SchedulerPolicyIF::ExitCode_t
EmulsionSchedPol::Schedule(
		System & system,
		RViewToken_t & status_view) {
	SchedulerPolicyIF::ExitCode_t result = SCHED_DONE;

	// Assign the object variables
	sys = &system;
	sched_status_view = status_view;

	Init();


	for (AppPrio_t priority = 0;
				   priority <= sys->ApplicationLowestPriority();
				   priority ++) {

		// Checking if there are applications at this priority
		if (!sys->HasApplications(priority))
			continue;

		AppsUidMapIt app_it;
		bbque::app::AppCPtr_t papp = sys->GetFirstWithPrio(priority, app_it);
		for (; papp; papp = sys->GetNextWithPrio(priority, app_it)) {

			switch (papp->RTLevel()) {
				case RT_NONE:
				break;
				case RT_SOFT:
					ScheduleSoftRTEntity(papp);
				break;
#ifdef BBQUE_RT_HARD
				case RT_HARD:
				break;
#endif
				default:
					logger->Crit("Unknown RT Level, undefined "
								"behaviour may occur.");
				break;
			}

		}
	
	}

	status_view = sched_status_view;

	return result;
}

SchedulerPolicyIF::ExitCode_t
EmulsionSchedPol::ScheduleSoftRTEntity(bbque::app::AppCPtr_t papp) {
	int napps = sys->ApplicationsCount(RT_SOFT);
	assert(napps > 0);
	int assigned_rt_cpu = total_rt_cpu_available / napps;

	logger->Debug("Assigned [%i] of CPU to RT task [%s]",
			assigned_rt_cpu, papp->StrId());

	ScheduleApplication(papp, assigned_rt_cpu);

	return SCHED_DONE;
}

SchedulerPolicyIF::ExitCode_t EmulsionSchedPol::ScheduleApplication(
		bbque::app::AppCPtr_t papp,
		uint32_t proc_quota)
{
	// Build a new working mode featuring assigned resources
	ba::AwmPtr_t pawm = papp->CurrentAWM();
	if (pawm == nullptr) {
		pawm = std::make_shared<ba::WorkingMode>(
				papp->WorkingModes().size(),"Run-time", 1, papp);
	}
	else
		pawm->ClearResourceRequests();

	logger->Debug("Schedule: [%s] adding the processing resource request...",
		papp->StrId());

	pawm->AddResourceRequest(
		"sys0.cpu.pe", proc_quota, br::ResourceAssignment::Policy::SEQUENTIAL);

	logger->Debug("Schedule: [%s] CPU binding... (with quota [%d])", papp->StrId(), proc_quota);
	int32_t ref_num = -1;
	ref_num = pawm->BindResource(br::ResourceType::CPU, R_ID_ANY, R_ID_ANY, ref_num);
	auto resource_path = std::make_shared<bbque::res::ResourcePath>("sys0.cpu.pe");

	bbque::app::Application::ExitCode_t app_result =
		papp->ScheduleRequest(pawm, sched_status_view, ref_num);
	if (app_result != bbque::app::Application::APP_SUCCESS) {
		logger->Error("Schedule: scheduling of [%s] failed", papp->StrId());
		return SCHED_ERROR;
	}

	return SCHED_OK;
}



} // namespace plugins

} // namespace bbque
