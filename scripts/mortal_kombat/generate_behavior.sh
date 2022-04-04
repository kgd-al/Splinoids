#!/bin/bash

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome to extract data from"
  exit 1
fi

parts(){
  readlink -m "$1" \
  | sed 's|\(.*\)/ID\(.*\)/\([A-Z]\)/gen\(.*\)/.*.dna|\1 \2 \3 \4|'
}

fetchopponents(){
  off=$1
  [ -z $off ] && off=1 
  read base id p g <<< $(parts "$ind")
  gen="gen"$(($g - $off))
  ls $base/ID$id/[A-Z]/$gen/mk__*_0.dna | grep -v "/$p/"
}

datafolder(){
  read lbase lid lp lg <<< $(parts "$1")
  read rbase rid rp rg <<< $(parts "$2")
  [ ! -z $3 ] && scnr="__$3"
  echo $indfolder/$lid-$lp-$lg-0__$rid-$rp-$rg-0$scnr/
}

line(){ 
  c=$1; [ -z $c ] && c='#'
  n=$2; [ -z $n ] && n=80
  printf "$c%.0s" $(seq 1 $n)
  echo
}

ind="$1"
echo "Processing $ind"
opponents=$(fetchopponents)
nbopps=$(wc -w <<< "$opponents")
# printf "opponents: %s\n" $opponents
indfolder=$(dirname $ind)/$(basename $ind .dna)
shift

sfolder=$(dirname $0) # script folder
teams=$(jq '.dna | fromjson | .[0]' $ind)

################################################################################
# Behavior against previous champion(s)
echo
line
echo "Extracting generic behavior"
line

if ls $indfolder/*1998* >/dev/null 2>&1
then
  echo "Data folder '$indfolder' already exists. Skipping"
else
  $sfolder/evaluate.sh $ind
  r=$?
  [ $r -ne 0 -a $r -ne 42 ] && exit 2
  
  hitrate=$indfolder/hitrate.dat
  if [ -f "$hitrate" ]
  then
    echo "Hit rates '$hitrate' already computed. Skipping"
  else
    awk -vt=$teams -vp=$nbopps '
      function tid(str){ return int((substr(str, 4, 1)-1)/t) }
      FNR>1{
        if (tid($2) == 0) hits[tid($3) == 0]++;
        mag[tid($2)][tid($3)] += $6;
      } END {
        print "True-hits:", hits[0]/p;
        print "False-hits:", hits[1]/p;
        print "Self-damage:", mag[0][0]/p;
        print "Good-damage:", mag[0][1]/p;
        print "Bad-damage:", mag[1][0]/p;
        print "Enemy-stupidity:", mag[1][1]/p;
      }' $indfolder/*1999*1998*/impacts.dat > $hitrate
  fi
  
  communication=$indfolder/communication.dat
  if [ -f "$communication" ]
  then
    echo "Communication profile '$communication' already computed. Skipping"
  else
    awk 'FNR>1{
      print $10,$11,$12;
      if (NF >= 24) print $22, $23, $24;
    }' $indfolder/*/acoustics.dat | awk '{
      for (i=1; i<=3; i++) {
        sum += $i;
        on[i] += ($i > 0);
      }
    } END {
      osum=0;
      ocount=0;
      maxChannelValue = on[0];
      maxChannel = "nan";
      for (i in on) {
        osum += on[i];
        ocount += (on[i] > 0);
        if (on[i] > maxChannelValue) {
          maxChannelValue = on[i]
          maxChannel = i
        }
      }
      print "Volume:", sum / NR;
      print "Talkativeness:", osum / NR;
      print "Channels:", ocount;
      print "Preferred:", maxChannel;
      printf "Occupation:";
      for (i=1; i<=3; i++) printf " %.g", osum == 0 ? 0 : on[i] / osum;
      printf "\n";
    }' | tee $communication
  fi  
fi

################################################################################
# Generic stats
stats=$indfolder/stats
if [ -f "$stats" ]
then
  echo "Stats file '$stats' already exists. Skipping"
