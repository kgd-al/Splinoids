#include <csignal>

#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"

namespace eval {
using namespace simu;

class ForagingEvaluator : public Simulation {
  std::ofstream _logger;
public:
  void init (const Simulation &reference, const CGenome &g, float a,
             const std::string &tag) {
    if (!tag.empty()) {
      std::ostringstream oss;
      oss << tag << ".dat";
      _logger.open(oss.str());
    }

    InitData idata {};
    idata.ienergy = 100;
    idata.cRatio = 0;
    idata.nCritters = 0;
    idata.cRange = 0;
    idata.seed = 0;
    Simulation::init(reference.environment().genotype(), g, idata);

    clear();
    _ssga.clear();
    _ssga.setEnabled(false);

    float age = .5 * (g.matureAge + g.oldAge);
    Critter *c = addCritter(g, 0, 0, 0,
                            2 * Critter::maximalEnergyStorage(Critter::MAX_SIZE),
                            age);

    float r = config::Simulation::plantMaxRadius();
    P2D p = fromPolar(a, .75*c->visionRange());
    addFoodlet(BodyType::PLANT, p.x, p.y, r,
               Foodlet::maxStorage(BodyType::PLANT, r));
  }

  void step (void) override {
    Simulation::step();
    if (_logger) {
      Critter *c = *_critters.begin();
      _logger << _time.timestamp() << " " << c->x() << " " << c->y() << "\n";
    }
  }

  bool success (void) const {
    return !_environment->feedingEvents().empty();
  }

  void preClear(void) {
    _logger.close();
  }
};

}

int main(int argc, char *argv[]) {
  using CGenome = genotype::Critter;
//  using EGenome = genotype::Environment;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string loadSaveFile, loadConstraints, loadFields;

//  std::string duration = "=100";
  std::string outputFolder = "tmp/eval/";
  char overwrite = simu::Simulation::UNSPECIFIED;

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

    ("l,load", "Load a previously saved simulation",
     cxxopts::value(loadSaveFile))
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

  if (loadSaveFile.empty())
    utils::doThrow<std::invalid_argument>(
      "No save file provided for evaluation!");

  // ===========================================================================
  // == Loading

  simu::Simulation reference;
  simu::Simulation::load(loadSaveFile, reference, loadConstraints, loadFields);
  if (verbosity != Verbosity::QUIET)  config::Simulation::printConfig();

  std::vector<CGenome> genomes;
  for (const simu::Critter *c: reference.critters())
    genomes.push_back(c->genotype());

  rng::FastDice dice (0);
  dice.shuffle(genomes);

  if (!outputFolder.empty())
    reference.setDataFolder(outputFolder, simu::Simulation::Overwrite(overwrite));

//  genotype::Critter::printMutationRates(std::cout, 2);

  eval::ForagingEvaluator testArea;

  static const auto steps = config::Simulation::ticksPerSecond() * 100;
  uint gid_w = 0;
  for (const auto &g: genomes)
    gid_w = std::max(gid_w, uint(std::ceil(std::log10(uint(g.id())))));

  uint ncritters = std::min(genomes.size(), 50ul);
  float nangles = 8;

  std::ostream &os = std::cout;

  os << "GID/Angle";
  for (uint j=0; j<nangles; j++)
    os << " " << 360 * (j / nangles);
  os << "\n";

  for (uint i=0; i<ncritters; i++) {
    const auto &g = genomes[i];

    os << "G" << g.id();
    for (uint j=0; j<nangles; j++) {
      float angle = 2 * M_PI * (j / nangles);

      std::ostringstream oss;
      oss << "G" << g.id() << "A" << 360 * (j/nangles);

      testArea.init(reference, g, angle, oss.str());

      for (uint k=0; k<steps && !testArea.success(); k++)
        testArea.step();

      uint fitness = 0;
      if (testArea.success()) fitness = steps - testArea.currTime().timestamp();

      os << " " << fitness;

      testArea.clear();
    }

    os << std::endl;
  }

  return 0;
}
