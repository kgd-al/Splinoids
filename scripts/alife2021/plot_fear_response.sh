#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Provided strategies log '$1' is not valid"
  exit 1
fi

tmp=.input
head -n51 $1 | tail -n+2 | cut -d ' ' -f 2,9,10 | sort -k2,2gr | nl -w1 -s' ' > $tmp
# cat $tmp
column -t $tmp
classes=$(cut -d ' ' -f 4 $tmp | sort -u)

cmd="
  set term pdfcairo size 11.2,7 font ',24';
  set output '$(dirname $1)/fear_response.pdf';
  set style fill transparent solid .3;
  set key above;
  set xtics rotate 90;
  set xlabel 'Runs';
  set ylabel 'Fear response';
  
  plot for [c in '$classes'] '< grep '.c.' $tmp' using 1:3:(1):xtic(2) with boxes title c;
  "
  
printf "%s\n" "$cmd"
gnuplot -e "$cmd"
rm $tmp
