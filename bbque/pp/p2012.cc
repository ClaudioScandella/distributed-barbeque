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
#include <cstdlib>

#include "bbque/resource_manager.h"
#include "bbque/app/working_mode.h"
#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_path.h"
#include "bbque/res/usage.h"
#include "bbque/pp/p2012.h"

#define MODULE_NAMESPACE PLATFORM_PROXY_NAMESPACE ".sthorm"

namespace bbque {


P2012PP::P2012PP() :
	PlatformProxy(),
	pwr_sample_dfr("pp.pwr_sample", std::bind(&P2012PP::PowerSample, this)),
	out_queue_id(P2012_INVALID_QUEUE_ID),
	in_queue_id(P2012_INVALID_QUEUE_ID) {

	// Init power management information
	power = {
		FABRIC_POWER_FULL_MW,
		DEFAULT_POWER_SAMPLE_T_MS,
		DEFAULT_POWER_CHECK_T_S,
		DEFAULT_POWER_CHECK_T_S * 1000 / DEFAULT_POWER_SAMPLE_T_MS,
		DEFAULT_POWER_GUARD_THR,
		0, 0, 0
	};
	logger->Info("STHORM: Power [B:%d mW, Tp:%d ms, Tc:%d s, #S:%d]",
			power.budget_mw, power.sample_period, power.check_period,
			power.check_samples);

	// Register a command dispatcher
	CommandManager &cm = CommandManager::GetInstance();
	cm.RegisterCommand(MODULE_NAMESPACE ".budget_mw",
			static_cast<CommandHandler*>(this),
			"Set the budget of power consumption for the fabric [mW]");

	cm.RegisterCommand(MODULE_NAMESPACE ".sample_ms",
			static_cast<CommandHandler*>(this),
			"The period of power consumption polling [ms]");

	cm.RegisterCommand(MODULE_NAMESPACE ".check_s",
			static_cast<CommandHandler*>(this),
			"The period of power budget checking [s]");

	cm.RegisterCommand(MODULE_NAMESPACE ".read_mw",
			static_cast<CommandHandler*>(this),
			"FAKE power consumption read [mW]");

	p2012_ts = p2012_mw = 0;
	pwr_sample_ema = pEma_t(new EMA(POWER_EMA_SAMPLES, 0));

	// Instance the power polling deferrable with default period
	pwr_sample_dfr.SetPeriodic(milliseconds(power.sample_period));


	// NOTE: Could we move this at the end of LoadPlatformData?
	SetPilInitialized();
	logger->Info("STHORM: Built Platform Proxy");
}

P2012PP::~P2012PP() {
	int p2012_result;
	logger->Info("STHORM: Destroying Platform Proxy...");

	// Destroy message queues
	p2012_deleteMsgQueue(out_queue_id);
	p2012_deleteMsgQueue(in_queue_id);

	//TODO: Move outside
	_Stop();
	logger->Debug("STHORM: Stop signal sent to platform");

	// Unmap shared memory buffer
	p2012_result = p2012_unmapMemBuf(&sh_mem);
	if (p2012_result) {
		logger->Error("STHORM: Error in unmapping device descriptor (%s)",
				strerror(p2012_result));
		return;
	}
	logger->Debug("STHORM: Bye!");
}

P2012PP::ExitCode_t P2012PP::_LoadPlatformData() {
	ExitCode_t result;
	logger->Info("STHORM: ... Loading platform data ...");

	// Initialise message queues and shared memory buffer (hence the device
	// descriptor)
	result = InitPlatformComm();
	if (result != OK) {
		logger->Fatal("STHORM: Platform initialization failed.");
		return PLATFORM_INIT_FAILED;
	}
	logger->Info("STHORM: Platform initialization performed");

	// Register the resources
	result = InitResources();
	if (result != OK) {
		logger->Fatal("STHORM: Platform enumeration failed.");
		return PLATFORM_ENUMERATION_FAILED;
	}
	logger->Info("STHORM: Platform is ready");

	return OK;
}

P2012PP::ExitCode_t P2012PP::InitPlatformComm() {
	int p2012_result;
	int fabric_addr;

	// Init p2012 user library
	p2012_result = p2012_initUsrLib();
	if (p2012_result != 0) {
		logger->Fatal("STHORM: Initialization failed...");
		return PLATFORM_INIT_FAILED;
	}

	// Create output messages queue
	p2012_result = p2012_createMsgQueue(NULL, P2012_QUEUE_HOST,
			P2012_QUEUE_FC, NULL, &out_queue_id, &fabric_addr);
	if (p2012_result != 0) {
		logger->Fatal("STHORM: Can't create output message queue (%s)",
				strerror(p2012_result));
		return PLATFORM_INIT_FAILED;
	}

	// Create input messages queue
	p2012_result = p2012_createMsgQueue(NULL, P2012_QUEUE_FC,
			P2012_QUEUE_HOST, NULL, &in_queue_id, &fabric_addr);
	if (p2012_result != 0) {
		logger->Fatal("STHORM: Can't create input message queue (%s)",
				strerror(p2012_result));
		return PLATFORM_INIT_FAILED;
	}
	logger->Info("STHORM: Message queues initialized");

	// Initialize the shared memory buffer
	p2012_result = p2012_BBQInit(&sh_mem);
	if (p2012_result != 0) {
		logger->Fatal("STHORM: Driver initialization failed (%s)",
				strerror(p2012_result));
		return PLATFORM_INIT_FAILED;
	}
	logger->Info("STHORM: Driver initialized");

	// Map the memory buffer (device descriptor) into userland
	pdev = (ManagedDevice_t *) p2012_mapMemBuf(&sh_mem);
	if (!pdev) {
		logger->Fatal("STHORM: Unable to map device descriptor");
		return PLATFORM_INIT_FAILED;
	}
	logger->Info("STHORM: Device descriptor mapped in [%x]", pdev);

	// Clear the EXCs constraints vector
	ClearExcConstraints();

	return OK;
}

inline const char* P2012PP::_GetPlatformID() {
	if (!strcmp(pdev->descr.name, PLATFORM_NAME))
		return PLATFORM_ID;
	return "unknown";
}

P2012PP::ExitCode_t P2012PP::InitResources() {
	ExitCode_t result;
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ResourceAccounter::ExitCode_t ra_result;
	char rsrc_path[MAX_LEN_RPATH_STR];


	logger->Info("STHORM: -----------------------------");
	logger->Info("STHORM: Clusters .............. %3d",
			pdev->pdesc.clusters_count);
	logger->Info("STHORM: Processing elements ... %3d",
			pdev->pdesc.pes_count);
	logger->Info("STHORM: DMAs per cluster ...... %3d",
			CLUSTER_DMAS_MAX);
	logger->Info("STHORM: Simultaneous EXCs ..... %3d",
			EXCS_MAX);
	logger->Info("STHORM: TCDM .................. %3dKB",
			pdev->pdesc.cluster[0].dmem_kb);
	logger->Info("STHORM: -----------------------------");

	logger->Debug("STHORM: ExcConstraints @[%x]",
			(uint32_t) &pdev->pcons - (uint32_t) pdev);

	// PE fabric quota max (=1 if the maximum number of cluster is available)
	pe_fabric_quota_max =
		pdev->pdesc.clusters_count * pdev->pdesc.pes_count * 100;
	logger->Debug("STHORM: Maximum fabric quota = %d", pe_fabric_quota_max);

	// Register the max power consumption of the fabric
	power.unreserved = FABRIC_POWER_FULL_MW;
	ra_result = ra.RegisterResource(
			FABRIC_POWER_RESOURCE, "", power.unreserved);

	// Cluster level resources
	for (uint16_t i = 0; i < pdev->pdesc.clusters_count; ++i) {
		// L1 memory: Resource path
		snprintf(rsrc_path, MAX_LEN_RPATH_STR, "sys0.acc0.grp%d.mem0", i);

		logger->Debug("STHORM: C[%d] TCDM mem = %-3d Kb",
				i, pdev->pdesc.cluster[i].dmem_kb);

		// L1 memory: Register the resource
		ra_result =	ra.RegisterResource(rsrc_path, "Kb",
				pdev->pdesc.cluster[i].dmem_kb);
		if (ra_result != ResourceAccounter::RA_SUCCESS) {
			logger->Fatal("STHORM: Unable to register '%s'", rsrc_path);
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
		logger->Fatal("STHORM: Unable to register '%s'", rsrc_path);
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
		logger->Fatal("STHORM: Unable to register DMA channel: %s", rsrc_path);
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
	logger->Debug("STHORM: Resource reclaiming for [%s]",
			papp->StrId());

	// Retrieve the EXC constraints descriptor and clear
	xcs_id = GetExcConstraints(papp);
	if (xcs_id < 0) {
		logger->Warn("STHORM: "
				"No EXC constraints descriptor for [%s]",
				papp->StrId());
		return PLATFORM_DATA_NOT_FOUND;
	}
	ClearExcConstraints(xcs_id);

	// Decrement the count of EXCs constraints
	pdev->pcons.count--;
	logger->Debug("STHORM: EXC constraints count = %d",
			pdev->pcons.count);

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
		logger->Error("STHORM: "
				"cannot retrieve an EXC constraints descriptor");
		return MAPPING_FAILED;
	}

	// Increment the count of EXCs constraints
	pdev->pcons.count++;
	logger->Debug("STHORM: EXC constraints count = %02d",
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
		if (pbind->cluster_id != R_ID_NONE) {
			logger->Debug("STHORM: Resource [%s] mapped into cluster %d",
					r_path->ToString().c_str(), pbind->cluster_id);
		}

		// Update EXC constraint into the device descriptor
		result = UpdateExcConstraints(papp, xcs_id, pbind);
		if (result != OK) {
			logger->Error("STHORM: Unable to update assignment [%s]" PRIu64	"to [%s]",
					r_path->ToString().c_str(), pbind->amount, papp->StrId());
			return MAPPING_FAILED;
		}
	}

	return OK;
}

void P2012PP::_Stop() {
}

void P2012PP::Monitor() {
	// TODO: we should switch to the poll interface as soon as it is
	// available
	logger->Info("STHORM: waiting for platform events...");
#if 0
	p2012_result = p2012_getNextMessage(buffer, P2012_MSG_SIZE);
	if (p2012_result != 0) {
		logger->Error("STHORM: waiting platform event FAILED! "
				"(Error: get next message failure)");
		::usleep(500000);
		return;
	}
#else
# warning "P2012: notification disabled"
	Wait();
#endif
}

void P2012PP::Task() {
	logger->Info("STHORM: Monitoring thread STARTED");
	while (!done) {
		Monitor();
	}
	logger->Info("STHORM: Monitoring thread ENDED");
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
		logger->Error("STHORM: "
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
	logger->Debug("STHORM: "
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
	logger->Debug("STHORM: Getting a free EXC constraints descriptor");

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
	logger->Debug("STHORM: Update: Resource type '%s'",
			ResourceIdentifier::StringFromType(pbind->type));
	switch (pbind->type) {
	case Resource::PROC_ELEMENT:
		// OpenCL constraints: range [0, 10.000] to manage values with two
		// decimals, e.g. 75.20% = 7520
		pdev->pcons.exc[xcs_id].u.ocl.fabric_quota +=
			GetPeFabricQuota(pbind->amount) * 100.0;
		logger->Info("STHORM: %s X[%d] allowed to use %3.2f %% of the fabric",
				papp->StrId(), xcs_id,
				(float) pdev->pcons.exc[xcs_id].u.ocl.fabric_quota / 100.0);
		break;

	case Resource::MEMORY:
		pdev->pcons.exc[xcs_id].u.generic.dmem.L2_KB = pbind->amount / 1024;
		logger->Info("STHORM: %s X[%d] booked %02d Kb from L2 memory",
				papp->StrId(), xcs_id,
				pdev->pcons.exc[xcs_id].u.generic.dmem.L2_KB);
		break;

	case Resource::IO:
		logger->Warn("STHORM: DMA currently unmanaged");
		break;

	default:
		logger->Debug("STHORM: No control implemented for resource '%s'",
				pbind->type);
	}

	return result;
}

inline uint16_t P2012PP::GetPeFabricQuota(float const & pe_quota) {
	return pe_quota / static_cast<float>(pe_fabric_quota_max) * 100.0;
}

void P2012PP::PowerSample() {

	// Is there an update value?
	if (p2012_ts == power.curr_ts)
		return;

	// Yes: Read current power consumption and update EMA
	power.curr_ts = p2012_ts;
	power.count_s++;
	pwr_sample_ema->update(p2012_mw);
	logger->Info("STHORM: Power consumption sample: %d mW [ts:%d #S:%d]",
			p2012_mw, p2012_ts, power.count_s);
	logger->Info("PWR_STATS: %d %4.0f %d %d",
			p2012_mw,
			pwr_sample_ema->get(),
			power.budget_mw,
			power.unreserved);

	// Number of samples reached?
	if (power.count_s < power.check_samples)
		return;

	// Get the EMA value of power consumption, reset the count
	power.curr_mw = pwr_sample_ema->get();
	power.count_s = 0;
	logger->Info("STHORM: Call power policy [EMA: %d mW]", power.curr_mw);

	// Call the power management policy
	PowerPolicy();
}

void P2012PP::PowerPolicy() {
	ResourceManager   & rm(ResourceManager::GetInstance());
	ResourceAccounter & ra(ResourceAccounter::GetInstance());
	ResourceAccounter::ExitCode_t ra_result;
	int32_t  budget_new;
	int32_t  budget_diff;
	uint32_t consumption;

	// Consumption (+ guard threshold)
	consumption = power.curr_mw +
			(power.curr_mw * static_cast<float>(power.guard_margin) / 100);
	// Check the budget
	budget_diff = power.budget_mw - consumption;
	if ((budget_diff >= 0) &&
		(power.budget_mw <= power.unreserved)) {
		logger->Info("STHORM: Power budget OK [B:%d mW  D:%d mW]",
				power.budget_mw, budget_diff);
		return;
	}
	// Power budget overpassed => budget_diff negative)
	if (budget_diff < 0) {
		logger->Warn("STHORM: Power budget overpassed [B:%d mW  D:%d mW]",
				power.budget_mw, budget_diff);
	}

	// Change the amount of power resource
	budget_new = power.unreserved + budget_diff;
	budget_new = std::max<int32_t>(budget_new, FABRIC_POWER_IDLE_MW);
	budget_new = std::min<int32_t>(budget_new, FABRIC_POWER_FULL_MW);
	if (static_cast<uint64_t>(budget_new) == power.unreserved) {
		logger->Debug("STHORM: No need to update power resource (BN:%d mW)",
				budget_new);
		return;
	}

	// Update the total amount of power resource (unreserved)
	ra_result  = ra.UpdateResource(FABRIC_POWER_RESOURCE, "", budget_new);
	if (ra_result == ResourceAccounter::RA_SUCCESS) {
		power.unreserved = ra.Unreserved(FABRIC_POWER_RESOURCE);
		logger->Warn("STHORM: [%s] updated to % " PRIu64 " mW",
				FABRIC_POWER_RESOURCE, power.unreserved);
	}

	// Trigger a new optimization
	rm.NotifyEvent(ResourceManager::BBQ_OPTS);
}

void P2012PP::PowerConfig(PowerSetting_t pwr_sett, uint32_t value) {
	// Select the setting to configure
	switch (pwr_sett) {
	// Config sampling period
	case BUDGET_MW:
		power.budget_mw = value;
		break;
	// Config sampling period
	case SAMPLING_PERIOD:
		if (value > power.check_period * 1000)
			return;
		power.sample_period = value;
		power.check_samples = power.check_period * 1000 / power.sample_period;
		break;
	// Config checking/policy period
	case CHECKING_PERIOD:
		if (value < power.sample_period / 1000)
			return;
		power.check_period  = value;
		power.check_samples = power.check_period * 1000 / power.sample_period;
		break;
	// Config guard margin on the power consumption read
	case GUARD_MARGIN:
		power.guard_margin = value;
		break;
	}
}

int P2012PP::CommandsCb(int argc, char *argv[]) {
	uint8_t cmd_offset = ::strlen(MODULE_NAMESPACE) + 1;

	logger->Debug("STHORM: Processing command [%s]", argv[0] + cmd_offset);
	switch (argv[0][cmd_offset]) {
	// Power budget
	case 'b':
		if ((atoi(argv[1]) <= FABRIC_POWER_FULL_MW) &&
			(atoi(argv[1]) >= FABRIC_POWER_IDLE_MW)) {
			PowerConfig(BUDGET_MW, atoi(argv[1]));
			logger->Info("STHORM: Power budget set to %d mW", power.budget_mw);
		}
		else
			logger->Warn("STHORM: Power budget (%d mW) out of range [%d, %d] mW",
					atoi(argv[1]),FABRIC_POWER_IDLE_MW, FABRIC_POWER_FULL_MW);
		break;
	// Polling period
	case 's':
		PowerConfig(SAMPLING_PERIOD, atoi(argv[1]));
		logger->Info("STHORM: Power polling period set to %d ms [#S:%d]",
				power.sample_period, power.check_samples);
		pwr_sample_dfr.SetPeriodic(milliseconds(power.sample_period));
		break;
	// Checking period
	case 'c':
		PowerConfig(CHECKING_PERIOD, atoi(argv[1]));
		logger->Info("STHORM: Power checking period set to %d s [#S:%d]",
				power.check_period, power.check_samples);
		break;
	// Fake power consumption read
	case 'r':
		p2012_mw = atoi(argv[1]);
		p2012_ts = atoi(argv[2]);
		break;

	default:
		logger->Warn("STHORM: Unknown command [%s], ignored...", argv[0]);
	}

	return 0;
}


} // namespace bbque
