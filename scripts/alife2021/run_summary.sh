#!/bin/bash

show="no"
clean="no"
behaviorArg=""
d="*"
OPTIND=1         # Reset in case getopts has been used previously in the shell.
while getopts "h?f:sct:d:" opt; do
    case "$opt" in
    h|\?)
        usage
        exit 0
        ;;
    f)  folders=$OPTARG
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

rm .generated_files
for f in $folders
do
  printf "\n####\n$f\n"
  [ -z "$clean" ] && rm -rf $f/gen_stats.png $f/gen_last/*.png $f/gen_last/*/

  $(dirname $0)/fitnesses_summary.sh $f/gen_stats.csv
  
  for i in $f/gen_last/$d.dna
  do
    $(dirname $0)/plot_behavior.sh $i $behaviorArg
  done
done

[ -z "$show" ] && feh -. $(cat .generated_files)
