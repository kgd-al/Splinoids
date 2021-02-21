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

ptype="with lines"
ptype="smooth bezier"
    
cmd="
    set term ${ext}cairo size 1680,1050;
    set output '$o';
    set datafile separator ',';
    set multiplot layout 2,2;
    f='$f';
    
    titles=system('head -n 1 '.f.' | tr \",\" \" \"');
    title(i)=word(titles, int(i));

    type(j)=system('echo '.title(j).' | cut -d_ -f2');
    subtype(j)=system('echo '.title(j).' | cut -d_ -f3');
    
    set key above title 'aggregate';
    set yrange [-1:30];
    set y2range [-1/30:1];
    set y2tics (0,1);
    plot for [i in '16 14 15'] f using 1:(column(int(i))) $ptype t type(i), f using 1:11 w l axes x1y2 t 'brain?';
    set yrange [*:*];
    set y2range [*:*];
    unset y2tics;    
    
    do for [i in '2 5 8'] {
        set key above title type(i);
        plot for [j in '2 0 1'] f using 1:(column(int(i)+int(j))) $ptype title subtype(i+j);
    };
    unset multiplot;
"

echo "Executing $cmd"
gnuplot -e "$cmd"
