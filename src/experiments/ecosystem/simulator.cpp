#include <csignal>

#include "../../simu/simulation.h"

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

  simu::Simulation::InitData idata {};
  idata.ienergy = 1000;
  idata.cRatio = 1. / (1+config::Simulation::plantEnergyDensity());
  idata.nCritters = 50;
  idata.cRange = .25;
  idata.pRange = 1;
  idata.seed = -1;

  int taurus = -1;
  std::string eGenomeArg = "-1";
  std::vector<std::string> cGenomeArgs;

  genotype::Environment eGenome;
  std::vector<genotype::Critter> cGenomes;
  uint cGenomeMutations = 0;

  std::string loadSaveFile, loadConstraints, loadFields;

//  std::string duration = "=100";
  std::string outputFolder = "tmp_simu_run";
  char overwrite = simu::Simulation::UNSPECIFIED;

  uint generations = 1000;

  cxxopts::Options options("Splinoids (headless)",
                           "2D simulation of critters in a changing environment");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

//    ("d,duration", "Simulation duration. ",
//     cxxopts::value(duration))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

    ("e,energy", "Total energy of the system", cxxopts::value(idata.ienergy))
    ("s,seed", "Seed value for simulation's RNG", cxxopts::value(idata.seed))
    ("n,i-splinoids", "Initial number of splinoids",
     cxxopts::value(idata.nCritters))
    ("i-prange", "Region in which initial plants are placed",
     cxxopts::value(idata.pRange))
    ("i-crange", "Region in which initial splinoids are placed",
     cxxopts::value(idata.cRange))
    ("i-sratio", "Initial fraction of energy devoted to the splinoids",
     cxxopts::value(idata.cRatio))
    ("scenario", "Additional actions to be performed (inline json or file)",
     cxxopts::value(idata.scenario))

    ("env-genome", "Environment's genome or a random seed",
     cxxopts::value(eGenomeArg))
    ("spln-genome", "Splinoid genome to start from or a random seed. Use "
                    "multiple times to define multiple sub-populations",
     cxxopts::value(cGenomeArgs))
    ("spln-mutations", "Additional mutations applied to the original genome",
     cxxopts::value(cGenomeMutations))

    ("taurus", "Whether the environment is a taurus or uses fixed boundaries",
      cxxopts::value(taurus))

    ("generations", "Number of generations to let the simulation run for",
     cxxopts::value(generations))

//    ("l,load", "Load a previously saved simulation",
//     cxxopts::value(loadSaveFile))
//    ("load-constraints", "Constraints to apply on dependencies check",
//     cxxopts::value(loadConstraints))
//    ("load-fields", "Individual fields to load",
//     cxxopts::value(loadFields))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
//              << "\n\nOption 'auto-config' is the default and overrides 'config'"
//                 " if both are provided"
//              << "\nEither both 'plant' and 'environment' options are used or "
//                 "a valid file is to be provided to 'load' (the former has "
//                 "precedance in case all three options are specified)"
//              << "\n\n" << config::Dependencies::Help{}
//              << "\n\n" << simu::Simulation::LoadHelp{}
              << std::endl;
    return 0;
  }

//  bool missingArgument = (!result.count("environment") || !result.count("plant"))
//      && !result.count("load");

//  if (missingArgument) {
//    if (result.count("environment"))
//      utils::Thrower("No value provided for the plant's genome");

//    else if (result.count("plant"))
//      utils::Thrower("No value provided for the environment's genome");

