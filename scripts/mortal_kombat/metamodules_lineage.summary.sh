#!/bin/bash

S=25
[ -n $1 ] && S=$1
echo $S

for f in ../alife2022_results/*/ID*/*/mann_lineage.stats.dat
do
  awk -vS=$S 'NR>1{i=int((NR-1)/S); if (i==(NR-1)/S) g[i] = $1; if (m[i] < $4) m[i] = $4;}END{for(i in m) print i, g[i], m[i];}' $f \
  > $(dirname $f)/$(basename $f .stats.dat).sws.dat
done

gnuplot -c /dev/stdin << __DOC
  o = '../alife2022_results/analysis/metamodules/sliding_window_summary.pdf'
  set term pdfcairo;
  set output o;
  
  set key above;
  
  set xlabel 'Generation';
  set ylabel 'Modularity';
  
  files=system('ls ../alife2022_results/*/ID*/*/mann_lineage.sws.dat');
  title(f)=system('echo '.f.' | sed "s|.*/ID79\([0-9]*/[A-Z]\)/.*|\1|"');
  nfiles=words(files);
  plot for [i=1:nfiles] word(files, i) using 2:3 with linespoints pt 7 ps .25 dt 2 lw .5 lc i notitle, \
       for [i=1:nfiles] word(files, i) using 2:3 smooth bezier lc i title title(word(files, i));
  
  print('Generated', o);

__DOC
