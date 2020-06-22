cp -uv cecyle/test_runs/completed.list results/ann_size_test/cecyle_completed.list

for r in $(cat results/ann_size_test/cecyle_completed.list)
do
  i=$(sed 's/results/cecyle/' <<< "$r");

  for g in 1 250 500 750 1001
  do
    o=results/ann_size_test/$(cut -d '/' -f 3 <<< "$r")/gen$g;
    printf "Processing %s\n" "$i";
    
    save=""
    
    if [ -d $o ]
    then
      save=$(find $o -name "*g$g-*.save.ubjson")
    else
      mkdir -p $o
    fi
    
    if [ -z "$save" ]
    then
#       save=$(ls -v $i/*save.ubjson | tail -1)
      save=$(find $i/ -name "*g$g-*.save.ubjson")
      cp -uv $save $o/
      save=$o/$(basename $save)
    fi
    
    l=$(sed 's/.*HNL\([0-9]\).*/\1/' <<< "$i")
    v=$(sed 's/.*HNV\([0-9]\).*/\1/' <<< "$i")
    
    if [ ! -f "$o/champ_genome.spln.json" ]
    then
      hyperNEATHiddenNeuronLayers=$l hyperNEATVisualNeurons=$v ./build_release/evaluator --load $save --overwrite 'i' -f $o
    fi
    
  #   if [ ! -f "$o/carnivorous_behavior.dat" ]
  #   then
  #     tmp=$o/feeding_behavior_evaluation
  #     hyperNEATHiddenNeuronLayers=$l hyperNEATVisualNeurons=$v ./build_release/evaluator --load $save --overwrite p -f $tmp --feeding-behavior=true
  #     mv $tmp/carnivorous_behavior.dat $o/
  # #     rm -v $tmp/*
  # #     rmdir -v $tmp
  #   fi
    
    [ ! -f "$o/champ_strategy.png" ] && ./scripts/plot_trajectories.sh $o
    
    [ ! -f "$o/champ_ann.png" ] && ./scripts/plot_brain.sh $o/champ
    
    printf "\033[32mProcessed %s/%d\033[0m\n\n" $o
  done
done
