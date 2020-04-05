#include "kgd/random/dice.hpp"

#include "simulation.h"

namespace simu {

static constexpr int debugCritterManagement = 0;
static constexpr int debugFoodletManagement = 0;

Simulation::Simulation(void) : _environment(nullptr), _stepTimeMs(0) {}

Simulation::~Simulation (void) {
  clear();
}

void Simulation::init(const Environment::Genome &egenome,
                      Critter::Genome cgenome,
                      const InitData &data) {

  // TODO Remove (debug)
//  cgenome.vision.angleBody = M_PI/4.;
//  cgenome.vision.angleRelative = -M_PI/4;
//  cgenome.vision.width = M_PI/4.;
//  cgenome.vision.precision = 1;
  std::cerr << "   Plant min energy: "
            << Foodlet::maxStorage(BodyType::PLANT,
                                   config::Simulation::plantMinRadius())
            << "\n";

  auto le = [&cgenome] (float v) {
    return Critter::lifeExpectancy(Critter::clockSpeed(cgenome, v));
  };
  auto sd = [&cgenome] (float v, bool y) {
    auto s = y ? Critter::MIN_SIZE : Critter::MAX_SIZE;
    return Critter::starvationDuration(s,
      Critter::maximalEnergyStorage(s),
      Critter::clockSpeed(cgenome, v));
  };
  std::cerr << "Splinoid min energy: " << Critter::energyForCreation() << "\n"
            << "   Old age duration: " << le(0) << ", " << le(.5) << ", "
              << le(1) << "\n"
            << "Starvation duration:\n\t"
              << sd(0, true) << ", " << sd(.5, true) << ", " << sd(1, true)
              << "\n\t" << sd(0, false) << ", " << sd(.5, false) << ", "
              << sd(1, false) << std::endl;

  _systemExpectedEnergy = data.ienergy;

  _environment = std::make_unique<Environment>(egenome);
  _environment->init(data.ienergy, data.seed);

  _gidManager.setNext(cgenome.id());
  cgenome.gdata.setAsPrimordial(_gidManager);

  decimal energy = data.ienergy;
  decimal cenergy = energy * data.cRatio;
  decimal energyPerCritter =
    std::min(Critter::energyForCreation(), decimal(cenergy / data.nCritters));

//  energy -= cenergy;
//  cenergy -= energyPerCritter * data.nCritters;
//  energy += cenergy;

  const float W = _environment->extent();
  const float C = W * data.cRange;

  auto &dice = _environment->dice();
  for (uint i=0; i<data.nCritters; i++) {
    auto cg = cgenome.clone(_gidManager);
    cg.cdata.sex = (i%2 ? Critter::Sex::MALE : Critter::Sex::FEMALE);
    float a = dice(0., 2*M_PI);
    float x = dice(-C, C);
    float y = dice(-C, C);

    // TODO
    if (i == 0) x = 0, y = -1, a = 0;
    if (i == 1) x = 0, y = 1, a = 0;

//    if (i == 0) x = 49, y = 49, a = M_PI/4;
//    if (i == 1) x = 47, y = 50-sqrt(2), a = M_PI/2;
//    if (i == 2) x = 50-sqrt(2), y = 47, a = 0;

    addCritter(cg, x, y, a, energyPerCritter);
  }

  _nextPlantID = 0;

  addFoodlet(BodyType::PLANT,
             2, 0,
             1, 2); // TODO
  plantRenewal(W * data.pRange);

  _statsLogger.open(config::Simulation::logFile());

  _ssga.init(_critters.size());

  postInit();

  logStats();
}

Critter* Simulation::addCritter (const CGenome &genome,
                                 float x, float y, float a, decimal e) {

  b2BodyDef bodyDef;
  bodyDef.type = b2_dynamicBody;
  bodyDef.position.Set(x, y);
  bodyDef.angle = a;
  bodyDef.angularDamping = .95;
  bodyDef.linearDamping = .9;

  b2Body *body = physics().CreateBody(&bodyDef);

#ifndef NDEBUG
  decimal prevEE = _environment->energy();
#endif

  decimal dissipatedEnergy =
    e * .5 *(1 - config::Simulation::healthToEnergyRatio());
  decimal e_ = e - dissipatedEnergy;
  _environment->modifyEnergyReserve(-e_);

  Critter *c = new Critter (genome, body, e_);
  _critters.insert(c);

#ifndef NDEBUG
  decimal e__ = c->totalEnergy();
  if (e_ != e__)
    std::cerr << "Delta energy on C" << c->id() << " creation: " << e__-e_
              << std::endl;
  decimal currEE = _environment->energy(), dEE = currEE - prevEE;
  if (dEE != e_)
    std::cerr << "Delta energy reserve on C" << c->id() << " creation: "
              << dEE +e_ << std::endl;
#endif

  if (debugCritterManagement)
    std::cerr << "Created critter " << c->id() << " at " << c->pos()
              << std::endl;

  return c;
}

void Simulation::delCritter (Critter *critter) {
  if (debugCritterManagement) critter->autopsy();
  _critters.erase(critter);
  physics().DestroyBody(&critter->body());
  delete critter;
}

Foodlet* Simulation::addFoodlet(BodyType t, float x, float y, float r, decimal e) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_staticBody;
  bodyDef.position.Set(x, y);

