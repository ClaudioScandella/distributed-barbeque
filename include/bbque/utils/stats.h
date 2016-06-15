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

#ifndef BBQUE_UTILS_STATS_H_
#define BBQUE_UTILS_STATS_H_

#include <memory>
#include <cmath>
#include <list>

#include <bbque/utils/utility.h>

namespace bbque { namespace utils {


/**
 * @class EMA
 * @brief Exponential Moving Average accumulator
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

private:
	double alpha;
	double value;
	double  last;
	uint8_t load;
public:
	EMA(int samples = 6, double firstValue = 0) :
		alpha(2.0 / (samples +1)),
		value(firstValue),
		load(samples) {
	}
	double update(double newValue) {
		last  = newValue;
		value = ((alpha * last) + ((1 - alpha) * value));
		if (unlikely(load > 0))
			--load;
		return value;
	}
	void reset(int samples, double firstValue) {
		alpha = 2.0 / (samples +1);
		value = firstValue;
		load  = samples;
	}
	double get() const {
		if (unlikely(load > 0))
			return 0;
		return value;
	}
	double last_value() const {
		return last;
	}
};

class MovingStats {

private:
	// Max number of recent values to be stored and used to compute statistics
	int16_t max_window_size;
	// Current number of stored values
	int16_t curr_window_size = 0;
	// The stored values
	std::list<double> values_window;
	// The stored values, squared (for utils purposes)
	std::list<double> squared_values_window;

	// Returns the sum of the squared values, i.e. V0^2 + V1^2 + .. + VN^2
	double GetSumSquared() {
		if (curr_window_size == 0)
			return 0.0;

		double result = 0.0;
		for (auto value : squared_values_window)
			result += value;
		return result;
	}

	// Returns the average of the squared values, i.e. (V0^2 + .. + VN^2) / N
	double GetAverageSquared() {
		if (curr_window_size == 0)
			return 0.0;
		return GetSumSquared() / curr_window_size;
	}

public:

	MovingStats(int8_t _max_window_size = 1) :
		max_window_size(_max_window_size) {
	}

	// Returns the sum of the values, i.e. V0 + V1 + .. + VN
	double GetSum() {

		if (curr_window_size == 0)
			return 0.0;

		double result = 0.0;
		for (auto value : values_window)
			result += value;
		return result;
	}

	// Returns the average of the values, i.e. (V0 + .. + VN) / N
	double GetAverage() {
		if (curr_window_size == 0)
			return 0.0;
		return GetSum() / curr_window_size;
	}

	// Return the variance, which equals to E(X^2) - E^2(X)
	double GetVariance() {
		if (curr_window_size == 0)
			return 0.0;
		return GetAverageSquared() - (GetAverage() * GetAverage());
	}

	// Standard deviation: square root of the variance
	double GetStandartDeviation() {
		if (curr_window_size == 0)
			return 0.0;
		return std::sqrt(GetVariance());
	}

	// Standard error: square root of (variance / N)
	double GetStandardError() {
		if (curr_window_size == 0)
			return 0.0;
		return std::sqrt(GetVariance() / curr_window_size);
	}

	// CI 95%: 1.96 * standard error
	double GetConfidenceInterval95() {
		return 1.96 * GetStandardError();
	}

	// CI 99%: 2.58 * standard error
	double GetConfidenceInterval99() {
		return 2.58 * GetStandardError();
	}

	uint16_t GetWindowSize() {
		return curr_window_size;
	}

	double GetLastValue() {
		return values_window.front();
	}

	// Clear all values
	void Reset() {
		curr_window_size = 0;
		values_window.clear();
		squared_values_window.clear();
	}

	// Changes the max number of stored values, deleting values if needs be
	void ResetWindowSize(int16_t new_size) {
		if (new_size > curr_window_size) {
			values_window.resize(new_size);
			squared_values_window.resize(new_size);
			curr_window_size = new_size;
		}
		max_window_size = new_size;
	}

	// Store a new value
	void InsertValue(double value) {
		// If maximum value is reached, delete a value
		if (curr_window_size == max_window_size) {
			values_window.pop_back();
			squared_values_window.pop_back();
		} else
			curr_window_size ++;

		values_window.push_front(value);
		squared_values_window.push_front(value * value);
	}

};

/**
 * @brief A pointer to an EMA-defined accounter
 */
typedef std::shared_ptr<EMA> pEma_t;

/**
 * @brief A pointer to an MovingStats-defined accounter
 */
typedef std::shared_ptr<MovingStats> pMovingStats_t;


} // namespace utils

} // namespace bbque

#endif // BBQUE_UTILS_STATS_H_
