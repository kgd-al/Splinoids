#!/bin/bash

if [ ! -d "$1" ]
then
  echo "Please provide a genome data folder"
  exit 1
fi

line(){ 
  c=$1; [ -z $c ] && c='#'
  n=$2; [ -z $n ] && n=80
  printf "$c%.0s" $(seq 1 $n)
  echo
}

ind=$(dirname $1)/$(basename $1).dna
if [ ! -f "$ind" ]
then
  echo "Could not find champion dna for folder '$1'"
  exit 1
fi

echo "Processing $ind"
indfolder=$(dirname $ind)/$(basename $ind .dna)
shift

sfolder=$(dirname $0) # script folder

bins=10
bwidth=$(awk -vbins=$bins 'BEGIN{print 2/bins;}')

ssteps=25 # steps per seconds
vchannels=1 # vocal channels
# vc_outputs=10-12,22-24 # Voice outputs in acoustics.dat
vc_outputs=6,12 # Voice outputs in acoustics.dat

ext=png
if [ ! -z $2 ]
then
  ext=$2
fi

# Visu flag for pdf rendering
annRender="--ann-render=$ext"

plotoutputs(){
  gnuplot -e "
    set output '$2';
    set term pngcairo size 1680,1050;
    
    f='$1';
    set multiplot layout 4,1;
    set key above;
    set yrange [-1.05:1.05];
    set grid;
    
    set title 'Motors';
    plot for [c in 'ML MR'] f using (column(c)) with lines title c;
    
    set title 'Clock speed';
    plot for [c in 'CS'] f using (column(c)) with lines title c;
    
    set title 'Voice';
    plot for [c in 'VV VC'] f using (column(c)) with lines title c;
    
    set title 'Arms';
    plot for [c in 'A0 A1 A2 A3'] f using (column(c)) with lines title c;
    
    unset multiplot;" #|| rm -fv $outputsoverview
}

plotmodules(){
  if [ -z $3 ]
  then
    echo "No neural flags provided"
    exit 3
  fi
  
  line =
  echo $1
  head -n 1 $1
  line =
  cols=$(awk -vflags="$3" 'NR==1 {
      for(i=2; i<=NF; i++) {
        if (substr($i, length($i), 1) == "M") {
          f=substr($i, 1, length($i)-1);
          if (f == 0)
            name = "Neutral";
          else {
            name = "";
            for (j=0; j<length(flags); j++)
              if (and(f, 2^j))
                name = name substr(flags, length(flags)-j, 1);
          }
          print i, f, name;
        }
      }
    }' $1 | sort -k2,2g | cut -d ' ' -f1,3)
  gnuplot -e "
    set output '$2';
    set term pngcairo size 1680,1050;
    
    f='$1';
    set key above;
    set yrange [-.05:1.05];
    set grid;
    
    cols='$cols';
    plot for [i=1:words(cols):2] '< tail -n +2 '.f using 0:(column(word(cols, i)+0)) with lines title word(cols, i+1);
    
    set output '$(dirname $2)/$(basename $2 .png).pdf';
    set term pdfcairo size 6,2;
    replot;
    ";
}

##############################################################################
# CPPN
line
echo "Extracting CPPN"
cppn=$(dirname $1)"/cppn.png"
if [ -f "$cppn" ]
then
  echo "CPPN '$cppn' already generated. Skipping"
else
  $(dirname $0)/evaluate.sh --gui $ind --overwrite i \
    --scenario "d" --data-folder $sfolder --cppn-render="$ext"
fi

################################################################################
# Generic output
dfolder=$indfolder/baseline
line 
line
echo "Plotting baseline behavior in '$dfolder'"
line

