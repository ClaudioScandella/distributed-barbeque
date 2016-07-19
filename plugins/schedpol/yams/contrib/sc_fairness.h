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

#ifndef BBQUE_SC_FAIRNESS_
#define BBQUE_SC_FAIRNESS_

#include "sched_contrib.h"

#define SC_FAIR_DEFAULT_EXPBASE    2
#define SC_FAIR_DEFAULT_PENALTY    5

using bbque::app::AppPrio_t;

namespace bbque { namespace plugins {


class SCFairness: public SchedContrib {

public:

	/**
	 * @brief Constructor
	 *
	 * @see SchedContrib
	 */
	SCFairness(
		const char * _name,
		BindingInfo_t const & _bd_info,
		uint16_t const cfg_params[]);

	~SCFairness();

	/**
	 * @brief Perform per-priority class information setup
	 *
	 * Get the number of applications in the given priority level, the
	 * availability (not clustered), and computes the fair partitions of each
	 * resource type
	 *
	 * @param params Expected pointer to AppPrio_t type
	 *
	 * @return SC_SUCCESS. No error conditions expected
	 */
	ExitCode_t Init(void * params);

private:

	/** Base for exponential functions used in the computation */
	uint16_t expbase;

	/**
	 * Fairness penalties per resource type. This stores the values parsed
	 * from the configuration file.
	 */
	uint16_t penalties_int[R_TYPE_COUNT];

	/** Number of applications to schedule */
	uint16_t num_apps = 0;

	/** List of managed resource types */
	std::list<br::ResourceType> r_types;

	/** Resource availabilities */
	uint64_t r_avail[R_TYPE_COUNT];

	/** Lowest BD resources availability */
	uint64_t min_bd_r_avail[R_TYPE_COUNT];

	/** Highest BD resources availability */
	uint64_t max_bd_r_avail[R_TYPE_COUNT];

	/** Fair partitions */
	uint64_t fair_pt[R_TYPE_COUNT];

	/**
	 * @brief Compute the congestion contribute
	 *
	 * @param evl_ent The entity to evaluate (EXC/AWM/BindingID)
	 * @param ctrib The contribute to set
	 *
	 * @return SC_SUCCESS for success
	 */
	ExitCode_t _Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			float & ctrib);

	/**
	 * @brief Set the parameters for the filter function
	 *
	 * More in detail the parameters set are exclusively the ones of the
	 * exponential function, since the Sub Fair Region (SFR) returns the
	 * constant index (1) and the Over Fair Region (OFR) the result of the
	 * exponential function.
	 *
	 * @param bfp Binding fair partition. Used to compute the "x-scale"
	 * parameter.
	 * @param bra Binding resource availability. This is the "x-offset" and a
	 * component of the "x-scale" parameter.
	 * @param penalty The un-fairness penalty of the resource
	 * @param params Parameters structure to fill
	 */
	void SetIndexParameters(uint64_t bfp, uint64_t bra,	float & penalty,
			CLEParams_t & params);

};

} // plugins

} // bbque

#endif // BBQUE_SC_FAIRNESS_
