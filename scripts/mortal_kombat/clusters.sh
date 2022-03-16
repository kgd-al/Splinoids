#!/bin/bash

folder=$1
if [ ! -d "$folder" ]
then
  echo "Data folder '$folder' does not exist"
  exit 1
fi

eval=$2
if [ -z $eval ]
then
  echo "No evaluation folder specified"
  exit 2
fi

name=$eval

func="id"
if [ ! -z ${TYPE} ]
then
  case $TYPE in
   id|percent|binary)  func=$TYPE
   ;;
   *) echo "Unknown histogram type '$TYPE'. Defaulting to value"
   ;;
  esac
fi
name="${name}_$func"

echo "Using function:" $func

folder=../alife2022_results/analysis/clusters/$(basename $folder | sed 's/v1-//')
mkdir -p $folder
o=$folder/$name.dat
o2=$(dirname $o)/.$(basename $o)
awk -vagg=$func '
  function id(v,t) { return v; }
  function percent(v,t) { return 100*v/t; }
  function binary(v,t) { return (v>0); }
  NR == 1 {
    for (i=2; i<=NF; i++)
      headers[i] = $i;
  }
  FNR == 1{
    f=FILENAME;
    match(FILENAME, "ID([0-9]*/[A-Z])", arr);
    f=arr[1];
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
    for (f in t) {
      printf "%s %d", f, t[f];
      total = t[f];
      if (total == 0) total = 1;
      for (i in headers)  printf " %d", @agg(d[f][i], total);
      printf "\n"
    }
  }
  ' $1/ID*/*/gen1999/mk*/eval_first_pass/$eval/neural_groups.dat > $o
  
echo "Sorting by network size"
head -n 1 $o > $o2
tail -n +2 $o | sort -k2,2gr >> $o2
mv -v $o2 $o
# column -t $o

img="$folder/$name.png"
cols=$(head -n 1 $o)
echo "columns:" $cols
fcols=$(awk '
  NR==1 {
    for (i=1; i<=NF; i++) h[i] = $i;
  }
  NR>1{
    for (i=2; i<=NF; i++) sum[i]+=$i;
  } END {
    printf "%s", h[1];
    for (i=2; i<=NF; i++) if (sum[i] > 0) printf " %s", h[i];
    print "\n";
  }' $o)
echo "non empty:" $fcols
gnuplot -e "
  set output '$img';
  set term pngcairo size 1680,1050;
  set style fill solid .5;
  
  cols='$fcols';
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
    c = word(cols, i);
    set y2label ''.c;
    if (i == ncols) { set xtics out rotate by 45 right noenhanced nomirror; };
    plot '$o' using 0:(column(c)):(1):xtic(1) with boxes ls i-1 notitle;
  };
  
  unset multiplot;"
echo "Generated '$img'"
