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

#ifndef BBQUE_OCL_STATS_H_
#define BBQUE_OCL_STATS_H_

#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <CL/cl.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#define CL_CMD_QUEUED_TIME 0
#define CL_CMD_SUBMIT_TIME 1
#define CL_CMD_EXEC_TIME   2

namespace bac = boost::accumulators;

typedef class RTLIB_OCL_QueueProf RTLIB_OCL_QueueProf_t;
typedef struct CmdProf CmdProf_t;
typedef std::array<bac::accumulator_set<double, bac::stats<bac::tag::sum> >,3> AccArray_t;
typedef std::shared_ptr<RTLIB_OCL_QueueProf_t> QueueProfPtr_t;
typedef std::shared_ptr<CmdProf_t> CmdProfPtr_t;
typedef std::map<cl_command_queue, QueueProfPtr_t> OclEventsStatsMap_t;
typedef std::pair<cl_command_type, std::string> CmdStrPair_t;
typedef std::pair<cl_command_queue, QueueProfPtr_t> QueueProfPair_t;
typedef std::pair<void *, cl_event> AddrEventPair_t;
typedef std::pair<void *, CmdProfPtr_t> AddrCmdPair_t;

/**
 * @class Collect events profiling info for an OpenCL command queue
 */
class RTLIB_OCL_QueueProf {
public:
	~RTLIB_OCL_QueueProf() {
		std::map<void *, cl_event>::iterator it_ev;
		cl_uint ref_count;
		for (it_ev = events.begin(); it_ev != events.end(); it_ev++) {
			clGetEventInfo(it_ev->second, CL_EVENT_REFERENCE_COUNT, sizeof(cl_uint), &ref_count, NULL);
			if (ref_count > 0)
				clReleaseEvent(it_ev->second);
		}

		cmd_prof.clear();
	};

	std::map<void *, cl_event> events;
	std::map<void *, CmdProfPtr_t> cmd_prof;
};

/**
 * @struct Collect timings for OpenCL commands
 */
struct CmdProf {
	cl_command_type cmd_type;
	AccArray_t prof_time;
};

#endif // BBQUE_OCL_STATS_H_