for s in l f r
do  
  sfolder="$dfolder/d_$s"
  line '='
  echo "> scenario '$s'"
    
  ##############################################################################
  # Final position/health
  trajectory="$sfolder/ptrajectory.png"
  if [ -f "$trajectory" ]
  then
    echo "Trajectory overview '$trajectory' already generated. Skipping"
  else
    drawVision=0 $(dirname $0)/evaluate.sh --gui $ind --trace 10 --overwrite i \
      --scenario "d_$s" --data-folder $sfolder --no-restore --tags
  fi
  
  ##############################################################################
  # Communication (rough conspecific interaction)
  soundoverview=$sfolder/sounds.png
  if [ -f $soundoverview ]
  then
    echo "Sound overview '$soundoverview' already generated. Skipping"
  else
    data=$sfolder/.acoustics.midpoints.dat
    cut -d ' ' -f $vc_outputs $sfolder/acoustics.dat \
    | awk -vsteps=$ssteps -vchannels=$vchannels '
      NR==1{ 
        print "T", $0;
        printf "%g", 0;
        for (i=1; i<=2*channels; i++)
          printf " 0";
        printf "\n";        
      }
      NR>2{
        printf "%g", (NR-2)/steps;
        for (i=1; i<=2*channels; i++)
          printf " %g", $i < prev[i] ? $i : prev[i];
        printf "\n";
      }
      NR>1 {
        print (NR-1.5)/steps, $0;
        for (i=1; i<=2*channels; i++) prev[i] = $i;
      }' > $data
    last=$(tail -n 1 $data | cut -d ' ' -f1)
  
    gnuplot -e "
      set output '$soundoverview';
      set term pngcairo size 1680,1050;
      
      set multiplot layout 2,1 margins .03, .98, .06, .97 spacing 0,0;
      set xrange [0:10];
      set style fill solid .5 noborder;
      
      set object rect from $last,1 to 10, -1 fc rgb 'gray' fs solid .25 noborder back;
      
      set key above left;
      set tics front;
      set xtics format '';
      set yrange [0:1];
      set ytics .2,.2,.8;
      set y2label 'Emitter';
      plot for [j=0:$vchannels-1] '$data' using 1:2+j:(0) with filledcurves ls j+2 title columnhead;

      set key above right;
      set xtics;
      set yrange [-1:0];
      set ytics ('0.2' -.2, '0.4' -.4, '0.6' -.6, '0.8' -.8);
      set xlabel 'Time (s)';
      set y2label 'Receiver';
      plot for [j=0:$vchannels-1] '$data' using 1:(-column(2+$vchannels+j)):(0) with filledcurves ls j+2 title columnhead(2+$vchannels+j);
        
      unset multiplot;
    " || rm -fv $soundoverview
    echo "Generated Sound overview '$soundoverview'"
    
#     rm $data
  fi
#   
#   for f in $dfolder/outputs*.dat
#   do
#     o=$(dirname $f)/$(basename $f .dat).png
#     if [ -f "$o" ]
#     then
#       echo "Output overview '$o' already generated. Skipping"
#     else
#       plotoutputs $f $o
#       echo "Generated outputs overview '$o'"
#     fi
#   done
done

aggregate=$dfolder/summary.png
if [ -f "$aggregate" ]
then
  echo "Behavorial aggregate '$aggregate' already generated. Skipping"
else
#   set -x
  labels=$(jq -r '.stats | to_entries[] | "\(.key): \(.value)"' $ind \
    | sed -e 's/d_l/0 &/' -e 's/d_f/1 &/' -e 's/d_r/2 &/' | sort \
    | sed -n 's/lg_[0-2] d/label:d/p' | tr -d ' ')
  stats=$(jq -r '.stats | to_entries[] | "\(.key): \(.value)"' $ind \
    | grep -v "lg_" | awk '{ printf "%s %g\n", $1, $2 }' | column -t)
#   montage -geometry '640x>+0+0' $dfolder/d_{l,f,r}/ptrajectory.png $dfolder/d_{l,f,r}/sounds.png \
#     $aggregate
  convert -family Courier \
          \( -font Courier -pointsize 24 label:"$ind" +append \) \
          \( -font Courier -pointsize 18 label:"------\n$stats\n" +append \) \
          \( $dfolder/d_{l,f,r}/ptrajectory.png -resize 640x +append \) \
          \( $dfolder/d_{l,f,r}/sounds.png -resize 640x +append \) \
          \( $labels -gravity center -extent 640x -font Courier  +append \) \
          -append $aggregate
       
  echo "Generated behavorial aggregate '$aggregate'"
