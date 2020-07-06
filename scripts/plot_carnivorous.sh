#!/bin/bash

f=$1
# echo "base folder is $f"
(head -n1 "$f/results/e00_r/fitnesses.dat"; tail -qn +2 $f/results/e*_r/fitnesses.dat) | tr -d "\0" > "$f/fitnesses.dat"

if [ "$2" -gt 0 ] 2>/dev/null
then
  preloop="while (1) {"
  postloop="    pause $2;
  }"
else
  out="set term pngcairo size 1680,1050; set output '$f/fitnesses.png';"
fi

cmd="
  c=3;
  w=3;
  set ylabel 'Carnivorous (%)';
  set y2label 'Population size';
  set xlabel 'Generation';
  set key above;
  set ytics nomirror;
  set y2tics;
  set grid;
  $out 
  $preloop
    set title '$f @ $(date)' noenhanced;
    plot for [i=0:1] '$f/fitnesses.dat' using 1:w*i+2 with lines ls i+1 axes x1y2 title columnhead,
         for [i=0:1] '' using 1:(100*column(w*i+c)) with lines ls i+1 dt 3 title columnheader(w*i+c);
  $postloop
"

[ ! -z ${VERBOSE+x} ] && echo "$cmd"

gnuplot -e "$cmd"
