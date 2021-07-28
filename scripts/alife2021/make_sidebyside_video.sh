#!/bin/bash

log(){
  printf "%-*s\r" $(tput cols) "$1"
}

id=$1
scenarios=($2)
scount=$(echo $scenarios | wc -w)

labels=($3)

fmax=100000
[ ! -z $4 ] && fmax=$4 

nmax=0
ns=()

base=tmp/res_clone/$id
dna=$base.dna
  
for i in $(seq 0 $scount)
do
  snStr=${scenarios[$i]}
  sn=$(echo $snStr | cut -d_ -f 1)
  l=$(echo $snStr | cut -d_ -f 2 | tr -d "l")
  [ -z $l ] && l=0
  echo $snStr $sn $l
  
  dir=$base/1$snStr/screenshots

  if [ ! -d $dir ]
  then
    n=0
  else
    n=$(find $dir -name "frame*.png" | wc -l)
  fi
  
  if [ $n -eq 0 ]
  then 
    drawVision=2 selectionZoomFactor=0 \
      ./build/release/pp-visualizer \
        --spln-genome $dna \
        --scenario "1$sn" --lesions "$l" -1 \
        --tags --no-restore --snapshots=500 --snapshots-views='' \
        --ann-aggregate --ann-tags tmp/results/alife2021/v2n/$id/*/*/*/fitness*_0/neural_groups_E.ntags \
        --background=white;
    n=$(ls $dir/*simu.png | wc -l)
  fi

  n=$(($n-1))
  [ $n -gt $fmax ] && n=$fmax
  
  ns[$i]=$(printf "%05g" $n)
    
  [ $nmax -lt $n ] && nmax=$n
  
  for frame in $(seq -f "%05g" 0 $n)
  do
    o=$dir/frame$frame.png
    arg=""; [ ! -f "$o" ] && \
      montage -tile x2 -geometry +0+0 $dir/${frame}_{simu,mann}.png $o
    log $(ls $o)
  done
  echo
done

echo
echo $nmax "${ns[@]}"

odir=tmp/movies/${id}_$(echo "${scenarios[@]}" | tr " " "_")
mkdir -p $odir
echo $odir

for frame in $(seq -f "%05g" 0 $nmax)
do
  o=$odir/frame$frame.png
  
  [ -f "$o" ] && continue
  
  files=""
  for i in $(seq 0 $scount)
  do
    n=${ns[$i]}
    [ $n -gt $frame ] && n=$frame
    
    time=$(echo $n | awk '{ printf "%.1fs", $1/10; }')
    files="$files -label ${labels[$i]}\n$time
                $base/1${scenarios[$i]}/screenshots/frame$n.png"
  done
  
  montage $files -tile x1 -geometry +10+0 -pointsize 24 $o
  log $(ls $o)
done

ffmpeg -framerate 10 -i "$odir/frame%05d.png" -an $odir.mp4 -y -hide_banner
cp $odir/frame00000.png $odir.png
