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
		BindingInfo_t const & _bd_info,
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
	int r_type_index = static_cast<int>(br::ResourceType::GROUP);
	int r_type_last  = static_cast<int>(br::ResourceType::CUSTOM);
	for (; r_type_index <= r_type_last; ++r_type_index) {
		br::ResourceType r_type = static_cast<br::ResourceType>(r_type_index);
		snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.penalty.%s",
				name, br::GetResourceTypeString(r_type));
		opts_desc.add_options()
			(conf_str,
			 po::value<uint16_t>
				(&penalties_int[r_type_index])->default_value(
					SC_FAIR_DEFAULT_PENALTY),
			 "Fairness penalty per resource");
		;
	}
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Boundaries enforcement (0 <= penalty <= 100)
	r_type_index = static_cast<int>(br::ResourceType::GROUP);
	r_type_last  = static_cast<int>(br::ResourceType::CUSTOM);
	for (; r_type_index <= r_type_last; ++r_type_index) {
		br::ResourceType r_type = static_cast<br::ResourceType>(r_type_index);
		if (penalties_int[r_type_index] > 100) {
			logger->Warn("penalty.%s out of range [0,100]: "
					"found %d. Setting to %d",
					br::GetResourceTypeString(r_type),
					penalties_int[r_type_index],
					SC_FAIR_DEFAULT_PENALTY);
			penalties_int[r_type_index] = SC_FAIR_DEFAULT_PENALTY;
		}
		logger->Debug("penalty.%-3s: %.2f",
				br::GetResourceTypeString(r_type),
				static_cast<float>(penalties_int[r_type_index]) / 100.0);
	}
}

SCFairness::~SCFairness() {
}

SchedContrib::ExitCode_t SCFairness::Init(void * params) {
	char r_path_str[20];
	uint64_t bd_r_avail;
	AppPrio_t priority;
	std::vector<BBQUE_RID_TYPE>::iterator ids_it;
	std::list<br::ResourceType>::iterator type_it;

	// Applications/EXC to schedule, given the priority level
	priority = *(static_cast<AppPrio_t *>(params));
	num_apps = sv->ApplicationsCount(priority);
	r_types  = sv->ResourceTypesList();
	logger->Debug("Priority [%d]:  %d applications", priority, num_apps);
	logger->Debug("Bindings [%s]:  %d",
			bd_info.base_path->ToString().c_str(),
			bd_info.resources.size());
	logger->Debug("Resource types: %d", r_types.size());

	// For each resource type get the availability and the fair partitioning
	// among the application having the same priority
	for (br::ResourceType & r_type: r_types) {
		snprintf(r_path_str, 20, "%s.%s",
				bd_info.base_path->ToString().c_str(),
				br::GetResourceTypeString(r_type));
		int r_type_index = static_cast<int>(r_type);


		// Look for the binding domain with the lowest availability
		r_avail[r_type_index] = sv->ResourceAvailable(r_path_str, status_view);
		min_bd_r_avail[r_type_index] = r_avail[r_type_index];
		max_bd_r_avail[r_type_index] = 0;

		for (BBQUE_RID_TYPE & bd_id: bd_info.r_ids) {
			snprintf(r_path_str, 20, "%s%d.%s",
					bd_info.base_path->ToString().c_str(),
					bd_id,
					br::GetResourceTypeString(r_type));
			bd_r_avail = sv->ResourceAvailable(r_path_str, status_view);
			logger->Debug("R{%s} availability : % " PRIu64,
					r_path_str, bd_r_avail);

			// Update (?) the min availability value
			if (bd_r_avail < min_bd_r_avail[r_type_index]) {
				min_bd_r_avail[r_type_index] = bd_r_avail;
				logger->Debug("R{%s} minAV of %s: %" PRIu64,
						r_path_str,
						br::GetResourceTypeString(r_type),
						min_bd_r_avail[r_type_index]);
			}

			// Update (?) the max availability value
			if (bd_r_avail > max_bd_r_avail[r_type_index]) {
				max_bd_r_avail[r_type_index] = bd_r_avail;
				logger->Debug("R{%s} maxAV of %s: %" PRIu64,
						r_path_str,
						br::GetResourceTypeString(r_type),
						max_bd_r_avail[r_type_index]);
			}
		}

		// System-wide fair partition
		fair_pt[r_type_index] = max_bd_r_avail[r_type_index] / num_apps;
		logger->Debug("R{%s} maxAV: %" PRIu64 " fair partition: %" PRIu64,
				r_path_str,
				max_bd_r_avail[r_type_index],
				fair_pt[r_type_index]);
	}

	return SC_SUCCESS;
}

SchedContrib::ExitCode_t
SCFairness::_Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {
	br::ResourceAssignmentMap_t::const_iterator usage_it;
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
	for (auto const & ru_entry: evl_ent.pawm->ResourceRequests()) {
		ResourcePathPtr_t const & r_path(ru_entry.first);
		br::ResourceAssignmentPtr_t    const & r_assign(ru_entry.second);
		int r_type_index = static_cast<int>(r_path->Type());


		// Binding domain fraction (resource type related)
		logger->Debug("%s: R{%s} BD{'%s'} maxAV: %" PRIu64 " fair: %d",
				evl_ent.StrId(), r_path->ToString().c_str(),
				bd_info.base_path->ToString().c_str(),
				max_bd_r_avail[r_type_index],
				fair_pt[r_type_index]);
		// Safety check
		if (fair_pt[r_type_index] == 0) {
			logger->Warn("%s:  Fair partition is 0!", evl_ent.StrId());
			return SC_SUCCESS;
		}

		bd_fract = ceil(
				max_bd_r_avail[r_type_index] /
					fair_pt[r_type_index]);
		logger->Debug("%s: R{%s} BD{'%s'} fraction: %d",
				evl_ent.StrId(),
				r_path->ToString().c_str(),
				bd_info.base_path->ToString().c_str(),
				bd_fract);
		bd_fract == 0 ? bd_fract = 1 : bd_fract;

		// Binding domain fair partition
		bd_fair_pt = max_bd_r_avail[r_type_index] / bd_fract;
		if (bd_info.resources.size() > 1)
			bd_fair_pt = std::max(min_bd_r_avail[r_type_index], bd_fair_pt);
		logger->Debug("%s: R{%s} BD{'%s'} fair partition: %" PRIu64 "",
				evl_ent.StrId(), r_path->ToString().c_str(),
				bd_info.base_path->ToString().c_str(), bd_fair_pt);

		// Set last parameters for index computation
		penalty = static_cast<float>(penalties_int[r_type_index]) / 100.0;
		SetIndexParameters(
				bd_fair_pt,
				max_bd_r_avail[r_type_index],
				penalty,
				params);

		logger->Debug("%s: R{%s} requested = %" PRIu64,
				evl_ent.StrId(),
				r_path->ToString().c_str(),
				r_assign->GetAmount());

		// Compute the region index
		ru_index = CLEIndex(0, bd_fair_pt, r_assign->GetAmount(), params);
		logger->Debug("%s: R{%s} fairness index = %.4f",
				evl_ent.StrId(),
				r_path->ToString().c_str(),
				ru_index);

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
