#!/bin/bash

show="no"
clean="no"
behaviorArg=""
d="*"
OPTIND=1         # Reset in case getopts has been used previously in the shell.
while getopts "h?sct:d:" opt; do
    case "$opt" in
    h|\?)
        usage
        exit 0
        ;;
    s)  show=""
        ;;
    c)  clean=""
        ;;
    d)  d=$OPTARG
        ;;
    t)  behaviorArg=$OPTARG
        ;;
    esac
done
shift $((OPTIND-1))
folders=$@
echo $folders

rm .generated_files
count=$(ls -d $folders | wc -l)
index=1
for f in $folders
do
  printf "\n####\n\033[33m[%2d / %2d: %3d%%]\033[0m $f\n" $index $count $((100*$index / $count))
  [ -z "$clean" ] && rm -rf $f/gen_stats.png $f/gen_last/*.png $f/gen_last/*/

  $(dirname $0)/fitnesses_summary.sh $f/gen_stats.csv || exit 2
  
  for i in $f/gen_last/$d.dna
  do
    $(dirname $0)/plot_behavior.sh $i $behaviorArg || exit 3
  done
  
  index=$(($index + 1))
done 2>&1 | tee .$(basename $0).log

[ -z "$show" ] && feh -. $(cat .generated_files)

printf "\n####\nDone\n####\n\n"
