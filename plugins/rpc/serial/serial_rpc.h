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

#ifndef BBQUE_PLUGINS_SERIAL_RPC_H_
#define BBQUE_PLUGINS_SERIAL_RPC_H_

#include <cstdint>

#include "bbque/config.h"
#include "bbque/plugins/rpc_channel.h"
#include "bbque/plugins/plugin.h"
#include "bbque/utils/logging/logger.h"

#define MODULE_NAMESPACE RPC_CHANNEL_NAMESPACE ".serial"

// These are the parameters received by the PluginManager on create calls
struct PF_ObjectParams;

namespace bu = bbque::utils;

namespace bbque { namespace plugins {

/**
 * @brief A SERIAL implementation of the RPCChannelIF interface.
 *
 * This class provide a SERIAL based communication channel between the BarbequeRTRM
 * and the applications.
 */
class SerialRPC: public RPCChannelIF {


typedef struct channel_data : ChannelData {
	/** The handler to the application channel */
	int app_channel_fd;
	/** The application channel filename */
	char app_channel_filename[BBQUE_RPC_PUBLIC_CHANNEL_NAME_LENGTH];
} channel_data_t;


public:

	virtual ~SerialRPC();

//----- static plugin interface

	static void * Create(PF_ObjectParams *);

	static int32_t Destroy(void *);


//----- RPCChannelIF module interface

	virtual int Poll();

	virtual ssize_t RecvMessage(rpc_msg_ptr_t & msg);

	virtual ssize_t SendMessage(
	    plugin_data_t & pd, rpc_msg_ptr_t msg, size_t count);

	virtual void FreeMessage(rpc_msg_ptr_t & msg);


	virtual plugin_data_t GetPluginData(rpc_msg_ptr_t & msg);

	virtual void ReleasePluginData(plugin_data_t & pd);

//----

private:

	/**
	 * @brief System logger instance
	 */
	std::unique_ptr<bu::Logger> logger;

	/**
	 * @brief True if the channel has been correctly initalized
	 */
	bool initialized;

	/**
	 * @brief Filesystem directory of the channel
	 */
	std::string conf_channel_dir;

	/**
	 * @brief Channel file descriptor
	 */
	int channel_fd;

	/**
	 * @brief  The plugin constructor
	 * Plugins objects could be build only by using the "create" method.
	 * Usually the PluginManager acts as object
	 * @param   
	 * @return  
	 */
	SerialRPC(std::string const &);

	int Init();

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_PLUGINS_TESTING_H_
