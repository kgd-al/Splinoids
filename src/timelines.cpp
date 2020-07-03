#include <csignal>

#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

#include "omp.h"

//#include "config/dependencies.h"

/// TODO Move constructor for Simulation
/// TODO Swap for Simulation/Environment
/// TODO Clone for Simulation
/// TODO environmental variation (RNG seed only)
/// TODO Local data folder (instead of program-wide cd)

std::atomic<bool> aborted = false;
void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation(s) "
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
  int edensSeed = 0;      // Seed for EDEnS-related RNG rolls

  stdfs::path workFolder = "tmp/timelines_test";

  uint currEpoch = 0;
};

template <typename ...ARGS>
std::string cpp_printf(const std::string &format, ARGS... args) {
//  auto size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
//  char *buffer = new char [size];
//  std::snprintf(buffer, size, format.c_str(), args...);
//  std::string s (buffer);
//  delete[] buffer;
//  return s;
  auto size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
  std::string result (size, '\0');
  std::snprintf(&result[0], size, format.c_str(), args...);
  return result;
}

using Simulation = simu::Simulation;
struct Alternative {
  uint index;

  static const int fitnessLowerBound = -10;
  struct FitnessData {
    static std::array<float, 3> weights;

    std::array<uint, 2> counts;
    std::array<float, 2> means, variances, deviations;
    float value;

    void zero (void) {
      counts.fill(0);
      means.fill(0);
      variances.fill(0);
      deviations.fill(0);
      value = -fitnessLowerBound-1;
    }

    void update (const std::set<simu::Critter*> &pop, uint minSize) {
      zero();

      std::array<float, 2> plants {0}, corpses {0};
      for (const simu::Critter *c: pop) {
        if (std::isnan(c->carnivorousBehavior())) continue;
        uint i = c->userIndex;
        const auto& fs = c->feedingSources();
        plants[i] += fs[simu::BodyType::PLANT];
        corpses[i] += fs[simu::BodyType::CORPSE];
        counts[i]++;
//        std::cerr << simu::CID(c)
//                  << cpp_printf(" (%d, %03d) %g %g %g%%\n",
//                                i, counts[i],
//                                fs[simu::BodyType::PLANT],
//                                fs[simu::BodyType::CORPSE],
//          100 * fs[simu::BodyType::CORPSE] /
//            (  fs[simu::BodyType::PLANT]
//             + fs[simu::BodyType::CORPSE]));
      }
      for (uint i=0; i<2; i++) {
        means[i] = corpses[i] / ((corpses[i]+plants[i]) * float(counts[i]));
//        std::cerr << cpp_printf("Mean[%d] = %g = %g / (%g + %g)\n",
//                                i, means[i], corpses[i], corpses[i], plants[i]);
      }


      for (const simu::Critter *c: pop) {
        uint i = c->userIndex;
        float cb = c->carnivorousBehavior();
        if (std::isnan(cb)) continue;
        variances[i] += (means[i] - cb) * (means[i] - cb);
      }

      for (uint i=0; i<2; i++) {
        variances[i] /= float(counts[i]);
        deviations[i] = std::sqrt(variances[i]);
      }

      // Compute fitness
      if (!std::isnan(means[0]) && !std::isnan(means[1]))
        value = std::max(means[0], means[1]);
      else if (std::isnan(means[0]) && std::isnan(means[1]))
        value = fitnessLowerBound;
      else if (std::isnan(means[0]))
        value = means[1];
      else if (std::isnan(means[1]))
        value = means[0];

//      value = weights[0] * std::fabs(means[0] - means[1])   // [0,1]
//            - weights[1] * deviations[0]                    // [0,.5]
//            - weights[2] * deviations[1];                   // [0,.5]

      // Apply penalties
      for (uint i=0; i<2; i++)
        if (counts[i] < minSize)
          value -= 2 - float(counts[i]) / minSize;
    }

    float operator() (void) const {
      return value;
    }

    struct FHeader {
      friend std::ostream& operator<< (std::ostream &os, const FHeader&) {
        return os << cpp_printf(" %3s %7s %7s %3s %7s %7s %10s",
                                "C0", "M0", "D0", "C1", "M1", "D1", "F");
      }
    };
    static const FHeader header (void) {
      return FHeader();
    }

    friend std::ostream& operator<< (std::ostream &os, const FitnessData &d) {
      for (uint i=0; i<2; i++)
        os << cpp_printf(" %03d %.1e %.1e", d.counts[i],
                         d.means[i], d.deviations[i]);
      os << cpp_printf(" %+.3e", d.value);
      return os;
    }

  } fitness;
  std::map<uint, FitnessData> fitnessHistory;

  Simulation simulation;

  Alternative (uint i) : index(i) {
    fitness.zero();
  }

  void clearFitnessHistory (void) {
    fitnessHistory.clear();
  }

  void updateFitness (uint minPopSize) {
    fitness.update(simulation.critters(), minPopSize);
    fitnessHistory[simulation.minGeneration()] = fitness;
  }

