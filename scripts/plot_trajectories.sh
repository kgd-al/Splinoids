#!/bin/bash

if [ ! -d "$1" ]
then
  echo "Please provide a valid evaluation directory in which I can find champ_A*.dat files"
  exit 1
fi

r=$(basename $1 | tr "_" " ")

gnuplot -e "
  set terminal pngcairo size 1680,1050;
  set output '$1/champ_foraging.png';
  set title '$r Foraging';
  set size square;
  set xrange [-50:50];
  set yrange [-50:50];
  plot for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::::0 with circles title 'plant', \
       for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::1 using 1:2:0 with lines lc palette z title 'trajectory';
  set title '$r Strategy';
  set output '$1/champ_strategy.png';
  plot '$1/champ_strategy.dat' every :::::0 with circles title 'plants', \
                            '' every :::1 using 1:2:0 with lines lc palette z title 'trajectory';"
