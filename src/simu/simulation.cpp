#include "kgd/random/dice.hpp"

#include "simulation.h"

#include "box2dutils.h"

namespace simu {
using json = nlohmann::json;

static constexpr bool debugShowStepHeader = false;
static constexpr int debugEntropy = 0;
static constexpr int debugCritterManagement = 0;
static constexpr int debugFoodletManagement = 0;
static constexpr int debugAudition = 0;
static constexpr int debugReproduction = 0;

namespace statis_stats_details {

using SConfig = config::Simulation;

struct FormatClockSpeed {
  float c;
  friend std::ostream& operator<< (std::ostream &os, const FormatClockSpeed &f) {
    return os << "(" << f.c << "): "
              << 1. / (f.c * SConfig::baselineAgingSpeed())
              << "s\n";
  }
};

struct FormatStarvation {
  float s;
  friend std::ostream& operator<< (std::ostream &os, const FormatStarvation &f) {
    return os << Critter::starvationDuration(f.s,
                                             Critter::maximalEnergyStorage(f.s),
                                             1) << "s\n";
  }
};

struct FormatAbsorption {
  float e, s;
  friend std::ostream& operator<< (std::ostream &os, const FormatAbsorption &f) {
    return os << "(" << f.e << "," << f.s << "): "
              << Critter::maximalEnergyStorage(f.s)
                 / (f.e * SConfig::energyAbsorptionRate()) << "s\n";
  }
};

}
void Simulation::printStaticStats (void) {
  std::cerr << "==================\n"
               "== Static stats ==\n";

  using namespace statis_stats_details;
  using CConfig = genotype::Critter::config_t;

  std::cerr << "\n= Plant energy =\n"
            << "min: " << Foodlet::maxStorage(BodyType::PLANT,
                                              SConfig::plantMinRadius()) << "\n"
            << "max: " << Foodlet::maxStorage(BodyType::PLANT,
                                              SConfig::plantMaxRadius()) << "\n";

  std::cerr << "\n= Splinoid energy =\n"
            << "     min: " << Critter::maximalEnergyStorage(Critter::MIN_SIZE)
              << "\n"
            << "     max: " << Critter::maximalEnergyStorage(Critter::MAX_SIZE)
              << "\n"
            << "creation: " << Critter::energyForCreation() << "\n";

  std::cerr << "\n= Aging speed =\n"
            << "clock speed\n"
            << " min: " << FormatClockSpeed{CConfig::minClockSpeedBounds().min}
            << " avg: " << FormatClockSpeed{.25f * (
                               CConfig::minClockSpeedBounds().min
                             + CConfig::minClockSpeedBounds().max
                             + CConfig::maxClockSpeedBounds().min
                             + CConfig::maxClockSpeedBounds().max)}
            << "      " << FormatClockSpeed{1}
            << " max: " << FormatClockSpeed{CConfig::maxClockSpeedBounds().max};

  std::cerr << "\n= Starvation =\n"
            << "newborn: " << FormatStarvation{Critter::MIN_SIZE}
            << "  adult: " << FormatStarvation{Critter::MAX_SIZE};

  std::cerr << "\n= Regeneration =\n"
            << "adult: " << 1. / SConfig::baselineRegenerationRate() << "s\n";

  std::cerr << "\n= Reproduction =\n"
            << "adult: " << .5 * Critter::energyForCreation()
               / (Critter::maximalEnergyStorage(Critter::MAX_SIZE)
                  * SConfig::baselineGametesGrowth()) << "s\n";

  std::cerr << "\n= Absorption =\n"
            << "newborn " << FormatAbsorption{.25,   .75 * Critter::MIN_SIZE
                                                   + .25 * Critter::MAX_SIZE}
            << "  adult " << FormatAbsorption{1, 1}
            << "    old " << FormatAbsorption{.25, 1};

  std::cerr << std::endl;
}

Simulation::Simulation(void)
  : _environment(nullptr), _printedHeader(false), _workPath("."),
    _timeMs(), _finished(false), _aborted(false) {}

Simulation::~Simulation (void) {
  clear();
}

bool Simulation::setWorkPath (const stdfs::path &path, Overwrite o) {
  static const auto &verb = config::Simulation::verbosity();

  if (stdfs::exists(path) && !stdfs::is_empty(path)) {
    if (o == UNSPECIFIED) {
      std::cerr << "WARNING: data folder '" << path << "' is not empty."
                   " What do you want to do ([a]bort, [p]urge)? "
                << std::flush;
      o = Overwrite(std::cin.get());
    }

    switch (o) {
    case IGNORE:  std::cerr << "Assuming you know what you are doing..."
                            << std::endl;
                  break;

    case PURGE: if (verb)
                  std::cerr << "Purging contents from " << path << std::endl;
                stdfs::remove_all(path);
                break;

    case ABORT: std::cerr << "Aborting simulation as requested." << std::endl;
                abort();  break;

    default:  std::cerr << "Invalid overwrite option '" << o
                        << "' defaulting to abort." << std::endl;
              abort();
    }
  }
  if (_aborted) return false;

  if (!stdfs::exists(path)) {
    if (verb > 1) std::cout << "Creating data folder " << path << std::endl;
    stdfs::create_directories(path);
  }

  if (verb > 1) std::cout << "Changed working directory from " << _workPath;
  _workPath = path;
  if (verb > 1) std::cout << " to " << _workPath << std::endl;

  stdfs::path statsPath = localFilePath("global.dat");
  if (_statsLogger.is_open()) _statsLogger.close();
  _statsLogger.open(statsPath, std::ofstream::out | std::ofstream::trunc);
  if (!_statsLogger.is_open())
    utils::doThrow<std::invalid_argument>(
      "Unable to open stats file ", statsPath);

  stdfs::path competPath = localFilePath("competition.dat");
  if (_competitionLogger.is_open()) _competitionLogger.close();
  _competitionLogger.open(competPath, std::ofstream::out | std::ofstream::trunc);
  if (!_competitionLogger.is_open())
    utils::doThrow<std::invalid_argument>(
      "Unable to open compet file ", competPath);

//  using O = genotype::cgp::Outputs;
//  using U = EnumUtils<O>;
//  for (O o: U::iterator()) {
//    auto o_t = U::toUnderlying(o);
//    stdfs::path envPath = path / envPaths.at(o);
//    std::ofstream &ofs = _envFiles[o_t];

//    if (ofs.is_open()) ofs.close();
//    if (!config::CGP::isActiveOutput(o_t))  continue;

//    ofs.open(envPath, openMode);

//    if (!ofs.is_open())
//      utils::doThrow<std::invalid_argument>(
//        "Unable to open voxel file ", envPath, " for ", U::getName(o));
//  }
  _printedHeader = false;

  return true;
}

void Simulation::init(const Environment::Genome &egenome,
                      std::vector<Critter::Genome> cgenomes,
                      const InitData &data) {

//  // Parse scenario settings
//  json scenarioData;
//  if (stdfs::exists(data)) {
//    std::cout << "Parsing contents of " << data << " for scenario"
//                                                               " data\n";
//    scenarioData = json::parse(utils::readAll(data));

//  } else
//    scenarioData = json::parse(data);

//  InitData data;

  _finished = false;
  _aborted = false;
  _genData.min = 0;
  _genData.max = 0;
  _genData.goal = std::numeric_limits<decltype(_genData.goal)>::max();
  _time.set(0);

  // TODO Remove (debug)
//  cgenome.vision.angleBody = M_PI/4.;
//  cgenome.vision.angleRelative = -M_PI/4;
//  cgenome.vision.width = M_PI/4.;
//  cgenome.vision.precision = 1;
//  cgenome.matureAge = .05;

//  auto le = [&cgenome] (float v) {
//    return Critter::lifeExpectancy(Critter::clockSpeed(cgenome, v));
//  };
//  auto sd = [&cgenome] (float v, bool y) {
//    auto s = y ? Critter::MIN_SIZE : Critter::MAX_SIZE;
//    return Critter::starvationDuration(s,
//      Critter::maximalEnergyStorage(s),
//      Critter::clockSpeed(cgenome, v));
//  };

//  if (debugShowStaticStats)
//    std::cerr << "Stats for provided genome\n"
//              << "   Old age duration: " << le(0) << ", " << le(.5) << ", "
//                << le(1) << "\n"
//              << "Starvation duration:\n\t"
//                << sd(0, true) << ", " << sd(.5, true) << ", " << sd(1, true)
//                << "\n\t" << sd(0, false) << ", " << sd(.5, false) << ", "
//                << sd(1, false) << std::endl;

  _systemExpectedEnergy = data.ienergy;

  _environment = std::make_unique<Environment>(egenome);
  _environment->init(data.ienergy, data.seed);
  std::cerr << "Using seed: " << data.seed << " -> "
            << _environment->dice().getSeed() << "\n";

  if (cgenomes.size() > 0) {
    auto nextGID = cgenomes.front().id();
    for (CGenome &g: cgenomes) {
      if (g.id() != Critter::ID::INVALID) nextGID = std::max(nextGID, g.id());
      g.gdata.setAsPrimordial(_gidManager);
    }
    _gidManager.setNext(nextGID);
  }

  decimal energy = data.ienergy;
  decimal cenergy = energy * data.cRatio;
  decimal energyPerCritter =
    std::min(Critter::energyForCreation(), decimal(cenergy / data.nCritters));

//  energy -= cenergy;
//  cenergy -= energyPerCritter * data.nCritters;
//  energy += cenergy;

  const float W = _environment->extent();
  const float C = W * data.cRange;

  _populations = cgenomes.size();
  auto &dice = _environment->dice();
  for (uint i=0; i<data.nCritters; i++) {
    uint bindex = i%_populations;
    uint psize = i/_populations;
    auto cg = cgenomes[bindex].clone(_gidManager);
    cg.cdata.sex = (psize%2 ? Critter::Sex::MALE : Critter::Sex::FEMALE);
    cg.gdata.generation = 0;

    float a = dice(0., 2*M_PI);
    float x = dice(-C, C);
    float y = dice(-C, C);

    // TODO
//    if (i == 0) x = -.75, y = .25, a = 0;
//    if (i == 1) x = -.75, y = -.25, a = 0;

//    if (i == 0) x = 49, y = 49, a = M_PI/4;
//    if (i == 1) x = 47, y = 50-sqrt(2), a = M_PI/2;
//    if (i == 2) x = 50-sqrt(2), y = 47, a = 0;

    Critter *c = addCritter(cg, x, y, a, energyPerCritter, data.cAge);
    c->userIndex = bindex;
  }

  _nextFoodletID = 0;

//  addFoodlet(BodyType::PLANT,
//             0, 0,
//             1, 2); // TODO
  plantRenewal(W * data.pRange);

  _statsLogger.open(config::Simulation::logFile());
  _reproductions = ReproductionStats{};
  _autopsies = Autopsies{};

//  _ssga.init(_critters.size());

  postInit();

  logStats();

//  if (false) {
//    Critter *c = *_critters.begin();
//    CGenome &g = c->genotype();

//    auto save = [this] (Critter *c, CGenome &g, uint i) {
//      std::ostringstream oss;
//      oss << "tmp/mutated_" << i << "_";

//      std::string p = oss.str();
//      std::ofstream gos (localFilePath(p + "cppn.dot"));
//      g.connectivity.toDot(gos);

//      std::ofstream pos (localFilePath(p + "ann.dat"));
//      auto substrate = substrateFor(c->raysEnd(), g.connectivity);
//      NEAT::NeuralNetwork brain;
//      g.connectivity.BuildHyperNEATPhenotype(brain, substrate);
//      g.connectivity.phenotypeToDat(pos, brain);
//    };

//    auto &mr = genotype::Critter::config_t::mutationRates.ref();
//    for (auto &p: mr) if (p.first != "connectivity")  p.second = 0;

//    genotype::Critter::printMutationRates(std::cout, 2);

//    rng::FastDice dice (1);
//    save(c, g, 0);
//    for (uint i=1; i<=10; i++) {
//      uint n = 100;
//      for (uint j=0; j<n; j++)  g.mutate(dice);
//      save(c, g, i*n);
//    }

//    exit(242);
//  }
}

b2Body* Simulation::critterBody (float x, float y, float a) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_dynamicBody;
  bodyDef.position.Set(x, y);
  bodyDef.angle = a;
  bodyDef.angularDamping = .95;
  bodyDef.linearDamping = .9;

