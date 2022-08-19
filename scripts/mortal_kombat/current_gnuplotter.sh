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

# # Morphological fresco with lineage divergence
# python3 - << __DOC > .foo
# data= [
#   [ '.',  '.',  '.',  '.',  '.',  799,  900,  999,  '.',  '.', 1292, 1381, 1494, 1600, 1700, 1799, 1900, 1999, ],
#   [   0,  200,  400,  600,  700,  800,  '.',  '.',  '.', 1200, 1300, 1400, 1500,  '.',  '.',  '.',  '.',  '.', ],
#   [ '.',  '.',  '.',  '.',  '.',  '.',  879, 1000, 1100, 1192, 1279,  '.',  '.', 1599, 1694, 1800, 1880,  1988, ]
# ]
# for d in data:
#   for v in d:
#     print(v, end=' ')
#   print()
# __DOC
# rows=$(wc -l < .foo)
# files=$(awk '{
#   for (i=1; i<=NF; i++) print($i == "."?"null:":"-label "$i" ../alife2022_results/v1-t2p3/ID797421/B/gen"$i"/mk*_0_0.png");
# }' .foo
# )
# montage -tile x$rows -trim -pointsize 36 $files .tmp.png
# convert -alpha deactivate .tmp.png ../alife2022_results/v1-t2p3/ID797421/B/morphological_lineage.png

# 
# e1 (first, exemple)
f=../alife2022_results/v1-t1p3/ID799055/C/gen1999/mk__1.23709_0/eval_second_pass/e1/
o=$f/e1_summary.pdf
cols=$(pretty_modules_columns_names $f/e1_a/modules.dat THI)
echo $cols
gnuplot -e "
  set output '$o';
  set term pdfcairo size 6,6 font ',22';
  
  set multiplot layout 3,1 margins 0.1,0.95,0.125,0.89 spacing 0,0;
  
  unset key;
  set grid lc 'gray' dt '-';
  
  set format x '';
  set xtics 0,4,24;
  set ytics 0,.5,1;
  
  set ytics rangelimited;
  set yrange [-.1:1.1];
  set y2range [-.1:1.1];
  cols='$cols';
  
  do for [t=1:3] {
    if (t==3) {
      set format x;
      set xlabel 'Time (s)';
      set key above title 'Modules';
    };
    set y2label word('Health Pain Touch', t);
  
    f='$f/e1_'.word('a i t', t).'/modules.dat';
    plot for [i=1:words(cols):2] f using (\$0/25):(column(word(cols, i)+0)) with lines title word(cols, i+1); 
  };
  
  unset multiplot;
  print 'Generated $o';
"

# # e1 (second, flee-er)
# f=../alife2022_results/v1-t1p3/ID797308/B/gen1999/mk__0.582902_0/eval_second_pass/e1/797308-B-1999-0__797308-A-1998-0/
# o=$f/e1_summary.pdf
# modules=$f/modules.dat
# cols=$(pretty_modules_columns_names $modules THI | grep -v "I.*T")
# echo $cols
# gnuplot -e "
#   set output '$o';
#   set term pdfcairo size 6,2 font ',14';
#   
#   set multiplot layout 3,1 margins 0.07,0.81,0.25,0.98 spacing 0,0;
#   set key outside right center;
#   
#   set format x '';
#   set xtics 0,125,500;
#   
#   set ytics rangelimited;
# 
#   do for [x in '246 257 337'] { set arrow from first x, graph 0 to first x, graph 1 nohead lc 'gray'; };
#   
#   set yrange [.4:1.05];
#   cols='$cols';
#   set ytics (.5, .75, 1);
#   set y2label 'Modules';
#   plot for [i=1:words(cols):2] '$modules' using 0:(column(word(cols, i)+0)) with lines lc i+3 title word(cols, i+1);
# 
#   set yrange [-1.2:1.2];
#   set ytics (-1, 0, 1);
#   set y2label 'Arms';
#   plot for [i=0:1] '$f/outputs.dat' using 0:7+2*i with lines title word('Left Right', i+1);
#     
#   set xtics ();
#   do for [i=0:500:125] { set xtics add (''.(i/25) i); };
#   set format x;
#   set xlabel 'Time (s)';
#   set yrange [-.05:1.2];
#   set y2label 'Motors';
#   plot for [i=0:1] '$f/outputs.dat' using 0:1+i with lines title word('Left Right', i+1);
#   
#   unset multiplot;
#   print 'Generated $o';
# "

