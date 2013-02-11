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

#include "bbque/app/working_mode.h"

#include <string>

#include "bbque/modules_factory.h"
#include "bbque/plugin_manager.h"
#include "bbque/resource_accounter.h"

#include "bbque/app/application.h"
#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_path.h"
#include "bbque/res/identifier.h"
#include "bbque/res/binder.h"
#include "bbque/res/usage.h"
#include "bbque/utils/utility.h"

namespace br = bbque::res;
namespace bp = bbque::plugins;

using br::ResourcePathUtils;
using br::ResourceBinder;
using br::ResourceIdentifier;

namespace bbque { namespace app {


WorkingMode::WorkingMode():
	hidden(false) {
	resources.sched_bindings.resize(MAX_R_ID_NUM);
	resources.binding_masks.resize(
		static_cast<uint16_t>(ResourceIdentifier::TYPE_COUNT));
	// Set the log string id
	strncpy(str_id, "", 12);
}

WorkingMode::WorkingMode(uint8_t _id,
		std::string const & _name,
		float _value):
	id(_id),
	name(_name),
	hidden(false) {

	// Value must be positive
	_value > 0 ? value.recpv = _value : value.recpv = 0;

	// Init the size of the scheduling bindings vector
	resources.sched_bindings.resize(MAX_R_ID_NUM);
	resources.binding_masks.resize(ResourceIdentifier::TYPE_COUNT);

	// Get a logger
	bp::LoggerIF::Configuration conf(AWM_NAMESPACE);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));

	// Set the log string id
	snprintf(str_id, 15, "AWM{%d,%s}", id, name.c_str());
}

WorkingMode::~WorkingMode() {
	resources.requested.clear();
	if (resources.sync_bindings)
		resources.sync_bindings->clear();
}

WorkingMode::ExitCode_t WorkingMode::AddResourceUsage(
		std::string const & rsrc_path,
		uint64_t required_amount) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());

	// Build a resource path object
	ResourcePathPtr_t ppath(new ResourcePath(rsrc_path));
	if (!ppath) {
		logger->Error("%s AddResourceUsage: {%s} invalid resource path",
				str_id,	rsrc_path.c_str());
		return WM_RSRC_ERR_TYPE;
	}

	// Check the existance of the resource required
	if (!ra.ExistResource(ppath)) {
		logger->Warn("%s AddResourceUsage: {%s} not found.",
				str_id, rsrc_path.c_str());
		return WM_RSRC_NOT_FOUND;
	}

	// Insert a new resource usage object in the map
	UsagePtr_t pusage(UsagePtr_t(new Usage(required_amount)));
	resources.requested.insert(
			std::pair<ResourcePathPtr_t, UsagePtr_t>(ppath, pusage));
	logger->Debug("%s AddResourceUsage: added {%s}\t[usage: %" PRIu64 "]",
			str_id, ppath->ToString().c_str(), required_amount);

	return WM_SUCCESS;
}

WorkingMode::ExitCode_t WorkingMode::Validate() {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	UsagesMap_t::iterator usage_it, it_end;
	uint64_t total_amount;

	// Initialization
	usage_it = resources.requested.begin();
	it_end   = resources.requested.end();
	hidden   = false;

	// Map of resource usages requested
	for (; usage_it != it_end; ++usage_it) {
		// Current resource: path and amount required
		ResourcePathPtr_t const & rcp_path(usage_it->first);
		UsagePtr_t & rcp_pusage(usage_it->second);

		// Check the total amount available. Hide the AWM if the current total
		// amount available cannot satisfy the amount required.
		// Consider the resource template path, since the requested resource
		// can be mapped on more than one system/HW resource.
		total_amount = ra.Total(rcp_path, ResourceAccounter::TEMPLATE);
		if (total_amount < rcp_pusage->GetAmount()) {
			logger->Warn("%s Validate: {%s} usage required (%" PRIu64 ") "
					"exceeds total (%" PRIu64 ")",
					str_id, rcp_path->ToString().c_str(),
					rcp_pusage->GetAmount(), total_amount);
			hidden = true;
			logger->Warn("%s Validate: set to 'hidden'", str_id);
			return WM_RSRC_USAGE_EXCEEDS;
		}
	}

	return WM_SUCCESS;
}

