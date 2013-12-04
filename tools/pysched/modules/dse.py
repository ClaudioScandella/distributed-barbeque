#!/usr/bin/python

import logging
import sys

def check(params):
	valid = True

	# constants > 0
	if params['bds_number'] <= 0:
		valid = False
		logging.error('bds_number <= 0')
	# binding domains containing at least 1 PE
	if params['bds_size'] < 100:
		valid = False
		logging.error('bds_size to small')

	# at least one app to test
	if params['apps_number'] <= 0:
		valid = False
		logging.error('apps_number <= 0')

	# params are ordered
	if sorted(params['bds_step']) != params['bds_step']:
		valid = False
		logging.error('bds_step not sorted')

	# params are ordered
	if sorted(params['apps_instances']) != params['apps_instances']:
		valid = False
		logging.error('apps_instances not sorted')

	# params are ordered
	if sorted(params['awms_number']) != params['awms_number']:
		valid = False
		logging.error('awms_number not sorted')

	# params are ordered
	if sorted(params['min_awm_res']) != params['min_awm_res']:
		valid = False
		logging.error('min_awm_res not sorted')

	# params are ordered
	if sorted(params['max_awm_res']) != params['max_awm_res']:
		valid = False
		logging.error('max_awm_res not sorted')

	# binding domains step in 0 - bd_size
	if params['bds_step'][0] < 0 or params['bds_step'][-1] > \
params['bds_size']:
		valid = False
		logging.error('bds_step out of range')

	# awms number in [1 - whatever]
	if params['awms_number'][0] < 1:
		valid = False
		logging.error('too few awms in the recipes')

	# awms resources properly set: [0 < min < max < bds_size]
	if params['min_awm_res'][0] <= 0 or params['min_awm_res'][-1] >= \
params['max_awm_res']:
		valid = False
		logging.error('min_awm_res out of range')
	if params['max_awm_res'][-1] > params['bds_size']:
		valid = False
		logging.error('max_awm_res out of range')

	# a BD is composed by N PEs, each giving 100% CPU quota
	if float(params['bds_size']) / 100 != float(params['bds_size']) // 100:
		valid = False
		logging.error('BD size must be multiple of 100')

	return valid

def ds_size(params):

	# computing the size of the design space
	bds = pow(len(params['bds_step']), params['bds_number'])
	inst = pow(len(params['apps_instances']), params['apps_number'])
	awms = pow(len(params['awms_number']), params['apps_number'])
	min_res = pow(len(params['min_awm_res']), params['apps_number'])
	max_res = pow(len(params['max_awm_res']), params['apps_number'])

	return bds * inst * awms * min_res * max_res

def merge_inputs(params):
	merged = []
	for i in range(params['bds_number']): 
		merged.append(params['bds_step'])
	for i in range(params['apps_number']): 
		merged.append(params['apps_instances'])
	for i in range(params['apps_number']): 
		merged.append(params['awms_number'])
	for i in range(params['apps_number']): 
		merged.append(params['min_awm_res'])
	for i in range(params['apps_number']): 
		merged.append(params['max_awm_res'])
	return merged

def extract_resources(params, design_point):
	return design_point[0:params['bds_number']]

def extract_instances(params, design_point):
	return design_point[params['bds_number'] : params['bds_number'] + \
params['apps_number']]

def extract_awms_number(params, design_point):
	return design_point[params['bds_number'] + params['apps_number'] : \
params['bds_number'] + params['apps_number'] + params['apps_number']]

def extract_awms_min_res(params, design_point):
	return design_point[params['bds_number'] + params['apps_number'] + \
params['apps_number'] : params['bds_number'] + \
params['apps_number'] + params['apps_number'] + 
params['apps_number']]

def extract_awms_max_res(params, design_point):
	return design_point[params['bds_number'] + params['apps_number'] + \
params['apps_number'] + params['apps_number'] : \
params['bds_number'] + params['apps_number'] + \
params['apps_number'] + params['apps_number'] + \
params['apps_number']]

def command_list():

	commands = [ ["help", "prints the help"],
                     ["list", "prints the command list"],
                     ["single-point", "check a specified point"],
		     ["debug", "activate debug mode"],
                     ["complexity", "naive complexity computation"],
                     ["complexity-hard", "precise complexity computation \
(could be long)"] ]

	print "\n"
	print "+========================================="
	print "| Command list                            "
	print "+========================================="
	
	for command in commands:
		print "| [%s] : [%s]" %(command[0], command[1])

	print "+========================================="
	print "\n"

def help():

	print "\n"
	print "+========================================="
	print "| Help                                   "
	print "+========================================="
	print "| Design points are created taking into"
	print "| account the user-defined parameters"
	print "| fount at line 12, module 'parameters'."
	print "| The fist three components are constant,"
	print "| the others contain all the possible"
	print "| values of the parameters."
	print "| At runtime, each variable parameter is"
	print "| istantiated multiple times (generally,"
	print "| 'bds_number' or 'apps_number' times)."
	print "| Thus, be aware that increasing the"
	print "| the value of constant vales or the size"
	print "| of variable parameters affects"
	print "| hugely the complexity of the exploration."
	print "+========================================="
	print "| The user can specify an unique point"
	print "| to test, which can be specified at line"
	print "| 79, module 'parameters'. Please note"
	print "| that the point has to be compliant with"
	print "| the parameters defined above."
	print "+========================================="
	print "| Use the command [list] to access to"
	print "| the command list"
	print "+========================================="
	print "\n"

