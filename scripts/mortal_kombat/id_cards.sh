#!/bin/bash

base=../alife2022_results/analysis/
stats=$base/stats/stats.dat
normalized=$(dirname $stats)/$(basename $stats .dat).normalized.dat
ofolder=$base/idcards
mkdir -p $ofolder

cols="ID bdy-mass arm-mass viw-prec ann-hidn Damage Talkativeness"
pretty_names="Body mass,Arms mass,Retina size,Hidden neurons,Damage,Talkativeness"

awk -vcols="$cols" '
  NR==1 {
    split(cols, acols, " ");
    for (c in acols) cnames[acols[c]] = 1;
    for (i=1;i<=NF;i++) if ($i in cnames) headers[i] = $i;
    next;
  }
  NR == 2 {
    for (i in headers) {
      if (int(i) > 2) {
        max[i] = $i+0;
        min[i] = $i;
      }
    }
  }
  {
    for (i in headers) {
      data[NR][i] = $i;
      if (int(i) > 2) {
        max[i] = (max[i] >= $i+0 ? max[i] : $i);
        min[i] = (min[i] <= $i+0 ? min[i] : $i);
      }
    }
  }
  END {
    for (i in max) range[i] = max[i] - min[i];
    
    printf "-" > "/dev/stderr";
    for (i in headers) if (int(i) > 2) printf " %s", headers[i] > "/dev/stderr";
    printf "\nmin:" > "/dev/stderr";
    for (i in headers) if (int(i) > 2) printf " %g", min[i] > "/dev/stderr";
    printf "\nmax:" > "/dev/stderr";
    for (i in headers) if (int(i) > 2) printf " %g", max[i] > "/dev/stderr";
    printf "\nrng:" > "/dev/stderr";
    for (i in headers) if (int(i) > 2) printf " %g", range[i] > "/dev/stderr";
    printf "\n" > "/dev/stderr";
    
    for (i in headers) printf "%s ", headers[i];
    printf "\n";
    for (r in data) {
      for (i in headers) {
        if (int(i) <= 2)
          printf "%s", data[r][i];
        else
          printf " %g", (data[r][i] - min[i]) / range[i];
      }
      printf "\n";
    }
  }
  ' $stats 2>&1 > $normalized | column -t
  

# tail -n +2 $normalized | head -n 1 | while read line
# do
#   id=$(cut -d ' ' -f1 <<< "$line")
#   readarray -d ' ' -t data < <(cut -d ' ' -f2- <<< "$line" | tr -d '\n')
#   
#   echo "ID'$id' array(${data[@]})"
# done

python3 - <<__DOC
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

data=pd.read_csv('$normalized', delimiter=' ')
data.dropna(axis=1, inplace=True)

angles_edge = np.linspace(0.0, 2 * np.pi, data.shape[1]-1, endpoint=False)
angles_center = angles_edge + np.pi / len(angles_edge)

width = 2*np.pi / (data.shape[1]-1)
colors = plt.cm.hsv(angles_center / (2*np.pi)) 

prettyHeaders ='$pretty_names'.split(",")
print(data.columns[1:], '>>', prettyHeaders)

data.sort_values(by='ID', inplace=True)

plt.rcParams['figure.constrained_layout.use'] = True
ax = plt.subplot(projection='polar')

ax.set_ylim(0, 1)
# ax.set_theta_offset(np.pi - np.pi/6)

ax.set_xticks(angles_edge)
ax.set_xticklabels([])
ax.set_xticks(angles_center, minor=True)
ax.set_yticks(np.linspace(0, 1, 6))
ax.set_yticklabels([])

ax.grid(which='major', visible=False, axis='x')
ax.grid(which='minor', visible=True, axis='x')
ax.grid(which='major', visible=True, axis='y', linestyle=':')
ax.spines['polar'].set_visible(False)  
# ax.xaxis.set_tick_params(pad=0)

for a, h in zip(angles_edge, prettyHeaders):
  a_ = np.rad2deg(a)
  a_ = a_-180 if a_ > 180 else a_
  ax.text(a, 1.075, h, ha='center', va='center', rotation=a_-90)

rects = ax.bar(angles_edge, data.iloc[0,1:], width=width, bottom=0.0, color=colors, alpha=1)

# plt.tight_layout()
  
for r in range(data.shape[0]):
  for rect, v in zip(rects, data.iloc[r,1:]):
    rect.set_height(v)

  o = "$ofolder/{}.png".format(data.iloc[r,0].replace('/', ''))
  print(r, o, end='\r')
  plt.savefig(o, dpi=150)
  
print('Done')
__DOC
