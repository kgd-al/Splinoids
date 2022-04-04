#!/bin/bash

usage(){
  echo "Usage: $0 <ind> <opp|=>"
}

ind=$1
if [ ! -f "$ind" ]
then
  echo "DNA file '$ind' error: File does not exist"
  usage
  exit 1
else
  shift
fi

opp=$1
if [ -f "$opp" ]
then
  shift
elif [ "$opp" == "=" ]
then
  shift
else
  echo "Invalid opponent"
  usage
  exit 1
fi

views="simu"
if [ ! -f $1 ]
then
  views=$1
  shift
fi

indfolder=$(dirname $ind)/$(basename $ind .dna)

if ls $indfolder/*1999*1998*/screenshots > /dev/null 2>&1
then
  echo "Data folder(s) already populated. Skipping generation"
else
  echo "Generating"
  CMD=yes $(dirname $0)/evaluate.sh --gui $ind $opp --snapshots=500 --snapshots-views="$views" --tags
fi

for dfolder in $indfolder/*1999*1998*/screenshots
do
  echo "Processing $dfolder"
  for m in $(ls $dfolder/*.mid 2>/dev/null)
  do
    echo $m
    o=$(dirname $m)/$(basename $m .mid).mp3
    if [ -f $o ]
    then
      echo "$(basename $m) already converted to mp3. Skipping"
    else
      timidity $m -Ow -o - | ffmpeg -loglevel warning -i - -acodec libmp3lame -ab 64k $o > /dev/null
    fi
  done

  audio(){
    if [ -f $dfolder/1.mp3 ]
    then
      echo -i $dfolder/0.mp3 -i $dfolder/1.mp3 -filter_complex "[1:a][2:a]amerge=inputs=2[a]" -map "[a]"
    elif [ -f $dfolder/0.mp3 ]
    then
      echo -i $dfolder/0.mp3 -map "[a]"
    fi
  }

  o=$dfolder/movie.mp4
  [ ! -f "$o" ] && ffmpeg -loglevel warning -framerate 25 -i $dfolder/%05d_simu.png $(audio) -map v $o
  
  ls $o
done