  return physics().CreateBody(&bodyDef);
}

Critter* Simulation::addCritter (const CGenome &genome,
                                 float x, float y, float a, decimal e,
                                 float age) {

  b2Body *body = critterBody(x, y, a);

#ifndef NDEBUG
  decimal prevEE = _environment->energy();
#endif

  decimal dissipatedEnergy =
    e * .5 *(1 - config::Simulation::healthToEnergyRatio());
  decimal e_ = e - dissipatedEnergy;

  _environment->modifyEnergyReserve(-e_);

  Critter *c = new Critter (genome, body, e_, age);
  _critters.insert(c);


#ifndef NDEBUG
  decimal e__ = c->totalEnergy();
  if (debugEntropy && e_ != e__)
    std::cerr << "Delta energy on " << CID(c) << " creation: " << e__-e_
              << std::endl;
  decimal currEE = _environment->energy(), dEE = currEE - prevEE;
  if (debugEntropy && dEE != e_)
    std::cerr << "Delta energy reserve on " << CID(c) << " creation: "
              << dEE +e_ << std::endl;
#endif

  if (debugCritterManagement)
    std::cerr << "Created " << CID(c, "splinoid ") << " (SID: " << c->species()
              << ", gen " << c->genotype().gdata.generation << ") at "
              << c->pos() << std::endl;

//  if (_ssga.watching()) _ssga.registerBirth(c);

  if (age > 0) { /// WARNING this is quite ugly... (but it works)
    e_ = c->maxOutHealthAndEnergy();
    _environment->modifyEnergyReserve(-e_);
  }

  return c;
}

