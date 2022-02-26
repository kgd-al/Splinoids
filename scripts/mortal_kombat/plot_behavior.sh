#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome to extract data from"
  exit 1
fi

columnprint(){
  printf "%-*s\n" $(tput cols) "$1"
#   printf "."
}

parts(){
  readlink -m "$1" \
  | sed 's|\(.*\)/ID\(.*\)/\([A-Z]\)/gen\(.*\)/.*.dna|\1 \2 \3 \4|'
}

fetchopponents(){
  read base id p g <<< $(parts "$ind")
  gen="gen"$(($g - 1))
  ls $base/ID$id/[A-Z]/$gen/mk__*_0.dna | grep -v "/$p/"
}

datafolder(){
  read lbase lid lp lg <<< $(parts "$1")
  read rbase rid rp rg <<< $(parts "$2")
  [ ! -z $3 ] && scnr="__$3"
  echo $indfolder/$lid-$lp-$lg-0__$rid-$rp-$rg-0$scnr/
}

ind="$1"
columnprint "Processing $ind"
opponents=$(fetchopponents)
# printf "opponents: %s\n" $opponents
indfolder=$(dirname $ind)/$(basename $ind .dna)
shift

sfolder=$(dirname $0) # script folder

validscenarios="self othr"
if [ $(jq '.dna | fromjson | length' $ind) -eq 2 ]
then
  validscenarios="$validscenarios ally"
fi

bins=10
bwidth=$(awk -vbins=$bins 'BEGIN{print 2/bins;}')

ext=png
if [ ! -z $2 ]
then
  ext=$2
fi

# Visu flag for pdf rendering
annRender="--ann-render=$ext"

################################################################################
# Behavior against previous champion(s)
if [ -d $indfolder ]
then
  columnprint "Data folder '$indfolder' already exists. Skipping"
else
  $(dirname $0)/evaluate.sh $ind #|| exit 2
fi

################################################################################
# Generic output
for opp in $opponents
do  
  dfolder=$(datafolder $ind $opp)
  
  ##############################################################################
  # Health dynamics (rough strategy overview)
  healthoverview=$dfolder/health.png
  if [ -f $healthoverview ]
  then
    columnprint "Health overview '$healthoverview' already generated. Skipping"
  else
    gnuplot -e "
      set output '$healthoverview';
      set term pngcairo size 1680,1050;
      
      set multiplot layout 1,2;
      set xrange [0:20];
      set yrange [-.05:1.05];
      set key above;
      
      do for [f in system('ls $dfolder/[lr]hs.dat')] {
        plot for [i=5:6] f using 1:i w l title columnhead;
      };
      unset multiplot;
    " || rm -fv $healthoverview
    columnprint "Generated Health overview '$healthoverview'"
  fi
  
  ##############################################################################
  # Communication (rough conspecific interaction)
  soundoverview=$dfolder/sounds.png
  if [ -f $soundoverview ]
  then
    columnprint "Sound overview '$soundoverview' already generated. Skipping"
  else
    gnuplot -e "
      set output '$soundoverview';
      set term pngcairo size 1680,1050;
      
      set multiplot layout 4,3;
      set yrange [-.05:1.05];
      set key above;
      
      do for [j=0:3] {
        do for [i=0:2] {
          plot '$dfolder/acoustics.dat' using 0:i*4+j+1 w l title columnhead;
        }
      };
      unset multiplot;
    " || rm -fv $soundoverview
    columnprint "Generated Sound overview '$soundoverview'"
  fi
done

################################################################################
# Plot 3D ANN (with gnuplot since Qt is unpleasantly uncooperative)
aggregate="$indfolder/ann.$ext"
if [ -f "$aggregate" ]
then
  columnprint "ANN already generated. Skipping"
