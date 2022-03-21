#!/bin/bash

if [ ! -d "$1" ]
then
  echo "Please provide a genome data folder"
  exit 1
fi

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
opponents=$(fetchopponents)
# printf "opponents: %s\n" $opponents
indfolder=$(dirname $ind)/$(basename $ind .dna)
shift

sfolder=$(dirname $0) # script folder

bins=10
bwidth=$(awk -vbins=$bins 'BEGIN{print 2/bins;}')

ext=png
if [ ! -z $2 ]
then
  ext=$2
fi

# Visu flag for pdf rendering
annRender="--ann-render=$ext"

teamsize=$(jq '.dna | fromjson | .[0]' $ind)

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

################################################################################
# Generic output
for opp in $opponents
do  
  dfolder=$(datafolder $ind $opp)
  
  ##############################################################################
  # Final position/health
  aggregate="$dfolder/finish_health.png"
  if [ ! -f "$aggregate" ]
  then
    $(dirname $0)/evaluate.sh --gui $ind $opp --trace 0 --overwrite i
    mv $dfolder/ptrajectory.png $dfolder/finish.png
    renderType=Health $(dirname $0)/evaluate.sh --gui $ind $opp --trace 0 --overwrite i
    mv $dfolder/ptrajectory.png $dfolder/finish_health.png
  fi
  
  ##############################################################################
  # Health dynamics (rough strategy overview)
  healthoverview=$dfolder/health.png
  if [ -f $healthoverview ]
  then
    echo "Health overview '$healthoverview' already generated. Skipping"
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
    echo "Generated Health overview '$healthoverview'"
  fi
  
  ##############################################################################
  # Communication (rough conspecific interaction)
  soundoverview=$dfolder/sounds.png
  if [ -f $soundoverview ]
  then
    echo "Sound overview '$soundoverview' already generated. Skipping"
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
    echo "Generated Sound overview '$soundoverview'"
  fi
  
   
  for f in $dfolder/outputs*.dat
  do
    o=$(dirname $f)/$(basename $f .dat).png
    if [ -f "$o" ]
    then
      echo "Output overview '$o' already generated. Skipping"
    else
      plotoutputs $f $o
      echo "Generated outputs overview '$o'"
    fi
  done
done

################################################################################
# Communication overview

soundoverview=$indfolder/sounds.png
if [ -f "$soundoverview" ]
then
  echo "Sound overview '$soundoverview' already generated. Skipping"
else
  soundupper=$indfolder/sounds_mk.png
  gnuplot -e "
    n=$teamsize;
    files=system('ls $indfolder/*/acoustics.dat');
        
    set output '$soundupper';
    set term pngcairo size 1280, 800;

    set multiplot layout words(files), n;
    set key above;
    set style fill solid .75;
    
    set xrange [0:500]; set yrange [0:1];
    do for [i=1:n] {
      do for [f in files] {
        plot for [j=10:12] f using 0:12*(i-1)+j:(0) with filledcurves title columnhead;
      };
    };
    
    unset multiplot;
  " #|| rm -v "$soundoverview"
  
  soundlower=$indfolder/sounds_eval.png
  gnuplot -e "
    set output '$soundlower';
    set term pngcairo size 1280, 800;
    
    files=system('ls $indfolder/eval_first_pass/e1/*/acoustics.dat');
    set multiplot layout words(files), 1;
    set xrange [0:600]; set yrange [0:1];
    set style fill solid .25;
    set key above;
    
    do for [f in files] {
      plot for [i=10:12] f using 0:i:(0) with filledcurves title columnhead;
    };
    
    unset multiplot;
  "
  
  montage -tile x2 -geometry +0+0 $indfolder/sounds_{mk,eval}.png $soundoverview
  echo "Generated $soundoverview"
fi


################################################################################
# Neural clustering based on reproductible conditions

echo "Plotting neural clustering (first pass)"

ffpass(){
  echo "$indfolder/eval_$1_pass/$2"
}
firstopp=$(head -n 1 <<< "$opponents")

