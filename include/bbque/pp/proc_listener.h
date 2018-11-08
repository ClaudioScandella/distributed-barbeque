/*
 * Copyright (C) 2016  Politecnico di Milano
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

#ifndef BBQUE_LINUX_PROCLISTENER_H_
#define BBQUE_LINUX_PROCLISTENER_H_


#include <memory>
#include <string>
#include <linux/cn_proc.h>

#include "bbque/app/application.h"
#include "bbque/config.h"
#include "bbque/configuration_manager.h"
#include "bbque/command_manager.h"
#include "bbque/modules_factory.h"
#include "bbque/pm/power_manager.h"
#include "bbque/process_manager.h"
#include "bbque/res/identifier.h"
#include "bbque/utils/worker.h"

namespace bu = bbque::utils;

namespace bbque {

class ProcessListener : public Worker {
public:
	/**
	 * @brief Constructor (Singleton)
	 */
	static ProcessListener & GetInstance();

	/**
	 * @brief Destructor
	 */
	~ProcessListener();

	int Setup(std::string const & name, std::string const & logname);

private:

	/*** ProcessManager instance */
	ProcessManager & prm;

	/*** Constructor */
	ProcessListener();

	/**
	 * @brief The linux process connector API client
	 */
	void Task();

	/**
	 * @brief The socket number of the connection to kernel's API
	 */
	int sock;

	/**
	 * @brief The logger used by this module
	 */
	std::unique_ptr<bu::Logger> logger;

	/**
	 * @brief Buffer
	 */
	char *buf;
	int buffSize;
	/**
	 * @brief Helper function to retrieve the name of a given PID
	 */
	std::string GetProcName(int pid);
};

} // namespace bbque

#endif // BBQUE_LINUX_PROCLISTENER_H_

