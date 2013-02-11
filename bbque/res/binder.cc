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

#include <utility>

#include "bbque/resource_accounter.h"
#include "bbque/res/binder.h"
#include "bbque/res/bitset.h"
#include "bbque/res/resource_path.h"


#define MODULE_NAMESPACE 	"bq.rb"

namespace bbque { namespace res {


uint32_t ResourceBinder::Bind(
		UsagesMap_t const & src_um,
		ResourceIdentifier::Type_t r_type,
		ResID_t	src_r_id,
		ResID_t dst_r_id,
		UsagesMapPtr_t dst_pum) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	UsagesMap_t::const_iterator src_it, src_end;
	ResourcePath::ExitCode_t rp_result;
	uint32_t count = 0;

	// Sanity check
	if (r_type >= ResourceIdentifier::TYPE_COUNT)
		return 0;

	// Proceed with the resource binding...
	src_end = src_um.end();
	for (src_it = src_um.begin(); src_it != src_um.end(); ++src_it) {
		ResourcePathPtr_t const & src_ppath(src_it->first);
		UsagePtr_t const        & src_pusage(src_it->second);

		// Replace ID of the given resource type with a new ID
		ResourcePathPtr_t dst_ppath(new ResourcePath(src_ppath->ToString()));
		if (dst_ppath->NumLevels() == 0)
			return 0;
		rp_result = dst_ppath->ReplaceID(r_type, src_r_id, dst_r_id);

		// Set the bit corresponding to the bound ID
		if (rp_result == ResourcePath::OK) {
			DB(fprintf(stderr, FD("Bind: [%s] to [%s] done\n"),
					src_ppath->ToString().c_str(), dst_ppath->ToString().c_str());
			);
			++count;
		}
		else {
			DB(fprintf(stderr, FD("Bind: Nothing to do in [%s]\n"),
					src_ppath->ToString().c_str());
			);
		}

		// Create a new Usage object and set the binding list
		UsagePtr_t dst_pusage(new Usage(src_pusage->GetAmount()));
		dst_pusage->SetBindingList(ra.GetResources(dst_ppath));

		// Insert the resource usage object in the output map
		dst_pum->insert(
				std::pair<ResourcePathPtr_t, UsagePtr_t>(dst_ppath, dst_pusage));
	}
	return count;
}

ResourceBitset ResourceBinder::GetMask(
		UsagesMapPtr_t pum,
		ResourceIdentifier::Type_t r_type) {
	UsagesMap_t::iterator pum_it;
	ResourceBitset r_mask;
	ResID_t r_id;

	// Sanity check
	if (r_type >= ResourceIdentifier::TYPE_COUNT)
		return r_mask;

	// Scan the resource usages map
	for (pum_it = pum->begin();	pum_it != pum->end(); ++pum_it) {
		ResourcePathPtr_t const & ppath(pum_it->first);
		// Get the ID of the resource type in the path
		r_id = ppath->GetID(r_type);
		if ((r_id == R_ID_NONE) || (r_id == R_ID_ANY))
			continue;
		// Set the ID-th bit in the mask
		r_mask.Set(r_id);
	}
	return r_mask;
}

ResourceBinder::ExitCode_t ResourceBinder::Compatible(
		UsagesMapPtr_t src_pum,
		UsagesMapPtr_t dst_pum) {
	UsagesMap_t::iterator src_it, src_end;
	UsagesMap_t::iterator dst_it, dst_end;

	// Different size of the maps -> not compatible
	if (src_pum->size() != dst_pum->size())
		return NOT_COMPATIBLE;

	// Compare each resource path
	for (src_it = src_pum->begin(), dst_it = dst_pum->begin();
			src_it != src_end; ++src_it, ++dst_it) {
		ResourcePathPtr_t const & src_ppath(src_it->first);
		ResourcePathPtr_t const & dst_ppath(dst_it->first);
		// The path should be equal or have equal types (template)
		if (src_ppath->Compare(*(dst_ppath.get())) == ResourcePath::NOT_EQUAL) {
			return NOT_COMPATIBLE;
		}
	}

	return OK;
}

} // namespace res

} // namespace bbque

