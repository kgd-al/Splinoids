#!/usr/bin/python3

import sys
import csv
from scipy.stats import mannwhitneyu

def usage ():
  print ("Usage: ", sys.argv[0], " <file> [verbose]")

if (len(sys.argv) != 2 and len(sys.argv) != 3):
  usage()
  exit(10)
  
import os
infile = sys.argv[1]
filebase = os.path.splitext(infile)[0];

verbose = len(sys.argv) >= 3;

groups = {}
with open(infile) as file:
  reader = csv.reader(file, delimiter=' ')
  for row in reader:
    if len(row) > 2:
      key = row[0] + "_" + row[1];
      i = 2
    else:
      key = row[0]
      i = 1
      
    if (not key in groups):
      groups[key] = [];
    groups[key].append(row[i]);

for key, items in groups.items():
  print(key, "({}): ".format(len(items)), items)
print()
  
import igraph
g = igraph.Graph()
g.to_directed()
g.add_vertices(len(groups))
for i, k in enumerate(groups):
  g.vs[i]["name"] = k;
  g.vs[i]["label"] = k;
  
print(g)

for ilhs, klhs in enumerate(groups):
  for irhs, krhs in enumerate(groups):
    if (klhs != krhs):
      [U,p] = mannwhitneyu(groups[klhs], groups[krhs], alternative='less');
      
      if (p < .05 or verbose):
        print('{} < {} (p-value = {})'.format(klhs, krhs, p));  
        
      if (p < .05):
        g.add_edges([(ilhs, irhs)])
        
#print(g)
#layout = g.layout("fr")
#igraph.plot(g, layout=layout)
dotfile = filebase+'_graph.dot';
g.write_dot(dotfile)
print("Wrote dotfile to ", dotfile)

cmd="dot -Tpng "+dotfile+" -o "+filebase+"_graph.png";
ret = os.system(cmd);
print("Generating dot file '", cmd, "': ", ret);

tredfile = filebase+'_graph.tred.dot';
cmd = "tred "+dotfile+" > "+tredfile;
ret = os.system(cmd);
print("Generating filtered dot file '", cmd, "': ", ret);

cmd="dot -Tpng "+tredfile+" -o "+filebase+"_graph.tred.png";
ret = os.system(cmd)
print("Plotting filtered dot file '", cmd, "': ", ret);


#g1 = sys.argv[2];
#g2 = sys.argv[3];
#N = len(groups[g1])*len(groups[g2])

#[U,p] = mannwhitneyu(groups[g1], groups[g2], alternative='less');
#c = 'X' if p < .05 else ' ';
#print("[{}] mannwhitney({} < {}): U = {}, p = {}, f = {}".format(c, g1, g2, U, p, U / N))

#[U,p] = mannwhitneyu(groups[g1], groups[g2], alternative='greater');
#print("mannwhitney({} > {}): U = {}, p = {}, f = {}".format(g1, g2, U, p, U / N))

#[U,p] = mannwhitneyu(groups[g2], groups[g1], alternative='two-sided');
#print("mannwhitney({} != {}): U = {}, p = {}, f = {}".format(g1, g2, U, p, U / N))

#[U,p] = mannwhitneyu(groups[g2], groups[g1], alternative='less');
#print("mannwhitney({} < {}): U = {}, p = {}, f = {}".format(g2, g1, U, p, U / N))

#[U,p] = mannwhitneyu(groups[g2], groups[g1], alternative='greater');
#print("mannwhitney({} > {}): U = {}, p = {}, f = {}".format(g2, g1, U, p, U / N))

#[U,p] = mannwhitneyu(groups[g1], groups[g2], alternative='two-sided');
#print("mannwhitney({} != {}): U = {}, p = {}, f = {}".format(g2, g1, U, p, U / N))
