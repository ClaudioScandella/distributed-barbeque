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

#ifndef BBQUE_ANDROID_POLL_H_
#define BBQUE_ANDROID_POLL_H_

#include <poll.h>

static inline int ppoll(struct pollfd *fds, nfds_t nfds,
		const struct timespec *timeout, const sigset_t *sigmask) {

	if (timeout == NULL)
		return poll(fds, nfds, -1);

	if (timeout->tv_sec == 0)
		return poll(fds, nfds, 500);

	return poll(fds, nfds, timeout->tv_sec * 1000);
}

#endif // BBQUE_ANDROID_POLL_H_
