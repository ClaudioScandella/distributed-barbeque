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

#include "bbque/test_platform_data.h"

#include "bbque/configuration_manager.h"
#include "bbque/modules_factory.h"
#include "bbque/resource_accounter.h"

#define MODULE_NAMESPACE "bq.tpd"

namespace br = bbque::res;

namespace bbque {

TestPlatformData & TestPlatformData::GetInstance() {
	static TestPlatformData tpd;
	return tpd;
}


TestPlatformData::TestPlatformData() :
		platformLoaded(false) {

	//---------- Get a logger module
	std::string logger_name(TEST_PLATFORM_DATA_NAMESPACE);
	plugins::LoggerIF::Configuration conf(logger_name.c_str());
	logger = ModulesFactory::GetLoggerModule(std::cref(conf));
	if (!logger) {
		fprintf(stderr, "TPD: Logger module creation FAILED\n");
		assert(logger);
	}

}

TestPlatformData::~TestPlatformData() {
}

TestPlatformData::ExitCode_t
TestPlatformData::LoadPlatformData() {
		ConfigurationManager &cm(ConfigurationManager::GetInstance());
		ResourceAccounter &ra(ResourceAccounter::GetInstance());
		char resourcePath[] = "sys0.cpu256.mem0";
		//                     ........^
		//                        8

		if (platformLoaded)
				return TPD_SUCCESS;

		logger->Warn("Loading TEST platform data");
		logger->Debug("CPUs          : %5d", cm.TPD_CPUCount());
		logger->Debug("CPU memory    : %5d [MB]", cm.TPD_CPUMem());
		logger->Debug("PEs per CPU   : %5d", cm.TPD_PEsCount());
		logger->Debug("System memory : %5d", cm.TPD_SysMem());

		// Registering CPUs, per-CPU memory and processing elements (cores)
		logger->Debug("Registering resources:");
		for (uint8_t c = 0; c < cm.TPD_CPUCount(); ++c) {

				snprintf(resourcePath+8, 8, "%d.mem0", c);
				logger->Debug("  %s", resourcePath);
				ra.RegisterResource(resourcePath, "MB", cm.TPD_CPUMem());

				for (uint8_t p = 0; p < cm.TPD_PEsCount(); ++p) {
						snprintf(resourcePath+8, 8, "%d.pe%d", c, p);
						logger->Debug("  %s", resourcePath);
						ra.RegisterResource(resourcePath, " ", 100);
				}
		}

		// Registering system memory
		char sysMemPath[]   = "sys0.mem0";
		logger->Debug("  %s", sysMemPath);
		ra.RegisterResource(sysMemPath, "MB", cm.TPD_SysMem());

		platformLoaded = true;

		return TPD_SUCCESS;
}

} // namespace bbque

