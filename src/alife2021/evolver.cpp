#include <csignal>

#include "scenario.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"

std::atomic<bool> aborted = false;
void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  aborted = true;
}

long maybeSeed(const std::string& s) {
  if (s.empty())  return -2;
  if (std::find_if(
        s.begin(),
        s.end(),
        [](unsigned char c) { return !std::isdigit(c) && c != '-'; }
      ) != s.end()) return -2;

  char* p;
  long l = strtol(s.c_str(), &p, 10);
  return p? l : -2;
}

int main(int argc, char *argv[]) {
  using CGenome = genotype::Critter;
  using EGenome = genotype::Environment;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

  simu::Simulation::InitData idata = simu::Scenario::commonInitData;

  std::string eGenomeArg;
  std::vector<std::string> cGenomeArgs;

  std::string outputFolder = "tmppp-evo";
  char overwrite = simu::Simulation::ABORT;

  uint generations = 1000;

  cxxopts::Options options("Splinoids (pp-evolver)",
                           "Evolution of minimal splinoids in 2D simulations"
                           " with enforced prey/predator interactions");
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

    ("env-genome", "Environment's genome", cxxopts::value(eGenomeArg))
    ("spln-genome", "Splinoid genome to start from or a random seed. Use "
                    "multiple times to define multiple sub-populations",
     cxxopts::value(cGenomeArgs))

    ("generations", "Number of generations to let the evolution run for",
     cxxopts::value(generations))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";


//  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
//  if (configFile.empty()) config::Simulation::printConfig("");

//  std::cout << "Reading environment genome from input file '"
//            << eGenomeArg << "'" << std::endl;
//  eGenome = EGenome::fromFile(eGenomeArg);

//  if (cGenomeArgs.empty())  cGenomeArgs.push_back("-1");
//  for (const auto arg: cGenomeArgs) {
//    long cGenomeSeed = maybeSeed(arg);
//    if (cGenomeSeed < -1) {
//      std::cout << "Reading splinoid genome from input file '"
//                << arg << "'" << std::endl;
//      cGenomes.push_back(CGenome::fromFile(arg));

//    } else {
//      rng::FastDice dice;
//      if (cGenomeSeed >= 0) dice.reset(cGenomeSeed);
//      std::cout << "Generating splinoid genome from rng seed "
//                << dice.getSeed() << std::endl;
//      CGenome g = CGenome::random(dice);
//      for (uint i=0; i<cGenomeMutations; i++) g.mutate(dice);
//      cGenomes.push_back(g);
//    }
//  }

//  std::cout << "Environment:\n" << eGenome
//            << "\nSplinoid";
//  if (cGenomes.size() > 1) std::cout << "s";
//  std::cout << ":\n";
//  for (const CGenome &g: cGenomes)
//    std::cout << g;
//  std::cout << std::endl;

//  // ===========================================================================
//  // == SIGINT management

//  struct sigaction act = {};
//  act.sa_handler = &sigint_manager;
//  if (0 != sigaction(SIGINT, &act, nullptr))
//    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
//  if (0 != sigaction(SIGTERM, &act, nullptr))
//    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

//  // ===========================================================================
//  // == GAGA setup


//  genotype::Critter::printMutationRates(std::cout, 2);
////  if (!outputFolder.empty())
////    s.setWorkPath(outputFolder, simu::Simulation::Overwrite(overwrite));
//  simu::Simulation::printStaticStats();

//  auto duration = simu::Simulation::durationFrom(start);
//  uint days, hours, minutes, seconds, mseconds;
//  mseconds = duration % 1000;
//  duration /= 1000;
//  seconds = duration % 60;
//  duration /= 60;
//  minutes = duration % 60;
//  duration /= 60;
//  hours = duration % 24;
//  days = duration / 24;

//  std::cout << "### Simulation ";
//  if (s.extinct())  std::cout << "failed";
//  else              std::cout << "completed";
//  std::cout << " at step " << s.currTime().pretty()
//            << "; Wall time of ";
//  if (days > 0)
//    std::cout << days << " days ";
//  if (days > 0 || hours > 0)
//    std::cout << hours << "h ";
//  if (days > 0 || hours > 0 || minutes > 0)
//    std::cout << minutes << "min ";
//  if (days > 0 || hours > 0 || minutes > 0 || seconds > 0)
//    std::cout << seconds << "s ";

//  std::cout << mseconds << "ms" << std::endl;

////  s.destroy();
  return 0;
}
