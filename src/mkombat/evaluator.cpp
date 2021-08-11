#include <csignal>

#include "indevaluator.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"


void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  simu::Evaluator::aborted = true;
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
  // ===========================================================================
  // == Command line arguments parsing

  using CGenome = genotype::Critter;
  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string lhsTeamArg, rhsTeamArg;
  CGenome lhsTeam, rhsTeam;

  std::string outputFolder = "tmp/mk-eval/";
  char overwrite = simu::Simulation::PURGE;

  std::string lesions;

  std::string annNeuralTags;

  cxxopts::Options options("Splinoids (mk-evaluator)",
                           "Evaluation of aggressive splinoids evolved according"
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

    ("lhs-team", "Splinoid genomes for left-hand side team",
     cxxopts::value(lhsTeamArg))
    ("rhs-team", "Splinoid genomes for right-hand side team",
     cxxopts::value(rhsTeamArg))

    ("lesions", "Lesion type to apply (def: {})", cxxopts::value(lesions))

    ("ann-aggregate", "Specifiy a collections of position -> tag for the ANN"
     " nodes", cxxopts::value(annNeuralTags))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
  config::Simulation::verbosity.overrideWith(0);
  if (configFile.empty()) config::Simulation::printConfig("");


  if (lhsTeamArgs.empty())
    utils::doThrow<std::invalid_argument>("No data provided for lhs team");
  if (rhsTeamArgs.empty())
    utils::doThrow<std::invalid_argument>("No data provided for rhs team");



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

  simu::Evaluator eval (!v1scenarios);
  eval.setScenarios(scenarios);
  eval.setLesionTypes(lesions);

  static const auto &diffStats = [] (auto prev, auto curr) {
    for (const auto &p: curr) {
      auto it = prev.find(p.first);
      std::cout << "\t" << std::setw(10) << p.first << ": ";
      if (it != prev.end()) {
        if (p.second == it->second)
          std::cout << GAGA_COLOR_GREEN;
        else
          std::cout << GAGA_COLOR_RED << it->second << " >> ";
        std::cout << p.second << GAGA_COLOR_NORMAL;
      } else
        std::cout << "\t" << GAGA_COLOR_YELLOW << p.second << GAGA_COLOR_NORMAL;
      std::cout << "\n";
    }
  };

  for (auto &p: individuals) {
    auto &ind = p.second;
    auto prevStats = ind.stats;
    auto prevFitnesses = ind.fitnesses;

    eval.logsSavePrefix = p.first;
    eval.annTagsFile = annNeuralTags;
    eval(ind, 0);

    diffStats(prevStats, ind.stats);
    diffStats(prevFitnesses, ind.fitnesses);
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
