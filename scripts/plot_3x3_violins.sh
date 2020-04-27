#!/bin/bash

file=$1
folder=$(dirname $1)
base=$folder/$(basename $1 .dat)
out=$base.png

rows=$(cut -d ' ' -f 1 $file | sort | uniq)
cols=$(cut -d ' ' -f 2 $file | sort | uniq)
lmargin=$(cut -d ' ' -f 3 $file | sort -g | tail -1 | awk '{ v=log($1) / log(10); iv = int(v); w = (iv < v) ? iv+1 : v; print .08+.05*w; }')
echo $lmargin

size='840 350'
tmp=.gnuplot.tmp
lastrow=$(echo $rows | awk '{print $(NF) }')
for r in $rows
do
  grep "^$r" $file | cut -d ' ' -f 2,3 > $tmp
  tmpimg=${base}_row$r.png
  
  tics="unset xtics"
  [ "$r" == "$lastrow" ] && tics=""
  gnuplot -c ./scripts/violin_plot.gp $tmp $2 $tmpimg \
    "$tics; 
     set term pngcairo size $(tr ' ' ',' <<< $size) font \",24\";
     set lmargin at screen $lmargin;
     set rmargin at screen .99;
     set bmargin at screen 0.15;
     set tmargin at screen 0.95;
     set ylabel '$r';
     $3"
  rm $tmp
done

montage -tile 1x3 -geometry "$(tr ' ' 'x' <<< "$size")+0+0" ${base}_row*.png $out
rm ${base}_row*.png
