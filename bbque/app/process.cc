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

#include "bbque/app/process.h"

#define MODULE_NAMESPACE "bq.pr"
#define MODULE_CONFIG    "Process"

namespace bbque { namespace app {


Process::Process(std::string const & _name, AppPid_t _pid, AppPrio_t _prio) {
	name = _name;
	pid = _pid;
	priority = _prio;
	type = Schedulable::Type::PROCESS;
	logger = bbque::utils::Logger::GetLogger(MODULE_NAMESPACE);
	// Format the application string identifier for logging purpose
	snprintf(str_id, SCHEDULABLE_ID_MAX_LEN, "%05d:%10s",
		Pid(), Name().substr(0,10).c_str());
}


} // namespace app

} // namespace bbque

