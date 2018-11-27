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

#ifndef BBQUE_SCHEDULABLE_MANAGER_H_
#define BBQUE_SCHEDULABLE_MANAGER_H_


#include "bbque/app/schedulable.h"

namespace bbque {


class SchedulableManager {

public:

	/**
	 * @brief Dump a logline to report on current Status queue counts
	 */
	virtual void PrintStatusQ(bool verbose = false) const = 0;

	/**
	 * @brief Dump a logline to report on current Status queue counts
	 */
	virtual void PrintSyncQ(bool verbose = false) const = 0;

	/**
	 * @brief Dump a logline to report all applications status
	 *
	 * @param verbose print in INFO logleve is ture, in DEBUG if false
	 */
	virtual void PrintStatus(bool verbose = false) = 0;

};

} // namespace bbque

#endif // BBQUE_SCHEDULABLE_MANAGER_H_

