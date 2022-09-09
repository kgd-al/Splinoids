#!/usr/bin/gnuplot

folder='tmp/lg-evo/last';
print(ARG1);
print(ARGC);
if (ARGC >= 1) {
  folder=ARG1
}
system('clear');
print(folder);

while (1) {
  while (system('[ -f "'.folder.'/gen_stats.csv" ]; echo $?;') != 0) {
    system('ls -l '.folder.'/gen_stats.csv 2> /dev/null');
    system ('printf "[%s] Waiting for folder %s to become populated\r" "$(date)" '.folder);
    pause 1;
  }
  
  system ('printf "[%s] Processing\r" "$(date) "'.folder);
  call 'scripts/language/plot_evolution.gp' folder '1'
  pause 4
}