void Simulation::delCritter (Critter *critter) {
  {
    if (critter->tooOld())
      _autopsies.oldage++;

    else {
      uint age = critter->isYouth() ? 0 : critter->isAdult() ? 1 : 2;
      if (critter->starved())
        _autopsies.counts[Autopsies::STARVATION][age]++;
      else if (critter->fatallyInjured())
        _autopsies.counts[Autopsies::INJURY][age]++;
    }
  }

  if (debugCritterManagement) critter->autopsy();
//  if (_ssga.watching()) _ssga.registerDeath(critter);
  _critters.erase(critter);
  physics().DestroyBody(&critter->body());
  delete critter;
}

b2Body* Simulation::foodletBody(float x, float y) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_staticBody;
  bodyDef.position.Set(x, y);

  return _environment->physics().CreateBody(&bodyDef);
}

Foodlet* Simulation::addFoodlet(BodyType t, float x, float y, float r, decimal e) {
  b2Body *body = foodletBody(x, y);
  Foodlet *f = new Foodlet (t, _nextFoodletID++, body, r, e);

  if (debugFoodletManagement)
    std::cerr << "Added foodlet " << f->id() << " of type " << uint(f->type())
              << " at " << f->body().GetPosition() << " with energy "
              << e << std::endl;

  if (t == BodyType::PLANT)
    _environment->modifyEnergyReserve(-e);

  _foodlets.insert(f);
  return f;
}

void Simulation::delFoodlet(Foodlet *foodlet) {
  if (debugFoodletManagement)
    std::cerr << "Destroyed foodlet " << foodlet->id() << " of type "
              << uint(foodlet->type()) << " at "
              << foodlet->body().GetPosition() << std::endl;

  _foodlets.erase(foodlet);
  physics().DestroyBody(&foodlet->body());
  delete foodlet;
}

void Simulation::clear (void) {
  preClear();
//  _environment.reset(nullptr);
  while (!_critters.empty())  delCritter(*_critters.begin());
  while (!_foodlets.empty())  delFoodlet(*_foodlets.begin());
}

