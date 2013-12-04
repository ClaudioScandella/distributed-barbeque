#!/usr/bin/python

import getopt
import os
import string
import sys
import itertools
import logging

sys.path.append('./modules')

import parameters as user
import dse as dse
import barbeque_utils as bbque

logfile="test_log.txt"
resfile="res.log"
complexity_hard_mode = False
single_point = False
debug = False

if len(sys.argv) > 1:
	for command in range(1, len(sys.argv)):
		if sys.argv[command] == "help":
			dse.help()
			sys.exit()
		if sys.argv[command] == "debug":
			debug = True
		elif sys.argv[command] == "list":
			dse.command_list()		
			sys.exit()
		elif sys.argv[command] == "single-point":
			single_point = True		
		elif sys.argv[command] == "complexity":
			params = user.get_params()
			assert dse.check(params),\
			"Parameters error. Please see the log"
			print "Expected number of tests: %d" \
%(dse.ds_size(params))
			print "For the real number of test, use\
[complexity-hard]"
			sys.exit()
		elif sys.argv[command] == "complexity-hard":
			complexity_hard_mode = True
		else:
			print "[%s] is not a recongnized command. try [help]" \
%(sys.argv[1])
			sys.exit()

def logger_setup():
	if os.path.exists(logfile):
		os.remove(logfile)
	logging.basicConfig(filename=logfile,level=logging.DEBUG)

########################################
# Acquiring permissions
########################################

sys.stdout.write("\nAcquiring permissions..\n")
os.system("sudo echo done")

########################################
# Cleaning previous log
########################################

sys.stdout.write("\nSetting up the logfile [%s]\n" %(logfile))
logger_setup()
bbque.clean_log()
if os.path.exists(resfile):
		os.remove(resfile)

########################################
# Inputs collection
########################################

sys.stdout.write("Collecting parameters\n")
params = user.get_params()

assert dse.check(params),\
"Parameters error. Please see the log"

exp_tests = dse.ds_size(params)


########################################
# Design points db creation
########################################

sys.stdout.write("Generating Design Points\n")
inputs = dse.merge_inputs(params)
points = itertools.product(*inputs)

########################################
# Test start
########################################

test_number= 0
tests_skipped = 0
sys.stdout.write("Starting the exploration\n\n")
bbque.config_file_create()

for design_point in points:
	logging.info("exp:dse - new design point (%s)", str(design_point))
	launch_queue = []
	# print design_point
	# checking feasibility rules
	if user.check_point(design_point, params, single_point):
		logging.warning("exp:dse - invalid point: %s", \
str(design_point))
		tests_skipped += 1
		continue
	else:
		logging.info("exp:dse - [test %d]", test_number+1)
	if complexity_hard_mode == False:
		# showing progress on screen
		sys.stdout.write('\r')
		advancement = 100*float(test_number + tests_skipped) / \
float(exp_tests)
		adv_stat = int(advancement/2)
		sys.stdout.write("[%-50s] %d%% [%d/%d]" % ('='*adv_stat, \
advancement, test_number + tests_skipped, exp_tests))
		sys.stdout.flush()

		# starting barbeque
		bbque.startd(dse.extract_resources(params, design_point), \
params)

		# setting available resources
		bbque.set_resources(dse.extract_resources(params, \
design_point), params)

		# setting apps info
		for application in range(params['apps_number']):
			name = "application_%d" %(application)
			instances = dse.extract_instances(params, \
design_point)[application]
			awms = dse.extract_awms_number(params, \
design_point)[application]
			res_range = [dse.extract_awms_min_res(params, \
design_point)[application], dse.extract_awms_max_res(params, \
design_point)[application]]

			next_bundle = bbque.setup_application(name, \
instances, awms, res_range, application)
			launch_queue.append(next_bundle)

		# launching apps
		label = str(design_point)
		bbque.start_workload(launch_queue, label, test_number, debug)

		# stopping barbeque
		bbque.stop()
		logging.info("exp:dse - test ended")

	# updating progress on screen
	test_number += 1
	sys.stdout.write('\r')
	advancement = 100*float(test_number + tests_skipped)/float(exp_tests)
	adv_stat = int(advancement/2)
	sys.stdout.write("[%-50s] %d%% [%d/%d]" % ('='*adv_stat, advancement, \
test_number + tests_skipped, exp_tests))
	sys.stdout.flush()

sys.stdout.write("\n\n" +str(test_number)+" tests performed.\n")
sys.stdout.write(str(tests_skipped)+" tests skipped due to design rules.\n")
os.system("rm start.sh")
os.system("rm outputs/*")
os.system("mv test_log.txt outputs")
os.system("mv res.log outputs")


