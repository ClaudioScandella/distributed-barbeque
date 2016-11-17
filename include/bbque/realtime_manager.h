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
#include "bbque/utils/logging/logger.h"

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

public:
	/**
	 * @brief The errors enumeration
	 */
	enum ExitCode_t {
		RTM_OK,
		RTM_GENERIC_ERROR
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

	/**
	  *	@brief Performs the system calls to change the scheduling parameters
	  * 	   assigned to the application.
      * @note If the system has no real-time capabilities, no action is
      *       performed.
	  * @throw runtime_error is the application is not flagged as RT. 
	  */
	ExitCode_t SetupApp(app::AppPtr_t app);
	
	/**
      * @brief Returns true if the system has soft realtime capabilities
      */
	inline bool IsSoftRealTime() const noexcept { return this->is_soft; }

	/**
      * @brief Returns true if the system has hard realtime capabilities
      */
	inline bool IsHardRealTime() const noexcept { return this->is_hard; }


private:

	/** The value from /proc/sys/kernel/sched_rr_timeslice_ms */
	int sched_rr_interval_ms;

	bool is_soft = false;
	bool is_hard = false;

	/** The logger used by the resource accounter */
	std::unique_ptr<bu::Logger> logger;

	/** The constructor */
	RealTimeManager() noexcept;

	/** Read from command line the value of real time */
	void SetRTLevel() noexcept;

	/** Se the /proc/sys entries about Real Time */
	void SetKernelReservation() noexcept;


};

}

#endif
