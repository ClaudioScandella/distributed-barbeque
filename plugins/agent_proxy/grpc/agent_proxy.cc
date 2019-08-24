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

#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include <boost/program_options/options_description.hpp>

#include "bbque/config.h"
#include "bbque/pp/platform_description.h"
#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_type.h"

#include "agent_proxy.h"

namespace bbque
{
namespace plugins
{

using bbque::agent::ExitCode_t;

boost::program_options::variables_map agent_proxy_opts_value;

uint32_t AgentProxyGRPC::port_num = BBQUE_AGENT_PROXY_PORT_DEFAULT;

// =======================[ Static plugin interface ]=========================

bool AgentProxyGRPC::configured = false;

void * AgentProxyGRPC::Create(PF_ObjectParams * params) {
	if (!Configure(params))
		return nullptr;

	return new AgentProxyGRPC();
}

int32_t AgentProxyGRPC::Destroy(void * plugin) {
	if (!plugin)
		return -1;
	delete (AgentProxyGRPC *)plugin;
	return 0;
}

bool AgentProxyGRPC::Configure(PF_ObjectParams * params) {
	if (configured)
		return true;

	// Declare the supported options
	boost::program_options::options_description
		agent_proxy_opts_desc("AgentProxy options");
	agent_proxy_opts_desc.add_options()
		(MODULE_CONFIG".port", boost::program_options::value<uint32_t>
		 (&port_num)->default_value(BBQUE_AGENT_PROXY_PORT_DEFAULT),
		 "Server port number");

	// Get configuration params
	PF_Service_ConfDataIn data_in;
	data_in.opts_desc = &agent_proxy_opts_desc;
	PF_Service_ConfDataOut data_out;
	data_out.opts_value = &agent_proxy_opts_value;

	PF_ServiceData sd;
	sd.id = MODULE_NAMESPACE;
	sd.request  = &data_in;
	sd.response = &data_out;

	int32_t response =
	        params->platform_services->InvokeService(PF_SERVICE_CONF_DATA, sd);

	if (response != PF_SERVICE_DONE)
		return false;

	return true;
}

// =============================================================================

AgentProxyGRPC::AgentProxyGRPC() {
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);
	server_address_port = std::string("0.0.0.0:") + std::to_string(port_num);
	// server_address_port = std::string("192.168.1.140:") + std::to_string(port_num);
	logger->Info("AgentProxy Server will listen on %s",
		server_address_port.c_str());
	Setup("AgentProxyServer", MODULE_NAMESPACE".srv");
}

AgentProxyGRPC::~AgentProxyGRPC() {
	logger->Info("Destroying the AgentProxy module...");
	clients.clear();
}

void AgentProxyGRPC::SetPlatformDescription(
		bbque::pp::PlatformDescription const * platform) {

	if ((platform == nullptr) || (platform->GetSystemsAll().empty())) {
		logger->Error("No system descriptors");
		return;
	}

	this->platform = platform;
	logger->Debug("Systems in the managed platform: %d", platform->GetSystemsAll().size());

	local_sys_id = platform->GetLocalSystem().GetId();
	logger->Debug("Local system id: %d", local_sys_id);
}

void AgentProxyGRPC::StartServer() {
	if (server != nullptr) {
		logger->Warn("Server already started");
		return;
	}
	logger->Info("Starting the server task...");
	Start();
}

void AgentProxyGRPC::Task() {
	logger->Debug("Server task launched");
	RunServer();
	logger->Info("Server stopped");
	Notify();
}

void AgentProxyGRPC::RunServer() {
	grpc::ServerBuilder builder;
	builder.AddListeningPort(
	        server_address_port, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);
	server = builder.BuildAndStart();
	logger->Notice("Server listening on %s", server_address_port.c_str());
	server->Wait();
}

void AgentProxyGRPC::StopServer() {
	logger->Info("Stopping the server task...");
	if (server == nullptr) {
		logger->Warn("Server already stopped");
		return;
	}
	server->Shutdown();
}

void AgentProxyGRPC::WaitForServerToStop() {
	Wait();
}


uint16_t AgentProxyGRPC::GetSystemId(const std::string & system_path) const {
	return bbque::res::ResourcePathUtils::GetID(
		system_path, bbque::res::GetResourceTypeString(
			bbque::res::ResourceType::SYSTEM));
}

ExitCode_t AgentProxyGRPC::GeneralizeSystemID(std::string path, std::string & generalized_path) {
	res::ResourcePath resPath = res::ResourcePath(path);

	if(resPath.ReplaceID(res::ResourceType::SYSTEM, resPath.GetID(res::ResourceType::SYSTEM), R_ID_NONE) != res::ResourcePath::ExitCode_t::OK)
		return agent::ExitCode_t::REQUEST_REJECTED;

	generalized_path = resPath.ToString();

	return agent::ExitCode_t::OK;
}

std::shared_ptr<AgentClient> AgentProxyGRPC::GetAgentClient(std::string ip) {
	logger->Debug("GetAgentClient: retrieving a client for ip %s", ip.c_str());
	// if (!platform->ExistSystem(remote_system_id)) {
	// 	logger->Error("GetAgentClient: sys%d not registered", remote_system_id);
	// 	return nullptr;
	// }

	auto sys_client = clients.find(ip);
	if(sys_client == clients.end()) {
		logger->Debug("GetAgentClient: creating a client for ip %s", ip.c_str());

		// Al posto di server_address_port bisogna mettere l'ip effettivo
		std::shared_ptr<AgentClient> client_ptr =
			std::make_shared<AgentClient>(ip);
		clients.emplace(ip, client_ptr);
	}
	logger->Debug("GetAgentClient: active clients = %d", clients.size());
	return clients.at(ip);
}

ExitCode_t AgentProxyGRPC::Discover(
	std::string ip, bbque::agent::DiscoverRequest& iam) {

		ExitCode_t result;

		bbque::DiscoverRequest request;

		switch (iam.iam)
		{
			case bbque::agent::IAm::INSTANCE:
				request.set_iam(bbque::DiscoverRequest_IAm_INSTANCE);
				break;
			case bbque::agent::IAm::NEW:
				request.set_iam(bbque::DiscoverRequest_IAm_NEW);
				break;
			case bbque::agent::IAm::MASTER:
				request.set_iam(bbque::DiscoverRequest_IAm_MASTER);
				break;
			case bbque::agent::IAm::SLAVE:
				return agent::ExitCode_t::REQUEST_REJECTED;
		}

		result = AgentClient::Discover(ip, request);

		return result;
	}
	
ExitCode_t AgentProxyGRPC::Ping(
		std::string ip, int & ping_value) {
		ExitCode_t result;

		result = AgentClient::Ping(ip, ping_value);

		return result;
	}

ExitCode_t AgentProxyGRPC::GetResourceStatus(
		int16_t instance_id,
		std::string const & resource_path,
		agent::ResourceStatus & status) {

	std::string general_path;

	if(GeneralizeSystemID(resource_path, general_path) != agent::ExitCode_t::OK)
		return agent::ExitCode_t::REQUEST_REJECTED;

	std::map<int, std::string> inst_map = dism.GetInstancesID();
	std::string ip = inst_map[instance_id];
	logger->Debug("ip: %s", ip.c_str());

	std::shared_ptr<AgentClient> client(GetAgentClient(ip));
	if (client)
	{
		logger->Debug("client found");
		logger->Debug("Calling client->GetResourceStatus(path, status)");
		logger->Debug("path: %s", general_path.c_str());
		return client->GetResourceStatus(general_path, status);
	}
	logger->Debug("client NOT found");

	return agent::ExitCode_t::AGENT_UNREACHABLE;
}

ExitCode_t AgentProxyGRPC::GetWorkloadStatus(
		std::string const & path,
		agent::WorkloadStatus & status) {
	return GetWorkloadStatus(GetSystemId(path), status);
}

ExitCode_t AgentProxyGRPC::GetWorkloadStatus(
		int16_t instance_id,
		agent::WorkloadStatus & status) {
	std::map<int, std::string> inst_map = dism.GetInstancesID();
	std::string ip = inst_map[instance_id];

	std::shared_ptr<AgentClient> client(GetAgentClient(ip));
	if (client)
		return client->GetWorkloadStatus(status);
	return agent::ExitCode_t::AGENT_UNREACHABLE;
}

ExitCode_t AgentProxyGRPC::GetChannelStatus(
		std::string const & path,
		agent::ChannelStatus & status) {
	return GetChannelStatus(GetSystemId(path), status);
}

ExitCode_t AgentProxyGRPC::GetChannelStatus(
		int16_t instance_id,
		agent::ChannelStatus & status) {
	std::map<int, std::string> inst_map = dism.GetInstancesID();
	std::string ip = inst_map[instance_id];

	std::shared_ptr<AgentClient> client(GetAgentClient(ip));
	if (client)
		return client->GetChannelStatus(status);
	return agent::ExitCode_t::AGENT_UNREACHABLE;
}


// ------------- Multi-agent management functions ------------------

ExitCode_t AgentProxyGRPC::SendJoinRequest(std::string const & path) {

	return agent::ExitCode_t::AGENT_UNREACHABLE;
}

ExitCode_t AgentProxyGRPC::SendJoinRequest(int16_t instance_id) {

	return agent::ExitCode_t::AGENT_UNREACHABLE;
}


ExitCode_t AgentProxyGRPC::SendDisjoinRequest(std::string const & path) {

	return agent::ExitCode_t::AGENT_UNREACHABLE;
}

ExitCode_t AgentProxyGRPC::SendDisjoinRequest(int16_t instance_id) {

	return agent::ExitCode_t::AGENT_UNREACHABLE;
}


// ----------- Scheduling / Resource allocation functions ----------

ExitCode_t AgentProxyGRPC::SendScheduleRequest(
		int16_t instance_id,
		agent::ApplicationScheduleRequest const & request) {

	return agent::ExitCode_t::AGENT_UNREACHABLE;
}

} // namespace plugins

} // namespace bbque
