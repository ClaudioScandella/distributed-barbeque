/*
 * Copyright (C) 2014  Politecnico di Milano
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

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "bbque/power_monitor.h"

#include "bbque/resource_accounter.h"
#include "bbque/res/resource_path.h"
#include "bbque/utils/utility.h"

#define MODULE_CONFIG "PowerMonitor"
#define MODULE_NAMESPACE POWER_MONITOR_NAMESPACE

#define WM_LOGFILE_FMT "%s/BBQ_PowerMonitor_%.dat"
#define WM_LOGFILE_HEADER \
	"# Columns legend:\n"\
	"#\n"\
	"# 1: Load (%)\n"\
	"# 2: Temperature (°C)\n"\
	"# 3: Core frequency (MHz)\n"\
	"# 4: Fanspeed (%)\n"\
	"# 5: Voltage (mV)\n"\
	"# 6: Performance level\n"\
	"# 7: Power state\n"\
	"#\n"


namespace po = boost::program_options;

namespace bbque {


PowerMonitor & PowerMonitor::GetInstance() {
	static PowerMonitor instance;
	return instance;
}

PowerMonitor::PowerMonitor():
		Worker(),
		pm(PowerManager::GetInstance()),
#ifdef CONFIG_BBQUE_PM_BATTERY
		bm(BatteryManager::GetInstance()),
#endif
		cm(CommandManager::GetInstance()),
		cfm(ConfigurationManager::GetInstance()) {
	// Get a logger module
	logger = bu::Logger::GetLogger(POWER_MONITOR_NAMESPACE);
	assert(logger);
	logger->Info("PowerMonitor initialization...");
	Init();

	//---------- Loading configuration
	po::options_description opts_desc("Power Monitor options");
	opts_desc.add_options()
		(MODULE_CONFIG ".period_ms",
		 po::value<uint32_t>(&wm_info.period_ms)->default_value(WM_DEFAULT_PERIOD_MS),
		 "The period [ms] power monitor sampling");

	opts_desc.add_options()
		(MODULE_CONFIG ".log.dir",
		 po::value<std::string>(&wm_info.log_dir)->default_value("/tmp/"),
		 "The output directory for the status data dump files");
	opts_desc.add_options()
		(MODULE_CONFIG ".log.enabled",
		 po::value<bool>(&wm_info.log_enabled)->default_value(false),
		 "Default status of the data logging");
	// Thermal threshold configuration
	opts_desc.add_options()
		(MODULE_CONFIG ".temp.critical",
		 po::value<uint32_t>(&temp[WM_TEMP_CRITICAL_ID])->default_value(90),
		 "Default status of the data logging");
	po::variables_map opts_vm;

	opts_desc.add_options()
		(MODULE_CONFIG ".nr_threads",
		 po::value<uint16_t>(&nr_threads)->default_value(1),
		 "Number of monitoring threads");
	cfm.ParseConfigurationFile(opts_desc, opts_vm);

#define CMD_WM_DATALOG "datalog"
	cm.RegisterCommand(MODULE_NAMESPACE "." CMD_WM_DATALOG,
			static_cast<CommandHandler*>(this),
			"Start/stop power monitor data logging");
#ifdef CONFIG_BBQUE_PM_BATTERY
#define CMD_WM_SYSLIFETIME "syslifetime"
	cm.RegisterCommand(MODULE_NAMESPACE "." CMD_WM_SYSLIFETIME,
			static_cast<CommandHandler*>(this),
			"Set the system target lifetime");

	pbatt = bm.GetBattery();
	if (pbatt == nullptr)
		logger->Warn("Battery available: NO");
	else
		logger->Info("Battery available: %s", pbatt->StrId().c_str());
#endif // CONFIG_BBQUE_PM_BATTERY

	//---------- Setup Worker
	Worker::Setup(BBQUE_MODULE_NAME("wm"), POWER_MONITOR_NAMESPACE);
	Worker::Start();
}

void PowerMonitor::Init() {
	PowerMonitorGet[size_t(PowerManager::InfoType::LOAD)]        =
		&PowerManager::GetLoad;
	PowerMonitorGet[size_t(PowerManager::InfoType::TEMPERATURE)] =
		&PowerManager::GetTemperature;
	PowerMonitorGet[size_t(PowerManager::InfoType::FREQUENCY)]   =
		&PowerManager::GetClockFrequency;
	PowerMonitorGet[size_t(PowerManager::InfoType::POWER)]       =
		&PowerManager::GetPowerUsage;
	PowerMonitorGet[size_t(PowerManager::InfoType::PERF_STATE)]  =
		&PowerManager::GetPerformanceState;
	PowerMonitorGet[size_t(PowerManager::InfoType::POWER_STATE)] =
		&PowerManager::GetPowerState;
}

PowerMonitor::~PowerMonitor() {
	Stop();
	Worker::Terminate();
}

void PowerMonitor::Task() {
	logger->Debug("Monitor: waiting for platform to be ready...");
	ResourceAccounter & ra(ResourceAccounter::GetInstance());
	ra.WaitForPlatformReady();
	std::vector<std::thread> samplers(nr_threads);

	uint16_t nr_resources_to_monitor = wm_info.resources.size();
	uint16_t nr_resources_left = 0;
	if (nr_resources_to_monitor > nr_threads) {
		nr_resources_to_monitor /= nr_threads;
		nr_resources_left = wm_info.resources.size() % nr_threads;
	}
	else
		nr_threads = 1;
	logger->Debug("Monitor: nr_threads=%d nr_resources_to_monitor=%d",
		nr_threads, nr_resources_to_monitor);

	uint16_t nt = 0;
	for (; nt < nr_threads; ++nt) {
		logger->Debug("Starting monitoring thread %d...", nt);
		samplers.push_back(std::thread(
			&PowerMonitor::SampleResourcesStatus, this,
			nt * nr_resources_to_monitor,
			nt + nr_resources_to_monitor));
	}
	// The number of resources is not divisible by the number of threads...
	// --> spawn one more thread
	if (nr_resources_left > 0) {
		logger->Debug("Starting monitoring thread %d [extra]...", nr_threads);
		samplers.push_back(std::thread(
			&PowerMonitor::SampleResourcesStatus, this,
			nr_threads * nr_resources_to_monitor,
			nr_resources_left));
	}

#ifdef CONFIG_BBQUE_PM_BATTERY
#ifdef BBQUE_DEBUG
	samplers.push_back(std::thread(&PowerMonitor::SampleBatteryStatus, this));
#endif // BBQUE_DEBUG
#endif // CONFIG_BBQUE_PM_BATTERY
	while(!done)
		Wait();
	std::for_each(samplers.begin(), samplers.end(), std::mem_fn(&std::thread::join));
}


int PowerMonitor::CommandsCb(int argc, char *argv[]) {
	uint8_t cmd_offset = ::strlen(MODULE_NAMESPACE) + 1;
	char * command_id  = argv[0] + cmd_offset;
	logger->Info("PWR MNTR: Processing command [%s]", command_id);

	// Data logging control
	if (!strncmp(CMD_WM_DATALOG, command_id, strlen(CMD_WM_DATALOG))) {
		if (argc != 2) {
			logger->Error("PWR MNTR: Command [%s] missing action [start/stop/clear]",
					command_id);
			return 1;
		}
		return DataLogCmdHandler(argv[1]);
	}
#ifdef CONFIG_BBQUE_PM_BATTERY
	// System life-time target
	if (!strncmp(CMD_WM_SYSLIFETIME , command_id, strlen(CMD_WM_SYSLIFETIME))) {
		if (argc < 2) {
			logger->Error("PWR MNTR: Command [%s] missing argument"
					"[set/clear/info/help]", command_id);
			return 1;
		}
		if (argc > 2)
			return SystemLifetimeCmdHandler(argv[1], argv[2]);
		else
			return SystemLifetimeCmdHandler(argv[1], "");
	}
#endif
	logger->Error("PWR MNTR: Command unknown [%s]", command_id);
	return -1;
}


PowerMonitor::ExitCode_t PowerMonitor::Register(
		br::ResourcePathPtr_t rp,
		PowerManager::SamplesArray_t const & samples_window) {
	ResourceAccounter & ra(ResourceAccounter::GetInstance());

	// Register all the resources referenced by the path specified
	br::ResourcePtrList_t r_list(ra.GetResources(rp));
	if (r_list.empty()) {
		logger->Warn("PWR MNTR: No resources to monitor [%s]", rp->ToString().c_str());
		return ExitCode_t::ERR_RSRC_MISSING;
	}

	// Register each resource to monitor, specifying the number of samples to
	// consider in the (exponential) mean computation and the output log file
	// descriptor
	for (br::ResourcePtr_t rsrc: r_list) {
		rsrc->EnablePowerProfile(samples_window);
		logger->Info("PWR MNTR: Registering [%s]...", rsrc->Path().c_str());
		wm_info.resources.push_back({ra.GetPath(rsrc->Path()), rsrc});
		wm_info.log_fp.emplace(ra.GetPath(rsrc->Path()), new std::ofstream());
	}

	return ExitCode_t::OK;
}

PowerMonitor::ExitCode_t PowerMonitor::Register(
		const char * rp_str,
		PowerManager::SamplesArray_t const & samples_window) {
	ResourceAccounter & ra(ResourceAccounter::GetInstance());
	return Register(ra.GetPath(rp_str), samples_window);
}


void PowerMonitor::Start(uint32_t period_ms) {
	std::unique_lock<std::mutex> worker_status_ul(worker_status_mtx);
	if ((period_ms != 0) && (period_ms != wm_info.period_ms))
		wm_info.period_ms = period_ms;

	if (wm_info.started) {
		logger->Warn("PWR MNTR: Already started (T = %d ms)...",
			wm_info.period_ms);
		return;
	}

	logger->Info("PWR MNTR: Starting (T = %d ms)...", wm_info.period_ms);
	events.set(WM_EVENT_UPDATE);
	worker_status_cv.notify_all();
}


void PowerMonitor::Stop() {
	std::unique_lock<std::mutex> worker_status_ul(worker_status_mtx);
	if (!wm_info.started) {
		logger->Warn("PWR MNTR: Already stopped");
		return;
	}

	logger->Info("PWR MNTR: Stopping...");
	events.reset(WM_EVENT_UPDATE);
	worker_status_cv.notify_all();
}


void PowerMonitor::SampleBatteryStatus() {
#ifdef CONFIG_BBQUE_PM_BATTERY
	if (pbatt == nullptr) return;
	while (!done) {
		if (events.none())
			Wait();
		if (events.test(WM_EVENT_UPDATE))
			logger->Debug("PWR MNT: Battery power = %d mW", pbatt->GetPower());
		std::this_thread::sleep_for(std::chrono::milliseconds(wm_info.period_ms));
	}
#endif // CONFIG_BBQUE_PM_BATTERY
}

void  PowerMonitor::SampleResourcesStatus(
		uint16_t first_resource_index,
		uint16_t nr_resources_to_monitor) {
	PowerManager::SamplesArray_t samples;
	PowerManager::InfoType info_type;

	while (!done) {
		if (events.none()) {
			logger->Debug("PWR MNTR: No events to process [first=%d]",
				first_resource_index);
			Wait();
		}
		if (!events.test(WM_EVENT_UPDATE))
			continue;

		logger->Debug("PWR MNTR: resource@[%d] nr_resources=%d",
			first_resource_index, nr_resources_to_monitor);

		// Power status monitoring over all the registered resources
		uint16_t i = first_resource_index;
		for (; i < nr_resources_to_monitor; ++i) {
			br::ResourcePathPtr_t const & r_path(wm_info.resources[i].path);
			br::ResourcePtr_t & rsrc(wm_info.resources[i].resource_ptr);

			std::string log_inst_values("[");
			std::string log_mean_values("[");
			std::string log_file_values;
			log_inst_values += rsrc->Path() + "] (I): ";
			log_mean_values += rsrc->Path() + "] (M): ";
			uint info_idx   = 0;
			uint info_count = 0;

			for (; info_idx < PowerManager::InfoTypeIndex.size() &&
					info_count < rsrc->GetPowerInfoEnabledCount();
						++info_idx, ++info_count) {
				// Check if the power profile information has been required
				info_type = PowerManager::InfoTypeIndex[info_idx];
				if (rsrc->GetPowerInfoSamplesWindowSize(info_type) <= 0)
					continue;

				// Call power manager get function and update the resource
				// descriptor power profile information
				if (PowerMonitorGet[info_idx] == nullptr) {
					logger->Warn("Power monitoring for %s not available",
						PowerManager::InfoTypeStr[info_idx]);
					continue;
				}
				(pm.*(PMfunc) PowerMonitorGet[info_idx])(r_path, samples[info_idx]);
				rsrc->UpdatePowerInfo(info_type, samples[info_idx]);

				// Log messages
				std::stringstream ss_i;
				ss_i
					<< std::setprecision(0)       << std::fixed
					<< std::setw(str_w[info_idx]) << std::left
					<< rsrc->GetPowerInfo(info_type, br::Resource::INSTANT);
				log_inst_values += ss_i.str() + " ";
				log_file_values += ss_i.str() + " ";

				std::stringstream ss_m;
				ss_m
					<< std::setprecision(str_p[info_idx]) << std::fixed
					<< std::setw(str_w[info_idx])         << std::left
					<< rsrc->GetPowerInfo(info_type, br::Resource::MEAN);
				log_mean_values += ss_m.str() + " ";

			}

			logger->Debug("PWR MNTR: Sampling [%s] ", log_inst_values.c_str());
			logger->Debug("PWR MNTR: Sampling [%s] ", log_mean_values.c_str());
			if (wm_info.log_enabled) {
				DataLogWrite(r_path, log_file_values);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(wm_info.period_ms));
	}
	logger->Notice("PWR MNTR: Terminating monitor thread");
}

/*******************************************************************
 *                        DATA LOGGING                             *
 *******************************************************************/

