#!/bin/bash

log=$1/determinism.log
find $1 -name "mk*.dna" \
  | grep -v "gen0" \
  | sort -V \
  | while read dna
  do
    printf "%-80s\r" $dna >&2
    res=$(./scripts/mortal_kombat/evaluate.sh $dna 2>&1 | grep "mk:.*>>")
    res=$(sed -e 's/mk: //' -e 's/\x1b\[[0-9;]*m//g' <<< "$res")
    if [ ! -z "$res" ]
    then
      echo $(sed 's|.*/\([AB]\)/gen\([0-9]*\)/.*|\1 \2|' <<< $dna) \
           $(awk '{ d=$3-$1; printf " %+g (%g >> %g) %g", d, $1, $3, d<0?-d:d }' <<< "$res")
    fi
  done \
  | awk 'BEGIN{printf "- gen diff (evo >> eval)%20s\n", ""}{ print $0, $3<0?-$3:$3 }' \
  | sort -k7g | cut -d ' ' -f1-6 | tee $log | column -t
count=$(find $1 -name "mk*.dna" | wc -l)
fail=$(($(wc -l $log | cut -d ' ' -f1) - 1))
awk '{ printf "--\nfailures: %d / %d (%g%%)\n", $1, $2, 100*$1/$2 }' <<< "$fail $count"
