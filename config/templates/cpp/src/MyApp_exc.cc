/**
 *       @file  MyApp_exc.cc
 *      @brief  The MyApp BarbequeRTRM application
 *
 * Description: to be done...
 *
 *     @author  Name Surname (nickname), your@email.com
 *
 *     Company  Politecnico di Milano
 *   Copyright  Copyright (c) 20XX, Name Surname
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =====================================================================================
 */


#include "MyApp_exc.h"

#include <cstdio>
#include <bbque/utils/utility.h>

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "aem.myapp"
#undef  BBQUE_LOG_UID
#define BBQUE_LOG_UID GetChUid()

MyApp::MyApp(std::string const & name,
		std::string const & recipe,
		RTLIB_Services_t *rtlib) :
	BbqueEXC(name, recipe, rtlib) {

	fprintf(stderr, FW("New MyApp::MyApp()\n"));

	// NOTE: since RTLib 1.1 the derived class construct should be used
	// mainly to specify instantiation parameters. All resource
	// acquisition, especially threads creation, should be palced into the
	// new onSetup() method.

	fprintf(stderr, FI("EXC Unique IDentifier (UID): %u\n"), GetUid());

}

RTLIB_ExitCode_t MyApp::onSetup() {
	// This is just an empty method in the current implementation of this
	// testing application. However, this is intended to collect all the
	// application specific initialization code, especially the code which
	// acquire system resources (e.g. thread creation)
	fprintf(stderr, FW("MyApp::onSetup()\n"));

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onConfigure(uint8_t awm_id) {

	fprintf(stderr, FW("MyApp::onConfigure(): EXC [%s] => AWM [%02d]\n"),
		exc_name.c_str(), awm_id);

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onRun() {
	RTLIB_WorkingModeParams_t const wmp = WorkingModeParams();

	// Return after 5 cycles
	if (Cycles() >= 5)
		return RTLIB_EXC_WORKLOAD_NONE;

	// Do one more cycle
	fprintf(stderr, FW("MyApp::onRun()      : EXC [%s]  @ AWM [%02d]\n"),
		exc_name.c_str(), wmp.awm_id);

	return RTLIB_OK;
}

RTLIB_ExitCode_t MyApp::onMonitor() {
	RTLIB_WorkingModeParams_t const wmp = WorkingModeParams();

	fprintf(stderr, FW("MyApp::onMonitor()  : "
		"EXC [%s]  @ AWM [%02d], Cycle [%4d]\n"),
		exc_name.c_str(), wmp.awm_id, Cycles());

	return RTLIB_OK;
}

