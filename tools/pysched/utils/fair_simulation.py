#!/usr/bin/python

import getopt
import os
import string
import sys
import numpy as np
import itertools
import matplotlib.pyplot as plt
from math import sqrt

def w_mix(array):
	den = float(len(array))
	num = 0.0
	for i in range(len(array)):
		num += array[i]
	#print array, num, den, num/den
	return num/den

def variance(array, array2):
	avg = 0.0
	sq_avg = 0.0
	r_len = 0

	for i in range(len(array)):
		if array2[i] == 1:
			avg += array[i]
			sq_avg += (array[i] * array[i])
			r_len += 1

	if r_len == 0:
		return 0

	avg = avg / r_len
	sq_avg = sq_avg / r_len

	var = sq_avg - avg * avg

	return var


points = []

applications = 50
running_apps = np.arange(0, applications+0.1, 1)
var = np.arange(0.25 , -0.01, -0.05)

for a in range(len(running_apps)):
	for b in range(len(var)):
		design_point = [running_apps[a], var[b]]
		points.append(design_point)

x = []
y = []
z_1 = []
z_2 = []

for index, point in enumerate(points):

	print point
	r = point[0]
	v = point[1]
	a = applications

	#current = r/(a*(1+v))
	current = (r + (1 - 3 * v)) / (a + 1)
	wm = r / (a + 1)
	vi = (1 - 3.6*v) / (a + 1)

	x.append(index)
	#y.append((1 + w_mix(running) - (4 * variance(awms)))/2)
	#y.append((w_mix(running)*0.75 + (variance(awms))))
	y.append(current)
	z_1.append(wm)
	z_2.append(vi)

plt.plot(x, y, marker='*', linestyle='none')
#plt.plot(x, z_1)
#plt.plot(x, z_2)

plt.title("F_INDEX range, N=%d" %(applications))

plt.show()

