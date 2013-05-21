/*
 * Copyright (C) 2013  Politecnico di Milano
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

#include "sc_migration.h"
#include "bbque/res/binder.h"

namespace bbque { namespace plugins {


SCMigration::SCMigration(
		const char * _name,
		std::string const & b_domain,
		uint16_t const cfg_params[]):
	SchedContrib(_name, b_domain, cfg_params) {

	// Type of resource for the binding domain
	ResourcePath rb(b_domain);
	r_type = rb.Type();
}

SCMigration::~SCMigration() {
}

SchedContrib::ExitCode_t
SCMigration::Init(void * params) {
	(void) params;
	return SC_SUCCESS;
}

SchedContrib::ExitCode_t
SCMigration::_Compute(
		SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {
	ResourceBitset r_mask;

	// Migraton => index := 0
	if (evl_ent.IsMigrating(r_type)) {
		r_mask = evl_ent.papp->CurrentAWM()->BindingSet(r_type);
		logger->Debug("%s: is migrating to %s{%s}",
				evl_ent.StrId(),
				ResourceIdentifier::TypeStr[r_type],
				r_mask.ToStringCG().c_str());
		ctrib = 0.0;
		return SC_SUCCESS;
	}

	// No migration
	ctrib = 1.0;
	return SC_SUCCESS;
}

} // namespace plugins

} // namespace bbque
