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

#include "sc_reconfig.h"

namespace po = boost::program_options;

namespace bbque { namespace plugins {


SCReconfig::SCReconfig(
		const char * _name,
		SchedulerPolicyIF::BindingInfo_t const & _bd_info,
		uint16_t cfg_params[]):
	SchedContrib(_name, _bd_info, cfg_params) {
}

SCReconfig::~SCReconfig() {
}

SchedContrib::ExitCode_t SCReconfig::Init(void * params) {
	(void) params;

	return SC_SUCCESS;
}

SchedContrib::ExitCode_t
SCReconfig::_Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {
	UsagesMap_t::const_iterator usage_it;
	float reconf_cost  = 0.0;
	uint64_t rsrc_tot;

	// No reconfiguration (No AWM change) => Index := 1
	if (!evl_ent.IsReconfiguring()) {
		ctrib = 1.0;
		return SC_SUCCESS;
	}

	// Resource requested by the AWM (from the recipe)
	for_each_recp_resource_usage(evl_ent, usage_it) {
		ResourcePathPtr_t const & r_path(usage_it->first);
		UsagePtr_t const & pusage(usage_it->second);

		// Total amount of resource
		rsrc_tot = sv->ResourceTotal(r_path);
		logger->Debug("%s: {%s} R:%" PRIu64 " T:%" PRIu64 "",
				evl_ent.StrId(), r_path->ToString().c_str(),
				pusage->GetAmount(), rsrc_tot);

		// Reconfiguration cost
		reconf_cost += ((float) pusage->GetAmount() / (float) rsrc_tot);
	}

	// Contribution value
	ctrib = 1.0 - ((float) reconf_cost / sv->ResourceCountTypes());
	return SC_SUCCESS;
}


} // namespace plugins

} // namespace bbque

