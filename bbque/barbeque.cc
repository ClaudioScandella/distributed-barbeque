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

#include "bbque/barbeque.h"
#include "bbque/version.h"

#include "bbque/configuration_manager.h"
#include "bbque/modules_factory.h"
#include "bbque/platform_services.h"
#include "bbque/plugin_manager.h"
#include "bbque/resource_manager.h"
#include "bbque/signals_manager.h"

#include "bbque/utils/timer.h"
#include "bbque/utils/utility.h"
#include "bbque/utils/logging/logger.h"

#include "bbque/plugins/test.h"

#define MODULE_NAMESPACE "bq"

uint32_t hn_cluster = 0;

namespace bb = bbque;
namespace bp = bbque::plugins;
namespace bu = bbque::utils;
namespace po = boost::program_options;

/* The global timer, this can be used to get the time since Barbeque start */
bu::Timer bbque_tmr(true);
std::unique_ptr<bu::Logger> logger;

int Tests(bp::PluginManager & pm) {
	const bp::PluginManager::RegistrationMap & rm = pm.GetRegistrationMap();
	bp::PluginManager::RegistrationMap::const_iterator near_match =
		rm.lower_bound(TEST_NAMESPACE);

	if (near_match == rm.end() ||
			((*near_match).first.compare(0,
				strlen(TEST_NAMESPACE),TEST_NAMESPACE)))
		return false;

	logger->Info("Entering Testing Mode");

	do {
		bu::Timer test_tmr;

		logger->Info("___ Testing [%s]...", (*near_match).first.c_str());

		bp::TestIF * tms = bb::ModulesFactory::GetTestModule(
				(*near_match).first);

		test_tmr.start();
		tms->Test();
		test_tmr.stop();

		logger->Info("___ completed, [%11.6f]s", test_tmr.getElapsedTime());

		near_match++;

	} while (near_match != rm.end() &&
			((*near_match).first.compare(0,5,"test.")) == 0 );

	logger->Warn("All tests completed");
	return EXIT_SUCCESS;
}

// The deamonizing ruotine
extern int
daemonize(const char *name, const char *uid,
		const char *lockfile, const char *pidfile,
		const char *rundir);

static void DaemonizeBBQ(bb::ConfigurationManager & cm) {
	int result;

	logger->Notice("Starting BarbequeRTRM (v%s) as daemon [%s], running with uid [%s]...",
			BBQUE_VERSION_STR,
			cm.GetDaemonName().c_str(), cm.GetUID().c_str());
	logger->Notice("Using configuration file [%s]",
			cm.GetConfigurationFile().c_str());

	// Daemonize this process
	result = daemonize(
			cm.GetDaemonName().c_str(),
			cm.GetUID().c_str(),
			cm.GetLockfile().c_str(),
			cm.GetPIDfile().c_str(),
			cm.GetRundir().c_str()
			);
	if (result == 0) {
		// The OK is not reported, since actual BBQ initialization
		// could still fails
		return;
	}

	syslog(LOG_ERR, "Daemonization FAILED, terminate BBQ");
	logger->Fatal("FAILED");
	exit(EXIT_FAILURE);

}

int main(int argc, char *argv[]) {

	int exit_code;

	/* Initialize the logging interface */
	openlog(BBQUE_DAEMON_NAME, LOG_PID, LOG_LOCAL5);

	// Command line parsing
	bb::ConfigurationManager & cm = bb::ConfigurationManager::GetInstance();
	cm.ParseCommandLine(argc, argv);

	// Setup Logger configuration
	bu::Logger::SetConfigurationFile(cm.GetConfigurationFile());
	logger = bu::Logger::GetLogger(MODULE_NAMESPACE);

	// Check if we should run as daemon
	if (cm.RunAsDaemon()) {
		syslog(LOG_INFO, "Starting BBQ daemon (ver. %s)...", g_git_version);
		syslog(LOG_INFO, "BarbequeRTRM build time: " __DATE__  " " __TIME__);
		syslog(LOG_INFO, "                 flavor: " BBQUE_BUILD_FLAVOR);
		syslog(LOG_INFO, "               compiler: gcc v" STR(GCC_VERSION));
		DaemonizeBBQ(cm);
	} else {
		// Welcome screen
		logger->Info("Starting BBQ (ver. %s)...", g_git_version);
		logger->Info("BarbequeRTRM build time: " __DATE__  " " __TIME__);
		logger->Info("                 flavor: " BBQUE_BUILD_FLAVOR);
		logger->Info("               compiler: gcc v" STR(GCC_VERSION));
	}

	// Initialization
	bp::PluginManager & pm = bp::PluginManager::GetInstance();
	pm.GetPlatformServices().InvokeService =
		bb::PlatformServices::ServiceDispatcher;

	// Plugins loading
	if (cm.LoadPlugins()) {
		if (cm.RunAsDaemon())
			syslog(LOG_INFO, "Loading plugins from dir [%s]...",
					cm.GetPluginsDir().c_str());
		else
			logger->Info("Loading plugins from dir [%s]...",
					cm.GetPluginsDir().c_str());
		pm.LoadAll(cm.GetPluginsDir());
	}

	// Initialize Signals Manager module
	bb::SignalsManager::GetInstance();

	// Check if we have tests to run
	if (cm.RunTests())
	{
		logger->Info("Running tests!");
		return Tests(pm);
	}

	// Let's start baking applications...
	bb::ResourceManager::ExitCode_t result =
		bb::ResourceManager::GetInstance().Go();
	if (result != bb::ResourceManager::ExitCode_t::OK) {
		exit_code = EXIT_FAILURE;
	}

	// Final greetings
	if (cm.RunAsDaemon())
		syslog(LOG_NOTICE, "Goodbye! [%s]",
				(exit_code == EXIT_FAILURE) ? "FAILURE" : "SUCCESS" );
	else
		logger->Notice("Goodbye! [%s]",
				(exit_code == EXIT_FAILURE) ? "FAILURE" : "SUCCESS" );
	closelog();
	return exit_code;
}

