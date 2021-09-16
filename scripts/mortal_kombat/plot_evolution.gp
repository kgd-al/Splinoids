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
set key above;
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
set multiplot layout 2,2 title id.' @ '.system('date');

set arrow 1 from graph 0, first 0 to graph 1, first 0 lc "gray" nohead;
do for [t in "A B"] {
#       set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
  plot folder.'/'.t.'/gen_stats.csv' using 1:11:12 w filledcurves lc -1 fs transparent solid .25 notitle, \
                                  '' using 1:10 w l t 'mk', \
                    for [i=11:12] '' using 1:i w l lc -1 notitle;
}
unset arrow 1;

do for [t in "A B"] {
#       set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
  plot for [i in "2"] folder.'/'.t.'/gen_stats.csv' using int(i) w l t columnhead;
}

unset multiplot;
