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

#include "sc_value.h"

using namespace bbque::res;

namespace po = boost::program_options;

namespace bbque { namespace plugins {


SCValue::SCValue(
		const char * _name,
		std::string const & b_domain,
		uint16_t const cfg_params[]):
	SchedContrib(_name, b_domain, cfg_params) {
	char conf_str[50];
	uint16_t napw;
	po::options_description opts_desc("AWM value contribution parameters");

	// NAP weight
	snprintf(conf_str, 50, SC_CONF_BASE_STR"%s.nap.weight", name);
	opts_desc.add_options()
		(conf_str,
		 po::value<uint16_t>(&napw)->default_value(SC_VALUE_NAPW_DEFAULT),
		 "Normalized Actual Penalty (NAP) weight");
		;
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	// Check boundary value
	if (napw > 100) {
		napw = SC_VALUE_NAPW_DEFAULT;
		logger->Warn("NAP weight out of range (set to default = %.2f)",
				(float) SC_VALUE_NAPW_DEFAULT / 100.0);
	}

	// Convent to a indexed value [0..1]
	nap_weight = static_cast<float>(napw) / 100.0;
	logger->Debug("Normalized Actual Penalty weight \t= %.2f", nap_weight);
}

SchedContrib::ExitCode_t
SCValue::Init(void * params) {
	(void) params;

	return SC_SUCCESS;
}

SchedContrib::ExitCode_t
SCValue::_Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
		float & ctrib) {
	std::string rsrc_tmp_path;
	UsagesMap_t::const_iterator usage_it;
	AwmPtr_t const & curr_awm(evl_ent.papp->CurrentAWM());
	float nap = 0.0;

	// Initialize the index contribute to the AWM static value
	ctrib = (1.0 - nap_weight) * evl_ent.pawm->Value();
	logger->Debug("%s: AWM static value: %.4f", evl_ent.StrId(), ctrib);

	// NAP set?
	nap = nap_weight * static_cast<float>(evl_ent.papp->GetGoalGap()) / 100.0;
	if (!curr_awm || (nap == 0) ||
			(curr_awm->Value() >= evl_ent.pawm->Value()))
		return SC_SUCCESS;
	logger->Debug("%s: Normalized Actual Penalty (NAP) = %d/100): %.4f",
			evl_ent.StrId(), evl_ent.papp->GetGoalGap(), nap);

	// Add the NAP part to the value of the contribution
	ctrib += nap;
	logger->Debug("%s: AWM Value index: %.4f", evl_ent.StrId(),	ctrib);

	return SC_SUCCESS;
}


} // namespace plugins

} // namespace bbque

