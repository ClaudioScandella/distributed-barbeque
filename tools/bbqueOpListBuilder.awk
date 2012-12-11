#!/usr/bin/awk -f
#
# Copyright (C) 2012  Politecnico di Milano
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

############################## USAGE NOTES #####################################
# This is a simple filter to translate an XML OPs file into a statically
# allocates vector of operating points, which type is the OperatingPointsList
# class as defined by bbque/monitors/operating_point.h
#
# Given an input XML file of OPs, this script produce in a C source file which
# is suitable to be compiled and linked with the OP consumer code, which
# requires just to define an external reference to this variable, i.e.:
#    extern OperatingPointsList opList;
#
# The name of the generated vector could be customized by passing a proper value
# with the BBQUE_RTLIB_OPLIST variable. The name of this variable is also used
# as filename of the generated output.
#
# The generation of this C source file could be automatized via CMake by adding
# the CMakeList.txt a command like e.g.:
# BBQUE_OPS_C = "opList"
# add_custom_command (OUTPUT ${BBQUE_OPS_C}.cc
#      COMMAND ${PROJECT_SOURCE_DIR}/build_ops.awk ${BBQUE_OPS_XML} \
#              -v BBQUE_RTLIB_OPLIST="${BBQUE_OPS_C}"
#      DEPENDS ${BBQUE_OPS_C}.xml
#      COMMENT "Updating [${BBQUE_OPS_C}.cc] OPs list using [${BBQUE_OPS_C}.xml]...")
# NOTE: the generated output (i.e. opList.cc) should be listed as a source
# file in the target where this sould is part of.
# NOTE: the support for DMML library requires a similar approach but requires
# the definition of the BBQUE_DMMLIB_KNOBSLIST variable.
################################################################################

BEGIN {

	# Setup Filter Variables
	if (!length(BBQUE_RTLIB_OPLIST))  BBQUE_RTLIB_OPLIST="opList"
	if (!length(BBQUE_DMMLIB_KNOBSLIST))  BBQUE_DMMLIB_KNOBSLIST="dmm_knobs"

	# Setup output files
	ASRTM_FD  = sprintf("%s.cc", BBQUE_RTLIB_OPLIST)
	DMMLIB_FD = sprintf("%s.c",  BBQUE_DMMLIB_KNOBSLIST)

	# Dump AS-RTM OPList Source header
	printf ("/* This file has been automatically generated using */\n") >ASRTM_FD
	printf ("/* the bbque-opp Operating Points parser script. */\n") >>ASRTM_FD
	printf ("#include <bbque/monitors/operating_point.h>\n") >>ASRTM_FD
	printf ("using bbque::rtlib::as::OperatingPointsList;\n") >>ASRTM_FD
	printf ("OperatingPointsList %s = {\n", BBQUE_RTLIB_OPLIST) >>ASRTM_FD

	# Dump DMMS Tuning Knobs Source header
	printf ("/* This file has been automatically generated using */\n") >DMMLIB_FD
	printf ("/* the bbque-opp Operating Points parser script. */\n") >>DMMLIB_FD
	printf ("#include <dmmlib/knobs.h>\n") >>DMMLIB_FD
	printf ("#include <stdint.h>\n") >>DMMLIB_FD
	printf ("struct dmm_knobs_s %s [] = {\n", BBQUE_DMMLIB_KNOBSLIST) >>DMMLIB_FD
}

/<parameters>/ {
	IS_PARAM = 1
	printf ("  { //===== DMM Tuning #%03d =====\n", OP_COUNT) >>DMMLIB_FD
	printf ("    { //=== Parameters\n") >>DMMLIB_FD
	printf ("  { //===== OP #%03d =====\n", OP_COUNT) >>ASRTM_FD
	printf ("    { //=== Parameters\n") >>ASRTM_FD
	OP_COUNT++
	next;
}
match($0, /name="dmm.([^"]+).+value="([^"]+)/, o) {
	if (IS_PARAM) FIELD="p"; else FIELD="m"
	printf ("      %10s, // %c.%s\n", o[2], FIELD, o[1]) >>DMMLIB_FD
	next;
}
match($0, /name="([^"]+).+value="([^"]+)/, o) {
	printf ("      {\"%s\", %s},\n", o[1], o[2]) >>ASRTM_FD
	next;
}
/<system_metrics>/ {
	IS_PARAM = 0
	printf ("    },\n") >>DMMLIB_FD
	printf ("    { //=== Metrics\n") >>DMMLIB_FD
	printf ("    },\n") >>ASRTM_FD
	printf ("    { //=== Metrics\n") >>ASRTM_FD
	next;
}
/<\/system_metrics>/ {
	printf ("    },\n") >>DMMLIB_FD
	printf ("  },\n") >>DMMLIB_FD
	printf ("    },\n") >>ASRTM_FD
	printf ("  },\n") >>ASRTM_FD
	next;
}
END {
	printf ("};\n\n") >>DMMLIB_FD
	printf ("uint32_t %s_count = %d;\n\n", BBQUE_DMMLIB_KNOBSLIST, OP_COUNT) >>DMMLIB_FD
	printf ("};\n\n") >>ASRTM_FD
}
