#!/bin/bash

if [ ! -d "$1" ]
then
  echo "Please provide a valid evaluation directory in which I can find champ_A*.dat files"
  exit 1
fi

r=$(basename $1 | tr "_" " ")

#   set title '$r Foraging';
#   set title '$r Strategy';

pstyle="with circles lc rgb 'green' fs solid"
tstyle="with lines lc palette z"
vstyle(){
  echo "((\$0>$1) && (int(\$0)%$2==0) ?\$1:1/0):2:(cos(\$3)):(sin(\$3)):0 with vectors lc palette z"
}

gnuplot -e "
  set terminal pngcairo size 1680,1050 font ',24';
  set output '$1/champ_foraging.png';
  set size square;
  set xlabel 'x';
  set ylabel 'y';
  set xrange [-20:20];
  set yrange [-20:20];
  set cblabel 'step';
  set key above;
  set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
  set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
  plot for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::::0 $pstyle notitle, \
       1/0 $tstyle title 'trajectories', \
       1/0 $pstyle title 'plants', \
       for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::1 using 1:2:0 $tstyle notitle, \
       for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::1 using $(vstyle 10 10) notitle;
  set xrange [-50:50];
  set yrange [-50:50];
  set output '$1/champ_strategy.png';
  plot '$1/champ_strategy.dat' every :::1 using 1:2:0 $tstyle title 'trajectory', \
                            '' every :::::0 $pstyle title 'plants',
                            '' every :::1 using $(vstyle 0 50) notitle;"
