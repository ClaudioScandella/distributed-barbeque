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

#include "bbque/plugins/agent_proxy_if.h"
#include "bbque/system.h"
#include "bbque/utils/logging/logger.h"

#include <grpc/grpc.h>
#include "agent_com.grpc.pb.h"

namespace bbque
{
namespace plugins
{

class AgentImpl final: public bbque::RemoteAgent::Service
{

public:
	explicit AgentImpl():
		system(bbque::System::GetInstance()),
		logger(bbque::utils::Logger::GetLogger(AGENT_PROXY_NAMESPACE".grpc.imp")) {
	}

	virtual ~AgentImpl() {}

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

	bbque::System & system;

	std::unique_ptr<bbque::utils::Logger> logger;

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_AGENT_PROXY_GRPC_IMPL_H_
