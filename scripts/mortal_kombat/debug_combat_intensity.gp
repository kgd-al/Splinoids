#!/usr/bin/gnuplot

set key above;
set grid;
# while (1) {
#   last=system('ls -td tmp/mk-eval/*/ | head -n1');
#   system('printf "%s\r" "'.last.'"');

do for [eval in system('ls -d tmp/mk-eval/*__*/')] {
  print eval;

#   do for [TERM=0:1] {
    TERM=0;
    if (TERM) {
      set term wxt;
      unset output;
  
    } else {
      set term pngcairo size 1280,900;
      set output eval.'/debug.png';
    };

    set multiplot layout 4,1;

    set yrange [-.05:*];
    do for [i=3:4] {
      set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
      plot eval.'/lhs.dat' using 1:i w l t 'lhs', \
           eval.'/rhs.dat' using 1:i w l t 'rhs';
           
      unset key;
    }

    set yrange [-.05:1.05];
    do for [i=5:6] {
      set y2label system('head -n1 '.eval.'/lhs.dat | cut -d " " -f'.i);
      plot eval.'/lhs.dat' using 1:i w l, \
           eval.'/rhs.dat' using 1:i w l;
    }

    unset multiplot;
#   }

#   pause 4;
};
