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

#include "agent_client.h"

namespace bbque
{
namespace plugins
{

using bbque::agent::ExitCode_t;

// remote_id può restare un numero che è quello deciso dal distributed manager. Quando lo si invia va eliminato il numero per renderlo generico.
AgentClient::AgentClient(const std::string & ip):
	ip_address(ip)
{
	logger = bbque::utils::Logger::GetLogger(AGENT_PROXY_NAMESPACE".grpc.cln");
	Connect();
}

ExitCode_t AgentClient::Connect()
{
	logger->Debug("Connecting to %s...", ip_address.c_str());
	if (channel != nullptr) {
		logger->Debug("Channel already open");
		return agent::ExitCode_t::OK;
	}

	channel = grpc::CreateChannel(
			  ip_address, grpc::InsecureChannelCredentials());
	logger->Debug("Channel open");

	service_stub = bbque::RemoteAgent::NewStub(channel);
	if (service_stub == nullptr) {
		logger->Error("Channel opening failed");
		return agent::ExitCode_t::AGENT_UNREACHABLE;
	}
	logger->Debug("Stub ready");

	return ExitCode_t::OK;
}


bool AgentClient::IsConnected()
{
	if (!channel || !service_stub) {
		return false;
	}
	if ((channel->GetState(false) != 0) && (channel->GetState(false) != 2))
		return true;
	return true;
}

// ---------- Status

ExitCode_t AgentClient::Discover(std::string ip, bbque::DiscoverRequest& iam) {

	std::shared_ptr<grpc::Channel> c = grpc::CreateChannel(ip, grpc::InsecureChannelCredentials());
	std::shared_ptr<bbque::RemoteAgent::Stub> stub = bbque::RemoteAgent::NewStub(c);

	// std::unique_ptr<bbque::utils::Logger> logger = bbque::utils::Logger::GetLogger(AGENT_PROXY_NAMESPACE".grpc.cln");


	// char buffer[100];
	// sprintf(buffer, "agent_client: Discover ip %s", ip.c_str());
	// logger->Debug(buffer);

	bbque::DiscoverRequest request = iam;

	grpc::Status status;
	grpc::ClientContext context;
	bbque::DiscoverReply reply;

	// sprintf(buffer, "agent_client: peer: %s", context.peer().c_str());
	// logger->Debug(buffer);

	status = stub->Discover(&context, request, &reply);

	// sprintf(buffer, "agent_client: peer: %s", context.peer().c_str());
	// logger->Debug(buffer);

	// logger->Debug("agent_client: Discover: 2");

	if (status.ok()) {
		if (reply.iam() == bbque::DiscoverReply_IAm_INSTANCE){
			// logger->Debug("agent_client: Discover: 3");
			return ExitCode_t::OK;
		}
		else {
			// logger->Debug("agent_client: Discover: 4");
			return ExitCode_t::REQUEST_REJECTED;
		}
    } else {
    	// logger->Debug("agent_client: Discover: 5");
    	return ExitCode_t::AGENT_UNREACHABLE;
    }
}
	
ExitCode_t AgentClient::Ping(std::string ip, int & milliseconds) {
	std::shared_ptr<grpc::Channel> c = grpc::CreateChannel(ip, grpc::InsecureChannelCredentials());
	std::shared_ptr<bbque::RemoteAgent::Stub> stub = bbque::RemoteAgent::NewStub(c);

	bbque::GenericRequest request;
	request.set_sender_id(0);
	// request.set_dest_id(0);

	grpc::Status status;
	grpc::ClientContext context;
	bbque::GenericReply reply;

	bbque::utils::Timer t;

	auto start_timer = Clock::now();

	status = stub->Ping(&context, request, &reply);

	auto end_timer = Clock::now();

	// Just for debug
	bool culo = (std::chrono::duration_cast<std::chrono::microseconds>(end_timer - start_timer).count() % 10) % 2;
	if(!culo)
		std::this_thread::sleep_for(std::chrono::seconds(3));
	culo = (std::chrono::duration_cast<std::chrono::microseconds>(end_timer - start_timer).count() % 10) % 3;
	if(!culo)
		std::this_thread::sleep_for(std::chrono::seconds(3));

	if (status.ok()) {
		if (reply.value() == GenericReply_Code_OK) {
			milliseconds = std::chrono::duration_cast<std::chrono::microseconds>(end_timer - start_timer).count();
<<<<<<< HEAD
// std::cout << "milliseconds: " << milliseconds << std::endl;
=======
std::cout << "milliseconds: " << milliseconds << std::endl;
>>>>>>> 8ebc97520887cc15902a6b524bbea77369dd4898
			return ExitCode_t::OK;
		}
		else {
			return ExitCode_t::REQUEST_REJECTED;
		}
    } else {
    	return ExitCode_t::AGENT_UNREACHABLE;
    }
}

ExitCode_t AgentClient::GetResourceStatus(
		std::string const & resource_path,
		agent::ResourceStatus & resource_status) {
	// Connect...
	ExitCode_t exit_code = Connect();
	if (exit_code != ExitCode_t::OK) {
		logger->Error("ResourceStatus: Connection failed");
		return exit_code;
	}

	// Do RPC call
	bbque::ResourceStatusRequest request;

#ifdef CONFIG_BBQUE_DIST_FULLY
	request.set_sender_id(0);
	// request.set_dest_id(0);
#elif CONFIG_BBQUE_DIST_HIERARCHICAL
	request.set_sender_id(local_system_id);
	// request.set_dest_id(remote_system_id);
#endif

	request.set_path(resource_path);
	request.set_average(false);

	grpc::Status status;
	grpc::ClientContext context;
	bbque::ResourceStatusReply reply;

	logger->Debug("ResourceStatus: Calling implementation...");
	status = service_stub->GetResourceStatus(&context, request, &reply);
	if (!status.ok()) {
		logger->Error("ResourceStatus: Returned code %d", status.error_code());
		return ExitCode_t::AGENT_DISCONNECTED;
	}

	resource_status.total = reply.total();
	resource_status.used  = reply.used();
	resource_status.power_mw    = reply.power_mw();
	resource_status.temperature = reply.temperature();
	resource_status.degradation = reply.degradation();
        logger->Debug("ResourceStatus: T:%3d, U:%3d, PWR:%3d, TEMP:%5d",
		resource_status.total, resource_status.used,
		resource_status.power_mw, resource_status.temperature);

	return ExitCode_t::OK;
}

ExitCode_t AgentClient::GetWorkloadStatus(
		agent::WorkloadStatus & workload_status) {

	ExitCode_t exit_code = Connect();
	if (exit_code != ExitCode_t::OK) {
		logger->Error("WorkloadStatus: Connection failed");
		return exit_code;
	}

	bbque::GenericRequest request;

#ifdef CONFIG_BBQUE_DIST_FULLY
	request.set_sender_id(0);
	// request.set_dest_id(0);
#elif CONFIG_BBQUE_DIST_HIERARCHICAL
	request.set_sender_id(local_system_id);
	// request.set_dest_id(remote_system_id);
#endif

	grpc::Status status;
	grpc::ClientContext context;
	bbque::WorkloadStatusReply reply;

	logger->Debug("WorkloadStatus: Calling implementation...");
	status = service_stub->GetWorkloadStatus(&context, request, &reply);
	if (!status.ok()) {
		logger->Error("WorkloadStatus: Returned code %d", status.error_code());
		return ExitCode_t::AGENT_DISCONNECTED;
	}

	workload_status.nr_ready   = reply.nr_ready();
	workload_status.nr_running = reply.nr_running();
	logger->Debug("WorkloadStatus: RUN: %2d, RDY: %2d",
		workload_status.nr_ready, workload_status.nr_running);

	return ExitCode_t::OK;
}

ExitCode_t AgentClient::GetChannelStatus(agent::ChannelStatus & channel_status) {
	timer.start();
	ExitCode_t exit_code = Connect();
	if (exit_code != ExitCode_t::OK) {
		logger->Error("ChannelStatus: Connection failed");
		timer.stop();
		return exit_code;
	}

	bbque::GenericRequest request;

#ifdef CONFIG_BBQUE_DIST_FULLY
	request.set_sender_id(0);
	// request.set_dest_id(0);
#elif CONFIG_BBQUE_DIST_HIERARCHICAL
	request.set_sender_id(local_system_id);
	// request.set_dest_id(remote_system_id);
#endif

	grpc::Status status;
	grpc::ClientContext context;
	bbque::ChannelStatusReply reply;

	logger->Debug("ChannelStatus: Calling implementation...");
	status = service_stub->GetChannelStatus(&context, request, &reply);
	if (!status.ok()) {
		logger->Error("ChannelStatus: Returned code %d", status.error_code());
		timer.stop();
		return ExitCode_t::AGENT_DISCONNECTED;
	}

	channel_status.connected = reply.connected();
	timer.stop();
	channel_status.latency_ms = timer.getElapsedTimeMs();
	logger->Debug("ChannelStatus: Connected: %d, Latency: %.0f ms",
		channel_status.connected, channel_status.latency_ms);

	return ExitCode_t::OK;
}

// ----------- Multi-agent management

ExitCode_t AgentClient::SendJoinRequest()
{

	return ExitCode_t::OK;
}

ExitCode_t AgentClient::SendDisjoinRequest()
{

	return ExitCode_t::OK;
}

// ----------- Scheduling / Resource allocation

ExitCode_t AgentClient::SendScheduleRequest(
        agent::ApplicationScheduleRequest const & request)
{

	return ExitCode_t::OK;
}

} // namespace plugins

} // namespace bbque

