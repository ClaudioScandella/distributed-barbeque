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

#ifndef BBQUE_APPLICATION_SPECIFIC_RTM_H_
#define BBQUE_APPLICATION_SPECIFIC_RTM_H_

#include <vector>

#include <rtlib/bbque_exc.h>

#include <rtlib/monitors/goal_info.h>
#include <rtlib/monitors/op_manager.h>
#include <rtlib/monitors/generic_window.h>

using bbque::rtlib::BbqueEXC;

namespace bbque
{
namespace rtlib
{
namespace as
{

typedef std::vector<GenericWindowIF *> GoalsList;

/**
 * @brief Application-Specific Run-time Manager
 * @ingroup rtlib_sec04_rtrm
 *
 * @details
 * This class provides the ApplicationRTM, used to communicate between the
 * applications and Barbeque
 */
class RunTimeManager
{

public:

	/**
	 * @brief Constructor of the class
	 *
	 * @param Pointer to a BbqueEXC. It is be used to communicate with
	 * Barbeque
	 * @param opManager Pointer to the OPManager of an Execution Context.
	 * It is used to to have a knowledge of operating points
	 */
	RunTimeManager(BbqueEXC * bbqexc, OPManager & opManager)
		: bbqexc(bbqexc), opManager(opManager)
	{
	}

	/**
	 * @brief Constructor of the class
	 *
	 * @param bbqexc Pointer to a BbqueEXC.It is be used to communicate with
	 * Barbeque
	 * @param opManager Reference to the OPManager of an Execution Context.
	 * It is used to to have a knowledge of operating points
	 * @param goalsList Reference to a list of goals to register for the use
	 * with RunTimeManager
	 */
	RunTimeManager(BbqueEXC * bbqexc, OPManager & opManager,
				   GoalsList & goalsList)
		: bbqexc(bbqexc),
		  opManager(opManager),
		  goalsList(goalsList)
	{
	}

	/**
	 * @brief Sets the GoalsList to use inside RunTimeManager
	 * @param goalsList reference to a list of goals
	 */
	void setGoals(GoalsList & goalsList);

	/**
	 * @brief Checks the goals registered within the AS-RTM
	 *
	 * A GoalInfoList is filled with information about the achievement of
	 * each goal.
	 *
	 * @param goalsInfo output parameter for GoalInfoList. It will be used
	 * to store the information given by the goal-checking phase
	 * @return a value indicating whether all the goals have been achieved
	 * or not
	 */
	bool checkGoals(GoalInfoList & goalsInfo);


	/**
	 * @brief Adjusts OP selection contraints
	 *
	 * It adjust the previous constraints in order to achieve the current
	 * goals. It also creates new ones if necessary.
	 *
	 * The switchThreshold is only used if the goal is achieved. It allows
	 * the develeoper to specify from which point the AS-RTM can be
	 * confident about the application reaching its goals. If this threshold
	 * is passed, the AS-RTM could decide to change OP to get closer to the
	 * goal "from above". This feature allows the AS-RTM to trade-off
	 * between application metrics depending on the ordering of the OPs (the
	 * OP manager priorities), e.g. lower the performance in order to spare
	 * energy or increase accuracy, while still satisfying the goal.
	 * This behaviour must be enabled by the developer by explicitly setting
	 * the switchThreshold value. The default value (100.0) is exaggerately
	 * high on purpose in order to disable this feature by default. Since it
	 * is an absolute relative error, a switchThreshold equal to 1.0
	 * corresponds to adjusting the constraints whenever the monitored
	 * application metric is double its required goal.
	 *
	 * @param currentOp reference to the current Operating Point
	 * @param goalsInfo reference to the the current goal-achievement info
	 * @param opFilters reference to the list of constraints to adjust
	 * @param switchThreshold threshold corresponding to the max absolute
	 * relative error after which adjusting a constraint of an achieved goal
	 */
	void adjustConstraints(const OperatingPoint & currentOp,
						   const GoalInfoList & goalsInfo,
						   OPFilterList & opFilters,
						   float switchThreshold = 100.0);