  b2Body *body = _environment->physics().CreateBody(&bodyDef);
  Foodlet *f = new Foodlet (t, _nextPlantID++, body, r, e);

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
  while (!_critters.empty())  delCritter(*_critters.begin());
  while (!_foodlets.empty())  delFoodlet(*_foodlets.begin());
}

void Simulation::step (void) {
  auto stepStart = now(), start = stepStart;

//  std::cerr << "\n## Simulation step " << _time.timestamp() << " ("
//            << _time.pretty() << ") ##" << std::endl;

  _minGen = std::numeric_limits<uint>::max();
  _maxGen = 0;

  for (Critter *c: _critters) {
    c->step(*_environment);

    _minGen = std::min(_minGen, c->genotype().gdata.generation);
    _maxGen = std::max(_maxGen, c->genotype().gdata.generation);
  }
  _splnTimeMs = durationFrom(start);  start = now();

  if (_ssga.watching()) _ssga.prePhysicsStep(_critters);

  _environment->step();

  if (_ssga.watching()) _ssga.postPhysicsStep(_critters);
  _envTimeMs = durationFrom(start);  start = now();

  produceCorpses();
  decomposition();
  _decayTimeMs = durationFrom(start);  start = now();

  steadyStateGA();
  if (_time.secondFraction() == 0)  plantRenewal();
  _regenTimeMs = durationFrom(start);  start = now();

  if (false) {
  /* TODO DEBUG AREA
  */
    const auto doTo = [this] (Critter::ID id, auto f) {
      Critter *c = nullptr;
      for (Critter *c_: _critters) {
        if (c_->id() == id) {
          c = c_;
          break;
        }
      }
      if (c)  f(c);
    };
//    const auto forcedMotion = [this] (float a) {
//      return [this, a] (Critter *c){
//        if (_time.second() == 0)
//          c->body().ApplyForceToCenter(
//            4*config::Simulation::critterBaseSpeed()*fromPolar(a,1), true);
//      };
//    };
//    doTo(Critter::ID(1), forcedMotion(M_PI/4));
//    doTo(Critter::ID(2), forcedMotion(M_PI/2));
//    doTo(Critter::ID(3), forcedMotion(0));
//    doTo(Critter::ID(1), [this] (Critter *c) {
//      if (_time.timestamp() == 50) {
////        c->applyHealthDamage(
////            Critter::FixtureData(Critter::FixtureType::BODY, {0,0,0}),
////            .025,  *_environment);
//        Critter::FixtureData fakeFD (
//          Critter::FixtureType::ARTIFACT, {0,0,0}, 0, Critter::Side::LEFT, 0);

//        bool d = c->applyHealthDamage(fakeFD, .5,  *_environment);
//        if (d)  c->destroySpline(Critter::splineIndex(fakeFD));
//      }
//    });
  }

  if (_statsLogger) logStats();

  _time.next();

#ifndef NDEBUG
  // Fails when system energy diverges too much from initial value
  detectBudgetFluctuations();
#endif

  // If above did not fail, effect small corrections to keep things smooth
  if (config::Simulation::screwTheEntropy())  correctFloatingErrors();

  _stepTimeMs = durationFrom(stepStart);
}

