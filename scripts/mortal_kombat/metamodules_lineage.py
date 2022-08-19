#!/usr/bin/python3

import sys
import pathlib

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable

i_f=sys.argv[1]
o_f=pathlib.Path(i_f).with_suffix('.pdf')
df = pd.read_table(i_f, sep=' ').dropna()

plt.rcParams.update({'font.size': 18})

binwidth=25

ycol = 'Mod'
if len(sys.argv) >= 3:
  try:
    col = sys.argv[2]
    df[col].shape
    ycol = col
  except:
    print('Column', col, 'not found in the database. Defaulting to', ycol)
    print(list(df.columns))
    pass
  
xmin=df['Gen'].min()
xmax=df['Gen'].max()
binxcount=int(((xmax-xmin)/binwidth))

#ycol = 'Balanced'
ymin = df[ycol].min()
ymax = max(df[ycol].max(), 149)
#binycount=10
#binheight= round((ymax - ymin) / binycount)

binheight=10
binycount=int(((ymax-ymin)/binheight))

#print(binheight)

bins = np.zeros((binycount+1, binxcount+1))
#print(bins.shape)

for r in df.iterrows():
  gen=r[1]['Gen']
 
  xbin=int((gen-xmin)/binwidth)  
  ybin=int((r[1][ycol]-ymin)/binheight)

  #print(gen, r[1][ycol], xbin, ybin)
  bins[ybin][xbin] += 1

#print(bins)
cmax = bins.max()
bins[bins==0] = np.nan

#print(np.apply_along_axis(sum, 0, bins))
#print(np.apply_along_axis(sum, 1, bins))
#b = pd.DataFrame.from_dict(bins)
#b = b.sort_index()
#print(b)

fig, ax = plt.subplots(figsize=(10,4))
mat = ax.matshow(bins, origin='lower', cmap=plt.get_cmap('plasma'))

xticks = 9
ax.xaxis.tick_bottom()
ax.set_xlabel('Generations')
ax.set_xticks(np.linspace(0, bins.shape[1]-1, xticks),
              np.linspace(xmin-1, xmax+1, xticks, dtype=int))
yticks = 4
ax.set_ylabel('Modularity')
ax.set_yticks(np.linspace(0, bins.shape[0]-1, yticks),
              (np.linspace(0, bins.shape[0], yticks) * binheight + ymin).astype(int))

cticks = 4
#print(cmax)
divider = make_axes_locatable(ax)
cax = divider.append_axes("right", size="5%", pad=0.05)

cbar = plt.colorbar(mat, cax=cax, ticks = np.linspace(1, cmax, cticks))
cbar.set_label('Frequency')

plt.tight_layout()
plt.savefig(o_f, bbox_inches='tight', pad_inches=.1)
print(f"Generated '{o_f}'")
