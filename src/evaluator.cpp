#include <csignal>

#include "simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"

using CGenome = genotype::Critter;
using EGenome = genotype::Environment;

namespace eval {
using namespace simu;

// =============================================================================
// == Base class for specific evaluators =======================================
// =============================================================================

class BaseEvaluator : public Simulation {
protected:
  std::ofstream _trajectoryLogger, _annLogger;

  void baseEvaluatorInit (const Simulation &reference, const CGenome &g,
                          const std::string &tag, bool taurus) {

    InitData idata {};
    idata.ienergy = 0;
    idata.cRatio = 0;
    idata.nCritters = 0;
    idata.cRange = 0;
    idata.seed = 0;

    EGenome eg = reference.environment().genotype();
    eg.taurus = taurus;
    Simulation::init(eg, {g}, idata);

    assert(_critters.empty());
    assert(_foodlets.empty());

    clear();
    _ssga.clear();
    _ssga.setEnabled(false);
    _systemExpectedEnergy = -1; // Deactivate closed-system monitoring

    if (!tag.empty()) {
      _trajectoryLogger.open(trajectoriesOfstreamFor(tag));
      _annLogger.open(annOfstreamFor(tag));
    }
  }

  void annLoggerHeader (Critter *c) {
    for (const auto &h: c->neuralOutputsHeader()) _annLogger << h << " ";
    _annLogger << "\n";
  }

public:
  static std::string trajectoriesSuffix (void) {
    return ".dat";
  }
  static std::string trajectoriesOfstreamFor (const std::string &tag) {
    return tag + trajectoriesSuffix();
  }

  static std::string annSuffix (void) {
    return "_ann.outputs.dat";
  }
  static std::string annOfstreamFor (const std::string &tag) {
    return tag + annSuffix();
  }

  virtual void step (void) override {
    Simulation::step();
    if (_trajectoryLogger && !_critters.empty()) {
      Critter *c = *_critters.begin();
      _trajectoryLogger << c->x() << " " << c->y() << " " << c->rotation()
                        << "\n";

      for (const auto &v: c->neuralOutputs()) _annLogger << v << " ";
      _annLogger << "\n";
    }
  }

  void preClear(void) {
    _trajectoryLogger.close();
    _annLogger.close();
  }
};

// =============================================================================
// == Evaluation of capacity at reach foodlets all around ======================
// =============================================================================

class ForagingEvaluator : public BaseEvaluator {
public:
  void init (const Simulation &reference, const CGenome &g, float a,
             const std::string &tag) {

    BaseEvaluator::baseEvaluatorInit(reference, g, tag, false);

    float ce = .5 * 2 // Half the max (which is double the storage)
             * Critter::maximalEnergyStorage(Critter::MAX_SIZE);

    float age = .5 * (g.matureAge + g.oldAge);
    Critter *c = addCritter(g, 0, 0, 0, ce, age);

    float r = config::Simulation::plantMaxRadius();
    float fe = Foodlet::maxStorage(BodyType::PLANT, r);
    P2D p = fromPolar(a, .75*c->visionRange());
    Simulation::addFoodlet(BodyType::PLANT, p.x, p.y, r, fe);

    _trajectoryLogger << p.x << " " << p.y << " " << r << "\n\n";
    annLoggerHeader(c);
  }

  void step (void) override {
    BaseEvaluator::step();
    assert(_foodlets.size() == 1);
  }

//  Foodlet* addFoodlet(BodyType, float, float, float, decimal) override {
//    // Don't allow new foodlets
//    return nullptr;
//  }

