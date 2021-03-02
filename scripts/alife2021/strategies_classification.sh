#!/bin/bash

base=results/alife2021/v1
log=$base/strategies.log
cls=$base/classes.dat
dat=$base/$(basename $log log)dat

if [ -f "$log" ]
then
  echo "Log file '$log' already processed. Skipping"
else
  (
    printf "Run ID Stupid Fitness Handedness Rotation Speed Backwardness Fearful Class\n"
    for s in $base/*/*/*/*/*/scores.dat
    do
      r=$(cut -d/ -f 4 <<< "$s")
      printf "$r"
      awk '!/_l/{
        s[substr($1,3,1)] += $2;
      }
      END {
        printf " %d", (s["+"] < 4 || s["-"] < 4);
        printf " %f", s["+"] + s["-"];
        printf " %+.1f", s["+"]-s["-"];
      }' $s || break
      
      for t in $(dirname $s)/*[+-]/trajectory.dat
      do
        awk -vt=$t '
        function adiff(a,b) {
          d = atan2(sin(a-b), cos(a-b));
          return d < 0 ? -d : d;
        }
        BEGIN {PI = 3.14159265}
        {
          if (FNR>7) { pv[0] = v[0]; v[1] = v[1]; }
          if (FNR>6) { v[0] = $2 - c[0]; v[1] = $3 - c[1]; }
          if (FNR>5) { c[0] = $2; c[1] = $3; }
        }
        FNR > 7 {
          printf "%g", adiff(atan2(v[1], v[0]), atan2(pv[1], pv[0]));
        }
        FNR > 6 {
          s=sqrt(v[0]*v[0]+v[1]*v[1]);
          r=adiff($4, atan2(v[1], v[0])) > PI/2;
          printf " %g %g", s, r;
        }
        FNR > 5 {
          printf "\n";
        }' $t
      done | awk '{
        for (i=1; i<=NF; i++) s[i] += $i;
      }
      END {
        for (i=1; i<=NF; i++) printf " %g", s[i] / NR;
      }'
      
      for t in + -
      do
        paste -d ' ' $(dirname $s)/1[02]$t/trajectory.dat | tail -n +4 \
          | cut -d ' ' -f 2,3,12,13 | column -t | awk '
        NF != 4 { exit; }
        {
          d[0] = $3-$1; d[1] = $4-$2;
          sum += sqrt(d[0]*d[0]+d[1]*d[1]);
        }
        END { print sum / NR; }'
      done | awk '{sum+= $i;} END { printf " %g", sum; }'
      
      echo
    done | awk '
    function abs(x) { return x < 0 ? -x : x; }
    NR == 1 { for (i=2; i<=NF; i++) min[i] = max[i] = $i; }
    {
      if ($2)
        c = "Obstinate";
      else if ($7 > .5)
        c = "Backward";
      else if (abs($4) < 15)
        c = "Scanner";
      else
        c = "Generic";
      print NR, $0, c;
      for (i=2; i<=NF; i++) {
        if ($i < min[i])  min[i] = $i;
        sum[i] += $i;
        if (max[i] < $i)  max[i] = $i;
      }
    }
    END {
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

if [ -f "$dat" ]
then
  echo "Data file '$dat' already processed. Skipping"
else
  awk '
    function abs(x) { return x < 0 ? -x : x; }
    /^R/{
      print;
      next;
    }
    /^-/{next;}
    {
      for (i=1; i<=NF; i++) {
        d = $i;
        if (i > 2) {
          d = abs(d);
          if (NR == 2) {
            min[i] = max[i] = d;
          } else {
            if (d < min[i]) min[i] = d;
            if (max[i] < d) max[i] = d;
          }
        }
        data[NR,i] = d;
      }
      rows++;
    }
    END {
      for (j=2; j<=rows; j++) {
        printf "%s %s", data[j,1], data[j,2];
        for (i=3; i<=NF; i++)
          printf " %g", 100 * (data[j,i] - min[i]) / (max[i] - min[i]);
        printf "\n";
      }
    }
    ' $log > $dat
fi

ext=.splot.png 
for i in $(seq 2 $(awk 'END{print NR}' $dat));
do
  tmp=.data
  awk -vn=$i 'NR==1||NR==n' $dat > $tmp
  
  id=$(tail -n1 $tmp | cut -d ' ' -f 2)
  awk '{for (i=4; i<=NF; i++) printf "%s ", $i; printf "\n";}' $tmp > $tmp~
  mv $tmp~ $tmp
  
  o=$base/$id$ext
  if [ -f "$o" ]
  then
    echo "Spider plot '$o' already generated. Skipping"
  else
    cols=$(awk 'NR==1{print NF-1}' $tmp)
    cmd="
      set term pngcairo size 500,500;
      set output '$o';
      set title \"$(date): $id\n\";
      set spiderplot;
      set for [i=1:$cols] paxis i range [0:100];
      set style spiderplot fillstyle transparent solid .3 border linewidth 1 pointtype 6 pointsize 1.2;
      set grid spider linetype -1 linecolor 'grey' lw 1;
      plot for [i=1:$cols] '$tmp' using i title columnhead;"
#     echo "$cmd"
    ~/work/code/local/bin/gnuplot -e "$cmd" 2>&1 | grep -v "warning: iconv"
  fi
  
#   rm -f $tmp
done

agg=$base/splots.png
if [ -f $agg ]
then
  echo "Aggregate $agg already generated. Skipping"
else
  #generate file list sorted by fitness
  files=$(awk -vbase=$base -vext=$ext 'NR>1{ print $4, base"/"$2""ext }' $dat | sort -gr | cut -d ' ' -f 2)
  montage -trim -geometry 500x500+10+10 $files $agg
fi

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
    tagg="$dst/.$r.tmpagg.png"
    [ ! -f "$tmpt" ] && montage -trim -tile x2 -geometry 500x500+0+10 $base/$r/*/*/*/fitness*0/trajectories_v1_[+-].png $tmpt
    [ ! -f "$tagg" ] && montage -trim -geometry 250x500+10+0 $tmpt $base/$r$ext $tagg
    
    list+=($tagg)
  done
  printf "\n"
  
  champid=$(cat $cls | sort -k3,3gr | grep -m 1 $class | cut -d ' ' -f 2)
  cp $dst/.$champid.tmptrajectories.png $dst/champ_$class.png
  
#   echo "${list[@]}"
  montage -trim -geometry 500x500+25+0 "${list[@]}" -label $class -border 1 $dst/$class.png
  
  rm $dst/.*tmp*.png
done

