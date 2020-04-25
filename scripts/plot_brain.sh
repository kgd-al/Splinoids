#!/bin/bash

prefix=$1
[ "${prefix: -1}" != '_' ] && prefix=$1"_"

cppn=${prefix}cppn.dot
cppn_pdf=${prefix}cppn.pdf
if [ -f "$cppn" ]
then
  dot $cppn -Tpdf -o $cppn_pdf > log
  xdg-open $cppn_pdf
  ls -lh $cppn $cppn_pdf
fi

ann=${prefix}ann.dat
if [ -f "$ann" ]
then
  cmd="
    set title 'Neural connections';
    argb(w) = ((int(255*(1-(w > 0 ? w : -w))))<<24) + (int(w > 0 ? 255*w : 0)<<16) + (int(0)<<8) + int(w < 0 ? -255*w : 0);
    splot '$ann' lc variable with points notitle, \
              '' using 1:2:3:4:5:6:(argb(\$7)) every :::1 with vectors lc rgb variable notitle;"
  #   while 1 { replot; pause 10; }"

  gnuplot -e "$cmd" -
fi
