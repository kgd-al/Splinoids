#include <csignal>

#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"

/// TODO Move constructor for Simulation
/// TODO Swap for Simulation/Environment
/// TODO Clone for Simulation
/// TODO environmental variation (RNG seed only)
/// TODO Local data folder (instead of program-wide cd)

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

struct EDEnSParameters {
  uint branching = 10;    // 10 parallel evaluations
  uint epochLength = 10;  // Generations per epoch
  uint epochsCount = 100; // Number of epochs
  int edensSeed = 0;         // Seed for EDEnS-related RNG rolls

  stdfs::path workFolder = "tmp/timelines_test";

  uint currEpoch = 0;
};

using Simulation = simu::Simulation;
struct Alternative {
  uint index;
  double fitness;

  Simulation simulation;

  Alternative (uint i) : index(i) {}

  void updateFitness (void) {
    fitness = -1;
  }

  friend bool operator< (const Alternative &lhs, const Alternative &rhs) {
    return lhs.fitness < rhs.fitness;
  }
};

using Population = std::vector<Alternative>;
using SortedIndices = std::vector<uint>;

uint digits (uint number) {
  return std::ceil(std::log10(number));
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
  idata.ienergy = 100;
  idata.cRatio = 1. / (1+config::Simulation::plantEnergyDensity());
  idata.nCritters = 50;
  idata.cRange = .25;
  idata.pRange = 1;
  idata.seed = 0;

  int taurus = -1;
  std::string eGenomeArg = "-1";
  std::vector<std::string> cGenomeArgs;

  genotype::Environment eGenome;
  std::vector<genotype::Critter> cGenomes;
  uint cGenomeMutations = 0;

  char overwrite = simu::Simulation::UNSPECIFIED;

  EDEnSParameters params {};

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
     cxxopts::value(params.workFolder))
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
    ("sratio", "Initial fraction of energy devoted to the splinoids",
     cxxopts::value(idata.cRatio))

    ("env-genome", "Environment's genome or a random seed",
     cxxopts::value(eGenomeArg))
    ("spln-genome", "Splinoid genome to start from or a random seed",
     cxxopts::value(cGenomeArgs))
    ("spln-mutations", "Additional mutations applied to the original genome",
     cxxopts::value(cGenomeMutations))

    ("taurus", "Whether the environment is a taurus or uses fixed boundaries",
      cxxopts::value(taurus))

    ("branching", "Number of alternatives", cxxopts::value(params.branching))
    ("epoch-length", "Number of generations per epoch",
     cxxopts::value(params.epochLength))
    ("epochs", "Number of epochs", cxxopts::value(params.epochsCount))
    ("edens-seed", "Seed for EDEnS-related rolls",
     cxxopts::value(params.edensSeed))
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
//      utils::doThrow<std::invalid_argument>("No value provided for the plant's genome");

//    else if (result.count("plant"))
//      utils::doThrow<std::invalid_argument>("No value provided for the environment's genome");

//    else
//      utils::doThrow<std::invalid_argument>(
//        "No starting state provided. Either provide both an environment and plant genomes"
//        " or load a previous simulation");
//  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

//  config::Simulation::setupConfig(configFile, verbosity);
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


  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == EDEnS helpers

  const uint edigits = digits(params.epochsCount);
  const uint adigits = digits(params.branching);

  auto alternativeDataFolder = [&params, edigits, adigits]
      (uint epoch, uint alternative) {
    std::ostringstream oss;
    oss << "results/e" << std::setfill('0')
        << std::setw(edigits) << epoch
        << "_a" << std::setw(adigits) << alternative;
    return params.workFolder / oss.str();
  };

  auto championDataFolder = [&params, edigits] (uint epoch) {
    std::ostringstream oss;
    oss << "results/e" << std::setfill('0')
        << std::setw(edigits) << epoch << "_r";
    return params.workFolder / oss.str();
  };

  auto epochHeader = [edigits] (uint epoch) {
    static const auto CTSIZE =  utils::CurrentTime::width();
    std::cout << std::string(8+edigits+4+CTSIZE+2, '#') << "\n"
              << "# Epoch " << std::setw(edigits) << epoch
              << " at " << utils::CurrentTime{} << " #"
              << std::endl;
  };


