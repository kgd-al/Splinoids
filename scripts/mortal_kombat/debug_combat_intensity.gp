#!/usr/bin/gnuplot

folder=ARG1
term=ARG2

set key above;
set grid;

if (term) {
  set term wxt;
  unset output;

} else {
  set term pngcairo size 1280,900;
  set output folder.'/cbi_debug.png';
};

set multiplot layout 4,1;

set yrange [-.05:*];
do for [i=3:4] {
  set y2label system('head -n1 '.folder.'/lhs.dat | cut -d " " -f'.i);
  plot folder.'/lhs.dat' using 1:i w l t 'lhs', \
       folder.'/rhs.dat' using 1:i w l t 'rhs';
        
  unset key;
}

set yrange [-.05:1.05];
do for [i=5:6] {
  set y2label system('head -n1 '.folder.'/lhs.dat | cut -d " " -f'.i);
  plot folder.'/lhs.dat' using 1:i w l, \
       folder.'/rhs.dat' using 1:i w l;
}

unset multiplot;
