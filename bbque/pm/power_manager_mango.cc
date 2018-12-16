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

#include <cstring>

#include "bbque/pm/power_manager_mango.h"

extern uint32_t hn_cluster;
extern uint32_t hn_handler;

using namespace bbque::res;

namespace bbque {

MangoPowerManager::MangoPowerManager() {
	logger->Info("MangoPowerManager initialization...");
//	memset(&tile_stats, 0, sizeof(hn_tile_stats_t));

	int err = hn_get_num_tiles(&num_tiles[0], &num_tiles[1], &num_tiles[2], hn_cluster, hn_handler);
	if (err == HN_SUCCEEDED) {
		tiles_info.resize(num_tiles[0]);
		tiles_stats.resize(num_tiles[0]);
	}
	else {
		logger->Fatal("Unable to get the number of MANGO tiles [error=%d].", err);
		return;
	}

	for (uint_fast32_t i = 0; i < num_tiles[0]; i++) {
		int err = hn_get_tile_info(i, &tiles_info[i], hn_cluster, hn_handler);
		if (HN_SUCCEEDED != err) {
			logger->Fatal("Unable to get the tile nr.%d [error=%d].", i, err);
			return;
		}
	}
}


PowerManager::PMResult
MangoPowerManager::GetLoad(ResourcePathPtr_t const & rp, uint32_t & perc) {
	uint32_t tile_id = rp->GetID(br::ResourceType::ACCELERATOR);
/*
	uint32_t core_id = rp->GetID(br::ResourceType::PROC_ELEMENT);
	if (hn_get_tile_stats(tile_id, &tile_stats) != 0) {
		logger->Warn("GetLoad: error in HN library call...");
		return PMResult::ERR_UNKNOWN;
	}
	perc = tile_stats.unit_utilization;
*/
	if (tiles_info[tile_id].unit_family == HN_TILE_FAMILY_PEAK) {
		logger->Debug("GetLoad: tile id=%d is a PEAK processor", tile_id);
		return GetLoadPEAK(tile_id, 0, perc); // core_id not supported
	}
	else
		perc = 0;
	return PMResult::OK;
}


PowerManager::PMResult
MangoPowerManager::GetLoadPEAK(uint32_t tile_id, uint32_t core_id, uint32_t & perc) {
	hn_stats_monitor_t * curr_stats = new hn_stats_monitor_t;
	uint32_t nr_cores = 0;
	uint32_t err = hn_stats_monitor_read(tile_id, &nr_cores, &curr_stats, hn_cluster, hn_handler);
	if (err == 0 && (curr_stats != nullptr)) {
		float cycles_ratio =
			float(curr_stats->core_cycles - tiles_stats[tile_id].core_cycles) /
				(curr_stats->timestamp - tiles_stats[tile_id].timestamp);
		tiles_stats[tile_id] = *curr_stats;
		perc = cycles_ratio * 100;
		logger->Debug("GetLoadPEAK: tile id=%d [cores=%d]: ts=%ld tics_sleep=%d core_cycles=%d load=%d",
			tile_id, nr_cores,
			curr_stats->timestamp, curr_stats->tics_sleep, curr_stats->core_cycles,
			perc);
	}
	else {
		perc = 0;
		logger->Error("GetLoadPEAK: tile id=%d, error=%d", tile_id, err);
	}

	delete curr_stats;
	return PMResult::OK;
}


PowerManager::PMResult
MangoPowerManager::GetTemperature(ResourcePathPtr_t const & rp, uint32_t &celsius) {
	uint32_t tile_id = rp->GetID(br::ResourceType::ACCELERATOR);
	float temp = 0;
	int err = hn_get_tile_temperature(tile_id, &temp, hn_cluster, hn_handler);
	if (err != 0) {
		logger->Error("GetTemperature: tile id=%d, error=%d", tile_id, err);
		return PMResult::ERR_UNKNOWN;
	}
	celsius = static_cast<uint32_t>(temp);
	return PMResult::OK;
}


} // namespace bbque

