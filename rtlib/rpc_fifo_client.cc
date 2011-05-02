/**
 *       @file  rpc_fifo.cc
 *      @brief  A message passing based RPC framework based on UNIX FIFO
 *
 * Definition of the RPC protocol based on UNIX FIFOs to implement the
 * Barbeque communication channel. This defines the communication protocol in
 * terms of message format and functionalities.
 * The communication protocol must be aligend with the RTLib supported
 * services.
 *
 * @see bbque/rtlib.h
 * @see bbque/rtlib/rpc_messages.h
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  01/13/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =============================================================================
 */

#include "bbque/rtlib/rpc_fifo_client.h"

#include "bbque/rtlib/rpc_messages.h"


#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>


#define DB(x) x

#define FMT_DBG(fmt) "RTLIB_FIFO [DBG] - "fmt
#define FMT_INF(fmt) "RTLIB_FIFO [INF] - "fmt
#define FMT_WRN(fmt) "RTLIB_FIFO [WRN] - "fmt
#define FMT_ERR(fmt) "RTLIB_FIFO [ERR] - "fmt


namespace bbque { namespace rtlib {

BbqueRPC_FIFO_Client::BbqueRPC_FIFO_Client() :
	BbqueRPC(),
	app_fifo_path(BBQUE_PUBLIC_FIFO_PATH"/"),
	bbque_fifo_path(BBQUE_PUBLIC_FIFO_PATH"/"BBQUE_PUBLIC_FIFO) {

	DB(fprintf(stderr, FMT_DBG("Building FIFO RPC channel\n")));
}

BbqueRPC_FIFO_Client::~BbqueRPC_FIFO_Client() {
	DB(fprintf(stderr, FMT_DBG("Releaseing the FIFO RPC channel...\n")));
	ChannelRelease();
}

RTLIB_ExitCode BbqueRPC_FIFO_Client::ChannelRelease() {
	rpc_fifo_undef_t fifo_undef = {
		{
			FIFO_PKT_SIZE(undef)+RPC_PKT_SIZE(app_exit),
			FIFO_PKT_SIZE(undef),
			RPC_APP_EXIT
		}
	};
	rpc_msg_app_exit_t msg_exit = {
		RPC_APP_EXIT, chTrdPid, 0};
	size_t bytes;
	int error;

	DB(fprintf(stderr, FMT_DBG("Releasing FIFO RPC channel\n")));

	// Send FIFO header
	DB(fprintf(stderr, FMT_DBG("Sending FIFO header "
		"[sze: %hd, off: %hd, typ: %hd]...\n"),
		fifo_undef.header.fifo_msg_size,
		fifo_undef.header.rpc_msg_offset,
		fifo_undef.header.rpc_msg_type
	));
	bytes = ::write(server_fifo_fd, (void*)&fifo_undef, FIFO_PKT_SIZE(undef));
	if (bytes<=0) {
		fprintf(stderr, FMT_ERR("write to BBQUE fifo FAILED [%s]\n"),
			bbque_fifo_path.c_str());
		return RTLIB_BBQUE_CHANNEL_WRITE_FAILED;
	}

	// Send RPC header
	DB(fprintf(stderr, FMT_DBG("Sending RPC header "
		"[typ: %d, pid: %d, eid: %hd]...\n"),
		msg_exit.typ,
		msg_exit.app_pid,
		msg_exit.exc_id
	));
	bytes = ::write(server_fifo_fd, (void*)&msg_exit, RPC_PKT_SIZE(app_exit));
	if (bytes<=0) {
		fprintf(stderr, FMT_ERR("write to BBQUE fifo FAILED [%s]\n"),
			bbque_fifo_path.c_str());
		return RTLIB_BBQUE_CHANNEL_WRITE_FAILED;
	}

	// Closing the private FIFO
	error = ::unlink(app_fifo_path.c_str());
	if (error) {
		fprintf(stderr, FMT_ERR("FAILED unlinking the application FIFO [%s] "
					"(Error %d: %s)\n"),
				app_fifo_path.c_str(), errno, strerror(errno));
		return RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED;
	}

	return RTLIB_OK;

}

RTLIB_ExitCode BbqueRPC_FIFO_Client::ChannelPair() {
	rpc_fifo_app_pair_t fifo_pair = {
		{
			FIFO_PKT_SIZE(app_pair)+RPC_PKT_SIZE(app_pair),
			FIFO_PKT_SIZE(app_pair),
			RPC_APP_PAIR
		},
		"\0"
	};
	rpc_msg_app_pair_t msg_pair = {
		{RPC_APP_PAIR, chTrdPid, 0},
		BBQUE_RPC_FIFO_MAJOR_VERSION,
		BBQUE_RPC_FIFO_MINOR_VERSION};
	rpc_fifo_header_t hdr;
	rpc_msg_resp_t resp;
	size_t bytes;

	DB(fprintf(stderr, FMT_DBG("Pairing FIFO channels...\n")));

	// Setting up FIFO name
	strncpy(fifo_pair.rpc_fifo, app_fifo_filename, BBQUE_FIFO_NAME_LENGTH);

	// Send FIFO header
	DB(fprintf(stderr, FMT_DBG("Sending FIFO header "
		"[sze: %hd, off: %hd, typ: %hd, pipe: %s]...\n"),
		fifo_pair.header.fifo_msg_size,
		fifo_pair.header.rpc_msg_offset,
		fifo_pair.header.rpc_msg_type,
		fifo_pair.rpc_fifo
	));
	bytes = ::write(server_fifo_fd, (void*)&fifo_pair, FIFO_PKT_SIZE(app_pair));
	if (bytes<=0) {
		fprintf(stderr, FMT_ERR("write to BBQUE fifo FAILED [%s]\n"),
			bbque_fifo_path.c_str());
		return RTLIB_BBQUE_CHANNEL_WRITE_FAILED;
	}

	// Send RPC header
	DB(fprintf(stderr, FMT_DBG("Sending RPC header "
		"[typ: %d, pid: %d, eid: %hd, mjr: %d, mnr: %d]...\n"),
		msg_pair.header.typ,
		msg_pair.header.app_pid,
		msg_pair.header.exc_id,
		msg_pair.mjr_version,
		msg_pair.mnr_version
	));
	bytes = ::write(server_fifo_fd, (void*)&msg_pair, RPC_PKT_SIZE(app_pair));
	if (bytes<=0) {
		fprintf(stderr, FMT_ERR("write to BBQUE fifo FAILED [%s]\n"),
				bbque_fifo_path.c_str());
		return RTLIB_BBQUE_CHANNEL_WRITE_FAILED;
	}

	// Receive BBQUE response
	DB(fprintf(stderr, FMT_DBG("Waiting BBQUE response...\n")));

	// Read response FIFO header
	bytes = ::read(client_fifo_fd, (void*)&hdr, FIFO_PKT_SIZE(header));
	if (bytes<=0) {
		fprintf(stderr, FMT_ERR("FAILED read from application fifo [%s]\n"),
				app_fifo_path.c_str());
		return RTLIB_BBQUE_CHANNEL_READ_FAILED;
	}
	assert(hdr.rpc_msg_type == RPC_BBQ_RESP);

	// Read response RPC header
	bytes = ::read(client_fifo_fd, (void*)&resp, RPC_PKT_SIZE(resp));
	if (bytes<=0) {
		fprintf(stderr, FMT_ERR("FAILED read from application fifo [/%s]\n"),
				app_fifo_path.c_str());
		return RTLIB_BBQUE_CHANNEL_READ_FAILED;
	}

	// Check RPC server response
	if (resp.result != RTLIB_OK) {
		fprintf(stderr, FMT_ERR("bbque RPC pairing FAILED\n"));
		return RTLIB_BBQUE_CHANNEL_READ_FAILED;
	}

	return RTLIB_OK;

}

RTLIB_ExitCode BbqueRPC_FIFO_Client::ChannelSetup() {
	RTLIB_ExitCode result = RTLIB_OK;
	int error;

	DB(fprintf(stderr, FMT_INF("Initializing RPC FIFO channel\n")));

	// Opening server FIFO
	DB(fprintf(stderr, FMT_DBG("Opening bbque fifo [%s]...\n"), bbque_fifo_path.c_str()));
	server_fifo_fd = ::open(bbque_fifo_path.c_str(), O_WRONLY|O_NONBLOCK);
	if (server_fifo_fd < 0) {
		fprintf(stderr, FMT_ERR("FAILED opening bbque fifo [%s] (Error %d: %s)\n"),
				bbque_fifo_path.c_str(), errno, strerror(errno));
		return RTLIB_BBQUE_CHANNEL_SETUP_FAILED;
	}

	// Linking the server pipe to an ASIO stream descriptor
	//out.assign(server_fifo_fd);

	// Setting up application FIFO complete path
	app_fifo_path += app_fifo_filename;

	DB(fprintf(stderr, FMT_DBG("Creating [%s]...\n"),
				app_fifo_path.c_str()));

	// Creating the client side pipe
	error = ::mkfifo(app_fifo_path.c_str(), 0644);
	if (error) {
		fprintf(stderr, FMT_ERR("FAILED creating application FIFO [%s]\n"),
				app_fifo_path.c_str());
		result = RTLIB_BBQUE_CHANNEL_SETUP_FAILED;
		goto err_create;
	}

	DB(fprintf(stderr, FMT_DBG("Opening R/W...\n")));

	// Opening the client side pipe
	// NOTE: this is opened R/W to keep it opened even if server
	// should disconnect
	client_fifo_fd = ::open(app_fifo_path.c_str(), O_RDWR);
	if (client_fifo_fd < 0) {
		fprintf(stderr, FMT_ERR("FAILED opening application FIFO [%s]\n"),
				app_fifo_path.c_str());
		result = RTLIB_BBQUE_CHANNEL_SETUP_FAILED;
		goto err_open;
	}
	//in.assign(client_fifo_fd);

	// Pairing channel with server
	result = ChannelPair();
	if (result != RTLIB_OK)
		goto err_use;

	return RTLIB_OK;

err_use:
	::close(client_fifo_fd);
err_open:
	::unlink(app_fifo_path.c_str());
err_create:
	::close(server_fifo_fd);
	return result;

}

void BbqueRPC_FIFO_Client::ChannelTrd() {
	std::unique_lock<std::mutex> chSetup_ul(chSetup_mtx);

	// Getting client PID
	chTrdPid = gettid();
	DB(fprintf(stderr, FMT_INF("RPC FIFO channel thread [PID: %d] started\n"),
				chTrdPid));

	// Notifying the thread has beed started
	trdStarted_cv.notify_one();

	// Waiting for channel setup to be completed
	chSetup_cv.wait(chSetup_ul);

	// Wait for BBQ messages

}

RTLIB_ExitCode BbqueRPC_FIFO_Client::_Init(
			const char *name) {
	std::unique_lock<std::mutex> trdStatus_ul(trdStatus_mtx);
	RTLIB_ExitCode result = RTLIB_OK;

	// Starting the communication thread
	ChTrd = std::thread(&BbqueRPC_FIFO_Client::ChannelTrd, this);
	trdStarted_cv.wait(trdStatus_ul);

	// Setting up application FIFO filename
	snprintf(app_fifo_filename, BBQUE_FIFO_NAME_LENGTH,
			"bbque_%05d_%s", chTrdPid, name);

	// Setting up the communication channel
	result = ChannelSetup();
	if (result != RTLIB_OK)
		return result;

	// Start the reception thread
	chSetup_cv.notify_one();

	return result;
}

RTLIB_ExecutionContextHandler BbqueRPC_FIFO_Client::_Register(
			const char* name,
			const RTLIB_ExecutionContextParams* params) {
	//Silence "args not used" warning.
	(void)name;
	(void)params;

	fprintf(stderr, FMT_DBG("EXC Regisister: not yet implemeted"));

	return NULL;
}

void BbqueRPC_FIFO_Client::_Unregister(
			const RTLIB_ExecutionContextHandler ech) {
	//Silence "args not used" warning.
	(void)ech;

	fprintf(stderr, FMT_DBG("EXC Unregisister: not yet implemeted"));
}

RTLIB_ExitCode BbqueRPC_FIFO_Client::_Start(
			const RTLIB_ExecutionContextHandler ech) {
	//Silence "args not used" warning.
	(void)ech;

	fprintf(stderr, FMT_DBG("EXC Start: not yet implemeted"));

	return RTLIB_OK;
}

RTLIB_ExitCode BbqueRPC_FIFO_Client::_Stop(
			const RTLIB_ExecutionContextHandler ech) {
	//Silence "args not used" warning.
	(void)ech;

	fprintf(stderr, FMT_DBG("EXC Stop: not yet implemeted"));

	return RTLIB_OK;
}

RTLIB_ExitCode BbqueRPC_FIFO_Client::_Set(
			const RTLIB_ExecutionContextHandler ech,
			RTLIB_Constraint* constraints,
			uint8_t count) {
	//Silence "args not used" warning.
	(void)ech;
	(void)constraints;
	(void)count;

	fprintf(stderr, FMT_DBG("EXC Set: not yet implemeted"));

	return RTLIB_OK;
}

RTLIB_ExitCode BbqueRPC_FIFO_Client::_Clear(
			const RTLIB_ExecutionContextHandler ech) {
	//Silence "args not used" warning.
	(void)ech;

	fprintf(stderr, FMT_DBG("EXC Clear: not yet implemeted"));

	return RTLIB_OK;
}

RTLIB_ExitCode BbqueRPC_FIFO_Client::_GetWorkingMode(
			RTLIB_ExecutionContextHandler ech,
			RTLIB_WorkingModeParams *wm) {
	//Silence "args not used" warning.
	(void)ech;
	(void)wm;

	fprintf(stderr, FMT_DBG("EXC GetWorkingMode: not yet implemeted"));

	return RTLIB_OK;
}

void BbqueRPC_FIFO_Client::_Exit() {
	ChannelRelease();
}

} // namespace rtlib

} // namespace bbque

