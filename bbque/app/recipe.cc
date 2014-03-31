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

#include "bbque/app/recipe.h"

#include <cstring>

#include "bbque/app/working_mode.h"
#include "bbque/resource_accounter.h"
#include "bbque/res/resource_path.h"

namespace ba = bbque::app;
namespace bp = bbque::plugins;
namespace br = bbque::res;

namespace bbque { namespace app {


Recipe::Recipe(std::string const & name):
	pathname(name) {

	// Get a logger
	std::string logger_name(RECIPE_NAMESPACE"." + name);
	logger = bu::Logger::GetLogger(logger_name.c_str());
	assert(logger);

	// Clear normalization info
	memset(&norm, 0, sizeof(Recipe::AwmNormalInfo));
	norm.min_value = UINT8_MAX;
	working_modes.resize(MAX_NUM_AWM);
}

Recipe::~Recipe() {
	working_modes.clear();
	constraints.clear();
}

AwmPtr_t const Recipe::AddWorkingMode(uint8_t _id,
				std::string const & _name,
				uint8_t _value) {
	// Check if the AWMs are sequentially numbered
	if (_id != last_awm_id) {
		logger->Error("AddWorkingModes: Found ID = %d. Expected %d",
				_id, last_awm_id);
		return AwmPtr_t();
	}

	// Insert a new working mode descriptor into the vector
	AwmPtr_t new_awm(new app::WorkingMode(_id, _name, _value));

	// Insert the AWM descriptor into the vector
	working_modes[_id] = new_awm;
	++last_awm_id;

	return working_modes[_id];
}

void Recipe::AddConstraint(
		std::string const & rsrc_path,
		uint64_t lb,
		uint64_t ub) {
	// Check resource existance
	ResourceAccounter & ra(ResourceAccounter::GetInstance());
	ResourcePathPtr_t const r_path(ra.GetPath(rsrc_path));
	if (!r_path)
		return;

	// If there's a constraint yet, take the greater value between the bound
	// found and the one passed by argument
	ConstrMap_t::iterator cons_it(constraints.find(r_path));
	if (cons_it != constraints.end()) {
		(cons_it->second)->lower = std::max((cons_it->second)->lower, lb);
		(cons_it->second)->upper = std::max((cons_it->second)->upper, ub);
		logger->Debug("Constraint (edit): %s L=%d U=%d",
				r_path->ToString().c_str(),
				(cons_it->second)->lower,
				(cons_it->second)->upper);
		return;
	}

	// Insert a new constraint
	constraints.insert(std::pair<ResourcePathPtr_t, ConstrPtr_t>(
				r_path, ConstrPtr_t(new ResourceConstraint(lb, ub))));
	logger->Debug("Constraint (new): %s L=%" PRIu64 " U=%" PRIu64,
					r_path->ToString().c_str(),
					constraints[r_path]->lower,
					constraints[r_path]->upper);
}

void Recipe::Validate() {
	// Adjust the vector size
	working_modes.resize(last_awm_id);

	// Validate each AWM according to current resources total availability
	for (int i = 0; i < last_awm_id; ++i) {
		working_modes[i]->Validate();
		if (!working_modes[i]->Hidden())
			UpdateNormalInfo(working_modes[i]->RecipeValue());
	}

	// Normalize AWMs values
	NormalizeAWMValues();
}

void Recipe::UpdateNormalInfo(uint8_t last_value) {
	// This reset the "normalization done" flag
	norm.done = false;

	// Update the max value
	if (last_value > norm.max_value)
		norm.max_value = last_value;

	// Update the min value
	if (last_value < norm.min_value)
		norm.min_value = last_value;

	// Delta
	norm.delta = norm.max_value - norm.min_value;

	logger->Debug("AWM max value = %d", norm.max_value);
	logger->Debug("AWM min value = %d", norm.min_value);
	logger->Debug("AWM delta = %d", norm.delta);
}

void Recipe::NormalizeAWMValues() {
	float normal_value = 0.0;

	// Return if performed yet
	if (norm.done)
		return;

	// Normalization of the whole set of AWMs
	for (int i = 0; i < last_awm_id; ++i) {
		// Skip hidden AWMs
		if (working_modes[i]->Hidden())
			continue;

		// Normalize the value
		if (norm.delta > 0)
			// The most common case
			normal_value = working_modes[i]->RecipeValue() / norm.max_value;
		else if (working_modes.size() == 1)
			// There is only one AWM in the recipe
			normal_value = 1.0;
		else
			// This penalizes set of working modes having always the same QoS
			// value
			normal_value = 0.0;

		// Set the normalized value into the AWM
		working_modes[i]->SetNormalValue(normal_value);
		logger->Info("AWM %d normalized value = %.2f ",
					working_modes[i]->Id(), working_modes[i]->Value());
	}

	// Set the "normalization done" flag true
	norm.done = true;
}

} // namespace app

} // namespace bbque

