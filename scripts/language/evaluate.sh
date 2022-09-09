#!/bin/bash

guiArg="--gui"
statsArg="--stats"
usage(){
  echo "Usage: $0 [$guiArg|$statsArg] <dna> [...]"
  echo "       Encapsulates evaluation of an individual with/without gui"
  echo "       Config files and output folders are derived from dna file path"
}

gui=""
stats=""
if [ "$1" == "$guiArg" ]
then
  gui="yes"
  shift
elif [ "$1" == "$statsArg" ]
then
  stats="yes"
  shift
fi

lhs=$(readlink -m $1)
if [ ! -f "$lhs" ]
then
  echo "ERROR: file '$lhs' does not exist"
  usage
  exit 1
fi
shift

config=$(dirname $lhs)/../configs/Simulation.config
config=$(ls $config)
if [ ! -f "$config" ]
then
  echo "Failed to find config"
  exit 2
else
  echo "Using lhs config at '$config'"
fi

output=$(dirname $lhs)/$(basename $lhs .dna)/outputs

build=build/release/language
[ ! -z ${BUILD+x} ] && build=$BUILD
[ ! -z ${CMD+x} ] && set -x
[ ! -z ${TOOL+x} ] && tool=$TOOL
if [ -n "$gui" ]
then
  $build/lg-visualizer --config $config --data-folder ${output} \
    --overwrite 'p' --ind $lhs $@
    
elif [ -n "$stats" ]
then
  $build/lg-evaluator --config $config --data-folder $output \
      --overwrite 'p' --ind $lhs --stats
else
  $tool $build/lg-evaluator --config $config --data-folder $output \
    --overwrite 'p' --ind $lhs $@
fi