	/**
	 * @brief Gets the maximum NAP and relative error of goals
	 *
	 * This function uses the information given by the goal-checking phase
	 * to get the max NAP and max relative error.
	 *
	 * @param goalsInfo reference to the the current goal-achievement info
	 * @param maxNap output parameter for the max NAP of all the goals
	 * @param maxRelativeError output parameter for the max (respect to the
	 * absolute value) relative error of all the goals
	 */
	void getNapAndRelativeError(const GoalInfoList & goalsInfo,
								uint8_t & maxNap,
								float & maxRelativeError);

	/**
	 * @brief Gets next valid OP
	 *
	 * Gets the next OP that satisfies the contraints given by
	 * opFilters. Returns true if an OP has been found, false otherwise
	 *
	 * The switchThreshold is only used if the goal is achieved. It allows
	 * the develeoper to specify from which point the AS-RTM can be
	 * confident about the application reaching its goals. If this threshold
	 * is passed, the AS-RTM could decide to change OP to get closer to the
	 * goal "from above". This feature allows the AS-RTM to trade-off
	 * between application metrics depending on the ordering of the OPs (the
	 * OP manager priorities), e.g. lower the performance in order to spare
	 * energy or increase accuracy, while still satisfying the goal.
	 * This behaviour must be enabled by the developer by explicitly setting
	 * the switchThreshold value. The default value (100.0) is exaggerately
	 * high on purpose in order to disable this feature by default. Since it
	 * is an absolute relative error, a switchThreshold equal to 1.0
	 * corresponds to adjusting the constraints whenever the monitored
	 * application metric is double its required goal.
	 *
	 * @param op output parameter in which to save the OP
	 * @param opFilters reference to the list of constraints to satisfy.
	 * it will be adjusted if needed (a goal is not achieved)
	 * @param switchThreshold threshold corresponding to the max absolute
	 * relative error after which adjusting a constraint of an achieved goal
	 */
	bool getNextOp(OperatingPoint & op, OPFilterList & opFilters,
				   float switchThreshold = 100.0);

	/**
	 * @brief Gets next valid OP
	 *
	 * Gets the next OP that satisfies the contraints given by
	 * opFilters. Returns true if an OP has been found, false otherwise
	 *
	 * The switchThreshold is only used if the goal is achieved. It allows
	 * the develeoper to specify from which point the AS-RTM can be
	 * confident about the application reaching its goals. If this threshold
	 * is passed, the AS-RTM could decide to change OP to get closer to the
	 * goal "from above". This feature allows the AS-RTM to trade-off
	 * between application metrics depending on the ordering of the OPs (the
	 * OP manager priorities), e.g. lower the performance in order to spare
	 * energy or increase accuracy, while still satisfying the goal.
	 * This behaviour must be enabled by the developer by explicitly setting
	 * the switchThreshold value. The default value (100.0) is exaggerately
	 * high on purpose in order to disable this feature by default. Since it
	 * is an absolute relative error, a switchThreshold equal to 1.0
	 * corresponds to adjusting the constraints whenever the monitored
	 * application metric is double its required goal.
	 *
	 * @param op output parameter in which to save the OP
	 * @param opFilters reference to the list of constraints to satisfy.
	 * it will be adjusted if needed (a goal is not achieved)
	 * @param goalsInfo reference to the the current goal-achievement info
	 * @param switchThreshold threshold corresponding to the max absolute
	 * relative error after which adjusting a constraint of an achieved goal
	 */
	bool getNextOp(OperatingPoint & op, OPFilterList & opFilters,
				   const GoalInfoList & goalsInfo,
				   float switchThreshold = 100.0);

private:
	/**
	 * Pointer to a BbqueEXC. It is be used to communicate with Barbeque
	 */
	BbqueEXC * bbqexc;

	/**
	 * OPManager of an Execution Context. It is used to have a knowledge of
	 * operating points
	 */
	OPManager opManager;

	/**
	 * List of goals to register for the use with ApplicationRTRM
	 */
	GoalsList goalsList;
};

} // namespace as

} // namespace rtlib

} // namespace bbque

#endif /* BBQUE_APPLICATION_SPECIFIC_RTM_H_ */
