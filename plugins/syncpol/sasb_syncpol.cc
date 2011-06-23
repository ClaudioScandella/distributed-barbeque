/**
 *       @file  sasb_syncpol.cc
 *      @brief  The SASB synchronization policy
 *
 * This defines a dynamic C++ plugin which implements the "Starvation Avoidance
 * State Based" (SASB) heuristic for EXCc synchronizaiton.
 *
 *     @author  Patrick Bellasi (derkling), derkling@google.com
 *
 *   @internal
 *     Created  01/28/2011
 *    Revision  $Id: doxygen.templates,v 1.3 2010/07/06 09:20:12 mehner Exp $
 *    Compiler  gcc/g++
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 2011, Patrick Bellasi
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =============================================================================
 */

#include "sasb_syncpol.h"

#include "bbque/modules_factory.h"

#include <iostream>
#include <random>

namespace bbque { namespace plugins {

SasbSyncPol::SasbSyncPol() :
	status(STEP10) {

	// Get a logger
	plugins::LoggerIF::Configuration conf(
			SYNCHRONIZATION_POLICY_NAMESPACE
			SYNCHRONIZATION_POLICY_NAME);
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	if (!logger) {
		std::cout << "SASB: Build sasb synchronization policy "
			<< this << "] FAILED (Error: missing logger module)" << std::endl;
		assert(logger);
	}

	logger->Debug("Built a new dynamic object[%p]\n", this);

}

SasbSyncPol::~SasbSyncPol() {
}

//----- Scheduler policy module interface

char const *SasbSyncPol::Name() {
	return SYNCHRONIZATION_POLICY_NAME;
}


bbque::AppsUidMap_t const *SasbSyncPol::step1(
			bbque::SystemView const & sv) {
	AppsUidMap_t const *apps;

	logger->Debug("STEP 1.0: Running => Blocked");
	apps = sv.Applications(ApplicationStatusIF::BLOCKED);

	assert(apps);
	if (!apps->empty())
		return apps;

	logger->Debug("STEP 1.0:            "
			"No EXCs to be BLOCKED");
	return NULL;
}

bbque::AppsUidMap_t const *SasbSyncPol::step2(
			bbque::SystemView const & sv) {
	AppsUidMap_t const *apps;

	switch(status) {
	case STEP21:
		logger->Debug("STEP 2.1: Running => Migration (lower prio)");
		apps = sv.Applications(ApplicationStatusIF::MIGRATE);
		break;

	case STEP22:
		logger->Debug("STEP 2.2: Running => Migration/Reconf (lower prio)");
		apps = sv.Applications(ApplicationStatusIF::MIGREC);
		break;

	case STEP23:
		logger->Debug("STEP 2.3: Running => Reconf (lower prio)");
		apps = sv.Applications(ApplicationStatusIF::RECONF);
		break;

	default:
		// We should never got here
		assert(false);
	}

	assert(apps);
	if (!apps->empty())
		return apps;

	logger->Debug("STEP 2.0:            "
			"No EXCs to be reschedule (lower prio)");
	return NULL;
}


bbque::AppsUidMap_t const *SasbSyncPol::step3(
			bbque::SystemView const & sv) {
	AppsUidMap_t const *apps;

	switch(status) {
	case STEP31:
		logger->Debug("STEP 3.1: Running => Migration (higher prio)");
		apps = sv.Applications(ApplicationStatusIF::MIGRATE);
		break;

	case STEP32:
		logger->Debug("STEP 3.2: Running => Migration/Reconf (higher prio)");
		apps = sv.Applications(ApplicationStatusIF::MIGREC);
		break;

	case STEP33:
		logger->Debug("STEP 3.3: Running => Reconf (higher prio)");
		apps = sv.Applications(ApplicationStatusIF::RECONF);
		break;

	default:
		// We should never got here
		assert(false);
	}

	assert(apps);
	if (!apps->empty())
		return apps;

	logger->Debug("STEP 3.0:            "
			"No EXCs to be reschedule (higher prio)");
	return NULL;
}

bbque::AppsUidMap_t const *SasbSyncPol::step4(
			bbque::SystemView const & sv) {
	AppsUidMap_t const *apps;

	logger->Debug("STEP 4.0: Ready   => Running");
	apps = sv.Applications(ApplicationStatusIF::STARTING);

	assert(apps);
	if (!apps->empty())
		return apps;

	logger->Debug("STEP 4.0:            "
			"No EXCs to be started");
	return NULL;
}

bbque::AppsUidMap_t const *SasbSyncPol::GetApplicationsQueue(
			bbque::SystemView const & sv, bool restart) {
	bbque::AppsUidMap_t const *map;

	if (restart) {
		logger->Debug("Resetting sync status");
		status = STEP10;
	}

	for( ; status<=STEP40; ++status) {
		switch(status) {
		case STEP10:
			map = step1(sv);
			if (map)
				return map;
			break;
		case STEP21:
		case STEP22:
		case STEP23:
			map = step2(sv);
			if (map)
				return map;
			break;
		case STEP31:
		case STEP32:
		case STEP33:
			map = step3(sv);
			if (map)
				return map;
			break;
		case STEP40:
			map = step4(sv);
			if (map)
				return map;
			break;
		};
	}

	return NULL;

}

bool SasbSyncPol::DoSync(AppPtr_t papp) {
	(void)papp;
	return true;
}

//----- static plugin interface

void * SasbSyncPol::Create(PF_ObjectParams *) {
	return new SasbSyncPol();
}

int32_t SasbSyncPol::Destroy(void * plugin) {
  if (!plugin)
    return -1;
  delete (SasbSyncPol *)plugin;
  return 0;
}

} // namesapce plugins

} // namespace bbque

