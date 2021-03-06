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

#ifndef BBQUE_TESTS_H_
#define BBQUE_TESTS_H_

#include <iostream>
#include <random>
#include <cstdio>
#include <cstring>

#include <assert.h>
#include <stdio.h>
#include <cstdint>
#include <string>
#include <unistd.h>
#include <sys/syscall.h>

#include <bbque/utils/timer.h>
#include <bbque/rtlib/bbque_exc.h>

// Console colors definition
#define COLOR_WHITE  "\033[1;37m"
#define COLOR_LGRAY  "\033[37m"
#define COLOR_GRAY   "\033[1;30m"
#define COLOR_BLACK  "\033[30m"
#define COLOR_RED    "\033[31m"
#define COLOR_LRED   "\033[1;31m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_LGREEN "\033[1;32m"
#define COLOR_BROWN  "\033[33m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_LBLUE  "\033[1;34m"
#define COLOR_PURPLE "\033[35m"
#define COLOR_PINK   "\033[1;35m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_LCYAN  "\033[1;36m"

// Generic console logging message
# define BBQUE_FMT(color, module, fmt) \
	        color "[%05d - %11.6f] " module ": " fmt "\033[0m", \
			gettid(),\
			test_tmr.getElapsedTime()

// Generic code block comment on DEBUG compilation
#ifdef BBQUE_DEBUG
# define DB(x) x
#else
# define DB(x)
#endif

using bbque::rtlib::BbqueEXC;

/**
 * @brief A pointer to an EXC
 */
typedef std::shared_ptr<BbqueEXC> pBbqueEXC_t;

/**
 * @brief An entry of the map collecting managed EXCs
 */
typedef std::pair<std::string, pBbqueEXC_t> excMapEntry_t;

/**
 * @brief Maps recipes on corresponding EXCs
 */
typedef std::map<std::string, pBbqueEXC_t> excMap_t;

/**
 * @brief A regression test exit code
 */
typedef enum TestResult {
	TEST_PASSED = 0,
	TEST_WARNING,
	TEST_FAILED
} TestResult_t;

/**
 * @brief Prototype of a valid test function
 */
typedef TestResult_t (*test_function)(int argc, char *argv[]);

/**
 * The RNG which could be used for testcase initialization.
 */
extern std::mt19937 rng_engine;

/**
 * The test timer (for internal use only)
 */
extern bbque::utils::Timer test_tmr;

/**
 * Return the PID of the calling process/thread
 */
inline pid_t gettid() {
	        return syscall(SYS_gettid);
}

#endif // BBQUE_TESTS_H_
