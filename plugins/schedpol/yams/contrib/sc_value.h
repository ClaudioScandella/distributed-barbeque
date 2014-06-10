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

#ifndef BBQUE_SC_VALUE_
#define BBQUE_SC_VALUE_

#include "sched_contrib.h"

#define SC_VALUE_NAPW_DEFAULT 	60


namespace bbque { namespace plugins {


class SCValue: public SchedContrib {

public:

	/**
	 * @brief Constructor
	 *
	 * @see SchedContrib
	 */
	SCValue(const char * _name,
		BindingInfo_t const & _bd_info,
		uint16_t const cfg_params[]);

	~SCValue();

	ExitCode_t Init(void * params);

private:

	/** The weight of the NAP assertion in the contribution value */
	float nap_weight;

	/**
	 * @brief Compute the AWM value contribute
	 *
	 * The contribute starts from the static value associated to the AWM to
	 * evaluate. If a "goal gap" has been set, a "target" resource usage is
	 * considered accordingly. If the AWM to evaluate provides a resource
	 * usage greater or equal to the target usage, the static AWM value is
	 * returned as contribute index, otherwise the index will be a
	 * "penalization" of the static value.
	 *
	 * @param evl_ent The scheduling entity to evaluate
	 * @ctrib ctrib The contribute index to return
	 *
	 * @return SC_SUCCESS. No error conditions expected.
	 */
	ExitCode_t _Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			float & ctrib);

};

} // plugins

} // bbque

#endif // BBQUE_SC_VALUE_


