#!/bin/bash

usage(){
  echo "Usage: $0 <genealogy.dat>"
  echo
  echo "       Plots the vector field of populations accross generations"
}

if [ ! -f "$1" ]
then
  usage
  exit 1
fi

folder=$(dirname $1)
# head $1
# 
# o=$(dirname $1)/$(basename $1 .dat).png
# gnuplot -e "
#   set term pngcairo size 1680,1050;
#   set output '$o';
#   
#   unset key;
#   plot '$1' using 1:2:(1):(\$4-\$2):6 with vectors nohead lc palette z;
#   
#   print('Generated $o');"

ls -v $folder/gen[0-9]*/lg*.dna \
| xargs jq -r '"\(.id[0]) \(.id[1]) \(.parents[0][1]) \(.fitnesses.lg)"' \
| tee $folder/lineage.dat | column -t | grep "^12[0-9]"

o=$(dirname $1)/$(basename $1 .dat).png
gnuplot -e "
  set term pngcairo size 1680,1050;
  set output '$o';
  
  unset key;
  set xlabel 'Generations';
  set ylabel 'Index';
  set clabel 'Fitness';
  plot '$folder/lineage.dat' using 1:3:(1):(\$2-\$3):4 with vectors lc palette z;
  
  print('Generated $o');"
