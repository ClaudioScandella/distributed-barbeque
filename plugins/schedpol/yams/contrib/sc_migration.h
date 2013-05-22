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

#ifndef BBQUE_SC_MIGRATION_
#define BBQUE_SC_MIGRATION_

#include "sched_contrib.h"

namespace bbque { namespace plugins {


class SCMigration: public SchedContrib {

public:

	/**
	 * @brief Constructor
	 *
	 * @see SchedContrib
	 */
	SCMigration(
		const char * _name,
		SchedulerPolicyIF::BindingInfo_t const & _bd_info,
		uint16_t const cfg_params[]);

	~SCMigration();

	ExitCode_t Init(void * params);

private:

	/**
	 * @brief Compute the "Migration" contribution
	 *
	 * Current implementation simply returns 1 if the binding specified in the
	 * evaluation entity does not lead to a migration of the application.
	 * Conversely, an index value equal to 0 is returned.
	 *
	 * @param evl_ent The scheduling entity to evaluate
	 * @ctrib ctrib The contribute index to return
	 *
	 * @return SC_SUCCESS. No error conditions expected.
	 */
	ExitCode_t _Compute(
			SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			float & ctrib);

};

} // plugins

} // bbque

#endif // BBQUE_SC_MIGRATION_