# e2 (first, range finder)
# f=../alife2022_results/v1-t2p2/ID798074/B/gen1999/mk__0.73633_0/eval_second_pass/e2/798074-B-1999-0__798074-A-1998-0/
# o=$f/summary.pdf
# modules0=$f/modules_0.dat
# modules1=$f/modules_1.dat
# cols0=$(pretty_modules_columns_names $modules0 CBA)
# cols1=$(pretty_modules_columns_names $modules1 CBA)
# gnuplot -e "
#   set output '$o';
#   set term pdfcairo size 6.5,2;
#   
#   set multiplot layout 2,1 margins .035, .975, .2, .9 spacing 0, 0;
#   set key above;
# 
#   set format x '';
#   set xtics 0,5,20;
#   set xrange [0:];
#   set x2range [0:500];
#   
#   set yrange [0:1];
#   set ytics (.2,.5,1) rangelimited;
#   set grid lc 'gray';
#   
#   t(s)=(s/25);
#   
#   cols='$cols0';
#   set y2label 'Teammate 1';
#   plot for [i=1:words(cols):2] '$modules0' skip 1 using (t(\$0+1)):(column(word(cols, i)+0)) with lines title word(cols, i+1);
#   
#   unset key;
#   set format x;
#   set xlabel 'Time (s)';
#   cols='$cols1';
#   set y2label 'Teammate 2';
#   plot for [i=1:words(cols):2] '$modules1' skip 1 using (t(\$0+1)):(column(word(cols, i)+0)) with lines title word(cols, i+1);
#   
#   unset multiplot;
#   print 'Generated $o';
# "

