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

#ifndef BBQUE_RESOURCE_IDENTIFIER_H_
#define BBQUE_RESOURCE_IDENTIFIER_H_

#include <bitset>
#include <memory>
#include <string>

#define MAX_R_NAME_LEN       6

#define R_TYPE_SYSTEM        1
#define R_TYPE_GROUP         2
#define R_TYPE_CPU           4
#define R_TYPE_PROC_ELEMENT  8
#define R_TYPE_MEMORY        16
#define R_TYPE_ACCELERATOR   32
#define R_TYPE_INTERCONNECT  64
#define R_TYPE_IO            128

#define MAX_R_ID_NUM         64
#define R_ID_ANY            -1
#define R_ID_NONE           -2

namespace bbque { namespace res {


/* Forward declaration */
class ResourceIdentifier;

/** Data-type for the Resource IDs */
typedef int16_t ResID_t;
/** Shared pointer to a ResourceIdentifier object */
typedef std::shared_ptr<ResourceIdentifier> ResourceIdentifierPtr_t;


/**
 * @class ResourceIdentifier
 *
 * This is a base class providing data and function members to manage the
 * identification of a resource. More in detail, it provides access to the
 * type and the ID number of the resource.
 */
class ResourceIdentifier {

public:

	/**
	 * @enum Type_t
	 *
	 * The set of all the possible resource types
	 */
	enum Type_t {
		UNDEFINED = 0,
		SYSTEM       ,
		GROUP        ,
		CPU          ,
		ACCELERATOR  ,
		PROC_ELEMENT ,
		MEMORY       ,
		INTERCONNECT ,
		IO           ,

		TYPE_COUNT
	};

	/**
	 * @enum CResult_t
	 *
	 * Results due to a comparison operation
	 */
	enum CResult_t {
		EQUAL   = 0,
		EQUAL_TYPE ,
		NOT_EQUAL
	};

	/** Text strings related to each type */
	static const char * TypeStr[TYPE_COUNT];

	/**
	 * @brief Constructor
	 *
	 * @param type The resource type
	 * @param id The ID number of the resource
	 */
	ResourceIdentifier(Type_t type, ResID_t id);

	/**
	 * @brief Operator < overloading
	 *
	 * @return true if type < type of comparing resource identifier.
	 * if the type is the same, compare the ID numbers.
	 */
	bool operator< (ResourceIdentifier const &);

	/**
	 * @brief Resource identity as a text string
	 *
	 * @return A string made by type + ID
	 */
	inline std::string const & Name() const {
		return name;
	}

	/**
	 * @brief Get the resource ID number
	 *
	 * @return The ID number
	 */
	inline ResID_t ID() const {
		return id;
	}

	/**
	 * @brief Set the resource ID number
	 *
	 * @param _id The ID number to set
	 */
	void SetID(ResID_t _id);

	/**
	 * @brief Get the resource type
	 *
	 * @return The resource type
	 */
	inline Type_t Type() const {
		return type;
	}

	/**
	 * @brief Set the resource type
	 */
	inline void SetType(Type_t _type) {
		type < TYPE_COUNT? type = _type: type = UNDEFINED;
	}

	/**
	 * @brief Get the resource type from a text string
	 *
	 * @param _str a string object with the resource type in textual form
	 *
	 * @return The corresponding Type_t value
	 */
	static Type_t TypeFromString(std::string const & _str);

	static const char * StringFromType(Type_t);

	/**
	 * @brief Compare two resource identifiers
	 *
	 * @param ri The resource identifier to compare
	 * @return A CResult_t value.
	 * <li>
	 * 	<ul>EQUAL if type and ID are the same
	 * 	<ul>EQUAL_TYPE for equal type but different ID
	 * 	<ul>NOT_EQUAL when both ID and type are different
	 * </li>
	 */
	CResult_t Compare(ResourceIdentifier const & ri) const;

protected:

	/** Resource name (i.e. "mem0", "pe1", "dma1", ...) */
	std::string name;

	/** Type of resource */
	Type_t type;

	/** ID of the resource */
	ResID_t id;

};


}

}

#endif // BBQUE_RESOURCE_IDENTIFIER_H_

