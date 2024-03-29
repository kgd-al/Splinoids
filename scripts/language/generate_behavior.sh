#!/bin/bash

usage(){
  echo "Usage: $0 <ind.dna> <scenario>"
}

if [ ! -f "$1" ]
then
  echo "Please provide a valid genome to extract data from"
  usage
  exit 1
fi

if [ -z $2 ]
then
  echo "No evaluation scenario provided"
  usage
  exit 2
fi

line(){ 
  c=$1; [ -z $c ] && c='#'
  n=$2; [ -z $n ] && n=80
  printf "$c%.0s" $(seq 1 $n)
  echo
}

ind="$1"
scenario=$2
line
echo "Processing $ind for scenario $scenario"
indfolder=$(dirname $ind)/$(basename $ind .dna)
shift

sfolder=$(dirname $0) # script folder

vchannels=1 # vocal channels
# vc_outputs=10-12,22-24 # Voice outputs in acoustics.dat
vc_outputs=6,12 # Voice outputs in acoustics.dat

################################################################################
# Behavior in all scenarios
echo
line
echo "Extracting generic behavior"
line

dfolder=$indfolder/baseline
if ls $dfolder >/dev/null 2>&1
then
  echo "Data folder '$dfolder' already exists. Skipping"
else
  mkdir -pv $dfolder
  
  $sfolder/evaluate.sh $ind --data-folder $dfolder --scenario $scenario \
    --verbosity SHOW 2>&1 | tee $dfolder/eval.log
  r=${PIPESTATUS[0]}
  [ $r -ne 0 ] && exit $r
    
  communication=$indfolder/communication.dat
  if [ -f "$communication" ]
  then
    echo "Communication profile '$communication' already computed. Skipping"
  else
    cut -f $vc_outputs $dfolder/*/acoustics.dat | tail -n +2 | awk '{
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

# ################################################################################
# ################################################################################
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
        echo "Clustering of '$f' failed: not enough neurons"
      elif [ $r -gt 42 ]
      then
        echo "Clustering of '$f' failed: not enough data"
      elif [ $r -gt 0 ]
      then
        echo "Clustering of '$f' failed!"
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
  echo "Generated $(ls $aggregate)"

  # Mean
  # for (j=2; j<=NF; j++) printf " %.1f", data[n,j]/counts[n,j];

  # Union
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > 0);
  
  # Intersection (specifically for AEB)
  # for (j=2; j<=NF; j++) printf " %.1f", (data[n,j] > thr);
  
  neuralclusteringcolors $aggregate $flags
}

# ################################################################################
# # Evaluation for c22
if [ "$scenario" == "c22" ]
then
  scenario_arg=$indfolder/baseline/c22_XX/

  echo
  line
  echo "First-pass evaluation for $scenario"
  line

  pfolder=$(ffpass "first" "c22_e1")

  #               Emitter audition
  #               |     Receiver audition
  #               |     |     Emitter vision (single foodlet)
  #               |     |     |   Receiver vision (multiple foodlets)
  #               +-+   +-+   +-+ |
  #             v v v v v v   v v v
  currentflags="0;8;8;0;4;4;0;2;2;1"
# "AE2;AE1;AE0;AR2;AR1;AR0;VE2;VE1;VE0;VR"

  if ls $pfolder/* >/dev/null 2>&1
  then
    echo "Data folder '$pfolder' already populated. Skipping"
  else
    mkdir -p $pfolder
    
    ./scripts/language/evaluate.sh $ind --data-folder $pfolder \
      --scenario ne_c22 --scenario-arg $scenario_arg 
  fi

  aggregate="$pfolder/neurons_groups.dat"
  if [ -f "$aggregate" ]
  then
    echo "Neural aggregate '$aggregate' already generated. Skipping"
  else
    do_cluster $pfolder $currentflags
  fi
fi
# 
# ################################################################################
# # Evaluation 2: ally/opponent 1/opponent 2 discrimination
# echo
# line
# echo "First-pass evaluation 2: discrimination"
# line
# 
# pfolder=$(ffpass "first" "e2")
# validscenarios="e2_a e2_b e2_c"
# 
# #                   Second opponent (for p3)
# #                   | First opponent (anyone)
# #                   | | Ally (for t2)
# #                   v v v
# currentflags="0;0;0;4;2;1;0;0;0"
# 
# get_opponent_for_scenario(){
#   case $1 in
#   e1*|e2_[ab]|e3*) head -n 1 <<< "$opponents"
#   ;;
#   e2_c) tail -n 1 <<< "$opponents"
#   ;;
#   *) echo "Unknown scenario type '$1'"; exit 3
#   ;;
#   esac
# }
# 
# if ls $pfolder/* >/dev/null 2>&1
# then
#   echo "Data folder '$pfolder' already populated. Skipping"
# else
#   mkdir -p $pfolder
# 
#   s="e2_b"
#   echo "Scenario: $s"
#   opp=$(get_opponent_for_scenario $s)
#   $sfolder/evaluate.sh $ind $opp --scenario $s
#   archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s
#   
#   if [ $teams -gt 1 ]
#   then
#     s="e2_a"
#     echo "Scenario: $s"
#     $sfolder/evaluate.sh $ind $opp --scenario $s
#     archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s  
#   fi
#   
#   if [ $nbopps -gt 1 ]
#   then
#     s="e2_c"
#     echo "Scenario: $s"
#     opp=$(get_opponent_for_scenario $s)
#     $sfolder/evaluate.sh $ind $opp --scenario $s
#     archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s    
#   fi    
# fi
# 
# aggregate="$pfolder/neurons_groups.dat"
# if [ -f "$aggregate" ]
# then
#   echo "Neural aggregate '$aggregate' already generated. Skipping"
# else
#   do_cluster $pfolder $currentflags
# fi
# 
# ################################################################################
# # Evaluation 3: noise/sound ally/sound enemy
# echo
# line
# echo "First-pass evaluation 3: sounds"
# line
# 
# pfolder=$(ffpass "first" "e3")
# validscenarios="e2_n e2_f e2_o"
# 
# #             Opponent channel (if not mute)
# #             | Ally channel (for p2, if not mute)
# #             | | Noise
# #             v v v
# currentflags="4;2;1;0;0;0;0;0;0"
# 
# get_channel(){
#   field=$2
#   [ -z $field ] && field="Preferred"
#   file=$(find $(dirname $1 | sed 's/1998/1999/') -name "communication.dat")
#   grep "$field:" $file | cut -d ' ' -f 2
# }
# 
# get_opponent_for_e3o(){
#   maxVolume=0
#   opponent=""
#   for o in $(fetchopponents 0)
#   do
#     volume=$(get_channel $o "Volume")
#     echo "volume($o): $volume" >&2
#     echo "Greater: " $(awk "BEGIN{print ($volume > $maxVolume)}") >&2
#     [ $(awk "BEGIN{print ($volume > $maxVolume)}") -eq 0 ] && continue;
#     
#     maxVolume=$volume
#     opponent=$o
#     echo ">> Updated: $volume $o" >&2
#   done
#   echo "Returning: '$opponent'" >&2
#   echo $opponent
# }
# 
# 
# if ls $pfolder/* >/dev/null 2>&1
# then
#   echo "Data folder '$pfolder' already populated. Skipping"
# else
#   mkdir -p $pfolder
# 
#   s="e3_n"
#   echo "Scenario: $s"
#   opp=$(get_opponent_for_scenario $s)
#   $sfolder/evaluate.sh $ind $opp --scenario $s
#   archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s
# 
#   s="e3_f"
#   channelA=$(get_channel $ind)
#   if [ $channelA == "nan" ]
#   then
#     echo "Mute individual. Skipping $s"
#   else
#     echo "Scenario: $s"
#     $sfolder/evaluate.sh $ind $opp --scenario $s --scenario-arg $channelA
#     archive_eval_folder $(datafolder $ind $opp $s) $pfolder/$s  
#   fi
#   
#   s="e3_o"
#   opponent=$(get_opponent_for_e3o)
#   if [ -z $opponent ]
#   then
#     echo "Mute opponent(s). Skipping $s"
#   else
#     channelB=$(get_channel $opponent)
#     if [ $channelA == $channelB ]
#     then
#       echo "Opponent uses same channel as self. Skipping $s"
#     else
#       echo "Scenario: $s"
#       set -x
#       $sfolder/evaluate.sh $ind $opponent --scenario $s --scenario-arg $channelB
#       set +x
#       archive_eval_folder $(datafolder $ind $opponent $s) $pfolder/$s
#     fi
#   fi
# fi
# 
# aggregate="$pfolder/neurons_groups.dat"
# if [ -f "$aggregate" ]
# then
#   echo "Neural aggregate '$aggregate' already generated. Skipping"
# else
#   do_cluster $pfolder $currentflags
# fi
# 
# ################################################################################
# # Generate meta-modules
# mmann_aggregate=$indfolder/eval_first_pass/neural_groups.dat
# awk '
#   NR==1{print;next}
#   FNR==1{next;}
#   {
#     for(i=2;i<=NF;i++) d[$1][i]+=$i;
#   } END {
#     for (n in d) {
#       printf "%s", n;
#       for (i in d[n]) printf " %s", d[n][i];
#       printf "\n";
#     }
#   }' $indfolder/eval_first_pass/e*/neural_groups.dat > $mmann_aggregate
#   
# #               Sound
# #               |
# #               |     Vision     
# #             +-|-+ +-|-+ +-+-+- Touch
# #             v v v v v v v v v
# currentflags="4;4;4;2;2;2;1;1;1"
# neuralclusteringcolors $mmann_aggregate $currentflags
# 
# # cat $(dirname $aggregate)/$(basename $aggregate .dat).ntags
# 
################################################################################
# Second pass: rerun everything while monitoring modules
echo
line
echo "Second-pass evaluations"
line

