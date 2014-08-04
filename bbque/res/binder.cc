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
		br::ResourceIdentifier::Type_t r_type,
		br::ResID_t	src_r_id,
		br::ResID_t dst_r_id,
		UsagesMapPtr_t dst_pum,
		br::ResourceIdentifier::Type_t filter_rtype,
		ResourceBitset * filter_mask) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	UsagesMap_t::const_iterator src_it, src_end;
	ResourcePath::ExitCode_t rp_result;
	ResourcePtrList_t r_list;
	uint32_t count = 0;

	// Sanity check
	if (r_type >= br::ResourceIdentifier::TYPE_COUNT)
		return 0;

	// Proceed with the resource binding...
	src_end = src_um.end();
	for (src_it = src_um.begin(); src_it != src_um.end(); ++src_it) {
		ResourcePathPtr_t const & src_ppath(src_it->first);
		UsagePtr_t const        & src_pusage(src_it->second);

		ResourcePathPtr_t dst_ppath(new ResourcePath(src_ppath->ToString()));
		if (dst_ppath->NumLevels() == 0)
			return 0;

		// Replace ID of the given resource type with the bound ID
		rp_result = dst_ppath->ReplaceID(r_type, src_r_id, dst_r_id);
		if (rp_result == ResourcePath::OK) {
			DB(fprintf(stderr, FD("Bind: [%s] to [%s] done\n"),
					src_ppath->ToString().c_str(),
					dst_ppath->ToString().c_str());
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
		r_list = ra.GetResources(dst_ppath);
		if ((filter_rtype != br::ResourceIdentifier::UNDEFINED)
				&& (filter_mask != nullptr))
			dst_pusage->SetResourcesList(r_list, filter_rtype, *filter_mask);
		else
			dst_pusage->SetResourcesList(r_list);

		// Insert the resource usage object in the output map
		dst_pum->insert(
				std::pair<ResourcePathPtr_t, UsagePtr_t>(dst_ppath, dst_pusage));
	}
	return count;
}

ResourceBitset ResourceBinder::GetMask(
		UsagesMapPtr_t pum,
		br::ResourceIdentifier::Type_t r_type) {
	UsagesMap_t::iterator pum_it;
	ResourceBitset r_mask;
	br::ResID_t r_id;

	// Sanity check
	if (r_type >= br::ResourceIdentifier::TYPE_COUNT)
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

ResourceBitset ResourceBinder::GetMask(
		UsagesMap_t const & um,
		br::ResourceIdentifier::Type_t r_type) {
	UsagesMap_t::const_iterator um_it;
	ResourceBitset r_mask;
	br::ResID_t r_id;

	// Sanity check
	if (r_type >= br::ResourceIdentifier::TYPE_COUNT)
		return r_mask;

	// Scan the resource usages map
	for (um_it = um.begin(); um_it != um.end(); ++um_it) {
		ResourcePathPtr_t const & ppath(um_it->first);
		// Get the ID of the resource type in the path
		r_id = ppath->GetID(r_type);
		if ((r_id == R_ID_NONE) || (r_id == R_ID_ANY))
			continue;
		// Set the ID-th bit in the mask
		r_mask.Set(r_id);
	}
	return r_mask;
}

ResourceBitset ResourceBinder::GetMask(
		UsagesMapPtr_t pum,
		br::ResourceIdentifier::Type_t r_type,
		br::ResourceIdentifier::Type_t r_scope_type,
		br::ResID_t r_scope_id,
		AppSPtr_t papp,
		RViewToken_t vtok) {
	UsagesMap_t::iterator pum_it;
	br::ResourceIdentifier::Type_t rp_type;
	ResourceBitset r_mask;
	br::ResID_t rs_id;

	// Sanity check
	if ((r_type >= br::ResourceIdentifier::TYPE_COUNT)       ||
		(r_scope_type >= br::ResourceIdentifier::TYPE_COUNT))
		return r_mask;

	DB(fprintf(stderr, FD("GetMask: scope='%s%d' r='%s' view=%lu\n"),
				br::ResourceIdentifier::TypeStr[r_scope_type], r_scope_id,
				br::ResourceIdentifier::TypeStr[r_type], vtok););

	// Scan the resource usages map
	for (pum_it = pum->begin();	pum_it != pum->end(); ++pum_it) {
		ResourcePathPtr_t const & ppath(pum_it->first);
		UsagePtr_t const & pusage(pum_it->second);

		// From the resource path extract the ID of the "scope" resource type,
		// and the type of resource referenced
		rs_id   = ppath->GetID(r_scope_type);
		rp_type = ppath->Type();

		// If the ID has been found and the type of resource is the one
		// requested, then set the the bit
		DB(fprintf(stderr, FD("GetMask: ID search:%d found:%d\n"), r_scope_id, rs_id));
		if (((r_scope_id == R_ID_ANY) || (rs_id == r_scope_id))
				&& (rp_type == r_type)) {
			DB(fprintf(stderr, FD("GetMask: scope found in R{%s}!\n"),
						ppath->ToString().c_str()));
			return GetMask(pusage->GetResourcesList(), r_type, papp, vtok);
		}
	}
	return r_mask;
}

ResourceBitset ResourceBinder::GetMask(
		ResourcePtrList_t const & rpl,
		br::ResourceIdentifier::Type_t r_type,
		AppSPtr_t papp,
		RViewToken_t vtok) {
	ResourceBitset r_mask;

	// Sanity check
	if (r_type >= br::ResourceIdentifier::TYPE_COUNT)
		return r_mask;

	// Scan the resources list
	for (ResourcePtr_t const & pres: rpl) {
		DB(fprintf(stderr, FD("GetMaskL: %s\n"), pres->Name().c_str()));
		if (papp && (pres->ApplicationUsage(papp, vtok) == 0))
			continue;
		if (pres->Type() == r_type)
			r_mask.Set(pres->ID());
	}
	DB(fprintf(stderr, FD("GetMaskL: %s\n"), r_mask.ToString().c_str()));
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

