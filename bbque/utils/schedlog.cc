/*
 * Copyright (C) 2019  Politecnico di Milano
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

#include "bbque/app/working_mode.h"
#include "bbque/res/bitset.h"
#include "bbque/utils/schedlog.h"

namespace bbque {

namespace utils {


void SchedLog::BuildProgressBar(
			uint64_t used,
			uint64_t total,
			char * prog_bar,
			uint8_t len,
			char symbol) {
	uint8_t nr_bars = ((float) used / total) * len;
	memset(prog_bar, ' ', len);
	for (int i = 0; i < nr_bars; ++i)
		prog_bar[i] = symbol;
}

void SchedLog::BuildStateStr(
			app::SchedPtr_t sched_ptr,
			char * state_str,
			size_t len) {
	app::Schedulable::State_t state;
	app::Schedulable::SyncState_t sync_state;
	char st_str[] = "   ";
	char sy_str[] = "   ";

	state = sched_ptr->State();
	sync_state = sched_ptr->SyncState();

	// Sched stable state
	switch (state) {
	case app::Schedulable::NEW:
		strncpy(st_str, "NEW", 3); break;
	case app::Schedulable::READY:
		strncpy(st_str, "RDY", 3); break;
	case app::Schedulable::RUNNING:
		strncpy(st_str, "RUN", 3); break;
	case app::Schedulable::FINISHED:
		strncpy(st_str, "FIN", 3); break;
	default:
		strncpy(st_str, "SYN", 3); break;
	}

	// Sync state
	switch (sync_state) {
	case app::Schedulable::STARTING:
		strncpy(sy_str, "STA", 3); break;
	case app::Schedulable::RECONF:
		strncpy(sy_str, "RCF", 3); break;
	case app::Schedulable::MIGREC:
		strncpy(sy_str, "MCF", 3); break;
	case app::Schedulable::MIGRATE:
		strncpy(sy_str, "MGR", 3); break;
	case app::Schedulable::BLOCKED:
		strncpy(sy_str, "BLK", 3); break;
	case app::Schedulable::DISABLED:
		strncpy(st_str, "DIS", 3); break;
	default:
		// SYNC_NONE
		strncpy(sy_str, "---", 3); break;
	}

	snprintf(state_str, len, " %s %s ", st_str, sy_str);
}


#define GET_OBJ_RESOURCE(awm, r_type, r_bset) \
	r_type = res::ResourceType::GPU; \
	r_bset = awm->BindingSet(r_type); \
	if (r_bset.Count() == 0) { \
		r_type = res::ResourceType::CPU; \
		r_bset = awm->BindingSet(r_type); \
	}

void SchedLog::BuildSchedStateLine(
			app::SchedPtr_t sched_ptr,
			char * state_line,
			size_t len) {

	char state_str[10];
	char binding_str[10];
	char curr_awm_set[15]  = "";
	char next_awm_set[15]  = "";
	char curr_awm_name[12] = "";
	res::ResourceType r_type;
	res::ResourceBitset r_bset;

	app::AwmPtr_t const & awm(sched_ptr->CurrentAWM());
	app::AwmPtr_t const & next_awm(sched_ptr->NextAWM());

	// Reset AWM name (default for EXCs not running)
	strncpy(curr_awm_name, "-", sizeof(curr_awm_name));
	strncpy(curr_awm_set,  "-", sizeof(curr_awm_set));
	strncpy(next_awm_set,  "-", sizeof(next_awm_set));

	// Current AWM
	if (awm) {
		GET_OBJ_RESOURCE(awm, r_type, r_bset);
		// MIGRATE case => must see previous set of the same AWM
		if ((awm == next_awm) &&
			((awm->BindingChanged(res::ResourceType::GPU)) ||
				(awm->BindingChanged(res::ResourceType::CPU)))) {
			r_bset = awm->BindingSetPrev(r_type);
		}
		else {
			r_bset = awm->BindingSet(r_type);
		}

		// Binding string
		snprintf(binding_str, sizeof(binding_str), "%s{%s}",
				res::GetResourceTypeString(r_type),
				r_bset.ToStringCG().c_str());
		snprintf(curr_awm_set,  sizeof(curr_awm_set), "%02d:%s",
				awm->Id(), binding_str);
		snprintf(curr_awm_name, sizeof(curr_awm_name), "%s",
				awm->Name().c_str());
	}

	// Next AWM
	if (next_awm) {
		GET_OBJ_RESOURCE(next_awm, r_type, r_bset);
		snprintf(binding_str, sizeof(binding_str), "%s{%s}",
				res::GetResourceTypeString(r_type),
				r_bset.ToStringCG().c_str());
		snprintf(next_awm_set, 12, "%02d:%s", next_awm->Id(), binding_str);
	}

	// "SYN STA"
	BuildStateStr(sched_ptr, state_str, 10);

	// Build the entire line
	snprintf(state_line, len, "| %16s | %10s | %11s | %11s | %-11s |",
			sched_ptr->StrId(),
			state_str, curr_awm_set, next_awm_set, curr_awm_name);
}

} // namespace utils

} // namespace bbque

