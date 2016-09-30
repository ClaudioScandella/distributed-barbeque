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

#ifndef BBQUE_APPLICATION_CONF_IF_H_
#define BBQUE_APPLICATION_CONF_IF_H_

#include "bbque/app/application_status.h"

namespace bbque {

namespace res {
typedef size_t RViewToken_t;
}

namespace app {

class ApplicationConfIF;
class Application;
class WorkingMode;
typedef std::shared_ptr<ApplicationConfIF> AppCPtr_t;
typedef std::shared_ptr<Application> AppPtr_t;

/**
 * @class ApplicationConfIF
 * @brief Interface to configure application status
 *
 * This defines the interfaces for updating runtime information of the
 * application as priority, scheduled status and next working mode
 */
class ApplicationConfIF: public ApplicationStatusIF {

public:

	/**
	 * @brief Request to re-schedule this application into a new configuration
	 *
	 * The Optimizer call this method when an AWM is selected for this
	 * application to verify if it could be scheduled, i.e. bound resources
	 * are available, and eventually to update the application status.
	 *
	 * First the application verify resources availability. If the quality and
	 * amount of required resources could be satisfied, the application is
	 * going to be re-scheduled otherwise, it is un-scheduled.
	 *
	 * @param awm Next working mode scheduled for the application
	 * @param tok The token referencing the resources state view
	 * @param bid An optional identifier for the resource binding
	 *
	 * @return The method return an exit code representing the decision taken:
	 * APP_SUCCESS if the specified working mode can be scheduled for
	 * this application, APP_WM_REJECTED if the working mode cannot not be
	 * scheduled. If the application is currently disabled this call returns
	 * always APP_DISABLED.
	 */
	virtual ExitCode_t ScheduleRequest(AwmPtr_t const & awm,
			bbque::res::RViewToken_t tok,
			size_t b_refn = 0) = 0;

	/**
	 * @brief Set the scheduling metrics value
	 *
	 * The usage of this method should be in charge of the scheduling policy
	 *
	 * @param sched_metrics The scheduling metrics computed by the policy
	 */
	virtual void SetValue(float sched_metrics) = 0;
};

} // namespace app

} // namespace bbque

#endif // BBQUE_APPLICATION_CONF_IF_H_
