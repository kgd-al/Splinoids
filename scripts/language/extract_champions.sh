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

mkdir -vp $folder/champs
cd $folder/champs

ls -v ../gen*/lg*.dna | xargs jq -r '"\(input_filename) \(.alreadyEval)"' \
  | grep -v " true$" | cut -d ' ' -f1 | while read c
do
  l=$(sed 's|.*/gen\(.*\)/lg.*.dna|\1|' <<< "$c"| xargs printf "gen%0*d.dna" $width)
  [ ! -f $l ] && ln -sv $c $l
done

last=$(ls gen*.dna | tail -n 1)
[ -f last.dna ] && [ $(readlink -e $last) != $(readlink -e last.dna) ] && rm -v last.dna
[ ! -f last.dna ] && ln -sv $last last.dna
