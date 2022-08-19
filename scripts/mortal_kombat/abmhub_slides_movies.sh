#!/bin/bash

base=../alife2022_results/v1-t2p2/ID798167/B/gen1999/mk__0.317601_0
folder=$base/re-evals/$1
screenshots=$folder/screenshots

mute=$(sed -n 's/.*mutei\([01]\)/\1/p' <<< $1)
echo $mute

# Generate graph data
if [ ! -f "$folder/acoustics.dat" ]
then
  verbosity=0 ./scripts/mortal_kombat/evaluate.sh $base.dna = \
    --ann-aggregate $base/eval_first_pass/e3/neural_groups.ntags
fi

# Generate visual data
if [ ! -d $screenshots ]
then
  selectionZoomFactor=0 drawVision=0 brainDeadSelection=Unset ./scripts/mortal_kombat/evaluate.sh \
    --gui $base.dna = --snapshots=1000 --snapshots-views 'simu,mann,midi' --no-restore \
    --ann-tags $base/eval_first_pass/e3/neural_groups.ntags --ann-aggregate --tags \
    --overwrite 'i'
fi

if [ ! -d $folder ]
then
  mkdir -pv $(dirname $folder)
  mv -v $base/798167-B-1999-0__798167-A-1998-0 $folder
fi
  
# Convert midis
for i in 0 1
do
  [ ! -f "$screenshots/$i.mp3" ] && timidity $screenshots/$i.mid -Ow -o - \
  | ffmpeg -loglevel warning -i - -acodec libmp3lame -ab 64k $screenshots/$i.mp3
done

# Make base video
o=8167B_$1.mp4
if [ -z $mute ]
then
  audio="-i $screenshots/0.mp3 -i $screenshots/1.mp3 -filter_complex [1:a][2:a]amerge=inputs=2[a] -map [a]"
else
  audio="-i $screenshots/$((1-$mute)).mp3 -map 1:a"
fi
if [ ! -f $o ]
then
  set -x
  ffmpeg -framerate 25 -i $screenshots/%05d_simu.png $audio -map v $o
  set +x
fi
  
# Generate dynamic graphs
for ((i=0;i<=500;i++))
do
  o=$(printf "$screenshots/%05d_communication.png" $i)
  [ -f "$o" ] && continue;
  gnuplot -e "
    set term pngcairo size 1000,500;
    set output '$o';
    
    lm=.075; rm=.96; bm=.125; tm=.9;
    set multiplot layout 2,1 margins lm, rm, bm, tm spacing 0, 0;
    
    set style fill solid 1 noborder;
    unset xtics;
    set xrange [0:20];
    set ytics .2,.2,.8 rangelimited;
    set ylabel 'Volume';
    set key above title 'Channels';
    
    m='$mute';
    muted='Muted';
    do for [i=0:1] {
      set y2label 'Ind. '.(i+1);
      if (strlen(m) == 1 && i == m) {
        set arrow 10 from graph 0,0 to graph 1,1 nohead;
        set arrow 11 from graph 1,0 to graph 0,1 nohead;
        set obj 12 rect at graph .5, .5 size graph .2, .35 fs solid noborder fc 'white' front;
        set label 1 at graph .5, graph .5 muted font ',18' center front;
      };
      
      if (i==1) {
        set xtics 0,5,20;
        set xlabel 'Time (s)';
      };
      
      af='$folder/acoustics.dat';
      plot for [j=10:12] af every ::::($i+1) using ((\$0+1)/25):12*i+j:(0) with filledcurves title ''.(j-10);
      unset label;
      unset arrow 10;
      unset arrow 11;
      unset obj 12;
    };
    
    unset multiplot;"
    
  printf "Generated $o\r"
    
done

# Merge with simu
for ((i=0;i<=500;i++))
do
  id=$(printf "%05d" $i)
  o=$screenshots/${id}_sico.png
  [ -f $o ] && continue
  montage -tile x2 $screenshots/${id}_{simu,communication}.png -geometry +0+0 $o
  printf "Generated $o\r"
done

# Make graph video
o=8167B_$1_with_graphs.mp4
if [ ! -f $o ]
then
  ffmpeg -loglevel warning -framerate 25 -i $screenshots/%05d_sico.png $audio -map v $o
fi

# Merge with mann
for ((i=0;i<=500;i++))
do
  id=$(printf "%05d" $i)
  o=$screenshots/${id}_simann.png
  [ -f $o ] && continue
  montage -tile x2 $screenshots/${id}_{simu,mann}.png -geometry +0+0 $o
  printf "Generated $o\r"
done

# Make graph video
o=8167B_$1_with_mann.mp4
if [ ! -f $o ]
then
  ffmpeg -loglevel warning -framerate 25 -i $screenshots/%05d_simann.png $audio -map v $o
fi
