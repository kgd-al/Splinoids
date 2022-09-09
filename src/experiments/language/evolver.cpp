#include <csignal>

#include "indevaluator.h"
#include "kgd/external/cxxopts.hpp"

void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  simu::Evaluator::aborted = true;
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

void symlink_as_last (const stdfs::path &path) {
  stdfs::path link = path.parent_path() / "last";
  if (stdfs::exists(stdfs::symlink_status(link)))
    stdfs::remove(link);
  stdfs::create_directory_symlink(stdfs::canonical(path), link);
  std::cout << "Created " << link << " -> " << path << "\n";
}

template <typename D = std::chrono::milliseconds>
auto timestamp (void) {
  using namespace std::chrono;
  return duration_cast<D>(system_clock::now().time_since_epoch()).count();
}

void overrideMutationRates (void) {
  using Genome = simu::Evaluator::Genome;
  auto cm = Genome::config_t::mutationRates();
  for (auto &p: cm) p.second = 0;
  cm["brain"] = 1;
  utils::normalize(cm);
  Genome::config_t::mutationRates.overrideWith(cm);
}

void overrideGeneticFields (simu::Evaluator::Genome &g) {
  g.colors = {utils::uniformStdArray<simu::Color>(.5)};
  g.vision.angleBody = M_PI / 6.f;
  g.vision.angleRelative = -M_PI / 6.f;
  g.vision.width = M_PI / 2.;
  g.vision.precision = 3;
  g.minClockSpeed = 1;
  g.maxClockSpeed = 1;
  g.matureAge = .33;
  g.oldAge = .66;
}

void calibrateEnergyCosts (void) {
  static constexpr auto D = simu::Scenario::DURATION;
  static constexpr auto E = simu::Critter::maximalEnergyStorage(
                              simu::Critter::MAX_SIZE);
//  static constexpr auto TGT = E * .1; // % of total energy
  using C = config::Simulation;
  using decimal = C::decimal;

  static const auto round = [] (decimal f) {
    std::stringstream ss;
    ss << f;

    decimal f_;
    ss >> f_;
    return f_;
  };

  // Consume TARGET if both motors are continuously active at max speed
  auto me = round(.25 * E / (2 * D));
  C::motorEnergyConsumption.overrideWith(me);
  std::cerr << "motorEnergyConsumption = "
            << C::motorEnergyConsumption()
            << " = .25E / (2 * " << D << ")\n";

  // Consume TARGET if speaking continuously at max volume
  auto ve = round(.5 * E / D);
  C::voiceEnergyConsumption.overrideWith(ve);
  std::cerr << "voiceEnergyConsumption = "
            << C::voiceEnergyConsumption()
            << " = .5E / " << 2*D << "\n";

  // Consume TARGET if having ~maximal amount of neurons all active throughout
  const auto maxNeurons = std::pow(8, config::EvolvableSubstrate::maxDepth());
  auto ne = round(.125 * E / (maxNeurons * D));
  C::neuronEnergyConsumption.overrideWith(ne);
  std::cerr << "neuronEnergyConsumption = "
            << C::neuronEnergyConsumption()
            << " = .125E / (" << maxNeurons << " * " << D << ")\n";

  // Consume TARGET if all neurons are interconnected with length sqrt(2)
  auto ae = round(.125 * E / (maxNeurons * maxNeurons * sqrt(2) * D));
  C::axonEnergyCost.overrideWith(ae);
  std::cerr << "axonEnergyCost = "
            << C::axonEnergyCost()
            << " = .125E / (" << maxNeurons << "^2 * sqrt(2) * "
              << D << ")\n";
}

auto &apoget_force_link = config::PTree::rsetSize;
int main(int argc, char *argv[]) {
//  using Ind = simu::Evaluator::Ind;
  using Genome = simu::Evaluator::Genome;  
  using EvalType = simu::Scenario::Type;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;
  int gagaVerbosity = 1;
  bool novelty = true;

//  std::string load;

  std::string outputFolder = "tmp/lg-evo/";
  char overwrite = simu::Simulation::ABORT;

  std::string evalTypeStr = "ERROR";
  uint popSize = 5, generations = 1;
  uint threads = 1;
  long seed = -1;

  int gagaSavePopulations = 0;

  auto id = timestamp();

  cxxopts::Options options("Splinoids (lg-evolver)",
                           "Evolution of talking splinoids");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))
    ("gaga-verbosity", "GAGA verbosity level. ",
     cxxopts::value(gagaVerbosity))
    ("no-novelty", "Disable novelty fitness")

    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

