/*
 * Copyright (C) 2016  Politecnico di Milano
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

#ifndef BBQUE_PLATFORM_PROXY_H_
#define BBQUE_PLATFORM_PROXY_H_

#include "bbque/app/application.h"
#include "bbque/config.h"
#include "bbque/modules_factory.h"
#include "bbque/res/resource_assignment.h"
#include "bbque/pp/platform_description.h"

#include <cstdint>

#define PLATFORM_PROXY_NAMESPACE "bq.pp"

using bbque::res::ResourceAssignmentMapPtr_t;
using bbque::res::RViewToken_t;
using bbque::app::AppPtr_t;

namespace bbque {

/**
 * @class PlatformProxy
 * @brief The PlatformProxy class is the interface for all PlatformProxy
 * classes. The access to these classes is provided by the PlatformManager
 */
class PlatformProxy {

public:

    virtual ~PlatformProxy() {}

    /**
     * @brief Exit codes returned by methods of this class
     */
    typedef enum ExitCode {
        PLATFORM_OK = 0,
        PLATFORM_GENERIC_ERROR,
        PLATFORM_INIT_FAILED,
        PLATFORM_ENUMERATION_FAILED,
        PLATFORM_NODE_PARSING_FAILED,
        PLATFORM_DATA_NOT_FOUND,
        PLATFORM_DATA_PARSING_ERROR,
        PLATFORM_COMM_ERROR,
        PLATFORM_MAPPING_FAILED,
        PLATFORM_PWR_MONITOR_ERROR,
        PLATFORM_AGENT_PROXY_ERROR,
    } ExitCode_t;

    /**
     * @brief Return the Platform specific string identifier
     */
    virtual const char* GetPlatformID(int16_t system_id=-1) const = 0;

    /**
     * @brief Return the Hardware identifier string
     */
    virtual const char* GetHardwareID(int16_t system_id=-1) const = 0;

    /**
     * @brief Platform specific resource setup interface.
     */
    virtual ExitCode_t Setup(AppPtr_t papp) = 0;

    /**
     * @brief Platform specific resources enumeration
     *
     * The default implementation of this method loads the TPD, is such a
     * function has been enabled
     */
    virtual ExitCode_t LoadPlatformData() = 0;

    /**
     * @brief Platform specific resources refresh
     */
    virtual ExitCode_t Refresh() = 0;
    /**
     * @brief Platform specific resources release interface.
     */
    virtual ExitCode_t Release(AppPtr_t papp) = 0;

    /**
     * @brief Platform specific resource claiming interface.
     */
    virtual ExitCode_t ReclaimResources(AppPtr_t papp) = 0;

    /**
     * @brief Bind the specified resources to the specified application.
     *
     * @param papp The application which resources are assigned
     * @param pres The resources to be assigned
     * @param excl If true the specified resources are assigned for exclusive
     * usage to the application
     */
    virtual ExitCode_t MapResources(
			AppPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl = true) = 0;


    static const pp::PlatformDescription & GetPlatformDescription() {

        std::unique_ptr<bu::Logger> logger = bu::Logger::GetLogger(PLATFORM_PROXY_NAMESPACE);

        // Check if the plugin is never loaded
        // in that case load it and parse the
        // platform configuration
        if (pli == NULL) {
            logger->Debug("I'm creating a new instance of PlatformLoader plugin.");
            pli = ModulesFactory::GetModule<plugins::PlatformLoaderIF>(
                        std::string("bq.pl.") + BBQUE_PLOADER_DEFAULT);
            assert(pli);
            if (plugins::PlatformLoaderIF::PL_SUCCESS != pli->loadPlatformInfo()) {
                logger->Fatal("Unable to load platform information.");
                throw std::runtime_error("PlatformLoaderPlugin pli->loadPlatformInfo() failed.");
            } else {
                logger->Info("Platform information loaded successfully.");
            }
        }

        // Return the just or previous loaded configuration
        return pli->getPlatformInfo();
    }

private:
        static plugins::PlatformLoaderIF* pli;


};

} // namespace bbque

#endif // BBQUE_PLATFORM_PROXY_H_
