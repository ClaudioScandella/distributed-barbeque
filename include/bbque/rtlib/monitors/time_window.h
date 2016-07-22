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

#ifndef BBQUE_TIME_WINDOW_H_
#define BBQUE_TIME_WINDOW_H_

#include <bbque/cpp11/chrono.h>

#include <bbque/monitors/generic_window.h>

namespace bbque { namespace rtlib { namespace as {

/**
 * @class TimeWindow
 * @ingroup rtlib_sec04_mon_time
 *
 * @details
 * This class provides a window specifically created for the time monitor.
 */
class TimeWindow : public GenericWindow <uint32_t> {
public:

	typedef std::vector<TimeWindow::Target> Targets;
	typedef std::shared_ptr<Targets> TargetsPtr;

	/**
	 * @brief Initializes internal variables
	 */
	TimeWindow(std::string metricName,
		   TargetsPtr targets,
		   uint16_t windowSize = defaultWindowSize) :
			   GenericWindow<uint32_t>(metricName,
						   targets,
						   windowSize) {
	}

	/**
	 * @brief Initializes internal variables
	 */
	TimeWindow(uint16_t windowSize = defaultWindowSize) :
			GenericWindow<uint32_t>(windowSize) {
	}
	/**
	 * @brief The start time of the basic time monitor
	 */
	std::chrono::steady_clock::time_point tStart;

	/**
	 * @brief The stop time of the basic time monitor
	 */
	std::chrono::steady_clock::time_point tStop;

	/**
	 * @brief Indicates whether a starting time has been set or not
	 */
	bool started = false;

};

} // namespace as

} // namespace rtlib

} // namespace bbque

#endif /* BBQUE_TIME_WINDOW_H_ */
