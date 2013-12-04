#!/usr/bin/python

import getopt
import os
import string
import sys

import numpy as np
import matplotlib.pyplot as plt
from math import sqrt

util_columns = [0, 1]

standard_configuration = np.loadtxt(sys.argv[1], usecols = util_columns)
modified_configuration = np.loadtxt(sys.argv[2], usecols = util_columns)

xaxis = [point[0] for point in standard_configuration]
std_yaxis = [point[1] for point in standard_configuration]
mod_yaxis = [point[1] for point in modified_configuration]

ax0 = plt.subplot2grid((4,2), (0, 0), rowspan=1, colspan=1)
ax1 = plt.subplot2grid((4,2), (0, 1), rowspan=1, colspan=1, sharey=ax0)
ax2 = plt.subplot2grid((4,2), (1, 0), rowspan=3, colspan=2)


ax0.plot(xaxis, mod_yaxis, label='NEW')
ax1.plot(xaxis, std_yaxis, label='OLD')
plt.legend()

speedup_index = []
speedup_value = []
zero_axis = []
annotations = []

for i in range(len(standard_configuration)):
	if modified_configuration[i][1] / \
                                        standard_configuration[i][1] - 1 < -0.1:
		annotations.append((i + 1, modified_configuration[i][1] / \
                                         standard_configuration[i][1] - 1, i+1))
	elif modified_configuration[i][1] / \
                                         standard_configuration[i][1] - 1 > 0.3:
		annotations.append((i + 1, modified_configuration[i][1] / \
                                         standard_configuration[i][1] - 1, i+1))
	speedup_index.append(standard_configuration[i][0])
	speedup_value.append(modified_configuration[i][1] / \
                                               standard_configuration[i][1] - 1)
	zero_axis.append(0)

ax2.plot(speedup_index, speedup_value)
ax2.plot(speedup_index, zero_axis)

right = False

for exception in annotations:
	if right == False:	
		ax2.annotate("T" + str(exception[2]), xy=(exception[0],
                       exception[1]), xycoords='data', xytext=(exception[0] - 3,
                       min(exception[1] + 0.1, 0.9)), textcoords='data',
                       arrowprops=dict(arrowstyle="->", connectionstyle="arc3"))
		right = True
	else:
		ax2.annotate("T" + str(exception[2]), xy=(exception[0],
                       exception[1]), xycoords='data', xytext=(exception[0] + 1,
                       min(exception[1] + 0.1, 0.9)), textcoords='data',
                       arrowprops=dict(arrowstyle="->", connectionstyle="arc3"))
		right = False
plt.show()
