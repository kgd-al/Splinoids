#!/bin/bash

# resfolder=results
resfolder=tmp/results

# if [ ! -f "$1" ]
# then
#   echo "Provided strategies log '$1' is not valid"
#   exit 1
# fi
# 
# tmp=.input
# (head -n 1 $1; head -n51 $1 | tail -n+2 | sort -k4,4gr) | cut -d ' ' -f 2,4,6-9 | nl -v0 -w1 -s' ' > $tmp
# # cat $tmp
# # column -t $tmp
# classes=$(tail -n+2 $tmp | cut -d ' ' -f 6 | sort -u)
# 
# out=$(dirname $1)/response.pdf
# cmd="
#   set term pdfcairo size 7.5,4.6 font ',24';
#   set output '$out';
#   set style fill solid .3;
#   unset key;
#   unset xtics;
#   set autoscale xfix;
#   set autoscale yfixmin;
#   set grid back;
#   tics='2 1 .2';
#   
#   set cbtics 10;
#   set lmargin 8;
#   set rmargin .2;
#   set bmargin 1.5;
#   set tmargin .25;
#   set multiplot layout 3,1 spacing 0;
#   do for [c=4:6] {
#     set ylabel system('head -n1 \"$tmp\" | cut -d \" \" -f'.c.' | sed -e \"s/FF/False F/\" -e \"s/TF/True F/\"');
#     set ytics word(tics, c-3);
#     if (c == 6) {
#       set xtics ('(a)' 1, '(b)' 34, '(c)' 28, '(d)' 21);
#     } else {
#       set xtics ('' 1, '' 34, '' 28, '' 21);
#     };
#     if (c == 5) {
#       set cblabel 'Fitness';
#     } else {
#       unset cblabel;
#     };
#     
#     plot '$tmp' using 1:c:(1):3 with boxes lc palette notitle;
#   };
#   "
#     
# # printf "%s\n" "$cmd"
# gnuplot -e "$cmd"
# # rm $tmp
# # xdg-open $out
# 
# 
# tmp=.fmrtmp
# c=$resfolder/alife2021/v2n/classes.dat
# paste -d ' ' <(head -n 1 $c; tail -n +2 $c | sort -k2,2g) $resfolder/alife2021/v2n/module_sizes | cut -d ' ' -f 2,3,4,9- | nl -w 1 -s ' ' -v 0 > $tmp
# 
# (head -n1 $tmp; tail -n +2 $tmp | sort -k7,7gr) | cut -d ' ' -f 1-3,5- > $tmp~
# mv $tmp~ $tmp
#  
# out=$(dirname $1)/fear_modular_response.pdf
# cmd="
#   set term pdfcairo size 7.5,4.6 font ',24';
#   set output '$out';
#   set style fill solid .3;
#   unset key;
#   set autoscale fix;
#   set grid back;
#   
#   set cbtics 10;
#   set lmargin 1.5;
#   set rmargin 2;
#   
#   set xrange [-80:0];
#   set xlabel 'Fear center size (%)';
#   unset colorbox;
#   set cblabel 'Fitness';
#       
#   set multiplot layout 1,2;
#   plot '< tail -n +2 $tmp' using (-\$6*.5):0:(-\$6*.5):(.5) with boxxyerrorbars notitle;
# 
#   set lmargin 2;
#   set rmargin 1.5;
#   set xrange [0:80];
#   set xlabel 'Ally center size (%)';
#   set format y;
#   unset ylabel;
#   set colorbox user origin screen .8,.1 size screen .05,.8;
#   plot '< sort -k5,5gr $tmp | tail -n +2' using (\$5*.5):0:(\$5*.5):(.5) with boxxyerrorbars notitle;
#   "
#     
# printf "%s\n" "$cmd"
# gnuplot -e "$cmd"

if true
then
  enfilter=1
  mtmp=.mtmp
  echo "ID m m_l4 m_l5" > $mtmp
  for r in $resfolder/alife2021/v2n/5*/
  do
    f=$r/*/*/*/*/
    id=$(basename $r)
    [ -z $(grep -m1 $id $resfolder/alife2021/v2n/module_sizes | cut -d ' ' -f8 | awk '{print ($1 > 0) ? "ok" : "";}') ] && continue
    
    printf "%s" $id >> $mtmp
    
#     for t in '12[-+]' 'selective_lesions/12[-+]_l4' 'selective_lesions/12[-+]_l5'
    for t in 'eval_canonical_with_lesions/E[EB]'{,_l4,_l5} 
    do
      awk -venfilter=$enfilter '
        FNR==1 {
          j = 0;
          for (i=2; i<=NF; i++) if ($i == "1024M") j = i;
          if (j == 0) exit;
        } FNR>1 {
          if (enfilter && int(substr($1, 3, 2)) == 0)  next;
          sum += $j;
          count++;
        } END {
          printf " %g", sum/count;
        }' $f/$t/modules.dat >> $mtmp
    done
    echo >> $mtmp
  done

  column -t $mtmp

  $(dirname $0)/wilcoxon.py $mtmp 1 'greater'

  rm $mtmp
fi

if false
then
  enfilter=1
  out=$resfolder/alife2021/v2n/modular_amygdala
  echo "ID |m| m_l4<m m_l5<m" > $out
  for r in $resfolder/alife2021/v2n/5*
  do
    f=$r/*/*/*/*/
    id=$(basename $r)
    ms=$(grep -m1 $id $resfolder/alife2021/v2n/module_sizes | cut -d ' ' -f8)
    
    rtmp=.rtmp
    awk -venfilter=$enfilter '
      FNR==1 {
        j = 0;
        for (i=2; i<=NF; i++) if ($i == "1024M") j = i;
        if (j == 0) exit;
        split(FILENAME, fparts, "/");
        l=substr(fparts[10], 3);
        if (l == "") l = "none";
        lesions[l] = 1;
      } FNR>1 {
        if (enfilter && int(substr($1, 3, 2)) == 0)  next;
        data[l,FNR] = $j;
      } END {
        printf "row";
        for (l in lesions) printf " %s", l;
        printf "\n";
        for (r=2; r<=FNR; r++) {
          printf "%d", r;
          for (l in lesions)
            printf " %g", data[l,r];
          printf "\n";
        }
      }' $f/eval_canonical_with_lesions/E[EB]{,_l4,_l5}/modules.dat > $rtmp
      
    printf "%s %s " $id $ms >> $out      
    output=$($(dirname $0)/wilcoxon.py $rtmp 1 'greater')
    if [ $? -ne 0 ]
    then
#       column -t $rtmp
      exit 255
    fi
    pv=$(grep none <<< "$output" | tr -s " " | sed 's/.*p-value = \(.*\))/\1/' | paste -s -d' ')
    echo $id $pv $(wc -l $rtmp)
    echo $pv >> $out
  done
  
  column -t $out
fi
