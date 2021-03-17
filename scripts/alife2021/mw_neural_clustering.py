#!/usr/bin/python3

import sys
import csv
import scipy.stats
import os

def usage ():
  print ("Usage: ", sys.argv[0], " <file> [verbose]")

def identical(lhs, rhs):
  #print()
  #print("\t>", lhs)
  #print("\t>", rhs)
  #print("\t>", set(lhs+rhs))
  return len(set(lhs+rhs)) < 2

if (len(sys.argv) != 2 and len(sys.argv) != 3):
  usage()
  exit(10)
  
infile = sys.argv[1]
filebase = os.path.splitext(infile)[0];

verbose = False
quiet = False
if len(sys.argv) >= 3:
  verbose = int(sys.argv[2])
  if verbose < 0:
    verbose = False
    quiet = True

columns = 0
headers = []
data = {}

# console colors
normal="\033[0m"
meh="\033[0m"
bad="\033[2m"
good="\033[7m"

threshold=0.05

minGroupSize = 20
validtags = []
with open(infile) as file:
  if not quiet:
    print("Processing", infile)
    
  reader = csv.reader(file, delimiter=' ')
  headers=next(reader)
  columns=len(headers)-1

  basetags=[c for c in headers[0]]
  tags=[c+str(j) for c in basetags for j in range(0,2)]
  
  # Prepare top-level container
  for t in tags:
    data[t] = [[] for i in range(0, columns)]
      
  for row in reader:
    for c in range(len(row[0])):
      t=basetags[c]+row[0][c]
      for i in range(0, columns):
        data[t][i].append(abs(float(row[i+1])));

  for t in basetags:
    if len(data[t+'0'][0]) >= minGroupSize and len(data[t+'1'][0]) >= minGroupSize:
      validtags.append(t)
    elif not quiet:
      print("Tag {} does not have enough data in each group [{}, {}] <= {}"
            .format(t, len(data[t+'0'][0]), len(data[t+'1'][0]), minGroupSize))

if len(validtags) == 0:
  print("All provided groups are too small...")
  exit(42)
  
correctedThreshold = threshold / columns
  
outfile = filebase + "_groups.dat"
of = open(outfile, "w")

if verbose:
  print(" base tags:", basetags)
  print("valid tags:", validtags)
  print("      tags:", tags)
  
  print("\n{:8s}".format("-"), end='')
  for t in tags:
    print(" {:^5s}".format(t), end='')
  print()
      
  print("{:8s}".format("Sizes:"), end='')
  for t in tags:
    print(" {:^5d}".format(len(data[t][0])), end='')
  print("\n")

  print("{:20s}".format("-"), end='')
  for t in validtags:
    print(" {:^7s}".format(t), end='')
  print()
  
of.write("-")
for t in basetags:
  of.write(" {}".format(t))
of.write("\n")
  
for h in range(0, columns):
  of.write(headers[h+1])
  if verbose:
    print("{:20s}".format(headers[h+1]+":"), end='')
  for t in basetags:
    if not t in validtags:
      of.write(" nan")
      continue
    
    lhs = data[t+'0'][h]
    rhs = data[t+'1'][h]
    #print(lhs)
    #print(rhs)
    [U, p] = [0, 1]
    if not identical(lhs, rhs):
      [U, p] = scipy.stats.mannwhitneyu(lhs, rhs, alternative='less')
    of.write(" {:d}".format(p < correctedThreshold))
      
    if verbose:
      code = bad if p > threshold else meh if p > correctedThreshold else good
      print(" {}{:^0.5f}{}".format(code, p, normal), end='')
  of.write("\n")
  if verbose:
    print()
if verbose:
  print()

if not quiet:
  print("Generated", outfile)
of.close()
