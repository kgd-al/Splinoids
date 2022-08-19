#!/bin/bash

evals=$(ls $1 | wc -l)

log=.mmann.log
date > $log

dolog(){
  echo "$1 $dna" | tee -a $log
}

premature_exit(){
  if [ -n "$dna" ]
  then
    echo "Exited prematurely while processing $dna"
  else
    echo "Done"
  fi
}
trap premature_exit EXIT
trap premature_exit SIGTERM

for dna in $1
do
  echo "================================================================================"  >> $log
  echo "================================================================================"  >> $log
  dolog "Processing"
  echo "================================================================================"  >> $log
  echo "Generating behavior" >> $log
  FIRST_ONLY=yes $(dirname $0)/generate_behavior.sh $dna >> $log 2>&1
  echo "================================================================================"  >> $log
  echo "Plotting behavior" >> $log
  FIRST_ONLY=yes $(dirname $0)/plot_behavior.sh $(dirname $dna)/$(basename $dna .dna) >> $log 2>&1
  echo >> $log
done | pv -lN "Processing" -s $evals >/dev/null
dna=""

base=$(ls $1 | head -n1 | sed 's|\(.*/ID[0-9]*/[A-Z]/\).*|\1|')

awk '
  FNR==1{
    match(FILENAME, /gen([0-9]*)/, tokens);
    gen=tokens[1];
    t[gen][-1]=0;
  }!/^\//{
    t[gen][$2]++;
    t[gen][-1]++;
  }END{
    printf "Gen Total / T V TV A TA VA TVA\n";
    for (g in t) {
      printf "%d %d", g, t[g][-1];
      for (i=0; i<=7; i++) printf " %d", t[g][i];
      printf " %d", t[g][0];
      printf " %d", t[g][1]+t[g][2]+t[g][4];
      printf " %d", t[g][3]+t[g][5]+t[g][6];
      printf " %d", t[g][7];
      printf "\n";
    }
  }' $base/gen*/mk_*_0/eval_first_pass/neural_groups.ntags > $base/mann_lineage.dat
#   
# gnuplot -c /dev/stdin <<__DOC
#   set term pdfcairo size 7,5.5;
#   set output '$base/mann_lineage.pdf'
#   
#   set style data histograms;
#   set style histogram rowstacked;
#   set style fill solid .25;
#   set key above;
#   set xlabel 'Generation';
#   set ylabel 'Neurons';
#   color(i) = int((i&1 ? 0x00007F : 0) + (i&2 ? 0x007F00 : 0) + (i&4 ? 0x7F0000 : 0));
#   
#   label(g) = (int(g)%10 == 0 ? sprintf("%d", column(1)) : 1/0);
#   
# #   plot for [i=0:7] '$base/mann_lineage.dat' using i+2:(color(i)):xticlabels(label(\$0)) lc rgbcolor variable title title(i) enhanced;
# 
#   set y2tics;
#   set key title 'Modules types';
#   plot for [i=1:3] '$base/mann_lineage.dat' using i+10:xticlabels(label(\$0)) lc i+1 title columnhead(i+10), \
#        '' using (\$0-1):10 lc rgb '0x7F7F7F' axes x1y2 with lines title 'None';
# __DOC
# 
# echo "Generated '$base/mann_lineage.pdf'"

top=20
c=4

awk '
  NR==1{print $1, "NT NV NA Mod Mod%"; next;}
  {
    t = $2;
    nt=$4+$6+$8+$10;
    nv=$5+$6+$9+$10;
    na=$7+$8+$9+$10;
    printf "%d %d %d %d ", $1, nt, nv, na;
    if (t > 0) {
      mod=(nt+nv+na)/3;
      mod_p=100*mod/t;
      printf "%.1f %2.1f\n", mod, mod_p
    } else
      printf "NaN NaN\n";
  }' $base/mann_lineage.dat > $base/mann_lineage.stats.dat
  
# echo "Top $top:"
sort -k$c,${c}g $base/mann_lineage.stats.dat | tail -n$top | cut -d ' ' -f1 | sort -n > $base/mann_lineage.top
ind=$(awk -vb=$base '{print b"/gen"$1"/mk*_0.png"}' $base/mann_lineage.top)
mann=$(awk -vb=$base '{print "-label "$1" "b"/gen"$1"/mk*/eval_first_pass/mann.png"}' $base/mann_lineage.top)
montage -title "$(head -n 1 $base/mann_lineage.stats.dat | cut -d' ' -f$c) " -tile x2 $ind $mann $base/mann_lineage.top.png
