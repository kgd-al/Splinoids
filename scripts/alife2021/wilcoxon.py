#!/usr/bin/python3

import sys
import csv
import scipy.stats

def usage ():
  print ("Usage: ", sys.argv[0], " <file> [column] [alternative]")

argc=len(sys.argv)
if (argc < 2 or argc > 4):
  usage()
  exit(10)
  
import os
infile = sys.argv[1]

if argc > 2: 
  m = int(sys.argv[2])
else:
  m = 1
  
if argc > 3:
  a = sys.argv[3]
else:
  a = 'two-sided'

verbose = 1;

data = []
with open(infile) as file:
  reader = csv.reader(file, delimiter=' ')
  for row in reader:
    if (len(data) == 0):
      data = [[] for i in range(0, len(row))]
      headers = row
    else:
      for i in range(0, len(row)):
        data[i].append(float(row[i]))

if verbose > 1:
  for i in range(0, len(headers)):
    print(headers[i], data[i])
  print()
  

def null(l):
  return len(set(l)) < 2

for i in range(m, len(headers)):
  for j in range(i+1, len(headers)):
    s = 0
    p = float('nan')
    ddiff = [a_i - b_i for a_i, b_i in zip(data[i], data[j])]
    if not null(ddiff):
      [s,p] = scipy.stats.wilcoxon(data[i], data[j], alternative=a, zero_method="pratt");
    
    if (p < .05 or verbose):
      print('{:10s} {} {:10s} (s = {:+7.4f}, p-value = {:6.4f})'.format(headers[i], a, headers[j], s, p));  
        
