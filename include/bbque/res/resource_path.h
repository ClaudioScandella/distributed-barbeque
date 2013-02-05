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

#ifndef BBQUE_RESOURCE_PATH_H_
#define BBQUE_RESOURCE_PATH_H_

#include <bitset>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bbque/modules_factory.h"
#include "bbque/res/identifier.h"

#define MAX_NUM_LEVELS    	10
#define MAX_LEN_RPATH_STR 	(MAX_NUM_LEVELS * MAX_R_NAME_LEN)

namespace bp = bbque::plugins;

namespace bbque { namespace res {


/**
 * @class ResourcePath
 *
 * A resource path is a sort of reference object resource descriptors.
 * Instances of this class can be exploited for querying the status of a
 * specific resource, or a set of them.
 *
 * Basically, the resource path is based on a chain of ResourceIdentifier
 * objects, where each object is viewed as a "level" in a sort of namespace
 * fashion.
 *
 * Example: For resource path (string) "sys0.cpu1.pe2", a ResourcePath object
 * will create 3 ResourceIdentifier, having respectively type SYSTEM and ID 0,
 * type CPU and ID 1, type PROC_ELEMENT and ID 2.
 *
 * @see ResourceIdentifier for further details.
 */
class ResourcePath {

public:

	/** Wrapper types */
	typedef std::vector<ResourceIdentifierPtr_t>::iterator Iterator;
	typedef std::vector<ResourceIdentifierPtr_t>::const_iterator ConstIterator;

	/**
	 * @enum ExitCode_t
	 */
	enum ExitCode_t {
		OK        = 0 ,
		WRN_MISS_ID   ,
		ERR_UNKN_TYPE ,
		ERR_USED_TYPE
	};

	/**
	 * @enum ExitCode_t
	 *
	 * Results of a comparison operation
	 */
	enum CResult_t {
		EQUAL   = 0 ,
		EQUAL_TYPES ,
		NOT_EQUAL
	};

	/**
	 * @enum PathClass
	 * @brief The class of resource path specified in the query functions
	 */
	enum Class_t {
		UNDEFINED = 0,
		/** Exact resource path matching (type+ID). <br>
		 *  Example: sys1.cpu2.pe0 */
		EXACT ,
		/** Type matching if no ID provided, otherwise type+ID. <br>
		 *  Example: sys1.cpu.pe0  */
		MIXED ,
		/** Only type matching. <br>
		 *  Example: sys.cpu.pe    */
		TEMPLATE
	};

	/**
	 * @brief Constructor
	 *
	 * Builds a ResourcePath object from a resource path string
	 *
	 * @param r_path The resource path string
	 */
	ResourcePath(std::string const & r_path);

	/**
	 * @brief Destructor
	 */
	~ResourcePath();

	/**
	 * @brief Comparison for ordering
	 *
	 * @param r_path The ResourcePath to compare
	 *
	 * @return true if this resource path is "lower" than the one to compare
	 */
	bool operator< (ResourcePath const & r_path);

	/**
	 * @brief Compare two resource identifiers
	 *
	 * @param r_path The ResourcePath to compare
	 *
	 * @return The result of the comparison
	 */
	CResult_t Compare(ResourcePath const & r_path);

	/**
	 * @brief Append a resource type+id
	 *
	 * @param r_type_str The type in char string format
	 * @param r_id The integer ID
	 *
	 * @return OK for success, ERR_UNKN_TYPE for unknown resource type,
	 * ERR_USED_TYPE if the type has been already included in the path
	 */
	ExitCode_t Append(std::string const & r_type_str, ResID_t r_id);

	/**
	 * @brief Append a resource type+id
	 *
	 * @param r_type_str The resource type
	 * @param r_id The integer ID
	 *
	 * @return OK for success, ERR_UNKN_TYPE for unknown resource type,
	 * ERR_USED_TYPE if the type has been already included in the path
	 */
	ExitCode_t Append(ResourceIdentifier::Type_t r_type, ResID_t r_id);

	/**
	 * @brief Get the type of resource referenced by the path
	 *
	 * Example: path "sys.cpu.mem" will have global type equal to MEMORY
	 *
	 * @return The related @ref ResourceIdentifier::Type_t value
	 **/
	inline ResourceIdentifier::Type_t Type() const {
		return global_type;
	}

	/** Iterators **/

	ResourcePath::Iterator Begin() {
		return identifiers.begin();
	}

	ResourcePath::ConstIterator Begin() const {
		return identifiers.begin();
	}

	ResourcePath::Iterator End() {
		return identifiers.end();
	}

	ResourcePath::ConstIterator End() const {
		return identifiers.end();
	}

	/**
	 * @brief Get the ID associated to a resource (type) in the path
	 */
	ResID_t GetID(ResourceIdentifier::Type_t r_type) const;

	/**
	 * @brief Replace the ID associated to a resource (type) in the path
	 *
	 * @return WRN_MISS_ID if no valid ID has been specified.
	 *	ERR_UNKN_TYPE if no valid type has been specified.
	 */
	ExitCode_t ReplaceID(ResourceIdentifier::Type_t r_type,
			ResID_t src_r_id, ResID_t dst_r_id);

	/**
	 * @brief Check if the resource path is of "template" class
	 *
	 * A template path does not feature any ID.
	 * Example: "sys.cpu.mem"
	 *
	 * @return true if the path is a template, false otherwise
	 */
	bool IsTemplate() const;

	/**  Utility member functions  **/

	/**
	 * @brief Return the resource path in text string format
	 */
	std::string const & ToString();

	/**
	 * @brief Return the number of levels of the path
	 */
	inline size_t NumLevels() const {
		return identifiers.size();
	}

private:

	/** Logger instance */
	bp::LoggerIF *logger;

	/** Resource identifiers: one for each level of the the path */
	std::vector<ResourceIdentifierPtr_t> identifiers;

	/** Keep track of the resource types in the path */
	std::bitset<ResourceIdentifier::TYPE_COUNT> types_bits;

	/** Keep track of the position of the resource type in the vector */
	std::unordered_map<uint16_t, uint8_t> types_idx;

	/** The char string format */
	std::string str;

	/** The type of resource referenced by the path. */
	ResourceIdentifier::Type_t global_type;

	/** Number of levels counter */
	uint8_t level_count;

	/** Keep track of an ID replacement */
	bool id_changed;

	/**
	 * @brief Retrieve a resource identifier
	 *
	 * @param r_type The resource type
	 *
	 * @return A shared pointer to the resource identifier object
	 */
	ResourceIdentifierPtr_t GetIdentifier(
			ResourceIdentifier::Type_t r_type) const;

};

} // namespace bbque

} // namespace res

#endif // BBQUE_RESOURCE_PATH_H_


