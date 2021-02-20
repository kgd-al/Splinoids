#!/bin/bash

usage(){
  echo "Usage: $0 -i=<ind.dna> -s=<scenario spec> [-h=height]"
}


# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

S="720"
outfile=""
while getopts "h?i:s:h:" opt; do
    case "$opt" in
    h|\?)
        usage
        exit 0
        ;;
    i)  individual=$OPTARG
        ;;
    s)  scenario=$OPTARG
        ;;
    h)  S=$OPTARG
        ;;
    esac
done
resolution="$((2*$S))x$S"
echo "resolution: $resolution"

# -i data/test.dna -s "1++++"

./build/release/pp-visualizer --env-genome data/small.env.json --spln-genome $individual --overwrite p --scenario "$scenario" --snapshots=$S

base=$(dirname $individual)/$(basename $individual .dna)
imgfolder=$base/$scenario/
echo "image folder: $imgfolder"
if [ ! -d "$imgfolder" ]
then
    echo "Failed to find img folder '$imgfolder'"
    exit 2
fi

outfile=$base/$scenario.mp4
echo "output video: $outputfolder"
ffmpeg -framerate 10 -i "$imgfolder/%05d.png" -s $resolution -an $outfile \
  && echo -e "\n\e[32m Sucessfully created video file $outfile \e[0m"
