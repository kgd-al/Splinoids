#!/bin/bash

base=../alife2022_results/
awk '
  FNR==1{
    match(FILENAME, "ID79([0-9]*/[A-Z])", tokens);
    id=tokens[1];
    match(FILENAME, "v1-(t[12]p[23])", tokens);
    types[id]=tokens[1];
  }!/^\//{
    t[id][$2]++;
  }END{
    printf "Id type / T V TV A TA VA TVA NT NV NA Mod Mod%\n";
    for (id in t) {
      printf "%s %s", id, types[id];
      sum=0;
      for (i=0; i<=7; i++) {
        printf " %d", t[id][i];
        sum += t[id][i];
      }
      nt=t[id][1]+t[id][3]+t[id][5]+t[id][7];
      nv=t[id][2]+t[id][3]+t[id][6]+t[id][7];
      na=t[id][4]+t[id][5]+t[id][6]+t[id][7];
      printf " %d %d %d %.1f %2.1f\n", nt, nv, na, (nt+nv+na)/3, 100*(nt+nv+na)/(3*sum);
    }
  }' $base/v1*/ID*/*/gen1999/mk_*_0/eval_first_pass/neural_groups.ntags \
  | sort -k14,14g | tee $base/analysis/metamodules/distribution.dat | column -t

  
base=$base/analysis/metamodules/
# gnuplot -c /dev/stdin << __DOC
#   set term pdfcairo size 15,3;
#   set output '$base/distribution.pdf';
#   
#   set style fill solid .25;
#   set xtics rotate 90;
#   
#   plot '$base/distribution.dat' using 0:14:xticlabels(1) with boxes lc 2 notitle;
#   
# __DOC

python3 - <<__DOC
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import scipy.stats

plt.rcParams.update({'font.size': 18})

data = pd.read_csv('$base/distribution.dat', delimiter=' ', index_col='Id')

champions=['8167/B','8074/B','9055/C','7308/B']
print('\nChampions:')
print(data.loc[champions])

keys = data['type'].unique()
pkeys = np.vectorize(lambda s: s.upper())(keys)

for col, label in [('Mod', 'Modularity'), ['Mod%', 'Modularity (%)']]:
  fig, ax = plt.subplots()
  
  output=f'$base/'+col.replace('%', 'p')+'_distribution.violin.pdf'
  lists = []
  for t in keys:
    lists.append(data[data['type'] == t][col].values)

  for i in range(len(lists)):
    lhs = lists[i]
    for j in range(len(lists)):
      rhs = lists[j]
      if True: #not too_small(lhs, rhs):
        U, p = scipy.stats.mannwhitneyu(lhs, rhs, alternative='less')
        if p < .025:
          print(f'mannwhitneyu({col}, {keys[i]} < {keys[j]}): U = {U}, p = {p}')

  ax.violinplot(lists, showextrema=False)
  ax.boxplot(lists, widths=.15, whis=[5, 95])
  ax.set_xticks([i+1 for i in range(len(keys))])
  ax.set_xticklabels(pkeys, weight = 'bold')
  ax.set_ylabel(label)
  
  for c in champions:
    ax.plot(list(keys).index(data.loc[c]['type'])+1, data.loc[c][col], '.g')

  plt.tight_layout()
  plt.savefig(output, dpi=150)
  plt.close()

  print('Generated', output)

__DOC
