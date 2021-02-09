#!/usr/bin/gnuplot


file=ARG1
bins=ARG2
output=ARG3
commands=ARG4

print "      script : ", ARG0
print " input file  : ", file
print "        bins : ", bins
print " output file : ", output
print "     commands: ", commands

if (ARGC == 0) {
  print "Generates a histogram from raw data"
  exit
}

min=system('sort '.file.' -g | cut -d: -f2 | head -1')
max=system('sort '.file.' -g | cut -d: -f2 | tail -1')

print min, '-', max;

width=(max-min)/bins;
bin(x) = width*(floor((x-min)/width)+0.5) + min;
set boxwidth width;

set term pngcairo size 1680,1050 font ',24';
set output output

set style fill solid .25;

eval commands

set title noenhance file;
if (min < max) {
  plot file using (bin($1)):(width) smooth freq with boxes notitle
} else {
  set label 'Cannot represent a single value ('.min.')' at (0,0);
  set xrange [-1:1];
  set yrange [-1:1];
  plot 2
}