//    ("load", "Restart evolution from a saved population",
//     cxxopts::value(load))

    ("seed", "RNG seed for the evolution (except the GAGA part)",
     cxxopts::value(seed))
    ("population", "Population size", cxxopts::value(popSize))
    ("generations", "Number of generations to let the evolution run for",
     cxxopts::value(generations))
    ("threads", "Number of parallel threads", cxxopts::value(threads))
    ("id", "Run identificator (used to uniquely identify the output folder, "
           "defaults to the current timestamp)",
     cxxopts::value(id))
    ("type", "Type of evaluation. Valid values are "
     + simu::Evaluator::prettyEvalTypes(), cxxopts::value(evalTypeStr))

    ("save-populations",
     "Whether to save populations after evaluation (1+) and maybe before (2) "
     "or just the last (-1)",
     cxxopts::value(gagaSavePopulations))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  novelty = (!result.count("no-novelty"));

  EvalType evalType;
  {
    using utils::operator<<;
    std::istringstream iss (evalTypeStr);
    iss >> evalType;
    if (!iss)
      utils::Thrower("Provided eval type '", evalTypeStr,
        "' is not a valid value. See the help for more information");
  }

  stdfs::path dataFolder = stdfs::weakly_canonical(outputFolder);
#ifndef CLUSTER_BUILD
  dataFolder /= "ID" + std::to_string(id);
  std::cerr << "provided id: " << id << " of type "
            << utils::className<decltype(id)>() << ". ";
#else
  std::cerr << "[CLUSTER BUILD] Ignoring id. ";
#endif
  std::cerr << "Using " << dataFolder << " as data folder.\n";

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  std::string configFileAbsolute = stdfs::canonical(configFile).string();
  if (!stdfs::exists(configFile))
    utils::Thrower("Failed to find Simulation.config at ", configFileAbsolute);
  if (!config::Simulation::readConfig(configFile))
    utils::Thrower("Error while parsing config file ", configFileAbsolute,
                   " or dependency");

  { // Only the brain can mutate
    config::Simulation::verbosity.overrideWith(0);
    overrideMutationRates();
  }
  calibrateEnergyCosts(); // Set stimulating energy costs
  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
  config::Simulation::printConfig(stdfs::path(dataFolder) / "configs");


  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::Thrower<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::Thrower<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == Arguments summary

  std::cout << "\nArguments:\n"
            << "\t  Config file: " << configFileAbsolute << "\n"
            << "\t           ID: " << id << "\n"
            << "\tOutput folder: " << dataFolder << "\n"
            << "\t     RNG seed: ";

  rng::AtomicDice dice;
  if (seed >= 0) {
    dice.reset(seed);
    std::cout << seed << " -> ";
  }
  std::cout << dice.getSeed() << "\n";

  std::cout
    << "\t  CPU Threads: " << threads << "\n"
    << "\t   Population: " << popSize << "\n"
    << "\t  Generations: " << generations << "\n"
    << "---\t---\n\n";

  genotype::Critter::printMutationRates(std::cout, 2);

  std::cout << "---\t---\n";
#define C(X) "\t" << #X << ": " << config::Simulation::X() << "\n"
  std::cout << "Calibrated energy costs:\n"
            << C(motorEnergyConsumption)
            << C(voiceEnergyConsumption)
            << C(neuronEnergyConsumption)
            << C(axonEnergyCost);
#undef C

  std::cout << "\n";

  // ===========================================================================
  // == GA setup

  phylogeny::GIDManager gidManager;
  simu::Evaluator eval (evalType);

  using GA = simu::Evaluator::GA;

  GA ga;
  ga.setPopSize(popSize);
  ga.setNbThreads(threads);
  ga.setMutationRate(1);
  ga.setCrossoverRate(0);
  ga.setSelectionMethod(GAGA::SelectionMethod::paretoTournament);
  ga.setTournamentSize(4);
  ga.setNbElites(1);
  ga.setEvaluateAllIndividuals(false);
  ga.setVerbosity(gagaVerbosity);
//  ga.setSaveFolder(dataFolder);
  ga.setNbSavedElites(ga.getNbElites());
  ga.setSaveGenStats(true);
  ga.setSaveIndStats(true);
  ga.setSaveParetoFront(false);

  ga.disableGenerationHistory();
  if (gagaSavePopulations <= 0) ga.disablePopulationSave();

  ga.setSaveFolderGenerator([dataFolder] (auto) { return dataFolder; });

  ga.setNewGenerationFunction([&ga, gagaSavePopulations] {
    std::cout << "\nNew generation at " << utils::CurrentTime{} << "\n";

    auto gen = ga.getCurrentGenerationNumber();
#ifndef CLUSTER_BUILD
    if (gen == 0) symlink_as_last(ga.getSaveFolder());
#endif

    if (gagaSavePopulations >= 2 // save alot
        || gagaSavePopulations == -1 /*keep pop before evaluation*/) {
      std::cout << "Also saving population before evaluation"
                   " (just in case)\n";
      ga.savePop(ga.population);
    }
  });

  ga.setMutateMethod([&dice, &gidManager](Genome &g) {
    g.mutate(dice);
    g.gdata.updateAfterCloning(gidManager);
  });