else
  $sfolder/evaluate.sh --stats $ind #|| exit 2
fi

################################################################################
################################################################################
# Neural clustering based on reproductible conditions

neuralclusteringcolors(){
  o=$(dirname $1)/$(basename $1 dat)ntags
  ltags=$2
  if [ -z "$ltags" ]
  then
    echo "No neural tags provided"
    exit 3
  fi
  awk -vltags="$ltags" '
    BEGIN {
      split(ltags, tags, ";");
      n=length(tags);
      printf "// tags: %d", tags[1];
      for (i=2; i<=n; i++)
        printf ",%d", tags[i];
      printf "\n";
    }
    NR==1 { print "//    ", $0; }
    NR>1 {
      v=0;
      for (i=2; i<=NF; i++)
        if ($i != 0 && $i != "nan")
          v = or(v, tags[i-1]);
          
      r=""                    # initialize result to empty (not 0)
      a=v                     # get the number
      while(a!=0){            # as long as number still has a value
        r=((a%2)?"1":"0") r   # prepend the modulos2 to the result
        a=int(a/2)            # shift right (integer division by 2)
      }
     
      printf "// %s -> %0*d\n", $0, n, r;
      gsub(/[()]/, "", $1);
      printf "%s %d\n", $1, v;
    }' $1 > $o
  echo "Generated '$(ls $o)'"
}

ffpass(){
  echo "$indfolder/eval_$1_pass/$2"
}

archive_eval_folder(){  
  idir=$1
  odir=$2
  echo "$idir -> $odir"
  cp -rlf --no-target-directory $idir $odir && rm -r $idir
}

do_cluster(){
  folder=$1
  flags=$2
  for f in $folder/*/neurons.dat
  do
    dfolder=$(dirname $f)
    od=$dfolder/$(basename $f .dat)_groups.dat
    if [ ! -f $od ]
    then
      touch $od
      python3 $(dirname $0)/mw_neural_clustering.py $f -1
      r=$?
      if [ $r -eq 41 ]
      then
        echo "Clustering failed: not enough neurons"
      elif [ $r -gt 42 ]
      then
        echo "Clustering failed: not enough data"
      elif [ $r -gt 0 ]
      then
        echo "Clustering failed!"
        exit 2
      fi
      echo "Generated '$od'"
      neuralclusteringcolors $od $flags
    fi
  done
    
  aggregate="$folder/neural_groups.dat"
  echo "Generating data aggregate: $aggregate"
  
  ng_dat_files=$(ls $folder/*/neurons_groups.dat)
#   tail -n+0 $ng_dat_files
  awk '
  NR==1{ 
    header=$0;
    next;
  }
  FNR == 1 { next; }
  {
    neurons[$1]=1;
    for (i=2; i<=NF; i++) {
      if ($i != "nan") {
        data[$1,i] += $i;
        counts[$1,i]++;
      }
    }
  }
  END {
    print header;
    for (n in neurons) {
      printf "%s", n;
      for (j=2; j<=NF; j++) printf " %d", (data[n,j] > 0);
      printf "\n";
    }
  }' $ng_dat_files > $aggregate
  echo "Generated $aggregate"

  # Mean
  # for (j=2; j<=NF; j++) printf " %.1f", data[n,j]/counts[n,j];

  # Union
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
  
  # Intersection (specifically for AEB)
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > thr);
  
  neuralclusteringcolors $aggregate $flags
}

################################################################################
# Evaluation 1: pain/touch
echo
line
echo "First-pass evaluation 1: pain/touch"
line

pfolder=$(ffpass "first" "e1")
validscenarios="e1_a e1_i e1_t"

#                         Touch
#                         | Absolute pain
#                         | | Instantaneous pain
#                         v v v
currentflags="0;0;0;0;0;0;4;2;1"