# # e3 (shirper)
# f=../alife2022_results/v1-t2p2/ID798167/B/gen1999/mk__0.317601_0/eval_second_pass/e3/798167-B-1999-0__798167-A-1998-0/
# for f in $f tmp/re-eval-8167B/mute_t0i*;
# do
#   base=$(awk -F'/' '{print $1}' <<< "$f")
# 
#   modules0=$f/modules_0.dat
#   modules1=$f/modules_1.dat
#   # O F OF N ON FN OFN
#   if false
#   then
#     exclude="-v"
#     excluded="\b\(Neutral\|O\|F\|OF\|N\|ON\)\b"
#   else
#     excluded="\b\(FN\|OFN\)\b"
#   fi
#   cols0=$(pretty_modules_columns_names $modules0 NFO | grep $exclude "$excluded" | tr '\n' ' ')
#   cols1=$(pretty_modules_columns_names $modules1 NFO | grep $exclude "$excluded" | tr '\n' ' ')
# 
#   echo "'$cols0' '$cols1'"
#   gnuplot -c /dev/stdin <<__DOC
#     o = '$f/summary.pdf';
#     set output o;
#     
#     rows=4;
#     width=4;
#     rheight=1;
#     set term pdfcairo size width, rows*rheight font ',18';
#     
#     lm=.1; rm=.86; bm=.15; tm=.99;
#     set multiplot layout rows,1 margins lm, rm, bm, tm spacing 0, 0;
#     
#     unset key;
#     
#     set ytics .2,.2,.8 rangelimited;
#     set yrange [0:1];
#     
#     unset xtics;
#     set xrange [0:20];
#  
#     set label 10 'Modules' at screen .95, .8 center front rotate;  
#     set label 11 'Vocalization' at screen .95, .37 center front rotate;
# 
#     do for [x in '25 100 300'] {
#       set arrow from first (x/25), graph 0 to first (x/25), graph 1 nohead lc 'red' front;
#     };
#     set arrow from graph 0, first .5 to graph 1, first .5 nohead dt 3 lc 'gray' back;
# 
#     cols='$cols0';
#     set y2label 'Ind. 1';
#     plot for [i=1:words(cols):2] '$modules0' skip 1 using ((\$0+1)/25):(column(word(cols, i)+0)) with lines lc i+3 title word(cols, i+1);
#     
#     unset label;
#     
#     unset key;
#     cols='$cols1';
#     set y2label 'Ind. 2';
#     plot for [i=1:words(cols):2] '$modules1' skip 1 using ((\$0+1)/25):(column(word(cols, i)+0)) with lines lc i+3 title word(cols, i+1);
#       
#     af='$f/acoustics.dat';
#     m=system('echo $f | sed "s/.*mute_t0i\(.\)/\1/"');
#     muted="Muted";
#           
#     set style fill solid 1 noborder;
#     do for [i=0:1] {
#       set y2label 'Ind. '.(i+1);
#       if (strlen(m) == 1 && i == m) {
#         set arrow 10 from graph 0,0 to graph 1,1 nohead;
#         set arrow 11 from graph 1,0 to graph 0,1 nohead;
#         set obj 12 rect at graph .5, .5 size graph .2, .35 fs solid noborder fc 'white' front;
#         set label 1 at graph .5, graph .5 muted font ",18" center front;
#       };
#       
#       if (i==1) {
#         set xtics 0,5,20;
#         set xlabel 'Time (s)';
#       };
#       
#       plot for [j=10:12] af using ((\$0+1)/25):12*i+j:(0) with filledcurves title ''.(j-10);
#       unset label;
#       unset arrow 10;
#       unset arrow 11;
#       unset obj 12;
#     }
# 
#     unset multiplot;
#     print('Generated', o);
# 
#     if ('$base' eq '..') {
#       o='$f/summary_soundscape.pdf';
#       set term pdfcairo size width, 1.475*rheight;
#       set output o;
#       
#       set multiplot layout 1,1 margins lm, rm, bm+.27, tm;
#       set style fill transparent solid .5 noborder;
#       set xtics 0,5,20;
#       set xlabel 'Time (s)';
#       set y2label '';
#       plot for [j=70:72] '$f/inputs_0.dat' using (\$0/25):j:(0) with filledcurves title columnhead;
#       unset multiplot;
#       
#       print('Generated', o);
# 
#       o='$f/summary_key.pdf';
#       set term pdfcairo size width, 1.4*rheight;
#       set output o;
#       
#       unset border;
#       unset tics;
#       unset arrow;
#       unset xlabel;
#       
#       set multiplot layout 1,1 margins lm, rm, bm, tm;
#       
#       set key above width 2 height .5 box title 'Modules';
#       plot for [i=1:words(cols):2] '$modules1' skip 1 using (1/0) with lines lc i+3 title word(cols, i+1);
#       
#       set key below title 'Channels';
#       plot for [j=10:12] af using (1/0) with filledcurves title ''.(j-10);
#             
#       print('Generated', o);
#     };
# 
# __DOC
# done

# m-mann evolution
# for id in t2p2/ID798167/B t2p2/ID798074/B t1p3/ID797308/B t1p3/ID799055/C
# do  
#   $(dirname $0)/metamodules_lineage.py ../alife2022_results/v1-$id/mann_lineage.stats.dat
# done
#   
# f=../alife2022_results/v1-t2p2/ID798167/B/mann_lineage.stats.dat
# o=$(dirname $f)/$(basename $f .stats.dat).details.pdf
# gnuplot -c /dev/stdin << __DOC
#   max=system('cut -d" " -f4 $f | sort -g | tail -n 1');
#   bins=int(max / 5);
#   print(max, bins);
# 
#   set term pdfcairo size 6.5,1.5 font ',18';
#   set output '$o';
#   
#   set multiplot layout 1,1 margins .11, .95, .36, .95;
#   
#   set xrange [0:2000];
#   set xtics 0,250;
#   set xlabel 'Generation';
#   
#   unset ytics;
#   set yrange [0:140];
#   set ytics (0, 50, 100, 140);
#   set ylabel 'Modularity';
#   
#   plot '$f' using 1:4 pt 7 ps .25 notitle;
#   
#   print('Generated $o');
# __DOC
