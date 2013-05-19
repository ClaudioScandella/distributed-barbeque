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
#include "bbque/res/binder.h"

namespace po = boost::program_options;

namespace bbque { namespace plugins {


SCReconfig::SCReconfig(
		const char * _name,
		std::string const & b_domain,
		uint16_t cfg_params[]):
	SchedContrib(_name, b_domain, cfg_params) {
	char conf_str[40];

	// Configuration parameters
	po::options_description opts_desc("Reconfiguration contribute params");
	snprintf(conf_str, 40, SC_CONF_BASE_STR"%s.migfact", name);

	opts_desc.add_options()
		(conf_str,
		 po::value<uint16_t>
		 (&migfact)->default_value(DEFAULT_MIGRATION_FACTOR),
		 "Migration factor");
		;

	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);
	logger->Debug("Application migration cost factor \t= %d", migfact);

	// Type of resource for the binding domain
	ResourcePath rb(b_domain);
	r_type = rb.Type();
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
	uint8_t to_migrate = 0;
	uint64_t rsrc_avl  = 0.0;
	uint64_t rsrc_tot;
	ResourceBitset r_mask;

	// Check if a migration would be required, if yes enable the factor
	if (evl_ent.IsMigrating(r_type)) {
		to_migrate = 1;
		r_mask = evl_ent.papp->CurrentAWM()->BindingSet(r_type);
		logger->Debug("%s: evaluating ""%s"" migration to {%d}",
				evl_ent.StrId(), ResourceIdentifier::TypeStr[r_type],
				uint32_t(log(r_mask.ToULong())/log(2)));
	}

	// Reconfiguration Index := 1 if no migration and no AWM change
	if (!to_migrate && !evl_ent.IsReconfiguring()) {
		ctrib = 1.0;
		return SC_SUCCESS;
	}

	// Resource usages of the current entity (AWM + binding domain)
	for_each_sched_resource_usage(evl_ent, usage_it) {
		ResourcePathPtr_t const & r_path(usage_it->first);
		UsagePtr_t const & pusage(usage_it->second);
		ResourcePtrList_t & rsrc_bind(pusage->GetBindingList());

		// Query resource availability
		rsrc_avl = sv->ResourceAvailable(rsrc_bind, vtok, evl_ent.papp);

		// Total amount of resource
		rsrc_tot = sv->ResourceTotal(pusage->GetBindingList());
		logger->Debug("%s: {%s} R:%" PRIu64 " A:%" PRIu64 " T:%" PRIu64 "",
				evl_ent.StrId(), r_path->ToString().c_str(),
				pusage->GetAmount(), rsrc_avl, rsrc_tot);

		// Reconfiguration cost
		reconf_cost += ((float) pusage->GetAmount() / (float) rsrc_tot);
	}

	// Contribute value
	ctrib = 1.0 -
		(1.0 + (float) to_migrate * migfact) /
		(1.0 + (float) migfact) *
		((float) reconf_cost / sv->ResourceCountTypes());

	return SC_SUCCESS;
}


} // namespace plugins

} // namespace bbque

