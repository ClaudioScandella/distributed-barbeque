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

/** Set default congestion penalties values */
uint16_t SCFairness::penalties_default[SC_RSRC_COUNT] = {
	5,
	5
};

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
		 po::value<uint16_t>(&expbase)->default_value(DEFAULT_CONG_EXPBASE),
		 "Base for exponential function");
		;

	// Congestion penalties
	for (int i = 0; i < SC_RSRC_COUNT; ++i) {
		snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.penalty.%s",
				name, ResourceNames[i]);

		opts_desc.add_options()
			(conf_str,
			 po::value<uint16_t>
				(&penalties_int[i])->default_value(penalties_default[i]),
			 "Fairness penalty per resource");
		;
	}

	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Boundaries enforcement (0 <= penalty <= 100)
	for (int i = 0; i < SC_RSRC_COUNT; ++i) {
		if (penalties_int[i] > 100) {
			logger->Warn("Parameter penalty.%s out of range [0,100]: "
					"found %d. Setting to %d", ResourceNames[i],
					penalties_int[i], penalties_default[i]);
			penalties_int[i] = penalties_default[i];
		}
		logger->Debug("Resource [%s] saturation penalty \t= %.2f",
				ResourceNames[i], static_cast<float>(penalties_int[i]) / 100.0);
	}
}

SchedContrib::ExitCode_t SCFairness::Init(void * params) {
	AppPrio_t * prio;
	prio = static_cast<AppPrio_t *>(params);

	// Applications/EXC to schedule, given the priority level
	num_apps = sv->ApplicationsCount(*prio);
	logger->Debug("%d Applications/EXC for priority level %d", num_apps, *prio);

	// Get the total amount of resource per types
	for (int i = 0; i < SC_RSRC_COUNT; ++i) {
		rsrc_avail[i] = sv->ResourceAvailable(ResourceGenPaths[i], vtok);
		fair_parts[i] = rsrc_avail[i] / num_apps;
		logger->Debug("R{%s} AVL:%lu Fair partition:%lu",
				ResourceGenPaths[i], rsrc_avail[i], fair_parts[i]);
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
	uint64_t clust_rsrc_avl;
	uint64_t clust_fract;
	uint64_t clust_fair_part;
	ctrib = 1.0;

	// Fixed function parameters
	params.k = 1.0;
	params.exp.base = expbase;

	// Iterate the whole set of resource usage
	for_each_sched_resource_usage(evl_ent, usage_it) {
		std::string const & rsrc_path(usage_it->first);
		UsagePtr_t const & pusage(usage_it->second);
		ResourcePtrList_t & rsrc_bind(pusage->GetBindingList());

		// Resource availability (in the bound cluster)
		clust_rsrc_avl = sv->ResourceAvailable(rsrc_bind, vtok);
		logger->Debug("%s: R{%s} resource availability: %lu", evl_ent.StrId(),
				rsrc_path.c_str(), clust_rsrc_avl);

		// If there are no free resources the index contribute is equal to 0
		if (clust_rsrc_avl < pusage->GetAmount()) {
			ctrib = 0;
			return SC_SUCCESS;
		}

		// Compute the cluster factor (resource type related)
		std::string rsrc_name(ResourcePathUtils::GetNameTemplate(rsrc_path));
		if (rsrc_name.compare(ResourceNames[SC_RSRC_PE]) == 0) {
			clust_fract = ceil(static_cast<double>(clust_rsrc_avl) /
					fair_parts[SC_RSRC_PE]);
			penalty = static_cast<float>(penalties_int[SC_RSRC_PE]) / 100.0;
		}
		else {
			clust_fract = ceil(static_cast<double>(clust_rsrc_avl) /
					fair_parts[SC_RSRC_MEM]);
			penalty = static_cast<float>(penalties_int[SC_RSRC_MEM]) / 100.0;
		}

		logger->Debug("%s: R{%s} cluster fraction: %lu", evl_ent.StrId(),
				rsrc_path.c_str(), clust_fract);

		// Compute the cluster fair partition
		clust_fair_part = std::min<uint64_t>(clust_rsrc_avl,
				clust_rsrc_avl / clust_fract);
		logger->Debug("%s: R{%s} cluster fair partition: %lu",
				evl_ent.StrId(), rsrc_path.c_str(), clust_fair_part);

		// Set function parameters
		SetIndexParameters(clust_fair_part, clust_rsrc_avl, penalty, params);

		// Compute the region index
		//ru_index = CLEIndex(clust_fair_part, clust_fair_part,
		ru_index = CLEIndex(0, clust_fair_part, pusage->GetAmount(), params);
		logger->Debug("%s: R{%s} index = %.4f", evl_ent.StrId(),
				rsrc_path.c_str(), ru_index);

		// Update the contribute if the index is lower, i.e. the most
		// penalizing request dominates
		ru_index < ctrib ? ctrib = ru_index: ctrib;
	}

	return SC_SUCCESS;
}

void SCFairness::SetIndexParameters(uint64_t cfp,
		uint64_t cra,
		float & penalty,
		CLEParams_t & params) {
	// Linear parameters
	params.lin.xoffset = 0.0;
	params.lin.scale = penalty / static_cast<float>(cfp);

	// Exponential parameters
	params.exp.yscale = (1.0 - penalty) / (params.exp.base - 1.0);
	params.exp.xscale = static_cast<float>(cfp) - static_cast<float>(cra);
	params.exp.xoffset = static_cast<float>(cra);
}


} // namespace plugins

} // namespace bbque
