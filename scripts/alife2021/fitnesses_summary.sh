#!/bin/bash

if [ $# -lt 1 -o $# -gt 2 ]
then
  echo "Usage: $0 <gen_stats.csv> [ext]"
  exit 1
fi

f=$1
if [ ! -f "$f" ]
then
  echo "File '$1' does not exist"
  exit 2
fi

ext="png"
[ ! -z "$2" ] && ext=$2
o=$(dirname $f)/$(basename $f .csv).$ext
echo "$f -> $o"

echo $o >> .generated_files
if [ -f "$o" ]
then
  echo "Output file $o already generated. Skipping"
  exit 0
fi

ptype="with lines"
# ptype="smooth bezier"
    
c=$(head -n 1 $f | awk -F, '{print (NF-15)/3}')
layout=$(echo $(($c+1)) | awk '{a=int(sqrt($1)); printf "%d,%d", a, $1/a;}')
    
cmd="
    set term ${ext}cairo size 1680,1050;
    set output '$o';
    set datafile separator ',';
    set multiplot layout $layout;
    f='$f';
    
    titles=system('head -n 1 '.f.' | tr \",\" \" \"');
    title(i)=word(titles, int(i));

    type(j)=system('echo '.title(j).' | cut -d_ -f2');
    subtype(j)=system('echo '.title(j).' | cut -d_ -f2');
    
    set key above title 'aggregate';
    set yrange [-1:$(($c*10))];
    set y2range [-1/$(($c*10)):1];
    set y2tics (0,1);
    plot for [i in '2 0 1'] f using 1:(column($(($c*3+5))+int(i))) $ptype t type(29+i), f using 1:$(($c*3+2)) w l axes x1y2 t 'brain?';
    set yrange [*:*];
    set y2range [*:*];
    unset y2tics;    
    
    do for [i=0:$c-1] {
      ix=3*i+2;
      set key above title type(ix);
      plot for [j in '2 0 1'] f using 1:(column(ix+j)) $ptype title subtype(ix+j);
    };
    unset multiplot;
"

# echo "Executing $cmd"
gnuplot -e "$cmd"
