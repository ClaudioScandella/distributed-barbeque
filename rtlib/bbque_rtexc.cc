/*
 * Copyright (C) 2012-2016  Politecnico di Milano
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

#include "bbque/rtlib/bbque_rtexc.h"

#include "bbque/utils/utility.h"

#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <cstring>


#define DUMMY_KNOW_VALUE 0xCD

static unsigned char* stack_prefault_buffer;
static size_t         stack_prefault_bytes;
static void perform_stack_prefault(size_t bytes) __attribute__((optimize("-O0")));
static int check_stack_prefault() __attribute__((optimize("-O0")));

static void perform_stack_prefault(size_t bytes) {

	// Allocate the bytes on the stack (it will be destructed at the end
	// of this function)
	unsigned char *buffer = (unsigned char *) alloca(bytes);

	stack_prefault_buffer = buffer;
	stack_prefault_bytes = bytes;
	// In order to issue the stack soft-fault, we have to access to all pages
	// of the buffer created.
	for (size_t i=0; i < bytes;) {
		buffer[i] = DUMMY_KNOW_VALUE;

#ifndef BBQUE_DEBUG
		// We can move to the next page, thus we don't loose time to
		// write every single cell
		i += sysconf(_SC_PAGESIZE)
#else
		// But if we are in debug, we set all items in order to check
		// later if we have reached the top or not.
		i += sizeof(unsigned char);
#endif
	}

}

static int check_stack_prefault() {

	// Allocate the bytes on the stack (it will be destructed at the end
	// of this function)
	unsigned char *buffer = (unsigned char *) alloca(stack_prefault_bytes);

	// NOTE: Stack works with addresses in the opposite direction, so we have
	// to request other stack if this buffer assignment is not over the
	// previous
	if (buffer > stack_prefault_buffer) {
		(void) alloca(buffer - stack_prefault_buffer);
	}

	// Now we check when our value (DUMMY_KNOW_VALUE) does not exist in the
	// buffer. Obiously a random DUMMY_KNOW_VALUE may be present in the
	// stack, but the returned value should not be "precise"
	size_t i;
	for (i=0; i < stack_prefault_bytes; i++) {
		if (stack_prefault_buffer[i] != DUMMY_KNOW_VALUE) {
			break;
		}
	}

	return i;
}

namespace bbque
{
namespace rtlib
{


RTLIB_ExitCode_t BbqueRTEXC::StackPreFault(size_t bytes) const noexcept {

	int err;
	struct rlimit rlim;
	err = getrlimit(RLIMIT_STACK, &rlim);

	logger->Info("Stack pre-fault requested of %d bytes", bytes);

	if (err) {
		logger->Error("getrlimit FAILED [%d: %s].", errno, 
							std::strerror(errno));
		return RTLIB_ERROR;
	}

	if (bytes > rlim.rlim_cur) {
		if (bytes > rlim.rlim_max) {
			// We have reached the hard limit, we cannot allocate
			// this size of the stack.
			logger->Error("Stack size too big. "
					"Please check your ulimits.");
			return RTLIB_STACK_TOO_BIG;
		}
		// Otherwise get all possible
		rlim.rlim_cur = rlim.rlim_max;
		err = setrlimit(RLIMIT_STACK, &rlim);
		if (err) {
			logger->Error("setrlimit FAILED [%d: %s].", errno, 
							std::strerror(errno));
			return RTLIB_STACK_TOO_BIG;
		}

		// Ok, but we cannot know if this is sufficient. It depends on
		// how much the RTLIB and the application already consumed
		// of the stack.
		logger->Warn("Stack size soft-limit incrementedfrom %d to %d. "
				"Stack overflow may occur.", rlim.rlim_cur, 
				rlim.rlim_max);
	}

	perform_stack_prefault(bytes);

	logger->Notice("Pre-faulted stack of %d bytes", bytes);

	return RTLIB_OK;
}

void BbqueRTEXC::StackPreFaultPostCheck() const noexcept {
#ifdef BBQUE_DEBUG
	int remain = check_stack_prefault();
	if (remain==0) {
		logger->Error("Stack prefault was not sufficient.");
	} else {
		logger->Notice("Stack prefault OK, not used %d bytes.", remain);
	}
#endif
}

RTLIB_ExitCode_t BbqueRTEXC::MemoryLock() const noexcept {
	int err = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (err) {
		logger->Error("Unable to enforce memory locking. Error: "
				"%d (%s)", errno, strerror(errno));
		return RTLIB_ERROR;
	}
	return RTLIB_OK;
}


} // namespace rtlib

} // namespace bbque
