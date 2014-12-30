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
namespace bu = bbque::utils;

namespace bbque { namespace app {


WorkingMode::WorkingMode():
	hidden(false) {
	resources.binding_masks.resize(
		static_cast<uint16_t>(br::ResourceIdentifier::TYPE_COUNT));
	// Set the log string id
	strncpy(str_id, "", 12);
}

WorkingMode::WorkingMode(uint8_t _id,
		std::string const & _name,
		float _value):
	id(_id),
	name(_name),
	hidden(false) {
	// Get a logger
	logger = bu::Logger::GetLogger(AWM_NAMESPACE);

	// Value must be positive
	_value > 0 ? value.recipe = _value : value.recipe = 0;

	// Init the size of the scheduling bindings vector
	resources.binding_masks.resize(br::ResourceIdentifier::TYPE_COUNT);

	// Set the log string id
	snprintf(str_id, 15, "AWM{%d,%s}", id, name.c_str());

	// Default value for configuration time (if no profiled)
	memset(&config_time, 0, sizeof(ConfigTimeAttribute_t));
	config_time.normal = -1;
}

WorkingMode::~WorkingMode() {
	resources.requested.clear();
	if (resources.sync_bindings)
		resources.sync_bindings->clear();
	// Trash all the scheduling bindings
	if (!resources.sched_bindings.empty()) {
		logger->Fatal("resource sched bindings: %d",
				resources.sched_bindings.size());
		resources.sched_bindings.clear();
	}
}

WorkingMode::ExitCode_t WorkingMode::AddResourceUsage(
		std::string const & rsrc_path,
		uint32_t required_amount) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	br::ResourcePath::ExitCode_t rp_result;
	ExitCode_t result = WM_SUCCESS;

	// Init the resource path starting from the prefix
	br::ResourcePathPtr_t ppath(new br::ResourcePath(ra.GetPrefixPath()));
	if (!ppath) {
		logger->Error("%s AddResourceUsage: {%s} invalid prefix path",
				str_id,	ra.GetPrefixPath().ToString().c_str());
		return WM_RSRC_ERR_TYPE;
	}

	// Build the resource path object
	rp_result = ppath->Concat(rsrc_path);
	if (rp_result != br::ResourcePath::OK) {
		logger->Error("%s AddResourceUsage: {%s} invalid path",
				str_id,	rsrc_path.c_str());
		return WM_RSRC_ERR_TYPE;
	}

	// Check the existance of the resource required
	if (!ra.ExistResource(ppath)) {
		logger->Warn("%s AddResourceUsage: {%s} not found.",
				str_id, ppath->ToString().c_str());
		result = WM_RSRC_NOT_FOUND;
	}

	// Insert a new resource usage object in the map
	br::UsagePtr_t pusage(br::UsagePtr_t(new br::Usage(required_amount)));
	resources.requested.insert(
			std::pair<ResourcePathPtr_t, br::UsagePtr_t>(ppath, pusage));
	logger->Debug("%s AddResourceUsage: added {%s}\t[usage: %d]",
			str_id, ppath->ToString().c_str(), required_amount);

