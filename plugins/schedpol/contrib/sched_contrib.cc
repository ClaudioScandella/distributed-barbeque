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

#include <cmath>
#include <cstring>

#include "sched_contrib.h"
#include "bbque/modules_factory.h"

#define MODULE_NAMESPACE SCHEDULER_POLICY_NAMESPACE ".sc"

namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque { namespace plugins {


char const * SchedContrib::ConfigParamsStr[SC_CONFIG_COUNT] = {
	"msl"
};

uint16_t const SchedContrib::ConfigParamsDefault[SC_CONFIG_COUNT] = {
	90
};


SchedContrib::SchedContrib(const char * _name,
		std::string const & b_domain,
		uint16_t const params[]):
	cm(ConfigurationManager::GetInstance()),
	binding_domain(b_domain) {
	char logname[16];

	// Identifier name of the contribute
	strncpy(name, _name, SC_NAME_MAX_LEN);
	name[SC_NAME_MAX_LEN-1] = '\0';

	// Array of Maximum Saturation Levels parameters
	for (int i = 0; i < ResourceIdentifier::TYPE_COUNT; ++i)
		msl_params[i] = static_cast<float> (params[i]) / 100.0;

	// Get a logger instance
	snprintf(logname, 16, MODULE_NAMESPACE".%s", name);
	plugins::LoggerIF::Configuration conf(logname);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
}

SchedContrib::~SchedContrib() {
}

SchedContrib::ExitCode_t
SchedContrib::Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {

	// A valid token for the resource state view is mandatory
	if (vtok == 0) {
		logger->Error("Missing a valid system/state view");
		return SC_ERR_VIEW;
	}

	_Compute(evl_ent, ctrib);

	logger->Info("%s: %-10s = %.4f", evl_ent.StrId(), name, ctrib);
	assert((ctrib >= 0) && (ctrib <= 1));

	return SC_SUCCESS;
}

void SchedContrib::GetResourceThresholds(
		ResourcePathPtr_t rsrc_path,
		uint64_t rsrc_amount,
		SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		ResourceThresholds_t & rl) {

	// Total amount of resource
	rl.total = sv->ResourceTotal(rsrc_path);

	// Get the max saturation level of this type of resource
	rl.saturate = rl.total * msl_params[rsrc_path->Type()];

	// Resource availability (scheduling resource state view)
	rl.free  = sv->ResourceAvailable(rsrc_path, vtok);
	rl.usage = rl.total - rl.free;

	// Amount of resource remaining before reaching the saturation
	rl.sat_lack = 0;
	if (rl.free > rl.total - rl.saturate)
		rl.sat_lack = rl.saturate - rl.total + rl.free;

	assert(rl.sat_lack <= rl.free);
	logger->Debug("%s: REGIONS -> usg: %" PRIu64 "| sat: %" PRIu64 "|"
			" sat-lack: %" PRIu64 " | free: %" PRIu64 "| req: %" PRIu64 "|",
			evl_ent.StrId(),
			rl.usage, rl.saturate, rl.sat_lack, rl.free, rsrc_amount);
}

float SchedContrib::CLEIndex(uint64_t c_thresh,
		uint64_t l_thresh,
		float rsrc_amount,
		CLEParams_t const & params) {
	// SSR: Sub-Saturation Region
	if (rsrc_amount <= c_thresh) {
		logger->Debug("Region: ""Constant""");
		return params.k;
	}

	// ISR: In-Saturation Region
	if (rsrc_amount <= l_thresh) {
		logger->Debug("Region: ""Linear""");
		return FuncLinear(rsrc_amount, params.lin);
	}

	// OSR: Over-Saturation Region
	logger->Debug("Region: ""Exponential""");
	return FuncExponential(rsrc_amount, params.exp);
}

float SchedContrib::FuncLinear(float x, LParams_t const & p) {
	DB(
		fprintf(stderr, FD("LIN ==== 1 - %.6f * (%.2f - %.2f)\n"),
			p.scale, x, p.xoffset);
	  );
	return 1 - p.scale * (x - p.xoffset);
}

float SchedContrib::FuncExponential(float x, EParams_t const & p) {
	DB(
		fprintf(stderr, FD("EXP ==== %.4f * (%.4f ^ ((%.4f - %.4f) / %.4f) - 1)\n"),
			p.yscale, p.base, x, p.xoffset, p.xscale);
	  );
	return p.yscale * (pow(p.base, ((x - p.xoffset) / p.xscale)) - 1);
}


} // namespace plugins

} // namespace bbque
