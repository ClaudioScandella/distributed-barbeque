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
#include <map>

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc++/support/time.h>

#include "bbque/distributed_manager.h"
#include "bbque/res/resource_path.h"
#include "bbque/res/resource_type.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/worker.h"
#include "bbque/plugins/agent_proxy_if.h"
#include "bbque/plugin_manager.h"
#include "bbque/plugins/plugin.h"

#include "agent_client.h"
#include "agent_impl.h"
#include "agent_com.grpc.pb.h"

#define MODULE_NAMESPACE AGENT_PROXY_NAMESPACE".grpc"
#define MODULE_CONFIG AGENT_PROXY_CONFIG

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using bbque::GenericRequest;
using bbque::GenericReply;
using bbque::DiscoverRequest;
using bbque::DiscoverReply;
using bbque::ResourceStatusRequest;
using bbque::ResourceStatusReply;
using bbque::WorkloadStatusReply;
using bbque::ChannelStatusReply;
using bbque::NodeManagementRequest;
using bbque::ApplicationSchedulingRequest;
using bbque::RemoteAgent;

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

	ExitCode_t Discover(
		std::string ip, bbque::agent::DiscoverRequest iam, bbque::agent::DiscoverReply& reply) override;
		
	ExitCode_t Ping(
		std::string ip, int & ping_value) override;

	ExitCode_t GetResourceStatus(
			int16_t instance_id,
	        std::string const & resource_path,
	        agent::ResourceStatus & status) override;

	ExitCode_t GetWorkloadStatus(
	        std::string const & system_path, agent::WorkloadStatus & status) override;

	ExitCode_t GetWorkloadStatus(
	        int16_t instance_id, agent::WorkloadStatus & status) override;


	ExitCode_t GetChannelStatus(
	        std::string const & system_path, agent::ChannelStatus & status) override;

	ExitCode_t GetChannelStatus(
	        int16_t instance_id, agent::ChannelStatus & status) override;


	// ------------- Multi-agent management functions ------------------

	ExitCode_t SendJoinRequest(std::string const & system_path) override;

	ExitCode_t SendJoinRequest(int16_t instance_id) override;


	ExitCode_t SendDisjoinRequest(std::string const & system_path) override;

	ExitCode_t SendDisjoinRequest(int16_t instance_id) override;


	// ----------- Scheduling / Resource allocation functions ----------

	ExitCode_t SendScheduleRequest(
	        int16_t instance_id,
	        agent::ApplicationScheduleRequest const & request) override;

private:

	DistributedManager & dism = DistributedManager::GetInstance();

	std::string server_address_port = "0.0.0.0:";

	static uint32_t port_num;

	std::unique_ptr<bu::Logger> logger;


	bbque::pp::PlatformDescription const * platform;

	uint16_t local_sys_id;


	AgentImpl service;

	std::unique_ptr<grpc::Server> server;

	std::map<std::string, std::shared_ptr<AgentClient>> clients;

	bool server_started = false;

	// Plugin required
	static bool configured;

	static bool Configure(PF_ObjectParams * params);


	void Task();

	void RunServer();


	uint16_t GetSystemId(std::string const & path) const;

	/**
	 * @brief Return the path substituting the system id with sys*
	 */
	virtual ExitCode_t GeneralizeSystemID(std::string path, std::string & generalized_path);


	std::shared_ptr<AgentClient> GetAgentClient(std::string ip);

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_AGENT_PROXY_GRPC_H_