fi

aggregate=$dfolder/phonems.png
if [ -f "$aggregate" ]
then
  echo "Phonems aggregate '$aggregate' already (naively) generated. Skipping"
else
  gnuplot -e "
      set term pngcairo size 1680,1050;
      set output '$aggregate';
      
      set style fill solid .5;
      set multiplot layout 3,2 margins .05,.95, .05,.95 spacing .05,0;
      set xrange [0:10];
      set yrange [0:1.05];
      
      i=0;
      set key above;
      set xtics format '';
      do for [s in 'l f r'] {
        if (i==2) { set xtics format; };
        if (i!=0) { unset key; };
        i=i+1;
        
        folder='$dfolder/d_'.s.'/';
        set yrange [0:1]; set ytics (.2, .4, .6, .8) rangelimited;
        plot for [j=0:$vchannels-1] folder.'acoustics.dat' using (\$0/$ssteps):6+j with boxes title columnhead;
        set yrange [-1:1]; set ytics (-.5, 0, .5) rangelimited; set y2label s;
        plot for [j=0:1] folder.'outputs_r.dat' using (\$0/$ssteps):1+j with lines title columnhead;
      };
      
      unset multiplot;
      
      print('Generated $aggregate');
    " || rm -rfv $aggregate
fi

# ################################################################################
# # Communication overview
# 
# soundoverview=$indfolder/sounds.png
# if [ -f "$soundoverview" ]
# then
#   echo "Sound overview '$soundoverview' already generated. Skipping"
# else
#   soundupper=$indfolder/sounds_mk.png
#   gnuplot -e "
#     n=$teamsize;
#     files=system('ls $indfolder/*/acoustics.dat');
#         
#     set output '$soundupper';
#     set term pngcairo size 1280, 800;
# 
#     set multiplot layout words(files), n;
#     set key above;
#     set style fill solid .75;
#     
#     set xrange [0:500]; set yrange [0:1];
#     do for [i=1:n] {
#       do for [f in files] {
#         plot for [j=10:12] f using 0:12*(i-1)+j:(0) with filledcurves title columnhead;
#       };
#     };
#     
#     unset multiplot;
#   " #|| rm -v "$soundoverview"
#   
#   soundlower=$indfolder/sounds_eval.png
#   gnuplot -e "
#     set output '$soundlower';
#     set term pngcairo size 1280, 800;
#     
#     files=system('ls $indfolder/eval_first_pass/e1/*/acoustics.dat');
#     set multiplot layout words(files), 1;
#     set xrange [0:600]; set yrange [0:1];
#     set style fill solid .25;
#     set key above;
#     
#     do for [f in files] {
#       plot for [i=10:12] f using 0:i:(0) with filledcurves title columnhead;
#     };
#     
#     unset multiplot;
#   "
#   
#   montage -tile x2 -geometry +0+0 $indfolder/sounds_{mk,eval}.png $soundoverview
#   echo "Generated $soundoverview"
# fi
# 
# 
# ################################################################################
# # Neural clustering based on reproductible conditions
# 
# echo "Plotting neural clustering (first pass)"
# 
# ffpass(){
#   echo "$indfolder/eval_$1_pass/$2"
# }
# firstopp=$(head -n 1 <<< "$opponents")
# 
# # Rendering non-aggregated ann
# # for e in $pfolder/*/
# # do
# #   aggregate="$e/mann.$ext"
# #   if [ -f "$aggregate" ]
# #   then
# #     echo "ANN coloring '$aggregate' already generated. Skipping"
# #   else
# #     CMD=yes $sfolder/evaluate.sh --gui $ind $firstopp --overwrite i \
# #       --ann-render=$ext --ann-aggregate=-1 --ann-tags=$e/neurons_groups.ntags 
# #     echo "Generated $aggregate"
# #   fi
# # done
# 
# for e in e1 e2 e3
# do
#   pfolder=$(ffpass "first" $e)
#   
#   for f in $pfolder/*/outputs*.dat
#   do
#     o=$(dirname $f)/$(basename $f .dat).png
#     if [ -f "$o" ]
#     then
#       echo "Output overview '$o' already generated. Skipping"
#     else
#       plotoutputs $f $o
#       echo "Generated outputs overview '$o'"
#     fi
#   done
# 
#   # Rendering aggregated clustering
#   aggregate="$pfolder/mann.$ext"
#   if [ -f "$aggregate" ]
#   then
#     echo "ANN clustering '$aggregate' already generated. Skipping"
#   else
#     echo "Generating ANN clustering: $aggregate"
#     
#     $sfolder/evaluate.sh --gui $ind $firstopp --overwrite i \
#       --ann-render=$ext --ann-aggregate --ann-tags=$pfolder/neural_groups.ntags
#     
#     echo "Generated $aggregate"
#   fi
# done
# 
# # Render meta-modules
# pfolder=$(ffpass "first" "")
# for depth in false
# do
#   for symmetry in false
#   do
#     suffix=""
#     [ $depth == "true" ] && suffix="${suffix}_depth"
#     [ $symmetry == "true" ] && suffix="${suffix}_symmetry"
#     aggregate="$pfolder/mann$suffix.$ext"
#     if [ -f "$aggregate" ]
#     then
#       echo "ANN clustering '$aggregate' already generated. Skipping"
#     else
#       echo "Generating ANN clustering: $aggregate"
#       
#       mannWithDepth=$depth mannWithSymmetry=$symmetry CMD=yes $sfolder/evaluate.sh --gui $ind $firstopp --overwrite i \
#         --ann-render=$ext --ann-aggregate --ann-tags=$pfolder/neural_groups.ntags
#         
#       [ ! -z "$suffix" ] && mv -v $pfolder/mann.$ext $aggregate
#       
#       echo "Generated $aggregate"
#     fi
#   done
# done
# 
# declare -A flags=([e1]="THI" [e2]="CBA" [e3]="OFN")
# declare -p flags
# for e in e1 e2 e3
# do
#   pfolder=$(ffpass "second" $e)
#   flag="${flags[$e]}"
#     
#   for f in $pfolder/*/outputs*.dat
#   do
#     o=$(dirname $f)/$(basename $f .dat).png
#     if [ -f "$o" ]
#     then
#       echo "Output overview '$o' already generated. Skipping"
#     else
#       plotoutputs $f $o
#       echo "Generated outputs overview '$o'"
#     fi
#   done
# 
#   for f in $pfolder/*/modules*.dat
#   do
#     o=$(dirname $f)/$(basename $f .dat).png
#     if [ -f "$o" ]
#     then
#       echo "Modules overview '$o' already generated. Skipping"
#     else
#       plotmodules $f $o $flag
#       echo "Generated modules overview '$o'"
#     fi
#   done
# done
# 
# pfolder=$(ffpass "third" "")
# flag="AVT"
#     
# for f in $pfolder/*/outputs*.dat
# do
#   o=$(dirname $f)/$(basename $f .dat).png
#   if [ -f "$o" ]
#   then
#     echo "Output overview '$o' already generated. Skipping"
#   else
#     plotoutputs $f $o
#     echo "Generated outputs overview '$o'"
#   fi
# done
# 
# for f in $pfolder/*/modules*.dat
# do
#   o=$(dirname $f)/$(basename $f .dat).png
#   if [ -f "$o" ]
#   then
#     echo "Modules overview '$o' already generated. Skipping"
#   else
#     plotmodules $f $o $flag
#     echo "Generated modules overview '$o'"
#   fi
# done
