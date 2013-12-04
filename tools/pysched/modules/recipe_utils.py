#!/usr/bin/python

import getopt
import os
import string
import sys

def create_recipe(name, pr, aw, ms, bs, co):

	name = str(name)
	priority = int(pr)
	awm_number = int(aw)
	min_size = float(ms)
	bd_size = float(bs)
	cows_support = int(co)

	temp_recipe_path = "outputs/"+ name +".recipe"

	if bd_size < min_size:
		print "Bd size must be greater than minimum cpu request"
		sys.exit()
	
	step = (bd_size - min_size) / (awm_number - 1)
	
	if os.path.exists(temp_recipe_path):
		os.remove(temp_recipe_path)
	
	f = file(temp_recipe_path, "w")
	
	f.write('<?xml version="1.0"?>\n')
	f.write('<BarbequeRTRM recipe_version="0.8">\n')
	f.write('\t<application priority="' + str(priority) + '">\n')
	f.write('\t\t<platform id="org.linux.cgroup">\n')
	f.write('\t\t\t<awms>\n')
	
	for i in range(awm_number):
		cpu_quota = \
                        ((bd_size - (i * step))) - ((bd_size - (i * step)) % 10)
		value = (100 * (cpu_quota / bd_size)) // 1
	
		f.write('\t\t\t\t<awm id="'+ str(i) +'" name="wm'+ str(i) + \
                                                '" value="'+ str(value) +'">\n')
		f.write('\t\t\t\t\t<resources>\n')
		f.write('\t\t\t\t\t\t<sys id="0">\n')
		f.write('\t\t\t\t\t\t\t<cpu id="0">\n')
		f.write('\t\t\t\t\t\t\t\t<pe qty="'+ str(cpu_quota) +'"/>\n')
		f.write('\t\t\t\t\t\t\t\t<mem units="M" qty="250"/>\n')
		f.write('\t\t\t\t\t\t\t</cpu>\n')
		f.write('\t\t\t\t\t\t</sys>\n')
		f.write('\t\t\t\t\t</resources>\n')
	
		if cows_support == 1:
			f.write('\t\t\t\t\t<plugins>\n')
			f.write('\t\t\t\t\t\t<plugin name="cows">\n')
			f.write('\t\t\t\t\t\t\t<!-- 100000 * LLCM/cycle -->\n')
			f.write('\t\t\t\t\t\t\t<boundness>1</boundness>\n')
			f.write('\t\t\t\t\t\t\t<!-- 1000 * stalls/cycle -->\n')
			f.write('\t\t\t\t\t\t\t<stalls>1</stalls>\n')
			f.write('\t\t\t\t\t\t\t<!-- 1000 * retinst/cycle -->\n')
			f.write('\t\t\t\t\t\t\t<retired>1</retired>\n')
			f.write('\t\t\t\t\t\t\t<!-- 1000 * flops/cycle -->\n')
			f.write('\t\t\t\t\t\t\t<flops>1</flops>\n')
			f.write('\t\t\t\t\t\t</plugin>\n')
			f.write('\t\t\t\t\t</plugins>\n')
		
		f.write('\t\t\t\t</awm>\n')
	
	f.write('\t\t\t</awms>\n')
	f.write('\t\t</platform>\n')
	f.write('\t</application>\n')
	f.write('</BarbequeRTRM>\n')

	return temp_recipe_path
