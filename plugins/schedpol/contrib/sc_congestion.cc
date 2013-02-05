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

#include "sc_congestion.h"

namespace po = boost::program_options;

namespace bbque { namespace plugins {


SCCongestion::SCCongestion(
		const char * _name,
		std::string const & b_domain,
		uint16_t const cfg_params[]):
	SchedContrib(_name, b_domain, cfg_params) {
	char conf_str[50];

	// Configuration parameters
	po::options_description opts_desc("Congestion contribute parameters");

	// Base for exponential
	snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.expbase", name);
	opts_desc.add_options()
		(conf_str,
		 po::value<uint16_t>(&expbase)->default_value(SC_CONG_DEFAULT_EXPBASE),
		 "Base for exponential function");
		;

	// Congestion penalties
	for (int i = 1; i < ResourceIdentifier::TYPE_COUNT; ++i) {
		snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.penalty.%s",
				name, ResourceIdentifier::TypeStr[i]);

		opts_desc.add_options()
			(conf_str,
			 po::value<uint16_t>
			 (&penalties_int[i])->default_value(SC_CONG_DEFAULT_PENALTY),
			 "Congestion penalty per resource");
		;
	}
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Boundaries enforcement (0 <= penalty <= 100)
	for (int i = 1; i < ResourceIdentifier::TYPE_COUNT; ++i) {
		if (penalties_int[i] > 100) {
			logger->Warn("penalty.%s out of range [0,100]: "
					"found %d. Setting to %d",
					ResourceIdentifier::TypeStr[i],
					penalties_int[i], SC_CONG_DEFAULT_PENALTY);
			penalties_int[i] = SC_CONG_DEFAULT_PENALTY;
		}
		penalties[i] = static_cast<float>(penalties_int[i]) / 100.0;
		logger->Debug("penalty.%-3s: %.2f",
				ResourceIdentifier::TypeStr[i], penalties[i]);
	}
}

SCCongestion::~SCCongestion() {
}

SchedContrib::ExitCode_t
SCCongestion::Init(void * params) {
	(void) params;

	return SC_SUCCESS;
}

SchedContrib::ExitCode_t
SCCongestion::_Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {
	UsagesMap_t::const_iterator usage_it;
	ResourceThresholds_t rl;
	CLEParams_t params;
	float ru_index;
	ctrib = 1.0;

	// Fixed function parameters
	params.k = 1.0;
	params.exp.base = expbase;

	// Iterate the whole set of resource usage
	for_each_sched_resource_usage(evl_ent, usage_it) {
		ResourcePathPtr_t const & rsrc_path(usage_it->first);
		UsagePtr_t const & pusage(usage_it->second);
		logger->Debug("%s: {%s}",
				evl_ent.StrId(), rsrc_path->ToString().c_str());

		// Get the region of the (next) resource usage
		GetResourceThresholds(rsrc_path, pusage->GetAmount(), evl_ent, rl);

		// If there are no free resources the index contribute is equal to 0
		if (rl.free < pusage->GetAmount()) {
			ctrib = 0;
			return SC_SUCCESS;
		}

		// Set the last parameters for the index computation
		SetIndexParameters(rl, penalties[rsrc_path->Type()], params);

		// Compute the region index
		ru_index = CLEIndex(rl.sat_lack, rl.free, pusage->GetAmount(), params);
		logger->Debug("%s: {%s} index = %.4f", evl_ent.StrId(),
				rsrc_path->ToString().c_str(), ru_index);

		// Update the contribute if the index is lower, i.e. the most
		// penalizing request dominates
		ru_index < ctrib ? ctrib = ru_index: ctrib;
	}

	return SC_SUCCESS;
}

void SCCongestion::SetIndexParameters(ResourceThresholds_t const & rl,
		float & penalty,
		CLEParams_t & params) {

	// Linear parameters
	params.lin.xoffset = static_cast<float>(rl.sat_lack);
	params.lin.scale = penalty /
		(static_cast<float>(rl.free) - static_cast<float>(rl.sat_lack));

	// Exponential parameters
	params.exp.yscale  = (1.0 - penalty) / (params.exp.base - 1.0);
	params.exp.xscale  = static_cast<float>(rl.free) - static_cast<float>(rl.total);
	params.exp.xoffset = static_cast<float>(rl.total);
}


} // namespace plugins

} // namespace bbque