if ls $pfolder/* >/dev/null 2>&1
then
  echo "Data folder '$pfolder' already populated. Skipping"
else
  mkdir -p $pfolder
  
  opp=$(head -n 1 <<< "$opponents")
  for s in $validscenarios
  do
    echo "Scenario: $s"
    $sfolder/evaluate.sh $ind $opp --scenario $s
    
    archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s
  done
fi

aggregate="$pfolder/neurons_groups.dat"
if [ -f "$aggregate" ]
then
  echo "Neural aggregate '$aggregate' already generated. Skipping"
else
  do_cluster $pfolder $currentflags
fi

################################################################################
# Evaluation 2: ally/opponent 1/opponent 2 discrimination
echo
line
echo "First-pass evaluation 2: discrimination"
line

pfolder=$(ffpass "first" "e2")
validscenarios="e2_a e2_b e2_c"

#                   Second opponent (for p3)
#                   | First opponent (anyone)
#                   | | Ally (for t2)
#                   v v v
currentflags="0;0;0;4;2;1;0;0;0"

get_opponent_for_scenario(){
  case $1 in
  e1*|e2_[ab]|e3*) head -n 1 <<< "$opponents"
  ;;
  e2_c) tail -n 1 <<< "$opponents"
  ;;
  *) echo "Unknown scenario type '$1'"; exit 3
  ;;
  esac
}

if ls $pfolder/* >/dev/null 2>&1
then
  echo "Data folder '$pfolder' already populated. Skipping"
else
  mkdir -p $pfolder

  s="e2_b"
  echo "Scenario: $s"
  opp=$(get_opponent_for_scenario $s)
  $sfolder/evaluate.sh $ind $opp --scenario $s
  archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s
  
  if [ $teams -gt 1 ]
  then
    s="e2_a"
    echo "Scenario: $s"
    $sfolder/evaluate.sh $ind $opp --scenario $s
    archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s  
  fi
  
  if [ $nbopps -gt 1 ]
  then
    s="e2_c"
    echo "Scenario: $s"
    opp=$(get_opponent_for_scenario $s)
    $sfolder/evaluate.sh $ind $opp --scenario $s
    archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s    
  fi    
fi

aggregate="$pfolder/neurons_groups.dat"
if [ -f "$aggregate" ]
then
  echo "Neural aggregate '$aggregate' already generated. Skipping"
else
  do_cluster $pfolder $currentflags
fi

################################################################################
# Evaluation 3: noise/sound ally/sound enemy
echo
line
echo "First-pass evaluation 3: sounds"
line

pfolder=$(ffpass "first" "e3")
validscenarios="e2_n e2_f e2_o"

#             Opponent channel (if not mute)
#             | Ally channel (for p2, if not mute)
#             | | Noise
#             v v v
currentflags="4;2;1;0;0;0;0;0;0"

get_channel(){
  field=$2
  [ -z $field ] && field="Preferred"
  file=$(find $(dirname $1 | sed 's/1998/1999/') -name "communication.dat")
  grep "$field:" $file | cut -d ' ' -f 2
}

get_opponent_for_e3o(){
  maxVolume=0
  opponent=""
  for o in $(fetchopponents 0)
  do
    volume=$(get_channel $o "Volume")
    echo "volume($o): $volume" >&2
    echo "Greater: " $(awk "BEGIN{print ($volume > $maxVolume)}") >&2
    [ $(awk "BEGIN{print ($volume > $maxVolume)}") -eq 0 ] && continue;
    
    maxVolume=$volume
    opponent=$o
    echo ">> Updated: $volume $o" >&2
  done
  echo "Returning: '$opponent'" >&2
  echo $opponent
}

scenario_arg(){
  case $1 in
  e3_f)
    echo ' --scenario-arg' $(get_channel $ind)
    ;;
  e3_o)
    opp=$(get_opponent_for_e3o)
    echo ' --scenario-arg' $(get_channel $opp)
    ;;
  esac
}

if ls $pfolder/* >/dev/null 2>&1
then
  echo "Data folder '$pfolder' already populated. Skipping"
else
  mkdir -p $pfolder

  s="e3_n"
  echo "Scenario: $s"
  opp=$(get_opponent_for_scenario $s)
  $sfolder/evaluate.sh $ind $opp --scenario $s
  archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s

  s="e3_f"
  channelA=$(get_channel $ind)
  if [ $channelA == "nan" ]
  then
    echo "Mute individual. Skipping $s"
  else
    echo "Scenario: $s"
    $sfolder/evaluate.sh $ind $opp --scenario $s --scenario-arg $channelA
    archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s  
  fi
  
  s="e3_o"
  opponent=$(get_opponent_for_e3o)
  if [ -z $opponent ]
  then
    echo "Mute opponent(s). Skipping $s"
  else
    channelB=$(get_channel $opponent)
    if [ $channelA == $channelB ]
    then
      echo "Opponent uses same channel as self. Skipping $s"
    else
      echo "Scenario: $s"
      set -x
      $sfolder/evaluate.sh $ind $opponent --scenario $s --scenario-arg $channelB
      set +x
      archive_eval_folder $(datafolder $ind $opponent $s) $pfolder/$s
    fi
  fi
fi

aggregate="$pfolder/neurons_groups.dat"
if [ -f "$aggregate" ]
then
  echo "Neural aggregate '$aggregate' already generated. Skipping"
else
  do_cluster $pfolder $currentflags
fi

################################################################################
# Generate meta-modules
mmann_aggregate=$indfolder/eval_first_pass/neural_groups.dat
awk '
  NR==1{print;next}
  FNR==1{next;}
  {
    for(i=2;i<=NF;i++) d[$1][i]+=$i;
  } END {
    for (n in d) {
      printf "%s", n;
      for (i in d[n]) printf " %s", d[n][i];
      printf "\n";
    }
  }' $indfolder/eval_first_pass/e*/neural_groups.dat > $mmann_aggregate
  
