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
        | sed 's/0x1b\[[0-9]*m//' | tr -d "\n"
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
if [ ! -f $matrix ]
then
  awk '
  {d[$1][$2]=$3;}
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
gnuplot -e "
    set output '$img';
    set term pngcairo size 1050,1050;
    set xtics rotate 90 right;
    set palette defined (-4 'white', -2 'white', -2 'red', -1 'red', 0 'white', 1 'green', 2 'green');
    plot '$matrix' matrix columnheader rowheader with image;
  "
echo "Generated '$img'"
