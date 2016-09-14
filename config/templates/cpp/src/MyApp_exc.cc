/**
 *       @file  MyApp_exc.cc
 *      @brief  The MyApp BarbequeRTRM application
 *
 * Description: to be done...
 *
 *     @author  Name Surname (nickname), your@email.com
 *
 *     Company  Your Company
 *   Copyright  Copyright (c) 20XX, Name Surname
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =====================================================================================
 */


#include "MyApp_exc.h"

#include <cstdio>
#include <bbque/utils/utility.h>

MyApp::MyApp(std::string const & name,
		std::string const & recipe,
		RTLIB_Services_t *rtlib) :
	BbqueEXC(name, recipe, rtlib) {

	logger->Warn("New MyApp::MyApp()");

	// NOTE: since RTLib 1.1 the derived class construct should be used
	// mainly to specify instantiation parameters. All resource
	// acquisition, especially threads creation, should be palced into the
	// new onSetup() method.

	logger->Info("EXC Unique IDentifier (UID): %u", GetUniqueID());

}

RTLIB_ExitCode_t MyApp::onSetup() {
	// This is just an empty method in the current implementation of this
	// testing application. However, this is intended to collect all the
	// application specific initialization code, especially the code which
	// acquire system resources (e.g. thread creation)
	logger->Warn("MyApp::onSetup()");

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onConfigure(int8_t awm_id) {

	logger->Warn("MyApp::onConfigure(): EXC [%s] => AWM [%02d]",
		exc_name.c_str(), awm_id);

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onRun() {
	RTLIB_WorkingModeParams_t const wmp = WorkingModeParams();

	// Return after 5 cycles
	if (Cycles() >= 5)
		return RTLIB_EXC_WORKLOAD_NONE;

	// Do one more cycle
	logger->Warn("MyApp::onRun()      : EXC [%s]  @ AWM [%02d]",
		exc_name.c_str(), wmp.awm_id);

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onMonitor() {
	RTLIB_WorkingModeParams_t const wmp = WorkingModeParams();

	logger->Warn("MyApp::onMonitor()  : EXC [%s]  @ AWM [%02d], Cycle [%4d]",
		exc_name.c_str(), wmp.awm_id, Cycles());

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onRelease() {

	logger->Warn("MyApp::onRelease()  : exit");

	return RTLIB_OK;
}
