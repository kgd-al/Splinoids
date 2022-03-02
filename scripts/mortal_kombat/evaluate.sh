#!/bin/bash

guiArg="--gui"
usage(){
  echo "Usage: $0 [$guiArg] <dna> [dna|=] [...]"
  echo "       If rhs dna is not provided it defaults to previous generation"
  echo "        champion(s) of the opposite population(s). To provide options"
  echo "        to the underlying evaluator use filename = for rhs"
}

parts(){
  readlink -m "$1" \
  | sed 's|\(.*\)/ID\(.*\)/\([A-Z]\)/gen\(.*\)/.*.dna|\1 \2 \3 \4|'
}

fetchopponents(){
  read base id p g <<< $(parts "$1")
  gen="gen"$(($g - 1))
  ls $base/ID$id/[A-Z]/$gen/mk__*_0.dna | grep -v "/$p/"
}

if [ "$1" == "$guiArg" ]
then
  gui="yes"
  shift
else
  gui=""
fi

lhs=$(readlink -m $1)
if [ ! -f "$lhs" ]
then
  echo "ERROR: file '$lhs' does not exist"
  usage
  exit 1
fi
shift

rhs=$1
if [ "$rhs" == "=" ]
then
  rhs=""
  shift
fi
if [ -z "$rhs" ]
then
  rhs=$(fetchopponents $lhs)
  if [ -z "$rhs" ]
  then
    echo "Failed to find opponents for '$lhs'"
    exit 2
  else
    echo "defaulting to following champions:"
    for c in $rhs
    do
      echo "> '$c'"
    done
  fi
elif [ -f "$rhs" ]
then
  rhs=$(readlink -m $rhs)
  shift
else
  shift
fi

config=$(dirname $lhs)/../../configs/Simulation.config
config=$(ls $config)
if [ ! -f "$config" ]
then
  echo "Failed to find config"
  exit 2
else
  echo "Using lhs config at '$config'"
fi

output=$(dirname $lhs)/$(basename $lhs .dna)

build=build/release
[ ! -z ${BUILD+x} ] && build=$BUILD
[ ! -z ${CMD+x} ] && set -x
[ ! -z ${TOOL+x} ] && tool=$TOOL
if [ -z "$gui" ]
then
  rhsArg=$(tr " " "\n" <<< "$rhs" | sed 's/^/--opp /')
  $tool $build/mk-evaluator --config $config --data-folder $output \
    --overwrite 'p' --ind $lhs $rhsArg $@
else
  for c in $rhs
  do
    $build/mk-visualizer --config $config --data-folder $output \
      --overwrite 'p' --lhs $lhs --rhs $c $@
  done
fi
