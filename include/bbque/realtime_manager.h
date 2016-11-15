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

#ifndef BBQUE_REALTIME_MANAGER_H_
#define BBQUE_REALTIME_MANAGER_H_

#include "bbque/app/application_conf.h"

#ifndef CONFIG_BBQUE_RT
	#error "You must not include this file if CONFIG_BBQUE_RT is not enabled."
#endif

#ifndef CONFIG_TARGET_LINUX
	#error "Real-Time is available only in Linux platforms."
#endif

namespace bbque {

/**
 * @class RealTimeManager
 *
 * @brief Provides the function to manage the real time processes, e.g. the
 *        interface with the Linux syscalls
 */
class RealTimeManager {

	/**
	 * @brief The errors enumeration
	 */
	enum ExitCode_t {
		RT_OK,
		RT_GENERIC_ERROR
	};


	/**
	 * @brief The destructor, no particular actions are performed.
	 */
	virtual ~RealTimeManager();

	/**
	 * @brief Return the instance of the RealTimeManager
	 */
	static inline RealTimeManager & GetInstance() noexcept {
		static RealTimeManager instance;	// In C++11 this is thread-safe
		return instance;
	}

	ExitCode_t SetupApp(app::AppPtr_t app);

private:

	RealTimeManager() noexcept;

};

}

#endif