void Simulation::produceCorpses(void) {
  std::vector<Critter*> corpses;
  for (Critter *c: _critters)
    if (c->isDead())
      corpses.push_back(c);

  for (Critter *c: corpses) {
    Foodlet *f = addFoodlet(BodyType::CORPSE, c->x(), c->y(), c->bodyRadius(),
                            c->totalEnergy());
    f->setBaseColor(c->bodyColor());
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

  // Maybe pop-up a new plant
  auto &dice = _environment->dice();
  while (_environment->energy() > minE) {
    float r = dice(mR, MR);
    decimal e = std::min(_environment->energy(),
                         Foodlet::maxStorage(BodyType::PLANT, r));
    float E = (bounds > 0 ? bounds : _environment->extent()) - r;
    addFoodlet(BodyType::PLANT, dice(-E, E), dice(-E, E), r, e);

    assert(_environment->energy() >= 0);
  }
}

void Simulation::logStats (void) {
  Stats s;
  s.nplants = 0;
  s.ncorpses = 0;
  s.eplants = 0;
  s.ecorpses = 0;

  for (const auto &f: _foodlets) {
    if (f->type() == simu::BodyType::PLANT)
          s.nplants++,  s.eplants += f->energy();
    else  s.ncorpses++, s.ecorpses += f->energy();
  }

  s.ncritters = _critters.size();

  s.nfeedings = _environment->feedingEvents().size();
  s.nfights = _environment->fightingEvents().size();

  s.ecritters = 0;
  for (const auto &c: _critters) s.ecritters += c->totalEnergy();

  s.ereserve = _environment->energy();

  decimal eE = totalEnergy() - _systemExpectedEnergy;

  if (_startTime == _time)
    _statsLogger << "Step Date"
                    " NCritters NCorpses NPlants"
                    " NFeeding NFights"
                    " ECritters ECorpses EPlants EReserve"
                    " eE"
                    "\n";

  _statsLogger << _time.timestamp() << " " << _time.pretty() << " "

               << s.ncritters << " " << s.ncorpses << " " << s.nplants << " "
               << s.nfeedings << " " << s.nfights << " "

               << s.ecritters << " " << s.ecorpses << " " << s.eplants << " "
               << s.ereserve << " "
               << eE

               << std::endl;

  processStats(s);
}

decimal Simulation::totalEnergy(void) const {
  decimal e = 0;
  for (const auto &c: _critters)  e += c->totalEnergy();
  for (const auto &f: _foodlets)  e += f->energy();
  e += _environment->energy();
  return e;
}

void Simulation::steadyStateGA(void) {
  _ssga.update(_critters.size());

  static const auto minE = Critter::energyForCreation();
  if (_ssga.active() && _time.secondFraction() == 0) {
    float r = _environment->extent() - Critter::MIN_SIZE * Critter::RADIUS;

    auto &dice = _environment->dice();
    while (_environment->energy() >= minE && _ssga.active(_critters.size())) {
      auto genome = _ssga.getRandomGenome(dice, 5000);
      genome.gdata.setAsPrimordial(_gidManager);
      float a = dice(0., 2*M_PI);
      float x = dice(-r, r), y = dice(-r, r);
      addCritter(genome, x, y, a, minE);
    }
  }
}

void Simulation::correctFloatingErrors(void) {
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
  decimal E = totalEnergy(), dE = _systemExpectedEnergy-E;
  if (std::fabs(dE) > threshold) {
    std::cerr << std::setprecision(std::numeric_limits<decimal>::digits10)
              << "Excessive budget variation of "
              << E-_systemExpectedEnergy << " = " << E << " - "
              << _systemExpectedEnergy << std::endl;
    assert(false);
  }
}
#endif

} // end of namespace simu
