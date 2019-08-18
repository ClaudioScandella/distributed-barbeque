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

#ifndef BBQUE_AGENT_PROXY_GRPC_H_
#define BBQUE_AGENT_PROXY_GRPC_H_

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/time.h>

#include "bbque/utils/logging/logger.h"
#include "bbque/utils/worker.h"
#include "bbque/plugins/agent_proxy_if.h"
#include "bbque/plugin_manager.h"
#include "bbque/plugins/plugin.h"
#include "bbque/plugins/agent_proxy_types.h"

#include "agent_client.h"
#include "agent_impl.h"
#include "agent_com.grpc.pb.h"

#define MODULE_NAMESPACE AGENT_PROXY_NAMESPACE".grpc"
#define MODULE_CONFIG AGENT_PROXY_CONFIG

namespace bbque
{
namespace plugins
{

using bbque::agent::ExitCode_t;

/**
 * @class AgentProxyGRPC
 *
 */
class AgentProxyGRPC: public bbque::plugins::AgentProxyIF, public utils::Worker
{
public:

	AgentProxyGRPC();

	virtual ~AgentProxyGRPC();

	// --- Plugin required
	static void * Create(PF_ObjectParams *);

	static int32_t Destroy(void *);
	// ---

	void StartServer();

	void StopServer();

	void WaitForServerToStop();

	void SetPlatformDescription(bbque::pp::PlatformDescription const * platform);

	// ----------------- Query status functions --------------------

	ExitCode_t GetResourceStatus(
	        std::string const & resource_path,
	        agent::ResourceStatus & status) override;


	ExitCode_t GetWorkloadStatus(
	        std::string const & system_path, agent::WorkloadStatus & status) override;

	ExitCode_t GetWorkloadStatus(
	        int system_id, agent::WorkloadStatus & status) override;


	ExitCode_t GetChannelStatus(
	        std::string const & system_path, agent::ChannelStatus & status) override;

	ExitCode_t GetChannelStatus(
	        int system_id, agent::ChannelStatus & status) override;


	// ------------- Multi-agent management functions ------------------

	ExitCode_t SendJoinRequest(std::string const & system_path) override;

	ExitCode_t SendJoinRequest(int system_id) override;


	ExitCode_t SendDisjoinRequest(std::string const & system_path) override;

	ExitCode_t SendDisjoinRequest(int system_id) override;


	// ----------- Scheduling / Resource allocation functions ----------

	ExitCode_t SendScheduleRequest(
	        std::string const & system_path,
	        agent::ApplicationScheduleRequest const & request) override;

private:

	std::string server_address_port = "0.0.0.0:";

	static uint32_t port_num;

	std::unique_ptr<bu::Logger> logger;


	bbque::pp::PlatformDescription const * platform;

	uint16_t local_sys_id;


	AgentImpl service;

	std::unique_ptr<grpc::Server> server;

	std::map<uint16_t, std::shared_ptr<AgentClient>> clients;

	bool server_started = false;

	// Plugin required
	static bool configured;

	static bool Configure(PF_ObjectParams * params);


	void Task();

	void RunServer();


	uint16_t GetSystemId(std::string const & path) const;


	std::shared_ptr<AgentClient> GetAgentClient(uint16_t system_id);

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_AGENT_PROXY_GRPC_H_
