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

#include "sc_fairness.h"

#include <cmath>


namespace po = boost::program_options;

namespace bbque { namespace plugins {


SCFairness::SCFairness(
		const char * _name,
		std::string const & b_domain,
		uint16_t const cfg_params[]):
	SchedContrib(_name, b_domain, cfg_params) {
	char conf_str[50];

	// Configuration parameters
	po::options_description opts_desc("Fairness contribute parameters");

	// Base for exponential
	snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.expbase", name);
	opts_desc.add_options()
		(conf_str,
		 po::value<uint16_t>(&expbase)->default_value(SC_FAIR_DEFAULT_EXPBASE),
		 "Base for exponential function");
		;

	// Fairness penalties
	for (int i = ResourceIdentifier::GROUP;
			 i < ResourceIdentifier::TYPE_COUNT; ++i) {
		snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.penalty.%s",
				name, ResourceIdentifier::TypeStr[i]);
		opts_desc.add_options()
			(conf_str,
			 po::value<uint16_t>
				(&penalties_int[i])->default_value(SC_FAIR_DEFAULT_PENALTY),
			 "Fairness penalty per resource");
		;
	}
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Boundaries enforcement (0 <= penalty <= 100)
	for (int i = ResourceIdentifier::GROUP;
			 i < ResourceIdentifier::TYPE_COUNT; ++i) {
		if (penalties_int[i] > 100) {
			logger->Warn("penalty.%s out of range [0,100]: "
					"found %d. Setting to %d",
					ResourceIdentifier::TypeStr[i],
					penalties_int[i], SC_FAIR_DEFAULT_PENALTY);
			penalties_int[i] = SC_FAIR_DEFAULT_PENALTY;
		}
		logger->Debug("penalty.%-3s: %.2f",
				ResourceIdentifier::TypeStr[i],
				static_cast<float>(penalties_int[i]) / 100.0);
	}
}

SCFairness::~SCFairness() {
}

SchedContrib::ExitCode_t SCFairness::Init(void * params) {
	char rsrc_path_str[20];
	// Applications/EXC to schedule, given the priority level
	AppPrio_t * prio = static_cast<AppPrio_t *>(params);
	num_apps = sv->ApplicationsCount(*prio);
	logger->Debug("Applications/EXCs: Prio[%d] #:%d", *prio, num_apps);

	// For each resource type get the availability and the fair partitioning
	// among the application having the same priority
	for (int i = ResourceIdentifier::GROUP;
			 i < ResourceIdentifier::TYPE_COUNT; ++i) {
		snprintf(rsrc_path_str, 20, "%s.%s",
				binding_domain.c_str(), ResourceIdentifier::TypeStr[i]);
		rsrc_avail[i] = sv->ResourceAvailable(rsrc_path_str, vtok);
		fair_parts[i] = rsrc_avail[i] / num_apps;
		logger->Debug("R{%s} availability:%" PRIu64 " fair partition:%"	PRIu64,
				rsrc_path_str, rsrc_avail[i], fair_parts[i]);
	}

	return SC_SUCCESS;
}

SchedContrib::ExitCode_t
SCFairness::_Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {
	UsagesMap_t::const_iterator usage_it;
	CLEParams_t params;
	float ru_index;
	float penalty;
	uint64_t bd_rsrc_avail;
	uint64_t bd_fract;
	uint64_t bd_fair_part;
	ctrib = 1.0;

	// Fixed function parameters
	params.k = 1.0;
	params.exp.base = expbase;

	// Iterate the whole set of resource usage
	for_each_sched_resource_usage(evl_ent, usage_it) {
		UsagePtr_t const & pusage(usage_it->second);
		ResourcePathPtr_t const & rsrc_path(usage_it->first);
		ResourcePtrList_t & rsrc_bind(pusage->GetBindingList());

		// Resource availability (already bound)
		bd_rsrc_avail = sv->ResourceAvailable(rsrc_bind, vtok);
		logger->Debug("%s: R{%s} availability: %" PRIu64 "",
				evl_ent.StrId(), rsrc_path->ToString().c_str(), bd_rsrc_avail);

		// If there are no free resources the index contribute is equal to 0
		if (bd_rsrc_avail < pusage->GetAmount()) {
			ctrib = 0;
			return SC_SUCCESS;
		}

		// Binding domain fraction (resource type related)
		bd_fract = ceil(
				static_cast<double>(bd_rsrc_avail) /
				fair_parts[rsrc_path->Type()]);
		logger->Debug("%s: R{%s} BD{'%s'} fraction: %" PRIu64 "",
				evl_ent.StrId(), rsrc_path->ToString().c_str(),
				binding_domain.c_str(), bd_fract);
		bd_fract == 0 ? bd_fract = 1 : bd_fract;

		// Binding domain fair partition
		bd_fair_part =
			std::min<uint64_t>(bd_rsrc_avail, bd_rsrc_avail / bd_fract);
		logger->Debug("%s: R{%s} BD{'%s'} fair partition: %" PRIu64 "",
				evl_ent.StrId(), rsrc_path->ToString().c_str(),
				binding_domain.c_str(), bd_fair_part);

		// Set last parameters for index computation
		penalty = static_cast<float>(penalties_int[rsrc_path->Type()]) / 100.0;
		SetIndexParameters(bd_fair_part, bd_rsrc_avail, penalty, params);

		// Compute the region index
		ru_index = CLEIndex(0, bd_fair_part, pusage->GetAmount(), params);
		logger->Debug("%s: R{%s} fairness index = %.4f", evl_ent.StrId(),
				rsrc_path->ToString().c_str(), ru_index);

		// Update the contribution if the index is lower, i.e. the most
		// penalizing request dominates
		ru_index < ctrib ? ctrib = ru_index: ctrib;
	}

	return SC_SUCCESS;
}

void SCFairness::SetIndexParameters(
		uint64_t bfp,
		uint64_t bra,
		float & penalty,
		CLEParams_t & params) {
	// Linear parameters
	params.lin.xoffset = 0.0;
	params.lin.scale   = penalty / static_cast<float>(bfp);

	// Exponential parameters
	params.exp.yscale  = (1.0 - penalty) / (params.exp.base - 1.0);
	params.exp.xscale  = static_cast<float>(bfp) - static_cast<float>(bra);
	params.exp.xoffset = static_cast<float>(bra);
}


} // namespace plugins

} // namespace bbque
