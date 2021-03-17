#!/bin/bash

v=v1
[ ! -z "$1" ] && v=$1

base=results/alife2021/$v
log=$base/strategies.log
cls=$base/classes.dat
dat=$base/$(basename $log log)dat

average(){
  start=1
  [ ! -z $1 ] && start=$1
  awk -vi0=$start '{
    for (i=i0; i<=NF; i++) s[i] += $i;
  }
  END {
    for (i=i0; i<=NF; i++) printf " %g", s[i] / NR;
  }'
}

if false #[ -f "$log" ]
then
  echo "Log file '$log' already processed. Contents:"
  column -t $log
else
  (
    printf "Run ID Stupid Fitness Handedness TFear FFear Accuracy Class\n"
    for s in $base/*/*/*/*/*/scores.dat
    do
      r=$(cut -d/ -f 4 <<< "$s")
      printf "$r"
      
      # Stupid Fitness Handedness
      awk '!/_l/{
        s[substr($1,3,1)] += $2;
      }
      END {
        printf " %d", (s["+"] < 4 || s["-"] < 4);
        printf " %f", s["+"] + s["-"];
        printf " %+.1f", s["+"]-s["-"];
      }' $s || break
     
      for t in + -
      do
        # True fear
        tail -n1 $(dirname $s)/12$t/trajectory.dat | cut -d ' ' -f 2,3,8,9 \
          | awk '
        {
          d[0] = $3-$1; d[1] = $4-$2;
          printf " %g", sqrt(d[0]*d[0]+d[1]*d[1]);
        }'
        
        # False fear
        tail -n1 $(dirname $s)/11$t/trajectory.dat | cut -d ' ' -f 2,3,8,9 \
          | awk '
        {
          d[0] = $3-$1; d[1] = $4-$2;
          printf " %g", sqrt(d[0]*d[0]+d[1]*d[1]);
        }'
          
        # Accuracy
        tail -n +4 $(dirname $s)/13$t/trajectory.dat | cut -d ' ' -f 3 \
        | awk -vsign="${t}1" '{ sum += ($1 < 0 ? -1 : 1); }  END { printf " %g", sign * sum / NR }'
        
        echo
          
      done | average 1
      
      echo
    done | awk '
    function abs(x) { return x < 0 ? -x : x; }
    NR == 1 { fearindex=6; for (i=2; i<=NF; i++) min[i] = max[i] = $i; }
    {
      row[NR]=NR" "$0;
      fearfulness[NR]=$fearindex;
      for (i=2; i<=NF; i++) {
        if ($i < min[i])  min[i] = $i;
        sum[i] += $i;
        if (max[i] < $i)  max[i] = $i;
      }
    }
    END {
      fmax = max[fearindex];
      fmin = min[fearindex];
      frange = fmax - fmin;
      
      for (r=1; r<=NR; r++) {
        fear = fearfulness[r];
        if (fear < .25*frange+fmin)
          class = "Bold";
        else if (fear < .5*frange+fmin)
          class = "MildBold";
        else if (fear < .75*frange+fmin)
          class = "MildFearful";
        else
          class = "Fearful";
        print row[r], class; 
      }
      printf "\n-- ------\n-- Min";
      for (i=2; i<=NF; i++) printf " %g", min[i];
      printf "\n-- Avg";
      for (i=2; i<=NF; i++) printf " %g", sum[i] / NR;
      printf "\n-- Max";
      for (i=2; i<=NF; i++) printf " %g", max[i];
      printf "\n";
    }') | tee $log | column -t
    
  cut -d ' ' -f 1,2,4,10 $log | grep '^[R0-9]' | sort -k3,3g > $cls
fi

exit

