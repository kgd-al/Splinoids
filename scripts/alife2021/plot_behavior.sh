#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome from which evaluation trajectories will be generated"
  exit 1
fi

columnprint(){
#   printf "%-80s\r" "$1"
  printf "."
}

genome="$1"
folder=$(dirname $genome)/$(basename $genome .dna)
if [ -d $folder ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
  ./build/release/pp-evaluator --scenarios 'all' --spln-genome $genome $2 --lesions="all" || exit 2
fi
subtypes=( '' $(ls -d $folder/$v[0-9]* | sed 's|.*/[0-9]*[+-]||' | sort -u) )
 
pstyle="with circles lc rgb 'green' fs solid"
tstyle="with lines lc palette z"
vstyle(){
  x=$((1+$1))
  y=$((2+$1))
  a=$((3+$1))
  echo "((\$0>$2) && \
    (int(\$0)%$2==0) ?\$$x:1/0):$y:(.01*cos(\$$a)):(.01*sin(\$$a)) with vectors lc $4 size $3,20,60 filled back fixed"
}

getscore(){
  grep "$scenario " $folder/scores.dat | tr "_" " " | awk '
  {
    printf "Type: %s, ", $1;
    if (NF>2)
      printf "Lesion: %s, Score: %s\n", $2, $3
    else
      printf "Score: %s\n", $2
  }' 
}

v=0
[ ! -z "$2" ] && v=1

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
  score=$(getscore $scenario)
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
for s in + -
do
  for t in "${subtypes[@]}"
  do
    o=$folder/trajectories_v${v}_$s$t.png
    if false #[ -f "$o" ]
    then
      columnprint "Trajectory overlay file '$o' already generated. Skipping"
      continue
    fi
    
    # Final positions
#     for [i=1:words(list)] '<tail -n1 '.word(list, i) using 2:3:(.5) with circles lc ''.color(i) fs transparent solid .25 notitle,\
    
  #   scenario=$(basename $(dirname $f))
  #   score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
    files=$(ls $folder/$v*$s$t/trajectory.dat)
    lengths=$(wc -l $files | grep -v "total" | tr -s " " | cut -d ' ' -f 2)
    esize=$(head -n 1 $(echo $f | head -n 1) | cut -d ':' -f 2)
    cmd="
      set terminal pngcairo size 840,525 crop;
      set output '$o';
      set size square;
      set xrange [-$esize:$esize];
      set yrange [-$esize:$esize];
      set cblabel 'step';
      set key above;
      unset xtics;
      unset ytics;
      set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
      set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
      set object 1 rect from 0,-.25 to 3.5,.25 fc 'black' front;
      list='$files';
      color(i)=word('green blue red purple', i);
      hsv(i)=word('.33 .66 0 .87', i); 
      length(i)=word('$lengths', i);
      dcolor(i,j)=hsv2rgb(hsv(i)+0, j/(length(i)+0), j/(length(i)+0));
      stitle(i)=system('basename \$(dirname '.word(list, i).') | cut -c2-');
      plot for [i=1:words(list)] word(list, i) every :::1 using 2:3:(dcolor(i, column(0))) with lines lc rgbcolor variable notitle,\
           for [i=1:words(list)] word(list, i) every :::1 using $(vstyle 1 25 0.25 "''.color(i)") title stitle(i);"
            
#     printf "\n%s\n\n" "$cmd"
    columnprint "Plotting overlay $o"
    gnuplot -e "$cmd" || exit 3
  done
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
  score=$(getscore $scenario)
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

size="300x300<+10+10"

sortbyside(){
  ls $@ | sed 's|\(.*\([-+]\).*\)|\2 \1|' | sort -k1 | cut -d ' ' -f 2
}

for t in $subtypes
do
  aggregate="$folder/trajectories_v$v$t.png"
  if [ -f "$aggregate" ]
  then
    columnprint "Trajectories aggregate '$aggregate' already generated. Skipping\n"
  else
    columnprint "Generating aggregate: $aggregate"
    sorted=$(sortbyside $folder/{trajectories_v$v_{+,-},$v[0-9]*$t/trajectory}.png)
  #   printf "\n\nSorted trajectories:\n%s\n\n" "$sorted"
    echo
    echo montage -trim -geometry "$size" -tile x2 $sorted $aggregate
    echo
    montage -trim -geometry "$size" -tile x2 $sorted $aggregate || break
    columnprint "Generated $aggregate\n"
  fi
  echo $aggregate >> .generated_files

  aggregate="$folder/vocalisations_v$v$t.png"
  if [ -f "$aggregate" ]
  then
    columnprint "Trajectories aggregate '$aggregate' already generated. Skipping\n"
  else
    columnprint "Generating aggregate: $aggregate"
    montage -trim -geometry "$size" $folder/$v[0-9]*$t/vocalisation.png $aggregate || break
    columnprint "Generated $aggregate\n"
  fi
  echo $aggregate >> .generated_files
done

###############################################################################
# Generate neural substrate clustering
neuralclusteringcolors(){
  o=$(dirname $1)/$(basename $1 dat)qt
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
          printf " #%02X%s", 255*$i, colors[i-1];
        }
      }
      printf "\n";
    }' $1 > $o
  ls $o
}

