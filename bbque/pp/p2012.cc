/*
 * Copyright (C) 2012  Politecnico di Milano
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

#include <cstring>

#include "bbque/app/working_mode.h"
#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_path.h"
#include "bbque/res/usage.h"
#include "bbque/pp/p2012.h"


namespace bbque {


P2012PP::P2012PP() :
	PlatformProxy(),
	out_queue_id(P2012_INVALID_QUEUE_ID),
	in_queue_id(P2012_INVALID_QUEUE_ID) {

	logger->Info("PLAT P2012: Built Platform Proxy");

	// NOTE: Could we move this at the end of LoadPlatformData?
	SetPilInitialized();
}

P2012PP::~P2012PP() {
	int p2012_result;
	logger->Info("PLAT P2012: Destroying Platform Proxy...");

	// Destroy message queues
	p2012_deleteMsgQueue(out_queue_id);
	p2012_deleteMsgQueue(in_queue_id);

	//TODO: Move outside
	_Stop();
	logger->Debug("PLAT P2012: Stop signal sent to platform");

	// Unmap shared memory buffer
	p2012_result = p2012_unmapMemBuf(&sh_mem);
	if (p2012_result) {
		logger->Error("PLAT P2012: Error in unmapping device descriptor (%s)",
				strerror(p2012_result));
		return;
	}
	logger->Debug("PLAT P2012: Bye!");
}

P2012PP::ExitCode_t P2012PP::_LoadPlatformData() {
	ExitCode_t result;
	logger->Info("PLAT P2012: ... Loading platform data ...");

	// Initialise message queues and shared memory buffer (hence the device
	// descriptor)
	result = InitPlatformComm();
	if (result != OK) {
		logger->Fatal("PLAT P2012: Platform initialization failed.");
		return PLATFORM_INIT_FAILED;
	}
	logger->Debug("PLAT P2012: Platform initialization performed");

	// Register the resources
	result = InitResources();
	if (result != OK) {
		logger->Fatal("PLAT P2012: Platform enumeration failed.");
		return PLATFORM_ENUMERATION_FAILED;
	}
	logger->Debug("PLAT P2012: Platform is ready");

	return OK;
}

P2012PP::ExitCode_t P2012PP::InitPlatformComm() {
	ExitCode_t result;
	int p2012_result;
	int fabric_addr;

	// Init p2012 user library
	p2012_result = p2012_initUsrLib();
	if (p2012_result != 0) {
		logger->Info("PLAT P2012: Initialization failed...");
		return PLATFORM_INIT_FAILED;
	}

	// Create output messages queue
	p2012_result = p2012_createMsgQueue(NULL, P2012_QUEUE_HOST,
			P2012_QUEUE_FC, NULL, &out_queue_id, &fabric_addr);
	if (p2012_result != 0) {
		logger->Fatal("PLAT P2012: Can't create output message queue (%s)",
				strerror(p2012_result));
		return PLATFORM_INIT_FAILED;
	}

	// Create input messages queue
	p2012_result = p2012_createMsgQueue(NULL, P2012_QUEUE_FC,
			P2012_QUEUE_HOST, NULL, &in_queue_id, &fabric_addr);
	if (p2012_result != 0) {
		logger->Fatal("PLAT P2012: Can't create input message queue (%s)",
				strerror(p2012_result));
		return PLATFORM_INIT_FAILED;
	}
	logger->Debug("PLAT P2012: Message queues initialized");

	// Notify the platform about BBQ starting
	result = NotifyPlatform(P2012_ALL, BBQ_START, in_queue_id);
	if (result != OK) {
		logger->Error("PLAT P2012: Error occurred in platform start message");
		return PLATFORM_COMM_ERROR;
	}
	logger->Debug("PLAT P2012: Start notification sent");

	// Initialize the shared memory buffer
	p2012_result = p2012_BBQInit(&sh_mem);
	if (p2012_result != 0) {
		logger->Fatal("PLAT P2012: Driver initialization failed (%s)",
				strerror(p2012_result));
		return PLATFORM_INIT_FAILED;
	}
	logger->Debug("PLAT P2012: Driver initialized");

	// Map the memory buffer (device descriptor) into userland
	pdev = (ManagedDevice_t *) p2012_mapMemBuf(&sh_mem);
	if (!pdev) {
		logger->Fatal("PLAT P2012: Unable to map device descriptor");
		return PLATFORM_INIT_FAILED;
	}
	logger->Debug("PLAT P2012: Device descriptor mapped");

	// Clear the EXCs constraints vector
	ClearExcConstraints();

	return OK;
}

inline const char* P2012PP::_GetPlatformID() {
	if (!strcmp(pdev->descr.name, "STHORM"))
		return PLATFORM_ID;
	return "unknown";
}

P2012PP::ExitCode_t P2012PP::InitResources() {
	ExitCode_t result;
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ResourceAccounter::ExitCode_t ra_result;
	char rsrc_path[MAX_LEN_RPATH_STR];

	// PE fabric quota max (=1 if the maximum number of cluster is available)
	pe_fabric_quota_max =
		pdev->pdesc.clusters_count * pdev->pdesc.pes_count * 100;
	logger->Debug("PLAT P2012: Maximum fabric quota = %d", pe_fabric_quota_max);

	// Cluster level resources
	for (uint16_t i = 0; i < pdev->pdesc.clusters_count; ++i) {
		// L1 memory: Resource path
		snprintf(rsrc_path, MAX_LEN_RPATH_STR, "sys0.acc0.grp%d.mem0", i);
		// L1 memory: Register the resource
		ra_result =	ra.RegisterResource(rsrc_path, "Kb",
				pdev->pdesc.cluster[i].dmem_kb);
		if (ra_result != ResourceAccounter::RA_SUCCESS) {
			logger->Fatal("PLAT P2012: Unable to register '%s'", rsrc_path);
			return PLATFORM_ENUMERATION_FAILED;
		}

		// Processing elements
		for (uint16_t j = 0; j < pdev->pdesc.pes_count; ++j) {
			result = RegisterClusterPE(i, j);
			if (result != OK)
				return PLATFORM_ENUMERATION_FAILED;
		}

		// DMA channels
		for (int z = 0; z < CLUSTER_DMAS_MAX; ++z) {
			result = RegisterClusterDMA(i, z);
			if (result != OK)
				return PLATFORM_ENUMERATION_FAILED;
		}
	}

	return OK;
}

inline P2012PP::ExitCode_t P2012PP::RegisterClusterPE(
		uint8_t cluster_id,
		uint8_t pe_id) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ResourceAccounter::ExitCode_t ra_result;
	char rsrc_path[MAX_LEN_RPATH_STR];
	uint8_t pe_tot;

	// PE: Does it support PE bandwidth quota accounting?
	pdev->descr.caps & DEVICE_RT_CAPABILITIES_RTM_BW ? pe_tot = 100: pe_tot = 1;

	// PE: Resource path
	snprintf(rsrc_path, MAX_LEN_RPATH_STR,
			"sys0.acc0.grp%d.pe%1d", cluster_id, pe_id);

	// PE: Register the resource
	ra_result = ra.RegisterResource(rsrc_path, "", pe_tot);
	if (ra_result != ResourceAccounter::RA_SUCCESS) {
		logger->Fatal("PLAT P2012: Unable to register '%s'", rsrc_path);
		return PLATFORM_ENUMERATION_FAILED;
	}

	return OK;
}

inline P2012PP::ExitCode_t P2012PP::RegisterClusterDMA(
		uint8_t cluster_id,
		uint8_t dma_id) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ResourceAccounter::ExitCode_t ra_result;
	char rsrc_path[MAX_LEN_RPATH_STR];

	// DMA channel: Resource path
	snprintf(rsrc_path, MAX_LEN_RPATH_STR, "sys0.acc0.grp%d.io%d",
			cluster_id, dma_id);

	// DMA channel: Register the resource
	ra_result = ra.RegisterResource(rsrc_path, "Gbps",
			pdev->pdesc.cluster[0].dma[0].bandwidth.max);
	if (ra_result != ResourceAccounter::RA_SUCCESS) {
		logger->Fatal("PLAT P2012: "
				"Unable to register DMA channel: %s", rsrc_path);
		return PLATFORM_ENUMERATION_FAILED;
	}

	return OK;
}

// NOTE:Called inside MapResources() from father class PP
P2012PP::ExitCode_t P2012PP::_Setup(AppPtr_t papp) {
	(void) papp;

	return OK;
}

P2012PP::ExitCode_t P2012PP::_Release(AppPtr_t papp) {
	_ReclaimResources(papp);

	return OK;
}

P2012PP::ExitCode_t P2012PP::_ReclaimResources(AppPtr_t papp) {
	ExitCode_t result = OK;
	int16_t xcs_id;
	logger->Debug("PLAT P2012: Resource reclaiming for [%s]",
			papp->StrId());

	// Retrieve the EXC constraints descriptor and clear
	xcs_id = GetExcConstraints(papp);
	if (xcs_id < 0) {
		logger->Warn("PLAT P2012: "
				"No EXC constraints descriptor for [%s]",
				papp->StrId());
		return PLATFORM_DATA_NOT_FOUND;
	}
	ClearExcConstraints(xcs_id);

	// Decrement the count of EXCs constraints
	pdev->pcons.count--;
	logger->Debug("PLAT P2012: EXC constraints count = %d",
			pdev->pcons.count);

	// Send a notify to the device about the resource reclaiming
	result = NotifyPlatform(P2012_ALL, BBQ_UPDATE, xcs_id);
	if (result != OK)
		logger->Error("PLAT P2012: Unable to notify the platform");

	return result;
}

P2012PP::ExitCode_t P2012PP::_MapResources(AppPtr_t papp,
		UsagesMapPtr_t pusgm,
		RViewToken_t rvt,
		bool excl = true) {
	ExitCode_t result;
	br::UsagesMap_t::iterator uit;
	br::UsagePtr_t pusage;
	PlatformResourceBindingPtr_t pbind(new PlatformResourceBinding_t);
	int16_t xcs_id;

	// Mute compiler warnings
	(void) rvt;
	(void) excl;

	// Get a clean EXC constraints
	xcs_id = InitExcConstraints(papp);
	if (xcs_id < 0) {
		logger->Error("PLAT P2012: "
				"cannot retrieve an EXC constraints descriptor");
		return MAPPING_FAILED;
	}

	// Increment the count of EXCs constraints
	pdev->pcons.count++;
	logger->Debug("PLAT P2012: EXC constraints count = %02d",
			pdev->pcons.count);

	// Current AWM resource usages
	uit = pusgm->begin();
	for (; uit != pusgm->end(); ++uit) {
		ResourcePathPtr_t r_path((*uit).first);
		pusage = (*uit).second;

		// Get cluster information
		(*pbind) = {
			r_path->GetID(ResourceIdentifier::GROUP),
			pusage->GetAmount(),
			r_path->Type()
		};
		logger->Debug("PLAT P2012: Resource [%s] mapped into cluster %d",
				r_path->ToString().c_str(), pbind->cluster_id);

		// Update EXC constraint into the device descriptor
		result = UpdateExcConstraints(papp, xcs_id, pbind);
		if (result != OK) {
			logger->Error("PLAT P2012: "
					"Unable to update assignment [%s] (%llu) to [%s]",
					r_path->ToString().c_str(), pbind->amount, papp->StrId());
			return MAPPING_FAILED;
		}
	}

	return OK;
}

void P2012PP::_Monitor() {
	int p2012_result;
	char buffer[P2012_MSG_SIZE];

	// Init receiving message buffer
	BBQ_message_t * recv_msg =
		(BBQ_message_t *) (buffer + sizeof(P2012_ReceiveMessageHdr_t));

	// Wait for messages from P2012
	p2012_result = p2012_getNextMessage(buffer, P2012_MSG_SIZE);
	if (p2012_result != 0) {
		logger->Error("PLAT P2012: "
				"Errors in receiving message from the platform");
		return;
	}

	// Process the event
	switch (recv_msg->body.type) {

	case P2012_EVENT:
		// ... Manage events here ... //
		logger->Info("PLAT P2012: Events signaled");
		break;

	case P2012_OFFLINE:
		//done = true;
		//trdStatus_cv.notify_one();
		break;

	default:
		logger->Warn("PLAT 2012: "
				"Unexpected message type (%d) received from the platform",
				recv_msg->body.type);
		break;
	}
}

void P2012PP::_Stop() {
	NotifyPlatform(P2012_ALL, BBQ_STOP, 0);
}

int16_t P2012PP::InitExcConstraints(AppPtr_t papp) {
	int16_t xcs_id = -1;

	// Application starting: find the first "free slot" in the EXC
	// constraints descriptors array
	if (papp->Starting()) {
		xcs_id = GetExcConstraintsFree();
	}
	// Application already running: A match should be found
	else if (papp->Synching() && !papp->Blocking()) {
		xcs_id = GetExcConstraints(papp);
	}

	// Null check
	if (xcs_id < 0) {
		logger->Error("PLAT P2012: "
				"EXC constraints descriptors unavailable");
		return xcs_id;
	}

	// Blank previous data
	ClearExcConstraints(xcs_id);

	// Set EXC static information (ID and name)
	// TODO: Solve coherency BBQ/p2012 EXC ID
	pdev->pcons.exc[xcs_id].id = papp->Uid();
	strncpy(pdev->pcons.exc[xcs_id].name, papp->Name().c_str(),
			EXC_NAME_MAX);

	return xcs_id;
}

int16_t P2012PP::GetExcConstraints(AppPtr_t papp) {
	logger->Debug("PLAT P2012: "
			"Getting a EXC constraints descriptor previously allocated");

	// EXC constraints descriptors array
	for (int i = 0; i < EXCS_MAX; ++i) {
		// Application/EXC UID check
		if (pdev->pcons.exc[i].id != papp->Uid())
			continue;

		// Matched!
		return i;
	}
	return -1;
}

int16_t P2012PP::GetExcConstraintsFree() {
	ApplicationManager &am(ApplicationManager::GetInstance());
	AppPtr_t prev_papp;
	logger->Debug("PLAT P2012: Getting a free EXC constraints descriptor");

	// EXC constraints descriptors array
	for (int i = 0; i < EXCS_MAX; ++i) {
		// Free slot?
		if (pdev->pcons.exc[i].id == 0)
			return i;

		// Check if the application previously scheduled still exhists,
		// is going to finish, or has be unscheduled (blocked/disabled).
		// If true, return this as first available
		prev_papp = am.GetApplication(pdev->pcons.exc[i].id);
		if (!prev_papp || prev_papp->Blocking() || prev_papp->Disabled())
			return i;
	}
	return -1;
}

inline void P2012PP::ClearExcConstraints() {
	memset(pdev->pcons.exc, 0, sizeof(ExcConstraints_t) * EXCS_MAX);
	pdev->pcons.count = 0;
}

inline void P2012PP::ClearExcConstraints(int16_t xcs_id) {
	if ((xcs_id < 0) || (xcs_id >= EXCS_MAX))
		return;
	memset(&pdev->pcons.exc[xcs_id], 0, sizeof(ExcConstraints_t));
}

P2012PP::ExitCode_t P2012PP::UpdateExcConstraints(
		AppPtr_t papp,
		int16_t xcs_id,
		PlatformResourceBindingPtr_t pbind) {
	ExitCode_t result = OK;

	// Set the proper descriptor's fields according to the resource type
	switch (pbind->type) {
	case Resource::PROC_ELEMENT:
		// OpenCL constraints: range [0, 10.000] to manage values with two
		// decimals, e.g. 75.20% = 7520
		pdev->pcons.exc[xcs_id].u.ocl.fabric_quota +=
			GetPeFabricQuota(pbind->amount) * 100.0;
		logger->Info("PLAT P2012: %s X[%d] allowed to use %3.2f percent of the fabric",
				papp->StrId(), xcs_id,
				(float) pdev->pcons.exc[xcs_id].u.ocl.fabric_quota / 100.0);
		break;

	case Resource::MEMORY:
		pdev->pcons.exc[xcs_id].u.generic.dmem.L2_KB = pbind->amount / 1024;
		logger->Info("PLAT P2012: %s X[%d] booked %02d Kb from L2 memory",
				papp->StrId(), xcs_id,
				pdev->pcons.exc[xcs_id].u.generic.dmem.L2_KB);
		break;

	case Resource::IO:
		logger->Warn("PLAT P2012: DMA currently unmanaged");
		break;

	default:
		logger->Error("PLAT P2012: Resource type {%s} unmanaged",
				ResourceIdentifier::StringFromType(pbind->type));
		return PLATFORM_DATA_PARSING_ERROR;
	}

	// Send a notify to the device about the update
	result = NotifyPlatform(P2012_ALL, BBQ_UPDATE, xcs_id);
	if (result != OK)
		logger->Error("PLAT P2012: Unable to notify the platform");

	return result;
}

inline uint16_t P2012PP::GetPeFabricQuota(float const & pe_quota) {
	return pe_quota / static_cast<float>(pe_fabric_quota_max) * 100.0;
}

P2012PP::ExitCode_t P2012PP::NotifyPlatform(
		BBQ_p2012_target_t target,
		BBQ_msg_type_t type,
		uint32_t data) {
	int p2012_result;
	char buffer[P2012_MSG_SIZE];
	BBQ_message_t msg;

	// Fill the message
	msg.header.target = target;
	msg.body          = {type, data};
	memcpy(&buffer, &msg, sizeof(msg));

	// Send
	p2012_result = p2012_sendMessage(out_queue_id, &msg, sizeof(msg));
	if (p2012_result != 0) {
		logger->Error("PLAT P2012: Error in sending notification (%s)",
				strerror(p2012_result));
		return PLATFORM_COMM_ERROR;
	}

	return OK;
}

} // namespace bbque
