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

#include "bbque/res/usage.h"
#include "bbque/res/resource_path.h"

namespace bbque { namespace res {


Usage::Usage(uint32_t usage_amount):
	amount(usage_amount) {
}

Usage::~Usage() {
	resources.clear();
}

uint32_t Usage::GetAmount() {
	return amount;
}

ResourcePtrList_t & Usage::GetResourcesList() {
	return resources;
}

void Usage::SetResourcesList(ResourcePtrList_t & r_list) {
	resources         = r_list;
	first_resource_it = r_list.begin();
	last_resource_it  = r_list.end();
}

void Usage::SetResourcesList(
		ResourcePtrList_t & r_list,
		br::ResourceIdentifier::Type_t filter_rtype,
		ResourceBitset & filter_mask) {
	ResourcePtrListIterator_t r_it;

	if (r_list.empty())
		return;

	resources.clear();
	r_it = r_list.begin();
	for (; r_it != r_list.end(); ++r_it) {
		ResourcePtr_t rsrc(*r_it);

		// Is there a filter on the resources?
		if ((filter_rtype == rsrc->Type()) &&
				(!filter_mask.Test((rsrc->ID()))))
			continue;

		// Copy the resource in the list and keep track of the boundary
		// iterators
		resources.push_back(rsrc);
		last_resource_it = r_it;
		if (first_resource_it == resources.end())
			first_resource_it = r_it;
	}
}

bool Usage::EmptyResourcesList() {
	return resources.empty();
}

ResourcePtr_t Usage::GetFirstResource(
		ResourcePtrListIterator_t & it) {
	// Check if 'first_resource_it' points to a valid resource descriptor
	if (first_resource_it == resources.end())
		return ResourcePtr_t();

	// Set the argument iterator and return the shared pointer to the
	// resource descriptor
	it = first_resource_it;
	return (*it);
}

ResourcePtr_t Usage::GetNextResource(
		ResourcePtrListIterator_t & it) {
	do {
		// Next resource used by the application
		++it;

		// Return null if there are no more resources
		if ((it == resources.end()) || (it == last_resource_it))
			return ResourcePtr_t();
	} while ((*it)->ApplicationUsage(owner_app, status_view) == 0);

	// Return the shared pointer to the resource descriptor
	return (*it);
}

Usage::ExitCode_t Usage::TrackFirstResource(
		AppSPtr_t const & papp,
		ResourcePtrListIterator_t & first_it,
		RViewToken_t vtok) {
	if (!papp)
		return RU_ERR_NULL_POINTER;

	status_view = vtok;
	owner_app   = papp;
	first_resource_it  = first_it;

	return RU_OK;
}

Usage::ExitCode_t Usage::TrackLastResource(
		AppSPtr_t const & papp,
		ResourcePtrListIterator_t & last_it,
		RViewToken_t vtok) {
	if (!papp)
		return RU_ERR_NULL_POINTER;

	if (!owner_app)
		return RU_ERR_APP_MISMATCH;

	if (vtok != status_view)
		return RU_ERR_VIEW_MISMATCH;

	last_resource_it = last_it;
	return RU_OK;
}

} // namespace res

} // namespace bbque
