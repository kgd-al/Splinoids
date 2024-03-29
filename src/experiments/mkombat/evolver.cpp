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

auto &apoget_force_link = config::PTree::rsetSize;
int main(int argc, char *argv[]) {
//  using CGenome = ga::CoEvolution::Genome;
  using Ind = simu::Evaluator::Ind;
  using Team = simu::Evaluator::Genome;
  static_assert(std::is_same<Team, simu::Team>::value, "What!?");

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;
  int gagaVerbosity = 1;
  bool novelty = true;

//  std::string load;

  std::string outputFolder = "tmp/mk-evo/";
  char overwrite = simu::Simulation::ABORT;

  uint teamSize = 1, popSize = 5, generations = 1;
  uint threads = 1;
  long seed = -1;

  int gagaSavePopulations = 0;

  uint populations = 2;

  auto id = timestamp();

  cxxopts::Options options("Splinoids (mk-evolver)",
                           "Evolution of simplified splinoids in Mortal Kombat"
                           " conditions");
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
    ("team-size", "Size of each teams", cxxopts::value(teamSize))
    ("team-count", "Number of teams", cxxopts::value(popSize))
    ("generations", "Number of generations to let the evolution run for",
     cxxopts::value(generations))
    ("populations", "Number of concurrent populations (>1)",
     cxxopts::value(populations))
    ("threads", "Number of parallel threads", cxxopts::value(threads))
    ("id", "Run identificator (used to uniquely identify the output folder, "
           "defaults to the current timestamp)",
     cxxopts::value(id))

    ("save-populations",
     "Whether to save populations after evaluation (1+) and maybe before (2)",
     cxxopts::value(gagaSavePopulations))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  novelty = (!result.count("no-novelty"));

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

  { // alter mutation rates for this reproduction-less experiment
    config::Simulation::verbosity.overrideWith(0);
    auto cm = genotype::Critter::config_t::mutationRates();
    cm["cdata"] = 0.;
    cm["matureAge"] = 0.;
    cm["oldAge"] = 0.;
    utils::normalize(cm);
    genotype::Critter::config_t::mutationRates.overrideWith(cm);
  }
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
  // == GA setup

  rng::AtomicDice dice;
  std::cout << "Using seed ";
  if (seed >= 0) {
    dice.reset(seed);
    std::cout << seed << " -> ";
  }
  std::cout << dice.getSeed() << "\n";

  std::cout << "CPU Threads: " << threads << "\n"
            << "  Team size: " << teamSize << "\n"
            << "Populations: " << populations << "\n";

  genotype::Critter::printMutationRates(std::cout, 2);
//  simu::Simulation::printStaticStats();

  phylogeny::GIDManager gidManager;
  simu::Evaluator eval;

  using GA = simu::Evaluator::GA;
  struct Evolution {
    std::string name;
    GA ga;
    GAGA::NoveltyExtension<GA> nov;
    Evolution (void) : name("NA") {}
  };

  std::vector<Evolution> evolutions (populations);
  simu::Evaluator::Inds lastChampions (populations, Ind(Team::random(0, dice)));

  struct GAParameters {
    decltype(evolutions) &evos;
    decltype(lastChampions) &champs;
    const uint &np;
    const int &savePops;
  } gaParameters {evolutions, lastChampions, populations, gagaSavePopulations};

  for (uint p = 0; p<populations; p++) {
    Evolution &evo = evolutions[p];
    evo.name = std::string(1, 'A'+p);

    GA &ga = evo.ga;
    ga.setPopSize(popSize);
    ga.setNbThreads(threads);
    ga.setMutationRate(1);
    ga.setCrossoverRate(0);
    ga.setSelectionMethod(GAGA::SelectionMethod::paretoTournament);
    ga.setTournamentSize(4);
    ga.setNbElites(1);
    ga.setEvaluateAllIndividuals(true);
    ga.setVerbosity(gagaVerbosity);
//    ga.setSaveFolder(outputFolder);
    ga.setNbSavedElites(ga.getNbElites());
    ga.setSaveGenStats(true);
    ga.setSaveIndStats(true);
    ga.setSaveParetoFront(false);

    ga.disableGenerationHistory();
    if (gagaSavePopulations <= 0) ga.disablePopulationSave();

    ga.setSaveFolderGenerator([&evo, dataFolder] (auto) {
      return dataFolder / evo.name;
    });

    ga.setNewGenerationFunction([&gaParameters, &ga, p] {
      std::cout << "\n[POP " << gaParameters.evos[p].name
                << "] New generation at "
                << utils::CurrentTime{} << "\n";

      auto gen = ga.getCurrentGenerationNumber();
#ifndef CLUSTER_BUILD
      if (gen == 0 && p == 0) symlink_as_last(ga.getSaveFolder().parent_path());
#endif

      std::cout << "\tOpponent";
      if (gaParameters.np == 2)
            std::cout << " is: ";
      else  std::cout << "s are:\n";
      for (uint p_=0; p_<gaParameters.np; p_++) {
        if (p == p_) continue;
        const Ind &c = gaParameters.champs[p_];
        if (gaParameters.np > 2) std::cout << "\t\t";
        std::cout << gaParameters.evos[p_].name << simu::Evaluator::id(c)
                  << " of fitness " << c.fitnesses.at("mk") << "\n";
      }

      if (gaParameters.savePops >= 2 // save alot
          || gaParameters.savePops == -1 /*keep pop before evaluation*/) {
        std::cout << "Also saving population before evaluation"
                     " (just in case)\n";
        ga.savePop(ga.population);
      }
    });

    ga.setMutateMethod([&dice, &gidManager](Team &t) {
      auto &g = t.genome;
      g.mutate(dice);
      g.gdata.updateAfterCloning(gidManager);
    });

//    ga.setCrossoverMethod([](const CGenome&, const CGenome&)
//                          -> CGenome {assert(false);});

    // Evaluator is set below

  // -- -- -- -- -- -- SPECIFIC TO THE NOVELTY EXTENSION: -- -- -- -- -- -- --
    auto &nov = evolutions[p].nov;
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

//    if (load.empty()) {
      ga.initPopulation([teamSize, &dice, &gidManager] {
        auto t = Team::random(teamSize, dice);
        t.genome.gdata.setAsPrimordial(gidManager);
        return t;
      });
      std::cout << "Generating random population\n";

//    } else {
//      // copied from gaga to use appropriate constuctor
//      std::ifstream t(load);
//      std::stringstream buffer;
//      buffer << t.rdbuf();
//      auto o = nlohmann::json::parse(buffer.str());
//      assert(o.count("population"));

//      std::vector<Ind> pop;
//      for (auto ind : o.at("population")) {
//          pop.push_back(Ind(Team(ind.at("dna").get<std::string>())));
//          pop.back().evaluated = false;
//      }

//      ga.setPopulation(pop);
//      std::cout << "Loaded population from " << load << ", " << pop.size()
//                << " individuals\n";
//    }
  }

  // ===========================================================================
  // == Launch

  auto start = simu::Simulation::now();
  int success = 0;

  for (uint i=0; i<generations && !simu::Evaluator::aborted; i++) {
    // Update previous champions archive
    for (uint p = 0; p < populations; p++) {
      auto &ga = evolutions[p].ga;
      auto gen = ga.getCurrentGenerationNumber();

      bool first = (gen == 0);
      auto c = first ? ga.population[0]
                     : ga.getLastGenElites(1).at("mk").front();
      if (first)  c.fitnesses["mk"] = NAN;

      lastChampions[p] = c;
    }

    if (i == generations-1)
      for (uint p = 0; p < populations; p++)
        evolutions[p].nov.saveArchiveEnabled = true;

    for (uint p = 0; p < populations; p++) {
      std::vector<Ind> opponents; // Prepare competitors for each population
      for (uint p_ = 0; p_ < populations; p_++)
        if (p_ != p) opponents.push_back(lastChampions[p_]);

      GA &ga = evolutions[p].ga;
      ga.setEvaluator([&eval, &opponents] (auto &i, auto) {
          eval(i, opponents);
      }, "mortal-kombat");

      ga.step();

      if (gagaSavePopulations == -1 && i > 0) {
        stdfs::path previousPop = ga.getSaveFolder();
        previousPop /= utils::mergeToString("gen", i-1);
        previousPop /= utils::mergeToString("pop", i-1, ".pop");
        std::cerr << "Removing pre-evolution cached population\n";
        stdfs::remove(previousPop);
      }
    }
  }

  for (uint p = 0; p < populations; p++) {
    GA &ga = evolutions[p].ga;
    stdfs::create_directory_symlink(
      GAGA::concat("gen", ga.getCurrentGenerationNumber()-1),
      ga.getSaveFolder() / "gen_last");

    success +=
      (ga.getLastGenElites(1).at("mk").front().fitnesses.at("mk") >= 0);
  }

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
  if (success > 0)  std::cout << "completed";
  else              std::cout << "failed";
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

  return 0;
}
