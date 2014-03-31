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
#include "bbque/app/application.h"
#include "bbque/app/working_mode.h"
#include "bbque/res/resource_path.h"
#include "bbque/resource_accounter.h"
#include "bbque/utils/logging/logger.h"

#include <iostream>

namespace ba = bbque::app;
namespace br = bbque::res;
namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

RandomSchedPol::RandomSchedPol() :
	cm(ConfigurationManager::GetInstance()),
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
			binding_domain.c_str(), br::ResourceIdentifier::TypeStr[binding_type]);
}

RandomSchedPol::~RandomSchedPol() {
}

//----- Scheduler policy module interface

char const * RandomSchedPol::Name() {
	return SCHEDULER_POLICY_NAME;
}


/**
 * The RNG used for AWM selection.
 */
std::mt19937 rng_engine(time(0));

void RandomSchedPol::ScheduleApp(ba::AppCPtr_t papp) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ba::AwmPtrList_t::const_iterator it;
	ba::AwmPtrList_t::const_iterator end;
	ba::AwmPtrList_t const *awms;
	uint32_t selected_awm;
	uint32_t selected_bd;
	uint8_t bd_count;
	size_t b_refn;

	assert(papp);

	// Check for a valid binding domain count
	bd_count = ra.Total(binding_domain.c_str());
	if (bd_count == 0) {
		assert(bd_count != 0);
		return;
	}

	// Select a random AWM for this EXC
	awms = papp->WorkingModes();
	selected_awm = dist(rng_engine) % awms->size();
	logger->Debug("Scheduling EXC [%s] on AWM [%d of %d]",
			papp->StrId(), selected_awm, awms->size());
	it = awms->begin();
	end = awms->end();
	for ( ; selected_awm && it!=end; --selected_awm, ++it);
	assert(it!=end);

	// Bind to a random virtual binding domain
	selected_bd = dist(rng_engine) % bd_count;
	logger->Debug("Scheduling EXC [%s] on binding domain [%d of %d]",
			papp->StrId(), selected_bd, ra.Total(binding_domain.c_str()));
	b_refn = (*it)->BindResource(binding_type, R_ID_ANY, selected_bd);
	if (b_refn == 0) {
		logger->Error("Resource biding for EXC [%s] FAILED", papp->StrId());
		return;
	}

	// Schedule the selected AWM on the selected binding domain
	papp->ScheduleRequest((*it), ra_view);

}

SchedulerPolicyIF::ExitCode_t
RandomSchedPol::Init() {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ResourceAccounterStatusIF::ExitCode_t result;
	char token_path[32];

	// Set the counter (overflow will wrap the couter and that's ok)
	++ra_view_count;

	// Build a string path for the resource state view
	snprintf(token_path, 32, "%s%d", MODULE_NAMESPACE, ra_view_count);

	// Get a resource state view
	logger->Debug("Init: Requiring state view token for %s", token_path);
	result = ra.GetView(token_path, ra_view);
	if (result != ResourceAccounterStatusIF::RA_SUCCESS) {
		logger->Fatal("Init: Cannot get a resource state view");
		return SCHED_ERROR;
	}
	logger->Debug("Init: Resources state view token = %d", ra_view);

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

	// Pass back to the SchedulerManager a reference to the scheduled view
	rav = ra_view;
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

