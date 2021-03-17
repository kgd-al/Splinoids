#!/usr/bin/awk -f

BEGIN {
  split(columns, cols);
}

NR==1 {
  for (i=1; i<=NF; i++)
    for (c in cols)
      if (cols[c] == $i)
        map[c] = i;
     
  for (k in map)
    printf "%s ", $(map[k]);
  printf "\n"
}

NR > 1 {
  for (k in map)
    printf "%s ", $(map[k]);
  printf "\n"  
}
