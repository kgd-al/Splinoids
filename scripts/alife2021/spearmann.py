#!/usr/bin/python3

import sys
import csv
import scipy.stats

def usage ():
  print ("Usage: ", sys.argv[0], " <file> [verbose]")

if (len(sys.argv) != 2 and len(sys.argv) != 3):
  usage()
  exit(10)
  
import os
infile = sys.argv[1]

verbose = len(sys.argv) >= 3;

data = []
with open(infile) as file:
  reader = csv.reader(file, delimiter=' ')
  for row in reader:
    if (len(data) == 0):
      data = [[] for i in range(1, len(row))]
      headers = row
    else:
      for i in range(0, len(row)-1):
        data[i].append(float(row[i]))

if verbose:
  for i in range(0, len(headers)-1):
    print(headers[i], data[i])
  print()
  
m=3
for i in range(m, len(headers)-1):
  for j in range(i+1, len(headers)-1):
    [r,p] = scipy.stats.spearmanr(data[i], data[j]);
    
    if (p < .05 or verbose):
      print('{:10s} < {:10s} (r = {:+7.4f}, p-value = {:6.4f})'.format(headers[i], headers[j], r, p));  
        