//    else
//      utils::Thrower(
//        "No starting state provided. Either provide both an environment and plant genomes"
//        " or load a previous simulation");
//  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  if (loadSaveFile.empty()) {
//    config::Simulation::setupConfig(configFile, verbosity);
    if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
    if (configFile.empty()) config::Simulation::printConfig("");

    long eGenomeSeed = maybeSeed(eGenomeArg);
    if (eGenomeSeed < -1) {
      std::cout << "Reading environment genome from input file '"
                << eGenomeArg << "'" << std::endl;
      eGenome = EGenome::fromFile(eGenomeArg);

    } else {
      rng::FastDice dice;
      if (eGenomeSeed >= 0) dice.reset(eGenomeSeed);
      std::cout << "Generating environment genome from rng seed "
                << dice.getSeed() << std::endl;
      eGenome = EGenome::random(dice);
    }
    if (taurus != -1) eGenome.taurus = taurus;
    eGenome.toFile("last", 2);

    if (cGenomeArgs.empty())  cGenomeArgs.push_back("-1");
    for (const auto arg: cGenomeArgs) {
      long cGenomeSeed = maybeSeed(arg);
      if (cGenomeSeed < -1) {
        std::cout << "Reading splinoid genome from input file '"
                  << arg << "'" << std::endl;
        cGenomes.push_back(CGenome::fromFile(arg));

      } else {
        rng::FastDice dice;
        if (cGenomeSeed >= 0) dice.reset(cGenomeSeed);
        std::cout << "Generating splinoid genome from rng seed "
                  << dice.getSeed() << std::endl;
        CGenome g = CGenome::random(dice);
        for (uint i=0; i<cGenomeMutations; i++) g.mutate(dice);
        cGenomes.push_back(g);
      }
    }

//      cGenome.toFile("last", 2);

    std::cout << "Environment:\n" << eGenome
              << "\nSplinoid";
    if (cGenomes.size() > 1) std::cout << "s";
    std::cout << ":\n";
    for (const CGenome &g: cGenomes)
      std::cout << g;
    std::cout << std::endl;
  }

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

  simu::Simulation s;

  if (!loadSaveFile.empty()) {  // Full load init
//    simu::Simulation::load(loadSaveFile, s, loadConstraints, loadFields);
//    if (verbosity != Verbosity::QUIET)  config::Simulation::printConfig();

  } else {  // Regular init
    s.init(eGenome, cGenomes, idata);
//    s.periodicSave(); /// FIXME Carefull with this things might, surprisingly,
    /// not be all that initialized
  }

  if (!outputFolder.empty())
    s.setWorkPath(outputFolder, simu::Simulation::Overwrite(overwrite));

  genotype::Critter::printMutationRates(std::cout, 2);

//  {
//    std::ofstream ofs (s.dataFolder() / "controller.last.tex");
//    ofs << "\\documentclass[preview]{standalone}\n"
//           "\\usepackage{amsmath}\n"
//           "\\begin{document}\n"
//        << envGenome.controller.toTex()
//        << "\\end{document}\n";
//    ofs.close();

//    envGenome.controller.toDot(s.dataFolder() / "controller.last.dot",
//                               genotype::Environment::CGP::DotOptions::SHOW_DATA);
//  }

//  if (!duration.empty()) {
//    if (duration.size() < 2)
//      utils::Thrower("Invalid duration '", duration, "'. [+|=]<years>");

//    uint dvalue = 0;
//    if (!(std::istringstream (duration.substr(1)) >> dvalue))
//      utils::Thrower("Failed to parse '", duration.substr(1), "' as a duration (uint)");

//    s.setDuration(simu::Environment::DurationSetType(duration[0]), dvalue);
//  }

  simu::Simulation::printStaticStats();

  const uint years = 1000;
  s.setGenerationGoal(generations, simu::Simulation::GGoalModifier::SET);

  auto start = simu::Simulation::now();
  std::cout << "Staring simulation for a max of " << generations
            << " generations or " << years << " years" << std::endl;

  uint saveEveryGen = 1, saveNextGen = 1;
  while (!s.finished()
         && s.currTime().year() < years
         && s.competitionStats().survivingPopulations() == cGenomes.size()) {
    if (aborted)  s.abort();
    s.step();

    if (saveNextGen <= s.minGeneration()) {
      s.save(s.periodicSaveName());
      saveNextGen = s.minGeneration() + saveEveryGen;

    } else if (s.currTime().isStartOfYear())
      s.save(s.periodicSaveName());
  }
  s.atEnd();
  s.save(s.periodicSaveName());

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

  std::cout << "### Simulation ";
  if (s.extinct())  std::cout << "failed";
  else              std::cout << "completed";
  std::cout << " at step " << s.currTime().pretty()
            << "; Wall time of ";
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