void PowerMonitor::DataLogWrite(
		br::ResourcePathPtr_t rp,
		std::string const & data_line,
		std::ios_base::openmode om) {

	//std::string file_path(BBQUE_WM_DATALOG_PATH "/");
	std::string file_path(wm_info.log_dir + "/");
	file_path.append(rp->ToString());
	file_path.append(".dat");
	logger->Debug("PWR MNTR: Log file [%s]: %s",
		file_path.c_str(), data_line.c_str());

	// Open file
	wm_info.log_fp[rp]->open(file_path, om);
	if (!wm_info.log_fp[rp]->is_open()) {
		logger->Warn("PWR MNTR: Log file not open");
		return;
	}

	// Write data line
	*wm_info.log_fp[rp] << data_line << std::endl;
	if (wm_info.log_fp[rp]->fail()) {
		logger->Error("PWR MNTR: Log file write failed [F:%d, B:%d]",
			wm_info.log_fp[rp]->fail(),
			wm_info.log_fp[rp]->bad());
		*wm_info.log_fp[rp] << "Error";
		*wm_info.log_fp[rp] << std::endl;
		return;
	}
	// Close file
	wm_info.log_fp[rp]->close();
}


void PowerMonitor::DataLogClear() {
	for (auto log_ofs: wm_info.log_fp) {
		DataLogWrite(log_ofs.first, WM_LOGFILE_HEADER, std::ios_base::out);
	}
}


