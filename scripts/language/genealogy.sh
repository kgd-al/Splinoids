#!/bin/bash

usage(){
  echo "Usage: $0 <genealogy.dat>"
  echo
  echo "       Plots the vector field of populations accross generations"
}

if [ ! -f "$1" ]
then
  usage
  exit 1
fi

folder=$(dirname $1)

# Useless, unreadable spagetti mess
# o=$(dirname $1)/$(basename $1 .dat).png
# gnuplot -e "
#   set term pngcairo size 1680,1050;
#   set output '$o';
#   
#   unset key;
#   plot '$1' using 1:2:(1):(\$4-\$2):6 with vectors nohead lc palette z;
#   
#   print('Generated $o');"

# Needs intermediate files (or just the champions?)
# ls -v $folder/gen[0-9]*/lg*.dna \
# | xargs jq -r '"\(.id[0]) \(.id[1]) \(.parents[0][1]) \(.fitnesses.lg)"' \
# | tee $folder/lineage.dat | column -t | grep "^12[0-9]"
# 
# o=$(dirname $1)/$(basename $1 .dat).png
# gnuplot -e "
#   set term pngcairo size 1680,1050;
#   set output '$o';
#   
#   unset key;
#   set xlabel 'Generations';
#   set ylabel 'Index';
#   set clabel 'Fitness';
#   plot '$folder/lineage.dat' using 1:3:(1):(\$2-\$3):4 with vectors lc palette z;
#   
#   print('Generated $o');"

# Dot conversion with elementary colormap
dot=$folder/genealogy.dot
# lines=$(($(cat $1 | wc -l) -1))
if true #[ ! -f "$dot" ]
then
  echo "digraph {"
  echo "  RNG;"
  echo "  novelty [shape=\"box\"];"
  echo "  lg [shape=\"diamond\"];"
  echo "  { rank=min; RNG; novelty; lg; };" 
  tac $1 | awk '
    /^[0-9]/ {
    
    gen=$1;
    i=$2;    
    id=$3;
    pid=$5;
    score=$6;

    if (i != 1 && !(id in parents)) next;

    if (id in seen) {
      if (gens[id] > gen) gens[id] = gen;
      next;
    }
    seen[id] = 1;    
    parents[pid] = 1;
    
    if (pid == -1 || pid == 4294967295) pid="RNG";
    hue=(score+1)/(3*3);
    
    switch (i) {
    case 0:   shape="box";      break;
    case 1:   shape="diamond";   break;
    default:  shape="ellipse";  break;
    }
    
    links[pid,id]=1;
    hues[id] = hue;
    shapes[id] = shape;
    gens[id] = gen;
    
  } END {
    for (id in gens) {
      printf "  %d [color=\"%g 1 1\", shape=\"%s\", label=\"%s:%s\"];\n",
             id, hues[id], shapes[id], gens[id], id; 
      ranks[gens[id]] = ranks[gens[id]] id "; "
    }
      
    for (l in links) {
      split(l, tokens, SUBSEP);
      printf "  %s -> %d;\n", tokens[1], tokens[2];
    }
    
    for (gen in ranks)
      printf "  { rank=same; %s }\n", ranks[gen];      
  }'
  echo "}"
fi > $dot
wc -l $dot

awk '{
  if (NR==2) {
    printf "  ranksep=.1;\n";
    printf "  nodesep=.2;\n";
    printf "  node [label=\"\", width=.1, height=.1];\n";
    printf "  edge [arrowhead=none];\n"
  }
  print;
} ' $dot | sed 's/, label=".*"// ' | dot -Tpng -o $dot.png
dot $dot -Tpdf -O
ls $folder/genealogy.dot.*
