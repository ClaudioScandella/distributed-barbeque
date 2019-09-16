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

#include "agent_impl.h"

#include "bbque/config.h"

#include <unistd.h>

// Just for debug
#include <chrono>
#include <thread>
#include <time.h>

#ifdef CONFIG_BBQUE_PM
  #include "bbque/pm/power_manager.h"
#endif

namespace bbque
{
namespace plugins
{

grpc::Status AgentImpl::Discover(
		grpc::ServerContext * context,
		const bbque::DiscoverRequest * request,
		bbque::DiscoverReply * reply) {

	logger->Debug("Discover function called");

#ifdef CONFIG_BBQUE_DIST_FULLY

	reply->set_iam(bbque::DiscoverReply_IAm_INSTANCE);
	reply->set_id(0);

#else
#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	int id, localID;

	switch(request->iam()){
	case bbque::DiscoverRequest_IAm_INSTANCE:
		logger->Debug("Request from INSTANCE. Discover cancelled");
		return grpc::Status::CANCELLED;
	case bbque::DiscoverRequest_IAm_NEW:
		logger->Debug("Request from NEW");
		localID = dism.GetLocalID();
		switch(localID) {
		case -1:
			logger->Debug("I am NEW. Discover cancelled");
			// NEW
			return grpc::Status::CANCELLED;
		case 0:
			// Master
			reply->set_iam(DiscoverReply_IAm_MASTER);
			id = dism.GetNewID();
			reply->set_id(id);
			logger->Debug("I am MASTER. I reply with a new ID: %d", id);
			break;
		default:
			// Slave
			reply->set_iam(DiscoverReply_IAm_SLAVE);
			id = dism.GetLocalID();
			reply->set_id(id);
			logger->Debug("I am SLAVE. I reply with my ID: %d", id);
			break;
		}
		break;
	case bbque::DiscoverRequest_IAm_MASTER:
		logger->Debug("Request from MASTER");
		localID = dism.GetLocalID();
		switch(localID) {
		case -1:
			logger->Debug("I am NEW. Discover cancelled");
			// NEW
			return grpc::Status::CANCELLED;
		case 0:
			// Master. This should never happen. If this case happens then there are 2 masters, that is an unwanted behaviour.
			logger->Error("Duplicate MASTER found.");
			exit(-1);
		default:
			// Slave
			reply->set_iam(DiscoverReply_IAm_SLAVE);
			id = dism.GetLocalID();
			reply->set_id(id);
			logger->Debug("I am SLAVE. I reply with my ID: %d", id);
			break;
		}
		break;
	case bbque::DiscoverRequest_IAm_SLAVE:
		logger->Debug("Request from SLAVE");
		localID = dism.GetLocalID();
		switch(localID) {
		case -1:
			logger->Debug("I am NEW. Discover cancelled");
			// NEW
			return grpc::Status::CANCELLED;
		case 0:
			logger->Debug("I am MASTER. I reply with ID: 0");
			// Master
			reply->set_iam(DiscoverReply_IAm_MASTER);
			reply->set_id(0);
			break;
		default:
			// Slave
			reply->set_iam(DiscoverReply_IAm_SLAVE);
			int id = dism.GetLocalID();
			reply->set_id(id);
			logger->Debug("I am SLAVE. I reply with my ID: %d", id);
			break;
		}
		break;
	default:
		logger->Error("Request from unexepected instance.");
		exit(-1);
	}

#endif
#endif

	return grpc::Status::OK;
}

grpc::Status AgentImpl::Ping(
	grpc::ServerContext * context,
	const bbque::GenericRequest * request,
	bbque::GenericReply * reply) {

	logger->Debug("Ping function called");

	reply->set_value(GenericReply_Code_OK);

	return grpc::Status::OK;
}

grpc::Status AgentImpl::GetResourceStatus(
		grpc::ServerContext * context,
		const bbque::ResourceStatusRequest * request,
		bbque::ResourceStatusReply * reply) {

	logger->Debug("ResourceStatus: request from sys%d",
		request->sender_id());
	if (request->path().empty()) {
		logger->Error("ResourceStatus: invalid resource path specified");
		return grpc::Status::CANCELLED;
	}

	// Call ResourceAccounter member functions...
	int64_t total = system.ResourceTotal(request->path());
	int64_t used  = system.ResourceUsed(request->path());
	reply->set_total(total);
	reply->set_used(used);

	// Power information...
	bbque::res::ResourcePtr_t resource(system.GetResource(request->path()));
	if (resource == nullptr) {
		logger->Error("ResourceStatus: invalid resource path specified");
		return grpc::Status::CANCELLED;
	}

	bbque::res::ResourcePathPtr_t resource_path(
		system.GetResourcePath(request->path()));
	if (resource_path == nullptr) {
		logger->Error("ResourceStatus: invalid resource path specified");
		return grpc::Status::CANCELLED;
	}

	uint32_t degr_perc = 100;
	uint32_t power_mw = 0, temp = 0, load = 0;
#ifdef CONFIG_BBQUE_PM
	bbque::PowerManager & pm(bbque::PowerManager::GetInstance());
	pm.GetPowerUsage(resource_path, power_mw);
	pm.GetTemperature(resource_path, temp);
	pm.GetLoad(resource_path, load);
#endif
	reply->set_degradation(degr_perc);
	reply->set_power_mw(power_mw);
	reply->set_temperature(temp);
	reply->set_load(load);

	return grpc::Status::OK;
}


grpc::Status AgentImpl::GetWorkloadStatus(
		grpc::ServerContext * context,
		const bbque::GenericRequest * request,
		bbque::WorkloadStatusReply * reply) {
		
	logger->Debug("WorkloadStatus: request from sys%d",
		request->sender_id());
	reply->set_nr_running(system.ApplicationsCount(
		bbque::app::ApplicationStatusIF::RUNNING));
	reply->set_nr_ready(system.ApplicationsCount(
		bbque::app::ApplicationStatusIF::READY));

	return grpc::Status::OK;
}

grpc::Status AgentImpl::GetChannelStatus(
		grpc::ServerContext * context,
		const bbque::GenericRequest * request,
		bbque::ChannelStatusReply * reply) {
		
	logger->Debug("ChannelStatus: request from sys%d",
		request->sender_id());
	reply->set_connected(true);

	return grpc::Status::OK;
}


grpc::Status AgentImpl::SetNodeManagementAction(
		grpc::ServerContext * context,
		const bbque::NodeManagementRequest * action,
		bbque::GenericReply * error) {

	logger->Debug(" === SetNodeManagementAction ===");
	logger->Info("Management action: %d ", action->value());
	error->set_value(bbque::GenericReply::OK);

	return grpc::Status::OK;
}

} // namespace plugins

} // namespace bbque
