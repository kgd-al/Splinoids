#!/bin/bash

tmpfile=.gnuplot.tmp.$BASHPID.dat
clean(){
  rm -vf $tmpfile
}
trap clean EXIT

show_help(){
  echo "Usage: $0 -f <file> -c <columns> [-p] [-l] [-o <file>] [-q] [-s r|-t r] [-g]"
  echo "       -f <file> the datafile to use"
  echo "       -c <columns> columns specifiation (see below)"
  echo "       -p persist"
  echo "       -l <n> redraw graph every n seconds"
  echo "       -o <file> output file (implies no loop and no persist)"
  echo "       -s Use r samples row taken at regular intervals"
  echo "       -t Only show last r rows"
  echo "       -q Print a minimum of information"
  echo "       -g Additional gnuplot commands"
  echo 
  echo "Columns specifiation:"
  echo "  N, a column name"
  echo "  exp(N1,...), an arithmetic expression (recognised by gnuplot) using any number of column names"
  echo "  C = N|exp(N1,...)[:y2], additionally specify to plot on the right y axis"
  echo "  columns = C[;C]*"  
}

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

file=""
files=""
columns=""
persist=""
loop=""
outfile=""
verbose=""
gnuplotplus=""
while getopts "h?c:f:pl:o:s:t:qg:" opt; do
    case "$opt" in
    h|\?)
        show_help
        exit 0
        ;;
    c)  columns=$OPTARG
        ;;
    f)  file=$OPTARG
        [ ! -z "$files" ] && files="$files "
        files="$files$OPTARG"
        ;;
    p)  persist="-"
        ;;
    l)  loop="eval(loop($OPTARG))"
        ;;
    o)  outfile=$OPTARG
        
        case $outfile in
          *.png)
          output="  set term pngcairo size 1680,1050;"
          ;;
          *.pdf)
          output="  set term pdf size 11.2,7;"
          ;;
          *)
          echo "Unkown extension for output file '$outfile'. Aborting"
          exit 1
        esac
        
        output="$output
  set output '$outfile';
"
        ;;
    s)  samples=$OPTARG
        ;;
    t)  tails=$OPTARG
        ;;
    q)  verbose=no
        ;;
    g)  gnuplotplus=$OPTARG
        ;;
    esac
done

if [ "$outfile" ]
then
  persist=""
  loop=""
fi

if [ "$file" = "$files" ]
then
  [ -z "$verbose" ] && echo "   file: '$file'"
else
  [ -z "$verbose" ] && echo "  files: " $(echo $files | sed -e "s/ /' '/g" -e "s/^/'/" -e "s/$/'/")
  index=$(head -n 1 $file | awk -v header=$columns '{ for(i=1;i<=NF;i++) if ($i == header) print i}')
  echo "index: $index"
  
  i=0
  cfiles=""
  for file in $files
  do
    tmp=".$i.col"
    cut -d ' ' -f $index $file > $tmp && cfiles="$cfiles $tmp"
    i=$(expr $i + 1)
  done
  
  pasted=".n.col"
  paste -d " " $cfiles | awk -v cols=$columns 'BEGIN { print cols, cols "_lci", cols "_uci" } {
    sum=0;
    for (i=1; i<=NF; i++)  sum += $i;
    sum /= NF;
    std=0
    for (i=1; i<=NF; i++)  std += (sum-$i)*(sum-$i);
    std = sqrt(std / NF);
    Z=1.960;
    sr=sqrt(NF);
    print sum, sum - Z * std / sr, sum + Z * std / sr;
  }' > $pasted
  
  header=".header.col"
  cut -d ' ' -f 1 $file > $header

  decoyFile=".$columns.nh.col"
  paste -d " " $header $pasted > $decoyFile
  file=$decoyFile
#   columns="$columns; ${columns}_lci; ${columns}_uci"
fi

[ -z "$verbose" ] && echo "columns: '$columns'"

tics=6
[ ! -z ${TICS+x} ] && tics=$TICS

cmd="  set datafile separator ' ';
  set ytics nomirror;
  set y2tics nomirror;
  set key above autotitle columnhead;
  set style fill solid .25;
  
  min(a,b) = a<b ? a : b;" 

if [ "$gnuplotplus" ]
then
  cmd="$cmd
  
