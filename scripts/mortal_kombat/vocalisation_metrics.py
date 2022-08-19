#!/usr/bin/python3

import sys
import re
import pandas as pd

for f in sys.argv[1:]:
  m = re.search("gen([0-9]*)", f)
  gen = m.group(1)
  data = pd.read_table(f, sep=' ')
  #print(data)

  v = [0 for i in range(3)]

  i_prev = [0 for i in range(2)]
  i_spikes = [[] for i in range(2)]
  i_spikes_start = [[] for i in range(2)]

  c_prev = [0 for i in range(6)]
  c_spikes = [[] for i in range(6)]
  c_spikes_start = [[] for i in range(6)]

  r=0
  for row in data.itertuples():
    for i in range(2):
      prev = 0
      
      for j in range(3):
        ix=i*12+j+10;
        jx=i*3+j;

        d = row[ix]
        if (d > 0):
          #print(r, jx, d)
          v[j]+=1;
        
          if (i_prev[i] > 0):
            i_spikes[i][-1] += 1
          else:
            i_spikes[i].append(1)
            i_spikes_start[i].append(r)
            
          if (c_prev[jx] > 0):
            c_spikes[jx][-1] += 1
          else:
            c_spikes[jx].append(1)
            c_spikes_start[jx].append(r)
        
        prev += (d > 0)
        c_prev[jx] = (d > 0)
      i_prev[i] = prev
        
    r += 1
  
  # Detailed output
  if False:
    print("Talkativeness:", end='')
    for j in range(3):
      print(" {}".format(v[j]/(2*data.shape[0])), end='')
    print()

    print("Spikes:")
    for i in range(2):
      print(i, end=' ');
      for t in i_spikes_start[i]:
        print("{:3d}".format(t), end=' ');
      print("\n ", end=' ')
      for d in i_spikes[i]:
        print("{:3d}".format(d), end=' ');
      print()
      
    print("Spikes (channel-wise):")
    for i in range(6):
      print(i, end=' ');
      for t in c_spikes_start[i]:
        print("{:3d}".format(t), end=' ');
      print("\n ", end=' ')
      for d in c_spikes[i]:
        print("{:3d}".format(d), end=' ');
      print()

  tlk = 0
  for t in v:
    tlk += t/(2*data.shape[0])
    
  i_spk_ln, c_spk_ln = 0, 0
  i_spk_nb, c_spk_nb = 0, 0
  for s in i_spikes:
    for l in s:
      i_spk_ln += l
      i_spk_nb += 1
  i_spk_avg_ln = 0
  if i_spk_nb > 0:
    i_spk_avg_ln = i_spk_ln / i_spk_nb

  for s in c_spikes:
    for l in s:
      c_spk_ln += l
      c_spk_nb += 1
  c_spk_avg_ln = 0
  if c_spk_nb > 0:
    c_spk_avg_ln = c_spk_ln / c_spk_nb

  print(gen, tlk, i_spk_avg_ln, c_spk_avg_ln)
