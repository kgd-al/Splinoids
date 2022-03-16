#!/bin/bash

usage(){
  echo "Usage: $0 [--gui] [--clean|--clean-gui] <pattern>"
  echo 
  echo "       Generate post-evolution data for all files matching 'pattern'"
  echo "       if --gui is provided all generate screenshots, plots..."
  echo "       --clean deletes all previous evaluation data"
  echo "       --clean--gui deletes all previous pictures (png, pdf)"
}

steps=1

if [ "$1" == "--gui" ]
then
  withgui=true
  steps=$(($steps+1))
  shift
fi

if [ -z "$1" ]
then
  echo "No pattern provided"
  exit 1
fi

clean=0
if [ "$1" == "--clean-gui" ]
then
  clean=1
  shift
elif [ "$1" == "--clean" ]
then
  clean=2
  shift
elif [[ "$1" =~ "--" ]]
then
  echo "Unrecognized argument '$1'"
  exit 2
fi

champs=$(ls $1 2>/dev/null)
nchamps=$(wc -w <<< "$champs")
if [ $nchamps -gt 0 ]
then
  echo "Found $nchamps champions with pattern '$1'"
else
  echo "No champions found with pattern '$1'"
  exit 1
fi

sfolder=$(dirname $0)

procs=4
for c in $champs
do
  base=$(dirname $c)/$(basename $c .dna)/
  if [ $clean -gt 1 ]
  then
    rm -rf $base >&2
  elif [ $clean -gt 0 ]
  then
    find $base -name "*.png" -o "*.pdf" | xargs rm >&2
  fi

  mkdir -p $base
  
  echo "Processing" $c
  $sfolder/generate_behavior.sh $c > $base/.gn_bh.log 2>&1 <<< "y"
  
  if [ ! -z $withgui ]
  then
    echo "Rendering" $c
    $sfolder/plot_behavior.sh $base > $base/.pt_bh.log 2>&1
  fi
  
done | awk -vn=$nchamps -vcols=$(tput cols) -vs=$steps '{
    str = sprintf("[%5.2f%%] %s %s", 100*NR/(s*n), $1, $2);
    printf "%*s\r", cols, str;
  } END {
    printf "%*s\n", cols, "Done";
  }'