  friend bool operator< (const Alternative &lhs, const Alternative &rhs) {
    return lhs.fitness() < rhs.fitness();
  }
};
std::array<float, 3> Alternative::FitnessData::weights {1};

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
  int parallel = -1;

  std::string fitnessWeightsArg;

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

    ("parallel", "Maximum number of concurrent threads",
     cxxopts::value(parallel))
    ("branching", "Number of alternatives", cxxopts::value(params.branching))
    ("epoch-length", "Number of generations per epoch",
     cxxopts::value(params.epochLength))
    ("epochs", "Number of epochs", cxxopts::value(params.epochsCount))
    ("edens-seed", "Seed for EDEnS-related rolls",
     cxxopts::value(params.edensSeed))

    ("fitness-weights", "Weights as a single space-delimited argument",
     cxxopts::value(fitnessWeightsArg))
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

  stdfs::path rfolder = params.workFolder / "results";
  if (stdfs::exists(rfolder) && !stdfs::is_empty(rfolder)) {
    std::cerr << "Base folder is not empty!\n";
    switch (overwrite) {
    case simu::Simulation::Overwrite::PURGE:
      std::cerr << "Purging" << std::endl;
      stdfs::remove_all(rfolder);
      break;

    case simu::Simulation::Overwrite::ABORT:
    default:
      std::cerr << "Aborting" << std::endl;
      exit(1);
      break;
    }
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  config::Simulation::verbosity.overrideWith(0);
  config::Simulation::setupConfig(configFile, verbosity);
  if (configFile.empty()) config::Simulation::printConfig("");

//  config::Simulation::printConfig();

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
  const uint gdigits = digits(params.epochLength * params.epochsCount);

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

  if (parallel == -1) parallel = omp_get_num_procs();
  omp_set_num_threads(parallel);

  auto start = simu::Simulation::now();
  std::cout << "Staring timelines exploration for " << params.epochsCount
            << " epochs of " << params.epochLength << " generations each using "
            << parallel << " cores" << std::endl;

  if (!fitnessWeightsArg.empty()) {
    std::istringstream iss (fitnessWeightsArg);
    auto &w = Alternative::FitnessData::weights;
    iss >> w[0] >> w[1] >> w[2];
    for (float f: w)
      if (f < 0 || 10 < f)
        utils::doThrow<std::invalid_argument>(
          "Fitness weights '", fitnessWeightsArg, "' are invalid: error on ",
          f);
  }

  rng::FastDice dice (params.edensSeed);

  Population alternatives;
  alternatives.reserve(params.branching);
  for (uint i=0; i<params.branching; i++) alternatives.emplace_back(i);

  Alternative *reality = &alternatives.front();
  uint winner = 0;

  // To sort after an epoch
  SortedIndices aindices (params.branching);

  reality->simulation.init(eGenome, cGenomes, idata);
  config::Simulation::screwTheEntropy.ref() = false;

  // ===========================================================================
  // == EDEnS

  uint minPopSize = .25 * idata.nCritters;
  do {
    epochHeader(params.currEpoch);

    // Populate next epoch from current best alternative
    #pragma omp parallel for schedule(dynamic)
    for (uint a=0; a<params.branching; a++) {
      Simulation &s = alternatives[a].simulation;
      if (winner != a)  s.clone(reality->simulation);
      if (a > 0) s.mutateEnvController(dice);

      // Prepare data folder and set durations
      s.setWorkPath(alternativeDataFolder(params.currEpoch, a),
                    Simulation::Overwrite::ABORT);
      s.setGenerationGoal(params.epochLength, Simulation::GGoalModifier::ADD);
    }

    // Join here to ensure all copies have been made

    // Execute alternative simulations in parallel
    #pragma omp parallel for schedule(dynamic)
    for (uint ia=0; ia<params.branching; ia++) {
      Alternative &a = alternatives[ia];
      Simulation &s = a.simulation;

      a.clearFitnessHistory();

      uint minGen = s.minGeneration();
      while (!s.finished() && !aborted) {
        s.step();
        if (minGen < s.minGeneration()) {
          minGen = s.minGeneration();
          a.updateFitness(minPopSize);
          s.save();
        }
      }

      a.updateFitness(minPopSize);

      std::ofstream ofs (s.workPath() / "fitnesses.dat");
      ofs << "G" << Alternative::FitnessData::header() << "\n";
      for (const auto &p: a.fitnessHistory)
        ofs << p.first << p.second << "\n";
    }

    for (uint i=0; i<aindices.size(); i++)  aindices[i] = i;
    std::sort(aindices.begin(), aindices.end(),
              [&alternatives] (uint i, uint j) {
      return alternatives[i].fitness() > alternatives[j].fitness();
    });
    winner = aindices.front();
    reality = &alternatives[winner];

//    logFitnesses(params.currEpoch, winner);

    // Store result accordingly
    stdfs::create_directory_symlink(reality->simulation.workPath().filename(),
                                    championDataFolder(params.currEpoch));

    // Print summary
    std::cout << "# Alternatives:\n";
    std::cout << "# R I " << std::setw(gdigits) << "G" << " "
              << Alternative::FitnessData::header() << "\n";
    for (uint i=0; i<aindices.size(); i++) {
      const Alternative &a = alternatives[aindices[i]];
      std::cout << "# " << std::setw(adigits) << i
                << " " << std::setw(adigits) << a.index
                << " " << std::setw(gdigits) << a.simulation.minGeneration()
                << " " << a.fitness
                << "\n";
    }

    if (reality->simulation.extinct()
        || reality->fitness() < Alternative::fitnessLowerBound) {
      reality = nullptr;
      break;
    }

    params.currEpoch++;

  } while (params.currEpoch < params.epochsCount && !aborted);

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
  if (!reality) std::cout << "failed at epoch " << params.currEpoch;
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