$gnuplotplus;  
"
fi

if [ "$outfile" ]
then
  cmd="$cmd
$output"
else
  cmd="$cmd
  loop(x) = 'while (1) { r=_rows(0); decoy=maketmp(r); eval(setupXTics(r)); replot; pause '.x.'; };';"
fi

# rows=$(wc -l $file | cut -d ' ' -f 1)
# stride=$(($rows / 5))
# tics=$(cut -d ' ' -f 1 $file | awk -v s=$stride 'NR % s == 1 { printf "\"%s\" %d\n", $0, NR }' | paste -sd "," -)
# tics="$tics, \"y1000d00h0\" $(cat $file | wc -l)-1" # That's ugly but what the hell...

cmd="$cmd
  _rows(x) = int(system('cat $file | wc -l'));"

if [ ! -z "$samples" ]
then
  datafile=$tmpfile
  readfile="awk -v s='.int(r / $samples).' \"NR==1{print} NR % s == 2 {print}\""
  cmd="$cmd
  rows(r) = min($samples, r);
  maketmp(r) = system('$readfile $file > $tmpfile');"
  
elif [ ! -z "$tails" ]
then
  datafile=$tmpfile
  readfile="awk -v r='.int(r-rows(r)).' \"NR==1{print} NR >=r {print}\""
  cmd="$cmd
  rows(x) = min($tails, r);
  maketmp(r) = system('$readfile $file > $tmpfile');"
  
else
  datafile=$file
  readfile="cat"
  cmd="$cmd
  rows(r) = r;
  maketmp(r) = system(':');"
fi

cmd="$cmd
  stride(r) = rows(r) / ($tics - 1);
  computeXTics(r) = system('cut -d \" \" -f 2 $datafile | awk -v s='.stride(r).' \"(NR % s == 2) || (NR == '.rows(r).'+1) { print \\\$0, NR-2 }\"');
  setupXTics(r) = 'tics=computeXTics('.r.'); set for [i=1:words(tics):2] xtics (word(tics, i) word(tics, i+1));';
  r = _rows(0);
  r_ = rows(r);
  decoy = maketmp(r);
  eval(setupXTics(r));
  print 'Displaying ', r_, ' / ', r, ' lines of data';
  print 'Displaying ', system('$readfile $file | wc -l'), ' lines of data';
  print 'Displaying ', system('wc -l $datafile'), ' lines of data';
  plot '$datafile' using (0/0):(0/0) notitle"


# cmd="$cmd
#   xticsCount=$tics;
#   ticsEvery(x) = (system(\"wc -l $file | cut -d ' ' -f 1\") - 1) / xticsCount;
#   linesPerTic=ticsEvery(0);
#   plot '$file' using (0/0):xtic(int(\$0)%linesPerTic == 0 ? stringcolumn(1) : 0/0) notitle"
  
[ -z "$verbose" ] && printf "\nreading columns specifications\n"
IFS=';' read -r -a columnsArray <<< $columns
for elt in "${columnsArray[@]}"
do
    W="A-Za-z_\{\}"
    y=$(cut -s -d: -f 2 <<< $elt)
    [ "$y" == "" ] && y="y1"
    colname=$(cut -d: -f 1 <<< $elt)
    gp_elt=$(sed "s/\([^$W]*\)\([$W]\+\)\([^$W]*\)/\1column(\"\2\")\3/g" <<< $colname)
    [ -z "$verbose" ] && printf -- "$elt >> $gp_elt : $y\n"
    
    if [ "$file" != "$files" ]
    then
      cmd="$cmd,
       '' using 0:(column(\"${colname}_lci\")):(column(\"${colname}_uci\")) with filledcurves title '$colname confidence interval (95%)'"
    fi
    
    cmd="$cmd,
       '' using ($gp_elt) axes x1$y with lines title '$(cut -d: -f1 <<< $elt)'"
done

cmd="$cmd;"
if [ "$loop" ]
then
  cmd="$cmd
  $loop"
fi

if [ -z "$verbose" ]
then
  printf "\nGnuplot script:\n%s\n" "$cmd"
else
  printf "Plotting from '$file' using '$columns'"
  [ ! -z "$outfile" ] && printf " into '$outfile'"
  printf "\n"
fi

gnuplot -e "$cmd" --persist $persist
