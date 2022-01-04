#!/bin/bash

guiArg="--gui"
usage(){
  echo "Usage: $0 [$guiArg] <dna> [dna|=] [...]"
  echo "       If rhs dna is not provided it defaults to previous generation "
  echo "        champion of the opposite population. To provide options to the "
  echo "        underlying evaluator use filename = for rhs"
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
  parts=($(readlink -m "$lhs" | sed 's|\(.*\)/\([AB]\)/gen\(.*\)/.*.dna|\1 \2 \3|'))
#   echo "parts: ${parts[@]}"
  base="${parts[0]}"
  pop=$(echo "${parts[1]}" | tr "AB" "BA")
  gen="gen"$((${parts[2]}-1))
  rhs=$(ls $base/$pop/$gen/mk_*_0.dna)
  if [ ! -f "$rhs" ]
  then
    echo "Failed to find expected rhs '$rhs'"
    exit 2
  else
    echo "defaulting to '$rhs' for rhs"
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

# set -x
build=release
[ ! -z ${BUILD+x} ] && build=$BUILD
if [ -z "$gui" ]
then
  ./build/$build/mk-evaluator --config $config --data-folder $output \
    --overwrite 'p' --lhs $lhs --rhs $rhs $@
else
  ./build/$build/mk-visualizer --config $config --data-folder $output \
    --overwrite 'p' --lhs $lhs --rhs $rhs $@
fi
