#!/bin/bash

folder='tmp/timelines_test'
[ ! -z "$1" ] && folder=$1

gnuplot -e "
  c=3;
  w=3;

  set xlabel 'Generation';
  
  set key above;
  set ytics nomirror;
  set y2tics;
  set grid;
  
  while (1) {
    f=system('ls -dt $folder* | head -1');
    system('(head -n1 '.f.'/results/e00_r/fitnesses.dat; tail -qn +2 '.f.'/results/e*_r/fitnesses.dat) | tr -d \"\0\" > '.f.'/fitnesses.dat');
    system('awk \"{print NR, \\\$0}\" '.f.'/results/e*_r/env.dat > '.f.'/env.dat');
    system('awk \"NR==1||FNR>1\" '.f.'/results/e*r/global.dat > '.f.'/global.dat');
    
    set multiplot layout 1,2 title f.' @ '.system('date') noenhanced;

    set ylabel 'Carnivorous (%)';
    set y2label 'Population size';
    plot for [i=0:1] f.'/fitnesses.dat' using 1:w*i+2 with lines ls i+1 dt 3 axes x1y2 title columnhead,
         for [i=0:1] '' using 1:(100*column(w*i+c)) with lines ls i+1 title columnheader(w*i+c);
         
    set ylabel '?';
    set y2label 'Energy';
    plot f.'/env.dat' using 1:2 with lines title 'mvp',
         for [i=11:14] f.'/global.dat' using 1:i with lines axes x2y2 title columnheader;
    unset multiplot;
    
    pause 10;
  }" -p -
