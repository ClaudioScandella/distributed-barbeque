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
#include "bbque/utils/logging/logger.h"
#include "bbque/res/binder.h"
#include "contrib/sched_contrib_manager.h"
#include "bbque/utils/attributes_container.h"

namespace po = boost::program_options;

namespace bbque { namespace plugins {


SchedContribManager::Type_t YamsSchedPol::sc_types[] = {
	SchedContribManager::VALUE,
	SchedContribManager::RECONFIG,
	SchedContribManager::FAIRNESS,
	SchedContribManager::MIGRATION,
#ifndef CONFIG_BBQUE_SP_COWS_BINDING
	SchedContribManager::CONGESTION
#endif
};

SchedContribManager::Type_t YamsSchedPol::sc_gpu[] = {
	SchedContribManager::VALUE,
	SchedContribManager::FAIRNESS,
#ifndef CONFIG_BBQUE_SP_COWS_BINDING
	SchedContribManager::CONGESTION
#endif
};

#ifdef CONFIG_BBQUE_SP_COWS_BINDING
const char * YamsSchedPol::cows_metrics_str[] = {
	"stalls",
	"iret"  ,
	"flops" ,
	"llcm"
};
#endif // CONFIG_BBQUE_SP_COWS_BINDING


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
			"Reconfiguration contribution computing time [ms]"),
	YAMS_SAMPLE_METRIC("fair.comp",
			"Fairness contribution computing time [ms]"),
	YAMS_SAMPLE_METRIC("migr.comp",
			"Migration contribution computing time [ms]"),
#ifndef CONFIG_BBQUE_SP_COWS_BINDING
	YAMS_SAMPLE_METRIC("cgst.comp",
			"Congestion contribution computing time [ms]"),
#endif
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
		mc(bu::MetricsCollector::GetInstance()),
		cmm(CommandManager::GetInstance()) {

	// Get a logger
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	assert(logger);

	// Load binding domains configuration
	LoadBindingConfig();

	// Resource view counter
	vtok_count = 0;

	// Register all the metrics to collect
	mc.Register(coll_metrics, YAMS_METRICS_COUNT);
	mc.Register(coll_mct_metrics, YAMS_SC_COUNT);
}

YamsSchedPol::ExitCode_t YamsSchedPol::LoadBindingConfig() {
	br::Resource::Type_t bd_type;
	std::string bd_domains;
	std::string bd_str;
	size_t end_pos, beg_pos = 0;

	// Binding domain resource path
	po::options_description opts_desc("Scheduling policy parameters");
	opts_desc.add_options()
		(SCHEDULER_POLICY_CONFIG".binding.domain",
		 po::value<std::string>
		 (&bd_domains)->default_value(SCHEDULER_DEFAULT_BINDING_DOMAIN),
		"Resource binding domain");
	;
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Parse each binding domain string
	while (end_pos != std::string::npos) {
		end_pos = bd_domains.find(',', beg_pos);
		bd_str  = bd_domains.substr(beg_pos, end_pos);

		// Binding domain resource type
		br::ResourcePath rp(bd_str);
		bd_type = rp.Type();
		if (bd_type == br::Resource::UNDEFINED ||
				bd_type == br::Resource::TYPE_COUNT) {
			logger->Error("Invalid binding domain type for: %s",
					bd_str.c_str());
			beg_pos  = end_pos + 1;
			continue;
		}

		// New binding info structure
		bindings.insert(BindingPair_t(bd_type, new BindingInfo_t));
		bindings[bd_type]->domain = bd_str;
		bindings[bd_type]->type   = bd_type;
		logger->Info("Binding domain:'%s' Type:%s",
				bindings[bd_type]->domain.c_str(),
				br::ResourceIdentifier::TypeStr[bindings[bd_type]->type]);

		// Instantiate a scheduling contributions manager per binding domain
		if (bd_type == br::Resource::GPU) {
			scms.insert(SchedContribPair_t(
					bd_type,
					new SchedContribManager(
						sc_gpu, *(bindings[bd_type]), 3)));
		}
		else {
			scms.insert(SchedContribPair_t(
					bd_type,
					new SchedContribManager(
						sc_types, *(bindings[bd_type]), YAMS_SC_COUNT)));
		}

		// Next binding domain...
		beg_pos  = end_pos + 1;
	}

	// Register commands
#define CMD_SET_WEIGHTS ".set_weights"
	cmm.RegisterCommand(MODULE_NAMESPACE CMD_SET_WEIGHTS,
		static_cast<CommandHandler*>(this),
		"Set scheduling contributions weights");

#ifdef CONFIG_BBQUE_SP_COWS_BINDING
	// COWS: Init metrics weights
	cows_info.m_weights.resize(COWS_AGGREGATION_WEIGHTS);
	for (int i = 0; i < COWS_AGGREGATION_WEIGHTS; i ++)
		cows_info.m_weights[i] = COWS_TOTAL_WEIGHT_SUM
			/ cows_info.m_weights.size();
	// COWS: Register command for run-time change of weights
#define CMD_COWS_SET_WEIGHTS ".cows.set_weights"
	cmm.RegisterCommand(MODULE_NAMESPACE CMD_COWS_SET_WEIGHTS,
		static_cast<CommandHandler*>(this),
		"Set COWS binding metrics weights");
#endif

	logger->Debug("Binding domain: %d scheduling contrib manager(s)", scms.size());
	return YAMS_SUCCESS;
}

YamsSchedPol::~YamsSchedPol() {

}

YamsSchedPol::ExitCode_t YamsSchedPol::Init() {
	ExitCode_t result;

	// Init a new resource state view
	result = InitResourceStateView();
	if (result != YAMS_SUCCESS)
		return result;

	// Init resource bindings information
	InitBindingInfo();

	// Initialize information for scheduling contributions
	InitSchedContribManagers();

#ifdef CONFIG_BBQUE_SP_COWS_BINDING
	CowsSetup();
#endif

	return YAMS_SUCCESS;
}

YamsSchedPol::ExitCode_t YamsSchedPol::InitResourceStateView() {
	ResourceAccounterStatusIF::ExitCode_t ra_result;
	char token_path[30];

	// Set the resource state view token counter
	++vtok_count;

	// Build a string path for the resource state view
	snprintf(token_path, 30, "%s%d", MODULE_NAMESPACE, vtok_count);
	logger->Debug("Init: Lowest application prio : %d",
			sv->ApplicationLowestPriority());

	// Get a resource state view
	logger->Debug("Init: Requiring state view token for %s", token_path);
	ra_result = ra.GetView(token_path, vtok);
	if (ra_result != ResourceAccounterStatusIF::RA_SUCCESS) {
		logger->Fatal("Init: Cannot get a resource state view");
		return YAMS_ERROR_VIEW;
	}
	logger->Debug("Init: Resources state view token = %d", vtok);

	return YAMS_SUCCESS;
}

YamsSchedPol::ExitCode_t YamsSchedPol::InitBindingInfo() {
	std::map<br::Resource::Type_t, BindingInfo_t *>::iterator bd_it;

	for (bd_it = bindings.begin(); bd_it != bindings.end(); ++bd_it) {
		// Set information for each binding domain
		BindingInfo_t & bd(*(bd_it->second));
		bd.rsrcs = sv->GetResources(bd.domain);
		bd.num   = bd.rsrcs.size();
		bd.ids.resize(bd.num);
		if (bd.num == 0) {
			logger->Warn("Init: No bindings R{%s} available",
					bd.domain.c_str());
			continue;
		}

		// Get all the possible resource binding IDs
		br::ResourcePtrList_t::iterator br_it(bd.rsrcs.begin());
		br::ResourcePtrList_t::iterator br_end(bd.rsrcs.end());
		for (uint8_t j = 0; br_it != br_end; ++br_it, ++j) {
			br::ResourcePtr_t const & rsrc(*br_it);
			bd.ids[j] = rsrc->ID();
			logger->Debug("Init: R{%s} ID: %d",
					bd.domain.c_str(), bd.ids[j]);
		}
		logger->Debug("Init: R{%s}: %d possible bindings",
				bd.domain.c_str(), bd.num);
	}

	return YAMS_SUCCESS;
}

YamsSchedPol::ExitCode_t YamsSchedPol::InitSchedContribManagers() {
	std::map<br::Resource::Type_t, SchedContribManager *>::iterator scm_it;
	SchedContribPtr_t sc_recf;

	// Set the view information into the scheduling contribution managers
	for (scm_it = scms.begin(); scm_it != scms.end(); ++scm_it) {
		SchedContribManager * scm = scm_it->second;
		scm->SetViewInfo(sv, vtok);
		scm->SetBindingInfo(*(bindings[scm_it->first]));
		logger->Debug("Init: Scheduling contribution manager for R{%s} ready",
				br::ResourceIdentifier::TypeStr[scm_it->first]);

		// Init Reconfig contribution
		sc_recf = scm->GetContrib(SchedContribManager::RECONFIG);
		if (sc_recf != nullptr) {
			br::ResID_t first_id = *(bindings[scm_it->first]->ids.begin());
			sc_recf->Init(&first_id);
		}
	}

	return YAMS_SUCCESS;
}


SchedulerPolicyIF::ExitCode_t
YamsSchedPol::Schedule(System & sys_if, br::RViewToken_t & rav) {
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

	// Reset scheduling entities and resource bindings status
	Clear();

	// Report table
	ra.PrintStatusReport(vtok);
	return SCHED_DONE;

error:
	logger->Error("Schedule: an error occurred. Interrupted.");
	Clear();
	ra.PutView(vtok);
	return SCHED_ERROR;
}

inline void YamsSchedPol::Clear() {
	entities.clear();

	// Reset bindings
	std::map<br::Resource::Type_t, BindingInfo_t *>::iterator bd_it;
	for (bd_it = bindings.begin(); bd_it != bindings.end(); ++bd_it) {
		bd_it->second->full.Reset();
	}
}

void YamsSchedPol::SchedulePrioQueue(AppPrio_t prio) {
	bool sched_incomplete;
	uint8_t naps_count = 0;
	std::map<br::Resource::Type_t, SchedContribManager *>::iterator scm_it;
	SchedContribPtr_t sc_fair;

	// Init Fairness contributions
	logger->Debug("Schedule: Init FAIRNESS contributions (if any)");
	for (scm_it = scms.begin(); scm_it != scms.end(); ++scm_it) {
		SchedContribManager * scm = scm_it->second;
		sc_fair = scm->GetContrib(SchedContribManager::FAIRNESS);
		if (sc_fair == nullptr)
			continue;
		sc_fair->Init(&prio);
	}

	// Reset timer
	YAMS_RESET_TIMING(yams_tmr);

do_schedule:
	// Order schedule entities by aggregate metrics
	naps_count = OrderSchedEntities(prio);
	YAMS_GET_TIMING(coll_metrics, YAMS_ORDERING_TIME, yams_tmr);
	YAMS_RESET_TIMING(yams_tmr);

	// Select and schedule the best bound AWM for each application
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
	ba::AppCPtr_t papp;

	// Applications to be scheduled
	papp = sv->GetFirstWithPrio(prio, app_it);
	for (; papp; papp = sv->GetNextWithPrio(prio, app_it)) {
		// Check if the Application/EXC must be skipped
		if (CheckSkipConditions(papp))
			continue;

		// Compute the metrics for each AWM [and binding option]
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
	Application::ExitCode_t app_result = Application::APP_SUCCESS;
	SchedEntityList_t::iterator se_it(entities.begin());
	SchedEntityList_t::iterator end_se(entities.end());
	logger->Debug("=================| Scheduling entities |================"
									   "=");

	// Pick the entity and set the new AWM
	for (; se_it != end_se; ++se_it) {
		SchedEntityPtr_t & pschd(*se_it);

		// Skip if the <Application, AWM> has been already scheduled
		if (CheckSkipConditions(pschd->papp))
			continue;

#ifdef CONFIG_BBQUE_SP_COWS_BINDING
		// COWS: Find the best binding for the AWM of the Application
		CowsBinding(pschd);

		std::multimap<float,int>::reverse_iterator rit;
		for (rit = cows_info.ordered_bds.rbegin();
				rit != cows_info.ordered_bds.rend(); ++rit) {

			// Skip if the CPU is already full
			if (cpu_bindings->full.Test(cpu_bindings->ids[(*rit).second]))
				continue;

			// Do CPU binding
			logger->Info("COWS: Select BD[%d] (metrics=%2.2f)",
					cpu_bindings->ids[(*rit).second], (*rit).first);
			pschd->SetBindingID(
					cpu_bindings->ids[(*rit).second], br::Resource::CPU);
			ExitCode_t bd_result = BindResources(pschd, 0);
			if (bd_result != YAMS_SUCCESS) {
				logger->Error("COWS: CPU binding failed [%d]",
						bd_result);
				break;
			}

			// Send the schedule request
			logger->Debug("Selecting: %s schedule requested", pschd->StrId());
			app_result = pschd->papp->ScheduleRequest(
					pschd->pawm, vtok, pschd->bind_refn);
			if (app_result == ApplicationStatusIF::APP_WM_ACCEPTED){
				logger->Info("COWS: scheduling OK");
				//COWS: Update means and square means values
				CowsUpdateMeans((*rit).second);
				break;
			}
		}

		if (app_result != ApplicationStatusIF::APP_WM_ACCEPTED) {
			logger->Info("All options rejected!", pschd->StrId());
			continue;
		}
#else
		// Send the schedule request
		app_result = pschd->papp->ScheduleRequest(
				pschd->pawm, vtok, pschd->bind_refn);
		logger->Debug("Selecting: %s schedule requested", pschd->StrId());
		if (app_result != ApplicationStatusIF::APP_WM_ACCEPTED) {
			logger->Debug("Selecting: %s rejected!", pschd->StrId());
			continue;
		}
#endif

		if (!pschd->papp->Synching() || pschd->papp->Blocking()) {
			logger->Debug("Selecting: [%s] state %s|%s", pschd->papp->StrId(),
					Application::StateStr(pschd->papp->State()),
					Application::SyncStateStr(pschd->papp->SyncState()));
			continue;
		}
		logger->Notice("Selecting: %s on [%s] SCHEDULED metrics: %.4f",
				pschd->StrId(),
				br::ResourceIdentifier::TypeStr[pschd->bind_type],
				pschd->metrics);

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

void YamsSchedPol::InsertWorkingModes(ba::AppCPtr_t const & papp) {
	std::list<std::thread> awm_thds;
	ba::AwmPtrList_t const * awms = nullptr;
	ba::AwmPtrList_t::const_iterator awm_it;

	// AWMs evaluation (no binding)
	awms = papp->WorkingModes();
	for (awm_it = awms->begin(); awm_it != awms->end(); ++awm_it) {
		ba::AwmPtr_t const & pawm(*awm_it);
		SchedEntityPtr_t pschd(new SchedEntity_t(papp, pawm, R_ID_NONE, 0.0));
#ifdef CONFIG_BBQUE_SP_YAMS_PARALLEL
		awm_thds.push_back(
				std::thread(&YamsSchedPol::EvalWorkingMode, this, pschd)
		);
#else
		EvalWorkingMode(pschd);
#endif
	}

#ifdef CONFIG_BBQUE_SP_YAMS_PARALLEL
	for_each(awm_thds.begin(), awm_thds.end(), mem_fn(&std::thread::join));
	awm_thds.clear();
#endif
	logger->Debug("Eval: number of entities = %d", entities.size());
}

void YamsSchedPol::EvalWorkingMode(SchedEntityPtr_t pschd) {
	std::unique_lock<std::mutex> sched_ul(sched_mtx, std::defer_lock);
	std::map<br::Resource::Type_t, BindingInfo_t *>::iterator bd_it;
	std::map<br::Resource::Type_t, SchedEntityPtr_t> pschd_map;
	std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator next_it;
	std::vector<br::ResID_t>::reverse_iterator ids_it;
	float sc_value   = 0.0;
	uint8_t mlog_len = 0;
	char mlog[255];
	Timer comp_tmr;

	// Skip if the application has been disabled/stopped in the meanwhile
	if (pschd->papp->Disabled()) {
		logger->Debug("EvalAWM: %s disabled/stopped during schedule ordering",
				pschd->papp->StrId());
		return;
	}

	// Metrics computation start
	YAMS_RESET_TIMING(comp_tmr);

	// Aggregate binding-independent scheduling contributions
	for (bd_it = bindings.begin(); bd_it != bindings.end(); ++bd_it) {
		br::Resource::Type_t bd_type = bd_it->first;
		BindingInfo_t & bd(*(bd_it->second));
		br::ResourceBitset r_mask;
		logger->Debug("EvalAWM: current domain: %s",
			br::ResourceIdentifier::TypeStr[bd_type]);

		// Skipping empty binding domains
		r_mask = br::ResourceBinder::GetMask(
				pschd->pawm->RecipeResourceUsages(), bd_type);
		if ((bd.num == 0) || (r_mask.Count() == 0))
			continue;

		// Cumulate the scheduling contributions in the SchedEntity object
		SchedEntityPtr_t pschd_domain(new SchedEntity_t(*pschd.get()));
		pschd_domain->bind_type = bd_type;
		for (uint i = 0; i < YAMS_AWM_SC_COUNT; ++i) {
			GetSchedContribValue(pschd_domain, bd_type, sc_types[i], sc_value);
			pschd_domain->metrics += sc_value;
			mlog_len += sprintf(mlog + mlog_len, "%c:%5.4f, ",
						scms[bd_type]->GetString(
							sc_types[i])[0], sc_value);
		}
		mlog[mlog_len-2] = '\0';
		logger->Info("EvalAWM: %s metrics %s -> %5.4f",
				pschd_domain->StrId(), mlog,
				pschd_domain->metrics);
		// Base evaluation (without bound resources)
		pschd_map[bd_type] = pschd_domain;

#ifdef CONFIG_BBQUE_SP_COWS_BINDING
		// Insert the SchedEntity in the scheduling list
		sched_ul.lock();
		entities.push_back(pschd_domain);
		sched_ul.unlock();
		logger->Info("EvalAWM: %s scheduling metrics = %1.4f [%d]",
				pschd->StrId(), pschd->metrics, entities.size());
	}
#else
	}

	// Evaluate the AWM on bound resources
	EvalDomains(pschd_map.begin(), pschd_map.end(), next_it);
	YAMS_GET_TIMING(coll_metrics, YAMS_METRICS_COMP_TIME, comp_tmr);
#endif
}

void YamsSchedPol::EvalDomains(
		std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator dom_it,
		std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator dom_end,
		std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator & last_it) {
	ExitCode_t result;

	while (1) {
		// Current binding domain: evaluate IDs
		result = EvalBindings(dom_it, dom_end, last_it, nullptr);
		if (result == YAMS_SUCCESS)
			break;
		// Next binding domain
		if (last_it != dom_end)
			++dom_it;
	}
}

YamsSchedPol::ExitCode_t YamsSchedPol::EvalBindings(
		std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator dom_it,
		std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator dom_end,
		std::map<br::Resource::Type_t, SchedEntityPtr_t>::iterator & next_it,
		SchedEntityPtr_t pschd_parent) {
	std::unique_lock<std::mutex> sched_ul(sched_mtx, std::defer_lock);
	std::map<br::Resource::Type_t, BindingInfo_t *>::iterator bd_it;
	std::vector<br::ResID_t>::reverse_iterator ids_it;
	br::Resource::Type_t bd_type(dom_it->first);
	SchedEntityPtr_t pschd_domain(dom_it->second);
	float  sc_value  = 0.0;
	float  base_metr = 0.0;
	size_t base_refn = 0;
	br::ResID_t bd_id;
	ExitCode_t result;

	// Get the BindingInfo of the given resource binding type
	bd_it = bindings.find(bd_type);
	if (bd_it == bindings.end()) {
		logger->Fatal("EvalBindings: Unexpected binding type");
		return YAMS_ERROR;
	}
	BindingInfo_t bd = *(bd_it->second);
	logger->Debug("EvalBindings: [%s] base (AWM) metrics %1.4f",
			br::ResourceIdentifier::TypeStr[bd_type],
			pschd_domain->metrics);

	// Multiple bindings: cumulate metrics and keep track of the binding reference
	if (pschd_parent != nullptr) {
		base_refn = pschd_parent->bind_refn;
		base_metr = pschd_parent->metrics;
	}

	// Binding IDs
	for (ids_it = bd.ids.rbegin(); ids_it != bd.ids.rend(); ++ids_it) {
		next_it = dom_it;
		bd_id   = *ids_it;
		logger->Debug("EvalBindings: [%s] ID = %d",
			br::ResourceIdentifier::TypeStr[bd_type], bd_id);

		// Check resource availability
		if (bd.full[bd_id]) {
			logger->Info("EvalBindings: [%s%d] is full, skipping...",
				bd.domain.c_str(), bd_id);
			continue;
		}

		// Get the scheduling contributions for <AWM, Binding (ID)>
		SchedEntityPtr_t pschd_bound(
				new SchedEntity_t(*pschd_domain.get()));
		pschd_bound->SetBindingID(bd_id, bd_type);
		result = GetBoundContrib(pschd_bound, base_refn, sc_value);
		if (result != YAMS_SUCCESS) {
			logger->Debug("EvalBindings: nothing to bind to [%s]",
				br::ResourceIdentifier::TypeStr[bd_type]);
			return YAMS_IGNORE;
		}

		// Update the metrics value
		pschd_bound->metrics =
			(pschd_bound->metrics + (base_metr + sc_value)) / 2.0;

		// Next binding domain? Go recursively
		if (++next_it != dom_end) {
			logger->Debug("EvalBindings: next domain is [%s]",
				br::ResourceIdentifier::TypeStr[next_it->first]);
			EvalBindings(next_it, dom_end, next_it, pschd_bound);
		}
		else {
			// Insert the SchedEntity in the scheduling list
			sched_ul.lock();
			entities.push_back(pschd_bound);
			sched_ul.unlock();
			logger->Info("EvalBindings: %s scheduling metrics = %1.4f [%d]",
					pschd_bound->StrId(),
					pschd_bound->metrics, entities.size());
		}
	}
	logger->Debug("EvalBindings: [%s] - DONE -",
			br::ResourceIdentifier::TypeStr[dom_it->first]);
	return YAMS_SUCCESS;
}

void YamsSchedPol::GetSchedContribValue(
		SchedEntityPtr_t pschd,
		br::ResourceIdentifier::Type_t bd_type,
		SchedContribManager::Type_t sc_type,
		float & sc_value) {
	SchedContribManager::ExitCode_t scm_ret;
	SchedContrib::ExitCode_t sc_ret;
	EvalEntity_t const & eval_ent(*pschd.get());
	sc_value = 0.0;
	Timer comp_tmr;

	if (scms[bd_type] == nullptr) {
		logger->Error("SchedContrib: Missing resource binding [%s]",
				br::ResourceIdentifier::TypeStr[bd_type]);
		return;
	}

	// Compute the single contribution
	YAMS_RESET_TIMING(comp_tmr);

	scm_ret = scms[bd_type]->GetIndex(sc_type, eval_ent, sc_value, sc_ret);
	if (scm_ret != SchedContribManager::OK) {
		logger->Warn("SchedContrib: return code %d", scm_ret);
		if (scm_ret != SchedContribManager::SC_ERROR) {
			YAMS_RESET_TIMING(comp_tmr);
			return;
		}

		// SchedContrib specific error handling
		switch (sc_ret) {
		case SchedContrib::SC_RSRC_NO_PE:
			logger->Debug("SchedContrib: No available PEs in {%s} %d",
					bindings[bd_type]->domain.c_str(), pschd->bind_id);
			bindings[bd_type]->full.Set(pschd->bind_id);
			return;
		default:
			logger->Warn("SchedContrib: Unable to schedule in {%s} %d [err:%d]",
					bindings[bd_type]->domain.c_str(), pschd->bind_id, sc_ret);
			YAMS_GET_TIMING(coll_mct_metrics, sc_type, comp_tmr);
			return;
		}
	}
	YAMS_GET_TIMING(coll_mct_metrics, sc_type, comp_tmr);
	logger->Debug("SchedContrib: domain:%s, sc:%d",
		br::ResourceIdentifier::TypeStr[bd_type], sc_type);
}

YamsSchedPol::ExitCode_t YamsSchedPol::GetBoundContrib(
		SchedEntityPtr_t pschd_bd,
		size_t b_refn,
		float & value) {
	ExitCode_t result;
	float sc_value   = 0.0;
	uint8_t mlog_len = 0;
	char mlog[255];
	br::Resource::Type_t bd_type = pschd_bd->bind_type;
	logger->Info("GetBoundContrib: =========== BINDING:'%s' ID[%2d ] ===========",
			bindings[bd_type]->domain.c_str(), pschd_bd->bind_id);

	// Bind the resources of the AWM to the given binding domain
	result = BindResources(pschd_bd, b_refn);
	if (result != YAMS_SUCCESS) {
		return result;
	}

	// Aggregate binding-dependent scheduling contributions
	value = 0.0;
	for (uint i = YAMS_AWM_SC_COUNT; i < YAMS_SC_COUNT; ++i) {
		GetSchedContribValue(pschd_bd, bd_type, sc_types[i], sc_value);
		value    += sc_value;
		mlog_len += sprintf(mlog + mlog_len, "%c:%5.4f, ",
					scms[bd_type]->GetString(sc_types[i])[0], sc_value);
	}
	mlog[mlog_len-2] = '\0';
	logger->Info("GetBoundContrib: %s metrics %s -> %5.4f",
			pschd_bd->StrId(), mlog, value);
	logger->Info("GetBoundContrib: ================================================= ");

	return YAMS_SUCCESS;
}

YamsSchedPol::ExitCode_t YamsSchedPol::BindResources(
		SchedEntityPtr_t pschd,
		size_t b_refn) {
	ba::AwmPtr_t & pawm(pschd->pawm);
	br::ResID_t & bd_id(pschd->bind_id);
	br::Resource::Type_t & bd_type(pschd->bind_type);
	size_t r_refn;

	// Binding of the AWM resource into the current binding resource ID.
	// Since the policy handles more than one binding per AWM the resource
	// binding is referenced by a number.
	r_refn = pawm->BindResource(bd_type, R_ID_ANY, bd_id, b_refn);
	logger->Debug("BindResources: reference number %ld", r_refn);

	// The resource binding should never fail
	if (r_refn == 0) {
		logger->Warn("BindResources: AWM{%d} on '%s%d' failed",
			pawm->Id(), br::ResourceIdentifier::TypeStr[bd_type], bd_id);
		return YAMS_ERROR;
	}

	pschd->bind_refn = r_refn;
	logger->Info("BindResources: AWM{%d} to resource '%s' ID=%d [%ld]",
			pawm->Id(), bindings[bd_type]->domain.c_str(),
			bd_id, pschd->bind_refn);

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

#ifdef CONFIG_BBQUE_SP_COWS_BINDING

void YamsSchedPol::CowsSetup() {
	cpu_bindings = bindings[br::Resource::CPU];

	// COWS: Vectors and accumulators resizing depending on the total number
	// of possible bindings for the CPU resource
	cows_info.bd_load.resize(cpu_bindings->num);
	cows_info.bound_mix.resize(cpu_bindings->num);
	cows_info.stalls_metrics.resize(cpu_bindings->num);
	cows_info.iret_metrics.resize(cpu_bindings->num);
	cows_info.flops_metrics.resize(cpu_bindings->num);
	cows_info.migr_metrics.resize(cpu_bindings->num);
	cows_info.perf_data.resize(COWS_RECIPE_METRICS);
	cows_info.norm_stats.resize(COWS_NORMAL_VALUES);

	// COWS: The real accumulator sets
	binding_domains.resize(cpu_bindings->num);
	binding_speculative.resize(cpu_bindings->num);
	binding_empty.resize(cpu_bindings->num);
	syswide_sums.resize(COWS_UNITS_METRICS);
	syswide_empty.resize(COWS_UNITS_METRICS);

	// COWS: Reset the counters
	CowsClear();
	logger->Info("COWS: Support enabled");
}

void YamsSchedPol::CowsBinding(SchedEntityPtr_t pschd) {

	// Clear previous run information
	cows_info.ordered_bds.clear();
	for (int i = 0; i < COWS_NORMAL_VALUES; i++) {
		cows_info.norm_stats[i] = 0;
	}

	CowsInit(pschd);
	CowsBoundMix(pschd);
	CowsUnitsBalance();
	CowsAggregateResults();
}

void YamsSchedPol::CowsUpdateMeans(int logic_index) {
	// A new application has been scheduled
	cows_info.bd_load[logic_index]++;
	cows_info.bd_total_load++;

	// Applying the candidate scheduling entity statistics
	// Update accumulators for the chosen BD
	binding_domains[logic_index].
		llcm_info(cows_info.perf_data[COWS_LLCM]);
	binding_domains[logic_index].
		stalls_info(cows_info.perf_data[COWS_STALLS]);
	binding_domains[logic_index].
		iret_info(cows_info.perf_data[COWS_IRET]);
	binding_domains[logic_index].
		flops_info(cows_info.perf_data[COWS_FLOPS]);
}

void YamsSchedPol::CowsClear() {

	// Clearing the indexes needed to store evaluation results
	for (int i = 0; i < cpu_bindings->num ; i++) {
		cows_info.bd_load[i] = 0;
		cows_info.bound_mix[i] = 0;
		cows_info.stalls_metrics[i] = 0;
		cows_info.iret_metrics[i] = 0;
		cows_info.migr_metrics[i] = 0;
		cows_info.flops_metrics[i] = 0;
	}

	// Clearing my accumulators
	binding_domains = binding_empty;
	binding_speculative = binding_empty;

	// Clearing the indexes needed to normalize evaluation results
	for (int i = 0; i < COWS_NORMAL_VALUES; i++)
		cows_info.norm_stats[i] = 0;
	// Clearing system-wide data
	syswide_sums = syswide_empty;
	cows_info.bd_total_load = 0;
}

YamsSchedPol::ExitCode_t YamsSchedPol::CowsInit(SchedEntityPtr_t pschd) {
	PluginAttrPtr_t plugin_attr;

	// Safety checks
	if (!pschd) {
		logger->Error("COWS: Unexpected null scheduling entity");
		return YAMS_ERROR;
	}
	if (!pschd->pawm) {
		logger->Error("COWS: Unexpected null AWM specified");
		return YAMS_ERROR;
	}

	// Get the metrics parsed from the recipe
	for (uint8_t cm = COWS_STALLS; cm < COWS_MIGRA; ++cm) {
		plugin_attr = std::static_pointer_cast<PluginAttr_t>(
				pschd->pawm->GetAttribute("cows", cows_metrics_str[cm])
		);
		if (plugin_attr != nullptr) {
			cows_info.perf_data[cm] = atoi(plugin_attr->str.c_str());
		}
		else {
			cows_info.perf_data[cm] = 0;
			logger->Warn("COWS: %s  missing '%s' attribute [%d]."
					" Set to 0 by default",
					pschd->pawm->StrId(), cows_metrics_str[cm], cm);
		}
		logger->Info("COWS: %s '%s' = %.2f",
				pschd->StrId(), cows_metrics_str[cm], cows_info.perf_data[cm]);
	}

	return YAMS_SUCCESS;
}

void YamsSchedPol::CowsBoundMix(SchedEntityPtr_t pschd) {
	ExitCode_t result;
	float value;

	logger->Info("COWS: ------------ Bound mix computation -------------");
	logger->Info("COWS: Binding domain(s): %d", cpu_bindings->num);

	// BOUND MIX: compute the delta-variance for each binding domain
	for (int i = 0; i < cpu_bindings->num; i++) {

		// Computing system boundness status 'AS IF' the BD chosen to
		// contain the application is the current BD.
		// Thus, speculativeDomains accumulators are to be exploited.
		binding_speculative = binding_domains;
		binding_speculative[i].llcm_info
			(cows_info.perf_data[COWS_LLCM]);

		// Resetting the bound mix variable, which will contain the
		// boundness scores for each BD.
		cows_info.bound_mix[i] = 0;
		if (cows_info.bd_load[i] != 0) {
			// bound mix = variance (new case) - variance (current case)
			cows_info.bound_mix[i] =
				variance(binding_speculative[i].llcm_info) -
				variance(binding_domains[i].llcm_info);

			// Only positive contributions are used to normalize
			if (cows_info.bound_mix[i] > 0)
				cows_info.norm_stats[COWS_LLCM] +=
					cows_info.bound_mix[i];
		}
		else {
			// If the binding domain is empty, it is considered as a binding
			// domain containing an application with 0 llcm/cycle.
			// Thus, the resulting variance is X^2/4
			cows_info.bound_mix[i] =
				(cows_info.perf_data[COWS_LLCM] *
				cows_info.perf_data[COWS_LLCM])/4;
			cows_info.norm_stats[COWS_LLCM] +=
					cows_info.bound_mix[i];
		}

		logger->Info("COWS: Bound mix @BD[%d] for %s: %3.2f",
				cpu_bindings->ids[i], pschd->StrId(), cows_info.bound_mix[i]);

		// Set the binding ID
		pschd->SetBindingID(cpu_bindings->ids[i], br::Resource::CPU);
		result = BindResources(pschd, 0);
		if (result != YAMS_SUCCESS) {
			logger->Error("COWS: Resource binding failed [%d]", result);
		}

		// Get migration contribution
		value = 0.0;
		GetSchedContribValue(
				pschd, br::Resource::CPU, SchedContribManager::MIGRATION, value);
		cows_info.migr_metrics[i] = value;
		cows_info.norm_stats[COWS_MIGRA] += value;
	}

	// Normalization statistics initialization for already
	// collected metrics
	if (cows_info.norm_stats[COWS_LLCM] == 0) cows_info.norm_stats[COWS_LLCM]++;
	if (cows_info.norm_stats[COWS_MIGRA] == 0) cows_info.norm_stats[COWS_MIGRA]++;
}

void YamsSchedPol::CowsUnitsBalance() {
	logger->Info("COWS: ---------- Functional units balance ------------");

	// Update system-wide allocated resources amount
	for (int i = 0; i < cpu_bindings->num; i++) {
		syswide_sums[COWS_STALLS](sum(binding_domains[i].stalls_info));
		syswide_sums[COWS_IRET  ](sum(binding_domains[i].iret_info));
		syswide_sums[COWS_FLOPS ](sum(binding_domains[i].flops_info));
	}

	// For each binding domain, calculate the updated means AS IF
	// I scheduled the new app there, then calculate the corresponding
	// standard deviation
	for (int i = 0; i < cpu_bindings->num; i++) {
		logger->Info("COWS: Computing units balance for BD[%d]...",
				cpu_bindings->ids[i]);

		// Calculating standard deviations (squared). Again, if I'm on
		// BD i, the mean has changed
		for (int j = 0; j < cpu_bindings->num; j++) {

			float dist_from_avg_stalls =
					sum(binding_domains[j].stalls_info) -
					(mean(syswide_sums[COWS_STALLS]) +
					cows_info.perf_data[COWS_STALLS]/cpu_bindings->num);
				float dist_from_avg_iret =
					sum(binding_domains[j].iret_info) -
					(mean(syswide_sums[COWS_IRET]) +
					cows_info.perf_data[COWS_IRET]/cpu_bindings->num);
				float dist_from_avg_flops =
					sum(binding_domains[j].flops_info) -
					(mean(syswide_sums[COWS_FLOPS]) +
					cows_info.perf_data[COWS_FLOPS]/cpu_bindings->num);

			if (j == i) {
				dist_from_avg_stalls +=
					cows_info.perf_data[COWS_STALLS];
				dist_from_avg_iret +=
					cows_info.perf_data[COWS_IRET];
				dist_from_avg_flops +=
					cows_info.perf_data[COWS_FLOPS];

				cows_info.stalls_metrics[i] +=
					dist_from_avg_stalls * dist_from_avg_stalls;
				cows_info.iret_metrics[i] +=
					dist_from_avg_iret * dist_from_avg_iret;
				cows_info.flops_metrics[i] +=
					dist_from_avg_flops * dist_from_avg_flops;
			}
			else if (cows_info.bd_load[j] != 0) {
				cows_info.stalls_metrics[i] +=
					dist_from_avg_stalls * dist_from_avg_stalls;
				cows_info.iret_metrics[i] +=
					dist_from_avg_iret * dist_from_avg_iret;
				cows_info.flops_metrics[i] +=
					dist_from_avg_flops * dist_from_avg_flops;
			}
		}

		logger->Info("COWS: Total stalls quadratic deviation in BD  %d: %3.2f",
				cpu_bindings->ids[i], cows_info.stalls_metrics[i]);
		logger->Info("COWS: Total ret. instructions deviation in BD %d: %3.2f",
				cpu_bindings->ids[i], cows_info.iret_metrics[i]);
		logger->Info("COWS: Total X87 operations deviation in BD %d: %3.2f",
				cpu_bindings->ids[i], cows_info.flops_metrics[i]);
		logger->Info("COWS: Proceeding with next BD, if any ...");

		cows_info.norm_stats[COWS_STALLS] += cows_info.stalls_metrics[i];
		cows_info.norm_stats[COWS_IRET]   += cows_info.iret_metrics[i];
		cows_info.norm_stats[COWS_FLOPS]  += cows_info.flops_metrics[i];
	}
}

void YamsSchedPol::CowsAggregateResults() {
	float result = 0.0;
	logger->Info("COWS: ----------- Results aggregation ------------");

	if (cows_info.norm_stats[COWS_STALLS] == 0) cows_info.norm_stats[COWS_STALLS]++;
	if (cows_info.norm_stats[COWS_IRET] == 0) cows_info.norm_stats[COWS_IRET]++;
	if (cows_info.norm_stats[COWS_FLOPS] == 0) cows_info.norm_stats[COWS_FLOPS]++;

	// Normalizing
	logger->Info(" ======================================================"
			"==================");
	for (int i = 0; i < cpu_bindings->num; i++) {
		cows_info.stalls_metrics[i] /= cows_info.norm_stats[COWS_STALLS];
		cows_info.iret_metrics[i]   /= cows_info.norm_stats[COWS_IRET];
		cows_info.flops_metrics[i]  /= cows_info.norm_stats[COWS_FLOPS];
		cows_info.migr_metrics[i]   /= cows_info.norm_stats[COWS_MIGRA];

		if(cows_info.bound_mix[i] < 0) {
			cows_info.bound_mix[i] = 0;
		}
		else {
			cows_info.bound_mix[i] /= cows_info.norm_stats[COWS_LLCM];
		}

		logger->Info("| BD %d | Bound: %3.2f | Stalls:%3.2f | "
				"Ret:%3.2f | Flops:%3.2f | Migrat:%3.2f |",
				cpu_bindings->ids[i],
				cows_info.bound_mix[i],
				cows_info.stalls_metrics[i],
				cows_info.iret_metrics[i],
				cows_info.flops_metrics[i],
				cows_info.migr_metrics[i]);
	}
	logger->Info(" ======================================================"
			"==================");

	// Order the binding domains for the current <Application, AWM>
	for (int i = 0; i < cpu_bindings->num; i++) {
		result =
		// (W1*BOUNDNESS) - [W2*(ST + RET + FLOPS)] + (W3*MIGRATION)
		    cows_info.m_weights[COWS_BOUND_WEIGHT] * cows_info.bound_mix[i]
			-
			cows_info.m_weights[COWS_UNITS_WEIGHT] * (
				  cows_info.stalls_metrics[i]  +
				  cows_info.iret_metrics[i] +
				  cows_info.flops_metrics[i])
			+
			cows_info.m_weights[COWS_MIGRA_WEIGHT] * cows_info.migr_metrics[i];

		cows_info.ordered_bds.insert( std::pair<float,int>(result,i) );
	}
	logger->Info("COWS: Ordering binding domains");
	std::multimap<float,int>::reverse_iterator rit;
	for (rit = cows_info.ordered_bds.rbegin();
		rit != cows_info.ordered_bds.rend(); ++rit)
		logger->Info("--- BD: %d, Value: %f",
				cpu_bindings->ids[(*rit).second], (*rit).first);

	logger->Info("COWS: Performance counters: %3.2f, %3.2f, %3.2f, %3.2f",
			cows_info.perf_data[COWS_LLCM],
			cows_info.perf_data[COWS_STALLS],
			cows_info.perf_data[COWS_IRET],
			cows_info.perf_data[COWS_FLOPS]);
	logger->Info("==========|          COWS: Done             |==========");
}

int YamsSchedPol::CowsCommandsHandler(int argc, char * argv[]) {
	float w_sum = 0.0;
	uint8_t cmd_offset = ::strlen(MODULE_NAMESPACE) + strlen(".cows.");

	// Check number of command arguments
	if (argc != 4) {
		logger->Error("'cows.set_weights' expecting 3 parameters (possibly summing up to 10");
		logger->Error("Usage example: bq.sp.yams.cows.set_weights 5 2 3");
		return 1;
	}

	switch (argv[0][cmd_offset]) {
	// set_weigths
	case 's':
		for (int i = 1; i < COWS_AGGREGATION_WEIGHTS + 1; i++)
			w_sum += atof(argv[i]);

		// Normalizing. The inputs should sum up to COWS_TOTAL_WEIGHT_SUM
		if (w_sum != COWS_TOTAL_WEIGHT_SUM)
			logger->Info("COWS: weights sum up to %f. Normalizing...", w_sum);

		logger->Info(" ================================================= ");
		logger->Info("|                   Old weights                   |");
		logger->Info("|=========+=========+=========+=========+=========|");
		logger->Info("|  Bound  |  Stall  & Retired &  Flops  |  Recon  |");
		logger->Info("|=========+=========+=========+=========+=========|");
		logger->Info("|  %3.3f  |            %3.3f            |  %3.3f  |",
					cows_info.m_weights[COWS_BOUND_WEIGHT],
					cows_info.m_weights[COWS_UNITS_WEIGHT],
					cows_info.m_weights[COWS_MIGRA_WEIGHT]);
		logger->Info(" =========+=========+=========+=========+========= ");

		for (int i = 0; i < COWS_AGGREGATION_WEIGHTS; i++) {
			cows_info.m_weights[i] =
				COWS_TOTAL_WEIGHT_SUM * (atof(argv[i + 1])) / w_sum;
		}

		logger->Info(" ================================================= ");
		logger->Info("|                   New weights                   |");
		logger->Info("|=========+=========+=========+=========+=========|");
		logger->Info("|  Bound  |  Stall  & Retired &  Flops  |  Recon  |");
		logger->Info("|=========+=========+=========+=========+=========|");
		logger->Info("|  %3.3f  |            %3.3f            |  %3.3f  |",
					cows_info.m_weights[COWS_BOUND_WEIGHT],
					cows_info.m_weights[COWS_UNITS_WEIGHT],
					cows_info.m_weights[COWS_MIGRA_WEIGHT]);
		logger->Info(" =========+=========+=========+=========+========= ");
		break;
	default:
		logger->Warn("Commands: unknown command '%s'", argv[0]);
	}

	return 0;
}
#endif // CONFIG_BBQUE_SP_COWS_BINDING

int YamsSchedPol::CommandsCb(int argc, char *argv[]) {
	uint8_t cmd_offset = ::strlen(MODULE_NAMESPACE) + 1;
	logger->Debug("Processing command [%s]", argv[0] + cmd_offset);
#ifdef CONFIG_BBQUE_SP_COWS_BINDING
	size_t cmd_cows_pref_len = ::strlen(MODULE_NAMESPACE""".cows");
	if (strncmp(argv[0],
				MODULE_NAMESPACE""".cows",
				cmd_cows_pref_len) == 0) {
		logger->Debug("'%s' is a COWS command", argv[0]);
		return CowsCommandsHandler(argc, argv);
	}
#endif // CONFIG_BBQUE_SP_COWS_BINDING

	switch (argv[0][cmd_offset]) {
	case 's':
		logger->Info("Commands: Reconfigure scheduling contributions weights");
		if (argc-1 != YAMS_SC_COUNT) {
			logger->Error("Commands: %d of %d weights specified", argc-1, YAMS_SC_COUNT);
			logger->Error("\t Usage: %s 7 2 3 2", argv[0]);
			logger->Error("\t Usage: %s 7 - 3 2", argv[0]);
			return 1;
		}
		ReconfigSchedContribWeights(argc-1, argv);
		break;
	}

	return 0;
}

void YamsSchedPol::ReconfigSchedContribWeights(
		uint8_t num_weights,
		char * set_weights[]) {
	std::map<br::Resource::Type_t, SchedContribManager *>::iterator scm_it;
	uint16_t * new_weights = nullptr;
	uint16_t * old_weights = nullptr;
	logger->Debug("ReconfigSchedContribWeights: %d weights...", num_weights);

	for (scm_it = scms.begin(); scm_it != scms.end(); ++scm_it) {
		SchedContribManager * scm = scm_it->second;
		logger->Debug("ReconfigSchedContribWeights: BD[%s] %d weights...",
				br::ResourceIdentifier::TypeStr[scm_it->first],
				scm->GetNumMax());

		// Update starting from the old set of weights
		new_weights = (uint16_t *) malloc(sizeof(uint16_t)*scm->GetNumMax());
		old_weights = scm->GetWeights();
		if (old_weights == nullptr) {
			logger->Error("ReconfigSchedContribWeights: missing old weights");
			return;
		}
		for (uint8_t i = 0;  i < scm->GetNumMax(); ++i) {
			if ((i >= num_weights) || (strncmp(set_weights[i+1], "-", 1) == 0))
				new_weights[i] = old_weights[i];
			else
				new_weights[i] = atoi(set_weights[i+1]);
			logger->Info("ReconfigSchedContribWeights: [%11s]  %2d -> %2d ",
					scm->GetString(sc_types[i]),
					old_weights[i], new_weights[i]);
		}
		// Set the new weights
		scm->SetWeights(new_weights);
		free(new_weights);
	}
}

} // namespace plugins

} // namespace bbque
