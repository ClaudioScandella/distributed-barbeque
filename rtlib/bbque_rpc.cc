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

#include <cmath>

#include "bbque/config.h"
#include "rtlib/bbque_rpc.h"
#include "rtlib/rpc_fifo_client.h"
#include "rtlib/rpc_unmanaged_client.h"
#include "bbque/app/application.h"
#include "bbque/utils/cgroups.h"
#include "bbque/utils/logging/console_logger.h"
#include "bbque/utils/utility.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <sys/stat.h>

#ifdef CONFIG_BBQUE_OPENCL
#include "rtlib/bbque_ocl.h"
#endif

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "rpc"

namespace ba = bbque::app;
namespace bu = bbque::utils;

namespace bbque
{
namespace rtlib
{

std::unique_ptr<bu::Logger> BbqueRPC::logger;

#ifdef CONFIG_BBQUE_RTLIB_EXECUTION_ANALYSER
std::unique_ptr<bu::Logger> BbqueRPC::stat_logger;
#endif

// The RTLib configuration
RTLIB_Conf_t BbqueRPC::rtlib_configuration;

#ifdef CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT
// The CGroup forcing configuration (for UNMANAGED applications)
static bu::CGroups::CGSetup cgsetup;
static std::string cg_cpuset_cpus;
static std::string cg_cpuset_mems;
static std::string cg_cpu_cfs_period_us;
static std::string cg_cpu_cfs_quota_us;
static std::string cg_memory_limit_in_bytes;
#endif // CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT

// The file handler used for statistics dumping
static FILE * output_file = stderr;

BbqueRPC * BbqueRPC::GetInstance()
{
	static BbqueRPC * instance = nullptr;

	if (instance)
		return instance;

	// Get a Logger module
	logger = bu::Logger::GetLogger(BBQUE_LOG_MODULE);

#ifdef CONFIG_BBQUE_RTLIB_EXECUTION_ANALYSER
	stat_logger = bu::Logger::GetLogger("stat");
#endif

	// Parse environment configuration
	ParseOptions();
	// Instantiating a communication client based on the current mode
	// (currently, unmanaged or FIFO)
#ifdef CONFIG_BBQUE_RTLIB_UNMANAGED_SUPPORT

	if (rtlib_configuration.unmanaged.enabled) {
		logger->Warn("Running in UNMANAGED MODE");
		instance = new BbqueRPC_UNMANAGED_Client();
		return instance;
	}

#endif
#ifdef CONFIG_BBQUE_RPC_FIFO
	logger->Debug("Using FIFO RPC channel");
	instance = new BbqueRPC_FIFO_Client();
#else
#error RPC Channel NOT defined
#endif // CONFIG_BBQUE_RPC_FIFO

	return instance;
}

BbqueRPC::~ BbqueRPC(void) { }

RTLIB_ExitCode_t BbqueRPC::ParseOptions()
{
	// Get rtlib options envorinment variable, which is un the form XX:YY:ZZ:WW
	const char * commandline_rtlib_options = std::getenv("BBQUE_RTLIB_OPTS");

	// Command line options are not mandatory. If not present, no problem
	if (! commandline_rtlib_options) {
		logger->Info("BBQUE_RTLIB_OPTS is not set");
		return RTLIB_OK;
	}

#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT
	char * raw_perf_counter_code;
	size_t raw_perf_counter_length = 0;
	uint8_t number_of_raw_counters = 0;
#endif //CONFIG_BBQUE_RTLIB_PERF_SUPPORT
	logger->Debug("BBQUE_RTLIB_OPTS: [%s]", commandline_rtlib_options);
	// Separate options using ":" delimiter
	// NOTE: strtok doeasn't like constant char*; therefore, I use a non-const
	// copy (see strdup)
	char * option = strtok(strdup(commandline_rtlib_options), ":");

	// Parsing all options (only single char opts are supported)
	while (option) {
		logger->Debug("OPT: %s", option);

		switch (option[0]) {
		case 'D':
			// Setup processing duration timeout (cycles or seconds)
			rtlib_configuration.duration.enabled = true;

			if (option[1] == 's' || option[1] == 'S') {
				rtlib_configuration.duration.max_ms_before_termination = 1000 * atoi(
							option + 2);
				rtlib_configuration.duration.time_limit = true;
				logger->Warn("Enabling DURATION timeout %u [s]",
							 rtlib_configuration.duration.max_cycles_before_termination / 1000);
				break;
			}

			if (option[1] == 'c' || option[1] == 'C') {
				rtlib_configuration.duration.max_cycles_before_termination = atoi(option + 2);
				rtlib_configuration.duration.time_limit = false;
				logger->Warn("Enabling DURATION timeout %u [cycles]",
							 rtlib_configuration.duration.max_cycles_before_termination);
				break;
			}

			// No proper duration time/cycles configured
			rtlib_configuration.duration.enabled = false;
			break;

		case 'G':
			// Enabling Global statistics collection
			rtlib_configuration.profile.perf_counters.global = true;
			break;

		case 'K':
			// Disable Kernel and Hipervisor from collected statistics
			rtlib_configuration.profile.perf_counters.no_kernel = true;
			break;

		case 'O':
			// Collect statistics on RTLIB overheads
			rtlib_configuration.profile.perf_counters.overheads = true;
			break;

		case 'U':
			// Enable "unmanaged" mode with the specified AWM
			rtlib_configuration.unmanaged.enabled = true;

			if (*(option + 1))
				sscanf(option + 1, "%d", &rtlib_configuration.unmanaged.awm_id);

			logger->Warn("Enabling UNMANAGED mode, selected AWM [%d]",
						 rtlib_configuration.unmanaged.awm_id);
			break;

		case 'b':
			// Enabling "big numbers" notations
			rtlib_configuration.profile.perf_counters.big_num = true;
			break;

		case 'c':
			// Enabling CSV output
			rtlib_configuration.profile.output.CSV.enabled = true;
			break;
#ifdef CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT

		case 'C':
			char * pos, *next;

			// Enabling CGroup Enforcing
			if (! rtlib_configuration.unmanaged.enabled) {
				logger->Error("CGroup enforcing is supported only in UNMANAGED mode");
				break;
			}

			rtlib_configuration.cgroup_support.enabled = true;
			rtlib_configuration.cgroup_support.static_configuration = true;
			// Format:
			// [:]C <cpus> <cfs_period> <cfs_quota> <mems> <mem_bytes>
			next = option + 1;
			logger->Warn("CGroup Forcing Conf [%s]", next + 1);
			pos = ++ next;
			next = strchr(pos, ' ');
			*next = 0;
			cg_cpuset_cpus = pos;
			rtlib_configuration.cgroup_support.cpuset.cpus = (char *)
					cg_cpuset_cpus.c_str();
			pos = ++ next;
			next = strchr(pos, ' ');
			*next = 0;
			cg_cpu_cfs_period_us = pos;
			rtlib_configuration.cgroup_support.cpu.cfs_period_us = (char *)
					cg_cpu_cfs_period_us.c_str();
			pos = ++ next;
			next = strchr(pos, ' ');
			*next = 0;
			cg_cpu_cfs_quota_us  = pos;
			rtlib_configuration.cgroup_support.cpu.cfs_quota_us = (char *)
					cg_cpu_cfs_quota_us.c_str();
			pos = ++ next;
			next = strchr(pos, ' ');
			*next = 0;
			cg_cpuset_mems = pos;
			rtlib_configuration.cgroup_support.cpuset.mems = (char *)
					cg_cpuset_mems.c_str();
			pos = ++ next; //next = strchr(pos, ' '); *next = 0;
			cg_memory_limit_in_bytes = pos;
			rtlib_configuration.cgroup_support.memory.limit_in_bytes = (char *)
					cg_memory_limit_in_bytes.c_str();
			// Report CGroup configuration
			logger->Debug("CGroup Forcing Setup:");
			logger->Debug("   cpuset.cpus............. %s",
						  rtlib_configuration.cgroup_support.cpuset.cpus);
			logger->Debug("   cpuset.mems............. %s",
						  rtlib_configuration.cgroup_support.cpuset.mems);
			logger->Debug("   cpu.cfs_period_us....... %s",
						  rtlib_configuration.cgroup_support.cpu.cfs_period_us);
			logger->Debug("   cpu.cfs_quota_us........ %s",
						  rtlib_configuration.cgroup_support.cpu.cfs_quota_us);
			logger->Debug("   memory.limit_in_bytes... %s",
						  rtlib_configuration.cgroup_support.memory.limit_in_bytes);
			logger->Warn("Enabling CGroup FORCING mode");
			break;
#endif // CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT

		case 'f':
			// Enabling File output
			rtlib_configuration.profile.output.file = true;
			logger->Notice("Enabling statistics dump on FILE");
			break;

		case 'p':
			// Enabling perf...
			rtlib_configuration.profile.enabled = BBQUE_RTLIB_PERF_ENABLE;
			// ... with the specified verbosity level
			sscanf(option + 1, "%d",
				   &rtlib_configuration.profile.perf_counters.detailed_run);

			if (rtlib_configuration.profile.enabled) {
				logger->Notice("Enabling Perf Counters [verbosity: %d]",
							   rtlib_configuration.profile.perf_counters.detailed_run);
			}
			else {
				logger->Error("WARN: Perf Counters NOT available");
			}

			break;
#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT

		case 'r':
			// Enabling perf...
			rtlib_configuration.profile.enabled = BBQUE_RTLIB_PERF_ENABLE;
			// # of RAW perf counters
			sscanf(option + 1, "%d", &rtlib_configuration.profile.perf_counters.raw);

			if (rtlib_configuration.profile.perf_counters.raw > 0) {
				logger->Info("Enabling %d RAW Perf Counters",
							 rtlib_configuration.profile.perf_counters.raw);
			}
			else {
				logger->Warn("Expected RAW Perf Counters");
				break;
			}

			// Get the first raw performance counter
			raw_perf_counter_code = option + 3;
			// Get code length (= number of letters until a ","
			raw_perf_counter_length = strcspn(raw_perf_counter_code, ",");
			raw_perf_counter_code[raw_perf_counter_length] = '\0';

			// Insert the events into the array
			while (raw_perf_counter_code[0] != '\0') {
				number_of_raw_counters = InsertRAWPerfCounter(raw_perf_counter_code);

				if (number_of_raw_counters == rtlib_configuration.profile.perf_counters.raw)
					break;

				// Get the next raw performance counter
				raw_perf_counter_code += raw_perf_counter_length + 1;
				raw_perf_counter_length = strcspn(raw_perf_counter_code, ",");
				raw_perf_counter_code[raw_perf_counter_length] = '\0';
			}

			rtlib_configuration.profile.perf_counters.raw = number_of_raw_counters;
			break;
#endif //CONFIG_BBQUE_RTLIB_PERF_SUPPORT
#ifdef CONFIG_BBQUE_OPENCL

		case 'o':
			// Enabling OpenCL Profiling Output on file
			rtlib_configuration.profile.opencl.enabled = true;
			sscanf(option + 1, "%d", &rtlib_configuration.profile.opencl.level);
			logger->Notice("Enabling OpenCL profiling [verbosity: %d]",
						   rtlib_configuration.profile.opencl.level);
			break;
#endif //CONFIG_BBQUE_OPENCL

		case 's':

			// Setting CSV separator
			if (option[1])
				rtlib_configuration.profile.output.CSV.separator = option + 1;

			break;
		}

		// Get next option
		option = strtok(NULL, ":");
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::InitializeApplication(const char * name)
{
	if (rtlib_is_initialized)
		return RTLIB_OK;

	application_name = name;
	application_pid  = gettid();
	logger->Debug("Initializing app [%d:%s]", application_pid, application_name);
	RTLIB_ExitCode_t exitCode = _Init(name);

	if (exitCode != RTLIB_OK) {
		logger->Error("Initialization FAILED");
		return exitCode;
	}

	rtlib_is_initialized = true;
	logger->Debug("Initialization DONE");
	return RTLIB_OK;
}

uint8_t BbqueRPC::NextExcID()
{
	static uint8_t exc_id = 0;
	return exc_id ++;
}

RTLIB_EXCHandler_t BbqueRPC::Register(
	const char * name,
	const RTLIB_EXCParameters_t * params)
{
	RTLIB_ExitCode_t result;
	assert(rtlib_is_initialized);
	assert(name && params);
	logger->Info("Registering EXC [%s]...", name);

	// Ensuring the execution context has not been already registered
	for (auto & already_registered_exc : exc_map) {
		auto exc = already_registered_exc.second;

		if (exc->name == name) {
			logger->Error("Registering EXC [%s] FAILED "
						  "(Error: EXC already registered)", name);
			assert(exc->name != name);
			return nullptr;
		}
	}

	// Build the new EXC
	auto new_exc =
		pRegisteredEXC_t(new RegisteredExecutionContext_t(name, NextExcID()));
	memcpy((void *) & (new_exc->parameters), (void *) params,
		   sizeof (RTLIB_EXCParameters_t));
	// Calling the Low-level registration
	result = _Register(new_exc);

	if (result != RTLIB_OK) {
		logger->Error("Registering EXC [%s] FAILED "
					  "(Error %d: %s)", name, result, RTLIB_ErrorStr(result));
		return nullptr;
	}

	// Save the registered execution context
	exc_map.emplace(new_exc->id, new_exc);
	// Mark the EXC as Registered
	setRegistered(new_exc);

	STAT_LOG("APPLICATION:EXC_REGISTER");

	return (RTLIB_EXCHandler_t) & (new_exc->parameters);
}

RTLIB_ExitCode_t BbqueRPC::SetupCGroup(
	const RTLIB_EXCHandler_t exc_handler)
{
	auto exc = getRegistered(exc_handler);

	if (! exc)
		return RTLIB_EXC_NOT_REGISTERED;

	// Setup the control CGroup using the EXC private function
	return CGroupPathSetup(exc);
}

BbqueRPC::pRegisteredEXC_t BbqueRPC::getRegistered(
	const RTLIB_EXCHandler_t exc_handler)
{
	assert(exc_handler);

	// Checking for library initialization
	if (! rtlib_is_initialized) {
		logger->Error("EXC [%p] lookup FAILED "
					  "(Error: RTLIB not initialized)", (void *) exc_handler);
		assert(rtlib_is_initialized);
		return nullptr;
	}

	bool exc_found = false;
	pRegisteredEXC_t exc;

	// Ensuring the execution context has been registered
	for (auto & registered_exc : exc_map) {
		exc = registered_exc.second;

		if ((void *) exc_handler == (void *) &exc->parameters) {
			exc_found = true;
			break;
		}
	}

	// Handle EXC not found
	if (exc_found == false) {
		logger->Error("EXC [%p] lookup FAILED "
					  "(Error: EXC not registered)", (void *) exc_handler);
		assert(exc_found != false);
		return nullptr;
	}

	assert(isRegistered(exc) == true);

	return exc;
}

BbqueRPC::pRegisteredEXC_t BbqueRPC::getRegistered(uint8_t exc_id)
{
	// Checking for library initialization
	if (! rtlib_is_initialized) {
		logger->Error("EXC [uid %d] lookup FAILED "
					  "(Error: RTLIB not initialized)", exc_id);
		assert(rtlib_is_initialized);
		return nullptr;
	}

	bool exc_found = false;
	pRegisteredEXC_t exc;

	// Ensuring the execution context has been registered
	for (auto & registered_exc : exc_map) {
		exc = registered_exc.second;

		if (registered_exc.first == exc_id) {
			exc_found = true;
			break;
		}
	}

	// Handle EXC not found
	if (exc_found == false) {
		logger->Error("EXC [uid %d] lookup FAILED "
					  "(Error: EXC not registered)", exc_id);
		assert(exc_found != false);
		return nullptr;
	}

	assert(isRegistered(exc) == true);

	return exc;
}

void BbqueRPC::Unregister(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	// Dump (verbose) execution statistics
	DumpStats(exc, true);
	// Calling the low-level unregistration
	RTLIB_ExitCode_t result = _Unregister(exc);

	if (result != RTLIB_OK) {
		logger->Error("Unregister EXC [%p:%s] FAILED (Error %d: %s)",
					  (void *) exc_handler, exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return;
	}

	// Mark the EXC as Unregistered
	clearRegistered(exc);
	// Release the controlling CGroup
	CGroupDelete(exc);

	STAT_LOG("APPLICATION:EXC_STOP");
}

void BbqueRPC::UnregisterAll()
{
	RTLIB_ExitCode_t result;

	// Checking for library initialization
	if (! rtlib_is_initialized) {
		logger->Error("EXCs cleanup FAILED (Error: RTLIB not initialized)");
		assert(rtlib_is_initialized);
		return;
	}

	// Unregisterig all the registered EXCs
	for (auto & registered_exc : exc_map) {
		auto exc = registered_exc.second;

		// Jumping already un-registered EXC
		if (! isRegistered(exc))
			continue;

		// Calling the low-level unregistration
		result = _Unregister(exc);

		if (result != RTLIB_OK) {
			logger->Error("Unregister EXC [%s] FAILED (Error %d: %s)",
						  exc->name.c_str(), result, RTLIB_ErrorStr(result));
			return;
		}

		// Mark the EXC as Unregistered
		clearRegistered(exc);
	}
}

RTLIB_ExitCode_t BbqueRPC::Enable(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	assert(isEnabled(exc) == false);
	// Calling the low-level enable function
	RTLIB_ExitCode_t result = _Enable(exc);

	if (result != RTLIB_OK) {
		logger->Error("Enabling EXC [%p:%s] FAILED (Error %d: %s)",
					  (void *) exc_handler, exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return RTLIB_EXC_ENABLE_FAILED;
	}

	// Mark the EXC as Enabled
	setEnabled(exc);
	clearAwmValid(exc);
	clearAwmAssigned(exc);
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::Disable(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	assert(isEnabled(exc) == true);
	// Calling the low-level disable function
	RTLIB_ExitCode_t result = _Disable(exc);

	if (result != RTLIB_OK) {
		logger->Error("Disabling EXC [%p:%s] FAILED (Error %d: %s)",
					  (void *) exc_handler, exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return RTLIB_EXC_DISABLE_FAILED;
	}

	// Mark the EXC as Enabled
	clearEnabled(exc);
	clearAwmValid(exc);
	clearAwmAssigned(exc);
	// Unlocking eventually waiting GetWorkingMode
	exc->exc_condition_variable.notify_one();
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SetupAWMStatistics(pRegisteredEXC_t exc)
{
	assert(exc);
	pAwmStats_t awm_stats(exc->awm_stats[exc->current_awm_id]);

	// Check if this is a newly selected AWM
	if (! awm_stats) {
		logger->Debug("Setup stats for AWM [%d]", exc->current_awm_id);
		awm_stats = exc->awm_stats[exc->current_awm_id] =
						pAwmStats_t(new AwmStats_t);

		// Setup Performance Counters (if required)
		if (PerfRegisteredEvents(exc)) {
			PerfSetupStats(exc, awm_stats);
		}
	}

	// Update usage count
	awm_stats->number_of_uses ++;
	// Configure current AWM stats
	exc->current_awm_stats = awm_stats;
	return RTLIB_OK;
}

#define STATS_HEADER \
"# EXC    AWM     Uses    Cycles     Total |      Min      Max |      Avg      Var"
#define STATS_AWM_SPLIT \
"#===============================+=========+===================+=================="
#define STATS_CYCLE_SPLIT \
"#-------------------------------+---------+-------------------+------------------"
#define STATS_CONF_SPLIT \
"#-------------------------------+---------+-------------------+------------------"
#define STATS_AWM_CLOSE \
"                                +---------+-------------------+------------------"

void BbqueRPC::DumpStatsHeader()
{
	fprintf(output_file, STATS_HEADER "\n");
}

void BbqueRPC::DumpStatsConsole(pRegisteredEXC_t exc, bool verbose)
{
	AwmStatsMap_t::iterator it;
	pAwmStats_t awm_stats;
	int8_t awm_id;
	uint32_t cycles_count;
	double cycle_min, cycle_max, cycle_avg, cycle_var;
	double monitor_min, monitor_max, monitor_avg, monitor_var;
	double config_min, config_max, config_avg, config_var;
	double run_min, run_max, run_avg, run_var;

	// Print RTLib stats for each AWM
	for (auto & awm : exc->awm_stats) {
		awm_id = awm.first;
		awm_stats = awm.second;
		// Ignoring empty statistics
		cycles_count = count(awm_stats->cycle_samples);

		if (! cycles_count)
			continue;

		// Cycles statistics extraction
		cycle_min = min(awm_stats->cycle_samples);
		cycle_max = max(awm_stats->cycle_samples);
		cycle_avg = mean(awm_stats->cycle_samples);
		cycle_var = variance(awm_stats->cycle_samples);

		// Monitor statistics extraction
		monitor_min = min(awm_stats->monitor_samples);
		monitor_max = max(awm_stats->monitor_samples);
		monitor_avg = mean(awm_stats->monitor_samples);
		monitor_var = variance(awm_stats->monitor_samples);

		// Reconfiguration statistics extraction
		config_min = min(awm_stats->config_samples);
		config_max = max(awm_stats->config_samples);
		config_avg = mean(awm_stats->config_samples);
		config_var = variance(awm_stats->config_samples);

		// Reconfiguration statistics extraction
		run_min = min(awm_stats->run_samples);
		run_max = max(awm_stats->run_samples);
		run_avg = mean(awm_stats->run_samples);
		run_var = variance(awm_stats->run_samples);

		uint32_t execution_time = awm_stats->time_spent_configuring
			+ awm_stats->time_spent_running
			+ awm_stats->time_spent_monitoring;

		if (verbose) {
			fprintf(output_file, STATS_AWM_SPLIT"\n");
			fprintf(output_file, "%.8s %03d %8d  %8d | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					exc->name.c_str(), awm_id, awm_stats->number_of_uses, cycles_count,
					execution_time, cycle_min, cycle_max, cycle_avg, cycle_var);
		}
		else {
			logger->Debug(STATS_AWM_SPLIT);
			logger->Debug("%.8s %03d %8d  %8d | %7u | %8.3f %8.3f | %8.3f %8.3f",
					exc->name.c_str(), awm_id, awm_stats->number_of_uses, cycles_count,
					execution_time, cycle_min, cycle_max, cycle_avg, cycle_var);
		}

		if (verbose) {
			fprintf(output_file, STATS_CYCLE_SPLIT "\n");

			fprintf(output_file, "%31s | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					"Run", awm_stats->time_spent_running,
					run_min, run_max, run_avg, run_var);
			fprintf(output_file, "%31s | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					"Monitor", awm_stats->time_spent_monitoring,
					monitor_min, monitor_max, monitor_avg, monitor_var);
			fprintf(output_file, "%31s | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					"Configure - AWM wait", awm_stats->time_spent_configuring,
					config_min, config_max, config_avg, config_var);
			fprintf(output_file, STATS_AWM_CLOSE"\n\n");
		}
		else {
			logger->Debug("%31s | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					"Run", awm_stats->time_spent_running,
					run_min, run_max, run_avg, run_var);
			logger->Debug("%31s | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					"Monitor", awm_stats->time_spent_monitoring,
					monitor_min, monitor_max, monitor_avg, monitor_var);
			logger->Debug("%31s | %7u | %8.3f %8.3f | %8.3f %8.3f\n",
					"Configure - AWM wait", awm_stats->time_spent_configuring,
					config_min, config_max, config_avg, config_var);
			logger->Debug(STATS_AWM_CLOSE"\n");
		}
	}

	if (! PerfRegisteredEvents(exc) || ! verbose)
		return;

	// Print performance counters for each AWM
	for (auto & awm : exc->awm_stats) {
		awm_id = awm.first;
		awm_stats = awm.second;
		cycles_count = count(awm_stats->cycle_samples);
		fprintf(output_file, "\nPerf counters stats for '%s-%d' (%d cycles):\n\n",
				exc->name.c_str(), awm_id, cycles_count);
		PerfPrintStats(exc, awm_stats);
	}
}

#ifdef CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT

RTLIB_ExitCode_t BbqueRPC::CGroupCheckInitialization()
{
	bu::CGroups::CGSetup cgsetup;
	// Initialize CGroup Library
	bu::CGroups::Init(BBQUE_LOG_MODULE);

	if (likely(! rtlib_configuration.cgroup_support.enabled))
		return RTLIB_OK;

	// If not present, setup the "master" BBQUE CGroup as a clone
	// of the root CGroup
	if (bu::CGroups::Exists("/user.slice") == false) {
		logger->Info("Setup [/user.slice] master CGroup");
		bu::CGroups::CloneFromParent("/user.slice");
	}

	// If not present, setup the "master" BBQUE CGroup as a clone
	// of the root CGroup
	if (bu::CGroups::Exists("/user.slice/res") == false) {
		logger->Info("Setup [/user.slice/res] mdev CGroup");
		bu::CGroups::CloneFromParent("/user.slice/res");
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupPathSetup(pRegisteredEXC_t exc)
{
	if (! rtlib_configuration.cgroup_support.enabled)
		return RTLIB_OK;

	char cgpath[] = "/user.slice/res/12345:APPLICATION_NAME:00";
	// Setup the application specific CGroup
	snprintf(cgpath, sizeof (cgpath), "/user.slice/res/%05d:%.6s:%02d",
			 channel_thread_pid,
			 application_name,
			 exc->id);
	logger->Notice("CGroup of EXC %.05d:%s is: %s",
				   channel_thread_pid,
				   application_name,
				   cgpath);
	// Keep track of the configured CGroup path
	exc->cgroup_path = cgpath;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupDelete(pRegisteredEXC_t exc)
{
	bu::CGroups::CGSetup cgsetup;

	if (! rtlib_configuration.cgroup_support.enabled)
		return RTLIB_OK;

	if (exc->cgroup_path.empty()) {
		logger->Debug("CGroup delete FAILED (Error: cgpath not set)");
		return RTLIB_OK;
	}

	// Delete EXC specific CGroup
	bu::CGroups::Delete(exc->cgroup_path.c_str());

	// Mark this CGroup as removed
	exc->cgroup_path.clear();

	STAT_LOG("CGROUP:DELETE");

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupCreate(pRegisteredEXC_t exc)
{
	if (! rtlib_configuration.cgroup_support.enabled)
		return RTLIB_OK;

	bu::CGroups::CGSetup cgsetup;
	const char * cgroup_path = exc->cgroup_path.c_str();

	if (rtlib_configuration.cgroup_support.static_configuration) {
		logger->Warn("Setting up fixed configuration CGroups values");
		cgsetup.cpuset.cpus =
			rtlib_configuration.cgroup_support.cpuset.cpus;
		cgsetup.cpuset.mems =
			rtlib_configuration.cgroup_support.cpuset.mems;
		cgsetup.cpu.cfs_period_us =
			rtlib_configuration.cgroup_support.cpu.cfs_period_us;
		cgsetup.cpu.cfs_quota_us =
			rtlib_configuration.cgroup_support.cpu.cfs_quota_us;
		cgsetup.memory.limit_in_bytes =
			rtlib_configuration.cgroup_support.memory.limit_in_bytes;
	}
	else {
		bu::CGroups::Read("/user.slice/res", cgsetup);
	}

	// Setup CGroup PATH
	if (bu::CGroups::WriteCgroup(cgroup_path, cgsetup, 0) !=
		bu::CGroups::CGResult::OK) {
		logger->Error("CGroup setup [%s] FAILED");
		return RTLIB_ERROR;
	}

	STAT_LOG("CGROUP:CREATE");

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupCommitAllocation(pRegisteredEXC_t exc)
{
#ifdef CONFIG_BBQUE_RTLIB_UNMANAGED_SUPPORT
	if (rtlib_configuration.unmanaged.enabled)
		return RTLIB_OK;
#endif // CONFIG_BBQUE_RTLIB_UNMANAGED_SUPPORT

	// Proceed only in case of cgroup support and dynamic assignment
	if (! rtlib_configuration.cgroup_support.enabled ||
		rtlib_configuration.cgroup_support.static_configuration)
		return RTLIB_OK;

#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE

	bu::CGroups::CGSetup cgsetup;
	const char * cgroup_path = exc->cgroup_path.c_str();
	// Reading previous values
	bu::CGroups::Read(cgroup_path, cgsetup);

#ifdef CONFIG_RTLIB_DA_MIN_EFFICIENCY

	if (likely(exc->run_time_ms > 0)) {
		uint32_t execution_time = exc->config_time_ms + exc->monitor_time_ms + exc->run_time_ms;
		uint32_t process_time = exc->run_time_ms + exc->config_time_ms - exc->blocked_time_ms;

		float current_allocation_efficiency =
			(float) process_time / execution_time;

		if (current_allocation_efficiency < exc->min_allocation_efficiency) {
			logger->Debug("Skipping CGW: low efficiency (%.3f < %.3f)",
				current_allocation_efficiency,
				exc->min_allocation_efficiency);

			exc->trigger_reconfigure = false;

			return RTLIB_OK;
		}
	}

#endif // CONFIG_RTLIB_DA_MIN_EFFICIENCY

	// CFS_PERIOD: the period over which cpu bandwidth. limit is enforced
	cgsetup.cpu.cfs_period_us = std::to_string(exc->cgroup_cpu_cfs_period_us);
	// CFS_quota: the enforced CPU bandwidth wrt the period
	uint32_t cfs_quota = (uint32_t) (exc->cg_current_allocation.cpu_budget
				* exc->cgroup_cpu_cfs_period_us);

	logger->Debug("Updating cpu.cfs_quota_us: %s to %s",
		cgsetup.cpu.cfs_quota_us.c_str(),
		std::to_string(cfs_quota).c_str());

	cgsetup.cpu.cfs_quota_us = std::to_string(cfs_quota);

	// CPUSET representing the allocated processing elements
	logger->Debug("Updating cpuset.cpus: %s -> %s",
		 cgsetup.cpuset.cpus.c_str(),
		 exc->cg_current_allocation.cpuset_cpus.c_str());

	cgsetup.cpuset.cpus = exc->cg_current_allocation.cpuset_cpus;

	// MEMS representing the allocated memory nodes, if any
	if (exc->cg_current_allocation.cpuset_mems != "") {
		logger->Debug("Updating cpuset.mems: %s -> %s",
			cgsetup.cpuset.mems.c_str(),
			 exc->cg_current_allocation.cpuset_mems.c_str());

		cgsetup.cpuset.mems = exc->cg_current_allocation.cpuset_mems;
	} else
		logger->Debug("Keeping previous cpuset.mems value: %s",
				cgsetup.cpuset.mems.c_str());


	// Memory limit in bytes
	if (exc->cg_current_allocation.memory_limit_bytes != "") {
		logger->Debug("Updating memory.limit_in_bytes: %s -> %s",
			cgsetup.memory.limit_in_bytes.c_str(),
			exc->cg_current_allocation.memory_limit_bytes.c_str());

		cgsetup.memory.limit_in_bytes = exc->cg_current_allocation.memory_limit_bytes;
	} else
		logger->Debug("Keeping previous memory.limit_in_bytes value: %s",
				cgsetup.memory.limit_in_bytes.c_str());


	logger->Debug("Cgroup write: [pes %s] [mem %s - %s bytes] [cfs %s/%s]",
		cgsetup.cpuset.cpus.c_str(),
		cgsetup.cpuset.mems.c_str(),
		cgsetup.memory.limit_in_bytes.c_str(),
		cgsetup.cpu.cfs_quota_us.c_str(),
		cgsetup.cpu.cfs_period_us.c_str());

	bu::CGroups::WriteCgroup(cgroup_path, cgsetup, channel_thread_pid);

	STAT_LOG("CGROUP:WRITE:CPUSET %s", cgsetup.cpuset.cpus.c_str());
	STAT_LOG("CGROUP:WRITE:MEMS %s", cgsetup.cpuset.mems.c_str());
	STAT_LOG("CGROUP:WRITE:CFS_PERIOD %s", cgsetup.cpu.cfs_period_us.c_str());
	STAT_LOG("CGROUP:WRITE:CFS_QUOTA %s", cgsetup.cpu.cfs_quota_us.c_str());

#else
	UNUSED(exc);

#endif // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE

	return RTLIB_OK;
}

#else // CONFIG_BBQUE_RTLIB_CGROUPS_SUPPORT

RTLIB_ExitCode_t BbqueRPC::CGroupCommitAllocation(pRegisteredEXC_t exc)
{
	(void) exc;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupCheckInitialization()
{
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupPathSetup(pRegisteredEXC_t exc)
{
	(void) exc;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupDelete(pRegisteredEXC_t exc)
{
	(void) exc;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::CGroupCreate(pRegisteredEXC_t exc, int pid)
{
	(void) exc;
	return RTLIB_OK;
}

#endif // CONFIG_BBQUE_RTLIB_CGROUPS_SUPPPORT

void BbqueRPC::DumpStats(pRegisteredEXC_t exc, bool verbose)
{
	std::string outfile(BBQUE_PATH_VAR "/");

	// Statistics should be dumped only if:
	// - compiled in DEBUG mode, or
	// - VERBOSE execution is required
	// This check allows to avoid metrics computation in case the output
	// should not be generated.
	if (! verbose)
		return;

	output_file = stderr;

	if (rtlib_configuration.profile.output.file) {
		outfile += std::string("stats_") + channel_thread_unique_id + ":" + exc->name;
		output_file = fopen(outfile.c_str(), "w");
	}

	if (! rtlib_configuration.profile.output.file)
		logger->Notice("Execution statistics:\n\n");

	uint32_t execution_time = exc->config_time_ms + exc->monitor_time_ms + exc->run_time_ms;
	uint32_t process_time = exc->run_time_ms + exc->config_time_ms - exc->blocked_time_ms;
	double efficiency = (double) process_time / execution_time;

	if (verbose) {
		fprintf(output_file, "Cumulative execution statistics for '%s':\n\n",
				exc->name.c_str());
		fprintf(output_file, "  Total processing bursts           : %7lu\n", exc->cycles_count);
		fprintf(output_file, "  Latency to first processing burst : %7u [ms]\n\n", exc->starting_time_ms);
		fprintf(output_file, "  Time spent waiting for resources  : %7u [ms]\n", exc->blocked_time_ms);
		fprintf(output_file, "  Time spent (re)configuring        : %7u [ms]\n", exc->config_time_ms - exc->blocked_time_ms);
		fprintf(output_file, "  Time spent monitoring             : %7u [ms]\n", exc->monitor_time_ms);
		fprintf(output_file, "  Time spent processing             : %7u [ms]\n", exc->run_time_ms);
		fprintf(output_file, "\n");
		fprintf(output_file, "  Management efficiency             : %7.2f %%\n", efficiency * 100.0);
		fprintf(output_file, "\n");
	}
	else {
		logger->Debug("");
		logger->Debug("Cumulative execution statistics for '%s':", exc->name.c_str());
		logger->Debug("  Total processing bursts           : %7lu", exc->cycles_count);
		logger->Debug("  Latency to first processing burst : %7u [ms]", exc->starting_time_ms);
		logger->Debug("");
		logger->Debug("  Time spent waiting for resources  : %7u [ms]", exc->blocked_time_ms);
		logger->Debug("  Time spent (re)configuring        : %7u [ms]", exc->config_time_ms - exc->blocked_time_ms);
		logger->Debug("  Time spent monitoring             : %7u [ms]", exc->monitor_time_ms);
		logger->Debug("  Time spent processing             : %7u [ms]", exc->run_time_ms);
		logger->Debug("");
		logger->Debug("  Management efficiency             : %7.2f %%", efficiency * 100.0);
		logger->Debug("");
	}

	DumpStatsHeader();
	DumpStatsConsole(exc, verbose);
#ifdef CONFIG_BBQUE_OPENCL

	// Dump OpenCL profiling info for each AWM
	if (rtlib_configuration.profile.opencl.enabled)
		OclDumpStats(exc);

#endif //CONFIG_BBQUE_OPENCL

	if (rtlib_configuration.profile.output.file) {
		fclose(output_file);
		logger->Warn("Execution statistics dumped on [%s]", outfile.c_str());
	}
}

RTLIB_ExitCode_t BbqueRPC::UpdateCPUBandwidthStats(pRegisteredEXC_t exc)
{
	exc->cpu_usage_info.current_time = times(&exc->cpu_usage_info.time_sample);
	clock_t elapsed_time = exc->cpu_usage_info.current_time
						   - exc->cpu_usage_info.previous_time;
	clock_t system_time = exc->cpu_usage_info.time_sample.tms_stime
						  - exc->cpu_usage_info.previous_tms_stime;
	clock_t user_time = exc->cpu_usage_info.time_sample.tms_utime
						- exc->cpu_usage_info.previous_tms_utime;

	if (elapsed_time <= 0 || system_time < 0 || user_time < 0)
		return RTLIB_ERROR;

	double cpu_usage = 100.0 * (system_time + user_time) / elapsed_time;
	exc->cpu_usage_analyser.InsertValue(cpu_usage);
	logger->Debug("Measured CPU Usage: %f, average: %f",
				  cpu_usage, exc->cpu_usage_analyser.GetMean());
	return RTLIB_OK;
}

void BbqueRPC::InitCPUBandwidthStats(pRegisteredEXC_t exc)
{
	exc->cpu_usage_info.previous_time = times(&exc->cpu_usage_info.time_sample);
	exc->cpu_usage_info.previous_tms_stime =
		exc->cpu_usage_info.time_sample.tms_stime;
	exc->cpu_usage_info.previous_tms_utime =
		exc->cpu_usage_info.time_sample.tms_utime;
}

void BbqueRPC::ResetRuntimeProfileStats(RTLIB_EXCHandler_t exc_handler,
			bool new_user_goal)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	logger->Debug("Resetting cycle time history");
	exc->average_cycletime_pre_reset_ms = exc->time_analyser_usercycle.GetMean();

	exc->time_analyser_cycle.Reset();

	if (new_user_goal)
		exc->time_analyser_usercycle.Reset();

	logger->Debug("Resetting CPU quota history");
	exc->cpu_usage_analyser.Reset();
	exc->waiting_sync_timeout_ms = 0;
	exc->is_waiting_for_sync = false;
}

RTLIB_ExitCode_t BbqueRPC::WaitForWorkingMode(pRegisteredEXC_t exc)
{
	std::unique_lock<std::mutex> exc_u_lock(exc->exc_mutex);

	// Shortcut in case the AWM has been already assigned
	if (!isAwmAssigned(exc)) {

		// Notify we are going to be suspended waiting for an AWM
		setAwmWaiting(exc);

		// Wait for the EXC being un-BLOCKED
		if (isBlocked(exc))
			while (isBlocked(exc))
				exc->exc_condition_variable.wait(exc_u_lock);
		else

			// Wait for the EXC being assigned an AWM
			while (isEnabled(exc) && ! isAwmAssigned(exc) && ! isBlocked(exc))
				exc->exc_condition_variable.wait(exc_u_lock);

		clearAwmWaiting(exc);

		// TIMER: Get blocked time
		exc->blocked_time_ms += exc->execution_timer.getElapsedTimeMs()
					    - exc->configure_tstart_ms;
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::GetWorkingModeParams(
	pRegisteredEXC_t exc,
	RTLIB_WorkingModeParams_t * wm)
{
	setAwmValid(exc);
	wm->awm_id = exc->current_awm_id;
	wm->nr_sys = exc->resource_assignment.size();
	wm->systems = (RTLIB_SystemResources_t *) malloc(sizeof (
					  RTLIB_SystemResources_t)
				  * wm->nr_sys);
	int i = 0;

	for (auto system : exc->resource_assignment) {
		auto resource_assignment = system.second;
		wm->systems[i].sys_id = resource_assignment->sys_id;
		wm->systems[i].number_cpus  = resource_assignment->number_cpus;
		wm->systems[i].number_proc_elements = resource_assignment->number_proc_elements;
		wm->systems[i].cpu_bandwidth = resource_assignment->cpu_bandwidth;
		wm->systems[i].mem_bandwidth  = resource_assignment->mem_bandwidth;
#ifdef CONFIG_BBQUE_OPENCL
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		wm->res_allocation[i].gpu_bandwidth  = resource_assignment->gpu_bandwidth;
		wm->res_allocation[i].accelerator_bandwidth  =
			resource_assignment->accelerator_bandwidth;
#endif
#endif
	}

	// Setup AWM statistics
	SetupAWMStatistics(exc);
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::GetAssignedResources(
	RTLIB_EXCHandler_t exc_handler,
	const RTLIB_WorkingModeParams_t * wm,
	RTLIB_ResourceType_t r_type,
	int32_t & r_amount)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	if (! isAwmAssigned(exc)) {
		logger->Error("Getting resources for EXC [%p] FAILED "
					  "(Error: No resources assigned yet)", (void *) exc_handler);
		r_amount = - 1;
		return RTLIB_EXC_NOT_STARTED;
	}

#ifdef CONFIG_BBQUE_RTLIB_UNMANAGED_SUPPORT
	if (rtlib_configuration.unmanaged.enabled) {
		logger->Warn("Getting resources for EXC [%p] SKIPPED "
					  "(UNMANAGED mode)", (void *) exc_handler);
		r_amount = - 1;
		return RTLIB_OK;
	}
#endif // CONFIG_BBQUE_RTLIB_UNMANAGED_SUPPORT

	switch (r_type) {
	case SYSTEM:
		r_amount = wm->nr_sys;
		break;

	case CPU:
		r_amount = wm->systems[0].number_cpus;
		break;

	case PROC_NR:
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		r_amount = (int32_t) std::ceil(exc->cg_current_allocation.cpu_budget);
#else // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		r_amount = wm->systems[0].number_proc_elements;
#endif // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		break;

	case PROC_ELEMENT:
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		r_amount = (int32_t) (100.0f * exc->cg_current_allocation.cpu_budget);
#else // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		r_amount = wm->systems[0].cpu_bandwidth;
#endif // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
		break;

	case MEMORY:
		r_amount = wm->systems[0].mem_bandwidth;
		break;

#ifdef CONFIG_BBQUE_OPENCL
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
	case GPU:
		r_amount = wm->res_allocation[0].gpu_bandwidth;
		break;

	case ACCELERATOR:
		r_amount = wm->res_allocation[0].accelerator_bandwidth;
		break;
#endif
#endif // CONFIG_BBQUE_OPENCL

	default:
		r_amount = - 1;
		break;
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::GetAffinityMask(
		RTLIB_EXCHandler_t exc_handler,
		int32_t * ids_vector,
                int vector_size) {

	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	for (int id = 0; id < vector_size; id ++)
		ids_vector[id] = -1;

#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
    int ids_number = std::min(
			vector_size, (int) exc->cg_current_allocation.cpu_affinity_mask.size());

    for (int id = 0; id < ids_number; id ++)
		ids_vector[id] = exc->cg_current_allocation.cpu_affinity_mask[id];
#endif

    return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::GetAssignedResources(
	RTLIB_EXCHandler_t exc_handler,
	const RTLIB_WorkingModeParams_t * wm,
	RTLIB_ResourceType_t r_type,
	int32_t * sys_array,
	uint16_t array_size)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	if (! isAwmAssigned(exc)) {
		logger->Error("Getting resources for EXC [%p] FAILED "
					  "(Error: No resources assigned yet)", (void *) exc_handler);
		return RTLIB_EXC_NOT_STARTED;
	}

	int n_to_copy = wm->nr_sys < array_size ? wm->nr_sys : array_size;

	switch (r_type) {
	case SYSTEM:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->systems[i].sys_id;

		break;

	case CPU:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->systems[i].number_cpus;

		break;

	case PROC_NR:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->systems[i].number_proc_elements;

		break;

	case PROC_ELEMENT:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->systems[i].cpu_bandwidth;

		break;

	case MEMORY:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->systems[i].mem_bandwidth;

		break;
#ifdef CONFIG_BBQUE_OPENCL
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
	case GPU:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->res_allocation[i].gpu_bandwidth;

		break;

	case ACCELERATOR:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = wm->res_allocation[i].accelerator_bandwidth;

		break;
#endif
#endif // CONFIG_BBQUE_OPENCL

	default:
		for (int i = 0; i < n_to_copy; i ++)
			sys_array[i] = - 1;

		break;
	}

	return RTLIB_OK;
}

void BbqueRPC::StartPCountersMonitoring(
	RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	// Add all the required performance counters
	if (rtlib_configuration.profile.enabled) {
		logger->Notice("Starting performance counters monitoring");
		PerfSetupEvents(exc);

		if (rtlib_configuration.profile.perf_counters.global
			&& PerfRegisteredEvents(exc))
			PerfEnable(exc);
	}
	else
		logger->Info("Performance counters monitoring is disabled");
}

RTLIB_ExitCode_t BbqueRPC::WaitForSyncDone(pRegisteredEXC_t exc)
{
	std::unique_lock<std::mutex> exc_u_lock(exc->exc_mutex);

	while (isEnabled(exc) && ! isSyncDone(exc)) {
		logger->Debug("Waiting for reconfiguration to complete...");
		exc->exc_condition_variable.wait(exc_u_lock);
	}

	// TODO add a timeout wait to limit the maximum reconfiguration time
	// before notifying an anomaly to the RTRM
	clearSyncMode(exc);
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::RegisterControlThreadPID(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	if (unlikely(exc->control_thread_pid == 0)) {
		// Keep track of the Control Thread PID
		exc->control_thread_pid = gettid();
		logger->Debug("Tracking control thread PID [%d] for EXC [%d]...",
					  exc->control_thread_pid, exc->id);
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::GetWorkingMode(
	const RTLIB_EXCHandler_t exc_handler,
	RTLIB_WorkingModeParams_t * working_mode_params,
	RTLIB_SyncType_t synch_type)
{
	// FIXME Remove compilation warning
	UNUSED(synch_type);

	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Exit if the EXC has been disabled
	if (! isEnabled(exc))
		return RTLIB_EXC_GWM_FAILED;

#ifdef CONFIG_BBQUE_RTLIB_UNMANAGED_SUPPORT

	if (rtlib_configuration.unmanaged.enabled) {

		// If the EXC has already got a valid AWM, do not get one
		// (in unmanaged mode, AWM never changes)
		if (isAwmValid(exc)) {
			setSyncDone(exc);
			return RTLIB_OK;
		}

		// Configure unmanaged EXC with the selected AWM
		exc->event = RTLIB_EXC_GWM_START;
		exc->current_awm_id = rtlib_configuration.unmanaged.awm_id;

		setAwmAssigned(exc);
		GetWorkingModeParams(exc, working_mode_params);

		// Creating a CGroup for the application
		CGroupCreate(exc);

		return exc->event;
	}

#endif

	// Analyze the current Working Mode
	std::unique_lock<std::mutex> exc_u_lock(exc->exc_mutex);
	bool continue_running = ! isBlocked(exc) && isAwmValid(exc);
	bool ask_for_awm      = ! isSyncMode(exc) && ! isAwmValid(exc);
	exc_u_lock.unlock();

	// Ask for a valid AWM if needed
	RTLIB_ExitCode_t result = RTLIB_OK;

	if (! ask_for_awm) {
		logger->Debug("[%s] ScheduleRequest not needed", exc->name.c_str());
		setSyncDone(exc);
		// Notify about synchronization completed
		exc->exc_condition_variable.notify_one();
	} else {
		logger->Debug("[%s] Sending a ScheduleRequest", exc->name.c_str());
		result = _ScheduleRequest(exc);
	}

	if (result != RTLIB_OK) {
		logger->Error("[%s] ScheduleRequest FAILED (Error %d: %s)",
			exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return RTLIB_EXC_GWM_FAILED;
	}

	// Wait for a valid AWM
	if (! continue_running) {
		// Reset PostMonitor reconfigure flag, because the EXC will
		// already reconfigure due to AWM change (or be suspended)
		exc->trigger_reconfigure = false;

		logger->Debug("Waiting for assigned AWM...");
		// Waiting for an AWM being assigned
		result = WaitForWorkingMode(exc);

		if (result != RTLIB_OK) {
			logger->Error("[%s] WaitForWorkingMode FAILED "
						  "(Error %d: %s)", exc->name.c_str(), result,
						  RTLIB_ErrorStr(result));
			return RTLIB_EXC_GWM_FAILED;
		}

		GetWorkingModeParams(exc, working_mode_params);

		// Exit if the EXC has been disabled
		if (! isEnabled(exc))
			return RTLIB_EXC_GWM_FAILED;

		// Processing the required reconfiguration action
		switch (exc->event) {
		case RTLIB_EXC_GWM_START:
			CGroupCreate(exc);
		case RTLIB_EXC_GWM_RECONF:
		case RTLIB_EXC_GWM_MIGREC:
		case RTLIB_EXC_GWM_MIGRATE:
			logger->Debug("[%s] Migration", exc->name.c_str());
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
			// Distributed actuation: EXC will reconfigure on
			// allocation change (or EXC start), not on budget change
			if (exc->event != RTLIB_EXC_GWM_START)
				return RTLIB_OK;
#else  // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
			ResetRuntimeProfileStats(exc_handler);
#endif // CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
			break;

		case RTLIB_EXC_GWM_BLOCKED:
			logger->Debug("[%s] Blocked", exc->name.c_str());
			break;

		default:
			logger->Error("[%s] GWM FAILED (Error: Invalid event [%d])",
						exc->name.c_str(), exc->event);
			assert(exc->event >= RTLIB_EXC_GWM_START);
			assert(exc->event <= RTLIB_EXC_GWM_BLOCKED);
			break;
		}

		return exc->event;
	}

	// If no new AWM was assigned but PostMonitor asked for a
	// reconfiguration, mark the EXC as `migrating`
	if (exc->trigger_reconfigure) {
		ResetRuntimeProfileStats(exc_handler);
		// Reset PostMonitor reconfigure flag
		exc->trigger_reconfigure = false;
		// Trigger migration
		return RTLIB_EXC_GWM_MIGRATE;
	}

	return RTLIB_OK;
}

uint32_t BbqueRPC::GetSyncLatency(pRegisteredEXC_t exc)
{
	pAwmStats_t awm_stats = exc->current_awm_stats;
	double elapsedTime;
	double syncDelay;
	double _avg;
	double _var;
	// Get the statistics for the current AWM
	assert(awm_stats);
	std::unique_lock<std::mutex> stats_lock(awm_stats->stats_mutex);
	_avg = mean(awm_stats->cycle_samples);
	_var = variance(awm_stats->cycle_samples);
	stats_lock.unlock();
	// Compute a reasonale sync point esimation
	// we assume a NORMAL distribution of execution times
	syncDelay = _avg + (10 * std::sqrt(_var));
	// Discount the already passed time since lasy sync point
	elapsedTime = exc->execution_timer.getElapsedTimeMs();

	if (elapsedTime < syncDelay)
		syncDelay -= elapsedTime;
	else
		syncDelay = 0;

	logger->Debug("Expected sync time in %10.3f[ms] for EXC [%s:%02hu]",
				  syncDelay, exc->name.c_str(), exc->id);
	return std::ceil(syncDelay);
}

/******************************************************************************
 * Synchronization Protocol Messages
 ******************************************************************************/

RTLIB_ExitCode_t BbqueRPC::SyncP_PreChangeNotify(pRegisteredEXC_t exc)
{
	// Entering Synchronization mode
	setSyncMode(exc);
	// Resetting Sync Done
	clearSyncDone(exc);
	// Setting current AWM as invalid
	clearAwmValid(exc);
	clearAwmAssigned(exc);
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SyncP_PreChangeNotify( rpc_msg_BBQ_SYNCP_PRECHANGE_t
		msg,
		std::vector<rpc_msg_BBQ_SYNCP_PRECHANGE_SYSTEM_t> & systems)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(msg.hdr.exc_id);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	RTLIB_ExitCode_t result;
	uint32_t syncLatency;

	assert(msg.event < ba::ApplicationStatusIF::SYNC_STATE_COUNT);
	std::unique_lock<std::mutex> exc_u_lock(exc->exc_mutex);
	// Keep copy of the required synchronization action
	exc->event = (RTLIB_ExitCode_t) (RTLIB_EXC_GWM_START + msg.event);
	result = SyncP_PreChangeNotify(exc);

	// Set the new required AWM (if not being blocked)
	if (exc->event != RTLIB_EXC_GWM_BLOCKED) {
		exc->current_awm_id = msg.awm;

		for (uint16_t i = 0; i < systems.size(); i ++) {
			pSystemResources_t tmp = std::make_shared<RTLIB_SystemResources_t>();
			tmp->sys_id = i;
			tmp->number_cpus  = systems[i].nr_cpus;
			tmp->number_proc_elements = systems[i].nr_procs;
			tmp->cpu_bandwidth = systems[i].r_proc;
			tmp->mem_bandwidth  = systems[i].r_mem;
#ifdef CONFIG_BBQUE_OPENCL
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
			tmp->gpu_bandwidth  = res_allocation[i].gpu_bandwidth;
			tmp->accelerator_bandwidth  = res_allocation[i].accelerator_bandwidth;
			tmp->ocl_device_id = res_allocation[i].dev;
#endif
#endif // CONFIG_BBQUE_OPENCL
			exc->resource_assignment[i] = tmp;
		}

#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE

	// Initializing budget info:
	exc->cg_budget.cpuset_cpus_isolation = ""; // PEs allocated in isolation
	exc->cg_budget.cpuset_cpus_global    = ""; // All the allocated PEs
	exc->cg_budget.cpuset_mems           = ""; // Allocated mem nodes
	exc->cg_budget.memory_limit_bytes    = ""; // Allocated memory bw (bytes)
	exc->cg_budget.cpu_budget_isolation  = 0.0;
	exc->cg_budget.cpu_budget_shared = 0.0;

	unsigned long proc_elements = msg.cpu_ids;
	unsigned long proc_elements_isolation = msg.cpu_ids_isolation;
	unsigned long mem_nodes = msg.mem_ids;

	////////////////////////////////////////////////////////////////////////
	// Retrieving processing elements info /////////////////////////////////
	////////////////////////////////////////////////////////////////////////

	// All the proc elements that have been assigned to the application
	for (int pe_id = 0; pe_id < BBQUE_MAX_R_ID_NUM; pe_id ++) {

		// Skip if this processing element is NOT assigned to this app
		if (! ((1ll << pe_id) & proc_elements))
			continue;

		// Adding the processing element id in the global control group
		// setup string, which contains a comma-separated list of ids
		exc->cg_budget.cpuset_cpus_global +=
			(exc->cg_budget.cpuset_cpus_global == "")
			? std::to_string(pe_id)
			: "," + std::to_string(pe_id);

                exc->cg_budget.cpu_global_ids.push_back(pe_id);
	}

	// Setting the global PE bandwidth as the global available one
	exc->cg_budget.cpu_budget_shared =
		(double) exc->resource_assignment[0]->cpu_bandwidth / 100.0f;

	// Proc elements that have been EXCLUSIVELY assigned to application
	for (int pe_id = 0; pe_id < BBQUE_MAX_R_ID_NUM; pe_id ++) {

		// Skip if this processing element is NOT exclusively to this app
		if (! ((1ll << pe_id) & proc_elements_isolation))
			continue;

		// Adding the processing element id in the global control group
		// setup string, which contains a comma-separated list of ids
		exc->cg_budget.cpuset_cpus_isolation +=
			(exc->cg_budget.cpuset_cpus_isolation == "")
			? std::to_string(pe_id)
			: "," + std::to_string(pe_id);

		exc->cg_budget.cpu_budget_isolation++;
                exc->cg_budget.cpu_isolation_ids.push_back(pe_id);
	}

	// If one of the pes list is empty, it means that the proc elements
	// are all either shared or isolated. Hence, there is only a valid list.
	if (exc->cg_budget.cpuset_cpus_isolation == "")
		exc->cg_budget.cpuset_cpus_isolation =
			exc->cg_budget.cpuset_cpus_global;

	if (exc->cg_budget.cpuset_cpus_global == "")
		exc->cg_budget.cpuset_cpus_global =
			exc->cg_budget.cpuset_cpus_isolation;

	if (exc->cg_budget.cpu_budget_isolation > exc->cg_budget.cpu_budget_shared)
		exc->cg_budget.cpu_budget_isolation = exc->cg_budget.cpu_budget_shared;

	////////////////////////////////////////////////////////////////////////
	// Retrieving memory nodes info ////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	for (int mem_id = 0; mem_id < BBQUE_MAX_R_ID_NUM; mem_id ++) {

		// Skip if this memory node is NOT assigned to this app
		if (! ((1ll << mem_id) & mem_nodes))
			continue;

		// Adding the memory node id in the global control group
		// setup string, which contains a comma-separated list of ids
		exc->cg_budget.cpuset_mems +=
			(exc->cg_budget.cpuset_mems == "")
			? std::to_string(mem_id)
			: "," + std::to_string(mem_id);
	}

	// Setting the global memory bandwidth as the global available one
	exc->cg_budget.memory_limit_bytes =
		(exc->resource_assignment[0]->mem_bandwidth)
		? std::to_string(exc->resource_assignment[0]->mem_bandwidth)
		: "";

	logger->Debug("New allocation: [PE shared - %s isolation - %s - bw %.2f "
		"- %.2f] [mem: %s - %s bytes]]",
		exc->cg_budget.cpuset_cpus_global.c_str(),
		exc->cg_budget.cpuset_cpus_isolation.c_str(),
		exc->cg_budget.cpu_budget_shared,
		exc->cg_budget.cpu_budget_isolation,
		(exc->cg_budget.cpuset_mems == "")
			? "unchanged" : exc->cg_budget.cpuset_mems.c_str(),
		exc->cg_budget.memory_limit_bytes.c_str());

	if (exc->cycles_count == 0) {
		// On first cycle, apply all the budget
		exc->cg_current_allocation.cpu_budget =
			exc->cg_budget.cpu_budget_shared;
		exc->cg_current_allocation.cpuset_cpus =
			exc->cg_budget.cpuset_cpus_global;
		exc->cg_current_allocation.cpuset_mems =
			exc->cg_budget.cpuset_mems;
		exc->cg_current_allocation.memory_limit_bytes =
			exc->cg_budget.memory_limit_bytes;

		exc->cg_current_allocation.cpu_affinity_mask =
			exc->cg_budget.cpu_global_ids;
	}

#endif

		STAT_LOG("APPLICATION:ALLOC_CHANGED");
		STAT_LOG("RESOURCE:PE_N %d",
			 exc->resource_assignment[0]->number_proc_elements);
		STAT_LOG("RESOURCE:PE_BANDWIDTH %d",
			 exc->resource_assignment[0]->cpu_bandwidth);
		STAT_LOG("RESOURCE:CPU_N %d",
			 exc->resource_assignment[0]->number_cpus);

		logger->Info("SyncP_1 (Pre-Change) EXC [%d], Action [%d], Assigned AWM [%d]",
					 msg.hdr.exc_id, msg.event, msg.awm);
		logger->Debug("SyncP_1 (Pre-Change) EXC [%d], Action [%d], Assigned PROC=<%d>",
					  msg.hdr.exc_id, msg.event, systems[0].r_proc);
	}
	else {
		logger->Info("SyncP_1 (Pre-Change) EXC [%d], Action [%d:BLOCKED]",
					 msg.hdr.exc_id, msg.event);
	}

	// FIXME add a string representation of the required action
	syncLatency = 0;

	if (! isAwmWaiting(exc) && exc->current_awm_stats) {
		// Update the Synchronziation Latency
		syncLatency = GetSyncLatency(exc);
	}

	exc_u_lock.unlock();
	logger->Debug("SyncP_1 (Pre-Change) EXC [%d], SyncLatency [%u]",
				  msg.hdr.exc_id, syncLatency);
	result = _SyncpPreChangeResp(msg.hdr.token, exc, syncLatency);
#ifndef CONFIG_BBQUE_YM_SYNC_FORCE

	if (result != RTLIB_OK)
		return result;

	// Force a DoChange, which will not be forwarded by the BBQ daemon if
	// the Sync Point forcing support is disabled
	logger->Info("SyncP_3 (Do-Change) EXC [%d]", msg.hdr.exc_id);
	return SyncP_DoChangeNotify(exc);
#else
	return result;
#endif
}

RTLIB_ExitCode_t BbqueRPC::SyncP_SyncChangeNotify(pRegisteredEXC_t exc)
{
	std::unique_lock<std::mutex> exc_u_lock(exc->exc_mutex);

	// Checking if the apps is in Sync Status
	if (! isAwmWaiting(exc))
		return RTLIB_EXC_SYNCP_FAILED;

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SyncP_SyncChangeNotify(
	rpc_msg_BBQ_SYNCP_SYNCCHANGE_t & msg)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(msg.hdr.exc_id);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	RTLIB_ExitCode_t result = SyncP_SyncChangeNotify(exc);

	if (result != RTLIB_OK) {
		logger->Warn("SyncP_2 (Sync-Change) EXC [%d] CRITICAL "
					 "(Warning: Overpassing Synchronization time)",
					 msg.hdr.exc_id);
	}

	logger->Info("SyncP_2 (Sync-Change) EXC [%d]",
				 msg.hdr.exc_id);
	_SyncpSyncChangeResp(msg.hdr.token, exc, result);
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SyncP_DoChangeNotify(pRegisteredEXC_t exc)
{
	std::unique_lock<std::mutex> exc_u_lock(exc->exc_mutex);

	// Update the EXC status based on the last required re-configuration action
	if (exc->event == RTLIB_EXC_GWM_BLOCKED) {
		setBlocked(exc);
	}
	else {
		clearBlocked(exc);
		setAwmAssigned(exc);
	}

	// TODO Setup the ground for reconfiguration statistics collection
	// TODO Start the re-configuration by notifying the waiting thread
	exc->exc_condition_variable.notify_one();
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SyncP_DoChangeNotify(
	rpc_msg_BBQ_SYNCP_DOCHANGE_t & msg)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(msg.hdr.exc_id);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	RTLIB_ExitCode_t result = SyncP_DoChangeNotify(exc);
	// NOTE this command should not generate a response, it is just a notification
	logger->Info("SyncP_3 (Do-Change) EXC [%d]", msg.hdr.exc_id);
	return result;
}

RTLIB_ExitCode_t BbqueRPC::SyncP_PostChangeNotify(pRegisteredEXC_t exc)
{
	// TODO Wait for the apps to end its reconfiguration
	// TODO Collect stats on reconfiguraiton time
	return WaitForSyncDone(exc);
}

RTLIB_ExitCode_t BbqueRPC::SyncP_PostChangeNotify(
	rpc_msg_BBQ_SYNCP_POSTCHANGE_t & msg)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(msg.hdr.exc_id);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	RTLIB_ExitCode_t result = SyncP_PostChangeNotify(exc);

	if (result != RTLIB_OK) {
		logger->Warn("SyncP_4 (Post-Change) EXC [%d] CRITICAL "
					 "(Warning: Reconfiguration timeout)",
					 msg.hdr.exc_id);
	}

	logger->Info("SyncP_4 (Post-Change) EXC [%d]", msg.hdr.exc_id);
	_SyncpPostChangeResp(msg.hdr.token, exc, result);

	return RTLIB_OK;
}

/******************************************************************************
 * Channel Independant interface
 ******************************************************************************/

RTLIB_ExitCode_t BbqueRPC::SetAWMConstraints(
	const RTLIB_EXCHandler_t exc_handler,
	RTLIB_Constraint_t * constraints,
	uint8_t count)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Calling the low-level enable function
	RTLIB_ExitCode_t result = _SetAWMConstraints(exc, constraints, count);

	if (result != RTLIB_OK) {
		logger->Error("Constraining EXC [%p:%s] FAILED (Error %d: %s)",
					  (void *) exc_handler, exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return RTLIB_EXC_ENABLE_FAILED;
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::ClearAWMConstraints(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Calling the low-level enable function
	RTLIB_ExitCode_t result = _ClearAWMConstraints(exc);

	if (result != RTLIB_OK) {
		logger->Error("Clear constraints for EXC [%p:%s] FAILED (Error %d: %s)",
					  (void *) exc_handler, exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return RTLIB_EXC_ENABLE_FAILED;
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::UpdateAllocation(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Init ggap info
	float goal_gap = 0.0f;
	exc->runtime_profiling.cpu_goal_gap = 0.0f;
	// If set to true, the profile will be forwarded
	exc->runtime_profiling.rtp_forward = false;

	// Check SKIP conditions ///////////////////////////////////////////////

	// Allocation is not changed if there are not enough samples to compute
	// meaningful statistics
	float cycletime_ic99 = exc->time_analyser_cycle.GetConfidenceInterval99();
	if (exc->time_analyser_cycle.GetWindowSize() == 0 || cycletime_ic99 == 0) {
		logger->Debug("UpdateAllocation: No samples to analyse. SKIPPING.");
		return RTLIB_OK;
	}

	// Compute Goal Gap ////////////////////////////////////////////////////
	if (exc->cps_goal_min + exc->cps_goal_max > 0.0f && ! exc->explicit_ggap_assertion) {
		// Milliseconds per cycle
		float avg_cycletime_ms = exc->time_analyser_cycle.GetMean();
		float min_cycletime_ms = avg_cycletime_ms - cycletime_ic99;
		float max_cycletime_ms = avg_cycletime_ms + cycletime_ic99;

		if (avg_cycletime_ms == 0.0f) {
			logger->Warn("Cycle time computation not available. Skipping.");
			return RTLIB_OK;
		}

		float cps_avg = 1000.0f / avg_cycletime_ms;
		float cps_min = 1000.0f / max_cycletime_ms;
		float cps_max = 1000.0f / min_cycletime_ms;

		STAT_LOG("PERFORMANCE:AVERAGE_CPS_SYSTEM %.2f", cps_avg);
		STAT_LOG("PERFORMANCE:AVERAGE_CPS_USER %.2f",
			 1000.0f / exc->time_analyser_usercycle.GetMean());

		float target_cps;
		float current_cps;
		bool  bad_allocation = false;

		if (exc->cps_goal_min > 0.0f && exc->cps_goal_max == 0.0f) {
			// There isn't a max CPS goal
			target_cps = exc->cps_goal_min;
			current_cps = cps_min;
			bad_allocation = current_cps < target_cps;
		} else if (exc->cps_goal_min == 0) {
			// There isn't a min CPS goal
			target_cps = exc->cps_goal_max;
			current_cps = cps_max;
			bad_allocation = current_cps > target_cps;
		} else {
			target_cps =
				0.5f * (exc->cps_goal_min + exc->cps_goal_max);
			current_cps = cps_avg;
			bad_allocation =
				(current_cps < exc->cps_goal_min) ||
				(current_cps > exc->cps_goal_max);
		}

		if (bad_allocation) {
			// Goal gap [%] = (real performance - ideal performance) / ideal performance
			goal_gap = (current_cps - target_cps) / target_cps;
			// Constraining gap to avoid harsh allocation changes:
			// Never request less than half the budget
			goal_gap = std::min(0.33f, goal_gap);
		}

		STAT_LOG("PERFORMANCE:PERFORMANCE_GAP %.2f", 100.0f * goal_gap);

	} else if (exc->explicit_ggap_assertion){
		goal_gap = exc->explicit_ggap_value / 100.0f;
		exc->explicit_ggap_assertion = false;
		exc->explicit_ggap_value = 0.0;
		logger->Debug("Performance goal gap (EXPLICIT) is %f", 100.0f * goal_gap);

		STAT_LOG("PERFORMANCE:PERFORMANCE_GAP_EXPLICIT %.2f", 100.0f * goal_gap);
	} else
		return RTLIB_OK;

#ifndef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
	// If CGroup handling happens at global resource manager level,
	// Just set the goal gap. It willb e forwarded to bbque and used
	// to change allocation.
	exc->runtime_profiling.cpu_goal_gap = 100.0f * goal_gap;
#else
	// Real CPU usage according to the statistical analysis
	float avg_cpu_usage = exc->cpu_usage_analyser.GetMean();
	float ideal_cpu_usage = avg_cpu_usage / (1.0f + goal_gap);


	// Use Goal Gap to change Allocation ///////////////////////////////////
	if (goal_gap != 0.0f) {
		if (ideal_cpu_usage == 0.0f) {
			logger->Debug("CPU quota computation not available. Skipping.");
			return RTLIB_OK;
		}

		// Current usage, to be compared later with the new one
		float curr_pe_usage = std::floor(exc->cg_current_allocation.cpu_budget);
		std::string curr_cpuset = exc->cg_current_allocation.cpuset_cpus;

		exc->cg_current_allocation.cpu_budget =
			std::min(ideal_cpu_usage / 100.0f, exc->cg_budget.cpu_budget_shared);
		exc->cg_current_allocation.cpuset_cpus =
			(exc->cg_current_allocation.cpu_budget <= exc->cg_budget.cpu_budget_isolation)
			? exc->cg_budget.cpuset_cpus_isolation
			: exc->cg_budget.cpuset_cpus_global;

                exc->cg_current_allocation.cpu_affinity_mask =
                        (exc->cg_current_allocation.cpu_budget <= exc->cg_budget.cpu_budget_isolation)
			? exc->cg_budget.cpu_isolation_ids
			: exc->cg_budget.cpu_global_ids;

		logger->Debug("Applying CGroup configuration: CPU %.2f/%.2f",
			100.0f * exc->cg_current_allocation.cpu_budget,
			100.0f * exc->cg_budget.cpu_budget_shared);

		// New usage, to be compared with the old one
		float next_pe_usage = std::floor(exc->cg_current_allocation.cpu_budget);
		std::string next_cpuset = exc->cg_current_allocation.cpuset_cpus;

		// If there is a difference in either cpuset or number of
		// exploitable pes, a new reconfiguration is needed.
		if (curr_pe_usage != next_pe_usage ||
				next_cpuset.compare(curr_cpuset) != 0) {
			exc->trigger_reconfigure = true;
			STAT_LOG("APPLICATION:RECONFIGURATION");
		}

		CGroupCommitAllocation(exc);
	}

	// Check if the current CPU bandwidth allocation is OK /////////////////

	// Relaxed resource release policy: unused resources will be retained
	// until the resource manager actually needs them (and sizes them back)
	if (exc->runtime_profiling.release_policy ==
			ResourceReleasePolicy::RELAXED && goal_gap >= 0.0f) {
		logger->Debug("Relaxed resource release policy: retaining "
				"unused CPU resources");
		return RTLIB_OK;
	}

	// Ideal allocation should be enough to always guarantee to the
	// application the needed CPU bandwidth. 99% of the time, the
	// application will require less than ideal_allocation_99 CPU
	// bandwidth. 90% of the time, the application will require less
	// than ideal_allocation_90 CPU bandwidth.
	// An allocation between ideal_allocation_90 and ideal_allocation_99
	// should be often enough for the application to run properly.
	float ideal_allocation_99 = ideal_cpu_usage +
			exc->cpu_usage_analyser.GetConfidenceInterval99();

	float ideal_allocation_90 = ideal_cpu_usage +
			exc->cpu_usage_analyser.GetConfidenceInterval90();

	if (ideal_allocation_90 == ideal_allocation_99) {
		logger->Warn("Not enough samples to compute a valid usage metric");
		return RTLIB_OK;
	}

	// Maximum CPU bandwidth as allocated by the BarbequeRTRM
	float cpu_usage_budget = 100.0f * exc->cg_budget.cpu_budget_shared;

	// If the application is satisfied with its performance, let us get rid
	// of the surplus CPU bandwidth
	if (goal_gap >= 0.0f && cpu_usage_budget > ideal_allocation_99) {
		logger->Debug("CPU budget (%.2f) too high: need only %.2f",
			cpu_usage_budget, ideal_allocation_99);
		exc->runtime_profiling.rtp_forward = true;
		exc->runtime_profiling.cpu_goal_gap = 100.0f *
			(cpu_usage_budget - ideal_allocation_99) / ideal_allocation_99;
	}
	// If the application is not satisfied with its performance, let us
	// ask for more CPU bandwidth
	else if (goal_gap < 0.0f && cpu_usage_budget < ideal_allocation_90) {
		logger->Debug("CPU budget (%.2f) too low: need at least %.2f",
			cpu_usage_budget, ideal_allocation_90);
		exc->runtime_profiling.rtp_forward = true;
		exc->runtime_profiling.cpu_goal_gap = 100.0f *
			(cpu_usage_budget - ideal_allocation_90) / ideal_allocation_90;
	}

	logger->Debug("CPU budget goal gap is %f",
			exc->runtime_profiling.cpu_goal_gap);

	// Skip checks on CPU budget goal gap //////////////////////////////////

	// Removing decimals. This number is already a percentage, and
	// ideal quota computation is not that accurate.
	exc->runtime_profiling.cpu_goal_gap =
		floorf(exc->runtime_profiling.cpu_goal_gap);

	if (exc->runtime_profiling.cpu_goal_gap == 0.0f) {
		exc->runtime_profiling.rtp_forward = false;
		logger->Debug("UpdateAllocation: zero gap, disabling RTP forward.");
		return RTLIB_OK;
	}

	// Constraining gap to avoid harsh allocation changes:
	// Never request less than half the budget
	exc->runtime_profiling.cpu_goal_gap =
		std::min(33.3f, exc->runtime_profiling.cpu_goal_gap);

	// In case of negative goal gaps, let's skip to forwarding
	if (exc->runtime_profiling.cpu_goal_gap < 0.0f)
		return RTLIB_OK;

	// Conservative resource release policy: unused resources will be
	// partially retained until the resource manager actually needs them
	// (and sizes them back)
	if (exc->runtime_profiling.release_policy
			== ResourceReleasePolicy::CONSERVATIVE
			&& exc->runtime_profiling.cpu_goal_gap < 5.0f) {
		logger->Debug("Conservative resource release policy: retaining "
				"some of the unused CPU resources");
		exc->runtime_profiling.rtp_forward = false;
	}
#endif

return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::ForwardRuntimeProfile(
	const RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Check SKIP conditions ///////////////////////////////////////////////////

	// Ggap computing is inhibited for some ms when the application is
	// assigned a new set of resources
	int ms_from_last_allocation = exc->time_analyser_usercycle.GetSum();

	if (ms_from_last_allocation <
		rtlib_configuration.runtime_profiling.rt_profile_rearm_time_ms
		&& exc->is_waiting_for_sync) {
		logger->Info("RTP forward SKIPPED (inhibited for %d more ms)",
					  rtlib_configuration.runtime_profiling.rt_profile_rearm_time_ms -
					  ms_from_last_allocation);
		return RTLIB_OK;
	}

	exc->is_waiting_for_sync = false;

	// Forward is inhibited for some ms after a RTP has been forwarded.
	exc->waiting_sync_timeout_ms -= exc->time_analyser_usercycle.GetLastValue();

	if (exc->waiting_sync_timeout_ms > 0) {
		logger->Info("RTP forward SKIPPED (waiting sync for %d more ms)",
					  exc->waiting_sync_timeout_ms);
		return RTLIB_OK;
	}

	// Real CPU usage according to the statistical analysis
#ifdef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
	float cpu_usage = 100.0f * exc->cg_budget.cpu_budget_shared;
#else
	float cpu_usage = 100.0f *  exc->resource_assignment[0]->cpu_bandwidth / 100.0f;
#endif
	float goal_gap = exc->runtime_profiling.cpu_goal_gap;
	float cycle_time_avg_ms = exc->time_analyser_cycle.GetMean() +
		exc->time_analyser_cycle.GetConfidenceInterval99();

	exc->waiting_sync_timeout_ms =
			rtlib_configuration.runtime_profiling.rt_profile_wait_for_sync_ms;
	exc->is_waiting_for_sync = true;

	logger->Debug("[%p:%s] Profile notification : {Gap: %.2f, CPU: "
				 "%.2f, CTime: %.2f ms}", (void *) exc_handler, exc->name.c_str(),
				 goal_gap, cpu_usage, cycle_time_avg_ms);
	// Forward the RTP
	RTLIB_ExitCode_t result =
		_RTNotify(exc, std::round(goal_gap), std::round(cpu_usage),
				  std::round(cycle_time_avg_ms));

	if (result != RTLIB_OK) {
		logger->Error("[%p:%s] Profile notification FAILED (Error %d: %s)",
					  (void *) exc_handler, exc->name.c_str(), result, RTLIB_ErrorStr(result));
		return RTLIB_EXC_ENABLE_FAILED;
	}

	STAT_LOG("PERFORMANCE:RTPROFILE_FORWARD:CPU_GAP %.2f", goal_gap);
	STAT_LOG("PERFORMANCE:RTPROFILE_FORWARD:CPU_USAGE %.2f", cpu_usage);

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SetExplicitGoalGap(
	const RTLIB_EXCHandler_t exc_handler,
	int ggap)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	exc->explicit_ggap_assertion = true;
	exc->explicit_ggap_value = ggap;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::StopExecution(
	RTLIB_EXCHandler_t exc_handler,
	struct timespec timeout)
{
	//Silence "args not used" warning.
	(void) exc_handler;
	(void) timeout;
	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SendOCLRuntimeProfile(
	rpc_msg_BBQ_GET_PROFILE_t & msg)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(msg.hdr.exc_id);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Check the application is not in sync
	if (isSyncMode(exc))
		return RTLIB_OK;

	// Only OpenCL profiling actually supported
	if (! msg.is_ocl)
		return RTLIB_OK;

#ifdef CONFIG_BBQUE_OPENCL
	uint32_t exec_time, mem_time;
	OclGetRuntimeProfile(exc, exec_time, mem_time);
	// Send the profile to the resource manager
	_GetRuntimeProfileResp(msg.hdr.token, exc, exec_time, mem_time);
#endif
	return RTLIB_OK;
}

/*******************************************************************************
 *    Performance Monitoring Support
 ******************************************************************************/
#ifdef CONFIG_BBQUE_RTLIB_PERF_SUPPORT

BbqueRPC::PerfEventAttr_t * BbqueRPC::raw_events = nullptr;

BbqueRPC::PerfEventAttr_t BbqueRPC::default_events[] = {

	{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_TASK_CLOCK },
	{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES },
	{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_MIGRATIONS },
	{PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS },

	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES	},
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND },
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND },
#endif
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS },
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS },
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES },

};


BbqueRPC::PerfEventAttr_t BbqueRPC::detailed_events[] = {

	{PERF_TYPE_HW_CACHE, L1DC_RA },
	{PERF_TYPE_HW_CACHE, L1DC_RM },
	{PERF_TYPE_HW_CACHE, LLC_RA },
	{PERF_TYPE_HW_CACHE, LLC_RM },

};


BbqueRPC::PerfEventAttr_t BbqueRPC::very_detailed_events[] = {

	{PERF_TYPE_HW_CACHE,	L1IC_RA },
	{PERF_TYPE_HW_CACHE,	L1IC_RM },
	{PERF_TYPE_HW_CACHE,	DTLB_RA },
	{PERF_TYPE_HW_CACHE,	DTLB_RM },
	{PERF_TYPE_HW_CACHE,	ITLB_RA },
	{PERF_TYPE_HW_CACHE,	ITLB_RM },

};

BbqueRPC::PerfEventAttr_t BbqueRPC::very_very_detailed_events[] = {

	{PERF_TYPE_HW_CACHE,	L1DC_PA },
	{PERF_TYPE_HW_CACHE,	L1DC_PM },

};

static const char * _perfCounterName = 0;

uint8_t BbqueRPC::InsertRAWPerfCounter(const char * perf_str)
{
	static uint8_t idx = 0;
	uint64_t event_code_ul;
	char event_code_str[4];
	char label[10];
	char buff[15];

	// Overflow check
	if (idx == rtlib_configuration.profile.perf_counters.raw)
		return idx;

	// Extract label and event select code + unit mask
	strncpy(buff, perf_str, sizeof (buff));
	strncpy(strpbrk(buff, "-"), " ", 1);
	sscanf(buff, "%10s %4s", label, event_code_str);

	// Nullptr check
	if ((label == nullptr) || (event_code_str == nullptr))
		return 0;

	// Convert the event code from string to unsigned long
	event_code_ul = strtoul(event_code_str, NULL, 16);

	// Allocate the raw events array
	if (! raw_events)
		raw_events = (PerfEventAttr_t *)
					 malloc(sizeof (PerfEventAttr_t) *
							rtlib_configuration.profile.perf_counters.raw);

	// Set the event attributes
	raw_events[idx ++] = {PERF_TYPE_RAW, event_code_ul};
	return idx;
}

BbqueRPC::pPerfEventStats_t BbqueRPC::PerfGetEventStats(pAwmStats_t awm_stats,
		perf_type_id type, uint64_t config)
{
	PerfEventStatsMapByConf_t::iterator it;
	pPerfEventStats_t event_stats;
	uint8_t conf_key, curr_key;
	conf_key = (uint8_t) (0xFF & config);
	// LookUp for requested event
	it = awm_stats->events_conf_map.lower_bound(conf_key);

	for ( ; it != awm_stats->events_conf_map.end(); ++ it) {
		event_stats = (*it).second;
		// Check if current (conf) key is still valid
		curr_key = (uint8_t) (0xFF & event_stats->pattr->config);

		if (curr_key != conf_key)
			break;

		// Check if we found the required event
		if ((event_stats->pattr->type == type) &&
			(event_stats->pattr->config == config))
			return event_stats;
	}

	return pPerfEventStats_t();
}

void BbqueRPC::PerfSetupEvents(pRegisteredEXC_t exc)
{
	uint8_t tot_counters = ARRAY_SIZE(default_events);
	int fd;

	// TODO get required Perf configuration from environment
	// to add eventually more detailed counters or to completely disable perf
	// support

	// Adding raw events
	for (uint8_t e = 0; e < rtlib_configuration.profile.perf_counters.raw; e ++) {
		fd = exc->perf.AddCounter(
				 PERF_TYPE_RAW, raw_events[e].config,
				 rtlib_configuration.profile.perf_counters.no_kernel);
		exc->events_map[fd] = & (raw_events[e]);
	}

	// RAW events mode skip the preset counters
	if (rtlib_configuration.profile.perf_counters.raw > 0)
		return;

	// Adding default events
	for (uint8_t e = 0; e < tot_counters; e ++) {
		fd = exc->perf.AddCounter(
				 default_events[e].type, default_events[e].config,
				 rtlib_configuration.profile.perf_counters.no_kernel);
		exc->events_map[fd] = & (default_events[e]);
	}

	if (rtlib_configuration.profile.perf_counters.detailed_run <  1)
		return;

	// Append detailed run extra attributes
	for (uint8_t e = 0; e < ARRAY_SIZE(detailed_events); e ++) {
		fd = exc->perf.AddCounter(
				 detailed_events[e].type, detailed_events[e].config,
				 rtlib_configuration.profile.perf_counters.no_kernel);
		exc->events_map[fd] = & (detailed_events[e]);
	}

	if (rtlib_configuration.profile.perf_counters.detailed_run <  2)
		return;

	// Append detailed run extra attributes
	for (uint8_t e = 0; e < ARRAY_SIZE(very_detailed_events); e ++) {
		fd = exc->perf.AddCounter(
				 very_detailed_events[e].type, very_detailed_events[e].config,
				 rtlib_configuration.profile.perf_counters.no_kernel);
		exc->events_map[fd] = & (very_detailed_events[e]);
	}

	if (rtlib_configuration.profile.perf_counters.detailed_run <  3)
		return;

	// Append detailed run extra attributes
	for (uint8_t e = 0; e < ARRAY_SIZE(very_very_detailed_events); e ++) {
		fd = exc->perf.AddCounter(
				 very_very_detailed_events[e].type, very_very_detailed_events[e].config,
				 rtlib_configuration.profile.perf_counters.no_kernel);
		exc->events_map[fd] = & (very_very_detailed_events[e]);
	}
}

void BbqueRPC::PerfSetupStats(pRegisteredEXC_t exc, pAwmStats_t awm_stats)
{
	pPerfEventStats_t event_stats; // Statistics: value, sampling time, etc...
	pPerfEventAttr_t  event_attributes; // Description: type, etc...
	uint8_t           configuration_key;
	int               event_id;

	// Check if there is at least one event to monitor
	if (PerfRegisteredEvents(exc) == 0)
		return;

	// Setup statistics for registered performance counters
	for (auto & perf_event : exc->events_map) {
		event_id = perf_event.first;
		event_attributes = perf_event.second;
		// Build new perf counter statistics
		event_stats = std::make_shared<PerfEventStats_t>();
		assert(event_stats);
		event_stats->id = event_id;
		event_stats->pattr = event_attributes;
		// Keep track of perf statistics for this AWM
		awm_stats->events_map[event_id] = event_stats;
		// Index statistics by configuration (radix)
		configuration_key = (uint8_t) (0xFF & event_attributes->config);
		awm_stats->events_conf_map.insert(
			PerfEventStatsMapByConfEntry_t(configuration_key, event_stats));
	}
}

void BbqueRPC::PerfCollectStats(pRegisteredEXC_t exc)
{
	std::unique_lock<std::mutex> stats_lock(exc->current_awm_stats->stats_mutex);
	pAwmStats_t       awm_stats = exc->current_awm_stats;
	pPerfEventStats_t event_stats;
	uint64_t          increase_from_last_sampling;
	int               event_id;

	// Collect counters for registered events
	for (auto & event_counter : awm_stats->events_map) {
		event_stats = event_counter.second;
		event_id = event_counter.first;
		// Reading increase_from_last_sampling for this perf counter
		increase_from_last_sampling = exc->perf.Update(event_id);
		// Computing stats for this counter
		event_stats->value += increase_from_last_sampling;
		event_stats->perf_samples(increase_from_last_sampling);
	}
}

void BbqueRPC::PerfPrintNsec(pAwmStats_t awm_stats,
							 pPerfEventStats_t event_stats)
{
	pPerfEventAttr_t event_attributes = event_stats->pattr;
	double avg = mean(event_stats->perf_samples);
	double total, ratio = 0.0;
	double msecs = avg / 1e6;
	fprintf(output_file, "%19.6f%s%-25s", msecs,
			rtlib_configuration.profile.output.CSV.separator,
			bu::Perf::EventName(event_attributes->type, event_attributes->config));

	if (rtlib_configuration.profile.output.CSV.enabled)
		return;

	if (PerfEventMatch(event_attributes, PERF_SW(TASK_CLOCK))) {
		// Get AWM average running time
		total = mean(awm_stats->cycle_samples) * 1e6;

		if (total) {
			ratio = avg / total;
			fprintf(output_file, " # %8.3f CPUs utilized          ", ratio);
			return;
		}
	}
}

void BbqueRPC::PerfPrintAbs(pAwmStats_t awm_stats,
							pPerfEventStats_t event_stats)
{
	pPerfEventAttr_t event_attributes = event_stats->pattr;
	double avg = mean(event_stats->perf_samples);
	double total, total2, ratio = 0.0;
	pPerfEventStats_t event_stats2;
	const char * fmt;
	// shutdown compiler warnings for kernels < 3.1
	(void) total2;

	if (rtlib_configuration.profile.output.CSV.enabled)
		fmt = "%.0f%s%s";
	else if (rtlib_configuration.profile.perf_counters.big_num)
		fmt = "%'19.0f%s%-25s";
	else
		fmt = "%19.0f%s%-25s";

	fprintf(output_file, fmt, avg, rtlib_configuration.profile.output.CSV.separator,
			bu::Perf::EventName(event_attributes->type, event_attributes->config));

	if (rtlib_configuration.profile.output.CSV.enabled)
		return;

	if (PerfEventMatch(event_attributes, PERF_HW(INSTRUCTIONS))) {
		// Compute "instructions per cycle"
		event_stats2 = PerfGetEventStats(awm_stats, PERF_HW(CPU_CYCLES));

		if (! event_stats2)
			return;

		total = mean(event_stats2->perf_samples);

		if (total)
			ratio = avg / total;

		fprintf(output_file, " #   %5.2f  insns per cycle        ", ratio);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
		// Compute "stalled cycles per instruction"
		event_stats2 = PerfGetEventStats(awm_stats, PERF_HW(STALLED_CYCLES_FRONTEND));

		if (! event_stats2)
			return;

		total = mean(event_stats2->perf_samples);
		event_stats2 = PerfGetEventStats(awm_stats, PERF_HW(STALLED_CYCLES_BACKEND));

		if (! event_stats2)
			return;

		total2 = mean(event_stats2->perf_samples);

		if (total < total2)
			total = total2;

		if (total && avg) {
			ratio = total / avg;
			fprintf(output_file, "\n%45s#   %5.2f  stalled cycles per insn", " ", ratio);
		}

#endif
		return;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	// TODO add CPU front/back-end stall stats
#endif

	if (PerfEventMatch(event_attributes, PERF_HW(CPU_CYCLES))) {
		event_stats2 = PerfGetEventStats(awm_stats, PERF_SW(TASK_CLOCK));

		if (! event_stats2)
			return;

		total = mean(event_stats2->perf_samples);

		if (total) {
			ratio = 1.0 * avg / total;
			fprintf(output_file, " # %8.3f GHz                    ", ratio);
		}

		return;
	}

	// By default print the frequency of the event in [M/sec]
	event_stats2 = PerfGetEventStats(awm_stats, PERF_SW(TASK_CLOCK));

	if (! event_stats2)
		return;

	total = mean(event_stats2->perf_samples);

	if (total) {
		ratio = 1000.0 * avg / total;
		fprintf(output_file, " # %8.3f M/sec                  ", ratio);
		return;
	}

	// Otherwise, simply generate an empty line
	fprintf(output_file, "%-35s", " ");
}

bool BbqueRPC::IsNsecCounter(pRegisteredEXC_t exc, int fd)
{
	pPerfEventAttr_t event_attributes = exc->events_map[fd];

	if (PerfEventMatch(event_attributes, PERF_SW(CPU_CLOCK)) ||
		PerfEventMatch(event_attributes, PERF_SW(TASK_CLOCK)))
		return true;

	return false;
}

void BbqueRPC::PerfPrintStats(pRegisteredEXC_t exc, pAwmStats_t awm_stats)
{
	PerfEventStatsMap_t::iterator it;
	pPerfEventStats_t event_stats;
	pPerfEventAttr_t event_attributes;
	uint64_t avg_enabled, avg_running;
	double avg_value, std_value;
	int fd;

	// For each registered counter
	for (it = awm_stats->events_map.begin(); it != awm_stats->events_map.end();
		 ++ it) {
		event_stats = (*it).second;
		fd = (*it).first;
		// Keep track of current Performance Counter name
		event_attributes = exc->events_map[fd];
		_perfCounterName = bu::Perf::EventName(event_attributes->type,
											   event_attributes->config);

		if (IsNsecCounter(exc, fd))
			PerfPrintNsec(awm_stats, event_stats);
		else
			PerfPrintAbs(awm_stats, event_stats);

		// Print stddev ratio
		if (count(event_stats->perf_samples) > 1) {
			// Get AWM average and stddev running time
			avg_value = mean(event_stats->perf_samples);
			std_value = sqrt(static_cast<double> (variance(event_stats->perf_samples)));
			PrintNoisePct(std_value, avg_value);
		}

		// Computing counter scheduling statistics
		avg_enabled = exc->perf.Enabled(event_stats->id, false);
		avg_running = exc->perf.Running(event_stats->id, false);

		// Print percentage of counter usage
		if (avg_enabled != avg_running) {
			fprintf(output_file, " [%5.2f%%]", 100.0 * avg_running / avg_enabled);
		}

		fputc('\n', output_file);
	}

	if (! rtlib_configuration.profile.output.CSV.enabled) {
		fputc('\n', output_file);
		// Get AWM average and stddev running time
		avg_value = mean(awm_stats->cycle_samples);
		std_value = sqrt(static_cast<double> (variance(awm_stats->cycle_samples)));
		fprintf(output_file, " %18.6f cycle time [ms]", avg_value);

		if (count(awm_stats->cycle_samples) > 1) {
			fprintf(output_file, "                                        ");
			PrintNoisePct(std_value, avg_value);
		}

		fprintf(output_file, "\n\n");
	}
}

void BbqueRPC::PrintNoisePct(double total, double avg)
{
	const char * color;
	double pct = 0.0;

	if (avg)
		pct = 100.0 * total / avg;

	if (rtlib_configuration.profile.output.CSV.enabled) {
		fprintf(output_file, "%s%.2f%%",
				rtlib_configuration.profile.output.CSV.separator, pct);
		return;
	}

	color = PERF_COLOR_NORMAL;

	if (pct > 80.0)
		color = PERF_COLOR_RED;
	else if (pct > 60.0)
		color = PERF_COLOR_MAGENTA;
	else if (pct > 40.0)
		color = PERF_COLOR_YELLOW;

	fprintf(output_file, "  ( ");
	bu::Perf::FPrintf(output_file, color, "+-%6.2f%%", pct);
	fprintf(output_file, " )");
}

#endif // CONFIG_BBQUE_RTLIB_PERF_SUPPORT

/*******************************************************************************
 *    OpenCL support
 ******************************************************************************/
#ifdef CONFIG_BBQUE_OPENCL

#define SUM(v) \
	sum(it_ct->second[CL_CMD_ ## v ## _TIME])*1e-06
#define MIN(v) \
	min(it_ct->second[CL_CMD_ ## v ## _TIME])*1e-06
#define MAX(v) \
	max(it_ct->second[CL_CMD_ ## v ## _TIME])*1e-06
#define MEAN(v) \
	mean(it_ct->second[CL_CMD_ ## v ## _TIME])*1e-06
#define STDDEV(v) \
	sqrt(static_cast<double>(variance(it_ct->second[CL_CMD_ ## v ## _TIME])))*1e-06

void BbqueRPC::OclSetDevice(uint8_t device_id, RTLIB_ExitCode_t status)
{
	rtlib_ocl_set_device(device_id, status);
}

void BbqueRPC::OclClearStats()
{
	rtlib_ocl_prof_clean();
}

void BbqueRPC::OclCollectStats(int8_t current_awm_id, OclEventsStatsMap_t & ocl_events_map)
{
	rtlib_ocl_prof_run(
		current_awm_id, ocl_events_map, rtlib_configuration.profile.opencl.level);
}


#define OCL_STATS_HEADER \
"#           Command Queue          ||      Command Type       ||                 queue[ms]                   ||                 submit[ms]                  ||                     exec[ms]                ||\n"\
"# ---------------------------------++-------------------------++---------------------------------------------++---------------------------------------------++---------------------------------------------||\n"\
"#                                  ||                         ||        Ʃ (%%h %%v)      |     μ    |     σ    ||        Ʃ (%%h %%v)      |     μ    |     σ    ||        Ʃ (%%h %%v)      |     μ    |     σ    ||\n"\
"# ---------------------------------++-------------------------++-----------------------+----------+----------++-----------------------+----------+----------++-----------------------+----------+----------||\n"

#define OCL_STATS_HEADER_ADDR \
"#   Command Queue  || Code Address ||      Command Type       ||             queue[ms]          ||            submit[ms]          ||             exec[ms]           ||\n"\
"# -----------------++--------------++-------------------------++--------------------------------++--------------------------------++--------------------------------||\n"\
"#                  ||              ||                         ||    Ʃ     |    μ     |     σ    ||    Ʃ     |     μ    |    σ     ||     Ʃ    |     μ    |     σ    ||\n"\
"# -----------------++--------------++-------------------------++----------+----------+----------++----------+----------+----------++----------+----------+----------||\n"

#define OCL_EXC_AWM_HEADER \
"##=========================================================================================================================================================================================================##\n" \
"## %100s-%-99d##\n" \
"##=========================================================================================================================================================================================================##\n"

#define OCL_STATS_BAR \
"#==========================================================================================================================================================================================================##\n"

#define OCL_STATS_BAR_ADDR \
"#===================================================================================================================================================================##\n"

void BbqueRPC::OclDumpStats(pRegisteredEXC_t exc)
{
	AwmStatsMap_t::iterator it;
	pAwmStats_t awm_stats;
	int8_t current_awm_id;
	// Print RTLib stats for each AWM
	it = exc->awm_stats.begin();

	for ( ; it != exc->awm_stats.end(); ++ it) {
		current_awm_id = (*it).first;
		awm_stats = (*it).second;
		std::map<cl_command_queue, QueueProfPtr_t>::iterator it_cq;
		fprintf(output_file, OCL_EXC_AWM_HEADER, exc->name.c_str(), current_awm_id);
		fprintf(output_file, OCL_STATS_BAR);
		fprintf(output_file, OCL_STATS_HEADER);

		for (it_cq = awm_stats->ocl_events_map.begin();
			 it_cq != awm_stats->ocl_events_map.end(); it_cq ++) {
			QueueProfPtr_t stPtr = it_cq->second;
			OclDumpCmdStats(stPtr, it_cq->first);

			if (rtlib_configuration.profile.opencl.level == 0)
				continue;

			OclDumpAddrStats(stPtr, it_cq->first);
		}
	}

	fprintf(output_file, "\n\n");
}

void BbqueRPC::OclDumpCmdStats(QueueProfPtr_t stPtr, cl_command_queue cmd_queue)
{
	std::map<cl_command_type, AccArray_t>::iterator it_ct;
	double_t otot = 0, vtot_q = 0, vtot_s = 0, vtot_e = 0;

	for (it_ct = stPtr->cmd_prof.begin(); it_ct != stPtr->cmd_prof.end();
			it_ct ++) {
		vtot_q += SUM(QUEUED);
		vtot_s += SUM(SUBMIT);
		vtot_e += SUM(EXEC);
	}

	for (it_ct = stPtr->cmd_prof.begin(); it_ct != stPtr->cmd_prof.end();
		 it_ct ++) {
		otot = SUM(QUEUED) + SUM(SUBMIT) + SUM(EXEC);
		fprintf(output_file, "# %-32p || %-23s || "
				"%7.3f ( %5.2f %5.2f ) | %8.3f | %8.3f || "
				"%7.3f ( %5.2f %5.2f ) | %8.3f | %8.3f || "
				"%7.3f ( %5.2f %5.2f ) | %8.3f | %8.3f ||\n",
				(void *) cmd_queue, ocl_cmd_str[it_ct->first].c_str(),
				SUM(QUEUED), (100 * SUM(QUEUED)) / otot, (100 * SUM(QUEUED)) / vtot_q,
				MEAN(QUEUED), STDDEV(QUEUED),
				SUM(SUBMIT), (100 * SUM(SUBMIT)) / otot, (100 * SUM(SUBMIT)) / vtot_s,
				MEAN(SUBMIT), STDDEV(SUBMIT),
				SUM(EXEC), (100 * SUM(EXEC)) / otot, (100 * SUM(EXEC)) / vtot_e,
				MEAN(EXEC), STDDEV(EXEC));
	}

	fprintf(output_file, OCL_STATS_BAR);
}

void BbqueRPC::OclDumpAddrStats(QueueProfPtr_t stPtr, cl_command_queue cmd_queue)
{
	std::map<void *, AccArray_t>::iterator it_ct;
	cl_command_type cmd_type;
	fprintf(output_file, OCL_STATS_BAR_ADDR);
	fprintf(output_file, OCL_STATS_HEADER_ADDR);

	for (it_ct = stPtr->addr_prof.begin(); it_ct != stPtr->addr_prof.end();
		 it_ct ++) {
		cmd_type = rtlib_ocl_get_command_type(it_ct->first);
		fprintf(output_file, "# %-16p || %-12p || %-23s || "
				"%8.3f | %8.3f | %8.3f || "
				"%8.3f | %8.3f | %8.3f || "
				"%8.3f | %8.3f | %8.3f ||\n",
				(void *) cmd_queue, it_ct->first, ocl_cmd_str[cmd_type].c_str(),
				SUM(QUEUED), MEAN(QUEUED), STDDEV(QUEUED),
				SUM(SUBMIT), MEAN(SUBMIT), STDDEV(SUBMIT),
				SUM(EXEC), MEAN(EXEC), STDDEV(EXEC));
	}

	fprintf(output_file, OCL_STATS_BAR_ADDR);
}


#endif // CONFIG_BBQUE_OPENCL

/*******************************************************************************
 *    Utility Functions
 ******************************************************************************/

AppUid_t BbqueRPC::GetUniqueID(RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	return ((channel_thread_pid << BBQUE_UID_SHIFT) + exc->id);
}

/*******************************************************************************
 *    Cycles Per Second (CPS) Control Support
 ******************************************************************************/

RTLIB_ExitCode_t BbqueRPC::SetCPS(
	RTLIB_EXCHandler_t exc_handler,
	float cps)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Keep track of the maximum required CPS
	exc->cps_max_allowed = cps;
	exc->cycle_time_enforced_ms = 0.0f;

	if (exc->cps_max_allowed != 0.0f) {
		exc->cycle_time_enforced_ms = static_cast<float> (1e3) / exc->cps_max_allowed;
	}

	logger->Notice("Set max cycle-rate @ %.3f[Hz] (min %.3f[ms])",
				   exc->cps_max_allowed, exc->cycle_time_enforced_ms);
	return RTLIB_OK;
}

float BbqueRPC::GetCPS(
	RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	float ctime = 0;
	float cps = 0;

	// If cycle was reset, return CPS up to last forward window
	if (exc->time_analyser_usercycle.GetMean() == 0.0)
		return (exc->average_cycletime_pre_reset_ms == 0.0) ?
			   0.0 : 1000.0 / exc->average_cycletime_pre_reset_ms;

	// Get the current measured CPS
	ctime = exc->time_analyser_usercycle.GetMean();

	if (ctime != 0)
		cps = 1000.0 / ctime;

	return cps;
}

float BbqueRPC::GetJPS(
	RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	return GetCPS(exc_handler) * exc->jpc;
}

void BbqueRPC::ForceCPS(pRegisteredEXC_t exc)
{
	// Compute last cycle run time
	float real_cycle_time = exc->execution_timer.getElapsedTimeMs();
	float delay_ms = exc->cycle_time_enforced_ms - real_cycle_time;

	// Enforce CPS if needed
	if (delay_ms > 0.0f) {
		uint32_t sleep_us = 1e3 * static_cast<uint32_t> (delay_ms);
		logger->Debug("Cycle Time: %3.3f[ms], ET: %3.3f[ms], Sleep time %u [us]",
			real_cycle_time, exc->cycle_time_enforced_ms, sleep_us);
		usleep(sleep_us);
	}
}

RTLIB_ExitCode_t BbqueRPC::SetCPSGoal(
	RTLIB_EXCHandler_t exc_handler, float cps_min, float cps_max)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	// Keep track of the maximum required CPS
	exc->cps_goal_min = cps_min;
	exc->cps_goal_max = cps_max;

#ifdef CONFIG_RTLIB_DA_ADAPTIVE_CFS_PERIOD
	// Adapting cfs period to average cycle time, which in turn equals to
	// average_cps_goal^(-1).
	// NOTE: libcgroup does not support period lengths greater than 1 sec
	// (i.e. CPS must be >= 1.0)
	float average_cps_goal = std::max(1.0f, cps_max);

	// cpu.cfs_period must be integer and expressed in microseconds
	uint32_t average_cycletime_us = 1e6 * (1.0f / average_cps_goal);

	logger->Info("cpu.cfs_period_us adapted to %u us (%.2f periods / sec)",
		average_cycletime_us, average_cps_goal);
	exc->cgroup_cpu_cfs_period_us = average_cycletime_us;
#endif // CONFIG_RTLIB_DA_ADAPTIVE_CFS_PERIOD

	STAT_LOG("PERFORMANCE:CPSGOAL_MIN %.2f", exc->cps_goal_min);
	STAT_LOG("PERFORMANCE:CPSGOAL_MAX %.2f", exc->cps_goal_max);

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::UpdateJPC(
	RTLIB_EXCHandler_t exc_handler, int jpc)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	if (jpc == 0) {
		logger->Error("UpdateJPC: invalid args");
		return RTLIB_ERROR;
	}


	if (exc->jpc != jpc) {
		float correction_factor = (float) (exc->jpc) / (float) jpc;
		float new_cps_min = exc->cps_goal_min * correction_factor;
		float new_cps_max = exc->cps_goal_max * correction_factor;
		exc->jpc = jpc;
		return SetCPSGoal(exc_handler, new_cps_min, new_cps_max);
	}

	return RTLIB_OK;
}

RTLIB_ExitCode_t BbqueRPC::SetJPSGoal(
	RTLIB_EXCHandler_t exc_handler,
	float jps_min, float jps_max, int jpc)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	if (jpc == 0) {
		logger->Error("SetJPSGoal: JPC cannot be null");
		return RTLIB_ERROR;
	}

	if (jps_max == 0.0) {
		logger->Error("SetJPSGoal: invalid args (JPS Goal not set)");
		return RTLIB_ERROR;
	}

	if (jps_min > jps_max)
		jps_max = jps_min;


	exc->jpc = jpc;

	return SetCPSGoal(exc_handler, jps_min / jpc, jps_max / jpc);
}

float BbqueRPC::GetLastValueMs(RTLIB_EXCHandler_t exc_handler,
		RTLIB_ExecPhaseType aem_phase) {

	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	switch (aem_phase) {
		case RTLIB_ExecPhaseType::CONFIGURE:
			return exc->time_analyser_configure.GetLastValue();
		case RTLIB_ExecPhaseType::MONITOR:
			return exc->time_analyser_monitor.GetLastValue();
		case RTLIB_ExecPhaseType::RUN:
			return exc->time_analyser_run.GetLastValue();
		default:
			return exc->time_analyser_usercycle.GetLastValue();
	}
}

float BbqueRPC::GetAverageValueMs(RTLIB_EXCHandler_t exc_handler,
		RTLIB_ExecPhaseType aem_phase) {

	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return RTLIB_EXC_NOT_REGISTERED;

	switch (aem_phase) {
		case RTLIB_ExecPhaseType::CONFIGURE:
			return exc->time_analyser_configure.GetMean();
		case RTLIB_ExecPhaseType::MONITOR:
			return exc->time_analyser_monitor.GetMean();
		case RTLIB_ExecPhaseType::RUN:
			return exc->time_analyser_run.GetMean();
		default:
			return exc->time_analyser_usercycle.GetMean();
	}
}

/*******************************************************************************
 *    RTLib Notifiers Support
 ******************************************************************************/

void BbqueRPC::NotifyExit(
	RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	bool pcounters_collected_systemwide =
		rtlib_configuration.profile.perf_counters.global;
	bool pcounters_to_be_monitored = PerfRegisteredEvents(exc);

	if (pcounters_collected_systemwide && pcounters_to_be_monitored > 0) {
		PerfDisable(exc);
		PerfCollectStats(exc);
	}
}

void BbqueRPC::NotifyPreConfigure(
	RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	// Execution Cycle (configure + run + monitor) of the EXC starts here
	exc->execution_timer.start();
	// Configure phase of the EXC (get Working Mode, resume if needed and
	// configure accordingly) starts here
	exc->configure_tstart_ms = exc->execution_timer.getElapsedTimeMs();

#ifdef CONFIG_BBQUE_OPENCL
	pSystemResources_t local_sys(exc->resource_assignment[0]);
	assert(local_sys != nullptr);
	logger->Debug("NotifyPreConfigure - OCL Device: %d", local_sys->ocl_device_id);
	OclSetDevice(local_sys->ocl_device_id, exc->event);
#endif

	if(exc->cycles_count == 0)
		STAT_LOG("APPLICATION:EXC_START");

#ifndef CONFIG_RTLIB_DA_LOCAL_CGROUP_WRITE
	STAT_LOG("APPLICATION:RECONFIGURATION");
#endif

	logger->Debug("===> NotifyConfigure");
}

void BbqueRPC::NotifyPostConfigure(
	RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	logger->Debug("<=== NotifyConfigure");

#ifdef CONFIG_BBQUE_OPENCL
	// Clear pre-run OpenCL command events
	OclClearStats();
#endif

	// Resetting Runtime Statistics counters
	if (exc->cycles_count == 0) {
		logger->Debug("First cycle: applying all resource budget.");
		CGroupCommitAllocation(exc);
	}

	// Update total and AWM-wise time statistics
	pAwmStats_t awm_stats(exc->current_awm_stats);
	double configure_time_ms =
	    exc->execution_timer.getElapsedTimeMs() - exc->configure_tstart_ms;
	exc->config_time_ms += configure_time_ms;
	exc->time_analyser_configure.InsertValue(configure_time_ms);
	awm_stats->time_spent_configuring += configure_time_ms;
	awm_stats->config_samples(configure_time_ms);
}

void BbqueRPC::TogglePerfCountersPreCycle(BbqueRPC::pRegisteredEXC_t exc) {
	logger->Debug("Pre-Run: Checking if perf counters are activated");
	bool pcounters_collected_systemwide =
		rtlib_configuration.profile.perf_counters.global;
	bool pcounters_to_be_monitored = PerfRegisteredEvents(exc);

	if (! pcounters_collected_systemwide && pcounters_to_be_monitored > 0) {
		logger->Debug("Pre-Run: per-EXC perf counter support: ACTIVE");
		bool pcounters_monitor_rtlib_overheads =
			rtlib_configuration.profile.perf_counters.overheads;

		if (unlikely(pcounters_monitor_rtlib_overheads)) {
			logger->Debug("Pre-Run: RTLIB overheads mode: disabling perf");
			PerfDisable(exc);
			PerfCollectStats(exc);
		}
		else {
			logger->Debug("Pre-Run: standard profiling mode: enabling perf");
			PerfEnable(exc);
		}
	}
}

void BbqueRPC::TogglePerfCountersPostCycle(BbqueRPC::pRegisteredEXC_t exc) {
	logger->Debug("Post-Run: Checking if perf counters are activated");
	bool pcounters_collected_systemwide =
		rtlib_configuration.profile.perf_counters.global;
	bool pcounters_to_be_monitored = PerfRegisteredEvents(exc);

	if (! pcounters_collected_systemwide && pcounters_to_be_monitored > 0) {
		logger->Debug("Post-Run: per-EXC perf counter support: ACTIVE");
		bool pcounters_monitor_rtlib_overheads =
			rtlib_configuration.profile.perf_counters.overheads;

		if (unlikely(pcounters_monitor_rtlib_overheads)) {
			logger->Debug("Post-Run: RTLIB overheads mode: enabling perf");
			PerfEnable(exc);
		}
		else {
			logger->Debug("Post-Run: standard profiling mode: disabling perf");
			PerfDisable(exc);
			PerfCollectStats(exc);
		}
	}
}

void BbqueRPC::NotifyPreRun(RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	// Run phase of the EXC starts here
	exc->run_tstart_ms = exc->execution_timer.getElapsedTimeMs();

	// Update start latency
	if (unlikely(exc->starting_time_ms == 0))
		exc->starting_time_ms = exc->run_tstart_ms;

	TogglePerfCountersPreCycle(exc);

	logger->Debug("Pre-Run: Starting computing CPU quota");
	STAT_LOG("APPLICATION:CYCLE_START %d", exc->cycles_count);

	InitCPUBandwidthStats(exc);
}

void BbqueRPC::NotifyPostRun(RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	if (UpdateCPUBandwidthStats(exc) != RTLIB_OK)
		logger->Debug("PostRun: could not compute current CPU bandwidth");

	TogglePerfCountersPostCycle(exc);

#ifdef CONFIG_BBQUE_OPENCL

	if (rtlib_configuration.profile.opencl.enabled)
		OclCollectStats(exc->current_awm_id, exc->current_awm_stats->ocl_events_map);

#endif // CONFIG_BBQUE_OPENCL

	// Real cycle time for this execution cycle
	double cycle_time_ms = exc->execution_timer.getElapsedTimeMs();
	exc->time_analyser_cycle.InsertValue(cycle_time_ms);

	// Enforce minimum cycle time if the application requested it
	if (exc->cycle_time_enforced_ms != 0.0f)
		ForceCPS(exc);

	// Cycle time including CPS enforcing
	cycle_time_ms = exc->execution_timer.getElapsedTimeMs();

	double run_time_ms = cycle_time_ms - exc->run_tstart_ms;

	// Update total and AWM-wise time statistics
	pAwmStats_t awm_stats(exc->current_awm_stats);

	exc->time_analyser_run.InsertValue(run_time_ms);
	exc->run_time_ms += run_time_ms;
	awm_stats->run_samples(run_time_ms);
	awm_stats->time_spent_running += run_time_ms;

	// Execution Cycle of the EXC ends here
	awm_stats->cycle_samples(cycle_time_ms);
	exc->time_analyser_usercycle.InsertValue(
		exc->execution_timer.getElapsedTimeMs());

	exc->cycles_count += 1;

	STAT_LOG("APPLICATION:CYCLE_STOP %d", exc->cycles_count);
}

void BbqueRPC::NotifyPreMonitor(RTLIB_EXCHandler_t exc_handler)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	// Monitor phase of the EXC starts here
	exc->monitor_tstart_ms = exc->execution_timer.getElapsedTimeMs();

	logger->Debug("===> NotifyMonitor");
}

void BbqueRPC::NotifyPostMonitor(RTLIB_EXCHandler_t exc_handler,
		bool is_last_cycle)
{
	// Getting registered Execution Context from its handler
	pRegisteredEXC_t exc = getRegistered(exc_handler);
	if (! exc) return;

	if (! rtlib_configuration.unmanaged.enabled && ! is_last_cycle) {
		// Compute the ideal resource allocation for the application,
		// given its history
		UpdateAllocation(exc_handler);

		if (exc->runtime_profiling.rtp_forward == true)
			ForwardRuntimeProfile(exc_handler);
	}

	// Update total and AWM-wise time statistics
	pAwmStats_t awm_stats(exc->current_awm_stats);
	double monitor_time_ms =
		exc->execution_timer.getElapsedTimeMs() - exc->monitor_tstart_ms;

	exc->time_analyser_monitor.InsertValue(monitor_time_ms);
	exc->monitor_time_ms += monitor_time_ms;
	awm_stats->time_spent_monitoring += monitor_time_ms;
	awm_stats->monitor_samples(monitor_time_ms);

	logger->Debug("<=== NotifyMonitor");
}

#ifdef CONFIG_BBQUE_OPENCL

void BbqueRPC::OclGetRuntimeProfile(
	pRegisteredEXC_t exc, uint32_t & exec_time, uint32_t & mem_time)
{
	pAwmStats_t awm_stats = exc->current_awm_stats;
	CmdProf_t::const_iterator cmd_it;
	static uint32_t cum_exec_time_prev;
	static uint32_t cum_mem_time_prev;
	static uint32_t last_cycles_count = 0;
	uint32_t cum_exec_time = 0;
	uint32_t cum_mem_time  = 0;
	uint32_t delta_cycles_count;
	delta_cycles_count = exc->cycles_count - last_cycles_count;

	if (delta_cycles_count < 1) {
		exec_time = mem_time = 0;
		logger->Fatal("OCL: Runtime profile not updated");
		return;
	}

	// Iterate over all the command queues
	for (auto entry : awm_stats->ocl_events_map) {
		QueueProfPtr_t const & cmd_queue(entry.second);

		// Execution time
		for (int i = 0; i < 3; ++ i) {
			cmd_it = cmd_queue->cmd_prof.find(kernel_exec_cmds[i]);

			if (cmd_it == cmd_queue->cmd_prof.end())
				continue;

			cum_exec_time += bac::sum(cmd_it->second[CL_CMD_EXEC_TIME]) / 1000;
		}

		// Memory transfers time
		for (int i = 0; i < 14; ++ i) {
			cmd_it = cmd_queue->cmd_prof.find(memory_trans_cmds[i]);

			if (cmd_it == cmd_queue->cmd_prof.end())
				continue;

			cum_mem_time += bac::sum(cmd_it->second[CL_CMD_EXEC_TIME]) / 1000;
		}
	}

	// Update
	exec_time = (cum_exec_time - cum_exec_time_prev) / delta_cycles_count;
	mem_time  = (cum_mem_time  - cum_mem_time_prev)  / delta_cycles_count;
	logger->Fatal("OCL: Runtime profile %d cycles {exec_time=%d [us], mem_time=%d [us]}",
				  delta_cycles_count, exec_time, mem_time);
	cum_exec_time_prev = cum_exec_time;
	cum_mem_time_prev  = cum_mem_time;
	last_cycles_count  = exc->cycles_count;
}

#endif

} // namespace rtlib

} // namespace bbque
