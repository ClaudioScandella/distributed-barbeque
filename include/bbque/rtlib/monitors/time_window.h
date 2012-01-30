/**
 *      @file   time_window.h
 *      @class  TimeWindow
 *      @brief  Type of window used by time monitor
 *
 * This class provides a window specifically created for the time monitor.
 *
 *     @author  Andrea Di Gesare , andrea.digesare@gmail.com
 *     @author  Vincenzo Consales , vincenzo.consales@gmail.com
 *
 *   @internal
 *     Created
 *    Revision
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Di Gesare Andrea, Consales Vincenzo
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =============================================================================
 */

#ifndef BBQUE_TIME_WINDOW_H_
#define BBQUE_TIME_WINDOW_H_

#include <chrono>
#include "generic_window.h"

class TimeWindow : public GenericWindow <uint32_t> {
public:
	TimeWindow(std::vector<GenericWindow<uint32_t>::Target> targets):
		GenericWindow<uint32_t>(targets){}

	TimeWindow(std::vector<GenericWindow<uint32_t>::Target> targets,
		   uint16_t windowSize):
			   GenericWindow<uint32_t>(targets, windowSize){}
	/**
	 * @brief The start time of the basic time monitor
	 */
	std::chrono::monotonic_clock::time_point tStart;
	/**
	 * @brief The stop time of the basic time monitor
	 */
	std::chrono::monotonic_clock::time_point tStop;
	/**
	 * @brief Indicates whether a starting time has been set or not
	 */
	bool started;
};

#endif /* BBQUE_TIME_WINDOW_H_ */
