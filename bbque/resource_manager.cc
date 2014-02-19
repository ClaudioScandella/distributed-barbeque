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

#include "bbque/resource_manager.h"

#include "bbque/cpp11/chrono.h"

#include "bbque/configuration_manager.h"
#include "bbque/plugin_manager.h"
#include "bbque/modules_factory.h"
#include "bbque/signals_manager.h"
#include "bbque/application_manager.h"

#include "bbque/utils/utility.h"

#define RESOURCE_MANAGER_NAMESPACE "bq.rm"
#define MODULE_NAMESPACE RESOURCE_MANAGER_NAMESPACE

/** Metrics (class COUNTER) declaration */
#define RM_COUNTER_METRIC(NAME, DESC)\
 {RESOURCE_MANAGER_NAMESPACE "." NAME, DESC, \
	 MetricsCollector::COUNTER, 0, NULL, 0}
/** Increase counter for the specified metric */
#define RM_COUNT_EVENT(METRICS, INDEX) \
	mc.Count(METRICS[INDEX].mh);
/** Increase counter for the specified metric */
#define RM_COUNT_EVENTS(METRICS, INDEX, AMOUNT) \
	mc.Count(METRICS[INDEX].mh, AMOUNT);

/** Metrics (class SAMPLE) declaration */
#define RM_SAMPLE_METRIC(NAME, DESC)\
 {RESOURCE_MANAGER_NAMESPACE "." NAME, DESC, \
	 MetricsCollector::SAMPLE, 0, NULL, 0}
/** Reset the timer used to evaluate metrics */
#define RM_RESET_TIMING(TIMER) \
	TIMER.start();
/** Acquire a new time sample */
#define RM_GET_TIMING(METRICS, INDEX, TIMER) \
	mc.AddSample(METRICS[INDEX].mh, TIMER.getElapsedTimeMs());
/** Acquire a new (generic) sample */
#define RM_ADD_SAMPLE(METRICS, INDEX, SAMPLE) \
	mc.AddSample(METRICS[INDEX].mh, SAMPLE);

/** Metrics (class PERIDO) declaration */
#define RM_PERIOD_METRIC(NAME, DESC)\
 {RESOURCE_MANAGER_NAMESPACE "." NAME, DESC, \
	 MetricsCollector::PERIOD, 0, NULL, 0}
/** Acquire a new time sample */
#define RM_GET_PERIOD(METRICS, INDEX, PERIOD) \
	mc.PeriodSample(METRICS[INDEX].mh, PERIOD);

#define LNSCHB "::::::::::::::::::::: SCHEDULE START ::::::::::::::::::::::::"
#define LNSCHE ":::::::::::::::::::::  SCHEDULE END  ::::::::::::::::::::::::"
#define LNSYNB "**********************  SYNC START  *************************"
#define LNSYNF "*********************  SYNC FAILED  *************************"
#define LNSYNE "***********************  SYNC END  **************************"
#define LNPROB "~~~~~~~~~~~~~~~~~~~  PROFILING START  ~~~~~~~~~~~~~~~~~~~~~~~"
#define LNPROE "~~~~~~~~~~~~~~~~~~~~  PROFILING END  ~~~~~~~~~~~~~~~~~~~~~~~~"

namespace br = bbque::res;
namespace bu = bbque::utils;
namespace bp = bbque::plugins;
namespace po = boost::program_options;

using bp::PluginManager;
using bu::MetricsCollector;
using std::chrono::milliseconds;