uint64_t WorkingMode::ResourceUsageAmount(
		ResourcePathPtr_t ppath) const {
	UsagesMap_t::const_iterator r_it(resources.requested.begin());
	UsagesMap_t::const_iterator r_end(resources.requested.end());

	for (; r_it != r_end; ++r_it) {
		ResourcePathPtr_t const & wm_rp(r_it->first);
		if (ppath->Compare(*(wm_rp.get())) == ResourcePath::NOT_EQUAL)
			continue;
		return r_it->second->GetAmount();
	}
	return 0;
}

WorkingMode::ExitCode_t WorkingMode::BindResource(
		ResourceIdentifier::Type_t r_type,
		ResID_t src_ID,
		ResID_t dst_ID,
		uint16_t b_id) {
	uint32_t b_count;
	UsagesMap_t::const_iterator b_it, b_end;

	// Sanity check
	if (b_id >= MAX_R_ID_NUM)
		return WM_BIND_ID_OVERFLOW;

	// Allocate a new temporary resource usages map
	UsagesMapPtr_t bind_pum(UsagesMapPtr_t(new UsagesMap_t()));

	// Binding
	b_count = ResourceBinder::Bind(
			resources.requested, r_type, src_ID, dst_ID, bind_pum);
	if (b_count == 0) {
		logger->Warn("%s BindResource: nothing to bind", str_id);
		return WM_RSRC_MISS_BIND;
	}
	logger->Debug("%s BindResource: R{%s} b_id[%d] size:%d count:%d",
			str_id, ResourceIdentifier::StringFromType(r_type),
			b_id, bind_pum->size(), b_count);

	// Store the resource binding
	resources.sched_bindings[b_id] = bind_pum;

	return WM_SUCCESS;
}

WorkingMode::ExitCode_t WorkingMode::SetResourceBinding(uint16_t b_id) {
	ResourceBitset new_mask, temp_mask;
	uint8_t r_type;

	// Sanity check
	if (b_id >= MAX_R_ID_NUM)
		return WM_BIND_ID_OVERFLOW;

	// Update the resource binding bitmask (for each type)
	for (r_type = ResourceIdentifier::SYSTEM;
			r_type < ResourceIdentifier::TYPE_COUNT; ++r_type) {

		// Update the binding mask
		BindingInfo & r_mask(resources.binding_masks[r_type]);
		new_mask = ResourceBinder::GetMask(
				resources.sched_bindings[b_id],
				static_cast<ResourceIdentifier::Type_t>(r_type));
		r_mask.prev = r_mask.curr;
		r_mask.curr = new_mask;

		// Set the flag if changed and print a log message
		r_mask.changed = r_mask.prev != new_mask;
		logger->Debug("%s SetBinding: R{%-3s} changed? [%d]",
				str_id, ResourceIdentifier::TypeStr[r_type], r_mask.changed);
	}

	// Set the new binding / resource usages map
	resources.sync_bindings = resources.sched_bindings[b_id];
	resources.sched_bindings[b_id].reset();

	return WM_SUCCESS;
}

void WorkingMode::ClearResourceBinding() {
	uint8_t r_type = 0;
	resources.sync_bindings->clear();
	for (; r_type < ResourceIdentifier::TYPE_COUNT; ++r_type) {
		resources.binding_masks[r_type].curr =
			resources.binding_masks[r_type].prev;
	}
}

ResourceBitset WorkingMode::BindingSet(
		ResourceIdentifier::Type_t r_type) const {
	BindingInfo const & bi(resources.binding_masks[r_type]);
	return bi.curr;
}

ResourceBitset WorkingMode::BindingSetPrev(
		ResourceIdentifier::Type_t r_type) const {
   return resources.binding_masks[r_type].prev;
}

bool WorkingMode::BindingChanged(
		ResourceIdentifier::Type_t r_type) const {
   return resources.binding_masks[r_type].changed;
}

} // namespace app

} // namespace bbque

