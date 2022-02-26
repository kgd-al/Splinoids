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
pops=system('ls -d '.folder.'/[A-Z] | awk -F/ "{print \$NF}" | paste -s -d " "');
set multiplot layout 3, words(pops) title id.' @ '.system('date') \
  margins char 5, 8, 2, 3 spacing char 12, 3;
set key noenhanced horizontal center at graph .5, 1 bottom;
set format y2 "%.2h"

columnid(name)=system('awk -F, -vn='.name.' "NR==1{ c=0; for (i=1; i<=NF; i++) if (\$i == n) c=i; print c; }" '.folder.'/'.word(pops, 1).'/gen_stats.csv')+0;

mk_i=columnid('mk_avg');
set yrange [-2.05:2.05];
unset y2tics;
do for [i=-2:2:2] { set arrow from graph 0, first i to graph 1, first i lc "gray" nohead; }
do for [t in pops] {
  plot folder.'/'.t.'/gen_stats.csv' using 1:mk_i+1:mk_i+2 w filledcurves lc -1 fs transparent solid .25 notitle, \
                                  '' using 1:mk_i w l t columnhead, \
                      for [i=1:2] '' using 1:mk_i+i w l lc -1 notitle;
}
unset arrow;
unset yrange;

nrn_i=columnid('neurons_avg'); cnx_i=columnid('cxts_avg');
set ytics nomirror;
set y2tics;
set arrow from graph 0, first 8 to graph 1, first 8 lc "gray" nohead;
do for [t in pops] {
  base=folder.'/'.t;
  system('ls -v '.base.'/gen*/mk*.dna | xargs jq -r ".stats | \"\(.neurons),\(.cxts)\"" > '.base.'/champ_stats.csv');
  plot base.'/gen_stats.csv' using 1:nrn_i+1:nrn_i+2 w filledcurves lc 2 fs transparent solid .25 notitle, \
                          '' using 1:nrn_i w l dt 3 t columnhead, \
              for [i=1:2] '' using 1:nrn_i+i w l lc 2 dt 2 notitle, \
                          '' using 1:cnx_i+1:cnx_i+2 w filledcurves axes x1y2 lc 4 fs transparent solid .25 notitle, \
                          '' using 1:cnx_i w l axes x1y2 lc 4 dt 3 t columnhead, \
              for [i=1:2] '' using 1:cnx_i+i w l axes x1y2 lc 4 dt 2 notitle, \
     base.'/champ_stats.csv' using 1 w l lc 2 title 'cn', \
                          '' using 2 w l axes x1y2 lc 4 title 'cc';
}
unset arrow;

time_i=columnid('global_genTotalTime');
set yrange [-.05:1.05];
do for [t in pops] {
  plot for [i in "2"] folder.'/'.t.'/gen_stats.csv' using int(i) w l t columnhead, \
                                                 '' using 1:time_i w l axes x1y2 lc 7 t columnhead;
}
unset yrange;

unset multiplot;
