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

#ifndef BBQUE_COREINT_TEST_H_
#define BBQUE_COREINT_TEST_H_

#include <memory>
#include <vector>
#include "bbque/system_view.h"
#include "bbque/app/application.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/plugins/plugin.h"
#include "bbque/plugins/test.h"

#define COREINT_NAMESPACE "coreint"

using namespace bbque::app;
using bbque::ApplicationManager;
using bbque::res::ResourceAccounter;

// These are the parameters received by the PluginManager on create calls
struct PF_ObjectParams;

namespace bbque { namespace plugins {

/**
 * @brief A TEST for the Application-BBQ interaction
 *
 * This test class simulate core interactions between applications and
 * Barbeque RTRM.
 * It simulates the registering of a set of resources, and the start of an
 * application. Here a recipe is loaded and all the check upon correctness of
 * resource usages defined in are made.
 * Once the ApplicationManager confirms the application has loaded, the test
 * print out their working modes details and then trigger some scheduling
 * status and working mode changes (passing overheads info too).
 *
 * By using the term "core" we are excluding the part regarding the
 * communication interface with the RTLib. Indeed the focus of the test is to
 * verify the correctness of the changes of scheduling status of applications.
 * Such changes are triggered by propèr method calls.
 *
 * As a consequence of scheduling changes, a variation in resource usages
 * accounting should be observed.
 *
 * Platform initialization is simulated too through a function that, given an
 * hard-coded set of resources, invokes the resource registration method of
 * ResourceAccounter for each of them.
 */
class CoreInteractionsTest: public TestIF {

public:

	/**
	 * @brief Plugin creation method
	 */
	static void * Create(PF_ObjectParams *);

	/**
	 * @brief Plugin destruction method
	 */
	static int32_t Destroy(void *);

	/**
	 * @brief class destructor
	 */
	virtual ~CoreInteractionsTest();

	/**
	 * @brief Test launcher
	 */
	void Test();

private:

	/** The logger used by the test */
	std::unique_ptr<bu::Logger> logger;

	/** System view instance */
	SystemView & sv;

	/** Application manager instance */
	ApplicationManager & am;

	/** Resource Accounter instance */
	ResourceAccounter & ra;

	/**
	 * @brief Constructor
	 */
	CoreInteractionsTest();

	/**
	 * @brief Test application reconfiguration action
	 *
	 * @param test_app A shared pointer to the application descriptor
	 * @param wm Working mode to switch in
	 * @param ov_time Switching time overhead
	 */
	void testScheduleSwitch(AppPtr_t & test_app, uint8_t wm_id,
			double ov_time);

	/**
	 * @brief Test working mode reconfigurations and constraints assertion
	 *
	 * @param am Application Manager instance
	 * @param test_app Object application test
	 */
	void testApplicationLifecycle(AppPtr_t & test_app);

	/**
	 * @brief Simulate a start of "num" applications
	 * @param num The number of applications to start
	 */
	void testStartApplications(uint16_t num);

	/**
	 * @brief Test the scheduling policy
	 */
	void testScheduling();

	/**
	 * @brief Test the syncronized acquisition of the resources
	 */
	void testSyncResourcesUpdate();

	/**
	 * @brief Test constraint assertions
	 */
	void testConstraints(AppPtr_t & app);

};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_COREINT_TEST_H_
