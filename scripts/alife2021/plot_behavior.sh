#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome from which evaluation trajectories will be generated"
  exit 1
fi

columnprint(){
  printf "%-*s\r" $(tput cols) "$1"
#   printf "."
}

genome="$1"
folder=$(dirname $genome)/$(basename $genome .dna)
if [ -d $folder ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
  set -x
  ./build/release/pp-evaluator --scenarios 'all' --spln-genome $genome $2 || exit 2
  set +x
fi

ext=png
[ ! -z $3 ] && ext=$3

# Visu flag for pdf rendering
annRender="--ann-render=$ext"

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
  read ew eh < <(head -n 1 $f | cut -d ' ' -f 3-4)
  cmd="
    set terminal pngcairo size 840,525;
    set output '$o';
    set size ratio $eh./$ew;
    set xrange [-$ew:$ew];
    set yrange [-$eh:$eh];
    set cblabel 'step';
    unset key;
    set title '$score';
    set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
    set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
    set object 1 rect from 0,-.25 to $ew-1.5,.25 fc 'black' front;
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
  o=$folder/trajectories_v${v}_$s.$ext
  if [ -f "$o" ]
  then
    columnprint "Trajectory overlay file '$o' already generated. Skipping"
    continue
  fi
  
  # Final positions
#     for [i=1:words(list)] '<tail -n1 '.word(list, i) using 2:3:(.5) with circles lc ''.color(i) fs transparent solid .25 notitle,\
  
  term="pngcairo size 840,525 crop"
  [ "$ext" == "pdf" ] && term="pdfcairo size 6,3.5 font 'Monospace,12'";
  
#   scenario=$(basename $(dirname $f))
#   score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
  files=$(ls $folder/$v*$s/trajectory.dat)
  lengths=$(wc -l $files | grep -v "total" | tr -s " " | cut -d ' ' -f 2)
  read ew eh < <(head -n 1 $(echo $f | head -n 1) | cut -d ' ' -f 3-4)
  cmd="
    set terminal $term;
    set output '$o';
    set size ratio $eh./$ew;
    set xrange [-$ew:$ew];
    set yrange [-$eh:$eh];
    set cblabel 'step';
    set key above;
    unset xtics;
    unset ytics;
    set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
    set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
    set object 1 rect from 0,-.25 to $ew-1.5,.25 fc 'black' front;
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

# Generate audio traces
vocalisation(){
  scenario=$(basename $(dirname $1))
  score=$(getscore $scenario)
  cmd="
    set terminal pngcairo size 840,525 crop;
    set output '$2';
    unset xtics;
    set autoscale fix;
    set yrange [-.05:1.05];
    rows=3;
    set style fill transparent solid 0.5 noborder;
    set multiplot layout rows,1 spacing 0,0 title '$score';
    set key above;
    do for [r=1:rows] {
      if (r > 1) { unset key; };
      if (r == rows) { set xtics; };
      set y2label 'Channel '.r;
      plot for [i=1:0:-1] '$f' using 0:i*rows+r+1 with boxes title (i==0 ? 'Subject' : 'Ally');
    };
    unset multiplot"    
  echo "$cmd"
}
for f in $folder/$v[0-9]*/vocalisation.dat
do
  o=$(dirname $f)/$(basename $f dat)png
  if [ -f "$o" ]
  then
    columnprint "Audio file '$o' already generated. Skipping"
    continue
  fi
  
  cmd=$(vocalisation $f $o)
  columnprint "Plotting $f"
  gnuplot -e "$cmd" || exit 3
done

size="300x300^+10+10"

sortbyside(){
  ls $@ | sed 's|\(.*\([-+]\).*\)|\2 \1|' | sort -k1 | cut -d ' ' -f 2
}

aggregate="$folder/trajectories_v$v.png"
if [ -f "$aggregate" ]
then
  columnprint "Trajectories aggregate '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating aggregate: $aggregate"
  sorted=$(sortbyside $folder/{trajectories_v${v}_{+,-},$v[0-9]*[+-]/trajectory}.png)
#   printf "\n\nSorted trajectories:\n%s\n\n" "$sorted"
#     echo
#     echo montage -trim -geometry "$size" -tile x2 $sorted $aggregate
#     echo
  montage -trim -geometry "$size" -tile x2 $sorted $aggregate || break
  columnprint "Generated $aggregate\n"
fi
echo $aggregate >> .generated_files

aggregate="$folder/vocalisations_v$v.png"
if [ -f "$aggregate" ]
then
  columnprint "Vocalisations aggregate '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating aggregate: $aggregate"
  sorted=$(sortbyside $folder/$v[0-9]*[+-]/vocalisation.png)
  montage -trim -geometry "$size" -tile x2 $sorted $aggregate || break
  columnprint "Generated $aggregate\n"
fi
echo $aggregate >> .generated_files

###############################################################################
# Generate neural substrate clustering

aggregate="$folder/ann.$ext"
if [ -f "$aggregate" ]
then
  columnprint "ANN already generated. Skipping"
else
  ./build/release/pp-visualizer --spln-genome $genome --ann-render $2 $annRender
fi
echo "$aggregate" >> .generated_files

neuralclusteringcolors(){
  o=$(dirname $1)/$(basename $1 dat)ntags
  ltags="1;2;4;8;16"
  [ ! -z "$2" ] && ltags=$2
  awk -vltags="$ltags" '
    BEGIN {
      split(ltags, tags, ";");
      printf "//tags:%d", tags[1];
      for (i=2; i<=length(tags); i++)
        printf ",%d", tags[i];
      printf "\n";
    }
    NR>1 {
      v=0;
      for (i=2; i<=NF; i++)
        if ($i != 0 && $i != "nan")
          v = or(v, tags[i-1]);
          
      r=""                    # initialize result to empty (not 0)
      a=v                     # get the number
      while(a!=0){            # as long as number still has a value
        r=((a%2)?"1":"0") r   # prepend the modulos2 to the result
        a=int(a/2)            # shift right (integer division by 2)
      }
     
      printf "//%s -> %08d\n", $0, r;
      gsub(/[()]/, "", $1);
      printf "%s %d\n", $1, 256*v;
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
    $(dirname $0)/mw_neural_clustering.py $f 0
    r=$?
    if [ $r -eq 42 ]
    then
      echo "Clustering failed: not enough data"
      touch $od
    elif [ $r -gt 1 ]
    then
      echo "Clustering failed!"
      exit 2
    fi
  fi
# 
#   op=$(dirname $f)/ann_clustered.png
#   if [ -f "$op" ]
#   then
#     columnprint "Neural clustering '$op' already rendered. Skipping"
#   elif [ "$(cat $od | wc -l)" -eq 0 ]
#   then
#     montage -label 'N/A' null: $op
#   else
#     scenario=$(basename $(dirname $f))
#     score=$(getscore $scenario)
#     ntags=$(neuralclusteringcolors $od)
#     lesion=$(echo $scenario | cut -s -d_ -f 2 | tr -d l)
#     scenario=$(echo $scenario | cut -d_ -f1)
#     [ -z $lesion ] && lesion=0
# #     set -x
#     ./build/release/pp-visualizer --spln-genome $genome \
#       --ann-render --ann-tags=$ntags --ann-threshold .75 \
#       --scenario $scenario --lesions $lesion $2 || exit 2
# #     set +x
#     montage -label "$score" $op -geometry +0+0 $op~ || exit 2
#     printf "\n%s~ -> %s\n" $op~ $op
#     mv $op~ $op
#   fi
done

# aggregate="$folder/neural_groups.dat"
# if [ -f "$aggregate" ]
# then
#   columnprint "Neural aggregate '$aggregate' already generated. Skipping\n"
# else
#   columnprint "Generating data aggregate: $aggregate"
#   awk 'NR==1{ 
#     header=$0;
#     next;
#   }
#   FNR == 1 { next; }
#   {
#     neurons[$1]=1;
#     for (i=2; i<=NF; i++) {
#       if ($i != "nan") {
#         data[$1,i] += $i;
#         counts[$1,i]++;
#       }
#     }
#   }
#   END {
#     print header;
#     for (n in neurons) {
#       printf "%s", n;
#       for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
#       printf "\n";
#     }
#   }' $folder/$v[0-9]*[+-]/neurons_groups.dat > $aggregate
#   columnprint "Generated $aggregate\n"
# 
#   # Mean
#   # for (j=2; j<=NF; j++) printf " %.1f", data[n,j]/counts[n,j];
# 
#   # Union
#   # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
# 
# fi
# 
# # Aggregate individual rendered clusters
# aggregate="$folder/ann_agg_clustered.png"
# if [ -f "$aggregate" ]
# then
#   columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
# else
#   columnprint "Generating ANN clustering: $aggregate"
#   
#   sorted=$(sortbyside $folder/$v[0-9]*[+-]/ann_clustered.png)
# #   printf "\n\nSorted neural patterns:\n%s\n\n" "$sorted"
#   montage -trim -geometry "$size" $sorted $aggregate
# 
#   columnprint "Generated $aggregate\n"
# fi
# echo "$aggregate" >> .generated_files
# 
# # Rendering aggregated clustering 
# aggregate="$folder/ann_clustered.png"
# if [ -f "$aggregate" ]
# then
#   columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
# else
#   columnprint "Generating ANN clustering: $aggregate"
#   
#   # first convert activation clusters into flags (merges predator/noise and ally/voice)
#   ntags=$(neuralclusteringcolors $folder/neural_groups.dat "1;2;4;4;2")
# #     echo $ntags
#   
#   # then perform rendering
# #   set -x
#   ./build/release/pp-visualizer --spln-genome $genome \
#     --ann-render --ann-aggregate --ann-tags=$ntags --ann-threshold 0.01 $2 || exit 2
# #   set +x
#   
#   columnprint "Generated $aggregate\n"
# fi
# echo "$aggregate" >> .generated_files
# 
# # Aggregate before/after modularisation
# aggregate="$folder/ann_agg_modular.png"
# if [ -f "$aggregate" ]
# then
#   columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
# else
#   columnprint "Generating ANN clustering: $aggregate"
#   
#   montage -trim -geometry "$size" $folder/ann_colored.png $folder/ann_clustered.png $aggregate
# 
#   columnprint "Generated $aggregate\n"
# fi
# echo "$aggregate" >> .generated_files

################################################################################
# Neural clustering based on reproductible conditions
columnprint ""
columnprint "Evaluating neural clustering (first pass)"

pass(){
  echo "eval_$1_pass"
}
idpass=$(pass "first")

if [ 0 -lt $(find $folder -type d -ipath "*$idpass/E*" | wc -l) ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
#   set -x
  ./build/release/pp-evaluator --scenarios 'EE;EA;EB' --spln-genome $genome $2 || exit 2
#   set +x

  mkdir -p $folder/$idpass
  for f in $folder/E*/
  do
    o=$(dirname $f)/$idpass/$(basename $f)
    columnprint "$f -> $o"
    cp -rlf --no-target-directory $f $o
    rm -r $f
  done
fi

aggregate="$folder/neural_groups_E.dat"
if false #[ -f "$aggregate" ]
then
  columnprint "Neural aggregate '$aggregate' already generated. Skipping\n"
else

  for f in $folder/$idpass/E*/neurons.dat
  do
    od=$(dirname $f)/$(basename $f .dat)_groups.dat
    $(dirname $0)/mw_neural_clustering.py $f -1
    r=$?
    if [ $r -eq 42 ]
    then
      echo "Clustering failed: not enough data"
      touch $od
    elif [ $r -gt 0 ]
    then
      echo "Clustering failed!"
      exit 2
    fi
    
    op=$(dirname $f)/ann_clustered.$ext
    if [ -f "$op" ]
    then
      columnprint "Neural clustering '$op' already rendered. Skipping"
    elif [ "$(cat $od | wc -l)" -eq 0 ]
    then
      montage -label 'N/A' null: $op
    else
      scenario=$(basename $(dirname $f))
      score=$(getscore $scenario)
      ntags=$(neuralclusteringcolors $od "1;2;4;4;2")
      lesion=$(echo $scenario | cut -s -d_ -f 2 | tr -d l)
      scenario=$(echo $scenario | cut -d_ -f1)
      [ -z $lesion ] && lesion=0
#       set -x
      ./build/release/pp-visualizer --spln-genome $genome \
        $annRender --ann-aggregate --ann-tags=$ntags --ann-threshold .75 \
        --scenario $scenario --lesions $lesion $2 || exit 2
        
      if [ "$ext" == "png" ]
      then
        montage -label "$score" $op -geometry +0+0 $op~ || exit 2
  #       set +x
        columnprint "\n$op~ -> $op\n"
        mv $op~ $op
      fi
    fi
  done
    
  aggregate="$folder/ann_agg_clustered_E.png"
#   set -x
  montage -trim -geometry "$size" \
    $folder/$idpass/E*/ann_{colored,clustered}.png $aggregate || exit 2
#   set +x
  
  aggregate="$folder/neural_groups_E.dat"
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
      for (j=2; j<=NF; j++) printf " %.1f", (data[n,j]  == 2);
      printf "\n";
    }
  }' $folder/$idpass/E[AEB]/neurons_groups.dat > $aggregate
  columnprint "Generated $aggregate\n"

  # Mean
  # for (j=2; j<=NF; j++) printf " %.1f", data[n,j]/counts[n,j];

  # Union
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
  
  # Intersection (specifically for AEB)
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j]  == 2);
fi

