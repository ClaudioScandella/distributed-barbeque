
#include "bbque/modules_factory.h"
#include "bbque/pp/remote_platform_proxy.h"
#include "bbque/config.h"

namespace bbque {
namespace pp {

RemotePlatformProxy::RemotePlatformProxy() {
	logger = bu::Logger::GetLogger(REMOTE_PLATFORM_PROXY_NAMESPACE);
	assert(logger);
}

const char* RemotePlatformProxy::GetPlatformID(int16_t system_id) const {
	(void) system_id;
	logger->Error("GetPlatformID - Not implemented.");
	return "";
}

const char* RemotePlatformProxy::GetHardwareID(int16_t system_id) const {
	(void) system_id;
	logger->Error("GetHardwareID - Not implemented.");
	return "";
}


RemotePlatformProxy::ExitCode_t RemotePlatformProxy::Setup(SchedPtr_t papp) {
	(void) papp;
	logger->Error("Setup - Not implemented.");
	return PLATFORM_OK;
}

RemotePlatformProxy::ExitCode_t RemotePlatformProxy::LoadPlatformData() {

	ExitCode_t ec = LoadAgentProxy();
	if (ec != PLATFORM_OK) {
		logger->Error("Cannot start Agent Proxy");
		return ec;
	}

	return PLATFORM_OK;
}

RemotePlatformProxy::ExitCode_t RemotePlatformProxy::LoadAgentProxy() {

	// Carica il modulo agent denominato bq.gx.grpc che è l'agent proxy.
	agent_proxy = std::unique_ptr<bbque::plugins::AgentProxyIF>(
		ModulesFactory::GetModule<bbque::plugins::AgentProxyIF>(
			std::string(AGENT_PROXY_NAMESPACE) + ".grpc"));

	if (agent_proxy == nullptr) {
		logger->Fatal("Agent Proxy plugin loading failed!");
		return PLATFORM_AGENT_PROXY_ERROR;
	}

	logger->Debug("Providing the platform description to Agent Proxy...");
	// Setta semplicemente una descrizione della piattaforma corrente in agent proxy.
	agent_proxy->SetPlatformDescription(&GetPlatformDescription());

	logger->Info("Agent Proxy plugin ready");

	return PLATFORM_OK;
}

RemotePlatformProxy::ExitCode_t RemotePlatformProxy::Refresh() {
	logger->Error("Refresh - Not implemented.");
	return PLATFORM_OK;
}

RemotePlatformProxy::ExitCode_t RemotePlatformProxy::Release(SchedPtr_t papp) {
	(void) papp;
	logger->Error("Release - Not implemented.");
	return PLATFORM_OK;
}

RemotePlatformProxy::ExitCode_t RemotePlatformProxy::ReclaimResources(SchedPtr_t papp) {
	(void) papp;
	logger->Error("ReclaimResources - Not implemented.");
	return PLATFORM_OK;
}

RemotePlatformProxy::ExitCode_t RemotePlatformProxy::MapResources(
		SchedPtr_t papp,
		ResourceAssignmentMapPtr_t pres,
                bool excl) {
	(void) papp;
	(void) pres;
	(void) excl;

	logger->Error("MapResources - Not implemented.");
	return PLATFORM_OK;
}


void RemotePlatformProxy::Exit() {
	StopServer();
	WaitForServerToStop();
}


bool RemotePlatformProxy::IsHighPerformance(
		bbque::res::ResourcePathPtr_t const & path) const {
	return false;
}

// --- AgentProxy

void RemotePlatformProxy::StartServer() {
	if (agent_proxy == nullptr) {
		logger->Error("Server start failed. AgentProxy plugin missing");
		return;
	}
	return agent_proxy->StartServer();
}

void RemotePlatformProxy::StopServer() {
	if (agent_proxy == nullptr) {
		logger->Error("Server stop failed. AgentProxy plugin missing");
		return;
	}
	return agent_proxy->StopServer();
}

void RemotePlatformProxy::WaitForServerToStop() {
	if (agent_proxy == nullptr) {
		logger->Error("Server wait failed. AgentProxy plugin missing");
		return;
	}
	return agent_proxy->WaitForServerToStop();
}

bbque::agent::ExitCode_t RemotePlatformProxy::Discover(
	std::string ip, bbque::agent::DiscoverRequest& iam) {
		if (agent_proxy == nullptr) {
			logger->Error("Discover failed. AgentProxy plugin missing");
			return bbque::agent::ExitCode_t::PROXY_NOT_READY;
		}
		return agent_proxy->Discover(ip, iam);
	}
		
bbque::agent::ExitCode_t RemotePlatformProxy::Ping(
	int system_id, int & ping_value) {
		if (agent_proxy == nullptr) {
			logger->Error("Ping failed. AgentProxy plugin missing");
			return bbque::agent::ExitCode_t::PROXY_NOT_READY;
		}
		return agent_proxy->Ping(system_id, ping_value);
	}

bbque::agent::ExitCode_t
RemotePlatformProxy::GetResourceStatus(
		std::string const & resource_path, agent::ResourceStatus & status) {
	if (agent_proxy == nullptr) {
		logger->Error("GetResourceStatus failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->GetResourceStatus(resource_path, status);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::GetWorkloadStatus(
		std::string const & system_path, agent::WorkloadStatus & status) {
	if (agent_proxy == nullptr) {
		logger->Error("GetWorkloadStatus failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->GetWorkloadStatus(system_path, status);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::GetWorkloadStatus(
		int system_id, agent::WorkloadStatus & status) {
	if (agent_proxy == nullptr) {
		logger->Error("GetWorkloadStatus failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->GetWorkloadStatus(system_id, status);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::GetChannelStatus(
		std::string const & system_path, agent::ChannelStatus & status) {
	if (agent_proxy == nullptr) {
		logger->Error("GetChannelStatus failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->GetChannelStatus(system_path, status);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::GetChannelStatus(int system_id, agent::ChannelStatus & status) {
	if (agent_proxy == nullptr) {
		logger->Error("GetChannelStatus failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->GetChannelStatus(system_id, status);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::SendJoinRequest(std::string const & system_path) {
	if (agent_proxy == nullptr) {
		logger->Error("SendJoinRequest failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->SendJoinRequest(system_path);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::SendJoinRequest(int system_id) {
	if (agent_proxy == nullptr) {
		logger->Error("SendJoinRequest failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->SendJoinRequest(system_id);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::SendDisjoinRequest(std::string const & system_path) {
	if (agent_proxy == nullptr) {
		logger->Error("SendDisjoinRequest failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->SendDisjoinRequest(system_path);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::SendDisjoinRequest(int system_id) {
	if (agent_proxy == nullptr) {
		logger->Error("SendDisjoinRequest failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->SendDisjoinRequest(system_id);
}

bbque::agent::ExitCode_t
RemotePlatformProxy::SendScheduleRequest(
		std::string const & system_path,
		agent::ApplicationScheduleRequest const & request) {
	if (agent_proxy == nullptr) {
		logger->Error("SendDisjoinRequest failed. AgentProxy plugin missing");
		return bbque::agent::ExitCode_t::PROXY_NOT_READY;
	}
	return agent_proxy->SendScheduleRequest(system_path, request);
}

} // namespace pp
} // namespace bbque

