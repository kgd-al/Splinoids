#!/bin/bash

S=100
[ ! -z ${SIZE+x} ] && S=$SIZE

usage(){
  echo "Usage: $0 <folder>"
  echo "       Creates a fresco of all populations champion's morphology across generations"
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

pops=$(ls -vd $f/[A-Z] | wc -l)
teams=$(sed 's|.*/v1-t\([1-9]\)p.*|\1|' <<< $f)
echo $pops $teams
echo $SUFFIX

[ -z ${SUFFIX+x} ] && [ $teams -gt 1 ] && SUFFIX="_0"

gens=$(ls -vd $f/A/gen[0-9]* | tail -n1 | sed 's|.*/gen\([0-9]*\)|\1|')
# echo $gens
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

  o=$(ofile $g)
  if [ -f $o ]
  then
    log "[$g / $gens] Pictures for A/B already merged"
    continue
  fi
  
  dna_a=$(ls $f/A/gen$g/mk*_0.dna)
  dna_a_o=$(dirname $dna_a)/$(basename $dna_a .dna)$SUFFIX.png
  dna_b=$(ls $f/B/gen$g/mk*_0.dna)
  dna_b_o=$(dirname $dna_b)/$(basename $dna_b .dna)$SUFFIX.png
  if [ ! -f $dna_a_o -o ! -f $dna_b_o ]
  then
    log "[$g / $gens] Generating pictures for A/B"
    
    drawVision=0 ./build/release/mk-visualizer --config $config \
      --lhs $dna_a --rhs $dna_b \
      --picture=$S --no-restore --overwrite 'i' >> $log 2>&1 || exit 42
  else
    log "[$g / $gens] Pictures for A/B already generated"
  fi
  
  if [ $pops -eq 3 ]
  then
    log "[$g / $gens] Generating pictures for A/C"
    
    dna_c=$(ls $f/C/gen$g/mk*_0.dna)
    dna_c_o=$(dirname $dna_c)/$(basename $dna_c .dna)$SUFFIX.png
    if [ ! -f $dna_c_o ]
    then
      log "[$g / $gens] Generating pictures for A/C"
      
      drawVision=0 ./build/release/mk-visualizer --config $config \
        --lhs $dna_a --rhs $dna_c \
        --picture=$S --no-restore --overwrite 'i' >> $log 2>&1
    else
      log "[$g / $gens] Pictures for A/C already generated"
    fi
    
    withC="/C"
    imgs="$dna_a_o $dna_b_o -label $g $dna_c_o"
  else
    imgs="$dna_a_o -label $g $dna_b_o"
  fi
  
  log "[$g / $gens] Merging pictures for A/B$withC"
  montage -tile 1x -gravity center -geometry ${S}x$S $imgs $o || exit 43
done

o=$f/morphologies.png
if [ ! -f "$o" ]
then
  log "Generating fresco"
  montage -tile x1 -geometry +0+0 $(ls -v $f/morphologies/*.png | tail -n 100) $o
fi

log "Done" "\n"
