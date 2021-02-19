#include <csignal>

#include "indevaluator.h"
#include "kgd/external/cxxopts.hpp"

void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  simu::IndEvaluator::aborted = true;
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

int main(int argc, char *argv[]) {
  using CGenome = genotype::Critter;
  using EGenome = genotype::Environment;
  using Ind = simu::IndEvaluator::Ind;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;
  int gagaVerbosity = 1;

  std::string eGenomeArg;
//  std::vector<std::string> cGenomeArgs;
  std::string load;

  EGenome eGenome;
//  std::vector<CGenome> cGenomes;

  std::string outputFolder = "tmp/pp-evo/";
  char overwrite = simu::Simulation::ABORT;

  uint popSize = 5, generations = 1, threads = 1;
  long seed = -1;

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
    ("gaga-verbosity", "Verbosity level (for gaga [0,3])",
     cxxopts::value(gagaVerbosity))

    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

    ("env-genome", "Environment's genome", cxxopts::value(eGenomeArg))
//    ("spln-genome", "Splinoid genome to start from or a random seed. Use "
//                    "multiple times to define multiple sub-populations",
//     cxxopts::value(cGenomeArgs))
    ("load", "Restart evolution from a saved population",
     cxxopts::value(load))

    ("seed", "Size of the evolved population", cxxopts::value(seed))
    ("pop-size", "Size of the evolved population", cxxopts::value(popSize))
    ("generations", "Number of generations to let the evolution run for",
     cxxopts::value(generations))
    ("threads", "Number of parallel threads", cxxopts::value(threads))
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
  { // alter mutation rates for this splineless experiment
    config::Simulation::verbosity.overrideWith(0);
    auto cm = genotype::Critter::config_t::mutationRates();
    cm["cdata"] = 0.;
    cm["matureAge"] = 0.;
    cm["oldAge"] = 0.;
    utils::normalize(cm);
    genotype::Critter::config_t::mutationRates.overrideWith(cm);
  }
  if (configFile.empty()) config::Simulation::printConfig("");

  std::cout << "Reading environment genome from input file '"
            << eGenomeArg << "'" << std::endl;
  eGenome = EGenome::fromFile(eGenomeArg);
  std::cout << "Environment:\n" << eGenome << "\n";

//  if (load.empty()) {
//    if (cGenomeArgs.empty())  cGenomeArgs.push_back("-1");
//    for (const auto arg: cGenomeArgs) {
//      long cGenomeSeed = maybeSeed(arg);
//      if (cGenomeSeed < -1) {
//        std::cout << "Reading splinoid genome from input file '"
//                  << arg << "'" << std::endl;
//        cGenomes.push_back(CGenome::fromFile(arg));

//      } else {
//        rng::FastDice dice;
//        if (cGenomeSeed >= 0) dice.reset(cGenomeSeed);
//        std::cout << "Generating splinoid genome from rng seed "
//                  << dice.getSeed() << std::endl;
//        CGenome g = CGenome::random(dice);
//        cGenomes.push_back(g);
//      }
//    }

//    std::cout << "Splinoid";
//    if (cGenomes.size() > 1) std::cout << "s";
//    std::cout << ":\n";
//    for (const CGenome &g: cGenomes)
//      std::cout << g;
//    std::cout << std::endl;
//  }

  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == GAGA setup


  CGenome::printMutationRates(std::cout, 2);
//  if (!outputFolder.empty())
//    s.setWorkPath(outputFolder, simu::Simulation::Overwrite(overwrite));
  simu::Simulation::printStaticStats();

  bool success = true;
  rng::AtomicDice dice (seed);
  std::cout << "Using seed " << seed << " -> " << dice.getSeed() << "\n";

  simu::IndEvaluator eval (eGenome);
  simu::IndEvaluator::GA ga;
  ga.setPopSize(popSize);
  ga.setNbThreads(threads);
  ga.setMutationRate(1);
  ga.setCrossoverRate(0);
  ga.setSelectionMethod(GAGA::SelectionMethod::paretoTournament);
  ga.setTournamentSize(4);
  ga.setNbElites(4);
  ga.setEvaluateAllIndividuals(true);
  ga.setVerbosity(gagaVerbosity);
  ga.setSaveFolder(outputFolder);
  ga.setNbSavedElites(ga.getNbElites());
  ga.setSaveGenStats(true);
  ga.setSaveIndStats(true);
  ga.setSaveParetoFront(false);

  ga.setNewGenerationFunction([&dice, &eval, &ga] {
    std::cout << "\nNew generation at " << utils::CurrentTime{} << "\n";
    if (ga.getCurrentGenerationNumber() == 0)
      symlink_as_last(ga.getSaveFolder());
    eval.selectCurrentScenarios(dice);
    std::cout << std::endl;
  });
  ga.setMutateMethod([&dice](CGenome &dna){ dna.mutate(dice); });
//  ga.setCrossoverMethod([](const CGenome&, const CGenome&) -> CGenome {assert(false);});
  ga.setEvaluator([&eval] (auto &i, auto p) { eval(i, p); },
                  "prey-maybe-predator");

  if (load.empty()) {
    ga.initPopulation([&dice] { return CGenome::random(dice); });
    std::cout << "Generating random population\n";

  } else {
    // copied from gaga to use appropriate constuctor
    std::ifstream t(load);
    std::stringstream buffer;
    buffer << t.rdbuf();
    auto o = nlohmann::json::parse(buffer.str());
    assert(o.count("population"));

    std::vector<Ind> pop;
    for (auto ind : o.at("population")) {
        pop.push_back(Ind(CGenome(ind.at("dna").get<std::string>())));
        pop.back().evaluated = false;
    }

    ga.setPopulation(pop);
    std::cout << "Loaded population from " << load << ", " << pop.size()
              << " individuals\n";
  }

  // ===========================================================================
  // == Launch

  auto start = simu::Simulation::now();
  ga.step(generations);

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
  if (success)  std::cout << "completed";
  else          std::cout << "failed";
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
