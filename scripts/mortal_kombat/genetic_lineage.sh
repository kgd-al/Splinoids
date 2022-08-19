#!/bin/bash

usage(){
  echo "Usage: $0 <folder> <jq expression> [headers]"
  echo
  echo "       Extracts the corresponding field(s) from the genomes of every"
  echo "        generation and produces gnuplot graphs"
}

if [ ! -d "$1" ]
then
  echo "'$1' is not a valid folder"
  usage
  exit 1
fi

if [ -z "$2" ]
then
  echo "No jq expression provided"
  usage
  exit 2
fi

files=$(ls -v $1/gen[0-9]*/mk*.dna)
nfiles=$(echo $files | wc -w)
echo "Found $nfiles files"

data=$1/genetic_lineage.dat

headers=0
[ -n "$3" ] && headers=1

if [ ! -f $data ] || [ $(cat $data | wc -l) -ne $(($nfiles+1+$headers)) ]
then
  printf "# %s %s\n" "$(date)" "$2" > $data
  printf "Gen" >> $data
  [ -n "$3" ] && printf " $3" >> $data
  printf " Label\n" >> $data

  for dna in $files
  do
    printf "%s " $(sed 's|.*gen\([0-9]*\)/mk.*|\1|' <<< "$dna")
    jq '.dna | fromjson | .[1]'"$2" $dna | tr -d "," | paste -sd' ' | tr -d "\n"
    jq '.dna | fromjson | .[1] | .splines[0].data[3], .splines[3].data[2]' $dna \
      | paste -sd' ' | awk '{print "", $1 < .5 && .85 < $2 && $2 < .92}'
#     printf " 0\n"
  done | pv -s $nfiles -l >> $data
fi

columns=$(head -n 3 $data | tail -n1 | wc -w)
read rows cols < <(awk -vc=$columns '
  BEGIN{
    c-=2;
    x=sqrt(c); x=x%1 ? int(x)+1 : x;
    while ((c % x) != 0) x++;
    y=c/x;
    print x, y
  }')

echo $columns $rows $cols
  
output=$1/genetic_lineage.pdf
gnuplot -c /dev/stdin <<__DOC
  set term pdfcairo size 3.5*$cols,2*$rows;
  set output '$output';
  
  set key above;
  
  set multiplot layout $rows,$cols;
  
  do for [i=2:$columns-1] {
    plot '$data' using 1:i:$columns every ::1903 pt 7 ps .5 lc variable title columnhead(i);
  }
  
  unset multiplot;
__DOC

if [ $? -eq 0 ]
then
  echo "Generated '$output'"
else
  echo "Error while generating '$output'"
  exit 10
fi
