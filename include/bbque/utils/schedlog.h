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

#ifndef BBQUE_SCHEDLOG_H_
#define BBQUE_SCHEDLOG_H_

#include "bbque/app/schedulable.h"

#define HM_TABLE_DIV1 "==========================================================================="
#define HM_TABLE_DIV2 "|------------------+------------+-------------+-------------|-------------|"
#define HM_TABLE_HEAD "|      APP:EXC     | STATE/SYNC |     CURRENT |        NEXT | AWM_NAME    |"

#define PRINT_NOTICE_IF_VERBOSE(verbose, text)\
	if (verbose)\
		logger->Notice(text);\
	else\
		DB(\
		logger->Debug(text);\
		);

namespace bbque {

namespace utils {


class SchedLog {

public:

	static void BuildProgressBar(
			uint64_t used, uint64_t total, char * prog_bar,
			uint8_t len, char symbol='=');

	static void BuildSchedStateLine(
			app::SchedPtr_t, char * state_line, size_t len);

protected:

	static void BuildStateStr(
			app::SchedPtr_t papp, char * state_str, size_t len);

};

} // namespace utils

} // namespace bbque

#endif // BBQUE_SCHEDLOG_H_

