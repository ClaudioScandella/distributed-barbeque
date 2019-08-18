/*
 * Copyright (C) 2017  Politecnico di Milano
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

#ifndef BBQUE_TRIGGER_H_
#define BBQUE_TRIGGER_H_

#include <functional>

namespace bbque {

namespace trig {

/**
 * @class Trigger
 * @brief A trigger is a component including boolean functions aimed at verify if, given
 * some input parameters, a certain condition is verified or not. A typical use case is
 * the monitoring of hardware resources status and the detection of condition for which
 * an execution of the optimization policy must be triggered.
 */
class Trigger {

public:

	Trigger() {}

	Trigger(uint32_t threshold_high,
		uint32_t threshold_low,
		float margin,
		bool armed = true) :
		threshold_high(threshold_high),
		threshold_low(threshold_low),
		margin(margin),
		armed(armed){}

	virtual ~Trigger() {}

	/**
	 * @brief Check if a condition is verified given a current value
	 * @return true in case of condition verified, false otherwise
	 */
	//virtual bool Check(float ref_value, float curr_value, float margin = 0.0) const = 0;
	virtual bool Check(float curr_value) = 0;

	virtual bool DefaultCheck(float curr_value) = 0;

	inline const std::function<void()> & GetActionFunction() const {
		return this->action_func;
	}

	inline const std::function<bool(float)> & GetCheckFunction() const {
		return this->check_func;
	}

	inline void SetActionFunction(std::function<void()> func) {
		this->action_func = func;
	}

	inline void SetCheckFunction(std::function<bool(float)> func) {
		this->check_func = func;
	}

	inline bool IsArmed() const {
		return this->armed;
	}

	/// Threshold high value
	uint32_t threshold_high = 0;
	/// Threshold low armed value
	uint32_t threshold_low  = 0;
	/// Margin [0..1)
	float margin            = 0.1;
	/// Flag to verify if the trigger is armed
	bool armed              = true;

protected:

	std::function<bool(float)> check_func;

	std::function<void()> action_func;
};

} // namespace trig

} // namespace bbque


 #endif // BBQUE_TRIGGER_H_
