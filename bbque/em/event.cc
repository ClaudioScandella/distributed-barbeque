/*
 * Copyright (C) 2016  Politecnico di Milano
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

#include "bbque/em/event.h"

namespace bbque {

namespace em {

Event::Event(bool const & valid, std::string const & module,
	std::string const & resource, std::string const & application,
	std::string const & type, const int & value):
	valid(valid),
	timestamp(0),
	module(module),
	resource(resource),
	application(application),
	type(type),
	value(value) {

}

Event::~Event() {

}

} // namespace em

} // namespace bbque

