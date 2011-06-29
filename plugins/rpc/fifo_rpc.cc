/**
 *       @file  static_plugin.cc
 *      @brief  An example of static C++ plugin
 *
 * This defines a simple example of static C++ plugin which is intended both to
 * demostrate how to write them and to test the PluginManager implementation.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  01/28/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =============================================================================
 */

#include "fifo_rpc.h"

#include "bbque/modules_factory.h"

#include "bbque/config.h"
#include <boost/filesystem.hpp>

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace br = bbque::rtlib;
namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace bbque { namespace plugins {

FifoRPC::FifoRPC(std::string const & fifo_dir) :
	initialized(false),
	conf_fifo_dir(fifo_dir),
	rpc_fifo_fd(0) {

	// Get a logger
	plugins::LoggerIF::Configuration conf(MODULE_NAMESPACE);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	assert(logger!=NULL);

}

FifoRPC::~FifoRPC() {
	fs::path fifo_path(conf_fifo_dir);
	fifo_path /= "/"BBQUE_PUBLIC_FIFO;

	logger->Debug("FIFO RPC: cleaning up FIFO [%s]...",
			fifo_path.string().c_str());

	::close(rpc_fifo_fd);
	// Remove the server side pipe
	::unlink(fifo_path.string().c_str());
}

//----- RPCChannelIF module interface

int FifoRPC::Init() {
	int error;
	fs::path fifo_path(conf_fifo_dir);
	boost::system::error_code ec;

	if (initialized)
		return 0;

	logger->Debug("FIFO RPC: channel initialization...");

	fifo_path /= "/"BBQUE_PUBLIC_FIFO;
	logger->Debug("FIFO RPC: checking FIFO [%s]...",
			fifo_path.string().c_str());

	// If the FIFO already exists: destroy it and rebuild a new one
	if (fs::exists(fifo_path, ec)) {
		logger->Debug("FIFO RPC: destroying old FIFO [%s]...",
			fifo_path.string().c_str());
		error = ::unlink(fifo_path.string().c_str());
		if (error) {
			logger->Crit("FIFO RPC: cleanup old FIFO [%s] FAILED "
					"(Error: %s)",
					fifo_path.string().c_str(),
					strerror(error));
			assert(error == 0);
			return -1;
		}
	}

	// Make dir (if not already present)
	logger->Debug("FIFO RPC: create dir [%s]...",
			fifo_path.parent_path().c_str());
	fs::create_directories(fifo_path.parent_path(), ec);

	// Create the server side pipe (if not already existing)
	logger->Debug("FIFO RPC: create FIFO [%s]...",
			fifo_path.string().c_str());
	error = ::mkfifo(fifo_path.string().c_str(), 0666);
	if (error) {
		logger->Error("FIFO RPC: RPC FIFO [%s] cration FAILED",
				fifo_path.string().c_str());
		return -2;
	}

	// Ensuring we have a pipe
	if (fs::status(fifo_path, ec).type() != fs::fifo_file) {
		logger->Error("ERROR, RPC FIFO [%s] already in use",
				fifo_path.string().c_str());
		return -3;
	}

	// Opening the server side pipe (R/W to keep it opened)
	logger->Debug("FIFO RPC: opening R/W...");
	rpc_fifo_fd = ::open(fifo_path.string().c_str(),
			O_RDWR);
	if (rpc_fifo_fd < 0) {
		logger->Error("FAILED opening RPC FIFO [%s]",
					fifo_path.string().c_str());
		rpc_fifo_fd = 0;
		::unlink(fifo_path.string().c_str());
		return -4;
	}

	// Marking channel al already initialized
	initialized = true;

	logger->Info("FIFO RPC: channel initialization DONE\n");
	return 0;
}

size_t FifoRPC::RecvMessage(rpc_msg_ptr_t & msg) {
	br::rpc_fifo_header_t hdr;
	void *fifo_buff_ptr;
	size_t bytes;

	logger->Debug("FIFO RPC: waiting message...");

	// Wait for the next message being available
	// ... which always starts with the FIFO header
	bytes = ::read(rpc_fifo_fd, (void*)&hdr, FIFO_PKT_SIZE(header));
	if (bytes<=0) {
		if (bytes==EINTR)
			logger->Debug("FIFO RPC: exiting FIFO read...");
		else
			logger->Error("FIFO RPC: fifo read error");
		return bytes;
	}

	// Allocate a new message buffer
	fifo_buff_ptr = ::malloc(hdr.fifo_msg_size);
	if (!fifo_buff_ptr) {
		char c;
		logger->Error("FIFO RPC: message buffer creation FAILED");
		// Remove the remaining message from the FIFO
		for ( ; bytes<hdr.fifo_msg_size; bytes++) {
			::read(rpc_fifo_fd, (void*)&c, sizeof(char));
		}
		return 0;
	}

	// Save header into new buffer
	::memcpy(fifo_buff_ptr, &hdr, FIFO_PKT_SIZE(header));

	// Recover the payload start pointer
	switch (hdr.rpc_msg_type) {
	case br::RPC_APP_PAIR:

		::read(rpc_fifo_fd, &(((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->rpc_fifo),
			hdr.fifo_msg_size - FIFO_PKT_SIZE(header));

		msg = (rpc_msg_ptr_t)&(((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->pyl);
		logger->Debug("FIFO RPC: Rx FIFO_HDR [sze: %hd, off: %hd, typ: %hd] "
				"RPC_HDR [typ: %d, pid: %d, eid: %hd]",
			((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->hdr.fifo_msg_size,
			((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->hdr.rpc_msg_offset,
			((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->hdr.rpc_msg_type,
			((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->pyl.hdr.typ,
			((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->pyl.hdr.app_pid,
			((br::rpc_fifo_APP_PAIR_t*)fifo_buff_ptr)->pyl.hdr.exc_id
		     );
		break;
	default:
		::read(rpc_fifo_fd, &(((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->pyl),
			hdr.fifo_msg_size - FIFO_PKT_SIZE(header));

		msg = &(((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->pyl);
		logger->Debug("FIFO RPC: Rx FIFO_HDR [sze: %hd, off: %hd, typ: %hd] "
				"RPC_HDR [typ: %d, pid: %d, eid: %hd]",
			((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->hdr.fifo_msg_size,
			((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->hdr.rpc_msg_offset,
			((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->hdr.rpc_msg_type,
			((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->pyl.typ,
			((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->pyl.app_pid,
			((br::rpc_fifo_GENERIC_t*)fifo_buff_ptr)->pyl.exc_id
		     );
	}
	// Recovery the payload size to be returned
	bytes = hdr.fifo_msg_size - hdr.rpc_msg_offset;

	//logger->Debug("Rx Buffer FIFO_HDR [@%p] RPC_HDR [@%p] => Offset: %hd",
	//		fifo_buff_ptr, msg,
	//		(size_t)msg-(size_t)fifo_buff_ptr);

	// HEXDUMP the received buffer
	//RPC_FIFO_HEX_DUMP_BUFFER(fifo_buff_ptr, hdr.fifo_msg_size);

	return bytes;
}

RPCChannelIF::plugin_data_t FifoRPC::GetPluginData(
		rpc_msg_ptr_t & msg) {
	fifo_data_t * pd;
	fs::path fifo_path(conf_fifo_dir);
	boost::system::error_code ec;
	br::rpc_fifo_APP_PAIR_t * hdr;
	int fd;


	// We should have the FIFO dir already on place
	assert(initialized);

	// We should also have a valid RPC message
	assert(msg->typ == br::RPC_APP_PAIR);

	// Get a reference to FIFO header
	hdr = container_of(msg, br::rpc_fifo_APP_PAIR_t, pyl);
	logger->Debug("FIFO RPC: plugin data initialization...");

	// Build fifo path
	fifo_path /= "/";
	fifo_path /= hdr->rpc_fifo;

	// The application should build the channel, this could be used as
	// an additional handshaking protocol and API versioning verification
	logger->Debug("FIFO RPC: checking for application FIFO [%s]...",
			fifo_path.string().c_str());
	if (!fs::exists(fifo_path, ec)) {
		logger->Error("FIFO RPC: apps FIFO NOT FOUND [%s]...",
				fifo_path.string().c_str());
		goto err_open;
	}

	// Ensuring we have a pipe
	if (fs::status(fifo_path, ec).type() != fs::fifo_file) {
		logger->Error("FIFO RPC: apps FIFO not valid [%s]",
				fifo_path.string().c_str());
		goto err_open;
	}

	// Opening the application side pipe WRITE only
	logger->Debug("FIFO RPC: opening (WR only)...");
	fd = ::open(fifo_path.string().c_str(), O_WRONLY);
	if (fd < 0) {
		logger->Error("FAILED opening application RPC FIFO [%s] (Error %d: %s)",
					fifo_path.string().c_str(), errno, strerror(errno));
		fd = 0;
		// Debugging: abort on too many files open
		assert(errno!=EMFILE);
		goto err_open;
	}

	// Build a new set of plugins data
	pd = (fifo_data_t*)::malloc(sizeof(fifo_data_t));
	if (!pd) {
		logger->Error("FIFO RPC: get plugin data (malloc) FAILED");
		goto err_malloc;
	}

	::strncpy(pd->app_fifo_filename, hdr->rpc_fifo, BBQUE_FIFO_NAME_LENGTH);
	pd->app_fifo_fd = fd;

	logger->Info("FIFO RPC: [%5d:%s] channel initialization DONE",
			pd->app_fifo_fd, hdr->rpc_fifo);

	return plugin_data_t(pd);

err_malloc:
	::close(fd);
err_open:
	return plugin_data_t();

}

void FifoRPC::ReleasePluginData(plugin_data_t & pd) {
	fifo_data_t * ppd = (fifo_data_t*)pd.get();

	assert(initialized==true);
	assert(ppd && ppd->app_fifo_fd);

	// Close the FIFO and cleanup plugin data
	::close(ppd->app_fifo_fd);

	logger->Info("FIFO RPC: [%5d:%s] channel release DONE",
			ppd->app_fifo_fd, ppd->app_fifo_filename);

}

size_t FifoRPC::SendMessage(plugin_data_t & pd, rpc_msg_ptr_t msg,
		size_t count) {
	fifo_data_t * ppd = (fifo_data_t*)pd.get();
	br::rpc_fifo_GENERIC_t *fifo_msg;
	ssize_t error;

	assert(rpc_fifo_fd);
	assert(ppd && ppd->app_fifo_fd);

	// FIXME copying the RPC message into the FIFO one is not efficient at all,
	// but this it the less intrusive patch to use a single write on the PIPE.
	// A better solution, e.g. pre-allocating a channel message, should be
	// provided by a future patch

	// Build a new message of the required type
	// NOTE all BBQ generated command have the sam FIFO layout
	fifo_msg = (br::rpc_fifo_GENERIC_t*)::malloc(
			offsetof(br::rpc_fifo_GENERIC_t, pyl) + count);

	// Copy the RPC message into the FIFO msg
	::memcpy(&(fifo_msg->pyl), msg, count);

	logger->Debug("FIFO RPC: TX [typ: %d, sze: %d] "
			"using app channel [%d:%s]...",
			msg->typ, count,
			ppd->app_fifo_fd,
			ppd->app_fifo_filename);

	// Send the RPC FIFO message
	fifo_msg->hdr.fifo_msg_size = offsetof(br::rpc_fifo_GENERIC_t, pyl) + count;
	fifo_msg->hdr.rpc_msg_offset = offsetof(br::rpc_fifo_GENERIC_t, pyl);
	fifo_msg->hdr.rpc_msg_type = msg->typ;
	error = ::write(ppd->app_fifo_fd, fifo_msg, fifo_msg->hdr.fifo_msg_size);
	if (error == -1) {
		logger->Error("FIFO RPC: send massage (header) FAILED (Error %d: %s)",
				errno, strerror(errno));
		return -1;
	}

	return fifo_msg->hdr.fifo_msg_size;
}

void FifoRPC::FreeMessage(rpc_msg_ptr_t & msg) {
	void* fifo_msg;

	// Recover the beginning of the FIFO message
	switch (msg->typ) {
	case br::RPC_APP_PAIR:
		fifo_msg = (void*)container_of(msg, br::rpc_fifo_APP_PAIR_t, pyl);
		break;
	default:
		fifo_msg = (void*)container_of(msg, br::rpc_fifo_GENERIC_t, pyl);
		break;
	}

	// Releaseing the FIFO message buffer
	::free(fifo_msg);
}

//----- static plugin interface

void * FifoRPC::Create(PF_ObjectParams *params) {
	static std::string conf_fifo_dir;

	// Declare the supported options
	po::options_description fifo_rpc_opts_desc("FIFO RPC Options");
	fifo_rpc_opts_desc.add_options()
		(MODULE_NAMESPACE".dir", po::value<std::string>
		 (&conf_fifo_dir)->default_value(BBQUE_PUBLIC_FIFO_PATH),
		 "path of the FIFO dir")
		;
	static po::variables_map fifo_rpc_opts_value;

	// Get configuration params
	PF_Service_ConfDataIn data_in;
	data_in.opts_desc = &fifo_rpc_opts_desc;
	PF_Service_ConfDataOut data_out;
	data_out.opts_value = &fifo_rpc_opts_value;
	PF_ServiceData sd;
	sd.id = MODULE_NAMESPACE;
	sd.request = &data_in;
	sd.response = &data_out;

	int32_t response = params->
		platform_services->InvokeService(PF_SERVICE_CONF_DATA, sd);
	if (response!=PF_SERVICE_DONE)
		return NULL;

	DB(fprintf(stderr, "FIFO RPC: using dir [%s]\n",
			conf_fifo_dir.c_str()));

	return new FifoRPC(conf_fifo_dir);

}

int32_t FifoRPC::Destroy(void *plugin) {
  if (!plugin)
    return -1;
  delete (FifoRPC *)plugin;
  return 0;
}

} // namesapce plugins

} // namespace bque

