#!/bin/bash

usage(){
  echo "Usage: $0 <folder>"
  echo "       Parses genealogy data from both populations"
  echo 
}

f=$1
if [ ! -d "$f" ]
then
  echo "Provided folder '$f' does not exist"
  exit 1
fi

gens=$(readlink -e $f/A/gen_last | sed 's/.*[^0-9]\([0-9]\+\)/\1/')
for p in A B
do
  
  tmpraw="$f/$p/.tmp.lineage.raw"
  if [ ! -f $tmpraw ]
  then
    prevChamp=-1
    for g in $(seq $gens -1 0)
    do
      tmplocal="$f/$p/.tmp.lineage.local"
      jq -r '.population | sort_by(.fitnesses["mk"]) | reverse | .[] | [.id[0], (.dna | fromjson | .[0].gen | (.s.g, .m.g)), (.fitnesses["mk"])] | join(" ")' $f/$p/gen$g/pop*pop > $tmplocal
      awk -vID=$prevChamp 'NR==1 || $2 == ID' $tmplocal
      prevChamp=$(head -n1 $tmplocal | cut -d ' ' -f 3)
    done | pv -N "Processing ${p}.raw" > $tmpraw
  fi
    
  tmpdat="$f/$p/.tmp.lineage.dat"
  [ ! -f "$tmpdat" ] && \
    tac $tmpraw | awk '{ map[$2] = $4; if ($2 > $3) { prev=map[$3]; if (prev) { print $1-1, prev, 1, $4-prev; } } }' \
    | pv -N "Processing ${p}.dat" > $tmpdat
done

gnuplot -e "
  set term pngcairo size 1680,1050 font ',24';
  set output '$f/lineage.png';
  
  set multiplot layout 1,2;
  set yrange [-4:2.05];
  set cbrange [-6:6];
  unset key;
  do for [p in 'A B'] { 
    plot '$f/'.p.'/.tmp.lineage.dat' using 1:2:3:4:4 with vectors lc palette nohead title p;
  };
  unset multiplot;
  "
