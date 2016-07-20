#include "bbque/platform_manager.h"
#include "bbque/pp/local_platform_proxy.h"
#include "bbque/pp/remote_platform_proxy.h"
#include "bbque/res/binder.h"
#include "bbque/res/resource_utils.h"
#include "bbque/resource_manager.h"

namespace bbque
{

PlatformManager::PlatformManager()
{
	// Get a logger module
	logger = bu::Logger::GetLogger(PLATFORM_MANAGER_NAMESPACE);
	assert(logger);

	try {
		// Init the submodules
		this->lpp = std::unique_ptr<pp::LocalPlatformProxy>(
		                    new pp::LocalPlatformProxy());
		this->rpp = std::unique_ptr<pp::RemotePlatformProxy>(
		                    new pp::RemotePlatformProxy());
	} catch(const std::runtime_error & r) {
		logger->Fatal("Unable to setup some PlatformProxy: %s", r.what());
		return;
	}

	// Register a command dispatcher to handle CGroups reconfiguration
	CommandManager & cm = CommandManager::GetInstance();
	cm.RegisterCommand(PLATFORM_MANAGER_NAMESPACE ".refresh",
	                   static_cast<CommandHandler *>(this),
	                   "Refresh CGroups resources description");

	Worker::Setup(BBQUE_MODULE_NAME("plm"), PLATFORM_MANAGER_NAMESPACE);
}

PlatformManager::~PlatformManager()
{
	// Nothing to do
}

PlatformManager & PlatformManager::GetInstance()
{
	static PlatformManager plm; // Guranteed to be destroyed
	return plm;
}


PlatformManager::ExitCode_t PlatformManager::LoadPlatformConfig()
{
	try {
		(void) this->GetPlatformDescription();
	} catch(const std::runtime_error & err) {
		logger->Error("%s", err.what());
		return PLATFORM_DATA_PARSING_ERROR;
	}

	return PLATFORM_OK;
}

void PlatformManager::Task()
{

	logger->Info("Platform Manager monitoring thread STARTED");

	while (true) {
		if (platformEvents.none()) {
			Wait();
		}

		// Refresh available resources
		if (platformEvents.test(PLATFORM_MANAGER_EV_REFRESH)) {
			ResourceAccounter & ra(ResourceAccounter::GetInstance());

			// Set that the platform is NOT ready
			ra.SetPlatformNotReady();

			logger->Info("Platform Manager refresh event propagating to proxies");
			ExitCode_t ec;

			ec = this->lpp->Refresh();
			if (unlikely(ec != PLATFORM_OK)) {
				logger->Error("Error %i trying to refresh LOCAL platform data", ec);
				ra.SetPlatformReady();
				return;
			}
			ec = this->rpp->Refresh();
			if (unlikely(ec != PLATFORM_OK)) {
				logger->Error("Error %i trying to refresh REMOTE platform data", ec);
				ra.SetPlatformReady();
				return;
			}

			// Ok refresh successully

			// The platform is now ready
			ra.SetPlatformReady();

			// Reset for next event
			platformEvents.reset(PLATFORM_MANAGER_EV_REFRESH);

			// Notify a scheduling event to the ResourceManager
			ResourceManager & rm = ResourceManager::GetInstance();
			rm.NotifyEvent(ResourceManager::BBQ_PLAT);
		}
	}

	logger->Info("Platform Manager monitoring thread END");

}

const char * PlatformManager::GetPlatformID(int16_t system_id) const
{
	logger->Debug("Request a Platform ID for system %i", system_id);

	assert(system_id >= -1);

	if (system_id == -1) {
		// The local one
		return lpp->GetPlatformID();
	} else {
		const auto systems = this->GetPlatformDescription().GetSystemsAll();
		if (systems.at(system_id).IsLocal()) {
			return lpp->GetPlatformID();
		}
		return rpp->GetPlatformID(system_id);
	}
}

const char * PlatformManager::GetHardwareID(int16_t system_id) const
{
	logger->Debug("Request a Hardware ID for system %i", system_id);

	assert(system_id >= -1);

	if (system_id == -1) {
		// The local one
		return lpp->GetHardwareID();
	} else {
		const auto systems = this->GetPlatformDescription().GetSystemsAll();
		if (systems.at(system_id).IsLocal()) {
			return lpp->GetHardwareID();
		}

		return rpp->GetHardwareID(system_id);
	}
}

PlatformManager::ExitCode_t PlatformManager::Setup(AppPtr_t papp)
{
	logger->Error("Setup called at top-level");
	// Not implemented at top-level.
	(void) papp;   // Anti-warning
	return PLATFORM_GENERIC_ERROR;
}

PlatformManager::ExitCode_t PlatformManager::LoadPlatformData()
{
	if(platforms_initialized) {
		logger->Warn("Double call to LoadPlatformData, ignoring...");
		return PLATFORM_OK;
	}

	ExitCode_t ec;

	logger->Debug("Loading LOCAL platform data...");
	ec = this->lpp->LoadPlatformData();

	if (unlikely(ec != PLATFORM_OK)) {
		logger->Error("Error %i trying to load LOCAL platform data", ec);

		return ec;
	}

	logger->Debug("Loading REMOTE platform data...");
	ec = this->rpp->LoadPlatformData();

	if (unlikely(ec != PLATFORM_OK)) {
		logger->Error("Error %i trying to load REMOTE platform data", ec);

		return ec;
	}

	logger->Info("All PlatformProxy load platform data successfully");

	ResourceAccounter & ra(ResourceAccounter::GetInstance());
	ra.SetPlatformReady();
	ra.PrintStatusReport(0, true);

	return PLATFORM_OK;

}

PlatformManager::ExitCode_t PlatformManager::Refresh()
{
	std::lock_guard<std::mutex> worker_status_ul(worker_status_mtx);
	// Notify the platform monitoring thread about a new event ot be
	// processed
	platformEvents.set(PLATFORM_MANAGER_EV_REFRESH);
	worker_status_cv.notify_one();

	return PLATFORM_OK;
}

PlatformManager::ExitCode_t PlatformManager::Release(AppPtr_t papp)
{
	assert(papp->HasPlatformData());
	assert(papp->IsLocal() || papp->IsRemote());

	ExitCode_t ec;

	if (papp->IsLocal()) {
		ec = lpp->Release(papp);
		if (unlikely(ec != PLATFORM_OK)) {
			logger->Error("Failed to release LOCAL data of application [%s:%d]"
			              "(error code: %i)",
			              papp->Name().c_str(), papp->Pid(), ec);
			return ec;
		}
	}

	if (papp->IsRemote()) {
		ec = rpp->Release(papp);
		if (unlikely(ec != PLATFORM_OK)) {
			logger->Error("Failed to release REMOTE data of application [%s:%d]"
			              "(error code: %i)",
			              papp->Name().c_str(), papp->Pid(), ec);
			return ec;
		}
	}

	return PLATFORM_OK;

}

PlatformManager::ExitCode_t PlatformManager::ReclaimResources(AppPtr_t papp)
{
	assert(papp->HasPlatformData());
	assert(papp->IsLocal() || papp->IsRemote());

	ExitCode_t ec;

	if (papp->IsLocal()) {
		ec = lpp->ReclaimResources(papp);
		if (unlikely(ec != PLATFORM_OK)) {
			logger->Error("Failed to ReclaimResources LOCAL of application [%s:%d]"
			              "(error code: %i)",
			              papp->Name().c_str(), papp->Pid(), ec);
			return ec;
		}

		// The application now it is not local.
		papp->SetLocal(false);
	}

	if (papp->IsRemote()) {
		ec = rpp->ReclaimResources(papp);
		if (unlikely(ec != PLATFORM_OK)) {
			logger->Error("Failed to ReclaimResources REMOTE of application [%s:%d]"
			              "(error code: %i)",
			              papp->Name().c_str(), papp->Pid(), ec);
			return ec;
		}

		// The application now it is not remote.
		papp->SetRemote(false);
	}

	return PLATFORM_OK;
}

PlatformManager::ExitCode_t PlatformManager::MapResources(
        AppPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl)
{

	ExitCode_t ec;
	ResourceAccounter & ra(ResourceAccounter::GetInstance());
	RViewToken_t rvt = ra.GetScheduledView();
	logger->Debug("Mapping resources for app [%s], using view [%d]", papp->StrId(), rvt);

	// We have to know if the application is local or remote or both.
	// NOTE: the application is considered local/remote at the start
	//       of it and changed only to add a new state local/remote.
	//       this means that if an application was intially scheduled
	//       in Sys1, Sys2 (so, it's remote application), then it will
	//       be scheduled in Sys0 (local) it becomes also local. If
	//       subsequently becoms full local (so remove Sys1 and Sys2 from
	//       scheduling, the application DOES NOT change the 'remote' flag.
	//
	//       This is necessary because we have to inform the PlatformProxy
	//       even if they do not still manage the application.

	// Get the set of assigned (bound) Systems
	br::ResourceBitset systems(br::ResourceBinder::GetMask(
	                                   pres, br::ResourceType::SYSTEM));
	logger->Debug("Mapping: Resources binding includes %d systems", systems.Count());

	bool is_local  = false;
	bool is_remote = false;

	// Check if application is local or remote.
	for (int i = 0; i < systems.Count(); i++) {
		if (systems.Test(i)) {
			logger->Debug("Mapping: Checking system %d...", i);
			if (GetPlatformDescription().GetSystemsAll()[i].IsLocal() ) {
				is_local  = true;
				logger->Debug("Mapping: System %d is local", i);
			} else {
				is_remote = true;
				logger->Debug("Mapping: System %d is remote", i);
			}
		}
	}

	// yes, obviously we need at least one type of resource
	assert( is_local || is_remote );

	// If the application was previously mapped, it means that it must have
	// platform data loaded
	assert( !(papp->IsRemote() || papp->IsLocal()) || papp->HasPlatformData() );

	// If first time scheduled locally, we have to setup it
	if(is_local != papp->IsLocal()) {
		logger->Debug("Mapping: Application [%s] is local, call LPP Setup", papp->StrId());

		ec = lpp->Setup(papp);
		if (ec == PLATFORM_OK) {
			papp->SetLocal(true);
		} else {
			logger->Error("Mapping: Application [%s] FAILED to setup locally "
			              "(error code: %d)", papp->StrId(), ec);
			return ec;
		}
	}

	if(is_remote != papp->IsRemote()) {
		logger->Debug("Mapping: Application [%s] is remote, call RPP Setup",
		              papp->StrId());

		ec = rpp->Setup(papp);
		if (ec == PLATFORM_OK) {
			papp->SetRemote(true);
		} else {
			logger->Error("Mapping: Application [%s] FAILED to setup remotely "
			              "(error code: %d)", papp->StrId(), ec);
			return ec;
		}
	}

	if(!papp->HasPlatformData()) {
		// At least local or remote was called, so the application
		// platform data is initialized, mark it!
		papp->SetPlatformData();
	}

	// At this time we can actually map the resources
	if (papp->IsLocal()) {
		ec = lpp->MapResources(papp, pres, excl);;
		if (unlikely(ec != PLATFORM_OK)) {
			logger->Error("Mapping: Failed to MapResources LOCAL of application [%s]"
			              "(error code: %i)", papp->StrId(), ec);
			return ec;
		}
	}

	if (papp->IsRemote()) {
		ec = rpp->MapResources(papp, pres, excl);
		if (unlikely(ec != PLATFORM_OK)) {
			logger->Error("Failed to MapResources REMOTE of application [%s]"
			              "(error code: %i)", papp->StrId(), ec);
			return ec;
		}
	}

	return PLATFORM_OK;
}

int PlatformManager::CommandsCb(int argc, char * argv[])
{
	uint8_t cmd_offset = ::strlen(PLATFORM_MANAGER_NAMESPACE) + 1;
	(void)argc;
	(void)argv;

	// Notify all the PlatformProxy to refresh the platform description
	switch(argv[0][cmd_offset]) {
	case 'r': // refresh
		this->Refresh();
		break;
	default:
		logger->Warn("CommandsCb: Command [%s] not supported");
	}

	return 0;
}

} // namespace bbque
