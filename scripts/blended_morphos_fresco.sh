#!/bin/bash

files=$(ls $1)
if [ -z "$1" -o -z "$files" ]
then
  echo "ERROR: not input files"
  echo "Usage: $0 <glob-pattern>"
  exit 10
fi

name=$(basename $0 .sh)
tmp=tmp/.$name/
log=$name.log
endl="\n"

echo "$(date) Started blending" > $log

bfind(){
  find $1/ -name 'blended_[^m]*.png'
}

for f in $files
do
  o=$(dirname $f)
  if [ -z "$(bfind $o)" ]
  then
    ./build_release/visu-tool --load $f --show-blend $o -f $tmp --overwrite 'p' >> $log 2>&1 
    printf "Processed $f$endl" | tee -a $log
  else
    printf "Skipped $f$endl" | tee -a $log
  fi
  
  bfiles="$bfiles $(bfind $o | awk -F/ '{print $NF}')"
  bfiles=$(echo $bfiles | tr " " "\n" | sort | uniq)  
done
printf "Generated all blended morphologies\n"

# mpattern=$(echo "$bfiles" | awk '{ printf "
# echo $mpattern

s=50
wd=$(pwd)
bfile=blended_morphos.png
for f in $files
do
  o=$(dirname $f)
  if [ ! -d $o/$bfile ]
  then
    mcmd=""
    cd $o
    for bf in $bfiles
    do
      if [ -f $bf ] 
      then
        mcmd="$mcmd $bf"
      else
        mcmd="$mcmd null:"
      fi
    done
    montage -tile 1x -geometry ${s}x$s+1+1 $mcmd $bfile
    printf "Montage performed for $o$endl"
    
    cd $wd
    
    mglobal="$mglobal $o/$bfile"
  fi
done
printf "Final montage\n"

n=$(echo "$bfiles" | wc -w)
S=$(($s * $n))
echo $S

montage -tile x1 -geometry ${s}x$S+2+2 $mglobal blended_morphos_fresco.png

rm -rv $tmp
