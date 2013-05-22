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

#ifndef BBQUE_SC_RECONFIG_
#define BBQUE_SC_RECONFIG_

#include "sched_contrib.h"

namespace bbque { namespace plugins {


class SCReconfig: public SchedContrib {

public:

	/**
	 * @brief Constructor
	 */
	SCReconfig(
		const char * _name,
		SchedulerPolicyIF::BindingInfo_t const & _bd_info,
		uint16_t cfg_params[]);

	~SCReconfig();

	ExitCode_t Init(void * params);

private:

	/**
	 * The type of resource according to which evaluate task migration
	 * (e.g., CPU, GROUP, ACCELERATOR, ...)
	 */
	ResourceIdentifier::Type_t r_type;

	/**
	 * @brief Compute the reconfiguration contribute
	 */
	ExitCode_t _Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			float & ctrib);
};

} // plugins

} // bbque

#endif // BBQUE_SC_RECONFIG_