//  std::ofstream timelinesOFS (params.workFolder / "timelines.dat");
//  const auto logFitnesses =
//    [&timelinesOFS, &alternatives] (uint epoch, uint winner) {
//    std::ofstream &ofs = timelinesOFS;
//    auto N = alternatives.size();

//    if (epoch == 0) {
//      N = 1;
//      ofs << "E A W";
//      for (auto f: FUtils::iterator())
//        ofs << " " << FUtils::getName(f);
//      ofs << "\n";
//    }

//    for (uint i=0; i<N; i++) {
//      ofs << epoch << " " << i << " " << (i == winner);
//      for (const auto &f: alternatives[i].fitnesses)
//        ofs << " " << f;
//      ofs << "\n";
//    }

//    ofs.flush();
//  };

  // ===========================================================================
  // == EDEnS setup

  genotype::Critter::printMutationRates(std::cout, 2);

  simu::Simulation::printStaticStats();

  auto start = simu::Simulation::now();
  std::cout << "Staring timelines exploration for " << params.epochsCount
            << " epochs of " << params.epochLength << " generations each"
            << std::endl;

  rng::FastDice dice (params.edensSeed);

  Population alternatives;
  for (uint i=0; i<params.branching; i++) alternatives.emplace_back(i);

  Alternative *reality = &alternatives.front();
  uint winner = 0;
  uint nextGenThreshold = 0;

  // To sort after an epoch
  SortedIndices aindices (params.branching);

  reality->simulation.init(eGenome, cGenomes, idata);


  // ===========================================================================
  // == EDEnS

  do {
    epochHeader(params.currEpoch);

    // Populate next epoch from current best alternative
    if (winner != 0)  alternatives[0].simulation.clone(reality->simulation);
    #pragma omp parallel for schedule(dynamic)
    for (uint a=1; a<params.branching; a++) {
      if (winner != a)  alternatives[a].simulation.clone(reality->simulation);
      alternatives[a].simulation.mutateEnvController(dice);
    }

    // Prepare data folder and set durations
    nextGenThreshold = reality->simulation.minGeneration() + params.epochLength;
    for (uint a=0; a<params.branching; a++)
      alternatives[a]
          .simulation
          .setWorkPath(alternativeDataFolder(params.currEpoch, a),
                       Simulation::Overwrite::ABORT);


    // Execute alternative simulations in parallel
    #pragma omp parallel for schedule(dynamic)
    for (uint a=0; a<params.branching; a++) {
      Simulation &s = alternatives[a].simulation;

      while (!s.finished() && s.minGeneration() < nextGenThreshold) s.step();

      alternatives[a].updateFitness();
    }

    for (uint i=0; i<aindices.size(); i++)  aindices[i] = i;
    std::sort(aindices.begin(), aindices.end(),
              [&alternatives] (uint i, uint j) {
      return alternatives[i].fitness > alternatives[j].fitness;
    });
    winner = aindices.front();
    reality = &alternatives[winner];

//    logFitnesses(params.currEpoch, winner);

    // Store result accordingly
    stdfs::create_directory_symlink(reality->simulation.workPath().filename(),
                                    championDataFolder(params.currEpoch));

    // Print summary
    std::cout << "# Alternatives:\n";
    for (uint i=0; i<aindices.size(); i++) {
      const Alternative &a = alternatives[aindices[i]];
      std::cout << "# " << std::setw(adigits) << i
                << " " << std::setw(adigits) << a.index
                << " " << a.fitness << "\n";
    }

    if (reality->simulation.extinct()) {
      reality = nullptr;
      break;
    }

    params.currEpoch++;

  } while (params.currEpoch < params.epochsCount);

  // ===========================================================================
  // Pretty printing of duration
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

  std::cout << "### Timelines exploration ";
  if (!reality) std::cout << "failed";
  else          std::cout << "completed";
  std::cout << "; Wall time of ";
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