void Simulation::step (void) {
  auto stepStart = now(), start = stepStart;

  if (debugShowStepHeader)
    std::cerr << "\n## Simulation step " << _time.timestamp() << " ("
              << _time.pretty() << ") ##" << std::endl;

  auto prevMinGen = _genData.min, prevMaxGen = _genData.max;
  _genData.min = std::numeric_limits<uint>::max();
  _genData.max = 0;

  for (Critter *c: _critters) {
    c->step(*_environment);

    _genData.min = std::min(_genData.min, c->genotype().gdata.generation);
    _genData.max = std::max(_genData.max, c->genotype().gdata.generation);
  }
  if (_critters.empty())  _genData.min = 0;
  if (_timeMs.level > 1)  _timeMs.spln = durationFrom(start),  start = now();

  _environment->step();

  if (_timeMs.level > 1)  _timeMs.env = durationFrom(start),  start = now();

  audition();
  reproduction();
  produceCorpses();
  decomposition();
  if (_timeMs.level > 1)  _timeMs.decay = durationFrom(start),  start = now();

  if (_time.secondFraction() == 0)  plantRenewal();
  if (_timeMs.level > 1)  _timeMs.regen = durationFrom(start),  start = now();

  static const auto &lse = config::Simulation::logStatsEvery();
  if (_statsLogger && lse > 0 && (_time.timestamp() % lse) == 0) {
    logStats();
    _reproductions = ReproductionStats{};
    _autopsies = Autopsies{};
  }

  _time.next();

#ifndef NDEBUG
  // Fails when system energy diverges too much from initial value
  detectBudgetFluctuations();
#endif

  // If above did not fail, effect small corrections to keep things smooth
  if (config::Simulation::screwTheEntropy())  correctFloatingErrors();

  maybeCall(POST_STEP);

  if (_timeMs.level > 0)  _timeMs.step = durationFrom(stepStart);

  if (config::Simulation::verbosity() >= 1
      && (prevMinGen < _genData.min || prevMaxGen < _genData.max)) {
    std::cerr << "## Simulation step " << _time.pretty() << " gens: ["
              << _genData.min << "; " << _genData.max << "] at "
              << utils::CurrentTime{} << "\n";
  }
}

void Simulation::atEnd (void) {
  logStats();
}

void Simulation::audition(void) {
  for (Critter *c: _critters) c->ears().fill(0);

  static constexpr auto S = Critter::VOCAL_CHANNELS+1;
  static const auto &A = config::Simulation::soundAttenuation();
  static const auto process = [] (Critter *lhs, Critter *rhs) {
    const auto &sound = rhs->producedSound();
    auto &ears = lhs->ears();
    const auto earsDy = lhs->bodyRadius();
    std::array<float, 2> distances {
      b2Distance(rhs->pos(), lhs->body().GetWorldPoint({0, +earsDy})),
      b2Distance(rhs->pos(), lhs->body().GetWorldPoint({0, -earsDy}))
    };

    if (debugAudition > 1)  std::cerr << "\t" << CID(lhs) << " hears:\n";

    for (uint i=0; i<2; i++) {
      float att = A*distances[i];
      if (debugAudition >= 3)
        std::cerr << "\t\tatt = " << att << " = " << A << " * " << distances[i]
                  << "\n";

      for (uint j=0; j<S; j++) {
        uint ix = j + i*S;
        float v = std::max(0.f, sound[j] - att);
        assert(0 <= v && v <= 1);
        ears[ix] = std::max(ears[ix], v);

        if (debugAudition >= 2)
          std::cerr << "\t\t" << v << " = max(0, " << sound[j] << " - " << att
                    << ")\n";
      }

      if (debugAudition >= 2)  std::cerr << "\n";
    }

    if (debugAudition == 1) {
      using utils::operator <<;
      std::cerr << ears << "\n";
    }
  };

  if (debugAudition > 0)
    std::cerr << "\n" << _environment->hearingEvents().size()
              << " hearing events\n";
  for (auto &p: _environment->hearingEvents()) {
    Critter *a = p.first, *b = p.second;
    if (debugAudition > 0)
      std::cerr << "Audition step " << CID(a) << "/" << CID(b) << ": "
                << a->silent() << "/" << b->silent() << "\n";

    if (a->silent() && b->silent())  continue;

    if (!a->silent()) process(b, a);
    if (!b->silent()) process(a, b);
  }
}

