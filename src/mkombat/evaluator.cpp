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
bool diffStatsMap (const T &prev, const T &curr) {
  bool allOk = true;
  for (const auto &p: curr) {
    auto it = prev.find(p.first);
    std::cout << "\t" << std::setw(10) << p.first << ": ";
    if (it != prev.end()) {
      bool ok = (p.second == it->second);
      if (ok)
        std::cout << GAGA_COLOR_GREEN;
      else
        std::cout << GAGA_COLOR_RED << it->second << " >> ";
      std::cout << p.second << GAGA_COLOR_NORMAL;
      allOk &= ok;

    } else {
      if (!prev.empty()) std::cout << GAGA_COLOR_YELLOW;
      std::cout << p.second;
    }
    std::cout << GAGA_COLOR_NORMAL << "\n";
  }

  return allOk;
}

template <typename T, typename U>
bool diffStatsArray (const T &prev, const T &curr, const U &fields) {
  bool allOk = (std::size(prev) == std::size(curr));
  uint i=0;
  auto it_p = prev.begin(), it_c = curr.begin(), it_l = fields.begin();
  for (;
       it_p != prev.end() && it_c != curr.end() && it_l != fields.end();
       ++it_p, ++it_c, ++it_l, i++) {
    auto v_p = *it_p, v_c = *it_c;
    bool ok = (v_p == v_c);
    std::cout << "\t" << std::setw(10) << *it_l << ": ";
    if (ok)
      std::cout << GAGA_COLOR_GREEN;
    else
      std::cout << GAGA_COLOR_RED << v_p << " >> ";
    std::cout << v_c << GAGA_COLOR_NORMAL << "\n";
    allOk &= ok;
  }

  if (it_p != prev.end())
    std::cout << GAGA_COLOR_YELLOW << "! "
              << std::distance(it_p, prev.end())
              << " excess values in previous footprint\n";
  if (it_c != curr.end())
    std::cout << GAGA_COLOR_YELLOW << "! "
              << std::distance(it_c, curr.end())
              << " excess values in current footprint\n";

  return allOk;
}

int main(int argc, char *argv[]) {
  using Ind = simu::Evaluator::Ind;

  // To prevent missing linkages
  std::cout << config::PTree::rsetSize() << std::endl;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string lhsTeamArg;
  std::vector<std::string> rhsTeamArgs;
  std::string scenario;

  int teamSize = -1;

  std::string outputFolder = "tmp/mk-eval/";
  char overwrite = simu::Simulation::PURGE;

  std::string lesions;

  std::string annNeuralTags;

  bool stats = false;

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

    ("ind", "Splinoid genomes for left-hand side team",
     cxxopts::value(lhsTeamArg))
    ("opp", "Splinoid genomes for right-hand side team(s)",
     cxxopts::value(rhsTeamArgs))
    ("scenario", "Scenario name for canonical evaluations",
     cxxopts::value(scenario))

    ("team-size", "Force a specific team size"
                  " (independantly from genomes' preferences)",
     cxxopts::value(teamSize))

    ("lesions", "Lesion type to apply (def: {})", cxxopts::value(lesions))

    ("ann-aggregate", "Specify a collections of position -> tag for the ANN"
     " nodes", cxxopts::value(annNeuralTags))

    ("stats", "Dump a bunch of statistics to file",
     cxxopts::value(stats)->implicit_value("true"))
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
    utils::Thrower("Failed to find Simulation.config at ", configFileAbsolute);
  if (!config::Simulation::readConfig(configFile))
    utils::Thrower("Error while parsing config file ", configFileAbsolute,
                   " or dependency");
  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);

  if (lhsTeamArg.empty()) utils::Thrower("No data provided for lhs team");
  if (rhsTeamArgs.empty() && !stats)
    utils::Thrower("No data provided for rhs team(s)");

  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::Thrower<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::Thrower<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == Core setup

  if (stats) {
    simu::Evaluator::dumpStats(lhsTeamArg, outputFolder);
    return 0;
  }

  auto params = simu::Evaluator::Params::fromArgv(lhsTeamArg, rhsTeamArgs,
                                                  scenario, teamSize);
  Ind &ind = params.ind;

  auto start = simu::Simulation::now();
  const auto prevFitness = ind.fitnesses;
  const auto prevSignature = ind.signature;
  const auto prevInfos = ind.infos;
  const auto prevStats = ind.stats;

  simu::Evaluator eval;
//  eval.setLesionTypes(lesions);

  eval.logsSavePrefix = stdfs::path(outputFolder);
  eval.annTagsFile = annNeuralTags;
  eval(params);

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

  bool ok = true;
  if (!params.flags.any()) {
    auto oppsIds = params.opponentsIds();
    std::cout << prevInfos << " =?= " << oppsIds << "\n";
    if (prevInfos == oppsIds) {
      std::cout << "Rhs id(s) matching lhs memory. Comparison result for lhs:\n"
                << "\tfitness(es):\n";
      ok &= diffStatsMap(prevFitness, ind.fitnesses);
      std::cout << "\tStats:\n";
      ok &= diffStatsMap(prevStats, ind.stats);
      std::cout << "\tFootprint:\n";
      ok &= diffStatsArray(prevSignature, ind.signature,
                           simu::Evaluator::footprintFields(rhsTeamArgs.size()));

    } else {
      std::cout << "Result for lhs:\n";
      diffStatsMap({}, ind.fitnesses);
    }
  }

//  s.destroy();
  return ok ? 0 : 42;
}
