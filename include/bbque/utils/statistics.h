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

#ifndef BBQUE_STATISTICS_H_
#define BBQUE_STATISTICS_H_

#include "bbque/utils/utility.h"

namespace bbque { namespace utils {

/**
 * @brief The class implementing a Zero-Latency Exponential Moving Average
 *
 * This class provides a simple accumulator for on-line computation of an
 * Exponential Moving Average, with an exponential factor @see alpha.
 *
 * The alpha factor is expressed in terms of N samples, where alpha =
 * 2/(N+1). For example, N = 19 is equivalent to alpha = 0.1.
 * The half-life of the weights (the interval over which the
 * weights decrease by a factor of two) is approximately N/2.8854
 * (within 1% if N > 5).
 */
class EMA {

public:
	EMA(uint8_t _samples = 6, double _firstValue = 0) :
		samples(_samples),
		load(_samples),
		alpha(2.0 / (_samples +1)),
		value(_firstValue) {
	}
	void reset(int samples, double firstValue) {
		alpha = 2.0 / (samples +1);
		value = firstValue;
		load  = samples;
	}
	double update(double newValue) {
		value = ((alpha * newValue) + ((1 - alpha) * value));
		if (unlikely(load > 0))
			--load;
		return value;
	}
	double get() const {
		if (unlikely(load > 0))
			return 0;
		return value;
	}
	uint8_t getSamples() const {
		return samples;
	}

private:
	uint8_t samples;
	uint8_t load;
	double alpha;
	double value;

};

/**
 * @brief A pointer to an EMA-defined accounter
 */
typedef std::shared_ptr<EMA> pEma_t;

} // namespace utils

} // namespace bbque

#endif /* end of include guard: BBQUE_STATISTICS_H_ */
