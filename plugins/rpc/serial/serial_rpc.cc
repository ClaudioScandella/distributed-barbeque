/*
 * Copyright (C) 2015  Politecnico di Milano
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

#include "serial_rpc.h"

#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#ifdef ANDROID
# include "bbque/android/ppoll.h"
#endif
#include <fcntl.h>
#include <csignal>

#include "bbque/utils/utility.h"

namespace bl = bbque::rtlib;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

SerialRPC::SerialRPC(std::string const & ch_dir):
	initialized(false),
	conf_channel_dir(ch_dir) {

	// Get a logger
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	assert(logger);

	// Ignore SIGPIPE, which will otherwise result into a BBQ termination.
	// Indeed, in case of write errors the timeouts allows BBQ to react to
	// the application not responding or disappearing.
	signal(SIGPIPE, SIG_IGN);
	logger->Debug("Built SerialRPC object @%p", (void*)this);
}

SerialRPC::~SerialRPC() {
	fs::path channel_path(conf_channel_dir);
	channel_path /= "/" BBQUE_RPC_PUBLIC_CHANNEL;

	logger->Debug("SERIAL RPC: cleaning up channel [%s]...",
			channel_path.string().c_str());
}

//----- RPCChannelIF module interface

int SerialRPC::Init() {
	boost::system::error_code ec;
	fs::path channel_path(conf_channel_dir);
	channel_path /= "/" BBQUE_RPC_PUBLIC_CHANNEL;
	int error = 0;

	if (initialized)
		return 0;

	// Initialization code...
	logger->Debug("SERIAL RPC: channel initialization...");
	// If the channel already exists: destroy it and rebuild a new one
	logger->Debug("SERIAL RPC: checking channel [%s]...",
			channel_path.string().c_str());

	if (fs::exists(channel_path, ec)) {
		logger->Debug("SERIAL RPC: destroying old channel [%s]...",
			channel_path.string().c_str());
		error = ::unlink(channel_path.string().c_str());
		if (error) {
			logger->Crit("SERIAL RPC: cleanup old channel [%s] FAILED "
					"(Error: %s)",
					channel_path.string().c_str(),
					strerror(error));
			assert(error == 0);
			return -1;
		}
	}

	// Make dir (if not already present)
	logger->Debug("SERIAL RPC: create dir [%s]...",
			channel_path.parent_path().c_str());
	fs::create_directories(channel_path.parent_path(), ec);

	// Create the server side pipe (if not already existing)
	logger->Debug("SERIAL RPC: create channel [%s]...",
			channel_path.string().c_str());
	error = ::mkfifo(channel_path.string().c_str(), 0666);
	if (error) {
		logger->Error("SERIAL RPC: RPC channel [%s] cration FAILED",
				channel_path.string().c_str());
		return -2;
	}

	// Ensuring we have a pipe
	if (fs::status(channel_path, ec).type() != fs::fifo_file) {
		logger->Error("ERROR, RPC channel [%s] already in use",
				channel_path.string().c_str());
		return -3;
	}

	// Opening the server side pipe (R/W to keep it opened)
	logger->Debug("SERIAL RPC: opening R/W...");
	channel_fd = ::open(channel_path.string().c_str(), O_RDWR);
	if (channel_fd < 0) {
		logger->Error("FAILED opening RPC channel [%s]",
					channel_path.string().c_str());
		channel_fd = 0;
		::unlink(channel_path.string().c_str());
		return -4;
	}

	// Ensuring the channel is R/W to everyone
	if (fchmod(channel_fd, S_IRUSR|S_IWUSR|S_IWGRP|S_IWOTH)) {
		logger->Error("FAILED setting permissions on RPC channel [%s] "
				"(Error %d: %s)",
				channel_path.string().c_str(),
				errno, strerror(errno));
		channel_fd = 0;
		::unlink(channel_path.string().c_str());
		return -5;
	}

	if (error) {
		logger->Error("SERIAL RPC: RPC SERIAL [%s] cration FAILED",
				channel_path.c_str());
		return -2;
	}

	// Marking channel as already initialized
	initialized = true;

	logger->Info("SERIAL RPC: channel initialization DONE");
	return 0;
}

int SerialRPC::Poll() {
	struct pollfd channel_poll;
	sigset_t sigmask;
	int ret = 0;

	// Bind to the FIFO input stream
	channel_poll.fd     = channel_fd;
	channel_poll.events = POLLIN;

	// Wait for data availability or signal
	logger->Debug("SERIAL RPC: waiting message...");
	sigemptyset(&sigmask);	// Wait for data availability or signal
	ret = ::ppoll(&channel_poll, 1, NULL, &sigmask);
	if (ret < 0) {
		logger->Debug("SERIAL RPC: interrupted...");
		ret = -EINTR;
	}

	return ret;
}

ssize_t SerialRPC::RecvMessage(rpc_msg_ptr_t & msg) {
	ssize_t bytes;

	// Read next message channel header
	// ...
	char hdr[10];
	bytes = ::read(channel_fd, (void*)&hdr, 10);
	// ...

	if (bytes <= 0) {
		if (bytes == EINTR)
			logger->Debug("SERIAL RPC: exiting SERIAL read...");
		else
			logger->Error("SERIAL RPC: fifo read error");
		return bytes;
	}

	// Set the message object content
	// msg =

	return bytes;

exit_read_failed:
	logger->Error("SERIAL RPC: read RPC message FAILED (Error %d: %s)",
			errno, strerror(errno));

	msg = NULL;
	return -errno;
}

ssize_t SerialRPC::SendMessage(
		plugin_data_t & pd,
		rpc_msg_ptr_t msg,
		size_t count) {
	ssize_t error    = 0;
	ssize_t msg_size = 0;
	channel_data_t * ppd = (channel_data_t *) pd.get();

	// Send the RPC channel message
	// ...
	if (error == -1) {
		logger->Error("SERIAL RPC: send massage (header) FAILED (Error %d: %s)",
				errno, strerror(errno));
		return -errno;
	}

	logger->Info("SERIAL RPC: Message sent [%d bytes]", msg_size);
	return msg_size;
}

void SerialRPC::FreeMessage(rpc_msg_ptr_t & msg) {

}


RPCChannelIF::plugin_data_t SerialRPC::GetPluginData(
		rpc_msg_ptr_t & msg) {
	channel_data_t * pd;
	fs::path channel_path(conf_channel_dir);

	// We should have the channel directory already on place
	assert(initialized);
	logger->Debug("SERIAL RPC: plugin data initialization...");

	// Extract plugin data...

	return plugin_data_t(pd);
}

void SerialRPC::ReleasePluginData(plugin_data_t & pd) {
	channel_data_t * ppd = (channel_data_t*) pd.get();
	assert(initialized==true);

	// Close the channel and cleanup plugin data
	// ::close(ppd->app_channel_fd);

	logger->Info("SERIAL RPC: [%5d:%s] channel release DONE",
			ppd->app_channel_fd, ppd->app_channel_filename);
}


//----- static plugin interface

void * SerialRPC::Create(PF_ObjectParams *params) {
	static std::string conf_channel_dir;

	// Declare the supported options
	po::options_description rpc_opts_desc("SERIAL RPC Options");
	rpc_opts_desc.add_options()
		(MODULE_NAMESPACE".dir", po::value<std::string>
		 (&conf_channel_dir)->default_value(BBQUE_PATH_VAR),
		 "path of the SERIAL channel dir")
		;
	static po::variables_map rpc_opts_value;

	// Get configuration params
	PF_Service_ConfDataIn data_in;
	data_in.opts_desc   = &rpc_opts_desc;
	PF_Service_ConfDataOut data_out;
	data_out.opts_value = &rpc_opts_value;
	PF_ServiceData sd;
	sd.id       = MODULE_NAMESPACE;
	sd.request  = &data_in;
	sd.response = &data_out;

	int32_t response = params->
		platform_services->InvokeService(PF_SERVICE_CONF_DATA, sd);
	if (response!=PF_SERVICE_DONE)
		return NULL;

	if (daemonized)
		syslog(LOG_INFO, "Using SERIAL RPC dir [%s]",
				conf_channel_dir.c_str());
	else
		fprintf(stderr, FI("SERIAL RPC: using dir [%s]\n"),
				conf_channel_dir.c_str());

	return new SerialRPC(conf_channel_dir);

}

int32_t SerialRPC::Destroy(void *plugin) {
  if (!plugin)
    return -1;
  delete (SerialRPC *)plugin;
  return 0;
}

} // namesapce plugins

} // namespace bque
