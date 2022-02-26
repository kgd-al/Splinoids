#!/bin/bash

folder=$1
if [ ! -d "$folder" ]
then
  echo "data folder '$folder' does not exist"
  exit 1
fi

o=$folder/clusters.dat
awk '
  NR == 1 {
    for (i=2; i<=NF; i++)
      headers[i] = $i;
  }
  FNR == 1{
    f=FILENAME;
    split(FILENAME, tokens, "/");
    f=substr(tokens[4], 3)"/"tokens[5];
    t[f] = 0;
    for (i in headers) d[f][i] = 0;
    next;
  }
  {
    t[f]++;
    for (i=2; i<=NF; i++) d[f][i]+=$i;
  }
  END {
    printf "- Total";
    for (i in headers) printf " %s", headers[i];
    printf "\n";
    PROCINFO["sorted_in"]="@ind_str_asc";
    for (f in t) {
      printf "%s %d", f, t[f];
      for (i in headers)
        printf " %d", d[f][i];
      printf "\n"
    }
  }
  ' $1/ID*/[A-Z]/gen1999/mk*/neural_groups.dat > $o
  
o2=$(dirname $o)/.$(basename $o)
head -n 1 $o > $o2
tail -n +2 $o | sort -k2,2gr >> $o2
mv -v $o2 $o
column -t $o
 
cols=$(head -n 1 $o)
echo "columns:" $cols
gnuplot -e "
  set output '$folder/clusters.png';
  set term pngcairo size 1680,1050;
  set style fill solid .5;
  
  cols='$cols';
  ncols=words(cols);
  set multiplot layout ncols-1, 1 margins .05, .98, .1, .99 spacing 0, 0;
  
  set xrange [.5:$(wc -l $o | cut -d ' ' -f1)-.5];
  set yrange [0:*];
  set ytics rangelimited;
  set offset 0, 0, graph 0.05, 0;
  
  set grid noxtics ytics;
  unset xtics;
  
  set arrow from graph .5, graph 0 to graph .5, graph 1 lc 'gray' nohead;
  
  do for [i=2:ncols] {
    set y2label word(cols, i);
    if (i == ncols) { set xtics out rotate by 45 right noenhanced nomirror; };
    plot '$o' using 0:i:(1):xtic(1) with boxes ls i-1 notitle;
  };
  
  unset multiplot;"
  