  bool success (void) const {
    return !_environment->feedingEvents().empty();
  }
};

uint foragingEvaluation (const Simulation &reference,
                         const std::vector<CGenome> &genomes) {
  ForagingEvaluator testArea;

  static const auto steps = config::Simulation::ticksPerSecond() * 100;
  uint gid_w = 0;
  for (const auto &g: genomes)
    gid_w = std::max(gid_w, uint(std::ceil(std::log10(uint(g.id())))));

  float cratio = .25;
  uint ncritters = cratio * genomes.size();
  float nangles = 8;
  float fweights [] {
    1, 2, 3, 4, 2, 4, 3, 2
  };

  const auto dangle = [&nangles] (uint j) {
    return 360 * (j/nangles);
  };
  const auto rangle = [&nangles] (uint j) {
    return 2 * M_PI * (j/nangles);
  };
  static const auto gid = [] (const CGenome &g) {
    std::ostringstream oss;
    oss << "G0x" << std::hex << g.id();
    return oss.str();
  };
  static const auto tagFor = [] (const CGenome &g, float dangle){
    std::ostringstream oss;
    oss << gid(g) << "A" << std::setw(3) << std::setfill('0')
        << dangle;
    return oss.str();
  };

//  std::ostream &os = std::cout;
  std::ofstream os ("log");

  os << "GID/Angle";
  for (uint j=0; j<nangles; j++)
    os << " " << dangle(j);
  os << " Total\n";

  std::vector<float> fitnesses (ncritters, 0);
  std::vector<float> fitnessesbyAngle (nangles, 0);
  for (uint i=0; i<ncritters; i++) {
    const auto &g = genomes[i];
    fitnesses[i] = 0;

    os << gid(g);
    for (uint j=0; j<nangles; j++) {
      std::string tag = tagFor(g, dangle(j));
      testArea.init(reference, g, rangle(j), tag);

      // save brain on first angle tested
      if (j == 0) (*testArea.critters().begin())->saveBrain(gid(g));

      for (uint k=0; k<steps && !testArea.success(); k++)
        testArea.step();

      uint fitness = 0;
      if (testArea.success()) fitness = steps - testArea.currTime().timestamp();

      os << " " << fitness;
      fitnesses[i] += fitness * fweights[j];
      fitnessesbyAngle[j] += fitness;

      testArea.clear();
    }

    os << " " << fitnesses[i] << std::endl;
  }

  os << "Aggregated";
  for (uint j=0; j<nangles; j++)
    os << " " << std::round(fitnessesbyAngle[j] / ncritters);
  os << " " << [&fitnesses] {
    float s = 0; for (float f: fitnesses) s+= f; return s;
  }() << std::endl;

  std::vector<uint> indices (ncritters);
  std::iota(std::begin(indices), std::end(indices), 0);

  std::sort(indices.begin(), indices.end(),
            [&fitnesses] (uint ilhs, uint irhs) {
              return fitnesses[ilhs] < fitnesses[irhs];
  });

  uint ichamp = indices.back();
  const CGenome &champ = genomes[ichamp];
  for (uint j=0; j<nangles; j++) {
    float a = dangle(j);
    std::string tag = tagFor(champ, a);
    std::ostringstream oss;
    oss << "champ_A" << std::setfill('0') << std::setw(3) << a
        << eval::ForagingEvaluator::trajectoriesSuffix();
    stdfs::create_symlink(eval::ForagingEvaluator::trajectoriesOfstreamFor(tag),
                          oss.str());

    oss.str("");
    oss << "champ_A" << std::setfill('0') << std::setw(3) << a
        << eval::ForagingEvaluator::annSuffix();
    stdfs::create_symlink(eval::ForagingEvaluator::annOfstreamFor(tag),
                          oss.str());
  }

  std::string champBase = gid(champ);
  stdfs::create_symlink(champBase + "_cppn.dot", "champ_cppn.dot");
  stdfs::create_symlink(gid(champ) + "_ann.dat", "champ_ann.dat");
  champ.toFile("champ_genome");

  return ichamp;
}

// =============================================================================
// == Evaluation of strategy in face of multiple food sources ==================
// =============================================================================

class StrategyEvaluator : public BaseEvaluator {
  Critter *subject;
  uint contacts = 0;
  uint maxsteps = -1;
public:
  void init (const Simulation &reference, const CGenome &g,
             const std::string &tag) {

    BaseEvaluator::baseEvaluatorInit(reference, g, tag, true);

    float ce = .25 * 2 // Half the max of a youngling
             * Critter::maximalEnergyStorage(Critter::MIN_SIZE);

    float age = 0;
    subject = addCritter(g, 0, 0, 0, ce, age);
    maxsteps = Critter::lifeExpectancy(g.minClockSpeed) * _environment->dt();

    float r = config::Simulation::plantMaxRadius();
    float fe = Foodlet::maxStorage(BodyType::PLANT, r);

    static constexpr auto NP = 5;
    const auto S = _environment->size();
    for (uint i=0; i<NP; i++) {
      for (uint j=0; j<NP; j++) {
        // Dont't put one on top of the subject
        if (i == (NP-1)/2 && j == (NP-1)/2) continue;

        float x = S * (float(i) / (NP-1) - .5);
        float y = S * (float(j) / (NP-1) - .5);
        Simulation::addFoodlet(BodyType::PLANT, x, y, r, fe);
        _trajectoryLogger << x << " " << y << " " << r << "\n";
      }
    }

    _trajectoryLogger << "\n";
    annLoggerHeader(subject);
  }

