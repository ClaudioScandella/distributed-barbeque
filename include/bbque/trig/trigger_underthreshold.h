/*
 * Copyright (C) 2018  Politecnico di Milano
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

#ifndef BBQUE_TRIGGER_UNDERTHRESHOLD_H_
#define BBQUE_TRIGGER_UNDERTHRESHOLD_H_

#include "bbque/app/application.h"
#include "bbque/res/resources.h"

namespace bbque {

namespace trig {

/**
 * @class ThresholdTrigger
 * @brief Type of trigger based on threshold values
 */
class UnderThresholdTrigger: public Trigger {

public:

	UnderThresholdTrigger() {}

	UnderThresholdTrigger(uint32_t threshold_high,
		uint32_t threshold_low,
		float margin,
		bool armed = true) :
		Trigger(threshold_high, threshold_low, margin,armed){}

	virtual ~UnderThresholdTrigger() {}

	/**
	 * @brief The condition is verified if the current value is above the reference value
	 * for a given margin
	 * @return true in case of condition verified, false otherwise
	 */
	inline bool Check(float curr_value) {
		if(check_func)
			return check_func(curr_value);
		return DefaultCheck(curr_value);
	}

	/**
	 * @brief Default check function provided if no custom check function are set
	 *
	 * @brief true in case of condition verified, false otherwise
	 */
	inline bool DefaultCheck(float curr_value) {
		float thres_high_with_margin = static_cast<float>(threshold_high) * (1.0 - margin);
		if (curr_value < thres_high_with_margin)
			return true;
		return false;
	}

};

} // namespace trig

} // namespace bbque


 #endif // BBQUE_TRIGGER_UNDERTHRESHOLD_H_
