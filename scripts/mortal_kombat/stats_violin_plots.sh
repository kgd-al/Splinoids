#!/bin/bash

pattern='../alife2022_results/v1-t*/*/*/gen1999/mk*.dna'
[ ! -z $1 ] && pattern=$1

line(){ printf "$1%.0s" $(seq 1 $2); echo; }
transpose(){ cut -d ' ' -f$1 $2 | paste -s -d ' '; }

odir=../alife2022_results/analysis/stats/
output=$odir/stats.dat

if [ ! -f $output ]
then
  n=$(ls $pattern | wc -l)
  i=0
  for r in $pattern
  do
    line '#' 80
    echo $r
    
    indfolder=$(dirname $r)/$(basename $r .dna)
    o=$indfolder/stats
    [ ! -f $o ] && ./scripts/mortal_kombat/evaluate.sh --stats $r;
    
    [ ! -z ${VERBOSE+x} ] && cat $o
    
    parts=$(sed 's|.*/v1-\(t.*p[^/]*\)/ID\([0-9]*\)/\([A-Z]\).*|\1 \2/\3|' <<< "$r")
    
    if [ $i -eq 0 ]
    then
      (
        printf "Type ID "
        transpose 1 $o | tr -d ':\n'
        printf " Hits Damage Volume Talkativeness Channels Preferred C0 C1 C2 RawClusters Modules\n"
        ) > $output
    fi
    
    (
      printf "$parts "
      transpose 2 $o | tr '\n' ' '
      grep "True-hits\|Good-damage" $indfolder/hitrate.dat | transpose 2 - | tr '\n' ' '
      grep "Volume\|Talkativeness\|Channels\|Preferred\|Occupation" $indfolder/communication.dat | transpose 2- - | tr '\n' ' '
      c=$(grep "^$parts" $odir/../clusters/meta-clusters.dat | cut -d ' ' -f 13,13)
      echo $c $parts | awk '{
        s=9;
        if (substr($2, 2, 1) == "1") s--; 
        if (substr($2, 4, 1) == "2") s--;
        print $1, 100*$1/s;
      }'
    ) >> $output
    
    i=$(($i+1))
    
    line '#' 80
    line '#' 80
    echo
  done

  [ ! -z ${VERBOSE+x} ] && column -t $output
fi

cols=$(head -n1 $output | wc -w)

ext=png
[ ! -z ${EXT} ] && ext=$EXT

for t in t1p2 t1p3 t2p2 t2p3;
do
  grep -e "^$t" -e "Type" $output \
  | awk '
    function process(a) { return a; }
    NR==1 { pi=-1; for (i=1; i<=NF; i++) if ($i == "Preferred") pi = i; }
    NR>1 {
      t=$1;
      split($2, tokens, "/");
      r=tokens[1];
      p=tokens[2];
      data[t][r][p]=$(pi);
    } END {
      PROCINFO["sorted_in"]="@ind_str_asc";
      for (t in data) {
        printf "\"%s\"\n", t > "/dev/stderr";
        delete rows;
        for (r in data[t]) {
          rows[0]=rows[0]r" ";
          for (p in data[t][r]) rows[p]=rows[p] process(data[t][r][p]) " ";
        }
        for (r in rows) print rows[r];
      }
    }' > $odir/soundscapes.$t.dat

  p=$(sed 's/.*p//' <<< "$t")
  gnuplot -e "
      if ('$ext' eq 'png') {
        set output '$odir/soundscapes.$t.png';
        set terminal pngcairo size 500,50*$p;
      } else {
        set output '$odir/soundscapes.$t.pdf';
        set terminal pdfcairo crop size .2*48/$p,.5*$p;
      };
      
      set ytics ('A' 0, 'B' 1, 'C' 2);
      set cbrange [1:3];
      unset colorbox;
      unset xtics;
      plot '$odir/soundscapes.$t.dat' matrix columnheaders with image notitle;
    "
done

