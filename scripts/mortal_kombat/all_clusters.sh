#!/bin/bash

declare -A cols_per_eval=( [e1]="9-11" [e2]="6-8" [e3]="3-5")
declare -A header_per_eval=( [e1]="Touch Health Pain" [e2]="C B A" [e3]="Opp Ally Noise" )

ibase=../alife2022_results
obase=$ibase/analysis/clusters/
mkdir -p $obase

for e in e1 e2 e3
do
  for type in id percent binary
  do
    for t in t1p2 t1p3 t2p2 t2p3
    do
      if [ ! -f $obase/$t/${e}_${type}.png ]
      then
        printf "#%.0s" {1..80}
        printf "\n# %s %s %s\n" $e $t $type
        
        TYPE=$type $(dirname $0)/clusters.sh $ibase/v1-$t $e
      fi
    done
    
    o=$obase/$e.$type.png
    if [ ! -f $o ]
    then
      echo "Generating $o"
      montage -label '%d' $obase/*/${e}_${type}.png -geometry +2+2 $o
    fi
  done
  
  echo "$e modules (per type)"
  (
    echo "Type ${header_per_eval[$e]}"
    for t in t1p2 t1p3 t2p2 t2p3
    do
      printf "%s " $t
      awk '
        NR>1 {
          for (i=2;i<=NF;i++) sum[i]+=($i>0);
        } END {
          printf "foo"
          for (i=2;i<=NF;i++) printf " %.2f", 100*sum[i]/(NR-1);
          printf "\n";
        }' $obase/$t/${e}_id.dat | cut -d ' ' -f ${cols_per_eval[$e]}
    done
  ) | tee $obase/$e.modules-types.dat
  echo

  echo "$e modules (at least)"
  (
    echo "Type 1+ 2+ 3"
    for t in t1p2 t1p3 t2p2 t2p3
    do
      printf "%s" $t
      cut -d ' ' -f ${cols_per_eval[$e]} $obase/$t/${e}_id.dat \
      | awk '
        NR>1 {
          for (i=1;i<=NF;i++) sum[NR]+=($i>0);
        } END {
          for (i=1; i<=3; i++) {
            s=0;
            for (r in sum) s += (sum[r]>=i);
            printf " %.2f", 100*s/(NR-1);
          }
          printf "\n";
        }'
    done
  ) | tee $obase/$e.modules-at-least.dat
  echo
done
