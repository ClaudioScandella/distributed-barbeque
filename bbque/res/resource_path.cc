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

#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_path.h"

#define MODULE_NAMESPACE "bq.rp"

namespace bbque { namespace res {

ResourcePath::ResourcePath(std::string const & r_path):
		global_type(ResourceIdentifier::UNDEFINED),
		level_count(0) {
	std::string head, tail;
	std::string r_name;
	ResID_t r_id;
	ExitCode_t result;

	// Get a logger module
	bp::LoggerIF::Configuration conf(MODULE_NAMESPACE);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	logger->Debug("RP{%s} object construction", r_path.c_str());

	// Iterate over all the resources in the path string
	tail = r_path;
	do {
		// Get the resource ID and the name (string format type)
		head = ResourcePathUtils::SplitAndPop(tail);
		ResourcePathUtils::GetNameID(head, r_name, r_id);

		// Append a new resource identifier to the list
		result = Append(r_name, r_id);
		if (result != OK) {
			logger->Debug("RP{%s}: Cannot append type '%d'",
					r_path.c_str(), result);
			return;
		}
	} while (!tail.empty());
}

ResourcePath::~ResourcePath() {
	identifiers.clear();
	types_idx.clear();
}

bool ResourcePath::operator< (ResourcePath const & cmp_path) {
	ResourcePath::ConstIterator it, cmp_it;
	it     = identifiers.begin();
	cmp_it = cmp_path.Begin();

	// Per-level comparison of resource identifiers
	for (; it != identifiers.end(), cmp_it != cmp_path.End(); ++it, ++cmp_it) {
		ResourceIdentifier & rid(*((*it).get()));
		ResourceIdentifier & cmp_rid(*((*cmp_it).get()));

		if (rid < cmp_rid)
			return true;
	}
	return false;
}

ResourcePath::CResult_t ResourcePath::Compare(
		ResourcePath const & cmp_path) {
	ResourcePath::ConstIterator it, cmp_it;
	ResourceIdentifierPtr_t prid, pcmp_rid;
	CResult_t result;

	// Size checking
	if (identifiers.size() != cmp_path.NumLevels())
		return NOT_EQUAL;

	// Initialize iterators
	it     = identifiers.begin();
	cmp_it = cmp_path.Begin();
	result = EQUAL;

	// Per-level comparison of resource identifiers
	for (; it != identifiers.end(); ++it, ++cmp_it) {
		prid     = (*it);
		pcmp_rid = (*cmp_it);
		// Compare...
		if (prid->Compare(*(pcmp_rid.get())) == ResourceIdentifier::NOT_EQUAL)
			return NOT_EQUAL;
		else if (prid->Compare(*(pcmp_rid.get())) == ResourceIdentifier::EQUAL_TYPE)
			result = EQUAL_TYPES;
	}

	logger->Debug("Resource identifier comparison: %s EQUAL to %s",
			prid->Name().c_str(), pcmp_rid->Name().c_str());
	return result;
}

ResourcePath::ExitCode_t ResourcePath::Append(
		std::string const & r_name,
		ResID_t r_id) {
	ResourceIdentifier::Type_t r_type;

	// Resolve resource type from name first
	r_type = ResourceIdentifier::TypeFromString(r_name);
	logger->Debug("Append: S:%s T:%d ID:%d", r_name.c_str(), r_type, r_id);
	return Append(r_type, r_id);
}

ResourcePath::ExitCode_t ResourcePath::Append(
		ResourceIdentifier::Type_t r_type,
		ResID_t r_id) {
	ResourceIdentifierPtr_t prid;

	// Resource type checking
	if (r_type >= ResourceIdentifier::TYPE_COUNT) {
		logger->Error("Append: resource type [%d] unexpected", r_type);
		return ERR_UNKN_TYPE;
	}

	// Append the new resource identifier (sp) to the list
	prid = ResourceIdentifierPtr_t(new ResourceIdentifier(r_type, r_id));
	identifiers.push_back(prid);

	// Set the info about resource type
	if (types_bits.test(r_type)) {
		logger->Debug("Append: resource type [%d] already in the path", r_type);
		return ERR_USED_TYPE;
	}
	types_bits.set(r_type);
	types_idx.insert(std::pair<uint16_t, uint8_t>(r_type, level_count));
	global_type = r_type;

	// Build the text string
	if (level_count > 0)
		str.append(".");
	str.append(prid->Name());

	// Increase the levels counter
	++level_count;
	logger->Debug("Append: R{%s}, @%d, bs:%s",
			prid->Name().c_str(), types_idx[r_type],
			types_bits.to_string().c_str());
	logger->Debug("Append: SP:'%s', count: %d",
			str.c_str(), level_count);
	return OK;
}

ResourceIdentifierPtr_t ResourcePath::GetIdentifier(
		ResourceIdentifier::Type_t r_type) const {
	std::unordered_map<uint16_t, uint8_t>::const_iterator index_it;
	// Look for the vector position of the resource identifier by type
	index_it = types_idx.find(static_cast<uint16_t>(r_type));
	if (index_it == types_idx.end())
		return ResourceIdentifierPtr_t();
	// Get the ID from the resource identifier in the vector
	logger->Debug("GetIdentifier: type %s @pos:%d",
			ResourceIdentifier::TypeStr[r_type], (*index_it).second);
	return identifiers.at((*index_it).second);
}

bool ResourcePath::IsTemplate() const {
	ConstIterator rid_it(identifiers.begin());
	ConstIterator rid_end(identifiers.end());
	// A template has all the resources IDs unset
	for (; rid_it != rid_end; ++rid_it) {
		if (((*rid_it)->ID() != R_ID_NONE) &&
			((*rid_it)->ID() != R_ID_ANY))
			return false;
	}
	return true;
}

ResourcePath::ExitCode_t ResourcePath::ReplaceID(
		ResourceIdentifier::Type_t r_type,
		ResID_t src_r_id,
		ResID_t dst_r_id) {
	ResourceIdentifierPtr_t prid(GetIdentifier(r_type));
	if (!prid)
		return ERR_UNKN_TYPE;
	logger->Debug("ReplaceID: replace %s to ID[%d]",
			prid->Name().c_str(), dst_r_id);

	if ((src_r_id != R_ID_ANY) && (prid->ID() != src_r_id))
		return WRN_MISS_ID;

	prid->SetID(dst_r_id);
	logger->Debug("ReplaceID: from %d to %d, DONE",
			src_r_id, prid->ID());
	id_changed = true;

	return OK;
}

ResID_t ResourcePath::GetID(ResourceIdentifier::Type_t r_type) const {
	ResourceIdentifierPtr_t prid(GetIdentifier(r_type));
	if (!prid)
		return R_ID_NONE;
	return prid->ID();
}

std::string const & ResourcePath::ToString() {
	ResourcePath::ConstIterator it;
	std::string new_str;

	// If no ID has changed, return the saved string
	if (!id_changed)
		return str;
	// The resource identifiers
	for (it = identifiers.begin(); it != identifiers.end(); ++it) {
		if (it != identifiers.begin())
			new_str.append(".");
		new_str.append((*it)->Name());
	}
	// Update the string
	str = new_str;
	id_changed = false;

	return str;
}



} // namespace res

} // namespace bbque
