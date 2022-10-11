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
set autoscale xfix;

if (term) {
  set term wxt;
  unset output;

} else {
  output=folder.'/evo.png';
  set term pngcairo size 1280,900 noenhanced;
  set output output;
}

id=system('readlink -m '.folder);
id=system('basename '.id);
set multiplot layout 3, 1 title id.' @ '.system('date') \
  margins char 5, 8, 2, 3 spacing char 12, 3;
set key noenhanced horizontal center at graph .5, 1 bottom;
set format y2 "%.2h"

columnid(name)=system('awk -F, -vn='.name.' "NR==1{ c=0; for (i=1; i<=NF; i++) if (\$i == n) c=i; print c; }" '.folder.'/gen_stats.csv')+0;

lg_i=columnid('lg_avg');
set yrange [-1.1:2.1];
unset y2tics;
p0=0; p1=0; shift(x) = (p1 = p0, p0 = x);
plot for [i=0:2] folder.'/gen_stats.csv' using 1:lg_i+i w l t columnhead, \
     '' using 1:(shift(column(lg_i+1)), p1 < p0 ? p0 : 1/0) pt 7 ps .5 t 'New';
unset yrange;

nrn_i=columnid('neurons_avg'); cnx_i=columnid('cxts_avg');
set ytics nomirror; set yrange [.5:*];
set y2tics; set y2range [.5:*];
set logscale yy2 8;
set arrow from graph 0, first 8 to graph 1, first 8 lc "gray" nohead;
system('ls -v '.folder.'/gen*/lg*.dna | xargs jq -r ".stats | \"\(.neurons),\(.cxts)\"" > '.folder.'/champ_stats.csv');
plot folder.'/gen_stats.csv' using 1:nrn_i+1:nrn_i+2 w filledcurves lc 2 fs transparent solid .25 notitle, \
                          '' using 1:nrn_i w l dt 3 t columnhead, \
                          '' using 1:cnx_i+1:cnx_i+2 w filledcurves axes x1y2 lc 4 fs transparent solid .25 notitle, \
                          '' using 1:cnx_i w l axes x1y2 lc 4 dt 3 t columnhead, \
     folder.'/champ_stats.csv' using 1 w l lc 2 title 'cn', \
                            '' using 2 w l axes x1y2 lc 4 title 'cc';
unset logscale;
unset arrow;

time_i=columnid('global_genTotalTime');
stats=''.columnid('brain_avg').' '.columnid('mute_avg');
set yrange [-.05:1.05];
set format y2;
plot for [i in stats] folder.'/gen_stats.csv' using int(i) w l t columnhead, \
                                           '' using 1:time_i w l axes x1y2 lc 7 t columnhead;
unset yrange;

unset multiplot;

if (term == 0) {
  print('Generated '.output);
}