else
  rm -f $dfolder/brain.[xy][yz].dat
  awk -vbins=$bins -vbwidth=$bwidth -vof=$dfolder '
    function bin(x) {
      return int((x+1)/bwidth);
    }
    function coord(b) {
      return (b+.5)*bwidth-1;
    }
    function print3(f, v_xy, v_xz, v_yz) {
      printf f, v_xy >> of "/brain.xy.dat";
      printf f, v_xz >> of "/brain.xz.dat";
      printf f, v_yz >> of "/brain.yz.dat";  
    }
    {
      if ($1 != 2) next;
      bx = bin($2); by = bin($3); bz = bin($4);
      xy[bx][by]++;
      xz[bx][bz]++;
      yz[by][bz]++;
    }
    END {
      print3("- ");
      for (i=0; i<bins; i++) {
        v = coord(i);
        print3("%g ", v, v, v);
      }
      print3("%s", "\n", "\n", "\n");
      for (i=0; i<bins; i++) {
        v = coord(i);
        print3("%g ", v, v, v);
        for (j=0; j<bins; j++)
          print3("%d ", xy[j][i], xz[j][i], yz[j][i]);
        print3("%s", "\n", "\n", "\n");
      }
    }
  ' $dfolder/brain.dat
  
  gnuplot -e "
    set output '$aggregate';
    set term pngcairo size 1050,1050;
    
    unset key;
    
    set multiplot layout 2,2;
    
    set hidden3d;
    set xyplane 0;
    set view equal xyz;
    set grid;
    set xrange [-1.02:1.02];
    set yrange [-1.02:1.02];
    set zrange [-1.02:1.02];
    set xlabel 'X';
    set ylabel 'Y';
    splot '$dfolder/brain.dat' using 2:3:4:1 pt 7 lc rgb variable;
    unset xrange;
    unset yrange;
    
    set autoscale xfix;
    set autoscale yfix;
    unset colorbox;
    set palette gray negative;
    do for [p in 'yz xz xy'] {
      set xlabel substr(p, 1, 1);
      set ylabel substr(p, 2, 2);
      plot '$dfolder/brain.'.p.'.dat' matrix rowheaders columnheaders with image;
    };
    
  " || rm -v $aggregate
  columnprint "Rendered ANN to '$aggregate'"
fi

################################################################################
# # Neural clustering based on reproductible conditions

neuralclusteringcolors(){
  o=$(dirname $1)/$(basename $1 dat)ntags
  ltags="1;2;4;8;16"
  [ ! -z "$2" ] && ltags=$2
  awk -vltags="$ltags" '
    BEGIN {
      split(ltags, tags, ";");
      n=length(tags);
      printf "// tags: %d", tags[1];
      for (i=2; i<=n; i++)
        printf ",%d", tags[i];
      printf "\n";
    }
    NR==1 { print "//    ", $0; }
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
     
      printf "// %s -> %0*d\n", $0, n, r;
      gsub(/[()]/, "", $1);
      printf "%s %d\n", $1, v;
    }' $1 > $o
  ls $o
}

gnuplotplotclusters(){
  d=$1 # groups data
  o=$2 # output file
  f=$(dirname $d) 
  
  ntags=$(neuralclusteringcolors $d "1;2;4")
  
  for e in '' '.xy.' '.xz' '.yz'; do printf "" > $f/brain$e.gp.dat; done
  awk -vbins=$bins -vbwidth=$bwidth -vof=$f '
    function bin(x) {
      return int((x+1)/bwidth);
    }
    function coord(b) {
      return (b+.5)*bwidth-1;
    }
    function process(a, n) { # array, name
      ofile = of "/brain." n ".gp.dat";
      
      max = 0;
      for (i=0; i<bins; i++) {
        for (j=0; j<bins; j++) {
          if (typeof(a[j][i]) == "array") {
            m = 0;
            for (k in a[j][i]) m += a[j][i][k];
            if (max < m) max = m;
          }
        }
      }
      
      for (i=0; i<bins; i++) {
        for (j=0; j<bins; j++) {
          printf "%g %g ", coord(j), coord(i) >> ofile;
          if (typeof(a[j][i]) == "array") {
            t=0; r=0; g=0; b=0;
            for (flag in a[j][i]) {
              n = a[j][i][flag];
              t += n;
              if (and(flag, 4)) r += n;
              if (and(flag, 2)) g += n;
              if (and(flag, 1)) b += n;
            }
            
            printf "%d %d %d %d\n", 0xFF * r / t, \
                                    0xFF * g / t, \
                                    0xFF * b / t, \
                                    0xFF * t / max >> ofile;
          } else
            printf "0 0 0 0\n" >> ofile;
        }
      }

    }
    /^\/\//{ next; }
    {  }
    {
      split($1, c, ",");
      printf "%g %g %g 0x%08x\n", \
        c[1], c[2], c[3], 0x7F000000 \
                        + 0x00FF0000 * and($2, 4) \
                        + 0x0000FF00 * and($2, 2) \
                        + 0x000000FF * and($2, 1) >> of "/brain.gp.dat";
                        
      bx = bin(c[1]); by = bin(c[2]); bz = bin(c[3]);
      xy[bx][by][$2]++;
      xz[bx][bz][$2]++;
      yz[by][bz][$2]++;
    }
    END {
      process(xy, "xy");
      process(xz, "xz");
      process(yz, "yz");
    }
  ' $ntags
  
  gnuplot -e "
    set output '$o';
    set term pngcairo size 1050,1050;
    
    unset key;
    
    set multiplot layout 2,2;
    
    set hidden3d;
    set xyplane 0;
    set view equal xyz;
    set grid;
    set xrange [-1.02:1.02];
    set yrange [-1.02:1.02];
    set zrange [-1.02:1.02];
    set xlabel 'X';
    set ylabel 'Y';
    splot '$f/brain.gp.dat' using 1:2:3:4 pt 7 lc rgb variable;
    unset xrange;
    unset yrange;
    
    set autoscale xfix;
    set autoscale yfix;
    unset colorbox;
    do for [p in 'yz xz xy'] {
      set xlabel substr(p, 1, 1);
      set ylabel substr(p, 2, 2);
      plot '$f/brain.'.p.'.gp.dat' with rgbalpha;
    };
    
  "
  rm -f $f/brain*.gp.dat
}
columnprint ""
columnprint "Evaluating neural clustering (first pass)"