	return result;
}

WorkingMode::ExitCode_t WorkingMode::Validate() {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	br::UsagesMap_t::iterator usage_it, it_end;
	uint32_t total_amount;

	// Initialization
	usage_it = resources.requested.begin();
	it_end   = resources.requested.end();
	hidden   = false;

	// Map of resource usages requested
	for (; usage_it != it_end; ++usage_it) {
		// Current resource: path and amount required
		ResourcePathPtr_t const & rcp_path(usage_it->first);
		br::UsagePtr_t & rcp_pusage(usage_it->second);

		// Check the total amount available. Hide the AWM if the current total
		// amount available cannot satisfy the amount required.
		// Consider the resource template path, since the requested resource
		// can be mapped on more than one system/HW resource.
		total_amount = ra.Total(rcp_path, ResourceAccounter::TEMPLATE);
		if (total_amount < rcp_pusage->GetAmount()) {
			logger->Warn("%s Validate: {%s} usage required (%d) exceeds total (%d)",
					str_id, rcp_path->ToString().c_str(),
					rcp_pusage->GetAmount(), total_amount);
			hidden = true;
			logger->Warn("%s Validate: set to 'hidden'", str_id);
			return WM_RSRC_USAGE_EXCEEDS;
		}
	}

	return WM_SUCCESS;
}

uint32_t WorkingMode::ResourceUsageAmount(
		ResourcePathPtr_t ppath) const {
	br::UsagesMap_t::const_iterator r_it(resources.requested.begin());
	br::UsagesMap_t::const_iterator r_end(resources.requested.end());

	for (; r_it != r_end; ++r_it) {
		ResourcePathPtr_t const & wm_rp(r_it->first);
		if (ppath->Compare(*(wm_rp.get())) == br::ResourcePath::NOT_EQUAL)
			continue;
		return r_it->second->GetAmount();
	}
	return 0;
}

size_t WorkingMode::BindResource(
		br::ResourceIdentifier::Type_t r_type,
		br::ResID_t src_ID,
		br::ResID_t dst_ID,
		size_t b_refn,
		br::ResourceIdentifier::Type_t filter_rtype,
		br::ResourceBitset * filter_mask) {
	br::UsagesMap_t * src_pum;
	uint32_t b_count;
	size_t n_refn;
	std::map<size_t, br::UsagesMapPtr_t>::iterator pum_it;

	// Allocate a new temporary resource usages map
	br::UsagesMapPtr_t bind_pum(br::UsagesMapPtr_t(new br::UsagesMap_t()));

	if (b_refn == 0) {
		logger->Debug("[%s] BindResource: binding resources from recipe", str_id);
		src_pum = &resources.requested;
	}
	else {
		pum_it = resources.sched_bindings.find(b_refn);
		if (pum_it == resources.sched_bindings.end()) {
			logger->Error("[%s] BindResource: invalid binding reference [%ld]",
				str_id, b_refn);
			return 0;
		}
		src_pum = (pum_it->second).get();
	}

	// Binding
	b_count = br::ResourceBinder::Bind(
			*src_pum, r_type, src_ID, dst_ID, bind_pum,
			filter_rtype, filter_mask);
	if (b_count == 0) {
		logger->Warn("[%s] BindResource: nothing to bind", str_id);
		return 0;
	}

	// Store the resource binding
	n_refn = std::hash<std::string>()(BindingStr(r_type, src_ID, dst_ID, b_refn));
	resources.sched_bindings[n_refn] = bind_pum;
	logger->Info("[%s] BindResource: R{%s} refn[%ld] size:%d count:%d",
			str_id, br::ResourceIdentifier::StringFromType(r_type),
			n_refn, bind_pum->size(), b_count);
	return n_refn;
}

std::string WorkingMode::BindingStr(
		br::ResourceIdentifier::Type_t r_type,
		br::ResID_t src_ID,
		br::ResID_t dst_ID,
		size_t b_refn) {
	char tail_str[40];
	std::string str(br::ResourceIdentifier::TypeStr[r_type]);
	snprintf(tail_str, 40, ",%d,%d,%ld", src_ID, dst_ID, b_refn);
	str.append(tail_str);
	logger->Debug("BindingStr: %s", str.c_str());
	return str;
}

br::UsagesMapPtr_t WorkingMode::GetSchedResourceBinding(size_t b_refn) const {
	std::map<size_t, br::UsagesMapPtr_t>::const_iterator sched_it;
	sched_it = resources.sched_bindings.find(b_refn);
	if (sched_it == resources.sched_bindings.end()) {
		logger->Error("GetSchedResourceBinding: "
			"invalid reference [%ld]", b_refn);
		return nullptr;
	}
	return sched_it->second;
}

WorkingMode::ExitCode_t WorkingMode::SetResourceBinding(
		br::RViewToken_t vtok,
		size_t b_refn) {
	// Set the new binding / resource usages map
	resources.sync_bindings = GetSchedResourceBinding(b_refn);
	if (resources.sync_bindings == nullptr) {
		logger->Error("SetBinding: invalid scheduling binding [%ld]", b_refn);
		return WM_BIND_FAILED;
	}
	resources.sync_refn = b_refn;

	// Update the resource binding bit-masks
	UpdateBindingInfo(vtok, true);

	logger->Debug("SetBinding: resource binding [%ld] to allocate", b_refn);
	return WM_SUCCESS;
}


void WorkingMode::UpdateBindingInfo(
		br::RViewToken_t vtok,
		bool update_changed) {
	br::ResourceBitset new_mask;
	uint8_t r_type;

	// Update the resource binding bitmask (for each type)
	for (r_type = br::ResourceIdentifier::SYSTEM;
			r_type < br::ResourceIdentifier::TYPE_COUNT; ++r_type) {
		BindingInfo & bi(resources.binding_masks[r_type]);

		if (r_type == br::ResourceIdentifier::PROC_ELEMENT ||
			r_type == br::ResourceIdentifier::MEMORY) {
			logger->Debug("SetBinding: [%s] is a terminal resource",
					br::ResourceIdentifier::TypeStr[r_type]);
			// 'Deep' get bit-mask in this case
			new_mask = br::ResourceBinder::GetMask(
				resources.sched_bindings[resources.sync_refn],
				static_cast<br::ResourceIdentifier::Type_t>(r_type),
				br::ResourceIdentifier::CPU,
				R_ID_ANY, owner, vtok);
		}
		else {
			new_mask = br::ResourceBinder::GetMask(
				resources.sched_bindings[resources.sync_refn],
				static_cast<br::ResourceIdentifier::Type_t>(r_type));
		}
		logger->Debug("SetBinding: [%s] bitmask: %s",
				br::ResourceIdentifier::TypeStr[r_type],
				new_mask.ToStringCG().c_str());

		// Check if the bit-masks have changed only if required
		if (!update_changed)
			continue;

		// Update previous/current bit-masks
		if (new_mask.Count() == 0) continue;
		bi.prev = bi.curr;
		bi.curr = new_mask;

		// Set the flag if changed and print a log message
		bi.changed = bi.prev != new_mask;
		logger->Debug("%s SetBinding: R{%-3s} changed? [%d]",
				str_id, br::ResourceIdentifier::TypeStr[r_type],
				bi.changed);
	}
}

void WorkingMode::ClearResourceBinding() {
	uint8_t r_type = 0;
	resources.sync_bindings->clear();
	for (; r_type < br::ResourceIdentifier::TYPE_COUNT; ++r_type) {
		resources.binding_masks[r_type].curr =
			resources.binding_masks[r_type].prev;
	}
}

br::ResourceBitset WorkingMode::BindingSet(
		br::ResourceIdentifier::Type_t r_type) const {
	BindingInfo const & bi(resources.binding_masks[r_type]);
	return bi.curr;
}

br::ResourceBitset WorkingMode::BindingSetPrev(
		br::ResourceIdentifier::Type_t r_type) const {
   return resources.binding_masks[r_type].prev;
}

bool WorkingMode::BindingChanged(
		br::ResourceIdentifier::Type_t r_type) const {
   return resources.binding_masks[r_type].changed;
}

} // namespace app

} // namespace bbque