  void step (void) override {
    BaseEvaluator::step();
//    assert(_foodlets.size() == 1);
    if (_critters.empty())  return;

    // Keep it starving
    subject->overrideUsableEnergyStorage(.25*subject->maxUsableEnergy());

    // And sterile
    subject->overrideReproductionReserve(0);

    while (!_environment->feedingEvents().empty()) {
      auto f = _environment->feedingEvents().begin()->foodlet;
      delFoodlet(f);
      contacts++;
    }
  }

//  Foodlet* addFoodlet(BodyType, float, float, float, decimal) override {
//    // Don't allow new foodlets
//    return nullptr;
//  }

  bool success (void) const {
    return _foodlets.empty();
  }

  auto duration (void) const {
    return _time.timestamp();
  }

  auto maxDuration (void) const {
    return maxsteps;
  }

  auto successSpeed (void) const {
    return (float(maxsteps) - _time.timestamp()) / maxsteps;
  }

  auto result (void) {
    return contacts;
  }
};

void strategyEvaluation (const Simulation &reference, const CGenome &g) {
  StrategyEvaluator evaluator;
  evaluator.init(reference, g, "champ_strategy");

  std::ofstream os ("log", std::ios_base::in | std::ios_base::ate);

  while (!evaluator.extinct() && !evaluator.success())  evaluator.step();

  auto res = evaluator.result();
  auto time = evaluator.duration();
  auto maxtime = evaluator.maxDuration();
  auto speed = evaluator.successSpeed();

  os << "Champ G0x" << std::hex << g.id() << std::dec << " reached " << res
     << " foodlets in " << time << " steps out of " << maxtime;
  if (evaluator.success())  os << " (speed = " << speed << ")";
  os << std::endl;
}

}

// =============================================================================
// == Simple genetic field extraction ==========================================
// =============================================================================

void extractField (const simu::Simulation &simu,
                   const std::vector<std::string> &fields) {
//  std::cout << "Extracting field '" << field << "'..." << std::endl;
  for (const simu::Critter *c: simu.critters())
    for (const auto &field: fields)
      std::cout << field << ": " << c->genotype().getField(field)
                << "\n";
  std::cout << std::endl;
}


// =============================================================================
// == Evaluation of feeding behavior ===========================================
// =============================================================================

void feedingBehavior (simu::Simulation &s) {
  uint startGen = s.minGeneration();

  while (!s.finished() && s.minGeneration() < startGen+2)
    s.step();

  if (s.extinct())
    std::cout << "Unable to collect feeding behavior: simulation went extinct";

  else {
    std::ofstream ofs ("carnivorous_behavior.dat");
    for (const simu::Critter *c: s.critters())
      ofs << c->carnivorousBehavior() << "\n";
  }
}

int main(int argc, char *argv[]) {

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string loadSaveFile, loadConstraints, loadFields;

  std::vector<std::string> fieldsToExtract;
  bool extractFeedingBehavior = false;

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

    ("extract-field",
     "Extracts field from the plant population (repeatable option)",
     cxxopts::value(fieldsToExtract))

    ("feeding-behavior",
     "Extracts information of the feeding behavior of the provided population",
     cxxopts::value(extractFeedingBehavior))

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


  if (!outputFolder.empty())
    reference.setDataFolder(outputFolder,
                            simu::Simulation::Overwrite(overwrite));

  if (!fieldsToExtract.empty()) {
    extractField(reference, fieldsToExtract);
    exit(0);

  } else if (extractFeedingBehavior) {
    feedingBehavior(reference);
    exit(0);
  }

  std::vector<CGenome> genomes;
  for (const simu::Critter *c: reference.critters())
    genomes.push_back(c->genotype());

  rng::FastDice dice (0);
  dice.shuffle(genomes);

//  genotype::Critter::printMutationRates(std::cout, 2);

  std::string champFile = "champ_genome.spln.json";
  CGenome champ;
  if (!stdfs::exists(champFile)) {
    uint champIndex = eval::foragingEvaluation(reference, genomes);
    champ = genomes[champIndex];
    std::cerr << "Saved (?) champ to file" << std::endl;

  } else {
    std::cerr << "Loaded champ from file" << std::endl;
    champ = CGenome::fromFile(champFile);
  }

  eval::strategyEvaluation(reference, champ);

  return 0;
}
