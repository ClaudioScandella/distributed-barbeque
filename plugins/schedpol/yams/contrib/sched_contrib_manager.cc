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


#include <cstring>
#include <numeric>

#include "bbque/modules_factory.h"
#include "sched_contrib_manager.h"

// Binding independent
#include "sc_value.h"
#include "sc_reconfig.h"
#include "sc_fairness.h"
// Binding dependent
#include "sc_congestion.h"
#include "sc_migration.h"
// ...:: ADD_SC ::...

namespace po = boost::program_options;

namespace bbque { namespace plugins {

/*****************************************************************************
 *                         Static data initialization                        *
 *****************************************************************************/


bool SchedContribManager::config_ready = false;

char const * SchedContribManager::sc_str[SC_COUNT] = {
	"awmvalue",
	"reconfig",
	"fairness",
	"congestion",
	"migration"
	//"power",
	//"thermal",
	//"stability",
	//"robustness"
	// ...:: ADD_SC ::...
};

std::map<SchedContribManager::Type_t,
	SchedContribPtr_t> SchedContribManager::sc_objs  = {};
float SchedContribManager::sc_weights_norm[SC_COUNT] = {0};
uint16_t SchedContribManager::sc_weights[SC_COUNT]   = {0};
uint16_t
	SchedContribManager::sc_cfg_params[SchedContrib::SC_CONFIG_COUNT*Resource::TYPE_COUNT] = {
	0};


/*****************************************************************************
 *                       Public member functions                             *
 *****************************************************************************/

SchedContribManager::SchedContribManager(
		Type_t const * sc_types,
		SchedulerPolicyIF::BindingInfo_t const & _bd_info,
		uint8_t sc_num):
	cm(ConfigurationManager::GetInstance()),
	bd_info(_bd_info) {

	// Get a logger
	plugins::LoggerIF::Configuration conf(MODULE_NAMESPACE);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	if (logger)
		logger->Info("Built a new dynamic object[%p]", this);
	else
		fprintf(stderr, FI("%s: Built new dynamic object [%p]\n"),
				SC_MANAGER_NAMESPACE, (void *)this);

	// Parse the configuration parameters
	if (!SchedContribManager::config_ready) {
		ParseConfiguration();
		NormalizeWeights();
		AllocateContribs();
		SchedContribManager::config_ready = true;
	}

	// Init the map of scheduling contributions required
	for (int i = 0; i < sc_num; ++i) {
		switch (sc_types[i]) {
		case VALUE:
			sc_objs_reqs[VALUE] =
				SchedContribManager::sc_objs[VALUE];
			break;
		case RECONFIG:
			sc_objs_reqs[RECONFIG] =
				SchedContribManager::sc_objs[RECONFIG];
			break;
		case FAIRNESS:
			sc_objs_reqs[FAIRNESS] =
				SchedContribManager::sc_objs[FAIRNESS];
			break;
		case CONGESTION:
			sc_objs_reqs[CONGESTION] =
				SchedContribManager::sc_objs[CONGESTION];
			break;
		case MIGRATION:
			sc_objs_reqs[MIGRATION] =
				SchedContribManager::sc_objs[MIGRATION];
			break;
		default:
			logger->Error("Scheduling contribution unknown: %d", sc_types[i]);
		}
	}
}

SchedContribManager::~SchedContribManager() {
	sc_objs.clear();
}

SchedContribManager::ExitCode_t SchedContribManager::GetIndex(
		Type_t sc_type,
		SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & sc_value,
		SchedContrib::ExitCode_t & sc_ret,
		bool weighed) {
	SchedContribPtr_t psc;
	logger->Debug("GetIndex: requiring contribution %d", sc_type);

	// Boundary check
	if (sc_type >= SC_COUNT) {
		logger->Warn("GetIndex: unexpected contribution type (%d)", sc_type);
		return SC_TYPE_UNKNOWN;
	}

	// Return zero if weight is null
	if (weighed && (unlikely(sc_weights_norm[sc_type] == 0))) {
		sc_value = 0;
		return OK;
	}

	// Get the SchedContrib object
	psc = GetContrib(sc_type);
	if (!psc) {
		logger->Warn("GetIndex: contribution type (%d) not available", sc_type);
		return SC_TYPE_MISSING;
	}

	// Compute the SchedContrib index
	sc_ret = psc->Compute(evl_ent, sc_value);
	if (unlikely(sc_ret != SchedContrib::SC_SUCCESS)) {
		logger->Error("GetIndex: error in contribution %d. Return code:%d",
				sc_type, sc_ret);
		return SC_ERROR;
	}

	// Multiply the index for the weight
	if (weighed)
		sc_value *= sc_weights_norm[sc_type];

	logger->Debug("GetIndex: computed contribution %d", sc_type);
	return OK;
}

SchedContribPtr_t SchedContribManager::GetContrib(Type_t sc_type) {
	std::map<Type_t, SchedContribPtr_t>::iterator sc_it;

	// Find the SchedContrib object
	sc_it = sc_objs_reqs.find(sc_type);
	if (sc_it == sc_objs_reqs.end())
		return SchedContribPtr_t();

	return (*sc_it).second;
}

void SchedContribManager::SetViewInfo(System * sv, RViewToken_t vtok) {
	std::map<Type_t, SchedContribPtr_t>::iterator sc_it;

	// For each SchedContrib set the resource view information 
	for (sc_it = sc_objs_reqs.begin(); sc_it != sc_objs_reqs.end(); ++sc_it) {
		SchedContribPtr_t & psc(sc_it->second);
		psc->SetViewInfo(sv, vtok);
		logger->Debug("SetViewInfo: view %d set into %s", vtok, psc->Name());
	}
}

void SchedContribManager::SetBindingInfo(
		SchedulerPolicyIF::BindingInfo_t & _bd_info) {
	std::map<Type_t, SchedContribPtr_t>::iterator sc_it;

	// Set/update the current binding information
	bd_info = _bd_info;

	// For each SchedContrib set the resource view information
	for (sc_it = sc_objs_reqs.begin(); sc_it != sc_objs_reqs.end(); ++sc_it) {
		SchedContribPtr_t & psc(sc_it->second);
		psc->SetBindingInfo(bd_info);
		logger->Debug("SetBindingInfo: updated");
	}
}


/*****************************************************************************
 *                       Private member functions                            *
 *****************************************************************************/


void SchedContribManager::ParseConfiguration() {
	char weig_opts[SC_COUNT][40];
	char conf_opts[SchedContrib::SC_CONFIG_COUNT*Resource::TYPE_COUNT][40];
	uint16_t offset;

	// Load the weights of the metrics contributes
	po::options_description opts_desc("Scheduling contributions parameters");
	for (int i = 0; i < SC_COUNT; ++i) {
		snprintf(weig_opts[i], 40, MODULE_CONFIG".%s.weight", sc_str[i]);
		logger->Debug("%s", weig_opts[i]);
		opts_desc.add_options()
			(weig_opts[i],
			 po::value<uint16_t> (&sc_weights[i])->default_value(0),
			"Single contribution weight");
		;
	}

	// Global configuration parameters
	for (int j = 0; j < SchedContrib::SC_CONFIG_COUNT; ++j) {
		offset = j * ResourceIdentifier::TYPE_COUNT;
		// 1. Maximum saturation levels (MSL)
		for (int i = 1; i < ResourceIdentifier::TYPE_COUNT; ++i) {
			snprintf(conf_opts[i+offset], 30, SC_CONF_BASE_STR"%s.%s",
					SchedContrib::ConfigParamsStr[j],
					ResourceIdentifier::TypeStr[i]);
			opts_desc.add_options()
				(conf_opts[i+offset],
				 po::value<uint16_t>
				 (&sc_cfg_params[i+offset])->default_value(
					 SchedContrib::ConfigParamsDefault[j]),
				"Maximum saturation levels");
			;
		}
	}
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// MSL boundaries enforcement (0 <= MSL <= 100)
	for (int i = 1; i < ResourceIdentifier::TYPE_COUNT; ++i) {
		offset = SchedContrib::SC_MSL * ResourceIdentifier::TYPE_COUNT;
		logger->Debug("%s: %d",	conf_opts[i+offset],sc_cfg_params[i+offset]);
		if (sc_cfg_params[i] > 100) {
			logger->Warn("'%s' out of range [0,100]: found %d. Setting to %d",
					conf_opts[i+offset],
					sc_cfg_params[i+offset],
					SchedContrib::ConfigParamsDefault[SchedContrib::SC_MSL]);
			sc_cfg_params[i] =
				SchedContrib::ConfigParamsDefault[SchedContrib::SC_MSL];
		}
	}
}

void SchedContribManager::NormalizeWeights() {
	uint16_t sum = 0;

	// Accumulate (sum) the weights
	sum = std::accumulate(sc_weights, sc_weights + SC_COUNT, 0);

	// Normalize
	for (int i = 0; i < SC_COUNT; ++i) {
		sc_weights_norm[i] = sc_weights[i] / (float) sum;
		logger->Debug("Contribution [%.*s] weight \t= %.3f", 5,
				sc_str[i], sc_weights_norm[i]);
	}
}

void SchedContribManager::AllocateContribs() {
	// Init the map of scheduling contribution objects
	sc_objs[VALUE] = SchedContribPtr_t(
			new SCValue(sc_str[VALUE], bd_info, sc_cfg_params));
	sc_objs[RECONFIG] = SchedContribPtr_t(
			new SCReconfig(sc_str[RECONFIG], bd_info, sc_cfg_params));
	sc_objs[CONGESTION] = SchedContribPtr_t(
			new SCCongestion(sc_str[CONGESTION], bd_info, sc_cfg_params));
	sc_objs[FAIRNESS] = SchedContribPtr_t(
			new SCFairness(sc_str[FAIRNESS], bd_info, sc_cfg_params));
	sc_objs[MIGRATION] = SchedContribPtr_t(
			new SCMigration(sc_str[MIGRATION], bd_info, sc_cfg_params));
	// ...:: ADD_SC ::...
}

} // namespace plugins

} // namespace bbque
