/*
 * Copyright (C) 2019  Politecnico di Milano
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

#ifndef BBQUE_UTILS_MAP_ITERATOR_H_
#define BBQUE_UTILS_MAP_ITERATOR_H_

#include <list>
#include <map>

namespace bbque {

namespace utils {

/*******************************************************************************
 *     In-Loop Erase Safe Iterator support
 ******************************************************************************/

// Forward declaration
template <class P> class MapIterator;

/**
 * @typedef MapIteratorRetainer_t
 * @brief Retainer list of ILES iterators
 * @see MapIterator
 */
template<class P>
using MapIteratorRetainer_t = std::list<struct MapIterator<P> *>;

/**
 * @class MapIterator
 * @brief "In Loop Erase Safe" ILES iterator on an Map_t maps
 *
 * This is an iterator wrapper object which is used to implement safe iterations on
 * mutable maps, where an erase could occours on a thread while another thread
 * is visiting the elements of the same container container.
 * A proper usage of such an ILES interator requires to visit the container
 * elements using a pair of provided functions "GetFirst" and "GetNext".
 * @see @ref GetFirst, @ref GetNext
 */

template <class P>
class MapIterator {

public:

	~MapIterator() {
		Release();
	};

	void Init(std::map<unsigned int, P> & m, MapIteratorRetainer_t<P> & rl) {
		map = &m;
		ret = &rl;
		it = map->begin();
	}

	void Retain() {
		ret->push_front(this);
	}

	void Release() {
		if (ret) ret->remove(this);
		ret = NULL;
	}

	void Update() {
		++it; updated = true;
	}

	void operator++(int) {
		if (!updated) ++it;
		updated = false;
	}

	bool End() {
		return (it == map->end());
	}

	P Get() {
		return (*it).second;
	}

	/** An interator on a UIDs map */
	typename std::map<unsigned int, P>::iterator it;

private:

	/** The map to visit */
	std::map<unsigned int, P> * map = NULL;

	/** A flag to track iterator validity */
	bool updated = false;

	/** The retantion list on which this has been inserted */
	MapIteratorRetainer_t<P> * ret = NULL;

};

} // namespace utils

} // namespace bbque

#endif // BBQUE_UTILS_MAP_ITERATOR_H_

