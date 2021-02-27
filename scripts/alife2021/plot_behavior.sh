#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome from which evaluation trajectories will be generated"
  exit 1
fi

columnprint(){
  printf "%-80s\r" "$1"
}

genome="$1"
folder=$(dirname $genome)/$(basename $genome .dna)
if [ -d $folder ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
  ./build/release/pp-evaluator --scenarios 'all' --spln-genome $genome $2 || exit 2
fi

pstyle="with circles lc rgb 'green' fs solid"
tstyle="with lines lc palette z"
vstyle(){
  x=$((1+$1))
  y=$((2+$1))
  a=$((3+$1))
  echo "((\$0>$2) && \
    (int(\$0)%$2==0) ?\$$x:1/0):$y:(.01*cos(\$$a)):(.01*sin(\$$a)) with vectors lc $4 size $3,20,60 filled back fixed"
}

v=0
[ ! -z "$2" ] && v=1
# 
# # Generate behavior vectors
# for f in $folder/$v[0-9]*/trajectory.dat
# do
#   d=$(dirname $f)/bvectors.dat
#   o=$(dirname $f)/$(basename $d dat)png
#   
#   if [ -f "$o" ]
#   then
#     columnprint "Behavior vectors file '$o' already generated. Skipping"
#     continue
#   fi
#   
#   scenario=$(basename $(dirname $f))
#   score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
#   
#   rm -f $d
#   headers=("Goal" "Ally" "Enemy")
#   for i in 1 2 3
#   do
#     echo "\"${headers[$(($i-1))]}\"" >> $d
#     line=$(awk -vt=$i 'BEGIN{ r=-1; } substr($1, t, 1) == "1" { r=NR; exit } END { print r }' $f)
#     if [ $line -ge 0 ]
#     then
#       awk -vr=$line -vw=5 '
#         BEGIN { br=r-w; if (br < 2) br = 2; ar = r+w; }
#         NR==br || NR==ar { px = $2; py=$3; }
#         NR==br+1 || NR==ar+1 { print $2-px, $3-py; }
#         ' $f >> $d
#     else
#       printf "nan nan\nnan nan\n" >> $d
#     fi
#     printf "\n\n" >> $d
#   done
#   
#   cmd="
#     set terminal pngcairo size 840,525;
#     set output '$o';
#     set xrange [-.5:2.5];
#     set key above title '$score';
#     f='$d';
#     titles='before after';
#     do for [b=0:2] {
#       h=system('head -n '.(b*5+1).' '.f.' | tail -n 1');
#       set xtics add (h b);
#     };
#     plot for [b=0:2] f index b using (b):(0):1:2:0 with vectors lc variable filled notitle, 
#          for [t=0:1] NaN lc t+1 title word(titles, t+1);"
#            
# #   echo "$cmd"
#   columnprint "Plotting $f"
#   gnuplot -e "$cmd" || exit 3
# done

# Generate trajectories
for f in $folder/$v[0-9]*/trajectory.dat
do
  o=$(dirname $f)/$(basename $f dat)png
  if [ -f "$o" ]
  then
    columnprint "Trajectory file '$o' already generated. Skipping"
    continue
  fi
  
  scenario=$(basename $(dirname $f))
  score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
  esize=$(head -n 1 $f | cut -d ':' -f 2)
  cmd="
    set terminal pngcairo size 840,525;
    set output '$o';
    set size square;
    set xrange [-$esize:$esize];
    set yrange [-$esize:$esize];
    set cblabel 'step';
    unset key;
    set title '$score';
    set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
    set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
    plot 1/0 $pstyle title 'plant', \
          '<tail -n +3 $f' every :::::0 using 1:2:3 $pstyle notitle, \
          '$f' every :::1 using 2:3:0 $tstyle title 'trajectory',\
            '' every :::1 using $(vstyle 1 10 0.25 "'black'") notitle, \
          '$f' every :::1 using 5:6 w l lc 'blue' title 'clone', \
            '' every :::1 using $(vstyle 4 25 0.125 "'blue'") notitle, \
          '$f' every :::1 using 8:9 w l lc 'red' title 'predator', \
            '' every :::1 using $(vstyle 7 25 0.125 "'red'") notitle;"
           
#   echo "$cmd"
  columnprint "Plotting $f"
  gnuplot -e "$cmd" || exit 3
done

# Generate trajectories overlay
for t in $(ls -d $folder/$v[0-9]* | sed 's|.*/[0-9]*||' | sort | uniq)
do
  o=$folder/trajectories_$t.png
  if [ -f "$o" ]
  then
    columnprint "Trajectory overlay file '$o' already generated. Skipping"
    continue
  fi
  
#   scenario=$(basename $(dirname $f))
#   score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
  files=$(ls $folder/$v*$t/trajectory.dat)
  esize=$(head -n 1 $(echo $f | head -n 1) | cut -d ':' -f 2)
  cmd="
    set terminal pngcairo size 840,525;
    set output '$o';
    set size square;
    set xrange [-$esize:$esize];
    set yrange [-$esize:$esize];
    set cblabel 'step';
    set key above;
    set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
    set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
    list=system('ls $folder/$v*$t/trajectory.dat');
    color(i)=word('green blue red purple', i);
    stitle(i)=system('basename \$(dirname '.word(list, i).')');
    plot for [f in list] f every :::1 using 2:3:0 $tstyle notitle,\
         for [i=1:words(list)] word(list, i) every :::1 using $(vstyle 1 25 0.25 "''.color(i)") title stitle(i);"
           
#   printf "\n%s\n\n" "$cmd"
  columnprint "Plotting $f"
  gnuplot -e "$cmd" || exit 3
done

# Generate audio traces
for f in $folder/$v[0-9]*/vocalisation.dat
do
  o=$(dirname $f)/$(basename $f dat)png
  if [ -f "$o" ]
  then
    columnprint "Audio file '$o' already generated. Skipping"
    continue
  fi
  
  scenario=$(basename $(dirname $f))
  score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
  l=$(head -n 1 $f | wc -w)
  cmd="
    set terminal pngcairo size 840,525;
    set output '$o';
    set key above title '$score';
    set yrange [-.05:1.05];
    line(i)=1+(i-2)%3;
    dash(i)=1+3*int((i-2)/3);
    plot for [i=2:$l] '$f' using 0:i with lines lt line(i) dt dash(i) title columnhead(i)"
    
  #   echo "$cmd"
  columnprint "Plotting $f"
  gnuplot -e "$cmd" || exit 3
done

v=0
[ ! -z "$2" ] && v=1

size="300x300<+10+10"
# 
# aggregate="$folder/bvectors_v$v.png"
# if [ -f "$aggregate" ]
# then
#   columnprint "Behavioral vectors aggregate '$aggregate' already generated. Skipping\n"
# else
#   columnprint "Generating aggregate: $aggregate"
#   montage -trim -geometry "$size" $folder/$v[0-9]*/bvectors.png $aggregate
#   columnprint "Generated $aggregate\n"
# fi
# echo $aggregate >> .generated_files

aggregate="$folder/trajectories_v$v.png"
if [ -f "$aggregate" ]
then
  columnprint "Trajectories aggregate '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating aggregate: $aggregate"
  sorted=$(ls $folder/{trajectories_{+,-},$v[0-9]*/trajectory}.png | sed 's|\(.*\([-+]\).*\)|\2 \1|' | sort -k1 | cut -d ' ' -f 2)
  echo $sorted
  montage -trim -geometry "$size" -tile x2 $sorted $aggregate
  columnprint "Generated $aggregate\n"
fi
echo $aggregate >> .generated_files

aggregate="$folder/vocalisations_v$v.png"
if [ -f "$aggregate" ]
then
  columnprint "Trajectories aggregate '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating aggregate: $aggregate"
  montage -trim -geometry "$size" $folder/$v[0-9]*/vocalisation.png $aggregate
  columnprint "Generated $aggregate\n"
fi
echo $aggregate >> .generated_files

# Generate neural substrate clustering
for f in $folder/$v[0-9]*/neurons.dat
do
  o=$(dirname $f)/$(basename $f .dat)_groups.dat
  if [ -f "$o" ]
  then
    columnprint "Neural clustering file '$o' already generated. Skipping"
    continue
  fi
  
  $(dirname $0)/mw_neural_clustering.py $f 0
done

aggregate="$folder/neural_groups.dat"
if [ -f "$aggregate" ]
then
  columnprint "Neural aggregate '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating data aggregate: $aggregate"
  awk 'NR==1{ 
    header=$0;
    next;
  }
  FNR == 1 { next; }
  {
    neurons[$1]=1;
    for (i=2; i<=NF; i++) {
      if ($i != "nan") {
        data[$1,i] += $i;
        counts[$1,i]++;
      }
    }
  }
  END {
    print header;
    for (n in neurons) {
      printf "%s", n;
      for (j=2; j<=NF; j++) printf " %.1f", 100*data[n,j]/counts[n,j];
      printf "\n";
    }
  }' $folder/$v[0-9]*/neurons_groups.dat > $aggregate
  columnprint "Generated $aggregate\n"
fi

# Delegate rendering to pp-visu
aggregate="$folder/ann_clustered.png"
if [ -f "$aggregate" ]
then
  columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating ANN clustering: $aggregate"
  
  # first convert activation clusters into qt colors
  awk -vlcolors="00FF00;0000FF;FF0000" '
    BEGIN {
      split(lcolors, colors, ";");
    }
    NR>1 {
    print("//", $0);
    gsub(/[()]/, "", $1);
    printf "%s", $1, "()", "";
    for (i=2; i<=NF; i++) {
      if ($i == 0)
        printf " #00000000"
      else {
        printf " #%02X%s", 255*$i/100, colors[i-1];
      }
    }
    printf "\n";
  }' $folder/neural_groups.dat > $folder/neural_groups.qt
  
#   set -x
  ./build/release/pp-visualizer --spln-genome $genome --ann-render --ann-colors=$folder/neural_groups.qt --ann-threshold .75
#   set +x
  
  columnprint "Generated $aggregate\n"
fi
echo "$folder/ann.png $aggregate" >> .generated_files
