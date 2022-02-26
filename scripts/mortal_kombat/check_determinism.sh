#!/bin/bash

pops=$(ls -d $1/[A-Z] | awk -F'/' '{print $NF}')

log=$1/determinism.log
if [ ! -f "$log" ]
then
  for p in $pops
  do
    echo "Gen Remote Local" > $1/$p.local.fitnesses
  done

  find $1 -name "mk*.dna" \
    | grep -v "gen0" \
    | sort -V \
    | while read dna
    do
      read p g <<< $(sed 's|.*/\([A-Z]\)/gen\([0-9]*\)/.*|\1 \2|' <<< $dna)
      printf "%s %s %-80s\r" $p $g $dna >&2
      [ -z $p ] && continue
      [ -z $g ] && continue
      
      res=$(./scripts/mortal_kombat/evaluate.sh $dna 2>&1 | grep "mk:")
      res=$(sed -e 's/mk: //' -e 's/\x1b\[[0-9;]*m//g' <<< "$res")
      
      mk=$(sed 's/ >> / /' <<< "$res")
      echo $g $mk >> $1/$p.local.fitnesses
      
      res=$(grep ">>" <<< "$res")
      if [ ! -z "$res" ]
      then
        echo $p $g \
            $(awk '{ d=$3-$1; printf " %+g (%g >> %g) %g", d, $1, $3, d<0?-d:d }' <<< "$res")
      fi
    done \
    | awk 'BEGIN{printf "- gen diff (evo >> eval)%20s\n", ""}{ print $0, $3<0?-$3:$3 }' \
    | sort -k7g | cut -d ' ' -f1-6 | tee $log | column -t
  count=$(find $1 -name "mk*.dna" | wc -l)
  fail=$(($(wc -l $log | cut -d ' ' -f1) - 1))
  awk '{ printf "--\nfailures: %d / %d (%g%%)\n", $1, $2, 100*$1/$2 }' <<< "$fail $count"
fi

trustworthy(){
  tac $1.local.fitnesses | awk '{if (!g[1] && ($2==$3 || !$3)) { g[1]=$1 } if (!$3) { g[2]=$1; exit }}END{print g[1], g[2]}'
}

echo "Trustworthiness:"
printf "A %s\nB %s\n" "$(trustworthy $1/A)" "$(trustworthy $1/B)" | tee $1/trustworthiness

gnuplot -e "
  set output '$1/local.fitnesses.png';
  set term pngcairo size 1680,1050;
  set style fill solid .2;
  
  pops='$pops';
  set multiplot layout words(pops), 1;
  
  do for [p in pops] {
    tbounds=system('grep '.p.' $1/trustworthiness | cut -d \" \" -f2-');
    do for [g in tbounds] {
      set arrow from first g+0, graph 0 to first g+0, graph 1 lc 'gray' nohead;
    };
    set label word(tbounds, 1) at first word(tbounds, 1), graph 1 left;
    set label word(tbounds, 2) at first word(tbounds, 2), graph 1 right;
    plot '$1/'.p.'.local.fitnesses' using 1:(\$3-\$2):(1) w boxes notitle;
    unset arrow;
    unset label;
  };
  
  unset multiplot;
  "
