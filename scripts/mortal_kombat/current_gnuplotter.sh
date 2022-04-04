#!/bin/bash
pretty_modules_columns_names(){
  awk -vflags="$2" 'NR==1 {
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
    }' $1 | sort -k2,2g | cut -d ' ' -f1,3
}

## e1 (second, flee-er)
# f=../alife2022_results/v1-t1p3/ID797308/B/gen1999/mk__0.582902_0/eval_second_pass/e1/797308-B-1999-0__797308-A-1998-0/
# 
# modules=$f/modules.dat
# cols=$(pretty_modules_columns_names $modules THI | grep -v "I.*T")
# echo $cols
# gnuplot -e "
#   set output '$f/e1_summary.pdf';
#   set term pdfcairo size 6.5,3;
#   
#   set multiplot layout 3,1;
#   set key above right;
#   
#   set ytics rangelimited;
#   
#   unset xtics;
#   set yrange [.45:1.05];
#   do for [x in '246 257 337'] { set arrow from first x, graph 0 to first x, graph 1 nohead lc 'gray'; };
#   cols='$cols';
#   set y2label 'Modules';
#   plot for [i=1:words(cols):2] '< tail -n +2 $modules' using 0:(column(word(cols, i)+0)) with lines lc i+3 title word(cols, i+1);
# 
#   set yrange [-1.05:1.05];
#   set y2label 'Arms';
#   plot for [i=0:3] '$f/outputs.dat' using 0:(column('A'.i)) with lines title 'A'.i;
#   
#   set xtics;
#   set yrange [-.05:1.05];
#   set y2label 'Motors';
#   plot for [i in 'ML MR'] '$f/outputs.dat' using 0:(column(i)) with lines title i;
#   
#   unset multiplot;
# "

## e2 (first, range finder)
# f=../alife2022_results/v1-t2p2/ID798074/B/gen1999/mk__0.73633_0/eval_second_pass/e2/798074-B-1999-0__798074-A-1998-0/
# 
# modules0=$f/modules_0.dat
# modules1=$f/modules_1.dat
# cols0=$(pretty_modules_columns_names $modules0 CBA)
# cols1=$(pretty_modules_columns_names $modules1 CBA)
# gnuplot -e "
#   set output '$f/summary.pdf';
#   set term pdfcairo size 6.5,3;
#   
#   set multiplot layout 2,1 margins .05, .975, .075, .925 spacing 0, 0;
#   set key above;
#   
#   set ytics rangelimited;
#   unset xtics;
#   set yrange [0:1];
#   
#   set arrow from first 100, graph 0 to first 100, graph 1 nohead lc 'gray';
#   set arrow from first 495, graph 0 to first 495, graph 1 nohead lc 'gray';
#   
#   cols='$cols0';
#   set y2label 'Teammate 0';
#   plot for [i=1:words(cols):2] '<tail -n +2 $modules0' using 0:(column(word(cols, i)+0)) with lines title word(cols, i+1);
#   
#   unset key;
#   set xtics;
#   cols='$cols1';
#   set y2label 'Teammate 1';
#   plot for [i=1:words(cols):2] '<tail -n +2 $modules1' using 0:(column(word(cols, i)+0)) with lines title word(cols, i+1);
#   
#   unset multiplot;
# "