for f in $indfolder/eval_first_pass/${scenario}_*/
do
  pfolder=$(sed 's/eval_first/eval_second/' <<< "$f")
  ntags=$f/neural_groups.ntags

  if ls $pfolder/* >/dev/null 2>&1
  then
    echo 'Data folder '$pfolder' already exists. Skipping'
  else
    line '='
    echo "Monitoring modules on natural scenarios"
    $sfolder/evaluate.sh $ind \
      --scenario $scenario \
      -f $pfolder \
      --ann-aggregate=$ntags
      
    line '='
    echo "Monitoring modules on evaluation scenarios"
    $sfolder/evaluate.sh $ind \
      --scenario ne_$scenario --scenario-arg $scenario_arg \
      -f $pfolder \
      --ann-aggregate=$ntags
  fi
done

# ################################################################################
# # Third pass: rerun just combat while monitoring meta-modules
# echo
# line
# echo "Third-pass evaluations"
# line
# 
# mmann_ntags=$(dirname $mmann_aggregate)/$(basename $mmann_aggregate .dat).ntags
# pfolder=$(ffpass "third" "")
# if ls $pfolder/* >/dev/null 2>&1
# then
#   echo 'Data folder '$pfolder' already exists. Skipping'
# else
#   for o in $opponents
#   do
#     line
#     echo "# $e/$opp"
#     $sfolder/evaluate.sh $ind $o -f $pfolder --ann-aggregate=$mmann_ntags
#   done
# fi
