#!/bin/bash

tmp=.input
runs=$(ls -d results/alife2021/v2n/[0-9]*/)
awk '
BEGIN { PROCINFO["sorted_in"]="@ind_num_asc"; }
/^1/{
  split(FILENAME, fileparts, "/");
  run=fileparts[4];
  
  scenario=substr($1, 1, 2);
  if (scenario != "13") next;
  
  side=substr($1, 3, 1);
  lesion=substr($1, 4);
  
  runs[run] = 1;
  scenarios[scenario] = 1;
  sides[side] = 1;
  lesions[lesion] = 1;
  
  scores[run,scenario,side,lesion] = $2;
}
END {
  print length(scenarios), length(sides), length(lesions);
  printf "ID";
  for (sc in scenarios) {
    for (sd in sides) {
      for (l in lesions)  printf " %s%s%s", sc, sd, l;
      for (l in lesions)  if (l != "") printf " %s%s/%s%s%s", sc, sd, sc, sd, l;
    }
  }
  printf "\n";
  
  for (r in runs) {
    printf "%s", r;
    for (sc in scenarios) {
      for (sd in sides) {
        for (l in lesions)
          printf " %s", scores[r,sc,sd,l];
          
        for (l in lesions)
          if (l!="")
            printf " %g", scores[r,sc,sd,l] - scores[r,sc,sd,""];
      }
    }
    printf "\n";
  }
}' results/alife2021/v2n/[0-9]*/*/*/*/*/scores.dat > $tmp
# 
# tmp=.input
# head -n51 $1 | tail -n+2 | cut -d ' ' -f 2,9,10 | sort -k2,2gr | nl -w1 -s' ' > $tmp
# # cat $tmp
# column -t $tmp
# classes=$(cut -d ' ' -f 4 $tmp | sort -u)
# 
nruns=$(ls $runs | wc -l)

read scenarios sides lesions < <(head -n1 $tmp)

echo "$tmp:"
tail -n+2 $tmp > $tmp~
mv $tmp~ $tmp
cat $tmp | column -t

cmd="
  set term pdfcairo size 11.2,7 font ',24';
  set output 'results/alife2021/v1/lesion_effect.pdf';
  set style fill transparent solid .3;
  set key above;
  set xtics rotate 90;
  unset xtics;
  set xlabel 'Runs';
  set ylabel 'Variation';
  
  set multiplot layout 2,3;
  do for [i=0:1] {
    do for [j=0:2] {
      plot '$tmp' using 0:i*7+j+6:(1):xtic(1) with boxes title columnhead(i*7+j+6).': '.(i*7+j+6);
    }
  }
  "
  
printf "%s\n" "$cmd"
gnuplot -e "$cmd"
# rm $tmp