namespace bbque {

/* Definition of metrics used by this module */
MetricsCollector::MetricsCollection_t
ResourceManager::metrics[RM_METRICS_COUNT] = {

	//----- Event counting metrics
	RM_COUNTER_METRIC("evt.tot",	"Total events"),
	RM_COUNTER_METRIC("evt.start",	"  START events"),
	RM_COUNTER_METRIC("evt.stop",	"  STOP  events"),
	RM_COUNTER_METRIC("evt.plat",	"  PALT  events"),
	RM_COUNTER_METRIC("evt.opts",	"  OPTS  events"),
	RM_COUNTER_METRIC("evt.usr1",	"  USR1  events"),
	RM_COUNTER_METRIC("evt.usr2",	"  USR2  events"),

	RM_COUNTER_METRIC("sch.tot",	"Total Scheduler activations"),
	RM_COUNTER_METRIC("sch.failed",	"  FAILED  schedules"),
	RM_COUNTER_METRIC("sch.delayed","  DELAYED schedules"),
	RM_COUNTER_METRIC("sch.empty",	"  EMPTY   schedules"),

	RM_COUNTER_METRIC("syn.tot",	"Total Synchronization activations"),
	RM_COUNTER_METRIC("syn.failed",	"  FAILED synchronizations"),

	//----- Sampling statistics
	RM_SAMPLE_METRIC("evt.avg.time",  "Avg events processing t[ms]"),
	RM_SAMPLE_METRIC("evt.avg.start", "  START events"),
	RM_SAMPLE_METRIC("evt.avg.stop",  "  STOP  events"),
	RM_SAMPLE_METRIC("evt.avg.plat",  "  PLAT  events"),
	RM_SAMPLE_METRIC("evt.avg.opts",  "  OPTS  events"),
	RM_SAMPLE_METRIC("evt.avg.usr1",  "  USR1  events"),
	RM_SAMPLE_METRIC("evt.avg.usr2",  "  USR2  events"),

	RM_PERIOD_METRIC("evt.per",		"Avg events period t[ms]"),
	RM_PERIOD_METRIC("evt.per.start",	"  START events"),
	RM_PERIOD_METRIC("evt.per.stop",	"  STOP  events"),
	RM_PERIOD_METRIC("evt.per.plat",	"  PLAT  events"),
	RM_PERIOD_METRIC("evt.per.opts",	"  OPTS  events"),
	RM_PERIOD_METRIC("evt.per.usr1",	"  USR1  events"),
	RM_PERIOD_METRIC("evt.per.usr2",	"  USR2  events"),

	RM_PERIOD_METRIC("sch.per",   "Avg Scheduler period t[ms]"),
	RM_PERIOD_METRIC("syn.per",   "Avg Synchronization period t[ms]"),

};


ResourceManager & ResourceManager::GetInstance() {
	static ResourceManager rtrm;

	return rtrm;
}

ResourceManager::ResourceManager() :
	ps(PlatformServices::GetInstance()),
	sm(SchedulerManager::GetInstance()),
	ym(SynchronizationManager::GetInstance()),
	om(ProfileManager::GetInstance()),
	am(ApplicationManager::GetInstance()),
	ap(ApplicationProxy::GetInstance()),
	pm(PluginManager::GetInstance()),
	ra(ResourceAccounter::GetInstance()),
	mc(MetricsCollector::GetInstance()),
	pp(PlatformProxy::GetInstance()),
	cm(CommandManager::GetInstance()),
	optimize_dfr("rm.opt", std::bind(&ResourceManager::Optimize, this)) {

	//---------- Setup all the module metrics
	mc.Register(metrics, RM_METRICS_COUNT);

	//---------- Register commands
	CommandManager &cm = CommandManager::GetInstance();
#define CMD_STATUS_EXC ".exc_status"
	cm.RegisterCommand(MODULE_NAMESPACE CMD_STATUS_EXC, static_cast<CommandHandler*>(this),
			"Dump the status of each registered EXC");
#define CMD_STATUS_QUEUES ".que_status"
	cm.RegisterCommand(MODULE_NAMESPACE CMD_STATUS_QUEUES, static_cast<CommandHandler*>(this),
			"Dump the status of each Scheduling Queue");
#define CMD_STATUS_RESOURCES ".res_status"
	cm.RegisterCommand(MODULE_NAMESPACE CMD_STATUS_RESOURCES, static_cast<CommandHandler*>(this),
			"Dump the status of each registered Resource");
#define CMD_STATUS_SYNC ".syn_status"
	cm.RegisterCommand(MODULE_NAMESPACE CMD_STATUS_SYNC, static_cast<CommandHandler*>(this),
			"Dump the status of each Synchronization Queue");
#define CMD_OPT_FORCE ".opt_force"
	cm.RegisterCommand(MODULE_NAMESPACE CMD_OPT_FORCE, static_cast<CommandHandler*>(this),
			"Force a new scheduling event");

}

ResourceManager::ExitCode_t
ResourceManager::Setup() {

	//---------- Get a logger module
	std::string logger_name(RESOURCE_MANAGER_NAMESPACE);
	bp::LoggerIF::Configuration conf(logger_name.c_str());
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	if (!logger) {
		fprintf(stderr, "RM: Logger module creation FAILED\n");
		assert(logger);
		return SETUP_FAILED;
	}

	//---------- Loading configuration
	ConfigurationManager & cm = ConfigurationManager::GetInstance();
	po::options_description opts_desc("Resource Manager Options");
	opts_desc.add_options()
		("ResourceManager.opt_interval",
		 po::value<uint32_t>
		 (&opt_interval)->default_value(
			 BBQUE_DEFAULT_RESOURCE_MANAGER_OPT_INTERVAL),
		 "The interval [ms] of activation of the periodic optimization")
		;
	po::variables_map opts_vm;
	cm.ParseConfigurationFile(opts_desc, opts_vm);

	//---------- Dump list of registered plugins
	const bp::PluginManager::RegistrationMap & rm = pm.GetRegistrationMap();
	logger->Info("RM: Registered plugins:");
	bp::PluginManager::RegistrationMap::const_iterator i;
	for (i = rm.begin(); i != rm.end(); ++i)
		logger->Info(" * %s", (*i).first.c_str());

	//---------- Init Platform Integration Layer (PIL)
	PlatformProxy::ExitCode_t result = pp.LoadPlatformData();
	if (result != PlatformProxy::OK) {
		logger->Fatal("Platform Integration Layer initialization FAILED!");
		return SETUP_FAILED;
	}

	//---------- Start bbque services
	pp.Start();
	if (opt_interval)
		optimize_dfr.SetPeriodic(milliseconds(opt_interval));

	return OK;
}

void ResourceManager::NotifyEvent(controlEvent_t evt) {
	std::unique_lock<std::mutex> pendingEvts_ul(pendingEvts_mtx, std::defer_lock);

	// Ensure we have a valid event
	assert(evt<EVENTS_COUNT);

	// Set the corresponding event flag
	pendingEvts.set(evt);

	// Notify the control loop (just if it is sleeping)
	if (pendingEvts_ul.try_lock())
		pendingEvts_cv.notify_one();
}

std::map<std::string, Worker*> ResourceManager::workers_map;
std::mutex ResourceManager::workers_mtx;
std::condition_variable ResourceManager::workers_cv;

void ResourceManager::Register(const char *name, Worker *pw) {
	std::unique_lock<std::mutex> workers_ul(workers_mtx);
	fprintf(stderr, FI("Registering Worker[%s]...\n"), name);
	workers_map[name] = pw;
}

void ResourceManager::Unregister(const char *name) {
	std::unique_lock<std::mutex> workers_ul(workers_mtx);
	fprintf(stderr, FI("Unregistering Worker[%s]...\n"), name);
	workers_map.erase(name);
	workers_cv.notify_one();
}

void ResourceManager::TerminateWorkers() {
	std::unique_lock<std::mutex> workers_ul(workers_mtx);
	std::chrono::milliseconds timeout = std::chrono::milliseconds(300);

	// Waiting for all workers to be terminated
	for (uint8_t i = 3; i; --i) {

		// Signal all registered Workers to terminate
		for_each(workers_map.begin(), workers_map.end(),
				[=](std::pair<std::string, Worker*> entry) {
				fprintf(stderr, FI("Terminating Worker[%s]...\n"),
					entry.first.c_str());
				entry.second->Terminate();
				});

		// Wait up to 300[ms] for workers to terminate
		workers_cv.wait_for(workers_ul, timeout);
		if (workers_map.empty())
			break;

		DB(fprintf(stderr, FD("Waiting for [%lu] workers to terminate...\n"),
					workers_map.size()));

	}

	DB(fprintf(stderr, FD("All workers termianted\n")));
}

void ResourceManager::Optimize() {
	std::unique_lock<std::mutex> pendingEvts_ul(pendingEvts_mtx);
	SynchronizationManager::ExitCode_t syncResult;
	SchedulerManager::ExitCode_t schedResult;
	ProfileManager::ExitCode_t profResult;
	static bu::Timer optimization_tmr;
	double period;

	// Check if there is at least one application to synchronize
	if ((!am.HasApplications(Application::READY)) &&
			(!am.HasApplications(Application::RUNNING))) {
		logger->Debug("NO active EXCs, re-scheduling not required");
		return;
	}

	ra.PrintStatusReport();
	am.PrintStatusReport();
	logger->Info("Running Optimization...");

	// Account for a new schedule activation
	RM_COUNT_EVENT(metrics, RM_SCHED_TOTAL);
	RM_GET_PERIOD(metrics, RM_SCHED_PERIOD, period);

	//--- Scheduling
	logger->Notice(LNSCHB);
	optimization_tmr.start();
	schedResult = sm.Schedule();
	optimization_tmr.stop();
	switch(schedResult) {
	case SchedulerManager::MISSING_POLICY:
	case SchedulerManager::FAILED:
		logger->Warn("Schedule FAILED (Error: scheduling policy failed)");
		RM_COUNT_EVENT(metrics, RM_SCHED_FAILED);
		return;
	case SchedulerManager::DELAYED:
		logger->Error("Schedule DELAYED");
		RM_COUNT_EVENT(metrics, RM_SCHED_DELAYED);
		return;
	default:
		assert(schedResult == SchedulerManager::DONE);
	}
	logger->Info(LNSCHE);
	logger->Notice("Schedule Time: %11.3f[us]", optimization_tmr.getElapsedTimeUs());
	am.PrintStatusReport(true);

	// Check if there is at least one application to synchronize
	if (!am.HasApplications(Application::SYNC)) {
		logger->Debug("NO EXC in SYNC state, synchronization not required");
		RM_COUNT_EVENT(metrics, RM_SCHED_EMPTY);
		goto sched_profile;
	}

	// Account for a new synchronizaiton activation
	RM_COUNT_EVENT(metrics, RM_SYNCH_TOTAL);
	RM_GET_PERIOD(metrics, RM_SYNCH_PERIOD, period);
	if (period)
		logger->Notice("Schedule Run-time: %9.3f[ms]", period);

	//--- Synchroniztion
	logger->Notice(LNSYNB);
	optimization_tmr.start();
	syncResult = ym.SyncSchedule();
	optimization_tmr.stop();
	if (syncResult != SynchronizationManager::OK) {
		logger->Warn(LNSYNF);
		RM_COUNT_EVENT(metrics, RM_SYNCH_FAILED);
		// FIXME here we should implement some counter-meaure to
		// ensure consistency
		return;
	}
	logger->Info(LNSYNE);
	ra.PrintStatusReport(0, true);
	am.PrintStatusReport(true);
	logger->Notice("Sync Time: %11.3f[us]", optimization_tmr.getElapsedTimeUs());

sched_profile:

	//--- Profiling
	logger->Notice(LNPROB);
	optimization_tmr.start();
	profResult = om.ProfileSchedule();
	optimization_tmr.stop();
	if (profResult != ProfileManager::OK) {
		logger->Warn("Scheduler profiling FAILED");
	}
	logger->Info(LNPROE);
	logger->Notice("Prof Time: %11.3f[us]", optimization_tmr.getElapsedTimeUs());

}

void ResourceManager::EvtExcStart() {
	uint32_t timeout = 0;
	AppPtr_t papp;

	logger->Info("EXC Enabled");

	// Reset timer for START event execution time collection
	RM_RESET_TIMING(rm_tmr);

	// This is a simple optimization triggering policy based on the
	// current priority level of READY applications.
	// When an application issue a Working Mode request it is expected to
	// be in ready state. The optimization will be triggered in a
	// timeframe which is _invese proportional_ to the highest priority
	// ready application.
	// This should allows to have short latencies for high priority apps
	// while still allowing for reduced rescheduling on applications
	// startup burst.
	// TODO: make this policy more tunable via the configuration file
	papp = am.HighestPrio(ApplicationStatusIF::READY);
	if (!papp) {
		// In this case the application has exited before the start
		// event has had the change to be processed
		DB(logger->Warn("Overdue processing of a START event"));
		return;
	}
	timeout = 100 + (100 * papp->Priority());
	optimize_dfr.Schedule(milliseconds(timeout));
	
	// Collecing execution metrics
	RM_GET_TIMING(metrics, RM_EVT_TIME_START, rm_tmr);
}

void ResourceManager::EvtExcStop() {
	uint32_t timeout = 0;
	AppPtr_t papp;

	logger->Info("EXC Disabled");

	// Reset timer for START event execution time collection
	RM_RESET_TIMING(rm_tmr);

	// This is a simple oprimization triggering policy based on the
	// number of applications READY to run.
	// When an application terminates we check for the presence of READY
	// applications waiting to start, if there are a new optimization run
	// is scheduled before than the case in which all applications are
	// runnig.
	// TODO: make this policy more tunable via the configuration file
	timeout = 500 - (50 * (am.AppsCount(ApplicationStatusIF::READY) % 8));
	optimize_dfr.Schedule(milliseconds(timeout));

	// Collecing execution metrics
	RM_GET_TIMING(metrics, RM_EVT_TIME_STOP, rm_tmr);
}

void ResourceManager::EvtBbqPlat() {

	logger->Info("BBQ Platform Request");

	// Reset timer for START event execution time collection
	RM_RESET_TIMING(rm_tmr);

	// Platform generated events triggers an immediate rescheduling.
	// TODO add a better policy which triggers immediate rescheduling only
	// on resources reduction. Perhaps such a policy could be plugged into
	// the PlatformProxy module.
	optimize_dfr.Schedule();

	// Collecing execution metrics
	RM_GET_TIMING(metrics, RM_EVT_TIME_PLAT, rm_tmr);
}

void ResourceManager::EvtBbqOpts() {
	uint32_t timeout = 0;

	logger->Info("BBQ Optimization Request");

	// Reset timer for START event execution time collection
	RM_RESET_TIMING(rm_tmr);

	// Explicit applications requests for optimization are delayed by
	// default just to increase the chance for aggregation of multiple
	// requests
	// TODO: make this policy more tunable via the configuration file
	timeout = 500;
	if (am.AppsCount(ApplicationStatusIF::READY))
		timeout = 250;
	optimize_dfr.Schedule(milliseconds(timeout));

	// Collecing execution metrics
	RM_GET_TIMING(metrics, RM_EVT_TIME_OPTS, rm_tmr);
}


void ResourceManager::EvtBbqUsr1() {

	// Reset timer for START event execution time collection
	RM_RESET_TIMING(rm_tmr);

	logger->Info("");
	logger->Info("==========[ Status Queues ]============"
			"========================================");
	logger->Info("");
	am.ReportStatusQ(true);

	logger->Info("");
	logger->Info("");
	logger->Info("==========[ Synchronization Queues ]==="
			"========================================");
	logger->Info("");
	am.ReportSyncQ(true);

	logger->Notice("");
	logger->Notice("");
	logger->Notice("==========[ Resources Status ]========="
			"========================================");
	logger->Notice("");
	ra.PrintStatusReport(0, true);

	logger->Notice("");
	logger->Notice("");
	logger->Notice("==========[ EXCs Status ]=============="
			"========================================");
	logger->Notice("");
	am.PrintStatusReport(true);

	// Clear the corresponding event flag
	logger->Notice("");
	pendingEvts.reset(BBQ_USR1);

	// Collecing execution metrics
	RM_GET_TIMING(metrics, RM_EVT_TIME_USR1, rm_tmr);
}

void ResourceManager::EvtBbqUsr2() {

	// Reset timer for START event execution time collection
	RM_RESET_TIMING(rm_tmr);

	logger->Debug("Dumping metrics collection...");
	mc.DumpMetrics();

	// Clear the corresponding event flag
	pendingEvts.reset(BBQ_USR2);

	// Collecing execution metrics
	RM_GET_TIMING(metrics, RM_EVT_TIME_USR2, rm_tmr);
}

void ResourceManager::EvtBbqExit() {
	AppsUidMapIt apps_it;
	AppPtr_t papp;

	logger->Notice("Terminating Barbeque...");
	done = true;

	// Dumping collected stats before termination
	EvtBbqUsr1();
	EvtBbqUsr2();

	// Stop applications
	papp = am.GetFirst(apps_it);
	for ( ; papp; papp = am.GetNext(apps_it)) {
		// Terminating the application
		logger->Warn("TODO: Send application STOP command");
		// Removing internal data structures
		am.DestroyEXC(papp);
	}

	// Notify all workers
	logger->Notice("Stopping all workers...");
	ResourceManager::TerminateWorkers();

}

int ResourceManager::CommandsCb(int argc, char *argv[]) {
	uint8_t cmd_offset = ::strlen(MODULE_NAMESPACE) + 1;

	logger->Debug("Processing command [%s]", argv[0] + cmd_offset);

	switch (argv[0][cmd_offset]) {
	case 'e':
		if (strcmp(argv[0], MODULE_NAMESPACE CMD_STATUS_EXC))
			goto exit_cmd_not_found;

		logger->Notice("");
		logger->Notice("");
		logger->Notice("==========[ EXCs Status ]=============="
				"========================================");
		logger->Notice("");
		am.PrintStatusReport(true);
		break;

	case 'q':
		if (strcmp(argv[0], MODULE_NAMESPACE CMD_STATUS_QUEUES))
			goto exit_cmd_not_found;

		logger->Info("");
		logger->Info("==========[ Status Queues ]============"
				"========================================");
		logger->Info("");
		am.ReportStatusQ(true);
		break;

	case 'r':
		if (strcmp(argv[0], MODULE_NAMESPACE CMD_STATUS_RESOURCES))
			goto exit_cmd_not_found;

		logger->Notice("");
		logger->Notice("");
		logger->Notice("==========[ Resources Status ]========="
				"========================================");
		logger->Notice("");
		ra.PrintStatusReport(0, true);
		break;

	case 's':
		if (strcmp(argv[0], MODULE_NAMESPACE CMD_STATUS_SYNC))
			goto exit_cmd_not_found;

		logger->Info("");
		logger->Info("");
		logger->Info("==========[ Synchronization Queues ]==="
				"========================================");
		logger->Info("");
		am.ReportSyncQ(true);
		break;
	case 'o':
		logger->Info("");
		logger->Info("");
		logger->Info("==========[ User Required Scheduling ]==="
				"===============================");
		logger->Info("");
		NotifyEvent(ResourceManager::BBQ_OPTS);
		break;
	}

	return 0;

exit_cmd_not_found:
	logger->Error("Command [%s] not suppported by this moduel", argv[0]);
	return -1;
}


void ResourceManager::ControlLoop() {
	std::unique_lock<std::mutex> pendingEvts_ul(pendingEvts_mtx);
	double period;

	// Wait for a new event
	if (!pendingEvts.any())
		pendingEvts_cv.wait(pendingEvts_ul);

	// Checking for pending events, starting from higer priority ones.
	for(uint8_t evt=EVENTS_COUNT; evt; --evt) {

		logger->Debug("Checking events [%d:%s]",
				evt-1, pendingEvts[evt-1] ? "Pending" : "None");

		// Jump not pending events
		if (!pendingEvts[evt-1])
			continue;

		// Account for a new event
		RM_COUNT_EVENT(metrics, RM_EVT_TOTAL);
		RM_GET_PERIOD(metrics, RM_EVT_PERIOD, period);

		// Dispatching events to handlers
		switch(evt-1) {
		case EXC_START:
			logger->Debug("Event [EXC_START]");
			EvtExcStart();
			RM_COUNT_EVENT(metrics, RM_EVT_START);
			RM_GET_PERIOD(metrics, RM_EVT_PERIOD_START, period);
			break;
		case EXC_STOP:
			logger->Debug("Event [EXC_STOP]");
			EvtExcStop();
			RM_COUNT_EVENT(metrics, RM_EVT_STOP);
			RM_GET_PERIOD(metrics, RM_EVT_PERIOD_STOP, period);
			break;
		case BBQ_PLAT:
			logger->Debug("Event [BBQ_PLAT]");
			EvtBbqPlat();
			RM_COUNT_EVENT(metrics, RM_EVT_PLAT);
			RM_GET_PERIOD(metrics, RM_EVT_PERIOD_PLAT, period);
			break;
		case BBQ_OPTS:
			logger->Debug("Event [BBQ_OPTS]");
			EvtBbqOpts();
			RM_COUNT_EVENT(metrics, RM_EVT_OPTS);
			RM_GET_PERIOD(metrics, RM_EVT_PERIOD_OPTS, period);
			break;
		case BBQ_USR1:
			logger->Debug("Event [BBQ_USR1]");
			RM_COUNT_EVENT(metrics, RM_EVT_USR1);
			RM_GET_PERIOD(metrics, RM_EVT_PERIOD_USR1, period);
			EvtBbqUsr1();
			return;
		case BBQ_USR2:
			logger->Debug("Event [BBQ_USR2]");
			RM_COUNT_EVENT(metrics, RM_EVT_USR2);
			RM_GET_PERIOD(metrics, RM_EVT_PERIOD_USR2, period);
			EvtBbqUsr2();
			return;
		case BBQ_EXIT:
			logger->Debug("Event [BBQ_EXIT]");
			EvtBbqExit();
			return;
		case BBQ_ABORT:
			logger->Debug("Event [BBQ_ABORT]");
			logger->Fatal("Abortive quit");
			exit(EXIT_FAILURE);
		default:
			logger->Crit("Unhandled event [%d]", evt-1);
		}

		// Resetting event
		pendingEvts.reset(evt-1);

	}

}

ResourceManager::ExitCode_t
ResourceManager::Go() {
	ExitCode_t result;

	result = Setup();
	if (result != OK)
		return result;

	while (!done) {
		ControlLoop();
	}

	return OK;
}

} // namespace bbque