int PowerMonitor::DataLogCmdHandler(const char * arg) {
	std::string action(arg);
	logger->Info("PWR MNTR: Action = %s", action.c_str());
	// Start
	if ((action.compare("start") == 0)
			&& (!wm_info.log_enabled)) {
		logger->Info("PWR MNTR: Starting data logging...");
		wm_info.log_enabled = true;
		return 0;
	}
	// Stop
	if ((action.compare("stop") == 0)
			&& (wm_info.log_enabled)) {
		logger->Info("PWR MNTR: Stopping data logging...");
		wm_info.log_enabled = false;
		return 0;
	}
	// Clear
	if (action.compare("clear") == 0) {
		bool de = wm_info.log_enabled;
		wm_info.log_enabled = false;
		DataLogClear();
		wm_info.log_enabled = de;
		logger->Info("PWR MNTR: Clearing data logs...");
		return 0;
	}

	logger->Warn("PWR MNTR: unknown action [%s] or nothing to do",
		action.c_str());
	return -1;
}

/*******************************************************************
 *                 ENERGY BUDGET MANAGEMENT                        *
 *******************************************************************/


int32_t PowerMonitor::GetSysPowerBudget() {
#ifndef CONFIG_BBQUE_PM_BATTERY
	return 0;
#else
	std::unique_lock<std::mutex> ul(sys_lifetime.mtx);
/*
	if (!pbatt->IsDischarging() && pbatt->GetChargePerc() == 100) {
		logger->Debug("System battery full charged and power plugged");
		return 0;
	}
*/

	if (sys_lifetime.always_on) {
		logger->Debug("PWR MNTR: System lifetime target = 'always_on'");
		return -1;
	}

	if (sys_lifetime.power_budget_mw == 0) {
		logger->Debug("PWR MNTR: No system lifetime target");
		return 0;
	}

	// Compute power budget
	sys_lifetime.power_budget_mw = ComputeSysPowerBudget();
	return sys_lifetime.power_budget_mw;
#endif
}

