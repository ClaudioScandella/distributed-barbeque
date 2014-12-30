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

namespace br = bbque::res;
namespace po = boost::program_options;

namespace bbque { namespace plugins {


SCReconfig::SCReconfig(
		const char * _name,
		BindingInfo_t const & _bd_info,
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
SCReconfig::_Compute(
		SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {

	// No reconfiguration (No AWM change) => Index := 1
	if (!evl_ent.IsReconfiguring()) {
		ctrib = 1.0;
		return SC_SUCCESS;
	}

	// Configuration time available
	if (evl_ent.pawm->ConfigTime() >= 0) {
		ctrib = 1.0 - evl_ent.pawm->ConfigTime();
		return SC_SUCCESS;
	}

	// No configuration time profiled: call the 'estimator'
	ctrib = ComputeResourceProportional(evl_ent);
	return SC_SUCCESS;
}


float SCReconfig::ComputeResourceProportional(
		SchedulerPolicyIF::EvalEntity_t const & evl_ent) {
	br::UsagesMap_t::const_iterator usage_it;
	float reconf_cost  = 0.0;
	uint32_t rsrc_tot;

	// Resource requested by the AWM (from the recipe)
	for (auto const & ru_entry :evl_ent.pawm->RecipeResourceUsages()) {
		br::UsagePtr_t const & pusage(ru_entry.second);

		// Total amount of resource (overall)
		ResourcePathPtr_t r_path(new br::ResourcePath(
			br::ResourcePathUtils::GetTemplate(usage_it->first->ToString())));
		rsrc_tot = sv->ResourceTotal(r_path);
		logger->Debug("%s: {%s} R:%4d T:%4d",
				evl_ent.StrId(), r_path->ToString().c_str(),
				pusage->GetAmount(), rsrc_tot);

		// Reconfiguration cost
		reconf_cost += ((float) pusage->GetAmount() / (float) rsrc_tot);
	}

	// Contribution value
	return (1.0 - ((float) reconf_cost / sv->ResourceCountTypes()));
}

} // namespace plugins

} // namespace bbque