pass(){
  echo "eval_$1_pass"
}
idpass=$(pass "first")

for o in $opponents
do
  dfolder=$(datafolder $ind $o)
  if ls $dfolder/$idpass/* >/dev/null 2>&1
  then
    columnprint "Data folder '$dfolder/$idpass' already populated. Skipping"
  else
    mkdir -p $dfolder/$idpass
    
  #   set -x
    for s in $validscenarios
    do
      $sfolder/evaluate.sh $ind $o --scenario $s
      
      idir=$(datafolder $ind $o $s)
      odir=$dfolder/$idpass/$s
      columnprint "$idir -> $odir"
      cp -rlf --no-target-directory $idir $odir && rm -r $idir
    done
  #   set +x
  fi
done
 
aggregate="$indfolder/neural_groups.dat"
if [ -f "$aggregate" ]
then
  columnprint "Neural aggregate '$aggregate' already generated. Skipping"
else

  for f in $indfolder/*/$idpass/*/neurons.dat
  do
    dfolder=$(dirname $f)
    od=$dfolder/$(basename $f .dat)_groups.dat
    if [ ! -f $od ]
    then
      touch $od
      $(dirname $0)/mw_neural_clustering.py $f -1
      r=$?
      if [ $r -eq 41 ]
      then
        echo "Clustering failed: not enough neurons"
      elif [ $r -gt 42 ]
      then
        echo "Clustering failed: not enough data"
      elif [ $r -gt 0 ]
      then
        echo "Clustering failed!"
        exit 2
      fi
      columnprint "Generated '$od'"
    fi
    
    op=$dfolder/ann_clustered.$ext
    if [ -f "$op" ]
    then
      columnprint "Neural clustering '$op' already rendered. Skipping"
    elif [ "$(cat $od | wc -l)" -eq 0 ]
    then
      montage -label 'N/A' null: $op
    else
      gnuplotplotclusters $od $op
      columnprint "Rendered ANN to '$op'"
    fi
  done
    
  aggregate="$indfolder/neural_groups.dat"
  columnprint "Generating data aggregate: $aggregate"
  
  ng_dat_files=$(ls $indfolder/*/$idpass/*/neurons_groups.dat)
  half=$(ls $ng_dat_files | awk 'END{print NR/2}')
  tail -n+0 $ng_dat_files
  awk -vthr=$half '
  NR==1{ 
    header=$0;
    print "header:", header;
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
      for (j=2; j<=NF; j++) printf " %d", (data[n,j] > 0);
      printf "\n";
    }
  }' $ng_dat_files > $aggregate
  columnprint "Generated $aggregate"

  # Mean
  # for (j=2; j<=NF; j++) printf " %.1f", data[n,j]/counts[n,j];

  # Union
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
  
  # Intersection (specifically for AEB)
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > thr);
fi

aggregate=$indfolder/ann_agg.gp.$ext
if [ -f $aggregate ]
then
  columnprint "Aggregate $aggregate already exists. Skipping"
else
  gnuplotplotclusters $indfolder/neural_groups.dat $aggregate
  columnprint "Generated $aggregate"
fi

# # Rendering aggregated clustering 
# aggregate="$indfolder/ann_clustered.$ext"
# if  [ -f "$aggregate" ]
# then
#   columnprint "ANN clustering '$aggregate' already generated. Skipping\n"
# else
#   columnprint "Generating ANN clustering: $aggregate"
#   
#   # then perform rendering (while backing up previous pictures)
#   for t in clustered colored
#   do
#     cp -uvf $folder/ann_$t.$ext $folder/.ann_$t.$ext
#   done
#   
# #   set -x
#   ./build/release/pp-visualizer --spln-genome $genome \
#     $annRender --ann-aggregate --ann-tags=$ntags --ann-threshold 0.01 $2 || exit 2
# #   set +x
# 
#   for t in clustered colored
#   do
#     cp -uvf $folder/ann_$t.$ext $folder/ann_${t}_E.$ext
#     cp -uvf $folder/.ann_$t.$ext $folder/ann_$t.$ext
#   done
#   
#   columnprint "Generated $aggregate\n"
# fi
# echo "$aggregate" >> .generated_files

# #########
# # Tagging has been done on reproductible conditions
# # -> Perform actual monitoring
# idpass=$(pass second)
# columnprint ""
# columnprint "Evaluating neural clustering (second pass)"
# 
# # find $folder -type d -ipath "*$idpass/E*" | xargs rm -rv
# if [ 0 -lt $(find $folder -type d -ipath "*$idpass/E*" | wc -l) ]
# then
#   columnprint "Data folder '$folder' already exists. Skipping"
# else
# #   set -x
#   ntags=$folder/neural_groups_E.ntags
#   ./build/release/pp-evaluator --scenarios 'EE;EA;EB;EBI;EBM' \
#     --ann-aggregate=$ntags --spln-genome $genome $2 || exit 2
# #   set +x
# 
#   mkdir -p $folder/$idpass
#   for f in $folder/E*/
#   do
#     of=$(dirname $f)/$idpass/$(basename $f)
#     columnprint "$f -> $of"
#     cp -rlf --no-target-directory $f $of
#     rm -r $f
#   done
# fi
# 
# for f in $folder/$idpass/E*/
# do
#   o="$f/outputs.png"
#   if [ -f $o ]
#   then
#     columnprint "Outputs summary '$o' already generated. Skipping"
#   else
#     cmd="
#     set terminal pngcairo size 840,525 crop;
#     set output '$o';
#     
#     unset xtics;
#     set autoscale fix;
#     set style fill transparent solid 0.5 noborder;
#     
#     vrows=3;
#     set key above;
#     set multiplot layout vrows+1,1 spacing 0,0;
#     set rmargin 4;
#     
#     set yrange [-1.05:1.05];
#     plot for [i=1:3] '$f/outputs.dat' using 0:i with lines title columnhead;
#     
#     set yrange [-.05:1.05];
#     do for [r=1:vrows] {
#       if (r > 1) { unset key; };
#       if (r == vrows) { set xtics; };
#       set y2label 'Channel '.r;
#       plot for [i=1:0:-1] '$f/vocalisation.dat' using 0:i*vrows+r+1 with boxes title (i==0 ? 'Subject' : 'Ally');
#     };
#     unset multiplot"
#     
# #     printf "%s\n" "$cmd"
#     gnuplot -e "$cmd" || exit 2
#   fi
# done
# 
# aggregate="$folder/modular_responses.png"
# if [ -f $aggregate ]
# then
#   columnprint "Modular responses '$aggregate' already generated. Skipping"
# else
#   prettyTitles=$(awk 'NR==1{
#     for (i=2; i<=NF; i+=2) {
#       sub(/M/, "", $i);
#       tag=$i/256;
#       stag="";
#       if (tag == 0) stag="None";
#       else if (tag == 7) stag="All";
#       else {
#         sep=""
#         if (and(tag, 1)) stag = stag"Goal";
#         if (length(stag) > 0) sep="/";
#         if (and(tag, 2)) stag = stag sep "Ally";
#         if (length(stag) > 0) sep="/";
#         if (and(tag, 4)) stag = stag sep "Enemy";
#       }
#       printf "%d %s ", tag, stag;
#     }
#     exit;
#   }' $folder/$idpass/EB/modules.dat)
# 
#   cmd="
#     set term pngcairo size 840,525;
#     set output '$aggregate';
# 
#     headerData='$prettyTitles';
#     title(i)=word(headerData, i);
#     color(i)=word(headerData, i-1);
#     
#     set multiplot layout 5,1;
#     set yrange [-.05:1.05];
#     set ytics 0,.25,1;
#     
#     set rmargin 3;
#     set tmargin 0;
#     set lmargin 4;
#     set bmargin 0;
#     set grid;
#     set xtics format '';
#     set key horizontal center at screen 0.5,.9;
#     
#     set multiplot next;
#     
#     do for [i in 'E A B'] {
#       if (i eq 'A') {
#         unset key;
#         set ytics 0,.25,.75;
#       };
#       if (i eq 'B') {
#         set xtics format;
#       };
# 
#       f='$folder/$idpass/E'.i.'/modules.dat';
#       cols=system('head -n1 '.f.' | wc -w');
#       set y2label i;
#       plot for [j=2:cols:2] '< tail -n +2 '.f using 0:j with lines lc color(j) title title(j);
#     };
#     unset multiplot;
#     
#     set term pngcairo size 840,260;
#     unset y2label;
#     do for [i in 'A E B BI BM'] {
#       set output '$folder/$idpass/E'.i.'/modules.png';
#       set key above;
#       f='$folder/$idpass/E'.i.'/modules.dat';
#       cols=system('head -n1 '.f.' | wc -w');
#       plot for [j=2:cols:2] '< tail -n +2 '.f using 0:j with lines lc color(j) title title(j);    
#     }
#   "
#   
# #   printf "%s\n" "$cmd"
#   gnuplot -e "$cmd" || exit 3
#   columnprint "Generated $aggregate"
# fi
# 
# aggregate="$folder/communication_summary_E.png"
# if [ -f "$aggregate" ]
# then
#   columnprint "Communication '$aggregate' already generated. Skipping"
# else
#   size='1x1+10+10<'
#   inputs=$(ls -f $folder/$idpass/EB*/{outputs,modules}.png \
#     | sed "s|\($folder/$idpass/\(EB.*\)/modules.png\)|-label \2 \1|")
#   
#   printf "\n\n\n"; set -x;
#   montage -tile 3x2 -trim -geometry "$size" $inputs $aggregate
#   set +x; echo;
#   columnprint "Generated $aggregate"
# fi
# 
# aggregate="$folder/modules_summary.png"
# if [ -f "$aggregate" ]
# then
#   columnprint "Modular responses '$aggregate' already generated. Skipping"
# else
#   size='1x1+0+0<'
#   anntmp=.anntmp
#   montage -tile 2x -trim -geometry "$size" \
#     $folder/ann_colored_E.png $folder/ann_clustered_E.png $anntmp
#     
#   montage -tile x2 -trim -geometry "$size" \
#     $anntmp $folder/modular_responses.png $aggregate
#     
#   rm $anntmp
#   columnprint "Generated $aggregate"
# fi
# 
# 
# ################################################################################
# ## Selective lesions (red > amygdala)
# 
# idpass="selective_lesions"
# # find $folder -type d -ipath "*$idpass/E*" | xargs rm -rv
# if [ 0 -lt $(find $folder -type d -ipath "*$idpass/*l[45]" | wc -l) ]
# then
#   columnprint "Data folder '$folder' already exists. Skipping"
# else
#   set -x
#   ntags=$folder/neural_groups_E.ntags
#   ./build/release/pp-evaluator --scenarios 'all' \
#     --ann-aggregate=$ntags --spln-genome $genome $2 --lesions "0;4;5" || exit 2
#   set +x
# 
#   mkdir -p $folder/$idpass
#   for f in $folder/*l[45]/
#   do
#     of=$(dirname $f)/$idpass/$(basename $f)
#     columnprint "$f -> $of"
#     cp -rlf --no-target-directory $f $of
#     rm -r $f
#   done
# fi
# 
# # Generate trajectories overlay
# for l in 4 5
# do
#   for s in + -
#   do
#     o=$folder/trajectories_v${v}_${s}_l${l}.$ext
#     if [ -f "$o" ]
#     then
#       columnprint "Trajectory overlay file '$o' already generated. Skipping"
#       continue
#     fi
#     
#     # Final positions
#   #     for [i=1:words(list)] '<tail -n1 '.word(list, i) using 2:3:(.5) with circles lc ''.color(i) fs transparent solid .25 notitle,\
#     
#     term="pngcairo size 840,525 crop"
#     [ "$ext" == "pdf" ] && term="pdfcairo size 6,3.5 font 'Monospace,12'";
#     
#   #   scenario=$(basename $(dirname $f))
#   #   score=$(grep "$scenario" $folder/scores.dat | sed 's/\(.*\) \(.*\)/Type: \1, Score: \2'/)
#     files=$(ls $folder/$idpass/*${s}_l$l/trajectory.dat)
#     lengths=$(wc -l $files | grep -v "total" | tr -s " " | cut -d ' ' -f 2)
#     read ew eh < <(head -n 1 $(echo $files | tr " " "\n" | head -n 1) | cut -d ' ' -f 3-4)
# 
#     cmd="
#       set terminal $term;
#       set output '$o';
#       set size ratio $eh./$ew;
#       set xrange [-$ew:$ew];
#       set yrange [-$eh:$eh];
#       set cblabel 'step';
#       set key above;
#       unset xtics;
#       unset ytics;
#       set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
#       set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
#       set object 1 rect from 0,-.25 to $ew-1.5,.25 fc 'black' front;
#       list='$files';
#       color(i)=word('green blue red purple', i);
#       hsv(i)=word('.33 .66 0 .87', i); 
#       length(i)=word('$lengths', i);
#       dcolor(i,j)=hsv2rgb(hsv(i)+0, j/(length(i)+0), j/(length(i)+0));
#       stitle(i)=system('basename \$(dirname '.word(list, i).') | cut -c2-');
#       plot for [i=1:words(list)] word(list, i) every :::1 using 2:3:(dcolor(i, column(0))) with lines lc rgbcolor variable notitle,\
#             for [i=1:words(list)] word(list, i) every :::1 using $(vstyle 1 25 0.25 "''.color(i)") title stitle(i);"
#             
# #       printf "\n%s\n\n" "$cmd"
#     columnprint "Plotting overlay $o"
#     gnuplot -e "$cmd" || exit 3
#   done
#   
# done
# 
# # set -x
# # for s in + -
# # do
# #   tmpa=$folder/.traj_${s}_l.png
# #   montage -geometry '1x1+1+1<' -tile 1x2 $folder/trajectories_v${v}_${s}_l*.png $tmpa
# #   tmpb=$folder/.traj_${s}.png
# #   montage -geometry '1x1+1+1<' -tile 2x1 $folder/trajectories_v${v}_${s}.png $tmpa $tmpb
# # done
# # montage -geometry '1x1+1+10<' -tile 1x2 $folder/.traj_{+,-}.png $folder/trajectories_with_lesions.png
# # rm -v $folder/.traj_[-+]*.png
# # set +x
# 
# # Generate trajectories
# for f in $folder/$idpass/$v[0-9]*/trajectory.dat
# do
#   o=$(dirname $f)/$(basename $f dat)png
#   if [ -f "$o" ]
#   then
#     columnprint "Trajectory file '$o' already generated. Skipping"
#     continue
#   fi
#   
#   scenario=$(basename $(dirname $f))
#   score=$(getscore $scenario)
#   read ew eh < <(head -n 1 $f | cut -d ' ' -f 3-4)
#   cmd="
#     set terminal pngcairo size 840,525;
#     set output '$o';
#     set size ratio $eh./$ew;
#     set xrange [-$ew:$ew];
#     set yrange [-$eh:$eh];
#     set cblabel 'step';
#     unset key;
#     set title '$score';
#     set arrow from graph 0,.5 to graph 1,.5 lc rgb 'gray' dt 2 nohead;
#     set arrow from graph .5,0 to graph .5,1 lc rgb 'gray' dt 2 nohead;
#     set object 1 rect from 0,-.25 to $ew-1.5,.25 fc 'black' front;
#     plot 1/0 $pstyle title 'plant', \
#           '<tail -n +3 $f' every :::::0 using 1:2:3 $pstyle notitle, \
#           '$f' every :::1 using 2:3:0 $tstyle title 'trajectory',\
#             '' every :::1 using $(vstyle 1 10 0.25 "'black'") notitle, \
#           '$f' every :::1 using 5:6 w l lc 'blue' title 'clone', \
#             '' every :::1 using $(vstyle 4 25 0.125 "'blue'") notitle, \
#           '$f' every :::1 using 8:9 w l lc 'red' title 'predator', \
#             '' every :::1 using $(vstyle 7 25 0.125 "'red'") notitle;"
#            
# #   echo "$cmd"
#   columnprint "Plotting $f"
#   gnuplot -e "$cmd" || exit 3
# done
# # 
# # montage -geometry '1x1+5+10<' \
# #   $folder/{12+,$idpass/12+_l4,$idpass/12+_l5}/trajectory.png \
# #   $folder/{12-,$idpass/12-_l4,$idpass/12-_l5}/trajectory.png \
# #   $folder/trajectories_with_lesions.png
# 
# extractTitles(){
#   awk 'NR==1{
#     for (i=1; i<=NF; i++) {
#       sub(/M/, "", $i);
#       tag=$i/256;
#       stag="";
#       if (tag == 0) stag="None";
#       else if (tag == 6) stag="Both";
#       else {
#         sep=""
#         if (and(tag, 2)) stag = stag sep "Ally";
#         if (length(stag) > 0) sep="/";
#         if (and(tag, 4)) stag = stag sep "Enemy";
#       }
#       printf "%d %s ", tag/2, stag;
#     }
#     exit;
#   }' $1
# }
# 
# for f in {$folder/${v}[0-3][-+]/,$folder/$idpass/$v[0-9]*/}modules.dat
# do
#   o=$(dirname $f)/$(basename $f dat)png
#   if [ -f "$o" ]
#   then
#     columnprint "Modules file '$o' already generated. Skipping"
#     continue
#   fi
# 
#   mtmp=.mtmp
#   $(dirname $0)/reorder_and_filter_columns.awk -vcolumns="0M 1536M 512M 1024M" $f > $mtmp
#   cmd="
#     set term pngcairo crop size 840,525 font 'Monospace,12';
#     set output '$o';
#   
#     set grid;
#     set yrange [-.05:1.05];
#     set autoscale fix;
#     
#     set xlabel 'Steps';
#     set ylabel 'Activity';
#     
#     set key above;
# 
#     headerData='$(extractTitles $mtmp)';
#     title(i)=word(headerData, 2*i);
#     color(i)=word('#000000 #0000FF #FF0000 #7F007F', word(headerData, 2*i-1)+1);
#     dash(i)=word('1 1 1 0', word(headerData, 2*i-1)+1)+0;
#       
#     cols=words(headerData)/2;
#     plot for [j=1:cols] '< tail -n +2 $mtmp' using 0:j with lines lc rgb color(j) dt dash(j) title ''.title(j);  
#     
#     set term pdfcairo crop size 6,3.5 font 'Monospace,21';
#     set output '$(dirname $o)/$(basename $o png)pdf';
#     replot;
#   "
# #   printf "%s\n" "$cmd"
#   gnuplot -e "$cmd" || exit 3
#   rm $mtmp
# done
# 
# ################################################################################
# idpass=eval_canonical_with_lesions
# columnprint ""
# columnprint "Applying lesions on canonicals"
# 
# # find $folder -type d -ipath "*$idpass/E*" | xargs rm -rv
# if [ 0 -lt $(find $folder -type d -ipath "*$idpass/E*" | wc -l) ]
# then
#   columnprint "Data folder '$folder' already exists. Skipping"
# else
# #   set -x
#   ntags=$folder/neural_groups_E.ntags
#   ./build/release/pp-evaluator --scenarios 'EE;EA;EB' --lesions "4;5" \
#     --ann-aggregate=$ntags --spln-genome $genome $2 || exit 2
# #   set +x
# 
#   mkdir -p $folder/$idpass
#   for f in $folder/E*/
#   do
#     of=$(dirname $f)/$idpass/$(basename $f)
#     columnprint "$f -> $of"
#     cp -rlf --no-target-directory $f $of
#     rm -r $f
#   done
# fi
# 
# echo
