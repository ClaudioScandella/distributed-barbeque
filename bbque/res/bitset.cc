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

#include <cstring>

#include "bbque/res/bitset.h"
#include "bbque/utils/utility.h"

#define MODULE_NAMESPACE "bq.bs"

namespace bbque { namespace res {

ResourceBitset::ResourceBitset():
	first_set(R_ID_NONE),
	last_set(R_ID_NONE),
	count(0),
	none(true) {
}

ResourceBitset::~ResourceBitset() {}

ResourceBitset::ExitCode_t ResourceBitset::Set(ResID_t pos) {
	char buff[4] = "";

	// Boundary check
	if (pos > MAX_R_ID_NUM)
		return OUT_OF_RANGE;
	// Set bit
	if (bit_set.test(pos))
		return OK;
	bit_set.set(pos);

	// Track set boundaries
	if (pos > last_set)
		last_set  = pos;
	if ((pos < first_set) || (first_set < 0))
		first_set = pos;

	// Update CG string
	if (count > 0)
		snprintf(buff, 4, ",%d", pos);
	else
		snprintf(buff, 4, "%d", pos);
	cg_str.append(buff);

	++count;
	if (none) none = false;
	return OK;
}

ResourceBitset::ExitCode_t ResourceBitset::Reset() {
	bit_set.reset();
	first_set = last_set = R_ID_NONE;
	none  = true;
	count = 0;
	return OK;
}



} // namespace res

} // namespace bbque