for f in $folder/$v[0-9]*/neurons.dat
do
  od=$(dirname $f)/$(basename $f .dat)_groups.dat
  if [ -f "$od" ]
  then
    columnprint "Neural clustering file '$od' already generated. Skipping"
  else
    $(dirname $0)/mw_neural_clustering.py $f 0 || break
  fi

  op=$(dirname $f)/ann_clustered.png
  if [ -f "$op" ]
  then
    columnprint "Neural clustering '$op' already rendered. Skipping"
  else
    scenario=$(basename $(dirname $f))
    score=$(getscore $scenario)
    qt=$(neuralclusteringcolors $od)
  #   set -x
    ./build/release/pp-visualizer --spln-genome $genome --ann-render --ann-colors=$qt --ann-threshold .75 || break
  #   set +x
    montage -label "$score" $op -geometry +0+0 $op~ || break
    printf "\n%s~ -> %s\n" $op~ $op
    mv $op~ $op
  fi
done

for t in "${subtypes}"
do
  aggregate="$folder/neural_groups$t.dat"
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
        for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
        printf "\n";
      }
    }' $folder/$v[0-9]*[+-]$t/neurons_groups$t.dat > $aggregate
    columnprint "Generated $aggregate\n"

    # Mean
    # for (j=2; j<=NF; j++) printf " %.1f", data[n,j]/counts[n,j];

    # Union
    # for (j=2; j<=NF; j++) printf " %.1f", data[n,j] > 0;

  fi

  # Aggregate individual rendered clusters
  aggregate="$folder/ann_agg_clustered$t.png"
  if [ -f "$aggregate" ]
  then
    columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
  else
    columnprint "Generating ANN clustering: $aggregate"
    
    sorted=$(sortbyside $folder/$v[0-9]*[+-]$t/ann_clustered.png)
  #   printf "\n\nSorted neural patterns:\n%s\n\n" "$sorted"
    montage -trim -geometry "$size" $sorted $aggregate

    columnprint "Generated $aggregate\n"
  fi
  echo "$aggregate" >> .generated_files

  # Rendering aggregated clustering 
  aggregate="$folder/ann_clustered$t.png"
  if [ -f "$aggregate" ]
  then
    columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
  else
    columnprint "Generating ANN clustering: $aggregate"
    
    # first convert activation clusters into qt colors
    qt=$(neuralclusteringcolors $folder/neural_groups$t.dat)
    
    # then perform rendering
  #   set -x
    ./build/release/pp-visualizer --spln-genome $genome --ann-render --ann-colors=$qt --ann-threshold 0.01
  #   set +x
    
    columnprint "Generated $aggregate\n"
  fi
  echo "$aggregate" >> .generated_files
done
  
echo "$folder/ann.png" >> .generated_files
./build/release/pp-visualizer --spln-genome $genome --ann-render
