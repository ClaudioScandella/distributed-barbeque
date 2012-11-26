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

#include "bbque/config.h"
#include "bbque/rtlib/bbque_dmm.h"

#include <cstdio>
#include <sys/stat.h>

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "dmm"

/**
 * @brief The DMM Library Knobs tuning parameters
 *
 * Each AWM could be assigned a different set of DMM tuning parameters.
 * The value of such parameters is idenfitied at design time and represented
 * into a proper rtm_knobs_t data structure.
 * The collection of tuning values for each AWM is represente by this symbol
 * which an application is expected to export.
 *
 */
extern dmm_knobs_t dmm_knobs[] __attribute__((weak));


/**
 * @brief The number of suppported DMM knobs configurations
 *
 * This is the number of entries in the @ref dmm_knobs table.
 */
extern uint32_t dmm_knobs_count __attribute__((weak));

/**
 * @brief Initialized the DMM Library for Run-Time Resource Management
 *
 * @param conf A reference to the DMM knob parameters table
 * @param count The number of different knobs configurations
 *
 * @return 0 on success
 */
extern uint32_t dmm_rtm_init(const dmm_knobs_t *conf, uint32_t count) __attribute__((weak));

/**
 * @brief Set the DMM Library knobs parameters
 *
 * The DMM Library could be run-time tuned by configuring the value of a set
 * of knob parameters. This method allow to specify the set of value to be
 * used to reconfigure the knobs. The @ref conf parameter should reference an
 * entry of the @ref dmm_knobs array.
 *
 * @param conf a pointer to a dmm_knobs entry
 *
 * @return 0 on success
 */
extern uint32_t dmm_set_knobs(dmm_knobs_t *conf) __attribute__((weak));

namespace bbque { namespace rtlib {

bool LibDMM::initialized = false;

LibDMM::LibDMM() {
	DB(fprintf(stderr, FD("LibDMM ctor\n")));
}

LibDMM::~LibDMM() {
	DB(fprintf(stderr, FD("LibDMM dtor\n")));
}


RTLIB_ExitCode_t
LibDMM::Init(const dmm_knobs_t *conf, uint32_t count) {

	if (dmm_knobs == NULL
			|| dmm_rtm_init == NULL
			|| dmm_set_knobs == NULL)
		return RTLIB_ERROR;

	if (dmm_rtm_init(conf, count) != 0)
		return RTLIB_ERROR;

	initialized = true;
	return RTLIB_OK;

}


RTLIB_ExitCode_t
LibDMM::SetKnobs(uint32_t index) {
	DB(fprintf(stderr, FD("Setting DMM Knobs to [%02d]\n"), index));

	if (!initialized)
		return RTLIB_ERROR;

	if (index >= dmm_knobs_count) {
		fprintf(stderr, FW("ERROR: Knobs index [%02d] out-of-range\n"),
				index);
		return RTLIB_ERROR;
	}

	if (dmm_set_knobs(&dmm_knobs[index]) != 0) {
		fprintf(stderr, FW("FAILED: Knobs index [%02d] setting\n"),
				index);
		return RTLIB_ERROR;
	}

	return RTLIB_OK;
}

} // namespace rtlib

} // namespace bbque