void Simulation::reproduction(void) {
  static constexpr auto S = CGenome::SEXUAL;
  static constexpr auto A = CGenome::ASEXUAL;

  static const genotype::_details::FCOMPAT<CGenome> fcompat =
      &CGenome::compatibility;

  auto &dice = this->dice();
  auto matings = _environment->matingEvents();
  for (auto p: matings) {
    Critter *f = p.first, *m = p.second;
    assert(f->hasSexualReproduction());
    assert(m->hasSexualReproduction());

    if (!f->requestingMating(S)) continue; // Already reproduced this timestep
    if (!m->requestingMating(S)) continue;

    if (f->sex() != Critter::Sex::FEMALE) std::swap(f, m);
    assert(f->sex() == Critter::Sex::FEMALE);
    assert(m->sex() == Critter::Sex::MALE);

    if (debugReproduction)
      std::cerr << "Start of mating attempt between " << CID(f) << " ("
                << f->requestingMating(S) << ": "
                << f->reproductionReadiness(S) << ", "
                << f->reproductionOutput() << ") & "
                << CID(m) << " (" << m->requestingMating(S) << ": "
                << m->reproductionReadiness(S)
                << ", " << m->reproductionOutput() << ")" << std::endl;

    if (!f->requestingMating(S) || !m->requestingMating(S))
      continue;

    const CGenome &lhs = f->genotype(), &rhs = m->genotype();

    std::vector<CGenome> children (1);
    if (genotype::bailOutCrossver(lhs, rhs, children, dice, fcompat)) {
      assert(children.size() == 1);
      children.front().genealogy().updateAfterCrossing(
            lhs.genealogy(), rhs.genealogy(), _gidManager);

      // Pick one as reference critter to define position
      createChild(dice(.5) ? f : m, children.front(),
                  f->reproductionReserve() + m->reproductionReserve(), dice);

//      if (_ssga.watching()) _ssga.recordChildFor({f,m});
      _reproductions.sexual++;
    }

    _reproductions.attempts++;

    f->resetMating();
    m->resetMating();
  }

  for (Critter *c: _critters) {
    if (!c->hasAsexualReproduction() || !c->requestingMating(A)) continue;

    CGenome g = c->genotype();
    while (dice(genotype::BOCData::config_t::mutateChild()))  g.mutate(dice);
    g.genealogy().updateAfterCloning(_gidManager);

    Critter *child = createChild(c, g, c->reproductionReserve(), dice);
    _reproductions.asexual++;

    c->resetMating();

    child->userIndex = c->userIndex;
  }
}

Critter* Simulation::createChild(const Critter *parent, const CGenome &genome,
                                 decimal energy,
                                 rng::AbstractDice &dice) {

  const auto W = _environment->extent();
  float r = parent->bodyRadius();
  P2D p = parent->pos() + fromPolar(dice(0., 2*M_PI), dice(r, 10*r));
  utils::clip(-W, p.x, W);
  utils::clip(-W, p.y, W);

  Critter *c = addCritter(genome, p.x, p.y, dice(0., 2.*M_PI), energy);

  if (debugReproduction)  std::cerr << "\tSpawned " << CID(c) << std::endl;

  return c;
}

void Simulation::produceCorpses(void) {
  std::vector<Critter*> corpses;
  for (Critter *c: _critters)
    if (c->isDead())
      corpses.push_back(c);

  for (Critter *c: corpses) {
    Foodlet *f = addFoodlet(BodyType::CORPSE, c->x(), c->y(), c->bodyRadius(),
                            c->totalEnergy());
    f->setBaseColor(c->initialBodyColor());
    f->updateColor();
    delCritter(c);
  }
}

void Simulation::decomposition(void) {
  for (Foodlet *f: _foodlets) if (f->isCorpse())  f->update(*_environment);

  // Remove empty foodlets
  std::vector<Foodlet*> consumed;
  for (Foodlet *f: _foodlets)
    if (f->energy() <= 0)
      consumed.push_back(f), assert(f->energy() >= 0);

  for (Foodlet *f: consumed) delFoodlet(f);
}

void Simulation::plantRenewal(float bounds) {
  static const auto mR = config::Simulation::plantMinRadius(),
                    MR = config::Simulation::plantMaxRadius();
  static const auto minE = Foodlet::maxStorage(BodyType::PLANT, mR);

  const decimal mvp = _environment->genotype().maxVegetalPortion
                    * _systemExpectedEnergy;
  decimal vp = 0;
  for (const Foodlet *f: _foodlets) if (f->isPlant()) vp += f->energy();

  // Maybe pop-up a new plant
  auto &dice = _environment->dice();
  while (_environment->energy() > minE && vp < mvp) {
    float r = dice(mR, MR);
    decimal e = std::min(
                  std::min(_environment->energy(), mvp-vp),
                  Foodlet::maxStorage(BodyType::PLANT, r));
    float E = (bounds > 0 ? bounds : _environment->extent()) - r;
    addFoodlet(BodyType::PLANT, dice(-E, E), dice(-E, E), r, e);

    vp += e;
    assert(_environment->energy() >= 0);
  }
}