# ext=png
python3 - <<__DOC
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os.path
import scipy.stats

prettyHeaders = {
  "ann-hidn": "Number of hidden neurons"
}

def too_small(lhs, rhs):
  return len(set(lhs+rhs)) < 2

data=pd.read_csv('$output', delimiter=' ');
gp=data.groupby(by='Type')
ext='$ext'

if (ext == 'pdf'):
  plt.rcParams.update({'font.size': 16})

for i in range(2, data.shape[1]):
  name = data.columns[i]
  output='$odir/violin.'+name+'.$ext'
  
  if os.path.exists(output):
    print('Violin plot', output, 'already exists. Skipping', end='\n')
    continue
  else:
    print('Generating', output, 'for field', name) 
  
  def outlier(x):
    return False
    q25, q75 = x.quantile([.25,.75])
    iqr = q75-q25
    return (x < q25-1.5*iqr) | (q75+1.5*iqr < x)
  
  gpd = gp[data.columns[i]]
  keys, lists = zip(*[(k,i) for k, i in gpd])
  
  print("Processing", name)
  for i in range(len(lists)):
    lhs = lists[i].dropna().values.tolist()
    for j in range(len(lists)):
      rhs = lists[j].dropna().values.tolist()
      if not too_small(lhs, rhs):
        U, p = scipy.stats.mannwhitneyu(lhs, rhs, alternative='greater')
        if p < .05:
          print(f'mannwhitneyu({name}, {keys[i]}, {keys[j]}): U = {U}, p = {p}')
  
  flists = [l[~(np.isnan(l)|outlier(l))] for l in lists]

  med = gpd.median().mean()
  
  fig, ax = plt.subplots()
  
  if (ext != 'pdf'):
    l = ax.axline((.5,med), slope=0)
    l.set_dashes([1,1])
    l.set_linewidth(.25)
    l.set_color('gray')
  
  ax.set_ylabel(prettyHeaders[name] if name in prettyHeaders else name)
  
  ax.violinplot(flists, showextrema=False, points=1000)
  ax.boxplot(flists, widths=.15, whis=[0, 95], medianprops=dict(linewidth=1.5))
  ax.set_xticks([i+1 for i in range(len(keys))])
  ax.set_xticklabels(np.vectorize(lambda s: s.upper())(keys), weight = 'bold')

  plt.tight_layout()
  plt.savefig(output, dpi=150, bbox_inches='tight', pad_inches=0.05)
  plt.close()
__DOC

for f in ann-cnxt ann-hidn
do
  dat=$odir/$f.dat
  for t in t1p2 t2p2 t1p3 t2p3
  do
#     echo "\"$t\""
    for r in ls -vd ../alife2022_results/v1-$t/ID*
    do
      # Might not exist
      ls $r/*/gen1999/mk*/stats >/dev/null 2>&1 || continue
      
      grep $f $r/*/gen1999/mk*/stats | cut -d' ' -f 2 | sort -gr \
      | awk '{m+=$1;d[NR]=$1}END{for (i=1;i<=3;i++) printf "%g ", d[i]/m}'
      echo
    done | sort -k1,1gr
    printf "\n\n"
  done > $dat
  
  img=$odir/hist.$f.png
  gnuplot -c /dev/stdin <<__DOC
    set output '$img';
    set term pngcairo size 1680,1050;
    set style fill solid .25;
    set style data histograms;
    set style histogram rowstacked;
    unset key;
    set xrange [-.5:*];
    set yrange [0:1];
    set autoscale fix;
    set multiplot layout 2,2;
    do for [i=0:3] {
      if (i < 2) {
        set arrow from graph 0, first .5 to graph 1, first .5 nohead front;
      } else {
        set arrow from graph 0, first .33 to graph 1, first .33 nohead front;
        set arrow from graph 0, first .66 to graph 1, first .66 nohead front;
      }
      plot for [j=1:3] '$dat' using j index i;
      unset arrow;
    }
__DOC
done
