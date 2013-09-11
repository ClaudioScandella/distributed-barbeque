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

#ifndef BBQUE_RESOURCE_BINDER_H_
#define BBQUE_RESOURCE_BINDER_H_

#include <cstdint>

#include "bbque/res/usage.h"


namespace bbque { namespace res {

class ResourceBitset;

/**
 * @class ResourceBinder
 *
 * This a static class providing methods to solve or provide information about
 * resource bindings.
 *
 * The binding is a mandatory step, through which a scheduling policy links
 * the resources requested in the AWMs (referenced by a resource path built
 * from the recipe) to the system resources (referenced by the resource path
 * registered by the Platform Proxy.
 */
class ResourceBinder {

public:

	/**
	 * @enum Exit codes
	 */
	enum ExitCode_t {
		OK         = 0 ,
		NOT_COMPATIBLE
	};

	/**
	 * @brief Bind resource usages to system resources
	 *
	 * @param src_um The map of resource usages to bind
	 * @param r_type The type of resource to bind
	 * @param src_r_id The ID of the resource to bind
	 * @param dst_r_id The ID of the system resource to which bind
	 * @param dst_pum A shared pointer to the map of bound resources to fill
	 *
	 * @return The number of resources on which the binding has been performed
	 */
	static uint32_t Bind(
			UsagesMap_t const & src_um,
			ResourceIdentifier::Type_t r_type,
			ResID_t	src_r_id,
			ResID_t dst_r_id,
			UsagesMapPtr_t dst_pum);

	/**
	 * @brief Retrieve IDs of a type of resource from a UsagesMap_t
	 *
	 * @param pum A shared pointer to the map of resource usages
	 * @param r_type The type of resource to consider
	 *
	 * @return A ResourceBitset object tracking all the IDs
	 */
	static ResourceBitset GetMask(
			UsagesMapPtr_t pum, ResourceIdentifier::Type_t r_type);

	static ResourceBitset GetMask(
			UsagesMap_t const & um, ResourceIdentifier::Type_t r_type);

	/**
	 * @brief Retrieve IDs of a type of resource under a scope
	 *
	 * <tt>
	 * Example:<br>
	 *
	 * Type:       PROC_ELEMENT
	 * Scope type: CPU
	 * Scope ID:   2
	 *
	 * Matching resource paths:<br>
	 *
	 * "sys0.cpu2.pe0"
	 * "sys0.cpu2.pe1"
	 * "sys0.cpu2.pe2"
	 * "sys0.cpu2.pe..."
	 *
	 * </tt>
	 *
	 * @param pum A shared pointer to the map of resource usages
	 * @param r_type The target type of resource
	 * @param r_type_scope The type of the scope resource
	 * @param r_type_id The ID of the scope resource
	 * @param papp [optional] The application using the resource
	 * @param vtok [optional] The resource state view to consider
	 *
	 * @return A ResourceBitset object tracking all the IDs
	 */
	static ResourceBitset GetMask(
		UsagesMapPtr_t pum,
		ResourceIdentifier::Type_t r_type,
		ResourceIdentifier::Type_t r_scope_type,
		ResID_t r_scope_id,
		AppSPtr_t papp, RViewToken_t vtok = 0);

	/**
	 * @brief Retrieve IDs of a type of resource from a ResourcePtrList_t
	 *
	 * @param rpl A list of resource descriptors
	 * @param r_type The type of resource to consider
	 *
	 * @return A ResourceBitset object tracking all the IDs
	 */
	static ResourceBitset GetMask(
			ResourcePtrList_t const & rpl,
			ResourceIdentifier::Type_t r_type,
			AppSPtr_t papp, RViewToken_t vtok = 0);

	/**
	 * @brief Check if two resource usages map are compatible for binding
	 *
	 * @param src_pum A shared pointer to the first map of resource usages
	 * @param dst_pum A shared pointer to the second map of resource usages
	 *
	 * @return OK if compatible, NOT_COMPATIBLE otherwise
	 */
	static ExitCode_t Compatible(
			UsagesMapPtr_t src_pum, UsagesMapPtr_t dst_pum);
};


} // namespace res

} // namespace bbque

#endif // BBQUE_RESOURCE_BINDER_H_


