#!/bin/bash

prefix=$1
[ "${prefix: -1}" != '_' ] && prefix=$1"_"

cppn=${prefix}cppn.dot
cppn_png=${prefix}cppn.png
[ -f "$cppn" ] && dot $cppn -Tpng -o $cppn_png > log

ann=${prefix}ann.dat
if [ -f "$ann" ]
then
  if [ "$2" = "-p" ]
  then
    output=""
    persist="-"
  else
    output="set term pngcairo size 1680,1050 font ',24'; set output '${prefix}ann.png';"
    persist=""
  fi

  view=""
  [ ! -z "$3" ] && view="set view $3;"
  
#     set xrange [-1:1];
#     set yrange [-1:1];
#     set zrange [-1:1];
#     set view equal xyz;
  cmd="
    $output;
    $view;
    argb(w) = ((int(255*(1-(w > 0 ? w : -w))))<<24) + (int(w > 0 ? 255*w : 0)<<16) + (int(0)<<8) + int(w < 0 ? -255*w : 0);
    splot '$ann' with points lc variable ps 2 pt 7 notitle, \
              '' using 1:2:3:4:5:6:(argb(\$7)) every :::1 with vectors lc rgb variable notitle;"
  #   while 1 { replot; pause 10; }"

  gnuplot -e "$cmd" $persist
fi
