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

#ifndef BBQUE_PROCESS_H_
#define BBQUE_PROCESS_H_

#include "bbque/app/schedulable.h"
#include "bbque/utils/logging/logger.h"

namespace bbque { namespace app {

/**
 * @class Process
 * @brief This class is defined to instantiate descriptors of generic
 * processors (i.e. not AEM-integrated applications)
 */
class Process: public Schedulable {

public:

	/**
	 * @brief Constructur
	 * @param _pid the process id
	 * @param _prio the priority (optional)
	 */
	Process(AppPid_t _pid, AppPrio_t _prio=0) {
		pid = _pid;
		priority = _prio;
	}

	/**
	 * @brief Destructor
	 */
	virtual ~Process() { }

};


} // namespace app

} // namespace bbque

#endif // BBQUE_PROCESS_H_

