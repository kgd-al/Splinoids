#!/bin/bash

if [ ! -d "$1" ]
then
  echo "Please provide a valid folder"
  exit 1
fi

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

background(){
  echo "do for [i=0:$2] { set object rectangle from first (2*i+1+$3)*100, graph 0 to first (2*i+2+$3)*100, graph 1 fs solid .1 noborder fc '$1' behind; };"
}

# Modules activity

folder=$1
for st in $folder/E[BAE]/
do
  mtmp=$st/modules.mtmp
  $(dirname $0)/reorder_and_filter_columns.awk -vcolumns="0M 1536M 512M 1024M" $st/modules.dat > $mtmp
done

cmd="
  set term pdfcairo crop size 6,3.5 font 'Monospace,24';
  set output '$folder/m_summary.pdf';
  
  set multiplot;
  h0=.11;
  h=.27;
  set size 1,h;
  set lmargin 5;
  set rmargin 3;
  set bmargin 0;
  set tmargin 0;
  
  set xrange [0:600];
  unset xtics;
  
  set grid;
  set yrange [-.05:1.1];
  
  unset key;
    
  do for [i in 'A E B'] {
    print('## Plotting modules ################################################');
    if (i eq 'A') {
      $(background blue 2 0);
      set y2label 'Ally';
      set origin 0, h0+2*h;
      
    } else { if (i eq 'E') {
      $(background red 2 0);
      set y2label 'Enemy';
      set origin 0, h0+h;
      
    } else {
      $(background red 2 0);
      $(background blue 1 1);
      set origin 0, h0;
      set y2label 'Both';
      set xtics;
      set xlabel 'Steps';
      set key title 'Modules' horizontal at graph 0.5, screen 1 center top width 0;
    }};
    
    f='$folder/E'.i.'/modules.mtmp';
    headerData='$(extractTitles $mtmp)';
    title(i)=word(headerData, 2*i);
    color(i)=word('#000000 #0000FF #FF0000 #7F007F', word(headerData, 2*i-1)+1);
      
    cols=words(headerData)/2;
    plot for [j=1:cols] '< tail -n +2 '.f using 0:j with lines lc rgb color(j) title ''.title(j);  
  };
  unset multiplot;
"
# printf "%s\n" "$cmd"
gnuplot -e "$cmd" || exit 3
rm -v $folder/*/modules.mtmp
# exit

# Vocalisation/modules summary

# Histogram version (overlaps poorly)

#     set size .975,.475;
#     set origin .025,.525;
#     set xrange [0:600];
#     set yrange [-.05:1.05];
#     set ylabel 'Volume' offset char 1,0;
#     set ytics norotate 0,.5,1;
#     set style fill transparent solid 0.4 noborder;
#     
#     color(i)=word('#007F00 #0000FF', i);
#     plot for [i=2:1:-1] '$vtmp' using 0:i with boxes lc rgb color(i) title columnhead;

# Independent axis
#     set size .975,.475;
#     set origin .025,.525;
#     
#     set autoscale fix;
#     unset xtics;
#     unset colorbox;
#     set palette gray;
#     set ytics ('S' 0, 'A' 1);
#     set ylabel 'Agent';
#     plot '< tail -n +2 $vtmp' matrix using 2:1:3 with image title 'Vocalisation';

keysfile="$folder/cm_keys.pdf"
ctitles='Subject Ally';
ccolors="#32CD32 #5F5FFF"
mcolors="#000000 #0000FF #FF0000 #7F007F"
for st in $folder/EB*/
do
  vocalisations=$st/vocalisation.dat
  if [ ! -f "$vocalisations" ]
  then
    echo "Please provide a valid evaluation folder ('$vocalisations' were not found)"
    exit 2
  fi

  modules=$st/modules.dat
  if [ ! -f "$modules" ]
  then
    echo "Please provide a valid evaluation folder ('$modules' were not found)"
    exit 2
  fi

  vtmp=.vtmp
  awk 'NR>1{
    for (j=0; j<2; j++) {
      v = 0
      for (i=2+3*j; i<2+3*(j+1); i++) if (v < $i) v = $i;
      if (v == "nan") v = 0;
      data[j,NR] = v;
    }
  } END {
    print "Subject", "Ally";
    for (r=2; r<=NR; r++) print data[0,r], data[1,r];
  }' $vocalisations > $vtmp

