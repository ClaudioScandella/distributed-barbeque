#!/usr/bin/python

import getopt
import os
import string
import sys
import numpy as np
import itertools
import matplotlib.pyplot as plt
from math import sqrt

def create_input(n_samples):
	bundle = []
	
	for sample in range(n_samples):
		bundle.append(np.arange(0,1.1,0.2))
		bundle.append(np.arange(0,1.1,1))
	return bundle

def w_mix(array):
	den = float(len(array))
	num = 0.0
	for i in range(len(array)):
		num += array[i]

	print num/den
	return num/den

def variance(array):
	avg = 0.0
	sq_avg = 0.0

	for i in range(len(array)):
		avg += array[i]
		sq_avg += (array[i] * array[i])

	avg = avg / len(array)
	sq_avg = sq_avg / len(array)

	var = sq_avg - avg * avg

	return var

if len(sys.argv) != 2:
	print "an (integer) argument needed"
	sys.exit()

n_samples = int(sys.argv[1])
line = range

bundle = create_input(n_samples)
points = itertools.product(*bundle)

x = []
y = []

for index, point in enumerate(points):
	
	awms = []
	running = []
	is_awm = True

	for i in range(2*n_samples):
		if is_awm == True:
			awms.append(point[i])
			is_awm = False
		else:
			running.append(point[i])
			is_awm = True

	x.append(index)
	y.append((1 + w_mix(running) - (4 * variance(awms)))/2)

plt.plot(x, y)
plt.title("Variance range (%d AWMs)" %(n_samples))

plt.show()










