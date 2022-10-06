#!/bin/bash

width=3
usage(){
  echo "Usage: $0 <evo-folder>"
  echo "       Creates a folder 'champs' for storing symbolic links to all"
  echo "        unique champions (with different fitness) found under gen*"
  echo "       Also creates a link 'last' to the latest item for ease of use"
  echo "       Also zero-pads all generation to a width of $width"
}

if [ ! -d "$1" ]
then
  echo "'$1' is not a folder"
  usage
  exit 1
fi

folder=$1
ls $folder/gen[0-9]* > /dev/null 2>&1
if [ $? -ne 0 ]
then
  echo "Could not find gen* folders under '$1'"
  usage
  exit 2
fi

if ls $folder/champs/* > /dev/null 2>&1
then
  echo "Target folder already populated. Aborting"
  exit 3
fi

mkdir -vp $folder/champs
cd $folder/champs

ls -v ../gen*/lg*.dna | xargs jq -r '"\(input_filename) \(.alreadyEval)"' \
  | grep -v " true$" | cut -d ' ' -f1 | while read c
do
  l=$(sed 's|.*/gen\(.*\)/lg.*.dna|\1|' <<< "$c"| xargs printf "gen%0*d.dna" $width)
  ln -sv $c $l
done

ln -sv $(ls *.dna | tail -n 1) last.dna
