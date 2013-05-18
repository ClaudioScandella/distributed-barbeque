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

#include "yams_schedpol.h"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <functional>

#include "bbque/cpp11/thread.h"
#include "bbque/modules_factory.h"
#include "bbque/app/working_mode.h"
#include "bbque/plugins/logger.h"
#include "contrib/sched_contrib_manager.h"

namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {


SchedContribManager::Type_t YamsSchedPol::sc_types[] = {
	SchedContribManager::VALUE,
	SchedContribManager::RECONFIG,
	SchedContribManager::CONGESTION,
	SchedContribManager::FAIRNESS
};

// Definition of time metrics of the scheduling policy
MetricsCollector::MetricsCollection_t
YamsSchedPol::coll_metrics[YAMS_METRICS_COUNT] = {
	YAMS_SAMPLE_METRIC("ord",
			"Time to order SchedEntity into a cluster [ms]"),
	YAMS_SAMPLE_METRIC("sel",
			"Time to select AWMs/Clusters for the EXC [ms]"),
	YAMS_SAMPLE_METRIC("mcomp",
			"Time for computing a single metrics [ms]"),
	YAMS_SAMPLE_METRIC("awmvalue",
			"AWM value of the scheduled entity")
};

// Definition of time metrics for each SchedContrib computation
MetricsCollector::MetricsCollection_t
YamsSchedPol::coll_mct_metrics[YAMS_SC_COUNT] = {
	YAMS_SAMPLE_METRIC("awmv.comp",
			"AWM value computing time [ms]"),
	YAMS_SAMPLE_METRIC("recf.comp",
			"Reconfiguration contribute computing time [ms]"),
	YAMS_SAMPLE_METRIC("cgst.comp",
			"Congestion contribute computing time [ms]"),
	YAMS_SAMPLE_METRIC("fair.comp",
			"Fairness contribute computing time [ms]")
	// ...:: ADD_MCT ::...
};

// :::::::::::::::::::::: Static plugin interface ::::::::::::::::::::::::::::

void * YamsSchedPol::Create(PF_ObjectParams *) {
	return new YamsSchedPol();
}

int32_t YamsSchedPol::Destroy(void * plugin) {
	if (!plugin)
		return -1;
	delete (YamsSchedPol *)plugin;
	return 0;
}

// ::::::::::::::::::::: Scheduler policy module interface :::::::::::::::::::

char const * YamsSchedPol::Name() {
	return SCHEDULER_POLICY_NAME;
}

YamsSchedPol::YamsSchedPol():
	cm(ConfigurationManager::GetInstance()),
	ra(ResourceAccounter::GetInstance()),
	mc(bu::MetricsCollector::GetInstance()) {

	// Get a logger
	plugins::LoggerIF::Configuration conf(MODULE_NAMESPACE);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	if (logger)
		logger->Info("yams: Built a new dynamic object[%p]", this);
	else
		fprintf(stderr, FI("yams: Built new dynamic object [%p]\n"), (void *)this);

	// Binding domain resource path
	po::options_description opts_desc("Scheduling policy parameters");
	opts_desc.add_options()
		(SCHEDULER_POLICY_CONFIG".binding.domain",
		 po::value<std::string>
		 (&bindings.domain)->default_value(SCHEDULER_DEFAULT_BINDING_DOMAIN),
		"Resource binding domain");
	;
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Binding domain resource type
	ResourcePath rp(bindings.domain);
	bindings.type = rp.Type();
	logger->Debug("Binding domain:'%s' Type:%s",
			bindings.domain.c_str(), ResourceIdentifier::TypeStr[bindings.type]);

	// Instantiate the SchedContribManager
	scm = new SchedContribManager(sc_types, bindings.domain, YAMS_SC_COUNT);

	// Resource view counter
	vtok_count = 0;

	// Register all the metrics to collect
	mc.Register(coll_metrics, YAMS_METRICS_COUNT);
	mc.Register(coll_mct_metrics, YAMS_SC_COUNT);
}

YamsSchedPol::~YamsSchedPol() {

}

YamsSchedPol::ExitCode_t YamsSchedPol::Init() {
	ResourceAccounterStatusIF::ExitCode_t ra_result;
	char token_path[30];

	// Set the counter
	++vtok_count;

	// Build a string path for the resource state view
	snprintf(token_path, 30, "%s%d", MODULE_NAMESPACE, vtok_count);

	// Get a resource state view
	logger->Debug("Init: Requiring state view token for %s", token_path);
	ra_result = ra.GetView(token_path, vtok);
	if (ra_result != ResourceAccounterStatusIF::RA_SUCCESS) {
		logger->Fatal("Init: Cannot get a resource state view");
		return YAMS_ERR_VIEW;
	}
	logger->Debug("Init: Resources state view token = %d", vtok);

	// Get the base information for the resource bindings
	bindings.rsrcs = sv->GetResources(bindings.domain);
	bindings.num   = bindings.rsrcs.size();
	bindings.ids.resize(bindings.num);
	if (bindings.num == 0) {
		logger->Error("Init: No bindings to R{%s} possible. "
				"Please check the platform configuration.",
				bindings.domain.c_str());
		return YAMS_ERR_CLUSTERS;
	}

	// Get all the possible resource binding IDs
	ResourcePtrList_t::iterator b_it(bindings.rsrcs.begin());
	ResourcePtrList_t::iterator b_end(bindings.rsrcs.end());
	for (uint8_t j = 0; b_it != b_end; ++b_it, ++j) {
		ResourcePtr_t const & rsrc(*b_it);
		bindings.ids[j] = rsrc->ID();
		logger->Debug("Init: R{%s} ID: %d",
				bindings.domain.c_str(), bindings.ids[j]);
	}
	logger->Debug("Init: R{%s}: %d possible bindings",
			bindings.domain.c_str(), bindings.num);
	logger->Debug("Init: Lowest application prio : %d",
			sv->ApplicationLowestPriority());

	// Set the view information into the metrics contribute
	scm->SetViewInfo(sv, vtok);

	return YAMS_SUCCESS;
}

SchedulerPolicyIF::ExitCode_t
YamsSchedPol::Schedule(System & sys_if, RViewToken_t & rav) {
	ExitCode_t result;

	// Save a reference to the System interface;
	sv = &sys_if;

	// Initialize a new resources state view
	result = Init();
	if (result != YAMS_SUCCESS)
		goto error;

	// Schedule per priority
	for (AppPrio_t prio = 0; prio <= sv->ApplicationLowestPriority(); ++prio) {
		if (!sv->HasApplications(prio))
			continue;
		SchedulePrioQueue(prio);
	}

	// Set the new resource state view token
	rav = vtok;

	// Cleaning
	entities.clear();
	bindings.full.Reset();

	// Report table
	ra.PrintStatusReport(vtok);
	return SCHED_DONE;

error:
	logger->Error("Schedule: an error occurred. Interrupted.");
	entities.clear();
	bindings.full.Reset();

	ra.PutView(vtok);
	return SCHED_ERROR;
}

void YamsSchedPol::SchedulePrioQueue(AppPrio_t prio) {
	bool sched_incomplete;
	uint8_t naps_count = 0;
	SchedContribPtr_t sc_fair;

	// Reset timer
	YAMS_RESET_TIMING(yams_tmr);

do_schedule:
	// Init fairness contribute
	sc_fair = scm->GetContrib(SchedContribManager::FAIRNESS);
	assert(sc_fair != nullptr);
	sc_fair->Init(&prio);

	// Order schedule entities by aggregate metrics
	naps_count = OrderSchedEntities(prio);
	YAMS_GET_TIMING(coll_metrics, YAMS_ORDERING_TIME, yams_tmr);

	// Selection: for each application schedule a working mode
	YAMS_RESET_TIMING(yams_tmr);
	sched_incomplete = SelectSchedEntities(naps_count);
	entities.clear();

	if (sched_incomplete)
		goto do_schedule;

	// Stop timing metrics
	YAMS_GET_TIMING(coll_metrics, YAMS_SELECTING_TIME, yams_tmr);
}

uint8_t YamsSchedPol::OrderSchedEntities(AppPrio_t prio) {
	uint8_t naps_count = 0;
	AppsUidMapIt app_it;
	AppCPtr_t papp;

	// Applications to be scheduled
	papp = sv->GetFirstWithPrio(prio, app_it);
	for (; papp; papp = sv->GetNextWithPrio(prio, app_it)) {
		// Check if the Application/EXC must be skipped
		if (CheckSkipConditions(papp))
			continue;

		// Compute the metrics for each AWM binding resources to cluster 'bd_id'
		InsertWorkingModes(papp);

		// Keep track of NAPped Applications/EXC
		if (papp->GetGoalGap())
			++naps_count;
	}

	// Order the scheduling entities list
	entities.sort(CompareEntities);

	return naps_count;
}

bool YamsSchedPol::SelectSchedEntities(uint8_t naps_count) {
	Application::ExitCode_t app_result;
	SchedEntityList_t::iterator se_it(entities.begin());
	SchedEntityList_t::iterator end_se(entities.end());
	logger->Debug("=================| Scheduling entities |=================");

	// Pick the entity and set the new AWM
	for (; se_it != end_se; ++se_it) {
		SchedEntityPtr_t & pschd(*se_it);

		// Skip this AWM,Binding if the bound domain is full or if the
		// Application/EXC must be skipped
		if (bindings.full.Test(pschd->bind_id) ||
				(CheckSkipConditions(pschd->papp)))
			continue;

		// Send the schedule request
		app_result = pschd->papp->ScheduleRequest(pschd->pawm, vtok,
				pschd->bind_id);
		logger->Debug("Selecting: %s schedule requested", pschd->StrId());
		if (app_result != ApplicationStatusIF::APP_WM_ACCEPTED) {
			logger->Debug("Selecting: %s rejected!", pschd->StrId());
			continue;
		}
		if (!pschd->papp->Synching() || pschd->papp->Blocking()) {
			logger->Debug("Selecting: [%s] state %s|%s", pschd->papp->StrId(),
					Application::StateStr(pschd->papp->State()),
					Application::SyncStateStr(pschd->papp->SyncState()));
			continue;
		}
		logger->Notice("Selecting: %s SCHEDULED metrics: %.4f",
				pschd->StrId(), pschd->metrics);

		// Set the application value (scheduling metrics)
		pschd->papp->SetValue(pschd->metrics);
		YAMS_GET_SAMPLE(coll_metrics, YAMS_METRICS_AWMVALUE,
				pschd->pawm->Value());

		// Break as soon as all NAPped apps have been scheduled
		if (naps_count && (--naps_count == 0))
			break;
	}

	if (se_it != end_se) {
		logger->Debug("======================| NAP Break |===================");
		return true;
	}

	logger->Debug("========================| DONE |======================");
	return false;
}

void YamsSchedPol::InsertWorkingModes(AppCPtr_t const & papp) {
	std::list<std::thread> awm_thds;
	std::vector<ResID_t>::iterator ids_it;
	std::vector<ResID_t>::iterator end_ids;
	AwmPtrList_t const * awms = nullptr;
	AwmPtrList_t::const_iterator awm_it;
	ResID_t bd_id = R_ID_NONE;
	float metrics = 0.0;

	// For each binding domain (e.g., CPU node) evaluate the AWM
	ids_it = bindings.ids.begin();
	for (; ids_it != bindings.ids.end(); ++ids_it) {
		bd_id = *ids_it;
		logger->Info("Evaluate: :::::::::: BINDING:'%s' ID:[%d] :::::::::",
				bindings.domain.c_str(), bd_id);

		// Skip current cluster if full
		if (bindings.full[bd_id]) {
			logger->Info("Evaluate: domain [%s%d] is full, skipping...",
					bindings.domain.c_str(), bd_id);
			continue;
		}

		// AWMs (+resources bound to 'bd_id') evaluation
		awms = papp->WorkingModes();
		for (awm_it = awms->begin(); awm_it != awms->end(); ++awm_it) {
			AwmPtr_t const & pawm(*awm_it);
			SchedEntityPtr_t pschd(new SchedEntity_t(papp, pawm, bd_id, metrics));
#ifdef CONFIG_BBQUE_SP_YAMS_PARALLEL
			awm_thds.push_back(
					std::thread(&YamsSchedPol::EvalWorkingMode, this, pschd)
			);
#else
			EvalWorkingMode(pschd);
#endif
		}
	}

#ifdef CONFIG_BBQUE_SP_YAMS_PARALLEL
	for_each(awm_thds.begin(), awm_thds.end(), mem_fn(&std::thread::join));
	awm_thds.clear();
#endif
	logger->Debug("Evaluate: number of entities = %d", entities.size());
}

void YamsSchedPol::EvalWorkingMode(SchedEntityPtr_t pschd) {
	std::unique_lock<std::mutex> sched_ul(sched_mtx, std::defer_lock);
	ExitCode_t result;
	logger->Debug("Insert: %s ...metrics computing...", pschd->StrId());

	// Skip if the application has been disabled/stopped in the meanwhile
	if (pschd->papp->Disabled()) {
		logger->Debug("Insert: %s disabled/stopped during schedule ordering",
				pschd->papp->StrId());
		return;
	}

	// Bind the resources of the AWM to the current binding domain
	result = BindResources(pschd);
	if (result != YAMS_SUCCESS)
		return;

	// Metrics computation
	Timer comp_tmr;
	YAMS_RESET_TIMING(comp_tmr);
	AggregateContributes(pschd);
	YAMS_GET_TIMING(coll_metrics, YAMS_METRICS_COMP_TIME, comp_tmr);

	// Insert the SchedEntity in the scheduling list
	sched_ul.lock();
	entities.push_back(pschd);
	logger->Debug("Insert: %s scheduling metrics: %1.3f [%d]",
			pschd->StrId(), pschd->metrics, entities.size());
}

void YamsSchedPol::AggregateContributes(SchedEntityPtr_t pschd) {
	SchedContribManager::ExitCode_t scm_ret;
	SchedContrib::ExitCode_t sc_ret;
	float sc_value;
	char mlog[255];
	uint8_t mlog_len = 0;

	for (int i = 0; i < YAMS_SC_COUNT; ++i) {
		// Timer
		Timer comp_tmr;

		sc_value = 0.0;
		EvalEntity_t const & eval_ent(*pschd.get());
		YAMS_RESET_TIMING(comp_tmr);

		// Compute the single contribution
		scm_ret = scm->GetIndex(sc_types[i], eval_ent, sc_value, sc_ret);
		if (scm_ret != SchedContribManager::OK) {

			logger->Error("Aggregate: [SC manager error:%d]", scm_ret);
			if (scm_ret != SchedContribManager::SC_ERROR) {
				YAMS_RESET_TIMING(comp_tmr);
				continue;
			}

			// SchedContrib specific error handling
			switch (sc_ret) {
			case SchedContrib::SC_RSRC_NO_PE:
				logger->Debug("Aggregate: No available PEs in {%s} %d",
						bindings.domain.c_str(), pschd->bind_id);
				bindings.full.Set(pschd->bind_id);
				return;
			default:
				logger->Warn("Aggregate: Unable to schedule in {%s} %d [err:%d]",
						bindings.domain.c_str(), pschd->bind_id, sc_ret);
				YAMS_GET_TIMING(coll_mct_metrics, i, comp_tmr);
				continue;
			}
		}
		YAMS_GET_TIMING(coll_mct_metrics, i, comp_tmr);
		logger->Debug("Aggregate: back from index computation");

		// Cumulate the contribution
		pschd->metrics += sc_value;
		mlog_len += sprintf(mlog + mlog_len, "%c:%5.4f, ",
				scm->GetString(sc_types[i])[0],	sc_value);
	}

	mlog[mlog_len-2] = '\0';
	logger->Notice("Aggregate: %s app-value: %s => %5.4f",
			pschd->StrId(),	mlog, pschd->metrics);
}

YamsSchedPol::ExitCode_t YamsSchedPol::BindResources(SchedEntityPtr_t pschd) {
	WorkingModeStatusIF::ExitCode_t awm_result;
	AwmPtr_t & pawm(pschd->pawm);
	ResID_t & bd_id(pschd->bind_id);

	// Binding of the AWM resource into the current cluster.
	// Since the policy handles more than one binding per AWM the resource
	// binding is referenced by the ID of the bound resource
	awm_result = pawm->BindResource(bindings.type, R_ID_ANY, bd_id, bd_id);

	// The resource binding should never fail
	if (awm_result == WorkingModeStatusIF::WM_RSRC_MISS_BIND) {
		logger->Error("BindResources: AWM{%d} to resource '%s' ID=%d."
				"Incomplete	resources binding. %d / %d resources bound.",
				pawm->Id(), bindings.type, bd_id,
				pawm->GetSchedResourceBinding()->size(),
				pawm->RecipeResourceUsages().size());
		assert(awm_result == WorkingModeStatusIF::WM_SUCCESS);
		return YAMS_ERROR;
	}
	logger->Debug("BindResources: AWM{%d} to resource '%s' ID=%d",
			pawm->Id(), bindings.domain.c_str(), bd_id);

	return YAMS_SUCCESS;
}

bool YamsSchedPol::CompareEntities(SchedEntityPtr_t & se1,
		SchedEntityPtr_t & se2) {
	uint8_t gg1, gg2;

	// Metrics (primary sorting key)
	if (se1->metrics < se2->metrics)
		return false;
	if (se1->metrics > se2->metrics)
		return true;

	// Apps asserting a NAP should be considered first
	gg1 = se1->papp->GetGoalGap();
	gg2 = se2->papp->GetGoalGap();
	if ((gg1 > 0) && (gg1 >= gg2))
		return true;
	if ((gg2 > 0) && (gg2 >= gg1))
		return false;

	// Higher value AWM first
	if (se1->pawm->Value() > se2->pawm->Value())
		return true;

	return false;
}


} // namespace plugins

} // namespace bbque
