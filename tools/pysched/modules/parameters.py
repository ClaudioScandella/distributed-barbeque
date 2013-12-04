#!/usr/bin/python

import logging
import sys
import dse

def get_params():
	########################################
	# Inputs collection
	########################################
	
	params = {
	# constants
	'bds_number' : 			2,
	'bds_size' : 			200,
	'apps_number' : 		1,
	# variable params
	'bds_step' : 			[50, 100, 200],
	'apps_instances' : 		[1, 3, 5, 7],
	'awms_number' : 		[5],
	'min_awm_res' : 		[30, 70],
	'max_awm_res' : 		[150, 200],
	# add other components here
	}
	
	return params

def check_point(design_point, params, single_point):
	discard = False

	# point sample: (200, 200, 200, 2, 0, 3, 4, 50, 50, 200, 200)
	#                 |____|____|   |__|  |__|   |___|   |____|
	#                 |             |     |      |       |
	#                 |             |     |      |    max awm size (per app)
	#                 |             |     |   min awm size (per app)
	#                 |		|  number of awms (per app)
	#                 |	  number of instances (per app)
	#            available res (per BD)

	# extracting some major point info
	# total available resources
	total_available_resources = 0
	for avail_res in dse.extract_resources(params, design_point):
		total_available_resources += avail_res

	# total apps instances
	total_application_instances = 0
	for instances in dse.extract_instances(params, design_point):
		total_application_instances += instances

	# the resource requests amount at the minimum to ..
	min_awms_size = dse.extract_awms_min_res(params, design_point)
	# the available resources are ..
	avail_res_size = dse.extract_resources(params, design_point)
	# max applications (naive, conservative)
	max_apps = 0
	for res in avail_res_size:
		max_apps += res // max(min_awms_size)

	# discard = True if the point violates a rule
	if total_available_resources == 0 :
		discard = True

	if total_application_instances == 0 :
		discard = True
		total_application_instances += 1

	# there is an application whose minimal AWM cannot be
	# scheduled due to low resources
	if max(min_awms_size) > max(avail_res_size):
		discard = True

	# If I have too many applications and too few available
	# resources ...
	if total_available_resources / total_application_instances < \
max(min_awms_size):
		discard = True

	if single_point:
		if singlepoint_pruning(design_point):
			discard = True

	if total_application_instances > max_apps:
		discard = True

	return discard

def singlepoint_pruning(design_point):
	my_point = [200, 200, 1, 5, 30, 200]
	discard = False
	for index in range(len(my_point)):
		if my_point[index] != design_point[index]:
			discard = True
	return discard
	
