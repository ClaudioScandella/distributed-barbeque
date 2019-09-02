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

#ifndef BBQUE_AGENT_PROXY_GRPC_IMPL_H_
#define BBQUE_AGENT_PROXY_GRPC_IMPL_H_

#include <atomic>

#include "bbque/plugins/agent_proxy_if.h"
#include "bbque/system.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/distributed_manager.h"

#include <grpc/grpc.h>
#include "agent_com.grpc.pb.h"

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

class AgentImpl final: public bbque::RemoteAgent::Service
{

public:
	explicit AgentImpl():
		system(bbque::System::GetInstance()),
		logger(bbque::utils::Logger::GetLogger(AGENT_PROXY_NAMESPACE".grpc.imp")){
		}

	virtual ~AgentImpl() {}
	
	// Functions override from agent_com.grpc.pb.h
	
	grpc::Status Discover(
		grpc::ServerContext * context,
		const bbque::DiscoverRequest * request,
		bbque::DiscoverReply * reply) override;
		
	grpc::Status Ping(
		grpc::ServerContext * context,
		const bbque::GenericRequest * request,
		bbque::GenericReply * reply) override;

	grpc::Status GetResourceStatus(
		grpc::ServerContext * context,
		const bbque::ResourceStatusRequest * request,
		bbque::ResourceStatusReply * reply) override;

	grpc::Status GetWorkloadStatus(
		grpc::ServerContext * context,
		const bbque::GenericRequest * request,
		bbque::WorkloadStatusReply * reply) override;

	grpc::Status GetChannelStatus(
		grpc::ServerContext * context,
		const bbque::GenericRequest * request,
		bbque::ChannelStatusReply * reply) override;

	grpc::Status SetNodeManagementAction(
		grpc::ServerContext * context,
		const bbque::NodeManagementRequest * action,
		bbque::GenericReply * error) override;
private:
	DistributedManager & dism = DistributedManager::GetInstance();
	
	bbque::System & system;

	std::unique_ptr<bbque::utils::Logger> logger;
	
};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_AGENT_PROXY_GRPC_IMPL_H_