void Simulation::logStats (void) {
  Stats s {};
  assert(s.ncritters == 0);

  for (const auto &f: _foodlets) {
    if (f->type() == simu::BodyType::PLANT)
          s.nplants++,  s.eplants += f->energy();
    else  s.ncorpses++, s.ecorpses += f->energy();
  }

  s.ncritters = _critters.size();

  s.nfeedings = _environment->feedingEvents().size();
  s.nfights = _environment->fightingEvents().size();

  for (const auto &c: _critters) {
    s.ecritters += c->totalEnergy();
    if (c->isYouth()) s.nyoungs++;
    else if (c->isAdult())  s.nadults++;
    else s.nelders++, assert(c->isElder());
  }

  s.ereserve = _environment->energy();

  decimal eE = totalEnergy() - _systemExpectedEnergy;

//  s.fmin = _ssga.worstFitness();
//  s.favg = _ssga.averageFitness();
//  s.fmax = _ssga.bestFitness();

  if (!_printedHeader) {
    _statsLogger << "Step Date"
                    " NCritters NYoungs NAdults NElders"
                    " NCorpses NPlants"
                    " NFeeding NFights"
                    " ECritters ECorpses EPlants EReserve"
                    " eE"
                    " FRepro SRepro ARepro"// ERepro"
                    " GMin GMax"// FMin FAvg FMax"
                    " DYInj DAInj DOInj DYStr DAStr DOStr DOAge";

//    for (const auto &p: _ssga.champStats())
//      _statsLogger << " SSGAC" << p.first;

    _statsLogger << "\n";
  }

  _statsLogger << _time.timestamp() << " " << _time.pretty()

               << " " << s.ncritters << " "
               << s.nyoungs << " " << s.nadults << " " << s.nelders

               << " " << s.ncorpses << " " << s.nplants << " "
               << s.nfeedings << " " << s.nfights

               << " " << s.ecritters << " " << s.ecorpses << " " << s.eplants
               << " " << s.ereserve << " " << eE

               << " " << _reproductions.attempts << " " << _reproductions.sexual
               << " " << _reproductions.asexual// << " " << _reproductions.ssga

               << " " << _genData.min << " " << _genData.max << " "
               /*<< s.fmin << " " << s.favg << " " << s.fmax*/;

  for (const auto &a: _autopsies.counts)
    for (const auto v: a)
      _statsLogger << " " << v;
  _statsLogger << " " << _autopsies.oldage;

//  for (const auto &p: _ssga.champStats())
//    _statsLogger << " " << p.second;

  _statsLogger << std::endl;

  processStats(s);


  // Also log competition data
  auto &cs = _competitionStats;
  cs.counts = std::vector<uint>(_populations, 0);
  cs.regimens = std::vector<float>(_populations, 0);
  cs.gens = std::vector<uint>(_populations, std::numeric_limits<uint>::max());
  cs.fights = std::vector<uint>(_populations, 0);
  for (const auto &c: _critters) {
    auto i = c->userIndex;
    cs.counts[i]++;
    auto r = c->carnivorousBehavior();
    if (!isnan(r)) cs.regimens[i] += r;
    cs.gens[i] = std::min(cs.gens[i],
                                         c->genealogy().generation);
  }
  for (uint i=0; i<_populations; i++) {
    cs.regimens[i] /= cs.counts[i];
    if (cs.counts[i] == 0) cs.gens[i] = 0;
  }

  for (const auto &f: _environment->fightingEvents()) {
    cs.fights[f.first.first->userIndex]++;
    cs.fights[f.first.second->userIndex]++;
  }

  if (!_printedHeader) {
    _competitionLogger << "Time";
    for (uint i=0; i<_populations; i++)
      _competitionLogger << " Count" << i << " Carn" << i << " MinGen" << i
                         << " Fight" << i;
    _competitionLogger << "\n";
  }

  _competitionLogger << _time.timestamp();
  for (uint i=0; i<_populations; i++)
    _competitionLogger << " " << cs.counts[i] << " " << cs.regimens[i] << " "
                       << cs.gens[i] << " " << cs.fights[i];
  _competitionLogger << std::endl;

  _printedHeader = true;
}

decimal Simulation::totalEnergy(void) const {
  decimal e = 0;
  for (const auto &c: _critters)  e += c->totalEnergy();
  for (const auto &f: _foodlets)  e += f->energy();
  e += _environment->energy();
  return e;
}

void Simulation::correctFloatingErrors(void) {
  // Monitoring is deactivated
  if (_systemExpectedEnergy < 0)  return;

  static constexpr auto epsilonE = config::Simulation::epsilonE;
  static constexpr decltype(epsilonE) epsilonE_ = epsilonE / 10.f;
  decimal E = totalEnergy(), dE = E - _systemExpectedEnergy;
  if (epsilonE_ <= dE && dE <= _environment->energy()) {
    _environment->modifyEnergyReserve(-dE);
    std::cerr << "Corrected ambiant chaos by injecting " << (dE > 0 ? "+" : "")
              << dE << " in the environment" << std::endl;
  }
}

#ifndef NDEBUG
void Simulation::detectBudgetFluctuations(float threshold) {
  // Monitoring is deactivated
  if (_systemExpectedEnergy < 0)  return;

  decimal E = totalEnergy(), dE = _systemExpectedEnergy-E;
  if (std::fabs(dE) > threshold) {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<decimal>::digits10)
        << "Excessive budget variation of "
        << E-_systemExpectedEnergy << " = " << E << " - "
        << _systemExpectedEnergy << std::endl;

    decimal e = 0;
    oss << "\tCritters: ";
    for (const auto &c: _critters)  e += c->totalEnergy();
    oss << e << "\n";
    e = 0;
    oss << "\tFoodlets: ";
    for (const auto &f: _foodlets)  e += f->energy();
    oss << e << "\n";
    oss << "\t Reserve: " << _environment->energy() << "\n";

    std::cerr << oss.str() << std::endl;

//    assert(false);
    abort();
  }
}
#endif

// =============================================================================
// == Saving and loading

// Low level writing of a bytes array
bool save (const std::string &file, const std::vector<uint8_t> &bytes) {
  std::ofstream ofs (file, std::ios::out | std::ios::binary);
  if (!ofs)
    utils::doThrow<std::invalid_argument>(
          "Unable to open '", file, "' for writing");

  ofs.write((char*)bytes.data(), bytes.size());
  ofs.close();

  return true;
}

// Low level loading of a bytes array
bool load (const std::string &file, std::vector<uint8_t> &bytes) {
  std::ifstream ifs(file, std::ios::binary | std::ios::ate);
  if (!ifs)
    utils::doThrow<std::invalid_argument>(
          "Unable to open '", file, "' for reading");

  std::ifstream::pos_type pos = ifs.tellg();

  bytes.resize(pos);
  ifs.seekg(0, std::ios::beg);
  ifs.read((char*)bytes.data(), pos);

  return true;
}

