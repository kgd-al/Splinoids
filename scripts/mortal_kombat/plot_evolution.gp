#!/usr/bin/gnuplot

folder=ARG1
term=ARG2

if (strlen(folder) == 0) {
  print "ERROR: No data folder provided!";
  exit 2;
}

if (system('[ -d "'.folder.'" ]; echo $?;') != 0) {
  print "ERROR: Provided data folder is not a valid folder!";
  exit 2;
}

if (strlen(term) == 0) {
  term = 0;
}

# print "folder: ", folder;
# print "  term: ", term;

set datafile separator comma;
set grid;

if (term) {
  set term wxt;
  unset output;

} else {
  set term pngcairo size 1280,900 noenhanced;
  set output folder.'/evo.png';
}

id=system('readlink -m '.folder);
id=system('basename '.id);
set multiplot layout 3,2 title id.' @ '.system('date') \
  margins char 5, 8, 2, 3 spacing char 12, 4;
set key noenhanced horizontal center at graph .5, 1.05;
set format y2 "%.2h"

mk_i=22
set yrange [-2.05:2.05];
unset y2tics;
do for [i=-2:2:2] { set arrow from graph 0, first i to graph 1, first i lc "gray" nohead; }
do for [t in "A B"] {
#       set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
  plot folder.'/'.t.'/gen_stats.csv' using 1:mk_i+1:mk_i+2 w filledcurves lc -1 fs transparent solid .25 notitle, \
                                  '' using 1:mk_i w l t columnhead, \
                      for [i=1:2] '' using 1:mk_i+i w l lc -1 notitle;
}
unset arrow;
unset yrange;

nrn_i=8; cnx_i=5
set ytics nomirror;
set y2tics;
set arrow from graph 0, first 8 to graph 1, first 8 lc "gray" nohead;
do for [t in "A B"] {
#       set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
  plot folder.'/'.t.'/gen_stats.csv' using 1:nrn_i+1:nrn_i+2 w filledcurves lc 2 fs transparent solid .25 notitle, \
                                  '' using 1:nrn_i w l t columnhead, \
                      for [i=1:2] '' using 1:nrn_i+i w l lc 2 dt 2 notitle, \
                                  '' using 1:cnx_i+1:cnx_i+2 w filledcurves axes x1y2 lc 4 fs transparent solid .25 notitle, \
                                  '' using 1:cnx_i w l axes x1y2 lc 4 t columnhead, \
                      for [i=1:2] '' using 1:cnx_i+i w l axes x1y2 lc 4 dt 2 notitle;
}
# 
# nrn_i=5
# time_i=20
# set ytics nomirror;
# set y2tics;
# do for [t in "A B"] {
# #       set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
#   plot folder.'/'.t.'/gen_stats.csv' , \
#                                   '' using 1:time_i axes x1y2 w l lc 7 t columnhead;
# }
unset arrow;

time_i=17
set yrange [-.05:1.05];
do for [t in "A B"] {
#       set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
  plot for [i in "2"] folder.'/'.t.'/gen_stats.csv' using int(i) w l t columnhead, \
                                                 '' using 1:time_i w l axes x1y2 lc 7 t columnhead;
}
unset yrange;

unset multiplot;
