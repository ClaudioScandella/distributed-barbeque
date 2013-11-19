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
		SchedulerPolicyIF::BindingInfo_t const & _bd_info,
		uint16_t const cfg_params[]):
	SchedContrib(_name, _bd_info, cfg_params) {
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
	char r_path_str[20];
	uint64_t bd_r_avail;
	AppPrio_t priority;
	std::vector<ResID_t>::iterator ids_it;
	std::list<Resource::Type_t>::iterator type_it;

	// Applications/EXC to schedule, given the priority level
	priority = *(static_cast<AppPrio_t *>(params));
	num_apps = sv->ApplicationsCount(priority);
	r_types  = sv->ResourceTypesList();
	logger->Debug("Priority [%d]:  %d applications", priority, num_apps);
	logger->Debug("Bindings [%s]:  %d", bd_info.domain.c_str(), bd_info.num);
	logger->Debug("Resource types: %d", r_types.size());

	// For each resource type get the availability and the fair partitioning
	// among the application having the same priority
	type_it = r_types.begin();
	for (; type_it != r_types.end(); ++type_it) {
		snprintf(r_path_str, 20, "%s.%s",
				bd_info.domain.c_str(),
				ResourceIdentifier::TypeStr[*type_it]);

		// Look for the binding domain with the lowest availability
		r_avail[*type_it] = sv->ResourceAvailable(r_path_str, vtok);
		min_bd_r_avail[*type_it] = r_avail[*type_it];
		max_bd_r_avail[*type_it] = 0;

		ids_it = bd_info.ids.begin();
		for (; ids_it != bd_info.ids.end(); ++ids_it) {
			ResID_t bd_id = *ids_it;
			snprintf(r_path_str, 20, "%s%d.%s",
					bd_info.domain.c_str(),
					bd_id,
					ResourceIdentifier::TypeStr[*type_it]);
			bd_r_avail = sv->ResourceAvailable(r_path_str, vtok);
			logger->Debug("R{%s} availability: % " PRIu64,
					r_path_str, bd_r_avail);

			// Update (?) the min availability value
			if (bd_r_avail < min_bd_r_avail[*type_it]) {
				min_bd_r_avail[*type_it] = bd_r_avail;
				logger->Debug("R{%s} min availability of %s\t: %" PRIu64,
						r_path_str,
						ResourceIdentifier::TypeStr[*type_it],
						min_bd_r_avail[*type_it]);
			}

			// Update (?) the max availability value
			if (bd_r_avail > max_bd_r_avail[*type_it]) {
				max_bd_r_avail[*type_it] = bd_r_avail;
				logger->Debug("R{%s} max availability of %s\t: %" PRIu64,
						r_path_str,
						ResourceIdentifier::TypeStr[*type_it],
						max_bd_r_avail[*type_it]);
			}
		}

		// System-wide fair partition
		fair_pt[*type_it] = max_bd_r_avail[*type_it] / num_apps;
		logger->Debug("R{%s} maxAV: %" PRIu64 " fair partition: %" PRIu64,
				r_path_str, max_bd_r_avail[*type_it], fair_pt[*type_it]);
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
	uint64_t bd_fract;
	uint64_t bd_fair_pt;
	ctrib = 1.0;

	// Fixed function parameters
	params.k = 1.0;
	params.exp.base = expbase;

	// Iterate the whole set of resource usage
	for_each_recp_resource_usage(evl_ent, usage_it) {
		UsagePtr_t const & pusage(usage_it->second);
		ResourcePathPtr_t const & r_path(usage_it->first);

		// Binding domain fraction (resource type related)
		logger->Debug("%s: R{%s} BD{'%s'} max: %" PRIu64 " fair: %d",
				evl_ent.StrId(), r_path->ToString().c_str(),
				bd_info.domain.c_str(),
				max_bd_r_avail[r_path->Type()],
				fair_pt[r_path->Type()]);
		bd_fract = ceil(
				max_bd_r_avail[r_path->Type()] /
				fair_pt[r_path->Type()]);
		logger->Debug("%s: R{%s} BD{'%s'} fraction: %d",
				evl_ent.StrId(), r_path->ToString().c_str(),
				bd_info.domain.c_str(), bd_fract);
		bd_fract == 0 ? bd_fract = 1 : bd_fract;

		// Binding domain fair partition
		bd_fair_pt = max_bd_r_avail[r_path->Type()] / bd_fract;
		if (bd_info.num > 1)
			bd_fair_pt = std::max(min_bd_r_avail[r_path->Type()], bd_fair_pt);
		logger->Debug("%s: R{%s} BD{'%s'} fair partition: %" PRIu64 "",
				evl_ent.StrId(), r_path->ToString().c_str(),
				bd_info.domain.c_str(), bd_fair_pt);

		// Set last parameters for index computation
		penalty = static_cast<float>(penalties_int[r_path->Type()]) / 100.0;
		SetIndexParameters(
				bd_fair_pt, max_bd_r_avail[r_path->Type()],
				penalty, params);

		// Compute the region index
		ru_index = CLEIndex(0, bd_fair_pt, pusage->GetAmount(), params);
		logger->Debug("%s: R{%s} fairness index = %.4f",
				evl_ent.StrId(), r_path->ToString().c_str(), ru_index);

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
