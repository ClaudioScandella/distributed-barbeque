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
#include "tg/task_graph.h"
#include "tg/partition.h"

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
	 * @param status_view The token referencing the resources state view
	 * @param bid An optional identifier for the resource binding
	 *
	 * @return The method returns an exit code representing the decision taken:
	 * APP_SUCCESS if the specified working mode can be scheduled for
	 * this application, APP_WM_REJECTED if the working mode cannot not be
	 * scheduled. If the application is currently disabled this call returns
	 * always APP_DISABLED.
	 */
	virtual ExitCode_t ScheduleRequest(AwmPtr_t const & awm,
			bbque::res::RViewToken_t status_view,
			size_t b_refn = 0) = 0;

	/**
	 * @brief Re-schedule this application according to previous scheduling
	 * policy run
	 *
	 * @param status_view The token referencing the resources state view
	 *
	 * @return The method returns APP_SUCCESS if the application can be
	 * rescheduled,  APP_STATUS_NOT_EXP if the application is not in "running"
	 * stats, APP_WM_REJECTED if required resources are no longier available.
	 */
	virtual ExitCode_t ScheduleRequestAsPrev(bbque::res::RViewToken_t status_view) = 0;

	/**
	 * @brief Set the scheduling metrics value
	 *
	 * The usage of this method should be in charge of the scheduling policy
	 *
	 * @param sched_metrics The scheduling metrics computed by the policy
	 */
	virtual void SetValue(float sched_metrics) = 0;

	/**
	 * @brief Set the working mode assigned by the scheduling policy
	 *
	 * The usage of this method should be in charge of the
	 * ApplicationManager, when a scheduling request has been accepted
	 *
	 * @param awm the working mode
	 */
	virtual void SetNextAWM(AwmPtr_t awm) = 0;
	virtual void NoSchedule() = 0;

#ifdef CONFIG_BBQUE_TG_PROG_MODEL

	/**
	 * @brief Update the task-graph description shared with the RTLib
	 */
	virtual std::shared_ptr<TaskGraph> GetTaskGraph() = 0;

	/**
	 * @brief Set a new task-graph description
	 * @note Typically used by the scheduling policy for resource mapping purpose
	 * @param tg Shared pointer to task-graph descriptor
	 * @param write_through If set to true (default) update also the file or memory
	 * region storing the copy shared with the RTLib
	 */
	virtual void SetTaskGraph(std::shared_ptr<TaskGraph> tg, bool write_through=true) = 0;

	/**
	 * @brief Return the current partition
	 */
	virtual std::shared_ptr<Partition> GetPartition() = 0;

	/**
	 * @brief Set the allocated partition
	 * @note Typically used by the scheduling policy for resource mapping purpose
	 * @param partition the allocated partition object
	 */
	virtual void SetPartition(std::shared_ptr<Partition> partition) = 0;

#endif // CONFIG_BBQUE_TG_PROG_MODEL

};

} // namespace app

} // namespace bbque

#endif // BBQUE_APPLICATION_CONF_IF_H_