#   0M 1536M 512M 1024M
  mtmp=.mtmp
  $(dirname $0)/reorder_and_filter_columns.awk -vcolumns="512M 1024M" $modules > $mtmp

  cmd="
    set term pdfcairo crop size 6,3.5 font 'Monospace,20';
    set output '$st/cm_summary.pdf';
    
    set multiplot;
    set lmargin 5;
    set rmargin 0.1;
    set tmargin .5;
    unset key;
        
    set xtics ();
    do for [i=0:600:100] { set xtics add ('' i); };
    
    print('## Processing $st');
    print('## Plotting vocalisation ###########################################');  
    
    h=.25;
    set size .975,h;
    
    x=.025;
    y=2*h;

    set bmargin 1;
    
    set autoscale fix;
    set yrange [0:1];
    unset xlabel;
    set ytics (0, '' .5, 1);
    unset ylabel;
    set style fill solid;
    set grid front;
    
    do for [i=1:2] { 
      if (i == 2) { set ylabel 'Volume' offset graph 0,-.75; };
      set origin x,y;
      plot '$vtmp' using 0:i with boxes lc rgb word('$ccolors', i);
      y = y+h;
    };

    print('## Plotting modules ################################################');

    set size .975,2*h;
    set origin x,0;
    
    unset bmargin;

    headerData='$(extractTitles $mtmp)';
    title(i)=word(headerData, 2*i);
    color(i)=word('$mcolors', word(headerData, 2*i-1)+1);
    set xlabel 'Phases';
    set ytics ('0' 0, '.5' .5, '1' 1);
    set ylabel 'Activity' offset 0,0;
    set grid back;
          
    set xrange [0:600];
    set yrange [0:1];
    
    cols=words(headerData)/2;
    plot for [j=1:cols] '< tail -n +2 $mtmp' using 0:j with lines lc rgb color(j) title ''.title(j); 
    
    unset grid;
    set ylabel ' ';
    set xlabel ' ';
    unset ytics;
    set xtics ('0' 50) scale 0;
    plot NaN;
    
    set xtics () textcolor 'red';
    set for [i=150:550:200] xtics add (''.(i/100) i) scale 0;
    plot NaN;
    
    set xtics () textcolor 'blue';
    set for [i=250:550:200] xtics add (''.(i/100) i) scale 0;
    plot NaN;
    
    unset multiplot;
    file_exists(file) = system('[ -f '.file.' ] && echo 1 || echo 0') + 0;
    if (!file_exists('$keysfile')) {
      print('## Generating keys #####');
      set term pdfcairo crop size 6.7,.45 font 'Monospace,20';
      set output '$keysfile';
      set multiplot;
      unset tics;
      set noborder;
      set tmargin 0;
      set lmargin 0;
      set rmargin 0;
      set bmargin 0;
      unset xlabel;
      unset ylabel;
      fmttitle(s)=sprintf('%8s' , s);
      set key at screen .5,1 box horizontal title 'Communication';
      plot for [i=1:2] NaN with boxes lc rgb word('$ccolors', i) \
                           title fmttitle(word('$ctitles', i));
      set key at screen 1,1 title 'Modules';
      plot for [j=1:cols] NaN with lines lc rgb color(j) \
                              title fmttitle(title(j));
      unset multiplot;
    };
  "
#   printf "%s\n" "$cmd"
  gnuplot -e "$cmd" || exit 3
#   rm -v $vtmp $mtmp
done

echo
