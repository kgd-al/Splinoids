#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome from which evaluation trajectories will be generated"
  exit 1
fi

# r=$(basename $1 | tr "_" " ")
# 
# #   set title '$r Foraging';
# #   set title '$r Strategy';
# 
# pstyle="with circles lc rgb 'green' fs solid"
# tstyle="with lines lc palette z"
# vstyle(){
#   echo "((\$0>$1) && (int(\$0)%$2==0) ?\$1:1/0):2:(cos(\$3)):(sin(\$3)):0 with vectors lc palette z"
# }
# 
# gnuplot -e "
#   set terminal pngcairo size 1680,1050 font ',24';
#   set output '$1/champ_foraging.png';
#   set size square;
#   set xlabel 'x';
#   set ylabel 'y';
#   set xrange [-20:20];
#   set yrange [-20:20];
#   set cblabel 'step';
#   set key above;
#   set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
#   set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
#   plot for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::::0 $pstyle notitle, \
#        1/0 $tstyle title 'trajectories', \
#        1/0 $pstyle title 'plants', \
#        for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::1 using 1:2:0 $tstyle notitle, \
#        for [i=0:315:45] sprintf('$1/champ_A%03d.dat', i) every :::1 using $(vstyle 10 10) notitle;
#   set xrange [-50:50];
#   set yrange [-50:50];
#   set output '$1/champ_strategy.png';
#   plot '$1/champ_strategy.dat' every :::1 using 1:2:0 $tstyle title 'trajectory', \
#                             '' every :::::0 $pstyle title 'plants',
#                             '' every :::1 using $(vstyle 0 50) notitle;"

genome="$1"
./build/release/pp-evaluator --scenarios 'all' --spln-genome $genome $2 || exit 2

pstyle="with circles lc rgb 'green' fs solid"
tstyle="with lines lc palette z"
vstyle(){
  x=$((1+$1))
  y=$((2+$1))
  a=$((3+$1))
  echo "((\$0>$2) && \
    (int(\$0)%$2==0) ?\$$x:1/0):$y:(.01*cos(\$$a)):(.01*sin(\$$a)) with vectors lc '$4' size $3,20,60 filled back fixed"
}

v=0
[ ! -z "$2" ] && v=1

folder=$(dirname $genome)/$(basename $genome .dna)
for f in $folder/$v[0-9]*/trajectory.dat
do
  scenario=$(basename $(dirname $f))
  score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
  esize=$(head -n 1 $f | cut -d ':' -f 2)
  cmd="
    set terminal pngcairo size 1680,1050 font ',24';
    set output '$(dirname $f)/$(basename $f dat)png';
    set size square;
    set xrange [-$esize:$esize];
    set yrange [-$esize:$esize];
    set cblabel 'step';
    set key above title '$score';
    set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
    set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
    plot 1/0 $pstyle title 'plant', \
          '<tail -n +3 $f' every :::::0 using 1:2:(.5) $pstyle notitle, \
          '$f' every :::1 using 1:2:0 $tstyle title 'trajectory',\
            '' every :::1 using $(vstyle 0 10 0.25 black) notitle, \
          '$f' every :::1 using 4:5 w l lc 'blue' title 'clone', \
            '' every :::1 using $(vstyle 3 25 0.125 blue) notitle, \
          '$f' every :::1 using 7:8 w l lc 'red' title 'predator', \
            '' every :::1 using $(vstyle 6 25 0.125 red) notitle;"
           
#   echo "$cmd"
  printf "%-80s\r" "Plotting $f"
  gnuplot -e "$cmd" || exit 3
  
  audio=$(echo $f | sed 's/trajectory/vocalisation/')
  l=$(head -n 1 $audio | wc -w)
  cmd="
    set terminal pngcairo size 1680,1050 font ',24';
    set output '$(dirname $audio)/$(basename $audio dat)png';
    set key above title '$score';
    set yrange [-.05:1.05];
    plot for [i=1:$l] '$audio' using 0:i with lines title columnhead(i)"
    
  #   echo "$cmd"
  printf "%-80s\r" "Plotting $f"
  gnuplot -e "$cmd" || exit 3
done

v=0
[ ! -z "$2" ] && v=1

size="300x300<+10+10"
aggregate="$folder/trajectories_v$v.png"
printf "%-80s\r" "Generating aggregate: $aggregate"
montage -trim -geometry "$size" $folder/$v[0-9]*/trajectory.png $aggregate
printf "%-80s\n" "Generated $aggregate"

aggregate="$folder/vocalisations_v$v.png"
printf "%-80s\r" "Generating aggregate: $aggregate"
montage -trim -geometry "$size" $folder/$v[0-9]*/vocalisation.png $aggregate
printf "%-80s\n" "Generated $aggregate"