# Rendering non-aggregated ann
# for e in $pfolder/*/
# do
#   aggregate="$e/mann.$ext"
#   if [ -f "$aggregate" ]
#   then
#     echo "ANN coloring '$aggregate' already generated. Skipping"
#   else
#     CMD=yes $sfolder/evaluate.sh --gui $ind $firstopp --overwrite i \
#       --ann-render=$ext --ann-aggregate=-1 --ann-tags=$e/neurons_groups.ntags 
#     echo "Generated $aggregate"
#   fi
# done

for e in e1 e2 e3
do
  pfolder=$(ffpass "first" $e)
  
  for f in $pfolder/*/outputs*.dat
  do
    o=$(dirname $f)/$(basename $f .dat).png
    if [ -f "$o" ]
    then
      echo "Output overview '$o' already generated. Skipping"
    else
      plotoutputs $f $o
      echo "Generated outputs overview '$o'"
    fi
  done

  # Rendering aggregated clustering
  aggregate="$pfolder/mann.$ext"
  if [ -f "$aggregate" ]
  then
    echo "ANN clustering '$aggregate' already generated. Skipping"
  else
    echo "Generating ANN clustering: $aggregate"
    
    $sfolder/evaluate.sh --gui $ind $firstopp --overwrite i \
      --ann-render=$ext --ann-aggregate --ann-tags=$pfolder/neural_groups.ntags
    
    echo "Generated $aggregate"
  fi
done

# Render meta-modules
pfolder=$(ffpass "first" "")
for depth in false
do
  for symmetry in false
  do
    suffix=""
    [ $depth == "true" ] && suffix="${suffix}_depth"
    [ $symmetry == "true" ] && suffix="${suffix}_symmetry"
    aggregate="$pfolder/mann$suffix.$ext"
    if [ -f "$aggregate" ]
    then
      echo "ANN clustering '$aggregate' already generated. Skipping"
    else
      echo "Generating ANN clustering: $aggregate"
      
      mannWithDepth=$depth mannWithSymmetry=$symmetry CMD=yes $sfolder/evaluate.sh --gui $ind $firstopp --overwrite i \
        --ann-render=$ext --ann-aggregate --ann-tags=$pfolder/neural_groups.ntags
        
      [ ! -z "$suffix" ] && mv -v $pfolder/mann.$ext $aggregate
      
      echo "Generated $aggregate"
    fi
  done
done

declare -A flags=([e1]="THI" [e2]="CBA" [e3]="OFN")
declare -p flags
for e in e1 e2 e3
do
  pfolder=$(ffpass "second" $e)
  flag="${flags[$e]}"
    
  for f in $pfolder/*/outputs*.dat
  do
    o=$(dirname $f)/$(basename $f .dat).png
    if [ -f "$o" ]
    then
      echo "Output overview '$o' already generated. Skipping"
    else
      plotoutputs $f $o
      echo "Generated outputs overview '$o'"
    fi
  done

  for f in $pfolder/*/modules*.dat
  do
    o=$(dirname $f)/$(basename $f .dat).png
    if [ -f "$o" ]
    then
      echo "Modules overview '$o' already generated. Skipping"
    else
      plotmodules $f $o $flag
      echo "Generated modules overview '$o'"
    fi
  done
done

pfolder=$(ffpass "third" "")
flag="AVT"
    
for f in $pfolder/*/outputs*.dat
do
  o=$(dirname $f)/$(basename $f .dat).png
  if [ -f "$o" ]
  then
    echo "Output overview '$o' already generated. Skipping"
  else
    plotoutputs $f $o
    echo "Generated outputs overview '$o'"
  fi
done

for f in $pfolder/*/modules*.dat
do
  o=$(dirname $f)/$(basename $f .dat).png
  if [ -f "$o" ]
  then
    echo "Modules overview '$o' already generated. Skipping"
  else
    plotmodules $f $o $flag
    echo "Generated modules overview '$o'"
  fi
done
