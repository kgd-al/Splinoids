#include <csignal>

#include "indevaluator.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"


void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  simu::IndEvaluator::aborted = true;
}

//long maybeSeed(const std::string& s) {
//  if (s.empty())  return -2;
//  if (std::find_if(
//        s.begin(),
//        s.end(),
//        [](unsigned char c) { return !std::isdigit(c) && c != '-'; }
//      ) != s.end()) return -2;

//  char* p;
//  long l = strtol(s.c_str(), &p, 10);
//  return p? l : -2;
//}

int main(int argc, char *argv[]) {
  using CGenome = genotype::Critter;
  using EGenome = genotype::Environment;
  using Ind = simu::IndEvaluator::Ind;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  std::string eGenomeArg;
  std::vector<std::string> cGenomeArgs;
  std::string scenarios;

  EGenome eGenome;
  std::vector<Ind> individuals;

  std::string outputFolder = "tmp/pp-eval/";
  char overwrite = simu::Simulation::PURGE;

  cxxopts::Options options("Splinoids (pp-evaluator)",
                           "Evaluation of minimal splinoids evolved according"
                           " to evaluator class");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

    ("scenarios", "Specific scenario(s) to test",
     cxxopts::value(scenarios))
    ("env-genome", "Environment's genome",
     cxxopts::value(eGenomeArg))
    ("spln-genome", "Splinoid genome",
     cxxopts::value(cGenomeArgs))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";


  std::cout << "Reading environment genome from input file '"
            << eGenomeArg << "'" << std::endl;
  eGenome = EGenome::fromFile(eGenomeArg);
  std::cout << "Environment:\n" << eGenome << "\n";

  if (cGenomeArgs.empty())
    utils::doThrow<std::invalid_argument>("No splinoid genomes provided");
  for (const auto arg: cGenomeArgs) {
    long cGenomeSeed = -2;//maybeSeed(arg);
    if (cGenomeSeed < -1) {
      std::cout << "Reading splinoid genome from input file '"
                << arg << "'" << std::endl;
      individuals.push_back(simu::IndEvaluator::fromJsonFile(arg));

    } else {
//      rng::FastDice dice;
//      if (cGenomeSeed >= 0) dice.reset(cGenomeSeed);
//      std::cout << "Generating splinoid genome from rng seed "
//                << dice.getSeed() << std::endl;
//      CGenome g = CGenome::random(dice);
//      cGenomes.push_back(g);
    }
  }


  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == Core setup


  auto start = simu::Simulation::now();

  simu::IndEvaluator eval (eGenome);
  eval.setScenarios(scenarios);

  for (auto &ind: individuals) {
    eval(ind, 0);

    for (const auto &p: ind.stats)
      std::cout << "\t" << p.first << ": " << p.second << "\n";
    for (const auto &p: ind.fitnesses)
      std::cout << p.first << ": " << p.second << "\n";
  }

  auto duration = simu::Simulation::durationFrom(start);
  uint days, hours, minutes, seconds, mseconds;
  mseconds = duration % 1000;
  duration /= 1000;
  seconds = duration % 60;
  duration /= 60;
  minutes = duration % 60;
  duration /= 60;
  hours = duration % 24;
  days = duration / 24;

  std::cout << "### Evaluation completed";
  std::cout << " in ";
  if (days > 0)
    std::cout << days << " days ";
  if (days > 0 || hours > 0)
    std::cout << hours << "h ";
  if (days > 0 || hours > 0 || minutes > 0)
    std::cout << minutes << "min ";
  if (days > 0 || hours > 0 || minutes > 0 || seconds > 0)
    std::cout << seconds << "s ";
  std::cout << mseconds << "ms" << std::endl;

//  s.destroy();
  return 0;
}