# # e3 (shirper)
# f=../alife2022_results/v1-t2p2/ID798167/B/gen1999/mk__0.317601_0/eval_second_pass/e3/798167-B-1999-0__798167-A-1998-0/
# for f in $f tmp/re-eval-8167B/mute*;
# do
# 
# modules0=$f/modules_0.dat
# modules1=$f/modules_1.dat
# # O F OF N ON FN OFN
# if false
# then
#   exclude="-v"
#   excluded="\b\(Neutral\|O\|F\|OF\|N\|ON\)\b"
# else
#   excluded="\b\(FN\|OFN\)\b"
# fi
# cols0=$(pretty_modules_columns_names $modules0 NFO | grep $exclude "$excluded" | tr '\n' ' ')
# cols1=$(pretty_modules_columns_names $modules1 NFO | grep $exclude "$excluded" | tr '\n' ' ')
# echo "'$cols0' '$cols1'"
# gnuplot -c /dev/stdin <<__DOC
#   o = '$f/summary.pdf';
#   set output o;
#   set term pdfcairo size 4,6.5;
# #   set output '$f/summary.png';
# #   set term pngcairo size 1700,2000;
#   
# #   title '$f' noenhanced 
#   set multiplot layout 5,1 margins .06, .96, .035, .95 spacing 0, 0;
#   
#   set ytics rangelimited;
#   set yrange [0:1];
#   unset xtics;
# 
# #   foo='32 39';
# #   do for [x in '45 64 131 154 193 268 288 346 465'] {
# #    set arrow from first x, graph 0 to first x, graph 1 nohead lc 'gray' front;
# #   };
#   do for [x in '25 100 300'] {
#    set arrow from first x, graph 0 to first x, graph 1 nohead lc 'red' front;
#   };
#   set arrow from graph 0, first .5 to graph 1, first .5 nohead dt 3 lc 'gray' back;
# 
#   set key above left title 'Modules';
#   cols='$cols0';
#   set y2label 'Modules 1';
#   plot for [i=1:words(cols):2] '<tail -n +2 $modules0' using 0:(column(word(cols, i)+0)) with lines lc i+3 title word(cols, i+1);
#   
#   unset key;
#   cols='$cols1';
#   set y2label 'Modules 2';
#   plot for [i=1:words(cols):2] '<tail -n +2 $modules1' using 0:(column(word(cols, i)+0)) with lines lc i+3 title word(cols, i+1);
#     
#   af='$f/acoustics.dat';
#   m=system('echo $f | sed "s/.*mute_t0i\(.\)/\1/"');
#   muted="Muted";
#         
#   set style fill solid 1 noborder;
#   set key above right title 'Channels';
#   i=0;
#   set y2label 'Voice '.(i+1);
#   set ytics (.2,.4,.6,.8);
#   if (strlen(m) == 1 && i == m) {
#     set arrow 10 from graph 0,0 to graph 1,1 nohead;
#     set arrow 11 from graph 1,0 to graph 0,1 nohead;
#     set obj 12 rect at graph .5, .5 size graph .15, .3 fs solid noborder fc 'white' front;
#     set label 1 at graph .5, graph .5 muted font ",18" center front;
#   };
#   plot for [j=10:12] af using 0:12*i+j:(0) with filledcurves title ''.(j-10);
#   unset label;
#   unset arrow 10;
#   unset arrow 11;
#   unset obj 12;
#   
#   unset key;
#   set ytics;
#   i=1;
#   set y2label 'Voice '.(i+1);
#   if (strlen(m) == 1 && i == m) {
#     set arrow 10 from graph 0,0 to graph 1,1 nohead;
#     set arrow 11 from graph 1,0 to graph 0,1 nohead;
#     set obj 12 rect at graph .5, .5 size graph .15, .3 fs solid noborder fc 'white' front;
#     set label 1 at graph .5, graph .5 muted font ",18" center front;
#   };
#   plot for [j=10:12] af using 0:12*i+j:(0) with filledcurves title columnhead;
#   unset label;
#   unset arrow 10;
#   unset arrow 11;
#   unset obj 12;
# 
# 
#   set style fill transparent solid .5 noborder;
#   set xtics;
#   set y2label 'Audition 1';
#   plot for [j=70:72] '$f/inputs_0.dat' using 0:j:(0) with filledcurves title columnhead;
# 
#   unset multiplot;
#   print('Generated', o);
# __DOC
# 
# done

# m-mann
f=../alife2022_results/v1-t2p2/ID798167/B/gen1999/mk__0.317601_0/eval_third_pass/798167-B-1999-0__798167-A-1998-0
cols=$(pretty_modules_columns_names $f/modules_0.dat AVT | grep $exclude "$excluded" | tr '\n' ' ')
if false
then
  exclude="-v"
  excluded="\b\(Neutral\|O\|F\|OF\|N\|ON\)\b"
else
  excluded="\b\(FN\|OFN\)\b"
fi

gnuplot -c /dev/stdin <<__DOC
  o='$f/summary.pdf';
  set output o;
  set term pdfcairo size 4,6;
  cols='$cols';
  
  set multiplot layout words(cols)/2,2 spacing 0,0 margins .05, .95, .05, .95 columnsfirst;
  
  set ytics rangelimited;
  set key rmargin;
  
  do for [j=0:1] {
    do for [i=1:words(cols):2] {
      if (i==words(cols)-1) {
        set xtics;
      } else {
        unset xtics;
      };
      
      plot '<tail -n +2 $f/modules_'.j.'.dat' using 0:(column(word(cols, i)+0)) with lines lc i/2 title word(cols, i+1);
    };
    
    unset ytics;
  };
  
  unset multiplot;
  print('Generated', o);
__DOC