#ifdef CONFIG_BBQUE_PM_BATTERY

int PowerMonitor::SystemLifetimeCmdHandler(
		const std::string action, const std::string hours) {
	std::unique_lock<std::mutex> ul(sys_lifetime.mtx);
	std::chrono::system_clock::time_point now;
	logger->Info("PWR MNTR: action=[%s], hours=[%s]",
			action.c_str(), hours.c_str());
	// Help
	if (action.compare("help") == 0) {
		logger->Notice("PWR MNTR: %s set <HOURS> (set hours)", CMD_WM_SYSLIFETIME);
		logger->Notice("PWR MNTR: %s info  (target lifetime)", CMD_WM_SYSLIFETIME);
		logger->Notice("PWR MNTR: %s clear (clear setting)",   CMD_WM_SYSLIFETIME);
		logger->Notice("PWR MNTR: %s help  (this help)",  CMD_WM_SYSLIFETIME);
		return 0;
	}
	// Clear the target lifetime setting
	if (action.compare("clear") == 0) {
		logger->Notice("PWR MNTR: Clearing system target lifetime...");
		sys_lifetime.power_budget_mw = 0;
		sys_lifetime.always_on   = false;
		return 0;
	}
	// Return information about last target lifetime set
	if (action.compare("info") == 0) {
		logger->Notice("PWR MNTR: System target lifetime information...");
		sys_lifetime.power_budget_mw = ComputeSysPowerBudget();
		PrintSystemLifetimeInfo();
		return 0;
	}
	// Set the target lifetime
	if (action.compare("set") == 0) {
		logger->Notice("PWR MNTR: Setting system target lifetime...");
		// Argument check
		if (!IsNumber(hours)) {
			logger->Error("PWR MNTR: Invalid argument");
			return -1;
		}
		else if (hours.compare("always_on") == 0) {
			logger->Info("PWR MNTR: Set to 'always on'");
			sys_lifetime.power_budget_mw = -1;
			sys_lifetime.always_on     = true;
			return 0;
		}
		// Compute system clock target lifetime
		now = std::chrono::system_clock::now();
		std::chrono::hours h(std::stoi(hours));
		sys_lifetime.target_time     = now + h;
		sys_lifetime.always_on       = false;
		sys_lifetime.power_budget_mw = ComputeSysPowerBudget();
		PrintSystemLifetimeInfo();
	}
	return 0;
}


void PowerMonitor::PrintSystemLifetimeInfo() const {
	std::chrono::seconds secs_from_now;
	// Print output
	std::time_t time_out = std::chrono::system_clock::to_time_t(
			sys_lifetime.target_time);
	logger->Notice("PWR MNTR: System target lifetime: %s",
			ctime(&time_out));
	secs_from_now = GetSysLifetimeLeft();
	logger->Notice("PWR MNTR: System target lifetime [s]: %d",
			secs_from_now.count());
	logger->Notice("PWR MNTR: System power budget [mW]: %d",
			sys_lifetime.power_budget_mw);
}

#endif // Battery management enabled

} // namespace bbque

