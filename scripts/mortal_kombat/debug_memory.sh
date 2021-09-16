#!/bin/bash

for p in A B
do
  f=$1/$p
  ls $f/gen[0-9]*/mk*.dna -v | xargs jq '.dna | fromjson | .[0].gen.s.g' \
    | awk 'BEGIN{n=0}{new=(prev!=$1); if (new) n++; print n, new; prev=$1}' > .tmp.$p
#   cat .tmp.$p
done

gnuplot -e "
  set term pngcairo;
  set output '$1/memory.png';
  set key above;
  set multiplot layout 2,1;
  set grid;
  do for [p in 'A B'] {
    set y2label p;
    plot '.tmp.'.p using 0:1 with lines notitle, 
              '' using 0:(\$2==1?\$1+\$2:1/0) notitle; }"
  
rm .tmp.[AB]
