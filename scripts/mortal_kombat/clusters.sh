#!/bin/bash

folder=$1
if [ ! -d "$folder" ]
then
  echo "data folder '$folder' does not exist"
  exit 1
fi

name=clusters
o=$folder/clusters.dat
o2=$(dirname $o)/.$(basename $o)
awk '
  NR == 1 {
    for (i=2; i<=NF; i++)
      headers[i] = $i;
  }
  FNR == 1{
    f=FILENAME;
    split(FILENAME, tokens, "/");
    f=substr(tokens[5], 3)"/"tokens[6];
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
      total = t[f];
      if (total == 0) total = 1;
      for (i in headers)  printf " %d", d[f][i];
      printf "\n"
    }
  }
  ' $1/ID*/[A-Z]/gen1999/mk*/neural_groups.dat > $o
  
if [ ! -z ${NO_PINKY} ]
then
  name="${name}_nopinky"
  echo "Removing pinkies"
  awk '
  NR==1;
  NR>1{
    split($1, ids, "/");
    d[ids[1]][ids[2]] = $0
    t[ids[1]][ids[2]] = $2
  } END {
    for (r in t) {
      max = 0
      idmax = -1
      for (p in t[r]) {
        if (t[r][p] > max) {
          max = t[r][p]
          idmax = p
        }
      }
      print d[r][idmax]
    }
  }' $o > $o2
  mv -v $o2 $o
fi
  
if [ -f "$2" ]
then
  join $2 $o
  gnuplot -e "
    cols=system('head -n 1 $o');
    do for [i=2:words(cols)] {
      set output '$folder/$name.'.word(cols, i).'.scatter.png';
      set term pngcairo size 1680, 1050;
      
      set title word(cols, i);
      plot '<join --nocheck-order $2 $o' using 2:i+1 notitle;
    };
  "

  name="${name}_fsort"
  echo "Sorting by '$2'"
  awk 'FNR==NR{row[$1]=NR;next}{print row[$1], $0}' <(sort -k2,2gr $2) $o \
  | sort -k1,1g | cut -d ' ' -f2- > $o2
else
  echo "Sorting by network size"
  head -n 1 $o > $o2
  tail -n +2 $o | sort -k2,2gr >> $o2
fi
mv -v $o2 $o
column -t $o

img="$folder/$name.png"
cols=$(head -n 1 $o)
echo "columns:" $cols
gnuplot -e "
  set output '$img';
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
echo "Generated '$img'"
