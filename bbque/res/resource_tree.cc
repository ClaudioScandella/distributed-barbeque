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

#include "bbque/res/resource_tree.h"

#include <iostream>

#include "bbque/modules_factory.h"
#include "bbque/res/resources.h"
#include "bbque/res/resource_path.h"
#include "bbque/res/resource_utils.h"
#include "bbque/utils/utility.h"


namespace bbque { namespace res {


ResourceTree::ResourceTree():
	max_depth(0),
	count(0) {

	// Get a logger
	bp::LoggerIF::Configuration conf(RESOURCE_TREE_NAMESPACE);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	assert(logger);

	// Initialize the root node
	root         = new ResourceNode_t;
	root->data   = ResourcePtr_t(new Resource("root"));
	root->parent = NULL;
	root->depth  = 0;
}

ResourcePtrList_t
ResourceTree::findList(ResourcePath & rsrc_path, uint16_t match_flags) const {
	ResourcePtrList_t matchings;
	ResourcePath::Iterator head_path(rsrc_path.Begin());
	ResourcePath::Iterator const & end_path(rsrc_path.End());

	// match_flags = "11x" is a not valid configuration
	if (match_flags & RT_MATCH_TYPE & RT_MATCH_MIXED)
		match_flags = RT_MATCH_MIXED;

	findNode(root, head_path, end_path, match_flags, matchings);
	return matchings;
}

ResourcePtr_t & ResourceTree::insert(ResourcePath const & rsrc_path) {
	ResourceNode_t * curr_node;
	ResourcePath::ConstIterator path_it, path_end;
	ResourceNodesList_t::iterator tree_it, tree_end;

	// Seeking on the last matching resource path level (tree node)
	curr_node = root;
	path_end  = rsrc_path.End();
	for (path_it = rsrc_path.Begin(); path_it != path_end; ++path_it) {
		ResourceIdentifierPtr_t const & prid(*path_it);
		// No children -> add the first one
		if (curr_node->children.empty()) {
			ResourcePtr_t pres(new Resource(prid->Type(), prid->ID()));
			curr_node = addChild(curr_node, pres);
			curr_node->depth > max_depth ?
				max_depth = curr_node->depth: max_depth;
			continue;
		}

		// Children
		tree_end = curr_node->children.end();
		logger->Debug("insert: %s has %d children",
				curr_node->data->Name().c_str(), curr_node->children.size());
		for (tree_it = curr_node->children.begin(); tree_it != tree_end;
				++tree_it) {
			// Current resource path level matches: go one level down
			ResourcePtr_t & pres((*tree_it)->data);
			if (pres->Compare(*(prid.get())) != Resource::EQUAL) {
				logger->Debug("%-4s != %-4s",
						pres->Name().c_str(),
						prid->Name().c_str());
				continue;
			}
			// Matching
			curr_node = (*tree_it);
			break;
		}

		// No matching: add a children node
		if (tree_it == tree_end) {
			ResourcePtr_t pres(new Resource(prid->Type(), prid->ID()));
			curr_node = addChild(curr_node, pres);
		}
	}
	++count;
	logger->Debug("insert: count = %d, depth: %d", count, max_depth);

	// Return the new object just created
	return curr_node->data;
}

bool ResourceTree::findNode(
		ResourceNode_t * curr_node,
		ResourcePath::Iterator & path_it,
		ResourcePath::Iterator const & path_end,
		uint16_t match_flags,
		ResourcePtrList_t & matchings) const {
	ResourceNodesList_t::iterator tree_it, tree_end;
	Resource::CResult_t rresult;
	bool found;

	// Sanity/end of path checks
	if ((!curr_node)                  ||
		(curr_node->children.empty()) ||
		(path_it == path_end))
		return false;

	// Look for the current resource path level
	logger->Debug("findNode: %s has %d children",
			curr_node->data->Name().c_str(), curr_node->children.size());
	tree_end = curr_node->children.end();
	for (tree_it = curr_node->children.begin(); tree_it != tree_end; ++tree_it) {
		ResourcePtr_t & pres((*tree_it)->data);
		ResourceIdentifierPtr_t & prid(*path_it);
		found = false;

		// Compare the resource identities (type and ID)
		rresult = pres->Compare(*(prid.get()));
		logger->Debug("findNode: compare T:%4s to P:%4s = %d [match_flags %d]",
				pres->Name().c_str(), prid->Name().c_str(),
				rresult, match_flags);

		// Traverse the resource tree according to the comparison result
		switch (rresult) {
		case Resource::EQUAL_TYPE:
			//  Skip if mixed matching required but resource IDs do not match
			if (match_flags & RT_MATCH_MIXED) {
				if ((prid->ID() != R_ID_NONE) && (prid->ID() != R_ID_ANY)) {
					continue;
				}
			}
			// Skip if no type matching required
			else if (!(match_flags & RT_MATCH_TYPE)) {
				continue;
			}
		case Resource::EQUAL:
			if (path_it != path_end)
				// Go deeper in the resource tree
				findNode(*tree_it, ++path_it, path_end, match_flags, matchings);
			found = true;
			break;
		case Resource::NOT_EQUAL:
			continue;
		}

		// End of the resource path, and resource identity matching?
		// Y: Add the the resource descriptor (from tree) to the matchings list
		// N: Continue recursively
		if ((path_it == path_end) && (found)) {
			matchings.push_back(pres);
			logger->Debug("findNode: added back %s [%d]",
					pres->Name().c_str(), matchings.size());
			// Back to a "valid" iterator
			--path_it;
		}

		// Stop whether only the first matching has been required
		if (match_flags & RT_MATCH_FIRST)
			break;
	}

	// Back to one level up
	--path_it;
	logger->Debug("findNode: back to one level up");
	return !matchings.empty();
}

ResourceTree::ResourceNode_t *
ResourceTree::addChild(ResourceNode_t * curr_node, ResourcePtr_t pres) {
	// Create the new resource node
	ResourceNode_t * new_node = new ResourceNode_t;
	new_node->data   = pres;
	new_node->parent = curr_node;
	new_node->depth  = curr_node->depth + 1;

	// Append it as child of the current node
	curr_node->children.push_back(new_node);
	return new_node;
}

void ResourceTree::print_children(ResourceNode_t * _node, int _depth) {
	// Increase the level of depth
	++_depth;

	// Print all the children
	ResourceNodesList_t::iterator it(_node->children.begin());
	ResourceNodesList_t::iterator end(_node->children.end());
	for (; it != end; ++it) {
		// Child name
		for (int i= 0; i < _depth-1; ++i)
			logger->Debug("\t");

		logger->Debug("|-------%s", (*it)->data->Name().c_str());

		// Recursive call if there are some children
		if (!(*it)->children.empty())
			print_children(*it, _depth);
	}
}

void ResourceTree::clear_node(ResourceNode_t * _node) {
	ResourceNodesList_t::iterator it(_node->children.begin());
	ResourceNodesList_t::iterator end(_node->children.end());

	// Recursive clear
	for (; it != end; ++it) {
		if (!(*it)->children.empty())
			clear_node(*it);
		(*it)->children.clear();
	}
}

}   // namespace res

}   // namespace bbque

