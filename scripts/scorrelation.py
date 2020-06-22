#!/usr/bin/python3

import sys
import os
import csv
from scipy.stats import spearmanr

def usage ():
  print ("Usage: ", sys.argv[0], " <file>")
  print ("       Performs spearmann correlation test between each column of the provided file")

N = len(sys.argv);
if (N != 2):
  usage()
  exit(10)
  
infile = sys.argv[1];

with open(infile) as file:
  reader = csv.reader(file, delimiter=' ')
  lhs = [];
  rhs = [];
  for row in reader:
    lhs.append(row[1]);
    rhs.append(row[2]);

#print(len(lhs), len(rhs))

corr, pvalue = spearmanr(lhs, rhs);

tag = '+' if pvalue < .05 else '-';

print("{}> corr = {}, p-value = {} ".format(tag, corr, pvalue));
