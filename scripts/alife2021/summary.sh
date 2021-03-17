#!/bin/bash

src=results/alife2021/v2n/ann_stats_normalized
for c in $(seq 2 $(head -n1 $src | wc -w))
do
  name=$(head -n1 $src | cut -d ' ' -f $c)
  cmd="
    set term pngcairo size 840,525;
    set output 'results/alife2021/v2n/ann_stats_$name.png';"
      
  if [ $c -eq 5 ]
  then
    cmd="$cmd
      min=1;
      bwidth=1;
      bin(x) = bwidth * floor(x/bwidth);
      
      set title '$name';
      set style fill transparent solid .3;
      set boxwidth bwidth;
      
      set xlabel 'Number of modules';
      set ylabel 'Frequency';
      
      plot '$src' using (bin(\$$c)):(1.0) smooth freq with boxes notitle;
    "
  
  else
    cmd="$cmd
      bins=20;
      min=$(cut -d ' ' -f $c $src | sort -g | head -n 2 | tail -n 1);
      max=$(cut -d ' ' -f $c $src | sort -g | tail -n 1);
      bwidth=(max-min)/bins;
      bin(x) = bwidth * (floor((x-min)/bwidth)+.5) + min;
      
      set title '$name';
      set style fill transparent solid .3;
      set boxwidth bwidth;
      
      set xlabel 'Value (%)';
      set ylabel 'Frequency';
      
      plot '$src' using (bin(\$$c)):(1.0) smooth freq with boxes notitle;
    "
  fi
  printf "%s\n" "$cmd"
  gnuplot -e "$cmd" || exit 3
done