# if false #[ -f "$dat" ]
# then
#   echo "Data file '$dat' already processed. Skipping"
# else
#   # Careful reading order is right -> left
#   awk -vneg='010011' '
#     function abs(x) { return x < 0 ? -x : x; }
#     BEGIN {
#       smax=100;
#       for (i=1; i<=length(neg); i++)
#         smin[i] = (substr(neg, i, 1) == 0) ? 0 : -100;
#     }
#     /^R/{
#       print;
#       next;
#     }
#     /^-/{next;}
#     {
#       for (i=1; i<=NF; i++) {
#         d = $i;
#         if (i > 2) {
#           if (smin[i] < 0)
#             d = abs(d);
#           if (NR == 2) {
#             min[i] = max[i] = d;
#           } else {
#             if (d < min[i]) min[i] = d;
#             if (max[i] < d) max[i] = d;
#           }
#         }
#         data[NR,i] = d;
#       }
#       rows++;
#     }
#     END {
#       for (j=2; j<=rows; j++) {
#         printf "%s %s %s", data[j,1], data[j,2], data[j,3];
#         for (i=4; i<=NF; i++)
#           printf " %g", (smax - smin[i-3]) * (data[j,i] - min[i]) / (max[i] - min[i]) + smin[i-3];
#         printf "\n";
#       }
#     }
#     ' $log > $dat
# fi
# 
# ext=.splot.png 
# for i in $(seq 2 $(awk 'END{print NR}' $dat));
# do
#   tmp=.data
#   awk -vn=$i 'NR==1||NR==n' $dat > $tmp
#   
#   id=$(tail -n1 $tmp | cut -d ' ' -f 2)
#   awk '{for (i=4; i<=NF; i++) printf "%s ", $i; printf "\n";}' $tmp > $tmp~
#   mv $tmp~ $tmp
#   
#   o=$base/$id$ext
#   if [ -f "$o" ]
#   then
# #     echo "Spider plot '$o' already generated. Skipping"
#     printf "."
#   else
#     cols=$(awk 'NR==1{print NF-1}' $tmp)
# #     cmd="
# #       set term pngcairo size 500,500;
# #       set output '$o';
# #       set title \"$(date): $id\n\";
# #       set spiderplot;
# #       set for [i=1:$cols] paxis i range [0:100];
# #       set style spiderplot fillstyle transparent solid .3 border linewidth 1 pointtype 6 pointsize 1.2;
# #       set grid spider linetype -1 linecolor 'grey' lw 1;
# #       plot for [i=1:$cols] '$tmp' using i title columnhead;"
# # #     echo "$cmd"
# #     ~/work/code/local/bin/gnuplot -e "$cmd" 2>&1 | grep -v "warning: iconv"
#     
#   fi
#   
# #   rm -f $tmp
# done
# 
# agg=$base/splots.png
# if [ -f $agg ]
# then
#   echo "Aggregate $agg already generated. Skipping"
# else
#   #generate file list sorted by fitness
#   files=$(awk -vbase=$base -vext=$ext 'NR>1{ print $4, base"/"$2""ext }' $dat | sort -gr | cut -d ' ' -f 2)
#   montage -trim -geometry 500x500^+10+10 $files $agg
# fi

dst=$base/classes/
mkdir -vp $dst
classes=$(tail -n +2 $cls | cut -d ' ' -f 4 | sort | uniq)
echo "Processing classes '$(echo $classes | tr '\n' ' ')'"
for class in $classes
do
  printf "$class"
  list=()
  for r in $(grep $class $cls | sort -k3,3gr | cut -d ' ' -f 2)
  do
    printf " $r"
    
    tmpt="$dst/.$r.tmptrajectories.png"
#     tagg="$dst/.$r.tmpagg.png"
#     [ ! -f "$tmpt" ] && 
    montage -trim -tile x2 -geometry 500x500^+0+10 $base/$r/*/*/*/fitness*0/trajectories_v1_[+-].png $tmpt
#     [ ! -f "$tagg" ] && 
#     montage -trim -geometry 500x500^+10+0 $tmpt $base/$r$ext $tagg
    
#     list+=($tagg)
    list+=($tmpt)
  done
  printf "\n"
  
  champid=$(cat $cls | sort -k3,3gr | grep -m 1 $class | cut -d ' ' -f 2)
#   cp -v $dst/.$champid.tmpagg.png $dst/champ_$class.png
  cp -v $dst/.$champid.tmptrajectories.png $dst/champ_$class.png
  
#   echo "${list[@]}"
  montage -trim -geometry 500x500^+25+0 "${list[@]}" -label $class -border 1 $dst/$class.png
  
  rm $dst/.*tmp*.png
done

