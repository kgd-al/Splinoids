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

template <typename T>
void diffStats (const T &prev, const T &curr) {
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
}

int main(int argc, char *argv[]) {
  using Ind = simu::Evaluator::Ind;

  // To prevent missing linkages
  std::cerr << config::PTree::rsetSize() << std::endl;

  // ===========================================================================
  // == Command line arguments parsing

  using CGenome = genotype::Critter;
  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string lhsTeamArg, rhsArg;

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

    ("lhs", "Splinoid genomes for left-hand side team",
     cxxopts::value(lhsTeamArg))
    ("rhs", "Splinoid genomes for right-hand side team (for live evaluation)",
     cxxopts::value(rhsArg))
    ("scenario", "Scenario name for canonical evaluations",
     cxxopts::value(rhsArg))

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

  std::string configFileAbsolute = stdfs::canonical(configFile).string();
  if (!stdfs::exists(configFile))
    utils::doThrow<std::invalid_argument>(
      "Failed to find Simulation.config at ", configFileAbsolute);
  if (!config::Simulation::readConfig(configFile))
    utils::doThrow<std::invalid_argument>(
      "Error while parsing config file ", configFileAbsolute, " or dependency");
  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);

  if (lhsTeamArg.empty())
    utils::doThrow<std::invalid_argument>("No data provided for lhs team");

  if (rhsArg.empty())
    utils::doThrow<std::invalid_argument>(
      "No evaluation context provided. Either specify an opponent team (via rhs)"
      " or a canonical scenario (via scenario)");

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

  Ind lhsTeam = simu::Evaluator::fromJsonFile(lhsTeamArg);

  auto start = simu::Simulation::now();
  const auto prevFitness = lhsTeam.fitnesses;
  const auto prevInfos = lhsTeam.infos;

  simu::Evaluator eval;
//  eval.setLesionTypes(lesions);

  auto params = simu::Evaluator::fromString(lhsTeamArg, rhsArg);

  eval.logsSavePrefix = stdfs::path(outputFolder) / params.kombatName;
  eval.annTagsFile = annNeuralTags;
  eval(lhsTeam, params);

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

  if (!params.neuralEvaluation) {
    auto rhsId = simu::Evaluator::id(params.rhs);
    std::cout << prevInfos << " =?= " << rhsId << "\n";
    if (prevInfos == rhsId) {
      std::cout << "Rhs id matching lhs memory. Comparison result for lhs:\n";
      diffStats(prevFitness, lhsTeam.fitnesses);

    } else {
      std::cout << "Result for lhs:\n";
      diffStats({}, lhsTeam.fitnesses);
    }
  }

//  s.destroy();
  return 0;
}
