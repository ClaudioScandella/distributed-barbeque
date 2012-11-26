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

#ifndef BBQUE_DMM_H_
#define BBQUE_DMM_H_

#include "bbque/rtlib.h"
#include "bbque/config.h"
#include "bbque/utils/utility.h"

#include <dmmlib/knobs.h>

namespace bbque { namespace rtlib {

/**
 * @brief The class implementing the RTLib plain API
 * @ingroup rtlib_sec03_plain
 *
 * This RPC mechanism is channel agnostic and defines a set of procedure that
 * applications could call to send requests to the Barbeque RTRM.  The actual
 * implementation of the communication channel is provided by class derived by
 * this one. This class provides also a factory method which allows to obtain
 * an instance to the concrete communication channel that can be selected at
 * compile time.
 */
class LibDMM {

public:

/******************************************************************************
 * DMM Library Configuration Interface
 ******************************************************************************/

	/**
	 * @brief Initilize the DMM library for Run-Time management
	 *
	 * @param conf A reference to the DMM knob parameters table
	 * @param count The number of different knobs configurations
	 *
	 * @return RTLIB_OK on success, RTLIB_ERROR otherwise
	 */
	static RTLIB_ExitCode_t Init(const dmm_knobs_t *conf, const uint32_t count);

	/**
	 * @brief Set the DMM Library with the specified knob parameters
	 *
	 * @param index The ID of the knob parameters into the knob
	 * parameters table
	 *
	 * @return RTLIB_OK on success, RTLIB_ERROR otherwise
	 */
	static RTLIB_ExitCode_t SetKnobs(uint32_t index);

private:

	/**
	 * @brief The LibDMM is a singleton class
	 */
	LibDMM();
	~LibDMM(void);

	/**
	 * @brief True if the DMM Library has been correctly initialized for
	 * Run-Time Management
	 */
	static bool initialized;

};

} // namespace rtlib

} // namespace bbque

#endif /* end of include guard: BBQUE_DMM_H_ */
