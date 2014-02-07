/**
 *       @file  MyApp_main.cc
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

#include <cstdio>
#include <iostream>
#include <random>
#include <cstring>
#include <memory>

#include <libgen.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "version.h"
#include "MyApp_exc.h"
#include <bbque/utils/utility.h>

// Setup logging
#undef  BBQUE_LOG_MODULE
#define BBQUE_LOG_MODULE "MyApp"

namespace po = boost::program_options;

/**
 * @brief A pointer to an EXC
 */
typedef std::shared_ptr<BbqueEXC> pBbqueEXC_t;

/**
 * The decription of each MyApp parameters
 */
po::options_description opts_desc("MyApp Configuration Options");

/**
 * The map of all MyApp parameters values
 */
po::variables_map opts_vm;

/**
 * The services exported by the RTLib
 */
RTLIB_Services_t *rtlib;

/**
 * @brief The recipe to use for all the EXCs
 */
std::string recipe;

/**
 * @brief The EXecution Context (EXC) registered
 */
pBbqueEXC_t pexc;

void ParseCommandLine(int argc, char *argv[]) {
	// Parse command line params
	try {
	po::store(po::parse_command_line(argc, argv, opts_desc), opts_vm);
	} catch(...) {
		std::cout << "Usage: " << argv[0] << " [options]\n";
		std::cout << opts_desc << std::endl;
		::exit(EXIT_FAILURE);
	}
	po::notify(opts_vm);

	// Check for help request
	if (opts_vm.count("help")) {
		std::cout << "Usage: " << argv[0] << " [options]\n";
		std::cout << opts_desc << std::endl;
		::exit(EXIT_SUCCESS);
	}

	// Check for version request
	if (opts_vm.count("version")) {
		std::cout << "MyApp (ver. " << g_git_version << ")\n";
		std::cout << "Copyright (C) 2011 Politecnico di Milano\n";
		std::cout << "\n";
		std::cout << "Built on " <<
			__DATE__ << " " <<
			__TIME__ << "\n";
		std::cout << "\n";
		std::cout << "This is free software; see the source for "
			"copying conditions.  There is NO\n";
		std::cout << "warranty; not even for MERCHANTABILITY or "
			"FITNESS FOR A PARTICULAR PURPOSE.";
		std::cout << "\n" << std::endl;
		::exit(EXIT_SUCCESS);
	}
}

int main(int argc, char *argv[]) {

	opts_desc.add_options()
		("help,h", "print this help message")
		("version,v", "print program version")

		("recipe,r", po::value<std::string>(&recipe)->
			default_value("MyApp"),
			"recipe name (for all EXCs)")
	;

	ParseCommandLine(argc, argv);

	// Welcome screen
	fprintf(stdout, FI(".:: MyApp (ver. %s) ::.\n"), g_git_version);
	fprintf(stdout, FI("Built: " __DATE__  " " __TIME__ "\n"));


	// Initializing the RTLib library and setup the communication channel
	// with the Barbeque RTRM
	fprintf(stderr, FI("STEP 0. Initializing RTLib, application [%s]...\n"),
			::basename(argv[0]));
	RTLIB_Init(::basename(argv[0]), &rtlib);
	assert(rtlib);


	fprintf(stderr, FI("STEP 1. Registering EXC using [%s] recipe...\n"),
			recipe.c_str());
	pexc = pBbqueEXC_t(new MyApp("MyApp", recipe, rtlib));
	if (!pexc->isRegistered())
		return RTLIB_ERROR;


	fprintf(stderr, FI("STEP 2. Starting EXC control thread...\n"));
	pexc->Start();


	fprintf(stderr, FI("STEP 3. Waiting for EXC completion...\n"));
	pexc->WaitCompletion();


	fprintf(stderr, FI("STEP 4. Disabling EXC...\n"));
	pexc = NULL;


	fprintf(stderr, FI("===== MyApp DONE! =====\n"));
	return EXIT_SUCCESS;

}