# Rendering aggregated clustering 
aggregate="$folder/ann_clustered_E.$ext"
if  [ -f "$aggregate" ]
then
  columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
else
  columnprint "Generating ANN clustering: $aggregate"
  
  # first convert activation clusters into flags (merges predator/noise and ally/voice)
  ntags=$(neuralclusteringcolors $folder/neural_groups_E.dat "1;2;4;4;2")
  
  # then perform rendering (while backing up previous pictures)
  for t in clustered colored
  do
    cp -uvf $folder/ann_$t.$ext $folder/.ann_$t.$ext
  done
  
#   set -x
  ./build/release/pp-visualizer --spln-genome $genome \
    $annRender --ann-aggregate --ann-tags=$ntags --ann-threshold 0.01 $2 || exit 2
#   set +x

  for t in clustered colored
  do
    cp -uvf $folder/ann_$t.$ext $folder/ann_${t}_E.$ext
    cp -uvf $folder/.ann_$t.$ext $folder/ann_$t.$ext
  done
  
  columnprint "Generated $aggregate\n"
fi
echo "$aggregate" >> .generated_files

#########
# Tagging has been done on reproductible conditions
# -> Perform actual monitoring
idpass=$(pass second)
columnprint ""
columnprint "Evaluating neural clustering (second pass)"

