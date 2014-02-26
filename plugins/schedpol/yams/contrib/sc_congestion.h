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

#ifndef BBQUE_SC_CONGESTION_
#define BBQUE_SC_CONGESTION_

#include "sched_contrib.h"

#define SC_CONG_DEFAULT_EXPBASE     2
#define SC_CONG_DEFAULT_PENALTY     10

namespace bbque { namespace plugins {


class SCCongestion: public SchedContrib {

public:

	/**
	 * @brief Constructor
	 *
	 * @see SchedContrib
	 */
	SCCongestion(const char * _name, std::string const & b_domain,
			uint16_t const cfg_params[]);

	~SCCongestion();

	/**
	 * @brief Initialize the contribution
	 *
	 * Actually, this is an empty member function
	 */
	ExitCode_t Init(void * params);

private:

	/**
	 * Base for exponential functions used in the computation
	 */
	uint16_t expbase;

	/**
	 * Congestion penalties per resource type. This stores the values parsed
	 * from the configuration file.
	 */
	uint16_t penalties_int[ResourceIdentifier::TYPE_COUNT];

	/** Penalty indices */
	float penalties[ResourceIdentifier::TYPE_COUNT];

	/**
	 * @brief Compute the congestion contribute
	 *
	 * @param evl_ent The entity to evaluate (EXC/AWM/ClusterID)
	 * @param ctrib The contribute to set
	 *
	 * @return SC_SUCCESS for success
	 */
	ExitCode_t _Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			float & ctrib);

	/**
	 * @brief Set the parameters for the filter function
	 *
	 * @param rl The information filled by the region evaluation of the new
	 * resource usage level
	 * @param penalty The congestion penalty of the given resource request
	 * @param params Parameters structure to fill
	 */
	void SetIndexParameters(ResourceThresholds_t const & rl, float & penalty,
			CLEParams_t & params);

};

} // plugins

} // bbque

#endif // BBQUE_SC_CONGESTION_
