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


#ifndef BBQUE_POWER_MANAGER_MANGO_H_
#define BBQUE_POWER_MANAGER_MANGO_H_

#include <vector>
#include "bbque/pm/power_manager.h"
#include "bbque/res/resources.h"

#include <libhn/hn.h>

using namespace bbque::res;

namespace bbque {

/**
 * @class MangoPowerManager
 *
 * Provide power management API related to MANGO architectures,
 * by extending @ref PowerManager class.
 */
class MangoPowerManager: public PowerManager {

public:

	/**
	 * @see class PowerManager
	 */
	MangoPowerManager();

	virtual ~MangoPowerManager() {
		logger->Info("MangoPowerManager termination...");
	}

	/**
	 * @see class PowerManager
	 */
	PMResult GetLoad(ResourcePathPtr_t const & rp, uint32_t & perc) override;

	/**
	 * @see class PowerManager
	 */
	PMResult GetTemperature(ResourcePathPtr_t const & rp, uint32_t &celsius) override;


	/* ===========  Frequency management =========== */

	/**
	 * @see class PowerManager
	 */
	PMResult GetClockFrequency(ResourcePathPtr_t const & rp, uint32_t &khz) override {
		UNUSED(rp);
		UNUSED(khz);
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	/**
	 * @see class PowerManager
	 */
	PMResult SetClockFrequency(ResourcePathPtr_t const & rp, uint32_t khz) override {
		UNUSED(rp);
		UNUSED(khz);
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	/**
	 * @see class PowerManager
	 */
	PMResult SetClockFrequency(
			ResourcePathPtr_t const & rp,
			uint32_t khz_min,
			uint32_t khz_max) override {
		UNUSED(rp);
		UNUSED(khz_min);
		UNUSED(khz_max);
		return PMResult::ERR_API_NOT_SUPPORTED;
	}


	/**
	 * @see class PowerManager
	 */
	PMResult GetClockFrequencyInfo(
			br::ResourcePathPtr_t const & rp,
			uint32_t &khz_min,
			uint32_t &khz_max,
			uint32_t &khz_step) override {
		UNUSED(rp);
		UNUSED(khz_step);
		UNUSED(khz_min);
		UNUSED(khz_max);
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	/**
	 * @see class PowerManager
	 */
	PMResult GetAvailableFrequencies(
			ResourcePathPtr_t const & rp,
			std::vector<uint32_t> &freqs) override {
		UNUSED(rp);
		UNUSED(freqs);
		return PMResult::ERR_API_NOT_SUPPORTED;
	}


	/* ===========   Power consumption  =========== */

	/**
	 * @brief Power usage coming from sensors or models
	 */
	PMResult GetPowerUsage(
			br::ResourcePathPtr_t const & rp, uint32_t & mwatt) override {
		UNUSED(rp);
		mwatt = 0;
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	/* ===========   Performance/power states  =========== */

	/**
	 * @brief Change the current unit performance state
	 */
	PMResult GetPerformanceState(
			br::ResourcePathPtr_t const & rp, uint32_t &value) override {
		UNUSED(rp);
		value = 0;
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	/**
	 * @brief Get the number unit performance states available
	 */
	PMResult GetPerformanceStatesCount(
			br::ResourcePathPtr_t const & rp, uint32_t &count) override {
		UNUSED(rp);
		count = 0;
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	/**
	 * @brief Change the current unit performance state
	 */
	PMResult SetPerformanceState(
			br::ResourcePathPtr_t const & rp, uint32_t value) override {
		UNUSED(rp);
		UNUSED(value);
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

protected:

//	hn_tile_stats_t tile_stats;

	/// Dimensions of the MANGO platform
	uint32_t num_tiles[3]; // 0: total, 1:X, 2:Y

	/// Runtime counter statistics of a MANGO tiles
	std::vector<hn_stats_monitor_t> tiles_stats;

	/// Overall MANGO tiles information
	std::vector<hn_tile_info_t> tiles_info;

	/**
	 * @brief Current load estimation for PEAK processors
	 */
	PMResult GetLoadPEAK(uint32_t tile_id, uint32_t core_id, uint32_t & perc);

};

} // namespace bbque

#endif // BBQUE_POWER_MANAGER_MANGO_H_

