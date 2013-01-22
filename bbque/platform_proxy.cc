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

#include "bbque/platform_proxy.h"

#include "bbque/utils/utility.h"
#include "bbque/modules_factory.h"

#ifdef CONFIG_BBQUE_TEST_PLATFORM_DATA
# warning Using Test Platform Data (TPD)
# define PLATFORM_PROXY PlatformProxy // Use the base class when TPD in use
#else // CONFIG_BBQUE_TEST_PLATFORM_DATA
# ifdef CONFIG_TARGET_LINUX
#  include "bbque/pp/linux.h"
#  define  PLATFORM_PROXY LinuxPP
# endif
# ifdef CONFIG_TARGET_P2012
#  include "bbque/pp/p2012.h"
#  define  PLATFORM_PROXY P2012PP
# endif
#endif // CONFIG_BBQUE_TEST_PLATFORM_DATA

#define PLATFORM_PROXY_NAMESPACE "bq.pp"
#define MODULE_NAMESPACE PLATFORM_PROXY_NAMESPACE

namespace br = bbque::res;

namespace bbque {

PlatformProxy::PlatformProxy() : Worker(),
	pilInitialized(false),
	platformIdentifier(NULL) {

	//---------- Setup Worker
	Worker::Setup(BBQUE_MODULE_NAME("pp"), PLATFORM_PROXY_NAMESPACE);

#ifdef CONFIG_BBQUE_TEST_PLATFORM_DATA
	// Mark the Platform Integration Layer (PIL) as initialized
	SetPilInitialized();
#endif // !CONFIG_BBQUE_TEST_PLATFORM_DATA

}

PlatformProxy::~PlatformProxy() {
}

PlatformProxy & PlatformProxy::GetInstance() {
	static PLATFORM_PROXY instance;
	return instance;
}

void PlatformProxy::Task() {
#ifndef CONFIG_BBQUE_TEST_PLATFORM_DATA
	std::unique_lock<std::mutex> worker_status_ul(worker_status_mtx);

	logger->Info("PLAT PRX: Monitoring thread STARTED");

	while (!done) {

		// Waiting for an event from the platform
		worker_status_cv.wait(worker_status_ul);

		logger->Info("PLAT PRX: Processing platform event");

		// TODO add plataform parsing code

	}

	logger->Info("PLAT PRX: Monitoring thread ENDED");
#else
	logger->Info("PLAT PRX: Terminating monitoring thread (TPD in use)");
	return;
#endif

}


PlatformProxy::ExitCode_t
PlatformProxy::LoadPlatformData() {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	ExitCode_t result = OK;

	// Return if the PIL has not been properly initialized
	if (!pilInitialized) {
		logger->Fatal("PLAT PRX: Platform Integration Layer initialization FAILED");
		return PLATFORM_INIT_FAILED;
	}

	// Platform specific resources enumeration
	logger->Debug("PLAT PRX: loading platform data");
	result = _LoadPlatformData();
	if (unlikely(result != OK)) {
		logger->Fatal("PLAT PRX: Platform [%s] initialization FAILED",
				GetPlatformID());
		return result;
	}

	// Setup the Platform Specific ID
	platformIdentifier = _GetPlatformID();

	logger->Notice("PLAT PRX: Platform [%s] initialization COMPLETED",
			GetPlatformID());

	// Dump status of registered resource
	ra.PrintStatusReport(0, true);

	return result;
}

PlatformProxy::ExitCode_t
PlatformProxy::Setup(AppPtr_t papp) {
	ExitCode_t result = OK;

	logger->Debug("PLAT PRX: platform setup for run-time control "
			"of app [%s]", papp->StrId());
	result = _Setup(papp);
	return result;
}

PlatformProxy::ExitCode_t
PlatformProxy::Release(AppPtr_t papp) {
	ExitCode_t result = OK;

	logger->Debug("PLAT PRX: releasing platform-specific run-time control "
			"for app [%s]", papp->StrId());
	result = _Release(papp);
	return result;
}

PlatformProxy::ExitCode_t
PlatformProxy::ReclaimResources(AppPtr_t papp) {
	ExitCode_t result = OK;

	logger->Debug("PLAT PRX: Reclaiming resources of app [%s]", papp->StrId());
	result = _ReclaimResources(papp);
	return result;
}

PlatformProxy::ExitCode_t
PlatformProxy::MapResources(AppPtr_t papp, UsagesMapPtr_t pres, bool excl) {
	ResourceAccounter &ra = ResourceAccounter::GetInstance();
	RViewToken_t rvt = ra.GetScheduledView();
	ExitCode_t result = OK;

	logger->Debug("PLAT PRX: Mapping resources for app [%s], using view [%d]",
			papp->StrId(), rvt);

	// Platform Specific Data (PSD) should be initialized the first time
	// an application is scheduled for execution
	if (unlikely(!papp->HasPlatformData())) {

		// Setup PSD
		result = Setup(papp);
		if (result != OK) {
			logger->Error("Setup PSD for EXC [%s] FAILED",
					papp->StrId());
			return result;
		}

		// Mark PSD as correctly initialized
		papp->SetPlatformData();
	}

	// Map resources
	result = _MapResources(papp, pres, rvt, excl);

	return result;
}

} /* bbque */
