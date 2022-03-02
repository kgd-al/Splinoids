#!/bin/bash

pattern="$1"
if [ -z "$pattern" ]
then
  echo "No file pattern provided. How am I supposed to work ?"
  exit 1
else
  files=$(ls $pattern)
  nfiles=$(ls $pattern | wc -l)
  echo "Found $nfiles using pattern '$pattern'"
fi

name=fight.matrix
[ ! -z "$2" ] && name=$2
base=results/mortal_kombat/$name

nevals=$(($nfiles * $nfiles))

id(){
  readlink -m "$1" \
  | sed 's|\(.*\)/ID\(.*\)/\([A-Z]\)/gen\(.*\)/.*.dna|\2/\3|'
}

log=$base.log
if [ ! -f $log ]
then
  for lhs in $files
  do
    lname=$(id $lhs)
    for rhs in $files
    do
      rname=$(id $rhs)
      printf "$lname $rname"
      $(dirname $0)/evaluate.sh $lhs $rhs 2>&1 \
        | grep "mk:" | cut -d ':' -f 2 \
        | sed 's/\x1b\[[0-9]*m//' | tr -d "\n"
      printf "\n"
    done
  done | tee $log | awk -vN=$nevals '
    {printf "[%3.2f%%] %s %s %+g\r", 100*NR/N, $1, $2, $3;}'
else
  echo "Log file '$log' already generated"
fi

if [ $(wc -l $log | cut -d ' ' -f1) -ne $nevals ]
then
  echo "Log file contains $(wc -l $log) rows instead of $nevals expected evaluations"
  diff -y <(cut -d ' ' -f 1-2 $log) <(for l in $files; do for r in $files; do echo $(id $l) $(id $r); done; done) | less
  exit 2
fi

matrix=$base.dat
if : #[ ! -f $matrix ]
then
  awk '
  {d[$1][$2]=($3 < -2 ? 0 : $3);}
  END {
    PROCINFO["sorted_in"]="@ind_str_asc";
    printf "-";
    for (k in d) printf " %s", k;
      printf "\n";
    for (l in d) {
      printf "%s", l;
      for (r in d[l]) printf " %+ .2f", d[l][r];
      printf "\n";
    }
  }' $log | tee > $matrix
fi

ext=png
img=$base.png
tmpSumRows=$base.tmpSumRows
awk 'NR>1{s=0;for(i=2;i<=NF;i++)s+=$i; print NR, s/(NF-1);}' $matrix > $tmpSumRows
tmpSumCols=$base.tmpSumCols
awk 'NR>1{for(i=2;i<=NF;i++)d[i]+=$i;}END{for (i in d) print i, d[i]/(NR-1);}' $matrix > $tmpSumCols
gnuplot -e "
    set output '$img';
    set term pngcairo size 1500,1500;
    set xtics rotate right;
    set palette defined (-2 'red', -1 'red', 0 'white', 1 'green', 2 'green');
    unset key;
    
    set multiplot;
    
    M=.03;
    
    S = .8;
    MAIN = S-M;
    SCND = 1-S-M;
    CBW=1-S-.1; CBH=.05;
    
    set style fill solid .25 border -1;
    
    set margins 0, 0, M, M;
    unset xtics; unset ytics;
    unset colorbox;
    set size MAIN, MAIN; set origin 1-S, M;
    plot '$matrix' matrix columnheader rowheader with image,
                '' using (\$0-.5):(\$0-.5) with line lc -1 dt 3;
    
    set palette defined (-1 'red', 0 'red', 0 'green', 1 'green');
    
    set x2tics; set y2tics;
    set size MAIN, SCND; set origin 1-S, S;
    set x2range [-.5:$nfiles-.5];
    plot '$tmpSumCols' using 0:2:(1):2 with boxes axes x2y1 lc palette;
    unset xrange;
    
    unset x2tics; set xtics;
    unset y2tics; set ytics;
    set yrange [0:$nfiles-.5];
    w=1; hw = .5*w;
    set size SCND, MAIN; set origin M, M;
    plot '$tmpSumRows' using 2:0:(0):2:(\$0-hw):(\$0+hw):2 with boxxyerror lc palette;
  "
echo "Generated '$img'"