//    ga.setCrossoverMethod([](const CGenome&, const CGenome&)
//                          -> CGenome {assert(false);});

  ga.setEvaluator([&eval] (auto &i, auto /*p*/) { eval(i); }, "language");

// -- -- -- -- -- -- SPECIFIC TO THE NOVELTY EXTENSION: -- -- -- -- -- -- --
  GAGA::NoveltyExtension<GA> nov;  // novelty extension instance
  // Distance function (compares 2 signatures). Here a simple Euclidian distance.
  auto euclidianDist = [](const auto& fpA, const auto& fpB) {
      double sum = 0;
      for (size_t i = 0; i < fpA.size(); ++i)
        sum += std::pow(fpA[i] - fpB[i], 2);
      return sqrt(sum);
  };
  nov.setComputeSignatureDistanceFunction(euclidianDist);
  nov.K = 10;  // size of the neighbourhood to compute novelty.
  //(Novelty = avg dist to the K Nearest Neighbors)
  nov.saveArchiveEnabled = false;

  if (novelty)
    ga.useExtension(nov);  // we have to tell gaga we want to use this extension
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

  static_assert(Genome::SPLINES_COUNT == 0,
    "Not expecting splines in this experiment");

//  if (load.empty()) {
    ga.initPopulation([&dice, &gidManager] {
      auto g = Genome::random(dice);
      overrideGeneticFields(g);
      g.gdata.setAsPrimordial(gidManager);
      return g;
    });
    std::cout << "Generating random population\n";

//  } else {
//    // copied from gaga to use appropriate constuctor
//    std::ifstream t(load);
//    std::stringstream buffer;
//    buffer << t.rdbuf();
//    auto o = nlohmann::json::parse(buffer.str());
//    assert(o.count("population"));

//    std::vector<Ind> pop;
//    for (auto ind : o.at("population")) {
//        pop.push_back(Ind(Team(ind.at("dna").get<std::string>())));
//        pop.back().evaluated = false;
//    }

//    ga.setPopulation(pop);
//    std::cout << "Loaded population from " << load << ", " << pop.size()
//              << " individuals\n";
//  }

//  // ===========================================================================
//  // == Launch

  auto start = simu::Simulation::now();
  std::ofstream genealogy;

  for (uint i=0; i<generations && !simu::Evaluator::aborted; i++) {
    if (i == generations-1) nov.saveArchiveEnabled = true;

    ga.step();

    if (gagaSavePopulations == -1 && i > 0) {
      stdfs::path previousPop = ga.getSaveFolder();
      previousPop /= utils::mergeToString("gen", i-1);
      previousPop /= utils::mergeToString("pop", i-1, ".pop");
      std::cerr << "Removing pre-evolution cached population\n";
      stdfs::remove(previousPop);
    }

    if (i == 0) {
      genealogy.open(ga.getSaveFolder() / "genealogy.dat");
      if (!genealogy)
        utils::Thrower<std::logic_error>(
          "Failed to create genealogy file '",
          ga.getSaveFolder(), "/genealogy.dat'");
      genealogy << "Gen Ind_i Ind_GID Parent_i Parent_GID Score\n";
    }

    for (const GA::Ind_t &ind: ga.previousGenerations.back()) {
      genealogy << i << " " << ind.id.second << " "
                << ind.dna.gdata.self.gid << " ";
      if (i == 0)
        genealogy << "0 -1 ";
      else
        genealogy << ind.parents.front().second << " "
                  << ind.dna.gdata.mother.gid << " ";
      genealogy << ind.fitnesses.at("lg") << "\n";
    }
  }

  stdfs::create_directory_symlink(
    GAGA::concat("gen", ga.getCurrentGenerationNumber()-1),
    ga.getSaveFolder() / "gen_last");

  genealogy.close();


  // ===========================================================================
  // == Post-evolution

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

  std::cout << "### Evolution ";
  if (simu::Evaluator::aborted)
    std::cout << "aborted after ";
  else
    std::cout << "completed in ";
  if (days > 0)
    std::cout << days << " days ";
  if (days > 0 || hours > 0)
    std::cout << hours << "h ";
  if (days > 0 || hours > 0 || minutes > 0)
    std::cout << minutes << "min ";
  if (days > 0 || hours > 0 || minutes > 0 || seconds > 0)
    std::cout << seconds << "s ";

  std::cout << mseconds << "ms" << std::endl;

  return 0;
}
