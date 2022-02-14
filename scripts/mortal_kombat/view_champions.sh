#!/bin/bash

folder=tmp/mk-evo/last/
if [ ! -z $1 ]
then
  folder=$1
  shift
fi
if [ ! -d "$folder" ]
then
  echo "Data folder '$folder' does not exist"
  exit 1
fi

count=2
if [ ! -z $1 ]
then
  count=$1
  shift
fi

sep(){ printf "%s\n" "----"; }
for p in A B
do 
  ls $folder/$p/gen[0-9]*/mk*.dna \
  | sed 's/.*gen\([0-9]*\).*mk__\(.*\)_0.dna/& \1 \2/' \
  | sort -k3,3g -k2,2g \
  | tail -n $count \
  | while read dna gen fitness
  do
    echo
    sep
    printf "$dna\n"
    sep
    printf "gen: %d\n" $gen
    printf " mk: %g\n" $fitness
    sep
    ./scripts/mortal_kombat/evaluate.sh --gui $dna = --auto-quit $@
    sep
  done
done
