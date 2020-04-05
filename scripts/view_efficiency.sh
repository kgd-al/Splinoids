#!/bin/bash

LC_ALL=C

usage(){
  echo "Usage: $0 <genome> <epsilon>"
}

if [ $# -ne 2 ]
then
  usage
  exit 10
fi

if [ ! -f $1 ]
then
  echo "'$1' is not a regular file"
  usage
  exit 1
fi

c0=$(jq '.mature' $1)
c1=$(jq '.old' $1)

cmd="
  e = $2;
  c0 = $c0;
  c1 = $c1;
  c0C = atanh(1-e)/c0;
  c1C = sqrt(-(1-c1)*(1-c1)/(2*log(e)));
  gauss(a, m, s) = exp(-((a-m)**2)/(2*s**2));
  fy(a) = tanh(a * c0C);
  fo(a) = gauss(a, c1, c1C);
  f(a) = a <= c0 ? fy(a) : a < c1 ? 1 : fo(a);
  
  set xrange [0:1];
  set yrange [-.05:1.05];
  set samples 1000;
  
  set arrow from graph c0, 0 to graph c0, 1 nohead dt 2;
  set arrow from graph c1, 0 to graph c1, 1 nohead dt 2;
  
  set title sprintf('Efficiency(age); e = %g; errors: 0 (%g) c0 (%g) c1 (%g) 1 (%g)', e, f(0), f(c0)-1, f(c1)-1, f(1));
  unset title;
  set xtics (0, sprintf('c_0 = %.2f', c0) c0, sprintf('c_1 = %.2f', c1) c1, 1);
  unset key;
  
  set xlabel 'Age';
  set ylabel 'Efficiency';
  
  print sprintf('c0 = %5.2g', c0);
  print sprintf('c1 = %5.2g', c1);

  print sprintf('c0c = %5.2g', c0C);
  print sprintf('c1c = %5.2g', c1C);

  print '';
  print '------';
  print '';
  
  n=4.;
  do for [i=0:n] { x=c0*i/n; print sprintf('f(%5.2g) = %5.2g', x, f(x)); };
  do for [i=0:n] { x=c1+(1-c1)*i/n; print sprintf('f(%5.2g) = %5.2g', x, f(x)); };
  
  plot f(x);
"
echo "$cmd"

gnuplot -e "$cmd" -p
