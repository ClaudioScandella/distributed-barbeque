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

#ifndef BBQUE_AGENT_PROXY_IF_H
#define BBQUE_AGENT_PROXY_IF_H

#include "bbque/pp/platform_description.h"
#include "bbque/plugins/agent_proxy_types.h"

#include "../plugins/agent_proxy/grpc/proto/agent_com.grpc.pb.h"

#define AGENT_PROXY_NAMESPACE "bq.gx"
#define AGENT_PROXY_CONFIG    "AgentProxy"

using bbque::DiscoverRequest;
using bbque::DiscoverReply;

namespace bbque
{
namespace plugins
{

using bbque::agent::ExitCode_t;

/**
 * @class AgentProxyIF
 *
 * @brief Interface for enabling the multi-agent configuration of
 * the BarbequeRTRM
 */
class AgentProxyIF {

public:

	AgentProxyIF() {}

	virtual ~AgentProxyIF() {}


	virtual void StartServer() = 0;

	virtual void StopServer() = 0;

	virtual void WaitForServerToStop() = 0;


	virtual void SetPlatformDescription(bbque::pp::PlatformDescription const * platform) {
		(void) platform;
	}

	// ----------------- Query status functions --------------------
	
	virtual ExitCode_t Discover(
		std::string ip, bbque::agent::DiscoverRequest& iam) = 0;
		
	virtual ExitCode_t Ping(
		int system_id, int & ping_value) = 0;
	
	/**
	 * @brief GetResourceStatus
	 * @param path
	 * @param status
	 * @return
	 */
	virtual ExitCode_t GetResourceStatus(
		std::string const & resource_path,
		agent::ResourceStatus & status) = 0;

	/**
	 * @brief GetWorkloadStatus
	 * @param path
	 * @param status
	 * @return
	 */
	virtual ExitCode_t GetWorkloadStatus(
		std::string const & path, agent::WorkloadStatus & status) = 0;
	/**
	 * @param system_id
	 */
	virtual ExitCode_t GetWorkloadStatus(
		int system_id, agent::WorkloadStatus & status) = 0;

	/**
	 * @brief GetChannelStatus
	 * @param path
	 * @param status
	 * @return
	 */
	virtual ExitCode_t GetChannelStatus(
		std::string const & path, agent::ChannelStatus & status) = 0;
	/**
	 * @param system_id
	 */
	virtual ExitCode_t GetChannelStatus(
		int system_id, agent::ChannelStatus & status) = 0;


	// ------------- Multi-remote management functions ------------------

	/**
	 * @brief SendJoinRequest
	 * @param path
	 * @return
	 */
	virtual ExitCode_t SendJoinRequest(std::string const & path) = 0;
	/**
	 * @param system_id
	 */
	virtual ExitCode_t SendJoinRequest(int system_id) = 0;

	/**
	 * @brief SendDisjoinRequest
	 * @param path
	 * @return
	 */
	virtual ExitCode_t SendDisjoinRequest(std::string const & path) = 0;
	/**
	 * @param system_id
	 */
	virtual ExitCode_t SendDisjoinRequest(int system_id) = 0;


	// ----------- Scheduling / Resource allocation functions ----------

	/**
	* @brief SendScheduleRequest
	* @param path
	* @param request
	* @return
	*/
	virtual ExitCode_t SendScheduleRequest(
		std::string const & path,
		agent::ApplicationScheduleRequest const & request) = 0;

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_REMOTE_PROXY_IF_H