# find $folder -type d -ipath "*$idpass/E*" | xargs rm -rv
if [ 0 -lt $(find $folder -type d -ipath "*$idpass/E*" | wc -l) ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
#   set -x
  ntags=$folder/neural_groups_E.ntags
  ./build/release/pp-evaluator --scenarios 'EE;EA;EB;EBI;EBM' \
    --ann-aggregate=$ntags --spln-genome $genome $2 || exit 2
#   set +x

  mkdir -p $folder/$idpass
  for f in $folder/E*/
  do
    of=$(dirname $f)/$idpass/$(basename $f)
    columnprint "$f -> $of"
    cp -rlf --no-target-directory $f $of
    rm -r $f
  done
fi

for f in $folder/$idpass/E*/
do
  o="$f/outputs.png"
  if [ -f $o ]
  then
    columnprint "Outputs summary '$o' already generated. Skipping"
  else
    cmd="
    set terminal pngcairo size 840,525 crop;
    set output '$o';
    
    unset xtics;
    set autoscale fix;
    set style fill transparent solid 0.5 noborder;
    
    vrows=3;
    set key above;
    set multiplot layout vrows+1,1 spacing 0,0;
    set rmargin 4;
    
    set yrange [-1.05:1.05];
    plot for [i=1:3] '$f/outputs.dat' using 0:i with lines title columnhead;
    
    set yrange [-.05:1.05];
    do for [r=1:vrows] {
      if (r > 1) { unset key; };
      if (r == vrows) { set xtics; };
      set y2label 'Channel '.r;
      plot for [i=1:0:-1] '$f/vocalisation.dat' using 0:i*vrows+r+1 with boxes title (i==0 ? 'Subject' : 'Ally');
    };
    unset multiplot"
    
#     printf "%s\n" "$cmd"
    gnuplot -e "$cmd" || exit 2
  fi
done

aggregate="$folder/modular_responses.png"
if [ -f $aggregate ]
then
  columnprint "Modular responses '$aggregate' already generated. Skipping"
else
  prettyTitles=$(awk 'NR==1{
    for (i=2; i<=NF; i+=2) {
      sub(/M/, "", $i);
      tag=$i/256;
      stag="";
      if (tag == 0) stag="None";
      else if (tag == 7) stag="All";
      else {
        sep=""
        if (and(tag, 1)) stag = stag"Goal";
        if (length(stag) > 0) sep="/";
        if (and(tag, 2)) stag = stag sep "Ally";
        if (length(stag) > 0) sep="/";
        if (and(tag, 4)) stag = stag sep "Enemy";
      }
      printf "%d %s ", tag, stag;
    }
    exit;
  }' $folder/$idpass/EB/modules.dat)

  cmd="
    set term pngcairo size 840,525;
    set output '$aggregate';

    headerData='$prettyTitles';
    title(i)=word(headerData, i);
    color(i)=word(headerData, i-1);
    
    set multiplot layout 5,1;
    set yrange [-.05:1.05];
    set ytics 0,.25,1;
    
    set rmargin 3;
    set tmargin 0;
    set lmargin 4;
    set bmargin 0;
    set grid;
    set xtics format '';
    set key horizontal center at screen 0.5,.9;
    
    set multiplot next;
    
    do for [i in 'E A B'] {
      if (i eq 'A') {
        unset key;
        set ytics 0,.25,.75;
      };
      if (i eq 'B') {
        set xtics format;
      };

      f='$folder/$idpass/E'.i.'/modules.dat';
      cols=system('head -n1 '.f.' | wc -w');
      set y2label i;
      plot for [j=2:cols:2] '< tail -n +2 '.f using 0:j with lines lc color(j) title title(j);
    };
    unset multiplot;
    
    set term pngcairo size 840,260;
    unset y2label;
    do for [i in 'A E B BI BM'] {
      set output '$folder/$idpass/E'.i.'/modules.png';
      set key above;
      f='$folder/$idpass/E'.i.'/modules.dat';
      cols=system('head -n1 '.f.' | wc -w');
      plot for [j=2:cols:2] '< tail -n +2 '.f using 0:j with lines lc color(j) title title(j);    
    }
  "
  
#   printf "%s\n" "$cmd"
  gnuplot -e "$cmd" || exit 3
  columnprint "Generated $aggregate"
fi

aggregate="$folder/communication_summary_E.png"
if [ -f "$aggregate" ]
then
  columnprint "Communication '$aggregate' already generated. Skipping"
else
  size='1x1+10+10<'
  inputs=$(ls -f $folder/$idpass/EB*/{outputs,modules}.png \
    | sed "s|\($folder/$idpass/\(EB.*\)/modules.png\)|-label \2 \1|")
  
  printf "\n\n\n"; set -x;
  montage -tile 3x2 -trim -geometry "$size" $inputs $aggregate
  set +x; echo;
  columnprint "Generated $aggregate"
fi

aggregate="$folder/modules_summary.png"
if [ -f "$aggregate" ]
then
  columnprint "Modular responses '$aggregate' already generated. Skipping"
else
  size='1x1+0+0<'
  anntmp=.anntmp
  montage -tile 2x -trim -geometry "$size" \
    $folder/ann_colored_E.png $folder/ann_clustered_E.png $anntmp
    
  montage -tile x2 -trim -geometry "$size" \
    $anntmp $folder/modular_responses.png $aggregate
    
  rm $anntmp
  columnprint "Generated $aggregate"
fi


################################################################################
## Selective lesions (red > amygdala)

idpass="selective_lesions"
# find $folder -type d -ipath "*$idpass/E*" | xargs rm -rv
if [ 0 -lt $(find $folder -type d -ipath "*$idpass/*l[45]" | wc -l) ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
  set -x
  ntags=$folder/neural_groups_E.ntags
  ./build/release/pp-evaluator --scenarios 'all' \
    --ann-aggregate=$ntags --spln-genome $genome $2 --lesions "0;4;5" || exit 2
  set +x

  mkdir -p $folder/$idpass
  for f in $folder/*l[45]/
  do
    of=$(dirname $f)/$idpass/$(basename $f)
    columnprint "$f -> $of"
    cp -rlf --no-target-directory $f $of
    rm -r $f
  done
fi

# Generate trajectories overlay
for l in 4 5
do
  for s in + -
  do
    o=$folder/trajectories_v${v}_${s}_l${l}.$ext
    if [ -f "$o" ]
    then
      columnprint "Trajectory overlay file '$o' already generated. Skipping"
      continue
    fi
    
    # Final positions
  #     for [i=1:words(list)] '<tail -n1 '.word(list, i) using 2:3:(.5) with circles lc ''.color(i) fs transparent solid .25 notitle,\
    
    term="pngcairo size 840,525 crop"
    [ "$ext" == "pdf" ] && term="pdfcairo size 6,3.5 font 'Monospace,12'";
    
  #   scenario=$(basename $(dirname $f))
  #   score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
    files=$(ls $folder/$idpass/*${s}_l$l/trajectory.dat)
    lengths=$(wc -l $files | grep -v "total" | tr -s " " | cut -d ' ' -f 2)
    read ew eh < <(head -n 1 $(echo $files | tr " " "\n" | head -n 1) | cut -d ' ' -f 3-4)

    cmd="
      set terminal $term;
      set output '$o';
      set size ratio $eh./$ew;
      set xrange [-$ew:$ew];
      set yrange [-$eh:$eh];
      set cblabel 'step';
      set key above;
      unset xtics;
      unset ytics;
      set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
      set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
      set object 1 rect from 0,-.25 to $ew-1.5,.25 fc 'black' front;
      list='$files';
      color(i)=word('green blue red purple', i);
      hsv(i)=word('.33 .66 0 .87', i); 
      length(i)=word('$lengths', i);
      dcolor(i,j)=hsv2rgb(hsv(i)+0, j/(length(i)+0), j/(length(i)+0));
      stitle(i)=system('basename \$(dirname '.word(list, i).') | cut -c2-');
      plot for [i=1:words(list)] word(list, i) every :::1 using 2:3:(dcolor(i, column(0))) with lines lc rgbcolor variable notitle,\
            for [i=1:words(list)] word(list, i) every :::1 using $(vstyle 1 25 0.25 "''.color(i)") title stitle(i);"
            
#       printf "\n%s\n\n" "$cmd"
    columnprint "Plotting overlay $o"
    gnuplot -e "$cmd" || exit 3
  done
  
done

# set -x
# for s in + -
# do
#   tmpa=$folder/.traj_${s}_l.png
#   montage -geometry '1x1+1+1<' -tile 1x2 $folder/trajectories_v${v}_${s}_l*.png $tmpa
#   tmpb=$folder/.traj_${s}.png
#   montage -geometry '1x1+1+1<' -tile 2x1 $folder/trajectories_v${v}_${s}.png $tmpa $tmpb
# done
# montage -geometry '1x1+1+10<' -tile 1x2 $folder/.traj_{+,-}.png $folder/trajectories_with_lesions.png
# rm -v $folder/.traj_[-+]*.png
# set +x

# Generate trajectories
for f in $folder/$idpass/$v[0-9]*/trajectory.dat
do
  o=$(dirname $f)/$(basename $f dat)png
  if [ -f "$o" ]
  then
    columnprint "Trajectory file '$o' already generated. Skipping"
    continue
  fi
  
  scenario=$(basename $(dirname $f))
  score=$(getscore $scenario)
  read ew eh < <(head -n 1 $f | cut -d ' ' -f 3-4)
  cmd="
    set terminal pngcairo size 840,525;
    set output '$o';
    set size ratio $eh./$ew;
    set xrange [-$ew:$ew];
    set yrange [-$eh:$eh];
    set cblabel 'step';
    unset key;
    set title '$score';
    set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
    set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
    set object 1 rect from 0,-.25 to $ew-1.5,.25 fc 'black' front;
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
# 
# montage -geometry '1x1+5+10<' \
#   $folder/{12+,$idpass/12+_l4,$idpass/12+_l5}/trajectory.png \
#   $folder/{12-,$idpass/12-_l4,$idpass/12-_l5}/trajectory.png \
#   $folder/trajectories_with_lesions.png

extractTitles(){
  awk 'NR==1{
    for (i=1; i<=NF; i++) {
      sub(/M/, "", $i);
      tag=$i/256;
      stag="";
      if (tag == 0) stag="None";
      else if (tag == 6) stag="Both";
      else {
        sep=""
        if (and(tag, 2)) stag = stag sep "Ally";
        if (length(stag) > 0) sep="/";
        if (and(tag, 4)) stag = stag sep "Enemy";
      }
      printf "%d %s ", tag/2, stag;
    }
    exit;
  }' $1
}

for f in {$folder/${v}[0-3][-+]/,$folder/$idpass/$v[0-9]*/}modules.dat
do
  o=$(dirname $f)/$(basename $f dat)png
  if [ -f "$o" ]
  then
    columnprint "Modules file '$o' already generated. Skipping"
    continue
  fi

  mtmp=.mtmp
  $(dirname $0)/reorder_and_filter_columns.awk -vcolumns="0M 1536M 512M 1024M" $f > $mtmp
  cmd="
    set term pngcairo crop size 840,525 font 'Monospace,12';
    set output '$o';
  
    set grid;
    set yrange [-.05:1.05];
    set autoscale fix;
    
    set xlabel 'Steps';
    set ylabel 'Activity';
    
    set key above;

    headerData='$(extractTitles $mtmp)';
    title(i)=word(headerData, 2*i);
    color(i)=word('#000000 #0000FF #FF0000 #7F007F', word(headerData, 2*i-1)+1);
    dash(i)=word('1 1 1 0', word(headerData, 2*i-1)+1)+0;
      
    cols=words(headerData)/2;
    plot for [j=1:cols] '< tail -n +2 $mtmp' using 0:j with lines lc rgb color(j) dt dash(j) title ''.title(j);  
    
    set term pdfcairo crop size 6,3.5 font 'Monospace,21';
    set output '$(dirname $o)/$(basename $o png)pdf';
    replot;
  "
#   printf "%s\n" "$cmd"
  gnuplot -e "$cmd" || exit 3
  rm $mtmp
done

################################################################################
idpass=eval_canonical_with_lesions
columnprint ""
columnprint "Applying lesions on canonicals"

# find $folder -type d -ipath "*$idpass/E*" | xargs rm -rv
if [ 0 -lt $(find $folder -type d -ipath "*$idpass/E*" | wc -l) ]
then
  columnprint "Data folder '$folder' already exists. Skipping"
else
#   set -x
  ntags=$folder/neural_groups_E.ntags
  ./build/release/pp-evaluator --scenarios 'EE;EA;EB' --lesions "4;5" \
    --ann-aggregate=$ntags --spln-genome $genome $2 || exit 2
#   set +x

  mkdir -p $folder/$idpass
  for f in $folder/E*/
  do
    of=$(dirname $f)/$idpass/$(basename $f)
    columnprint "$f -> $of"
    cp -rlf --no-target-directory $f $of
    rm -r $f
  done
fi

echo
