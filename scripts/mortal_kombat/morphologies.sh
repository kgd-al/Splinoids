#!/bin/bash

S=100

usage(){
  echo "Usage: $0 <folder>"
  echo "       Creates a fresco of both populations champion's morphology across generations"
}

log() {
  nl="\r"
  [ ! -z "$2" ] && nl=$2
  c=$(stty size | awk '{print $2}')
  printf "%*s$nl" "$c" "$1"
}

f=$1
if [ ! -d "$f" ]
then
  echo "ERROR: '$f' is not a folder"
  exit 1
fi

log=.morphologies.log
date > $log

out=$f/morphologies/
mkdir -p $out

gens=$(readlink -e $f/A/gen_last | sed 's/.*[^0-9]\([0-9]\+\)/\1/')
# Just to get the ceil of the base 10 power...
gwidth=$(awk "{x=log($gens+1)/log(10); print x%1 ? int(x)+1 : x}" <<< $i)
# echo "$gens > $gwidth"
ofile(){
  printf "$out/%0${gwidth}d.png" $1
}

config=$(ls $f/configs/Simulation.config)
for g in $(seq 0 $gens)
do
  log "[$g / $gens] Processing $g"
  
  lhs=$(ls $f/A/gen$g/mk*_0.dna)
  lhs_o=$(dirname $lhs)/$(basename $lhs dna)png
  rhs=$(ls $f/B/gen$g/mk*_0.dna)
  rhs_o=$(dirname $rhs)/$(basename $rhs dna)png
  if [ ! -f $lhs_o ] && [ ! -f $rhs_o ]
  then
    log "[$g / $gens] Generating pictures for lhs/rhs"
    
    drawVision=0 ./build/release/mk-visualizer --config $config \
      --lhs $lhs --rhs $rhs \
      --picture=$S --no-restore >> $log 2>&1
  else
    log "[$g / $gens] Pictures for lhs/rhs already generated"
  fi
  
  o=$(ofile $g)
  if [ ! -f $o ]
  then
    log "[$g / $gens] Merging pictures for lhs/rhs"
    montage -tile 1x2 -gravity center -geometry ${S}x$S $lhs_o -label $g $rhs_o $o
  else
    log "[$g / $gens] Pictures for lhs/rhs already merged"
  fi
done

o=$f/morphologies.png
if [ ! -f "$o" ]
then
  log "Generating fresco"
  montage -tile x1 -geometry +0+0 $f/morphologies/*.png $o
fi

log "Done" "\n"