void Simulation::serializePopulations (json &jcritters, json &jfoodlets) const {
  for (const auto &f: _foodlets)
    jfoodlets.push_back({
      { f->x(), f->y() },
      Foodlet::save(*f)
    });

  for (const auto &p: _critters)
    jcritters.push_back({
      { p->x(), p->y(), p->rotation() },
      Critter::save(*p)
    });
}

void Simulation::deserializePopulations (const json &jcritters,
                                         const json &jfoodlets,
                                         bool /*updatePTree*/) {
  for (const auto &j: jfoodlets) {
    b2Body *b = foodletBody(j[0][0], j[0][1]);
    Foodlet *f = Foodlet::load(j[1], b);
    _foodlets.insert(f);
  }

  for (const auto &j: jcritters) {
    b2Body *b = critterBody(j[0][0], j[0][1], j[0][2]);
    Critter *c = Critter::load(j[1], b);

    _critters.insert(c);

//    if (updatePTree) {
//      PStats *pstats = _ptree.getUserData(p->genealogy().self);
//      p->setPStatsPointer(pstats);
//    }
  }

//  _env.postLoad();
//  updateGenStats();
}


std::string field (SimuFields f) {
  auto name = EnumUtils<SimuFields>::getName(f);
  transform(name.begin(), name.end(), name.begin(), ::tolower);
  return name;
}

void Simulation::save (stdfs::path file) const {
  auto startTime = clock::now();
  if (file.empty()) file = periodicSaveName();

  nlohmann::json jconf, jenv, jcrit, jfood;//, jt;
  config::Simulation::serialize(jconf);
  Environment::save(jenv, *_environment);
  serializePopulations(jcrit, jfood);

//  if (_ptreeActive)
//    PTree::toJson(jt, _ptree);

  nlohmann::json j;
  j["config"] = jconf;
  j[field(SimuFields::ENV)] = jenv;
  j[field(SimuFields::CRITTERS)] = jcrit;
  j[field(SimuFields::FOODLETS)] = jfood;
//  j[field(SimuFields::PTREE)] = jt;
  j["nextCID"] = Critter::ID(_gidManager);
  j["nextFID"] = _nextFoodletID;
  j["energy"] = _systemExpectedEnergy;
  j["time"] = _time;

  startTime = clock::now();

  std::vector<std::uint8_t> v;

  const auto ext = file.extension();
  if (ext == ".cbor")         v = json::to_cbor(j);
  else if (ext == ".msgpack") v = json::to_msgpack(j);
  else {
    if (ext != ".ubjson") file += ".ubjson";
    v = json::to_ubjson(j);
  }

  simu::save(file, v);

#if !defined(NDEBUG) && 0
  std::cerr << "Reloading for round-trip test" << std::endl;
  Simulation that;
  load(file, that, "");
  assertEqual(*this, that);
#endif

//  std::cerr << "Saved to " << file << std::endl;
}

void Simulation::load (const stdfs::path &file, Simulation &s,
                       const std::string &constraints,
                       const std::string &fields) {

  std::set<std::string> requestedFields;  // Default to all
  for (auto f: EnumUtils<SimuFields>::iterator())
    requestedFields.insert(field(f));

  for (std::string strf: utils::split(fields, ',')) {
    strf = utils::trim(strf);
    if (strf == "none") requestedFields.clear();
    else if (strf.empty() || strf == "all")
      continue;

    else if (strf.at(0) != '!')
      continue; // Ignore non removal requests

    else {
      strf = strf.substr(1);
      SimuFields f;
      std::istringstream (strf) >> f;

      if (!EnumUtils<SimuFields>::isValid(f)) {
        std::cerr << "'" << strf << "' is not a valid simulation field. "
                     "Ignoring." << std::endl;
        continue;
      }

      requestedFields.erase(field(f));
    }
  }

  std::cout << "Loading " << file << "...\r" << std::flush;

//  if (!fields.empty())
//    std::cout << "Requested fields: "
//              << utils::join(requestedFields.begin(), requestedFields.end(),
//                             ",")
//              << std::endl;

  std::vector<uint8_t> v;
  simu::load(file, v);

  std::cout << "Expanding " << file << "...\r" << std::flush;

  json j;
  const auto ext = file.extension();
  if (ext == ".cbor")         j = json::from_cbor(v);
  else if (ext == ".msgpack") j = json::from_msgpack(v);
  else if (ext == ".ubjson")  j = json::from_ubjson(v);
  else
    utils::doThrow<std::invalid_argument>(
      "Unkown save file type '", file, "' of extension '", ext, "'");

//  auto dependencies = config::Dependencies::saveState();
//  config::Simulation::deserialize(j["config"]);
//  if (!config::Dependencies::compareStates(dependencies, constraints))
//    utils::doThrow<std::invalid_argument>(
//      "Provided save has different build parameters than this one.\n"
//      "See above for more details... (aborting)");

  auto loadf = [&requestedFields] (SimuFields f) {
    return requestedFields.find(field(f)) != requestedFields.end();
  };

  bool loadEnv = loadf(SimuFields::ENV);
  if (loadEnv)  Environment::load(j[field(SimuFields::ENV)], s._environment);

  bool loadTree = loadf(SimuFields::PTREE);
//  if (loadTree) PTree::fromJson(j[field(SimuFields::PTREE)], s._ptree);

  bool loadCrits = loadf(SimuFields::CRITTERS);
  bool loadFood = loadf(SimuFields::FOODLETS);
  if (loadCrits || loadFood) {
    json jc = j[field(SimuFields::CRITTERS)];
    if (!loadCrits) jc.clear();

    json jf = j[field(SimuFields::FOODLETS)];
    if (!loadFood) jf.clear();

    s.deserializePopulations(jc, jf, loadTree);
  }

  s._finished = false;
  s._aborted = false;
  s._genData.min = 0;
  s._genData.max = 0;
  s._gidManager.setNext(j["nextCID"]);
  s._nextFoodletID = j["nextFID"];
  s._systemExpectedEnergy = j["energy"];
  s._time = j["time"];

//  s._ptreeActive = loadTree;

//  s.logToFiles();

  std::cout << "Loaded " << file << std::endl;
}