#               Sound
#               |
#               |     Vision     
#             +-|-+ +-|-+ +-+-+- Touch
#             v v v v v v v v v
currentflags="4;4;4;2;2;2;1;1;1"
neuralclusteringcolors $mmann_aggregate $currentflags

# cat $(dirname $aggregate)/$(basename $aggregate .dat).ntags

################################################################################
# Second pass: rerun everything while monitoring modules
echo
line
echo "Second-pass evaluations"
line

for e in $indfolder/eval_first_pass/e[0-9]
do
  eval=$(basename $e)  
  ntags=$e/neural_groups.ntags
  
  pfolder=$(ffpass "second" "$eval")
  if ls $pfolder/* >/dev/null 2>&1
  then
    echo 'Data folder '$pfolder' already exists. Skipping'
  else
    for s in $e/e*;
    do
      scenario=$(basename $s)
      line
      echo "# $scenario"
      opp=$(get_opponent_for_scenario $scenario)
      set -x
      $sfolder/evaluate.sh $ind $opp \
        --scenario $scenario $(scenario_arg $scenario $opp) \
        -f $pfolder \
        --ann-aggregate=$ntags > /dev/null
      set +x
      mv -v $pfolder/*$scenario $pfolder/$scenario
    done
    
    for opp in $opponents
    do
      line
      echo "# $e/$opp"
      $sfolder/evaluate.sh $ind $opp -f $pfolder --ann-aggregate=$ntags > /dev/null
    done
  fi    
done

################################################################################
# Third pass: rerun just combat while monitoring meta-modules
echo
line
echo "Third-pass evaluations"
line

mmann_ntags=$(dirname $mmann_aggregate)/$(basename $mmann_aggregate .dat).ntags
pfolder=$(ffpass "third" "")
if ls $pfolder/* >/dev/null 2>&1
then
  echo 'Data folder '$pfolder' already exists. Skipping'
else
  for o in $opponents
  do
    line
    echo "# $e/$opp"
    $sfolder/evaluate.sh $ind $o -f $pfolder --ann-aggregate=$mmann_ntags
  done
fi
