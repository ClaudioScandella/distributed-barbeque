/*
 * Copyright (C) 2012  Politecnico di Milano
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

#include "random_schedpol.h"

#include "bbque/system.h"
#include "bbque/binding_manager.h"
#include "bbque/app/application.h"
#include "bbque/app/working_mode.h"
#include "bbque/res/resource_path.h"
#include "bbque/resource_accounter.h"
#include "bbque/utils/logging/logger.h"

#include <iostream>
#include <random>

#define NR_ATTEMPTS_MAX  5

namespace ba = bbque::app;
namespace br = bbque::res;
namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

RandomSchedPol::RandomSchedPol() :
	cm(ConfigurationManager::GetInstance()),
	bdm(BindingManager::GetInstance()),
	dist(0, 100) {

	// Get a logger
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	assert(logger);

	logger->Debug("Built RANDOM SchedPol object @%p", (void*)this);

	// Resource binding domain information
	po::options_description opts_desc("Scheduling policy parameters");
	// Binding domain resource path
	opts_desc.add_options()
		(SCHEDULER_POLICY_CONFIG".binding.domain",
		 po::value<std::string>
		 (&binding_domain)->default_value(SCHEDULER_DEFAULT_BINDING_DOMAIN),
		"Resource binding domain");
	;
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Binding domain resource type
	br::ResourcePath rp(binding_domain);
	binding_type = rp.Type();
	logger->Debug("Binding domain:'%s' Type:%s",
			binding_domain.c_str(),
			br::GetResourceTypeString(binding_type));
}

RandomSchedPol::~RandomSchedPol() {
}

//----- Scheduler policy module interface

char const * RandomSchedPol::Name() {
	return SCHEDULER_POLICY_NAME;
}

void RandomSchedPol::ScheduleApp(ba::AppCPtr_t papp) {
	Application::ExitCode_t app_result = Application::APP_SUCCESS;
	ba::AwmPtr_t selected_awm;
	int8_t selected_awm_id;
	uint32_t selected_bd;
	uint8_t bd_count;
	int32_t b_refn;
	std::default_random_engine generator;

	assert(papp);

	// Check for a valid binding domain count
	BindingMap_t & bindings(bdm.GetBindingOptions());
	bd_count = bindings[br::ResourceType::CPU]->resources.size();
	logger->Debug("CPU binding domains : %d", bd_count);
	if (bd_count == 0) {
		assert(bd_count != 0);
		return;
	}

	ba::AwmPtrList_t const & awms(papp->WorkingModes());
	logger->Debug("Application working modes : %d", awms.size());
	std::uniform_int_distribution<int> awm_dist(0, awms.size()-1);
	std::uniform_int_distribution<int> bd_dist(0, bd_count);
	bool binding_done = false;
	int  nr_attempts  = 0;

	while (!binding_done) {
		// Select a random AWM for this EXC
		selected_awm_id = awm_dist(generator);
		logger->Debug("Scheduling EXC [%s] on AWM [%d of %d]",
				papp->StrId(), selected_awm_id, awms.size());
		for (auto & pawm: awms)
			if (pawm->Id() == selected_awm_id)
				selected_awm = pawm;
		assert(selected_awm != nullptr);

		// Bind to a random virtual binding domain
		selected_bd = bd_dist(generator);
		logger->Debug("Scheduling EXC [%s] on binding domain <%d of %d>",
				papp->StrId(), selected_bd, bd_count);
		b_refn = selected_awm->BindResource(binding_type, R_ID_ANY, selected_bd);

		// Scheduling attempt (if binding successful)
		if (b_refn < 0) {
			app_result = papp->ScheduleRequest(selected_awm, ra_view, b_refn);
			if (app_result == ba::ApplicationStatusIF::APP_SUCCESS) {
				logger->Info("Scheduling EXC [%s] on binding domain <%d> done.",
						papp->StrId(), selected_bd);
				binding_done = true;
				continue;
			}
		} else {
			logger->Warn("Resource binding for EXC [%s] on <%d> FAILED "
					"(attempt %d of %d)",
					papp->StrId(), selected_bd, nr_attempts, NR_ATTEMPTS_MAX);

		// Check if we still have attempts
		if (++nr_attempts >= NR_ATTEMPTS_MAX)
			return;
		}
	}
}

SchedulerPolicyIF::ExitCode_t
RandomSchedPol::Init() {
	// Set the counter (overflow will wrap the couter and that's ok)
	++ra_view_count;

	return SCHED_DONE;
}

SchedulerPolicyIF::ExitCode_t
RandomSchedPol::Schedule(bbque::System & sv, br::RViewToken_t &rav) {
	SchedulerPolicyIF::ExitCode_t result;
	AppsUidMapIt app_it;
	ba::AppCPtr_t papp;

	if (!logger) {
		assert(logger);
		return SCHED_ERROR;
	}

	ra_view = rav;

	// Initialize a new resources state view
	result = Init();
	if (result != SCHED_DONE)
		return result;

	logger->Info("Random scheduling RUNNING applications...");

	papp = sv.GetFirstRunning(app_it);
	while (papp) {
		ScheduleApp(papp);
		papp = sv.GetNextRunning(app_it);
	}

	logger->Info("Random scheduling READY applications...");

	papp = sv.GetFirstReady(app_it);
	while (papp) {
		ScheduleApp(papp);
		papp = sv.GetNextReady(app_it);
	}

	return SCHED_DONE;
}

//----- static plugin interface

void * RandomSchedPol::Create(PF_ObjectParams *) {
	return new RandomSchedPol();
}

int32_t RandomSchedPol::Destroy(void * plugin) {
  if (!plugin)
    return -1;
  delete (RandomSchedPol *)plugin;
  return 0;
}

} // namesapce plugins

} // namespace bque