// =============================================================================
// == EDEnS cloning

void Simulation::clone (const Simulation &s) {
  clear();

//  if (!_environment)
//    _environment = std::make_unique<Environment>(s._environment->genotype());
//  _environment->clone(*s._environment);
  _environment.reset(Environment::clone(*s._environment));

  _gidManager = s._gidManager;
  for (Critter *c: s._critters) {
    b2Body *b_ = Box2DUtils::clone(&c->body(), &physics());
    Critter *c_ = Critter::clone(c, b_);
    _critters.insert(c_);
  }

  _nextFoodletID = s._nextFoodletID;
  for (Foodlet *f: s._foodlets) {
    b2Body *b_ = Box2DUtils::clone(&f->body(), &physics());
    Foodlet *f_ = Foodlet::clone(f, b_);
    _foodlets.insert(f_);
  }

  _time = s._time;

  _genData = s._genData;

  _workPath = s._workPath;

  _populations = s._populations;

  _systemExpectedEnergy = s._systemExpectedEnergy;

  _timeMs.level = s._timeMs.level;
  _reproductions = s._reproductions;
  _autopsies = s._autopsies;
  _competitionStats = s._competitionStats;

  _finished = s._finished;
  _aborted = s._aborted;

#ifndef NDEBUG
  assertEqual(*this, s, true);

  detectBudgetFluctuations();
#endif
}

template <typename V>
struct IDSort {
  bool operator() (const V *lhs, const V *rhs) noexcept {
    return lhs->id() < rhs->id();
  }
};

#ifndef NDEBUG
void assertEqual (const Simulation &lhs, const Simulation &rhs, bool deepcopy) {
  using utils::assertEqual;

#define ASRT(X) assertEqual(lhs.X, rhs.X, deepcopy)
  ASRT(_environment);
  ASRT(_gidManager);
  ASRT(_nextFoodletID);
  ASRT(_time);
  ASRT(_genData.min);
  ASRT(_genData.max);
  ASRT(_genData.goal);
//  ASRT(_workPath);
//  ASRT(_statsLogger);
//  ASRT(_competitionLogger);
  ASRT(_populations);
  ASRT(_systemExpectedEnergy);
  ASRT(_timeMs.step);
  ASRT(_timeMs.spln);
  ASRT(_timeMs.env);
  ASRT(_timeMs.decay);
  ASRT(_timeMs.regen);
  ASRT(_timeMs.level);
  ASRT(_reproductions.attempts);
  ASRT(_reproductions.sexual);
  ASRT(_reproductions.asexual);
  ASRT(_autopsies.counts);
  ASRT(_autopsies.oldage);
  ASRT(_competitionStats.counts);
  ASRT(_competitionStats.regimens);
  ASRT(_competitionStats.gens);
  ASRT(_competitionStats.fights);
  ASRT(_finished);
  ASRT(_aborted);
#undef ASRT

  // Needs sorting
  using P = IDSort<Critter>;
  using V = decltype(lhs._critters)::value_type;
  static_assert(std::is_nothrow_invocable_r<bool, P, V, V>::value, "No");
  assertEqual(lhs._critters, rhs._critters, IDSort<Critter>(), deepcopy);
  assertEqual(lhs._foodlets, rhs._foodlets, IDSort<Foodlet>(), deepcopy);

  // Compare energy levels within some reasonnable margin
  decimal eclhs = 0, ecrhs = 0, eflhs = 0, efrhs = 0;
  for (auto ilhs = lhs._critters.begin(), irhs = rhs._critters.begin();
       ilhs != lhs._critters.end(); ++ilhs, ++irhs) {
    const Critter *clhs = *ilhs, *crhs = *irhs;
    eclhs += clhs->totalEnergy();
    ecrhs += crhs->totalEnergy();
  }
  for (auto ilhs = lhs._foodlets.begin(), irhs = rhs._foodlets.begin();
       ilhs != lhs._foodlets.end(); ++ilhs, ++irhs) {
    const Foodlet *flhs = *ilhs, *frhs = *irhs;
    eflhs += flhs->energy();
    efrhs += frhs->energy();
  }
  decimal eelhs = lhs._environment->energy(), eerhs = rhs._environment->energy();

  decimal T = config::Simulation::epsilonE;
  utils::assertFuzzyEqual(eclhs, ecrhs, T, false);
  utils::assertFuzzyEqual(eflhs, efrhs, T, false);
  utils::assertFuzzyEqual(eelhs, eerhs, T, false);
  utils::assertFuzzyEqual(eclhs+eflhs+eelhs, ecrhs+efrhs+eerhs, T, false);
}
#endif

} // end of namespace simu
