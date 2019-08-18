/*
 * Copyright (C) 2017  Politecnico di Milano
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

#ifndef BBQUE_TG_EVENT_H_
#define BBQUE_TG_EVENT_H_

#include <cstdint>
#include <boost/serialization/list.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace bbque {

/**
 * \class Event
 * \brief This class represents a synchronization resource used to track
 * events like tasks start/stop or buffers read/write
 */
class Event {

public:
	/**
	 * \brief Constructor
	 */
	Event() {}

	/**
	 * \brief Constructor
	 * \param _id Identification number
	 * \param _addr Optional memory address associated to the resource
	 */
	Event(uint32_t _id, uint32_t _addr = 0):
		id(_id), phy_addr(_addr) {}

	/**
	 * \brief Destructor
	 */
	virtual ~Event() {}

	/**
	 * \brief Identification number
	 */
	inline uint32_t Id() const { return id; }

	/**
	 * \brief Physical address of the synchronization resource
	 */
	inline uint32_t PhysicalAddress() const { return phy_addr; }

	/**
	 * \brief Set the physical address
	 * \param address The memory address
	 */
	inline void SetPhysicalAddress(uint32_t address) {
		phy_addr = address;
	}

private:

	uint32_t id;

	uint32_t phy_addr;


	friend class boost::serialization::access;

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		(void) version;
		ar & id;
		ar & phy_addr;
	}

};

} // namespace bbque

#endif // BBQUE_TG_EVENT_H_
