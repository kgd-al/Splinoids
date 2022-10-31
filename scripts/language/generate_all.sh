#!/bin/bash

usage(){
  echo "Usage: $0 [--gui] [--clean|--clean-gui] <pattern> <scenario>"
  echo 
  echo "       Generate post-evolution data for all files matching 'pattern'"
  echo "       Current valid scenarios are d, c22"
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

champs=$(ls -v $1 2>/dev/null)
# if [ -n "$2" ]
# then
#   echo "Filtering out duplicate champions"
#   champs=$(awk '{
#     match($1, /.*lg__(.*)_0.dna/, tokens);
#     f=tokens[1];
#     if (NR==1 || prev < f) print $1;    
#     prev=f;
#   }' <<< "$champs")
# fi
nchamps=$(wc -w <<< "$champs")
if [ $nchamps -gt 0 ]
then
  echo "Found $nchamps champions with pattern '$1'"
else
  echo "No champions found with pattern '$1'"
  exit 1
fi

scenario=$2
if [ -n "$2" ]
then
  echo "Evaluating for scenario '$scenario'"
else
  echo "No evaluation scenario provided"
  usage
  exit 1
fi

sfolder=$(dirname $0)

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
  $sfolder/generate_behavior.sh $c $scenario > $base/.gn_bh.log 2>&1 <<< "y"
  
  r=$?
  if [ $r -ne 0 ]
  then
    ## REMOVE
    if grep -q "Failed to extract a valid range" $base/.gn_bh.log 
    then
      echo "Ignoring $c until mute receivers are handled" >&2
      continue
    fi
  
    echo "Evaluation of '$c' failed with exit code '$r'. Aborting" >&2
    exit $r
  fi
  
  if [ ! -z $withgui ]
  then
    echo "Rendering" $c
    $sfolder/plot_behavior.sh $c $scenario > $base/.pt_bh.log 2>&1
  fi
  
  r=$?
  if [ $r -ne 0 ]
  then
    echo "Rendering of '$c' failed with exit code '$r'. Aborting" >&2
    exit $r
  fi
  
  [ -f "generate_all.stop" ] && break
  
done | pv -l -s $(($nchamps * $steps)) > .generate_all.log

r=${PIPESTATUS[0]}
exit $r
