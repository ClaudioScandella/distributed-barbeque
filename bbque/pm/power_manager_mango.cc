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

using namespace bbque::res;

namespace bbque {

MangoPowerManager::MangoPowerManager() {
	logger->Info("MangoPowerManager initialization...");
	uint32_t num_tiles[3]; // 0: total, 1:X, 2:Y

	// Get the number of clusters
	int err = hn_get_num_clusters(&this->num_clusters);
	if ( HN_SUCCEEDED != err ) {
		logger->Fatal("MangoPowerMonitor: unable to get the number of clusters "
			"[error=%d]", err);
	}

	// Initialize per-cluster tile info...
	for (uint32_t cluster_id=0; cluster_id < this->num_clusters; ++cluster_id) {
		int err = hn_get_num_tiles(&num_tiles[0], &num_tiles[1], &num_tiles[2], cluster_id);
		if (err == HN_SUCCEEDED) {
			tiles_info[cluster_id].resize(num_tiles[0]);
			tiles_stats[cluster_id].resize(num_tiles[0]);
		}
		else {
			logger->Fatal("Unable to get the number of MANGO tiles [error=%d].", err);
			return;
		}

		for (uint_fast32_t tile_id = 0; tile_id < num_tiles[0]; tile_id++) {
			int err = hn_get_tile_info(tile_id, &tiles_info[cluster_id][tile_id], cluster_id);
			if (HN_SUCCEEDED != err) {
				logger->Fatal("MangoPowerMonitor: unable to get info for "
					"cluster=<%d> tile=<%d> [error=%d].",
					cluster_id, tile_id, err);
				return;
			}
		}
	}
}


PowerManager::PMResult
MangoPowerManager::GetLoad(ResourcePathPtr_t const & rp, uint32_t & perc) {

	// HN cluster
	int32_t cluster_id = rp->GetID(br::ResourceType::GROUP);
	if (cluster_id < 0) {
		logger->Warn("GetLoad: no cluster ID, using 0 by default");
		cluster_id = 0;
	}

	// Tile
	uint32_t tile_id = rp->GetID(br::ResourceType::ACCELERATOR);
	hn_stats_monitor_t * nuplus_stats = new hn_stats_monitor_t;

	// Architecture type
	switch (tiles_info[cluster_id][tile_id].unit_family) {
	case HN_TILE_FAMILY_PEAK:
		logger->Debug("GetLoad: cluster=<%d> tile=<%d> is a PEAK processor",
			cluster_id, tile_id);
		return GetLoadPEAK(cluster_id, tile_id, 0, perc); // core_id not supported
	case HN_TILE_FAMILY_NUPLUS:
		logger->Debug("GetLoad: cluster=<%d> tile=<%d> is a NUPLUS processor",
			cluster_id, tile_id);
		return GetLoadNUP(cluster_id, tile_id, perc);
/*
	case HN_TILE_FAMILY_DCT:
		logger->Debug("GetLoad: cluster=<%d> tile=<%d> is a DCT accelerator",
			cluster_id, tile_id);
		hn_dct_get_utilization(tile_id, &perc, cluster_id);
		break;
	case HN_TILE_FAMILY_TETRAPOD:
		logger->Debug("GetLoad: cluster=<%d> tile=<%d> is a TETRAPOD accelerator",
			cluster_id, tile_id);
		hn_tetrapod_get_utilization(tile_id, &perc, cluster_id);
		break;
*/
	default:
		logger->Debug("GetLoad: cluster=<%d> tile=<%d> family=%s",
			cluster_id, tile_id,
			hn_to_str_unit_family(tiles_info[cluster_id][tile_id].unit_family));
		perc = 0;
	}
	return PMResult::OK;
}


PowerManager::PMResult
MangoPowerManager::GetLoadPEAK(
		uint32_t cluster_id,
		uint32_t tile_id,
		uint32_t core_id,
		uint32_t & perc) {
	UNUSED(core_id);

	hn_stats_monitor_t * curr_stats = new hn_stats_monitor_t;
	uint32_t nr_cores = 0;
	uint32_t err = hn_stats_monitor_read(tile_id, &nr_cores, &curr_stats, cluster_id);
	if (err == 0 && (curr_stats != nullptr)) {
		float cycles_ratio =
			float(curr_stats->core_cycles - tiles_stats[cluster_id][tile_id].core_cycles) /
				(curr_stats->timestamp - tiles_stats[cluster_id][tile_id].timestamp);
		tiles_stats[cluster_id][tile_id] = *curr_stats;
		perc = cycles_ratio * 100;
		logger->Debug("GetLoadPEAK: cluster=<%d> tile=<%d> [cores=%d]: "
			"ts=%ld tics_sleep=%d core_cycles=%d load=%d",
			cluster_id, tile_id, nr_cores,
			curr_stats->timestamp, curr_stats->tics_sleep, curr_stats->core_cycles,
			perc);
	}
	else {
		perc = 0;
		logger->Error("GetLoadPEAK: cluster=<%d> tile=<%d> [error=%d]",
			cluster_id, tile_id, err);
	}

	delete curr_stats;
	return PMResult::OK;
}

PowerManager::PMResult
MangoPowerManager::GetLoadNUP(
		uint32_t cluster_id,
		uint32_t tile_id,
		uint32_t & perc) {

	hn_stats_monitor_t * curr_stats = new hn_stats_monitor_t;
	uint32_t err = hn_nuplus_stats_read(tile_id, curr_stats, cluster_id);
	if (err == 0 && (curr_stats != nullptr)) {
		perc = (uint32_t) curr_stats->core_cpi;
		logger->Debug("GetLoadNUP: cluster=<%d> tile=<%d>: "
			"ts=%ld tics_sleep=%d core_cycles=%d load=%d",
			cluster_id, tile_id,
			curr_stats->timestamp, curr_stats->tics_sleep, curr_stats->core_cycles,
			perc);
		tiles_stats[cluster_id][tile_id] = *curr_stats;
	}
	else {
		perc = 0;
		logger->Error("GetLoadNUP: cluster=<%d> tile=<%d> [error=%d]",
			cluster_id, tile_id, err);
	}

	delete curr_stats;
	return PMResult::OK;
}


PowerManager::PMResult
MangoPowerManager::GetTemperature(ResourcePathPtr_t const & rp, uint32_t &celsius) {
	// HN cluster
	int32_t cluster_id = rp->GetID(br::ResourceType::GROUP);
	if (cluster_id < 0) {
		logger->Warn("GetLoad: no cluster ID, using 0 by default");
		cluster_id = 0;
	}

	// Tile
	uint32_t tile_id = rp->GetID(br::ResourceType::ACCELERATOR);
	float temp = 0;
	int err = hn_get_tile_temperature(tile_id, &temp, cluster_id);
	if (err != 0) {
		logger->Error("GetTemperature: cluster=<%d> tile=<%d>  [error=%d]",
			cluster_id, tile_id, err);
		return PMResult::ERR_UNKNOWN;
	}

	celsius = static_cast<uint32_t>(temp);
	return PMResult::OK;
}

PowerManager::PMResult
MangoPowerManager::GetPowerUsage(br::ResourcePathPtr_t const & rp, uint32_t & mwatt) {

	// HN cluster
	int32_t cluster_id = rp->GetID(br::ResourceType::GROUP);
	if (cluster_id < 0) {
		logger->Warn("GetPowerUsage: no cluster ID, using 0 by default");
		cluster_id = 0;
	}

	// Tile
	uint32_t tile_id = rp->GetID(br::ResourceType::ACCELERATOR);

	// Architecture type
	if (tiles_info[cluster_id][tile_id].unit_family != HN_TILE_FAMILY_NUPLUS) {
		mwatt = 0;
		return PMResult::ERR_API_NOT_SUPPORTED;
	}

	mwatt = static_cast<uint32_t>(tiles_stats[cluster_id][tile_id].power_est);
	return PMResult::OK;
}


} // namespace bbque

